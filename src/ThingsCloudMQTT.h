/*
  ThingsCloudMQTT.h - MQTT client for ThingsCloud.
  https://www.thingscloud.xyz
*/

#ifndef ThingsCloud_MQTT_H
#define ThingsCloud_MQTT_H

// #include <HTTPUpdate.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <vector>

#ifdef ESP8266

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#ifndef ESP_getChipId
#define ESP_getChipId() ESP.getChipId()
#endif

#else // for ESP32

#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#ifndef ESP_getChipId
#define ESP_getChipId() (uint64_t) ESP.getEfuseMac()
#endif

#endif

#define DEFAULT_MQTT_CLIENT_NAME "THINGSCLOUD_ESP32_ARDUINO_LIB"

const unsigned int mqttKeepAlive = 120;
const unsigned int socketTimeout = 300;

// MUST be implemented in your sketch. Called once device is connected to ThingsCloud.
void onMQTTConnect();

typedef std::function<void()> ConnectionStatusCallback;
typedef std::function<void(const String &message)> MessageReceivedCallback;
typedef std::function<void(const JsonObject &obj)> MessageReceivedCallbackJSON;
typedef std::function<void(const String &topicStr, const String &message)> MessageReceivedCallbackWithTopic;
typedef std::function<void(const String &topicStr, const JsonObject &obj)> MessageReceivedCallbackJSONWithTopic;
typedef std::function<void()> DelayedExecutionCallback;

class ThingsCloudMQTT
{
private:
    // Wifi related
    bool _handleWiFi;
    bool _wifiConnected;
    bool _connectingToWifi;
    unsigned long _lastWifiConnectiomAttemptMillis;
    unsigned long _nextWifiConnectionAttemptMillis;
    unsigned int _wifiReconnectionAttemptDelay;
    const char *_wifiSsid;
    const char *_wifiPassword;
    WiFiClient _wifiClient;

    // MQTT related
    bool _mqttConnected;
    unsigned long _nextMqttConnectionAttemptMillis;
    unsigned int _mqttReconnectionAttemptDelay;
    unsigned long _nextAccessTokenFetchAttemptMillis;
    unsigned int _accessTokenFetchAttemptDelay;
    const char *_mqttHost;
    String _accessToken;
    const char *_projectKey;
    const char *_apiEndpoint;
    String _deviceKey;
    String _typeKey;
    String _customerId;
    String _mqttClientName;
    short _mqttServerPort = 1883;
    bool _mqttCleanSession;
    char *_mqttLastWillTopic;
    char *_mqttLastWillMessage;
    bool _mqttLastWillRetain;
    unsigned int _failedMQTTConnectionAttemptCount;
    bool _needFetchAccessToken = false;
    bool _accessTokenFetched = false;

    PubSubClient _mqttClient;

    struct TopicSubscriptionRecord
    {
        String topic;
        MessageReceivedCallback callback;
        MessageReceivedCallbackJSON callbackJSON;
        MessageReceivedCallbackWithTopic callbackWithTopic;
        MessageReceivedCallbackJSONWithTopic callbackJSONWithTopic;
    };
    std::vector<TopicSubscriptionRecord> _topicSubscriptionList;

    // Delayed execution related
    struct DelayedExecutionRecord
    {
        unsigned long targetMillis;
        DelayedExecutionCallback callback;
    };
    std::vector<DelayedExecutionRecord> _delayedExecutionList;

    // General behaviour related
    ConnectionStatusCallback _onMQTTConnect;
    ConnectionStatusCallback _onMQTTDisconnect;
    ConnectionStatusCallback _onWifiConnect;
    ConnectionStatusCallback _onWifiDisconnect;
    bool _enableMQTTDisconnectCallback = false;
    bool _enableWifiConnectCallback = false;
    bool _enableWifiDisconnectCallback = false;
    bool _enableSerialLogs;
    bool _drasticResetOnConnectionFailures;
    unsigned int _connectionEstablishedCount; // Incremented before each _onMQTTConnect call

public:
    /// Wifi + MQTT with MQTT authentification
    ThingsCloudMQTT(
        const char *mqttHost,
        String accessToken,
        const char *projectKey);

    ThingsCloudMQTT(
        const char *mqttHost,
        String deviceKey,
        const char *projectKey,
        String typeKey,
        const char *apiEndpoint);

    ~ThingsCloudMQTT();

    void initState();

