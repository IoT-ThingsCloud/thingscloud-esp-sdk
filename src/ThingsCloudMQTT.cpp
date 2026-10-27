/*
  ThingsCloudMQTT.cpp - MQTT client for ThingsCloud.
  https://www.thingscloud.xyz
*/

#include "ThingsCloudMQTT.h"

// =============== Constructor / destructor ===================

ThingsCloudMQTT::ThingsCloudMQTT(
    const char *mqttHost,
    String accessToken,
    const char *projectKey) : _mqttHost(mqttHost),
                              _accessToken(accessToken),
                              _projectKey(projectKey),
                              _mqttClient(mqttHost, _mqttServerPort, _wifiClient)
{
    // WiFi connection
    _wifiConnected = false;
    _connectingToWifi = false;
    _nextWifiConnectionAttemptMillis = 500;
    _lastWifiConnectiomAttemptMillis = 0;
    _wifiReconnectionAttemptDelay = 60 * 1000;

    initState();
}

ThingsCloudMQTT::ThingsCloudMQTT(
    const char *mqttHost,
    String deviceKey,
    const char *projectKey,
    String typeKey,
    const char *apiEndpoint) : _mqttHost(mqttHost),
                               _deviceKey(deviceKey),
                               _projectKey(projectKey),
                               _typeKey(typeKey),
                               _apiEndpoint(apiEndpoint),
                               _mqttClient(mqttHost, _mqttServerPort, _wifiClient)
{
    _wifiConnected = false;
    _connectingToWifi = false;
    _nextWifiConnectionAttemptMillis = 500;
    _lastWifiConnectiomAttemptMillis = 0;
    _wifiReconnectionAttemptDelay = 60 * 1000;
    _needFetchAccessToken = true;
    if (_deviceKey == "")
    {
        _deviceKey = getEspChipUniqueId();
    }

    initState();
}

ThingsCloudMQTT::~ThingsCloudMQTT()
{
}

void ThingsCloudMQTT::initState()
{
    // MQTT client
    _mqttClientName = String(DEFAULT_MQTT_CLIENT_NAME) + "_" + getEspChipUniqueId();
    _mqttConnected = false;
    _nextMqttConnectionAttemptMillis = 0;
    _mqttReconnectionAttemptDelay = 15 * 1000;
    _accessTokenFetchAttemptDelay = 10 * 1000;
    _mqttLastWillTopic = 0;
    _mqttLastWillMessage = 0;
    _mqttLastWillRetain = false;
    _mqttCleanSession = true;
    _mqttClient.setBufferSize(1024);
    _mqttClient.setCallback([this](char *topic, uint8_t *payload, unsigned int length)
                            { this->mqttMessageReceivedCallback(topic, payload, length); });
    _failedMQTTConnectionAttemptCount = 0;
    // other
    _enableSerialLogs = false;
    _drasticResetOnConnectionFailures = false;
    _onMQTTConnect = onMQTTConnect;
    _connectionEstablishedCount = 0;
}

// =============== Configuration functions, most of them must be called before the first loop() call ==============

void ThingsCloudMQTT::enableDebuggingMessages(const bool enabled)
{
    _enableSerialLogs = enabled;
}

