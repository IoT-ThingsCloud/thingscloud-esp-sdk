/*
  ThingsCloudMQTT.h - MQTT client for ThingsCloud.
  https://www.thingscloud.xyz
*/

#ifndef ThingsCloud_MQTT_H
#define ThingsCloud_MQTT_H

#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <vector>
#include <memory>
#include "ThingsCloudCredentials.h"

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
    friend class ThingsCloudWiFiManager;
    // A weak lease avoids retaining the manager or calling it after destruction.
    std::weak_ptr<std::function<void()>> _wifiMaintenance;
    TCProvResult setWiFiMaintenance(const std::shared_ptr<std::function<void()>>& maintenance);

    // Elapsed-time comparisons survive 32-bit millis() wrap; zero is a valid time.
    struct ConnectionRetryTimer
    {
        uint32_t startedMillis = 0;
        uint32_t delayMillis = 0;
        bool active = false;
        void schedule(uint32_t delay)
        {
            startedMillis = static_cast<uint32_t>(millis());
            delayMillis = delay;
            active = true;
        }
        void cancel() { active = false; }
        bool due() const
        {
            return active && static_cast<uint32_t>(static_cast<uint32_t>(millis()) - startedMillis) >= delayMillis;
        }
    };

    String _mqttHostOwned, _projectKeyOwned, _apiOwned;
    TCProvisioningMode _provisioningMode = TCProvisioningMode::Project;
    String _productKey, _projectMQTTEndpoint;
    bool _allowPlaintext = false, _claimRequired = false;
    bool _wifiLoopStarted = false;
    bool _credentialsRestorePending = false;
    bool _credentialsInvalidated = false;
    bool _boundedProvisioningMQTT = false;
    TCDeviceCredentials _credentials;
    TCCredentialJob* _claimJob = nullptr;
    TCClaimRegistrationCallback _claimCallback;
    std::vector<String> _completedRequestIds;
    std::function<bool(String&)> _loadCredentials;
    std::function<bool(const String&)> _saveCredentials;
    std::function<bool()> _clearCredentials;
    void processClaimRegistration();
    bool applyManufacturerCredentials(const TCDeviceCredentials& credentials);
    bool configureMQTTTransport();
    // Wifi related
    bool _handleWiFi = false;
    bool _wifiConnected;
    bool _connectingToWifi;
    unsigned long _lastWifiConnectiomAttemptMillis;
    ConnectionRetryTimer _wifiRetry;
    unsigned int _wifiReconnectionAttemptDelay;
    const char *_wifiSsid;
    const char *_wifiPassword;
    WiFiClient _wifiClient;

    // MQTT related
    bool _mqttConnected;
    ConnectionRetryTimer _mqttRetry;
    unsigned int _mqttReconnectionAttemptDelay;
    bool _mqttFixedReconnectionDelay = false;
    ConnectionRetryTimer _accessTokenRetry;
    unsigned int _accessTokenFetchAttemptDelay;
    const char *_mqttHost;
    String _accessToken;
    const char *_projectKey;
    const char *_apiEndpoint = nullptr;
    String _deviceKey;
    String _typeKey;
    String _customerId;
    String _mqttClientName;
    uint16_t _mqttServerPort = 1883;
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

    explicit ThingsCloudMQTT(const TCManufacturerConfig& config);
    ~ThingsCloudMQTT();
    ThingsCloudMQTT(const ThingsCloudMQTT&) = delete;
    ThingsCloudMQTT& operator=(const ThingsCloudMQTT&) = delete;

    TCProvisioningMode provisioningMode() const { return _provisioningMode; }
    const String& productKey() const { return _productKey; }
    bool validProvisioningConfiguration() const;
    TCProvResult setWiFiManagementMode(TCWiFiManagement mode);
    TCProvResult requestClaimRegistration(const TCClaimRegistrationContext& context, TCClaimRegistrationCallback callback);
    void cancelClaimRegistration(const String& request_id, bool keepRequired = false);
    void setClaimRegistrationRequired(bool required) { _claimRequired = required; }
    // Optional Project MQTT URI (mqtt://host:port). Legacy raw-host constructors
    // retain their original TCP behavior unless this setup-time API is called.
    TCProvResult setMQTTEndpoint(const String& endpoint);
    // Allow HTTP device API endpoints for local development. MQTT uses TCP.
    void allowInsecureDevelopment(bool plaintext) { _allowPlaintext=plaintext; }
    bool allowsPlaintextDevelopment() const { return _allowPlaintext; }
    // Existing two-callback stores may clear by saving an empty record. An
    // optional clear callback can remove it instead; load must then return empty.
    void setCredentialStorage(std::function<bool(String&)> load, std::function<bool(const String&)> save,
                              std::function<bool()> clear = nullptr);
    bool restoreManufacturerCredentials();
    // Manufacturer only. Stops MQTT, cancels pending registration and removes
    // persisted + cached platform credentials. Does not clear WiFi or restart.
    TCProvResult clearPlatformCredentials();

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

    // Includes MQTT headers and topic. Preserved across reconnects; defaults to 1024.
    bool setMaxPacketSize(const uint16_t size);
    bool publish(const String &topic, const String &payload, bool retain = false);
    bool publish(const String &topic, const uint8_t *payload, unsigned int plength);
    bool subscribe(const String &topic, MessageReceivedCallback messageReceivedCallback, uint8_t qos = 0);
    bool subscribe(const String &topic, MessageReceivedCallbackJSON messageReceivedCallback, uint8_t qos = 0);
    bool subscribe(const String &topic, MessageReceivedCallbackWithTopic messageReceivedCallback, uint8_t qos = 0);
    bool subscribe(const String &topic, MessageReceivedCallbackJSONWithTopic messageReceivedCallback, uint8_t qos = 0);
    bool unsubscribe(const String &topic); // Unsubscribes from the topic, if it exists, and removes it from the CallbackList.
    void disconnect();                     // Gracefully disconnect from the MQTT broker

    // Wifi related
    void setWifiCredentials(const char *wifiSsid, const char *wifiPassword);

    // Other
    void executeDelayed(const unsigned long delay, DelayedExecutionCallback callback);

    inline const String getDeviceKey() const { return _deviceKey; };
    inline const String getAccessToken() const { return _accessToken; };

    inline bool isConnected() const { return isWifiConnected() && isMqttConnected(); };                // Return true if everything is connected
    inline bool isWifiConnected() const { return _wifiConnected; };                                    // Return true if wifi is connected
    inline bool isMqttConnected() const { return _mqttConnected; };                                    // Return true if mqtt is connected
    inline unsigned int getConnectionEstablishedCount() const { return _connectionEstablishedCount; }; // Return the number of time onConnectionEstablished has been called since the beginning.

    inline void setOnMQTTConnect(ConnectionStatusCallback callback) { _onMQTTConnect = callback; };
    inline void setOnMQTTDisconnect(ConnectionStatusCallback callback) { _onMQTTDisconnect = callback; };
    inline void setOnWifiConnect(ConnectionStatusCallback callback) { _onWifiConnect = callback; };
    inline void setOnWifiDisconnect(ConnectionStatusCallback callback) { _onWifiDisconnect = callback; };

    // Override the default 30/60/120-second staged backoff with a fixed base delay.
    // Minimum 1000 ms; each retry adds 0-20% hardware-random jitter.
    inline void setMqttReconnectionAttemptDelay(const unsigned int milliseconds)
    {
        _mqttReconnectionAttemptDelay = milliseconds < 1000 ? 1000 : milliseconds;
        _mqttFixedReconnectionDelay = true;
    }

    // Allow to set the minimum delay between each WiFi reconnection attempt. 60 seconds by default.
    inline void setWifiReconnectionAttemptDelay(const unsigned int milliseconds) { _wifiReconnectionAttemptDelay = milliseconds; };

private:
    bool handleWiFi();
    bool handleMQTT();
    uint32_t mqttRetryDelay() const;
    void scheduleMqttRetry();
    void onWiFiConnectionEstablished();
    void onWiFiConnectionLost();
    void onMQTTConnectionEstablished();
    void onMQTTConnectionLost();

    void connectToWifi();
    bool connectToMqttBroker();
    void processDelayedExecutionRequests();
    bool subscribeRecord(const TopicSubscriptionRecord &record, uint8_t qos);
    bool mqttTopicMatch(const String &topic1, const String &topic2);
    void mqttMessageReceivedCallback(char *topic, uint8_t *payload, unsigned int length);
    String getEspChipUniqueId();
    String bytesToHex(const uint8_t buf[], size_t size);
};

#endif
