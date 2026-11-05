/*
  ThingsCloudMQTT.cpp - MQTT client for ThingsCloud.
  https://www.thingscloud.xyz
*/

#include "ThingsCloudMQTT.h"
#include <memory>
#include <limits>
#include "ThingsCloudProvisioningProtocol.h"
#if defined(ARDUINO_ARCH_ESP32)
#include <esp_random.h>
#include <WiFiClientSecure.h>
#if __has_include(<esp_arduino_version.h>)
#include <esp_arduino_version.h>
#endif
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#elif defined(ARDUINO_ARCH_ESP8266)
#include <WiFiClientSecureBearSSL.h>
extern "C" {
#include <osapi.h>
}
#endif

// =============== Constructor / destructor ===================

ThingsCloudMQTT::ThingsCloudMQTT(
    const char *mqttHost,
    String accessToken,
    const char *projectKey) : _mqttHostOwned(mqttHost?mqttHost:""), _projectKeyOwned(projectKey?projectKey:""), _mqttHost(_mqttHostOwned.c_str()),
                              _accessToken(accessToken),
                              _projectKey(_projectKeyOwned.c_str()),
                              _mqttClient(mqttHost, _mqttServerPort, _wifiClient)
{
    // WiFi connection
    _wifiConnected = false;
    _connectingToWifi = false;
    _lastWifiConnectiomAttemptMillis = 0;
    _wifiReconnectionAttemptDelay = 60 * 1000;

    initState();
}

ThingsCloudMQTT::ThingsCloudMQTT(
    const char *mqttHost,
    String deviceKey,
    const char *projectKey,
    String typeKey,
    const char *apiEndpoint) : _mqttHostOwned(mqttHost?mqttHost:""), _projectKeyOwned(projectKey?projectKey:""), _apiOwned(apiEndpoint?apiEndpoint:""), _mqttHost(_mqttHostOwned.c_str()),
                               _projectKey(_projectKeyOwned.c_str()),
                               _apiEndpoint(_apiOwned.c_str()),
                               _deviceKey(deviceKey),
                               _typeKey(typeKey),
                               _mqttClient(mqttHost, _mqttServerPort, _wifiClient)
{
    _wifiConnected = false;
    _connectingToWifi = false;
    _lastWifiConnectiomAttemptMillis = 0;
    _wifiReconnectionAttemptDelay = 60 * 1000;
    _needFetchAccessToken = true;
    if (_deviceKey == "")
    {
        _deviceKey = getEspChipUniqueId();
    }

    initState();
}

ThingsCloudMQTT::ThingsCloudMQTT(const TCManufacturerConfig& config)
    : ThingsCloudMQTT("", config.device_key, "", "", "") {
    _provisioningMode=TCProvisioningMode::Manufacturer;
    _productKey=config.product_key;_needFetchAccessToken=false;
    // Global sketch objects are constructed before initArduino initializes NVS.
    // Restore only from the application loop (or an explicit setup-time call).
    _credentialsRestorePending=true;_boundedProvisioningMQTT=true;
}
ThingsCloudMQTT::~ThingsCloudMQTT()
{
    if(_claimJob) {
        _claimJob->cancelled.store(true);
#if defined(ARDUINO_ARCH_ESP32)
        while(!_claimJob->done.load(std::memory_order_acquire))delay(1);
#endif
        delete _claimJob;
    }
#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
    _mqttClient.disconnect();
#endif
}