bool ThingsCloudMQTT::fetchDeviceAccessToken()
{
    if (_apiEndpoint == nullptr || _apiEndpoint[0] == '\0')
        return false;

    HTTPClient http;
    if (_enableSerialLogs)
        Serial.println("Request device AccessToken by DeviceKey " + _deviceKey);
    String serverPath = String(_apiEndpoint) + "/device/v1/certificate";
#ifdef ESP32
    bool started = http.begin(serverPath.c_str());
#else
    bool started = http.begin(_wifiClient, serverPath.c_str());
#endif
    if (!started)
    {
        http.end();
        return false;
    }
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Project-Key", _projectKey);
    StaticJsonDocument<1024> postObject;
    postObject["device_key"] = _deviceKey;
    if (_typeKey != "")
    {
        postObject["type_key"] = _typeKey;
    }
    if (_customerId != "")
    {
        postObject["customer_id"] = _customerId;
    }
    String postData;
    if (postObject.overflowed())
    {
        http.end();
        return false;
    }
    serializeJson(postObject, postData);

    int httpResponseCode = http.POST(postData);
    if (httpResponseCode < 200 || httpResponseCode >= 300)
    {
        if (_enableSerialLogs)
            Serial.printf("AccessToken request failed, HTTP code: %d\n", httpResponseCode);
        http.end();
        return false;
    }

    String response = http.getString();
    http.end();
    // Ignore unrelated device metadata when allocating the certificate document.
    StaticJsonDocument<128> filter;
    filter["result"] = true;
    filter["device"]["access_token"] = true;
    DynamicJsonDocument doc(response.length() + JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(1) + 1);
    DeserializationError error = deserializeJson(doc, response, DeserializationOption::Filter(filter));
    if (error || !doc["result"].is<int>() || doc["result"].as<int>() != 1 ||
        !doc["device"]["access_token"].is<const char *>())
    {
        if (_enableSerialLogs)
            Serial.println("AccessToken response is invalid or refused");
        return false;
    }

    const char *deviceAccessToken = doc["device"]["access_token"];
    if (deviceAccessToken[0] == '\0')
        return false;
    String accessToken(deviceAccessToken);
    if (accessToken.length() != strlen(deviceAccessToken))
        return false;
    _accessToken = accessToken;
    if (_enableSerialLogs)
        Serial.println("AccessToken granted");
    return true;
}

void ThingsCloudMQTT::setCustomerId(const String customerId)
{
    _customerId = customerId;
    if (_enableSerialLogs)
        Serial.printf("Set customerId -> %s\n", _customerId.c_str());
}

// =============== Main loop / connection state handling =================

void ThingsCloudMQTT::loop()
{
    // WIFI handling
    bool wifiStateChanged = handleWiFi();

    // If there is a change in the wifi connection state, don't handle the mqtt connection state right away.
    // We will wait at least one lopp() call. This prevent the library from doing too much thing in the same loop() call.
    if (wifiStateChanged)
        return;

    // Fetch AccessToken
    if (!isWifiConnected())
        return;
    if (_needFetchAccessToken && !_accessTokenFetched)
    {
        if (_nextAccessTokenFetchAttemptMillis > 0 && millis() < _nextAccessTokenFetchAttemptMillis)
            return;

        bool ret = fetchDeviceAccessToken();
        if (ret)
        {
            _accessTokenFetched = true;
        }
        else
        {
            _nextAccessTokenFetchAttemptMillis = millis() + _accessTokenFetchAttemptDelay;
            return;
        }
    }

    // MQTT Handling
    bool mqttStateChanged = handleMQTT();
    if (mqttStateChanged)
        return;

    // Procewss the delayed execution commands
    processDelayedExecutionRequests();
}

bool ThingsCloudMQTT::handleWiFi()
{
    // When it's the first call, reset the wifi radio and schedule the wifi connection
    static bool firstLoopCall = true;
    if (_handleWiFi && firstLoopCall)
    {
        WiFi.disconnect(true);
        _nextWifiConnectionAttemptMillis = millis() + 500;
        firstLoopCall = false;
        return true;
    }

    // Get the current connextion status
    bool isWifiConnected = (WiFi.status() == WL_CONNECTED);
    bool stateChanged = isWifiConnected != _wifiConnected;

    /***** Detect ans handle the current WiFi handling state *****/

    // Connection established
    if (isWifiConnected && !_wifiConnected)
    {
        _wifiConnected = true;
        onWiFiConnectionEstablished();
        _connectingToWifi = false;

        // At least 500 miliseconds of waiting before an mqtt connection attempt.
        // Some people have reported instabilities when trying to connect to
        // the mqtt broker right after being connected to wifi.
        // This delay prevent these instabilities.
        _nextMqttConnectionAttemptMillis = millis() + 500;
        _nextAccessTokenFetchAttemptMillis = millis() + 500;
    }

    // Connection in progress
    else if (_connectingToWifi)
    {
        if (WiFi.status() == WL_CONNECT_FAILED || millis() - _lastWifiConnectiomAttemptMillis >= _wifiReconnectionAttemptDelay)
        {
            if (_enableSerialLogs)
                Serial.printf("WiFi! Connection attempt failed, delay expired. (%fs). \n", millis() / 1000.0);

            WiFi.disconnect(true);
            _nextWifiConnectionAttemptMillis = millis() + 500;
            _connectingToWifi = false;
        }
    }

    // Connection lost
    else if (!isWifiConnected && _wifiConnected)
    {
        _wifiConnected = false;
        onWiFiConnectionLost();

        if (_handleWiFi)
            _nextWifiConnectionAttemptMillis = millis() + 500;
    }

    // Connected since at least one loop() call
    else if (isWifiConnected && _wifiConnected)
    {
    }

    // Disconnected since at least one loop() call
    // Then, if we handle the wifi reconnection process and the waiting delay has expired, we connect to wifi
    else if (_handleWiFi && _nextWifiConnectionAttemptMillis > 0 && millis() >= _nextWifiConnectionAttemptMillis)
    {
        connectToWifi();
        _nextWifiConnectionAttemptMillis = 0;
        _connectingToWifi = true;
        _lastWifiConnectiomAttemptMillis = millis();
    }

    /**** Detect and return if there was a change in the WiFi state ****/

    return stateChanged;
}