    // Optional functionality
    void enableDebuggingMessages(const bool enabled = true);                                    // Allow to display useful debugging messages. Can be set to false to disable them during program execution
    void enableDrasticResetOnConnectionFailures() { _drasticResetOnConnectionFailures = true; } // Can be usefull in special cases where the ESP board hang and need resetting (#59)

    /// Main loop, to call at each sketch loop()
    void loop();

    // Get ThingsCloud device accessToken by deviceKey
    bool fetchDeviceAccessToken();
    void setCustomerId(const String customerId);

    bool reportAttributes(const String attributes);
    bool reportEvent(const uint16_t id, const String event);
    bool reportData(const String &topic, const String &payload);
    bool reportData(const String &topic, const uint8_t *payload, unsigned int plength);
    bool getAttributes();
    bool onAttributesGetResponse(MessageReceivedCallbackWithTopic messageReceivedCallbackWithTopic);
    bool onAttributesGetResponse(MessageReceivedCallbackJSONWithTopic messageReceivedCallbackWithTopic);
    bool onAttributesResponse(MessageReceivedCallback messageReceivedCallback);
    bool onAttributesPush(MessageReceivedCallback messageReceivedCallback);
    bool onAttributesPush(MessageReceivedCallbackJSON messageReceivedCallback);
    bool onCommandSend(MessageReceivedCallbackWithTopic messageReceivedCallbackWithTopic);
    bool onCommandSend(MessageReceivedCallbackJSONWithTopic messageReceivedCallbackWithTopic);

    bool setMaxPacketSize(const uint16_t size);
    bool publish(const String &topic, const String &payload, bool retain = false);
    bool publish(const String &topic, const uint8_t *payload, unsigned int plength);
    bool subscribe(const String &topic, MessageReceivedCallback messageReceivedCallback, uint8_t qos = 0);
    bool subscribe(const String &topic, MessageReceivedCallbackJSON messageReceivedCallback, uint8_t qos = 0);
    bool subscribe(const String &topic, MessageReceivedCallbackWithTopic messageReceivedCallback, uint8_t qos = 0);
    bool subscribe(const String &topic, MessageReceivedCallbackJSONWithTopic messageReceivedCallback, uint8_t qos = 0);
    bool unsubscribe(const String &topic); // Unsubscribes from the topic, if it exists, and removes it from the CallbackList.

    // Wifi related
    void setWifiCredentials(const char *wifiSsid, const char *wifiPassword);

    // Other
    void executeDelayed(const unsigned long delay, DelayedExecutionCallback callback);

    inline const String getDeviceKey() const { return _deviceKey; };

    inline bool isConnected() const { return isWifiConnected() && isMqttConnected(); };                // Return true if everything is connected
    inline bool isWifiConnected() const { return _wifiConnected; };                                    // Return true if wifi is connected
    inline bool isMqttConnected() const { return _mqttConnected; };                                    // Return true if mqtt is connected
    inline unsigned int getConnectionEstablishedCount() const { return _connectionEstablishedCount; }; // Return the number of time onConnectionEstablished has been called since the beginning.

    inline void setOnMQTTConnect(ConnectionStatusCallback callback) { _onMQTTConnect = callback; };
    inline void setOnMQTTDisconnect(ConnectionStatusCallback callback) { _onMQTTDisconnect = callback; };
    inline void setOnWifiConnect(ConnectionStatusCallback callback) { _onWifiConnect = callback; };
    inline void setOnWifiDisconnect(ConnectionStatusCallback callback) { _onWifiDisconnect = callback; };

    // Allow to set the minimum delay between each MQTT reconnection attempt. 15 seconds by default.
    inline void setMqttReconnectionAttemptDelay(const unsigned int milliseconds) { _mqttReconnectionAttemptDelay = milliseconds; };

    // Allow to set the minimum delay between each WiFi reconnection attempt. 60 seconds by default.
    inline void setWifiReconnectionAttemptDelay(const unsigned int milliseconds) { _wifiReconnectionAttemptDelay = milliseconds; };

private:
    bool handleWiFi();
    bool handleMQTT();
    void onWiFiConnectionEstablished();
    void onWiFiConnectionLost();
    void onMQTTConnectionEstablished();
    void onMQTTConnectionLost();

    void connectToWifi();
    bool connectToMqttBroker();
    void processDelayedExecutionRequests();
    bool mqttTopicMatch(const String &topic1, const String &topic2);
    void mqttMessageReceivedCallback(char *topic, uint8_t *payload, unsigned int length);
    String getEspChipUniqueId();
    String bytesToHex(const uint8_t buf[], size_t size);
};

#endif