void ThingsCloudMQTT::initState()
{
    // MQTT client
    _mqttClientName = String(DEFAULT_MQTT_CLIENT_NAME) + "_" + getEspChipUniqueId();
    _mqttConnected = false;
    _mqttRetry.cancel();
    _accessTokenRetry.cancel();
    _mqttReconnectionAttemptDelay = 30 * 1000;
    _mqttFixedReconnectionDelay = false;
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

    bool configuredProject=_projectMQTTEndpoint.length()>0;
    if(configuredProject) {
        String host;uint16_t port;
        if(!tcParseEndpoint(_apiOwned,false,_allowPlaintext,host,port))return false;
    }
#if defined(ARDUINO_ARCH_ESP32)
    std::unique_ptr<WiFiClientSecure> httpsClient;
#elif defined(ARDUINO_ARCH_ESP8266)
    std::unique_ptr<BearSSL::WiFiClientSecure> httpsClient;
#endif
    HTTPClient http;
    if (_enableSerialLogs)
        Serial.println("Request device certificate");
    String serverPath = String(_apiEndpoint) + "/device/v1/certificate";
    bool started=false;
#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
    if(configuredProject) {
        http.setTimeout(10000);
#if defined(ARDUINO_ARCH_ESP32)
        http.setConnectTimeout(5000);
#endif
        if(_apiOwned.startsWith("https://")) {
#if defined(ARDUINO_ARCH_ESP32)
            httpsClient.reset(new WiFiClientSecure());
            httpsClient->setHandshakeTimeout(5);
#else
            httpsClient.reset(new BearSSL::WiFiClientSecure());
#endif
            // HTTPS encryption is retained without CA configuration or server
            // certificate verification. MQTT always uses the TCP client.
            httpsClient->setInsecure();
            started=http.begin(*httpsClient,serverPath.c_str());
        } else started=http.begin(_wifiClient,serverPath.c_str());
    } else
#endif
    {
#ifdef ESP32
        started=http.begin(serverPath.c_str());
#else
        started=http.begin(_wifiClient,serverPath.c_str());
#endif
    }
    if (!started)
    {
        if (_enableSerialLogs)
        {
            Serial.printf("Certificate HTTP init failed: api_len=%u path_len=%u api_pointer_current=%d\n",
                          (unsigned)_apiOwned.length(), (unsigned)serverPath.length(), _apiEndpoint == _apiOwned.c_str());
#if defined(ARDUINO_ARCH_ESP32)
            Serial.printf("Certificate heap: free=%u largest=%u\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
#endif
        }
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
#if ARDUINOJSON_VERSION_MAJOR >= 7
    JsonDocument filter;
    JsonDocument doc;
#else
    StaticJsonDocument<128> filter;
    DynamicJsonDocument doc(response.length() + JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(1) + 1);
#endif
    filter["result"] = true;
    filter["device"]["access_token"] = true;
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
    // Legacy SoftAP re-provisioning must refresh the current user even when a
    // certificate was already fetched during an earlier provisioning round.
    _accessTokenFetched = false;
    _accessTokenRetry.cancel();
    if (_enableSerialLogs)
        Serial.println("Claim customer context updated");
}

// =============== Main loop / connection state handling =================

void ThingsCloudMQTT::loop()
{
    if(_credentialsRestorePending)restoreManufacturerCredentials();
    processClaimRegistration();
    // Drive linked SoftAP recovery for legacy sketches that only call client.loop().
    if (auto maintenance = _wifiMaintenance.lock()) (*maintenance)();
    // WIFI handling
    bool wifiStateChanged = handleWiFi();

    // If there is a change in the wifi connection state, don't handle the mqtt connection state right away.
    // We will wait at least one lopp() call. This prevent the library from doing too much thing in the same loop() call.
    if (wifiStateChanged)
        return;

    // Fetch AccessToken
    if (!isWifiConnected())
        return;
    if (_claimJob || _claimRequired) return;
    if (_needFetchAccessToken && !_accessTokenFetched)
    {
        if (_accessTokenRetry.active && !_accessTokenRetry.due())
            return;

        bool ret = fetchDeviceAccessToken();
        if (ret)
        {
            _accessTokenFetched = true;
            _accessTokenRetry.cancel();
        }
        else
        {
            _accessTokenRetry.schedule(_accessTokenFetchAttemptDelay);
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
    if (_handleWiFi && !_wifiLoopStarted)
    {
        WiFi.disconnect(true);
        _wifiRetry.schedule(500);
        _wifiLoopStarted = true;
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
        // WiFi recovery must not reset MQTT failures or bypass its backoff.
        if (_failedMQTTConnectionAttemptCount || _connectionEstablishedCount)
            scheduleMqttRetry();
        else
            _mqttRetry.schedule(500);
        _accessTokenRetry.schedule(500);
    }

    // Connection in progress
    else if (_connectingToWifi)
    {
        if (WiFi.status() == WL_CONNECT_FAILED || millis() - _lastWifiConnectiomAttemptMillis >= _wifiReconnectionAttemptDelay)
        {
            if (_enableSerialLogs)
                Serial.printf("WiFi! Connection attempt failed, delay expired. (%fs). \n", millis() / 1000.0);

            WiFi.disconnect(true);
            _wifiRetry.schedule(500);
            _connectingToWifi = false;
        }
    }

    // Connection lost
    else if (!isWifiConnected && _wifiConnected)
    {
        _wifiConnected = false;
        onWiFiConnectionLost();

        if (_handleWiFi)
            _wifiRetry.schedule(500);
    }

    // Connected since at least one loop() call
    else if (isWifiConnected && _wifiConnected)
    {
    }

    // Disconnected since at least one loop() call
    // Then, if we handle the wifi reconnection process and the waiting delay has expired, we connect to wifi
    else if (_handleWiFi && _wifiRetry.due())
    {
        connectToWifi();
        _wifiRetry.cancel();
        _connectingToWifi = true;
        _lastWifiConnectiomAttemptMillis = millis();
    }

    /**** Detect and return if there was a change in the WiFi state ****/

    return stateChanged;
}

uint32_t ThingsCloudMQTT::mqttRetryDelay() const
{
    uint32_t base = _mqttReconnectionAttemptDelay;
    if (!_mqttFixedReconnectionDelay)
    {
        if (_failedMQTTConnectionAttemptCount >= 10)
            base = 120000;
        else if (_failedMQTTConnectionAttemptCount >= 5)
            base = 60000;
    }
    // Never overflow even when an application supplies a very large delay.
    const uint32_t headroom = std::numeric_limits<uint32_t>::max() - base;
    const uint32_t jitterLimit = base / 5 < headroom ? base / 5 : headroom;
#if defined(ARDUINO_ARCH_ESP32)
    const uint32_t noise = esp_random();
#elif defined(ARDUINO_ARCH_ESP8266)
    const uint32_t noise = os_random();
#else
    const uint32_t noise = static_cast<uint32_t>(random(static_cast<long>(jitterLimit) + 1));
#endif
    return base + noise % (jitterLimit + 1);
}

void ThingsCloudMQTT::scheduleMqttRetry()
{
    _mqttRetry.schedule(mqttRetryDelay());
    if (_enableSerialLogs)
        Serial.printf("MQTT: Retrying in %lu ms after %u consecutive failures.\n",
                      static_cast<unsigned long>(_mqttRetry.delayMillis), _failedMQTTConnectionAttemptCount);
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
        _failedMQTTConnectionAttemptCount = 0;
        _mqttRetry.cancel();
        onMQTTConnectionEstablished();
    }

    // Connection lost
    else if (!isMqttConnected && _mqttConnected)
    {
        _mqttConnected = false;
        onMQTTConnectionLost();
        scheduleMqttRetry();
    }

    // It's time to  connect to the MQTT broker
    else if (!isMqttConnected && isWifiConnected() && _mqttRetry.due())
    {
        // Connect to MQTT broker
        if (connectToMqttBroker())
        {
            _failedMQTTConnectionAttemptCount = 0;
            _mqttRetry.cancel();
        }
        else
        {
            // Connection failed, plan another connection attempt
            _mqttClient.disconnect();
            if (_failedMQTTConnectionAttemptCount < std::numeric_limits<unsigned int>::max())
                _failedMQTTConnectionAttemptCount++;
            scheduleMqttRetry();

            if (_enableSerialLogs)
                Serial.printf("MQTT!: Failed MQTT connection count: %u \n", _failedMQTTConnectionAttemptCount);

            // When there is too many failed attempt, sometimes it help to reset the WiFi connection or to restart the board.
            if (_handleWiFi && _failedMQTTConnectionAttemptCount % 8 == 0 &&
                (!_drasticResetOnConnectionFailures || _failedMQTTConnectionAttemptCount == 8))
            {
                if (_enableSerialLogs)
                    Serial.println("MQTT!: Can't connect to broker after too many attempt, resetting WiFi ...");

                WiFi.disconnect(true);
                _wifiRetry.schedule(500);
            }
            else if (_drasticResetOnConnectionFailures && _failedMQTTConnectionAttemptCount == 12)
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
        scheduleMqttRetry();
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
    _wifiMaintenance.reset();
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

    if (!configureMQTTTransport()) return false;
    if (_mqttHost != nullptr && strlen(_mqttHost) > 0)
    {
        if (_enableSerialLogs)
        {
            Serial.printf("MQTT: Connecting (%fs)", millis() / 1000.0);
        }

        // Bound new provisioning clients without changing legacy MQTT defaults.
#if defined(ARDUINO_ARCH_ESP32)
        if(_boundedProvisioningMQTT) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
            _wifiClient.setConnectionTimeout(5000);
#else
            _wifiClient.setTimeout(5);
#endif
        }
#endif
        // explicitly set the server/port here in case they were not provided in the constructor
        _mqttClient.setServer(_mqttHost, _mqttServerPort);
        _mqttClient.setKeepAlive(mqttKeepAlive);
        _mqttClient.setSocketTimeout(_boundedProvisioningMQTT?10:socketTimeout);
        String username = _provisioningMode == TCProvisioningMode::Manufacturer
            ? String("cert_type=device_secret&project_id=") + tcQueryEncode(_credentials.project_id) + "&access_token=" + tcQueryEncode(_credentials.access_token) : _accessToken;
        const char* password = _provisioningMode == TCProvisioningMode::Manufacturer ? _credentials.device_secret.c_str() : _projectKey;
        success = _mqttClient.connect(_mqttClientName.c_str(), username.c_str(), password, _mqttLastWillTopic, 0, _mqttLastWillRetain, _mqttLastWillMessage, _mqttCleanSession);
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
#if ARDUINOJSON_VERSION_MAJOR < 7
                // Grow only when ArduinoJson needs more slots. A JSON array of
                // one-character values bounds the number of slots for this input.
                const size_t maxCapacity = JSON_ARRAY_SIZE(length / 2 + 1) + length + 1;
                size_t capacity = length + JSON_OBJECT_SIZE(8);
#endif
                while (true)
                {
#if ARDUINOJSON_VERSION_MAJOR >= 7
                    // ArduinoJson 7 grows automatically; retrying cannot fix OOM.
                    JsonDocument doc;
#else
                    DynamicJsonDocument doc(capacity);
#endif
                    DeserializationError error = deserializeJson(doc, payloadStr);
#if ARDUINOJSON_VERSION_MAJOR < 7
                    if (error == DeserializationError::NoMemory && doc.capacity() > 0 && capacity < maxCapacity)
                    {
                        capacity = capacity > maxCapacity / 2 ? maxCapacity : capacity * 2;
                        continue;
                    }
#endif
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
    for (size_t i = 0; i < size; i++)
    {
        if (buf[i] < 0x10)
        {
            hex += "0";
        }
        hex += String(buf[i], HEX);
    }
    return hex;
}


bool ThingsCloudMQTT::validProvisioningConfiguration() const {
    if(!_deviceKey.length())return false;
    if(_provisioningMode==TCProvisioningMode::Manufacturer)return tcValidProductKey(_productKey);
    bool valid=_needFetchAccessToken && _projectKeyOwned.length() && _typeKey.length() && _apiOwned.length() && _mqttHostOwned.length();
    if(valid&&_projectMQTTEndpoint.length()) {String host;uint16_t port;valid=tcParseEndpoint(_apiOwned,false,_allowPlaintext,host,port);}
    return valid;
}
TCProvResult ThingsCloudMQTT::setWiFiMaintenance(const std::shared_ptr<std::function<void()>>& maintenance) {
    if (_wifiLoopStarted) return {TCProvResultCode::InvalidState};
    _handleWiFi = false;
    _connectingToWifi = false;
    _wifiRetry.cancel();
    _wifiMaintenance = maintenance;
    // WiFi ownership does not change legacy MQTT connection/socket timeouts.
    return {TCProvResultCode::Ok};
}
TCProvResult ThingsCloudMQTT::setWiFiManagementMode(TCWiFiManagement mode) {
    if(_wifiLoopStarted&&mode==TCWiFiManagement::External)return {TCProvResultCode::InvalidState};
    _wifiMaintenance.reset();
    _handleWiFi=mode==TCWiFiManagement::Internal;if(mode==TCWiFiManagement::External)_boundedProvisioningMQTT=true;return {TCProvResultCode::Ok};
}
void ThingsCloudMQTT::setCredentialStorage(std::function<bool(String&)> load,std::function<bool(const String&)> save,std::function<bool()> clear) {
    _loadCredentials=load;_saveCredentials=save;_clearCredentials=clear;
}
TCProvResult ThingsCloudMQTT::clearPlatformCredentials() {
    if(_provisioningMode!=TCProvisioningMode::Manufacturer)return {TCProvResultCode::Unsupported};
    // The worker never writes storage; cancellation also rejects an already
    // completed result when loop() next consumes it. Keep its owner until done.
    if(_claimJob)_claimJob->cancelled.store(true);
    _claimCallback=nullptr;_claimRequired=true;_credentialsRestorePending=false;_credentialsInvalidated=true;
    _mqttClient.disconnect();_mqttConnected=false;
    _credentials=TCDeviceCredentials();_accessToken="";_accessTokenFetched=false;_customerId="";
    _mqttHostOwned="";_mqttHost=_mqttHostOwned.c_str();_mqttServerPort=1883;
    _apiOwned="";_apiEndpoint=_apiOwned.c_str();
    bool cleared;
    if(_loadCredentials||_saveCredentials||_clearCredentials) {
        if(!_loadCredentials||(!_clearCredentials&&!_saveCredentials))return {TCProvResultCode::Unsupported};
        cleared=_clearCredentials?_clearCredentials():_saveCredentials(String());
        if(cleared) {String remaining;cleared=!_loadCredentials(remaining)||remaining.length()==0;}
    } else cleared=tcClearCredentialsBlob();
    return {cleared?TCProvResultCode::Ok:TCProvResultCode::InternalError};
}
bool ThingsCloudMQTT::restoreManufacturerCredentials() {
    _credentialsRestorePending=false;
    if(_provisioningMode!=TCProvisioningMode::Manufacturer||_credentialsInvalidated)return false;
    String blob;TCDeviceCredentials c;
    bool loaded=_loadCredentials?_loadCredentials(blob):tcLoadCredentialsBlob(blob,_deviceKey);
    return loaded&&tcDecodeCredentials(blob,_deviceKey,_productKey,_allowPlaintext,c)&&applyManufacturerCredentials(c);
}
bool ThingsCloudMQTT::applyManufacturerCredentials(const TCDeviceCredentials& c) {
    String host;uint16_t port;
    if(!tcCredentialsValid(c,_allowPlaintext)||!tcParseEndpoint(c.mqtt_endpoint,true,_allowPlaintext,host,port))return false;
    _mqttClient.disconnect();_mqttConnected=false;
    _credentials=c;_accessToken=c.access_token;_accessTokenFetched=true;
    _mqttHostOwned=host;_mqttHost=_mqttHostOwned.c_str();_mqttServerPort=port;
    _apiOwned=c.device_api_endpoint;_apiEndpoint=_apiOwned.c_str();return true;
}
TCProvResult ThingsCloudMQTT::setMQTTEndpoint(const String& endpoint) {
    if(_provisioningMode!=TCProvisioningMode::Project)return {TCProvResultCode::Unsupported};
    String host;uint16_t port;
    if(!tcParseEndpoint(endpoint,true,_allowPlaintext,host,port))return {TCProvResultCode::InvalidArgument};
    _mqttClient.disconnect();_mqttConnected=false;
    _projectMQTTEndpoint=endpoint;_mqttHostOwned=host;_mqttHost=_mqttHostOwned.c_str();_mqttServerPort=port;
    _boundedProvisioningMQTT=true;
    if(!_mqttRetry.active)_mqttRetry.schedule(_failedMQTTConnectionAttemptCount?mqttRetryDelay():500);
    return {TCProvResultCode::Ok};
}
bool ThingsCloudMQTT::configureMQTTTransport() {
    if(_provisioningMode==TCProvisioningMode::Manufacturer&&
       !tcCredentialsValid(_credentials,_allowPlaintext))return false;
    return true;
}
TCProvResult ThingsCloudMQTT::requestClaimRegistration(const TCClaimRegistrationContext& c,TCClaimRegistrationCallback callback) {
    if(c.provisioning_mode!=_provisioningMode||!tcValidRequestId(c.request_id)||c.timeout_ms<1000||c.timeout_ms>60000||!callback)return {TCProvResultCode::InvalidArgument};
    if(!validProvisioningConfiguration())return {TCProvResultCode::Unsupported};
    if(_claimJob) {
        if(_claimJob->cancelled.load())return {TCProvResultCode::InvalidState};
        const auto& old=_claimJob->context;
        if(old.request_id==c.request_id && old.customer_id==c.customer_id && old.provisioning_token==c.provisioning_token &&
           old.device_api_endpoint==c.device_api_endpoint&&old.mqtt_endpoint==c.mqtt_endpoint)return {TCProvResultCode::Ok};
        return {TCProvResultCode::Busy};
    }
    for(const auto& id:_completedRequestIds)if(id==c.request_id)return {TCProvResultCode::InvalidState};
    if(_completedRequestIds.size()>=32)return {TCProvResultCode::RestartRequired};
    if(WiFi.status()!=WL_CONNECTED)return {TCProvResultCode::InvalidState};
    if(_provisioningMode==TCProvisioningMode::Project) {
        if(!tcValidCustomerId(c.customer_id)||c.provisioning_token.length()||c.device_api_endpoint.length()||c.mqtt_endpoint.length())return {TCProvResultCode::InvalidArgument};
    } else {
        String host;uint16_t port;
        if(c.customer_id.length()||!c.provisioning_token.length()||c.provisioning_token.length()>4096||
           !tcParseEndpoint(c.device_api_endpoint,false,_allowPlaintext,host,port)||!tcParseEndpoint(c.mqtt_endpoint,true,_allowPlaintext,host,port))return {TCProvResultCode::InvalidArgument};
    }
    if(_provisioningMode==TCProvisioningMode::Manufacturer) {
        auto cleared=clearPlatformCredentials();if(!cleared.ok())return cleared;
    }
    auto* job=new TCCredentialJob();job->context=c;job->deviceKey=_deviceKey;
    job->productKey=_provisioningMode==TCProvisioningMode::Manufacturer?_productKey:_typeKey;
    job->projectKey=_projectKeyOwned;job->api=_provisioningMode==TCProvisioningMode::Manufacturer?c.device_api_endpoint:_apiOwned;
    job->allowPlaintext=_allowPlaintext;job->debug=_enableSerialLogs;
    job->started=millis();job->budget=c.timeout_ms;
    _claimJob=job;_claimCallback=callback;_claimRequired=true;_boundedProvisioningMQTT=true;
#if defined(ARDUINO_ARCH_ESP32)
    if(xTaskCreate([](void* arg){tcRunCredentialJob(static_cast<TCCredentialJob*>(arg));vTaskDelete(nullptr);},"tc-certificate",8192,job,1,nullptr)!=pdPASS) {
        _claimJob=nullptr;_claimCallback=nullptr;delete job;return {TCProvResultCode::InternalError};
    }
#endif
    return {TCProvResultCode::Ok};
}
void ThingsCloudMQTT::cancelClaimRegistration(const String& id, bool keepRequired) {
    if(_claimJob&&_claimJob->context.request_id==id) {
        _claimJob->cancelled.store(true);_claimCallback=nullptr;
        if(!keepRequired)_claimRequired=false;
    }
}
void ThingsCloudMQTT::processClaimRegistration() {
    if(!_claimJob)return;
#if !defined(ARDUINO_ARCH_ESP32)
    if(!_claimJob->done.load()&&(!_claimJob->nextAttempt||(int32_t)(millis()-_claimJob->nextAttempt)>=0))tcRunCredentialJob(_claimJob);
#endif
    auto* job=_claimJob;if(!job->done.load(std::memory_order_acquire))return;
    if(job->cancelled.load()) {_completedRequestIds.push_back(job->context.request_id);delete job;_claimJob=nullptr;_claimCallback=nullptr;return;}
#if !defined(ARDUINO_ARCH_ESP32)
    bool transient=job->httpStatus<=0||job->httpStatus>=500||job->httpStatus==408||job->httpStatus==429;
    if(!job->success&&job->error!=TCProvError::CertificateRejected&&transient&&job->context.provisioning_mode==TCProvisioningMode::Project&&
       (uint32_t)(millis()-job->started)+10000<job->budget) {
        job->nextAttempt=millis()+10000;job->done.store(false);return;
    }
#endif
    TCClaimRegistrationResult result;result.request_id=job->context.request_id;result.success=job->success;result.error=job->error;result.http_status=job->httpStatus;
    if(result.success) {
        if(_provisioningMode==TCProvisioningMode::Manufacturer) {
            String blob=tcEncodeCredentials(job->credentials,_deviceKey,_productKey);
            bool saved=blob.length()&&(_saveCredentials?_saveCredentials(blob):tcSaveCredentialsBlob(blob,_deviceKey));
            if(!saved||!applyManufacturerCredentials(job->credentials)) {result.success=false;result.error=TCProvError::InternalError;}
            else _credentialsInvalidated=false;
        } else {_accessToken=job->credentials.access_token;_accessTokenFetched=true;_customerId="";}
        if(result.success)_claimRequired=false;
    }
    _completedRequestIds.push_back(result.request_id);
    auto callback=_claimCallback;_claimCallback=nullptr;_claimJob=nullptr;delete job;
    if(callback)callback(result);
}