bool ThingsCloudMQTT::handleMQTT()
{
    // PubSubClient main loop() call
    _mqttClient.loop();

    // Get the current connextion status
    bool isMqttConnected = (isWifiConnected() && _mqttClient.connected());

    /***** Detect and handle the current MQTT handling state *****/

    // Connection established
    if (isMqttConnected && !_mqttConnected)
    {
        _mqttConnected = true;
        onMQTTConnectionEstablished();
    }

    // Connection lost
    else if (!isMqttConnected && _mqttConnected)
    {
        _mqttConnected = false;
        onMQTTConnectionLost();
        _nextMqttConnectionAttemptMillis = millis() + _mqttReconnectionAttemptDelay;
    }

    // It's time to  connect to the MQTT broker
    else if (isWifiConnected() && _nextMqttConnectionAttemptMillis > 0 && millis() >= _nextMqttConnectionAttemptMillis)
    {
        // Connect to MQTT broker
        if (connectToMqttBroker())
        {
            _failedMQTTConnectionAttemptCount = 0;
            _nextMqttConnectionAttemptMillis = 0;
        }
        else
        {
            // Connection failed, plan another connection attempt
            _nextMqttConnectionAttemptMillis = millis() + _mqttReconnectionAttemptDelay;
            _mqttClient.disconnect();
            _failedMQTTConnectionAttemptCount++;

            if (_enableSerialLogs)
                Serial.printf("MQTT!: Failed MQTT connection count: %i \n", _failedMQTTConnectionAttemptCount);

            // When there is too many failed attempt, sometimes it help to reset the WiFi connection or to restart the board.
            if (_handleWiFi && _failedMQTTConnectionAttemptCount == 8)
            {
                if (_enableSerialLogs)
                    Serial.println("MQTT!: Can't connect to broker after too many attempt, resetting WiFi ...");

                WiFi.disconnect(true);
                _nextWifiConnectionAttemptMillis = millis() + 500;

                if (!_drasticResetOnConnectionFailures)
                    _failedMQTTConnectionAttemptCount = 0;
            }
            else if (_drasticResetOnConnectionFailures && _failedMQTTConnectionAttemptCount == 12) // Will reset after 12 failed attempt (3 minutes of retry)
            {
                if (_enableSerialLogs)
                    Serial.println("MQTT!: Can't connect to broker after too many attempt, resetting board ...");

#ifdef ESP8266
                ESP.reset();
#else
                ESP.restart();
#endif
            }
        }
    }

    /**** Detect and return if there was a change in the MQTT state ****/

    if (_mqttConnected != isMqttConnected)
    {
        _mqttConnected = isMqttConnected;
        return true;
    }
    else
        return false;
}

void ThingsCloudMQTT::onWiFiConnectionEstablished()
{
    if (_enableSerialLogs)
        Serial.printf("WiFi: Connected (%fs), ip : %s \n", millis() / 1000.0, WiFi.localIP().toString().c_str());

    if (_onWifiConnect)
    {
        _onWifiConnect();
    }
}

void ThingsCloudMQTT::onWiFiConnectionLost()
{
    if (_enableSerialLogs)
        Serial.printf("WiFi! Lost connection (%fs). \n", millis() / 1000.0);

    // If we handle wifi, we force disconnection to clear the last connection
    if (_handleWiFi)
    {
        WiFi.disconnect(true);
    }

    // loop() skips MQTT processing while WiFi is down, so report this loss here.
    if (_mqttConnected)
    {
        _mqttConnected = false;
        _mqttClient.disconnect();
        onMQTTConnectionLost();
    }

    if (_onWifiDisconnect)
    {
        _onWifiDisconnect();
    }
}

void ThingsCloudMQTT::onMQTTConnectionEstablished()
{
    _connectionEstablishedCount++;
    if (_onMQTTConnect)
        _onMQTTConnect();
}

void ThingsCloudMQTT::onMQTTConnectionLost()
{
    if (_enableSerialLogs)
    {
        Serial.printf("MQTT! Lost connection (%fs). \n", millis() / 1000.0);
        Serial.printf("MQTT: Retrying to connect in %i seconds. \n", _mqttReconnectionAttemptDelay / 1000);
    }

    if (_onMQTTDisconnect)
    {
        _onMQTTDisconnect();
    }
}

// =============== Public functions for interaction with thus lib =================

bool ThingsCloudMQTT::setMaxPacketSize(const uint16_t size)
{

    bool success = _mqttClient.setBufferSize(size);

    if (!success && _enableSerialLogs)
        Serial.println("MQTT! failed to set the max packet size.");

    return success;
}

bool ThingsCloudMQTT::reportAttributes(const String attributes)
{
    return publish("attributes", attributes);
}

bool ThingsCloudMQTT::reportEvent(const uint16_t id, const String event)
{
    return publish("event/report/" + String(id), event);
}

bool ThingsCloudMQTT::reportData(const String &topic, const String &payload)
{
    return publish(topic, payload);
}

bool ThingsCloudMQTT::reportData(const String &topic, const uint8_t *payload, unsigned int plength)
{
    return publish(topic, payload, plength);
}

bool ThingsCloudMQTT::getAttributes()
{
    return publish("attributes/get/1000", "{}");
}

bool ThingsCloudMQTT::onAttributesGetResponse(MessageReceivedCallbackWithTopic messageReceivedCallbackWithTopic)
{
    return subscribe("attributes/get/response/+", messageReceivedCallbackWithTopic);
}

bool ThingsCloudMQTT::onAttributesGetResponse(MessageReceivedCallbackJSONWithTopic messageReceivedCallbackWithTopic)
{
    return subscribe("attributes/get/response/+", messageReceivedCallbackWithTopic);
}

bool ThingsCloudMQTT::onAttributesResponse(MessageReceivedCallback messageReceivedCallback)
{
    return subscribe("attributes/response", messageReceivedCallback);
}

bool ThingsCloudMQTT::onAttributesPush(MessageReceivedCallback messageReceivedCallback)
{
    return subscribe("attributes/push", messageReceivedCallback);
}

bool ThingsCloudMQTT::onAttributesPush(MessageReceivedCallbackJSON messageReceivedCallback)
{
    return subscribe("attributes/push", messageReceivedCallback);
}

bool ThingsCloudMQTT::onCommandSend(MessageReceivedCallbackWithTopic messageReceivedCallbackWithTopic)
{
    return subscribe("command/send/+", messageReceivedCallbackWithTopic);
}

bool ThingsCloudMQTT::onCommandSend(MessageReceivedCallbackJSONWithTopic messageReceivedCallbackWithTopic)
{
    return subscribe("command/send/+", messageReceivedCallbackWithTopic);
}

bool ThingsCloudMQTT::publish(const String &topic, const String &payload, bool retain)
{
    // Do not try to publish if MQTT is not connected.
    if (!isConnected())
    {
        if (_enableSerialLogs)
            Serial.println("MQTT! Trying to publish when disconnected, skipping.");

        return false;
    }

    bool success = _mqttClient.publish(topic.c_str(), payload.c_str(), retain);

    if (_enableSerialLogs)
    {
        if (success)
            Serial.printf("MQTT << [%s] %s\n", topic.c_str(), payload.c_str());
        else
            Serial.println("MQTT! publish failed, is the message too long ? (see setMaxPacketSize())"); // This can occurs if the message is too long according to the maximum defined in PubsubClient.h
    }

    return success;
}

bool ThingsCloudMQTT::publish(const String &topic, const uint8_t *payload, unsigned int plength)
{
    // Do not try to publish if MQTT is not connected.
    if (!isConnected())
    {
        if (_enableSerialLogs)
            Serial.println("MQTT! Trying to publish when disconnected, skipping.");

        return false;
    }

    bool success = _mqttClient.publish(topic.c_str(), payload, plength);

    if (_enableSerialLogs)
    {
        if (success)
            Serial.printf("MQTT << [%s] (HEX)0x%s\n", topic.c_str(), bytesToHex(payload, plength).c_str());
        else
            Serial.println("MQTT! publish failed, is the message too long ? (see setMaxPacketSize())"); // This can occurs if the message is too long according to the maximum defined in PubsubClient.h
    }

    return success;
}

bool ThingsCloudMQTT::subscribe(const String &topic, MessageReceivedCallback messageReceivedCallback, uint8_t qos)
{
    return subscribeRecord({topic, messageReceivedCallback, nullptr, nullptr, nullptr}, qos);
}

bool ThingsCloudMQTT::subscribeRecord(const TopicSubscriptionRecord &record, uint8_t qos)
{
    const String &topic = record.topic;
    // Do not try to subscribe if MQTT is not connected.
    if (!isConnected())
    {
        if (_enableSerialLogs)
            Serial.println("MQTT! Trying to subscribe when disconnected, skipping.");

        return false;
    }

    bool success = _mqttClient.subscribe(topic.c_str(), qos);

    if (success)
    {
        // Replace the matching record, including its callback type, on re-registration.
        bool found = false;
        for (std::size_t i = 0; i < _topicSubscriptionList.size() && !found; i++)
        {
            if (_topicSubscriptionList[i].topic.equals(topic))
            {
                _topicSubscriptionList[i] = record;
                found = true;
            }
        }

        if (!found)
            _topicSubscriptionList.push_back(record);
    }

    if (_enableSerialLogs)
    {
        if (success)
            Serial.printf("MQTT: Subscribed to [%s]\n", topic.c_str());
        else
            Serial.println("MQTT! subscribe failed");
    }

    return success;
}

bool ThingsCloudMQTT::subscribe(const String &topic, MessageReceivedCallbackJSON messageReceivedCallback, uint8_t qos)
{
    return subscribeRecord({topic, nullptr, messageReceivedCallback, nullptr, nullptr}, qos);
}

bool ThingsCloudMQTT::subscribe(const String &topic, MessageReceivedCallbackWithTopic messageReceivedCallback, uint8_t qos)
{
    return subscribeRecord({topic, nullptr, nullptr, messageReceivedCallback, nullptr}, qos);
}

bool ThingsCloudMQTT::subscribe(const String &topic, MessageReceivedCallbackJSONWithTopic messageReceivedCallback, uint8_t qos)
{
    return subscribeRecord({topic, nullptr, nullptr, nullptr, messageReceivedCallback}, qos);
}

bool ThingsCloudMQTT::unsubscribe(const String &topic)
{
    // Do not try to unsubscribe if MQTT is not connected.
    if (!isConnected())
    {
        if (_enableSerialLogs)
            Serial.println("MQTT! Trying to unsubscribe when disconnected, skipping.");

        return false;
    }

    for (std::size_t i = 0; i < _topicSubscriptionList.size(); i++)
    {
        if (_topicSubscriptionList[i].topic.equals(topic))
        {
            if (_mqttClient.unsubscribe(topic.c_str()))
            {
                _topicSubscriptionList.erase(_topicSubscriptionList.begin() + i);
                i--;

                if (_enableSerialLogs)
                    Serial.printf("MQTT: Unsubscribed from %s\n", topic.c_str());
            }
            else
            {
                if (_enableSerialLogs)
                    Serial.println("MQTT! unsubscribe failed");

                return false;
            }
        }
    }

    return true;
}

void ThingsCloudMQTT::disconnect()
{
    if (_mqttClient.connected())
    {
        _mqttClient.disconnect();
        _mqttConnected = false;
        onMQTTConnectionLost();

        if (_enableSerialLogs)
            Serial.println("MQTT: Gracefully disconnected from broker.");
    }
}

void ThingsCloudMQTT::setWifiCredentials(const char *wifiSsid, const char *wifiPassword)
{
    _wifiSsid = wifiSsid;
    _wifiPassword = wifiPassword;
    _handleWiFi = true;
}

void ThingsCloudMQTT::executeDelayed(const unsigned long delay, DelayedExecutionCallback callback)
{
    DelayedExecutionRecord delayedExecutionRecord;
    delayedExecutionRecord.targetMillis = millis() + delay;
    delayedExecutionRecord.callback = callback;

    _delayedExecutionList.push_back(delayedExecutionRecord);
}

// ================== Private functions ====================-

// Initiate a Wifi connection (non-blocking)
void ThingsCloudMQTT::connectToWifi()
{
    WiFi.mode(WIFI_STA);
#ifdef ESP32
    WiFi.setHostname(_mqttClientName.c_str());
#else
    WiFi.hostname(_mqttClientName.c_str());
#endif
    WiFi.begin(_wifiSsid, _wifiPassword);

    if (_enableSerialLogs)
        Serial.printf("\nWiFi: Connecting to %s ... (%fs) \n", _wifiSsid, millis() / 1000.0);
}

// Try to connect to the MQTT broker and return True if the connection is successfull (blocking)
bool ThingsCloudMQTT::connectToMqttBroker()
{
    bool success = false;

    if (_mqttHost != nullptr && strlen(_mqttHost) > 0)
    {
        if (_enableSerialLogs)
        {
            Serial.printf("MQTT: Connecting to ThingsCloud \"%s\" with AccessToken \"%s\" ... (%fs)", _mqttHost, _accessToken.c_str(), millis() / 1000.0);
        }

        // explicitly set the server/port here in case they were not provided in the constructor
        _mqttClient.setServer(_mqttHost, _mqttServerPort);
        _mqttClient.setKeepAlive(mqttKeepAlive);
        _mqttClient.setSocketTimeout(socketTimeout);
        success = _mqttClient.connect(_mqttClientName.c_str(), _accessToken.c_str(), _projectKey, _mqttLastWillTopic, 0, _mqttLastWillRetain, _mqttLastWillMessage, _mqttCleanSession);
    }
    else
    {
        if (_enableSerialLogs)
            Serial.printf("MQTT: ThingsCloud options is not set, not connecting (%fs)\n", millis() / 1000.0);
        success = false;
    }

    if (_enableSerialLogs)
    {
        if (success)
            Serial.printf(" - ok. (%fs) \n", millis() / 1000.0);
        else
        {
            Serial.printf("unable to connect (%fs), reason: ", millis() / 1000.0);

            switch (_mqttClient.state())
            {
            case -4:
                Serial.println("MQTT_CONNECTION_TIMEOUT");
                break;
            case -3:
                Serial.println("MQTT_CONNECTION_LOST");
                break;
            case -2:
                Serial.println("MQTT_CONNECT_FAILED");
                break;
            case -1:
                Serial.println("MQTT_DISCONNECTED");
                break;
            case 1:
                Serial.println("MQTT_CONNECT_BAD_PROTOCOL");
                break;
            case 2:
                Serial.println("MQTT_CONNECT_BAD_CLIENT_ID");
                break;
            case 3:
                Serial.println("MQTT_CONNECT_UNAVAILABLE");
                break;
            case 4:
                Serial.println("MQTT_CONNECT_BAD_CREDENTIALS");
                break;
            case 5:
                Serial.println("MQTT_CONNECT_UNAUTHORIZED");
                break;
            }

            Serial.printf("MQTT: Retrying to connect in %i seconds.\n", _mqttReconnectionAttemptDelay / 1000);
        }
    }

    return success;
}

// Delayed execution handling.
// Check if there is delayed execution requests to process and execute them if needed.
void ThingsCloudMQTT::processDelayedExecutionRequests()
{
    if (_delayedExecutionList.size() > 0)
    {
        unsigned long currentMillis = millis();

        for (std::size_t i = 0; i < _delayedExecutionList.size(); i++)
        {
            if (_delayedExecutionList[i].targetMillis <= currentMillis)
            {
                _delayedExecutionList[i].callback();
                _delayedExecutionList.erase(_delayedExecutionList.begin() + i);
                i--;
            }
        }
    }
}

/**
 * Matching MQTT topics, handling the eventual presence of a single wildcard character
 *
 * @param topic1 is the topic may contain a wildcard
 * @param topic2 must not contain wildcards
 * @return true on MQTT topic match, false otherwise
 */
bool ThingsCloudMQTT::mqttTopicMatch(const String &topic1, const String &topic2)
{
    int i = 0;

    if ((i = topic1.indexOf('#')) >= 0)
    {
        String t1a = topic1.substring(0, i);
        String t1b = topic1.substring(i + 1);
        if ((t1a.length() == 0 || topic2.startsWith(t1a)) &&
            (t1b.length() == 0 || topic2.endsWith(t1b)))
            return true;
    }
    else if ((i = topic1.indexOf('+')) >= 0)
    {
        String t1a = topic1.substring(0, i);
        String t1b = topic1.substring(i + 1);

        if ((t1a.length() == 0 || topic2.startsWith(t1a)) &&
            (t1b.length() == 0 || topic2.endsWith(t1b)))
        {
            if (topic2.substring(t1a.length(), topic2.length() - t1b.length()).indexOf('/') == -1)
                return true;
        }
    }
    else
    {
        return topic1.equals(topic2);
    }

    return false;
}

void ThingsCloudMQTT::mqttMessageReceivedCallback(char *topic, uint8_t *payload, unsigned int length)
{
    // Copy the exact payload length without writing a terminator into PubSubClient's buffer.
    // This also handles empty messages and packets that fill the receive buffer.
    String payloadStr;
    if (length > 0 && (!payloadStr.reserve(length) || !payloadStr.concat((const char *)payload, length)))
    {
        if (_enableSerialLogs)
            Serial.println("MQTT! Not enough memory to copy received message.");
        return;
    }
    String topicStr(topic);

    // Logging
    if (_enableSerialLogs)
        Serial.printf("MQTT >> [%s] %s\n", topic, payloadStr.c_str());

    // Send the message to subscribers
    for (std::size_t i = 0; i < _topicSubscriptionList.size(); i++)
    {
        // A callback may publish (reusing the MQTT buffer) or update subscriptions.
        TopicSubscriptionRecord record = _topicSubscriptionList[i];
        if (mqttTopicMatch(record.topic, topicStr))
        {
            if (record.callback)
            {
                record.callback(payloadStr);
            }
            if (record.callbackWithTopic)
            {
                record.callbackWithTopic(topicStr, payloadStr);
            }
            if (record.callbackJSON || record.callbackJSONWithTopic)
            {
                // Grow only when ArduinoJson needs more slots. A JSON array of
                // one-character values bounds the number of slots for this input.
                const size_t maxCapacity = JSON_ARRAY_SIZE(length / 2 + 1) + length + 1;
                size_t capacity = length + JSON_OBJECT_SIZE(8);
                while (true)
                {
                    DynamicJsonDocument doc(capacity);
                    DeserializationError error = deserializeJson(doc, payloadStr);
                    if (error == DeserializationError::NoMemory && doc.capacity() > 0 && capacity < maxCapacity)
                    {
                        capacity = capacity > maxCapacity / 2 ? maxCapacity : capacity * 2;
                        continue;
                    }
                    if (error || !doc.is<JsonObject>())
                    {
                        if (_enableSerialLogs)
                            Serial.printf("JSON deserialize error: %s\n", error ? error.c_str() : "expected object");
                        break;
                    }
                    JsonObject obj = doc.as<JsonObject>();
                    if (record.callbackJSON)
                        record.callbackJSON(obj);
                    if (record.callbackJSONWithTopic)
                        record.callbackJSONWithTopic(topicStr, obj);
                    break;
                }
            }
        }
    }
}

String ThingsCloudMQTT::getEspChipUniqueId()
{
    uint32_t chipId = 0;
    for (int i = 0; i < 17; i = i + 8)
    {
        chipId |= ((ESP_getChipId() >> (40 - i)) & 0xff) << i;
    }
    String uniqueId = String(chipId, HEX);
    uniqueId.toUpperCase();
    return uniqueId;
}

String ThingsCloudMQTT::bytesToHex(const uint8_t buf[], size_t size)
{
    String hex = "";
    for (int i = 0; i < size; i++)
    {
        if (buf[i] < 0x10)
        {
            hex += "0";
        }
        hex += String(buf[i], HEX);
    }
    return hex;
}
