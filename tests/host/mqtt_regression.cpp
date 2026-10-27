#include "Arduino.h"
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <iostream>
#include <stdexcept>

// Inspect state without adding test-only API to the production library.
#define private public
#include "ThingsCloudMQTT.h"
#undef private
#include "../../src/ThingsCloudMQTT.cpp"

SerialMock Serial;
EspMock ESP;
WiFiMock WiFi;
HttpMock Http;
unsigned long fakeMillis = 1000;
void onMQTTConnect() {}

#define CHECK(condition) do { if (!(condition)) throw std::runtime_error(#condition); } while (0)

static void online(ThingsCloudMQTT &client) {
    client._wifiConnected = true;
    client._mqttConnected = true;
    client._mqttClient.connectedState = true;
    WiFi.state = WL_CONNECTED;
}

static void receive(ThingsCloudMQTT &client, const String &payload) {
    char topic[] = "test/a";
    std::vector<uint8_t> bytes(payload.c_str(), payload.c_str() + payload.length());
    // Canary after the payload catches the old terminator write, including length zero.
    bytes.push_back(0x7f);
    auto original = bytes;
    client.mqttMessageReceivedCallback(topic, bytes.data(), static_cast<unsigned int>(payload.length()));
    CHECK(bytes == original);
}

static void certificates() {
    ThingsCloudMQTT client("host", "device", "project", "type", "http://api");
    const std::vector<std::pair<int, String>> failures = {
        {-1, "{}"}, {-11, "{}"}, {500, "{\"result\":1,\"device\":{\"access_token\":\"bad\"}}"},
        {302, "{}"}, {200, "invalid"}, {200, "{}"},
        {200, "{\"result\":0,\"message\":\"refused\"}"},
        {200, "{\"result\":1,\"device\":{}}"},
        {200, "{\"result\":1,\"device\":{\"access_token\":null}}"},
        {200, "{\"result\":1,\"device\":{\"access_token\":123}}"},
        {200, "{\"result\":1,\"device\":{\"access_token\":\"\"}}"},
        {200, "{\"result\":\"1\",\"device\":{\"access_token\":\"bad\"}}"}
    };
    client._accessToken = "previous";
    for (const auto &failure : failures) {
        Http = HttpMock(); Http.status = failure.first; Http.body = failure.second;
        CHECK(!client.fetchDeviceAccessToken());
        CHECK(client.getAccessToken() == "previous");
        CHECK(Http.ends == 1);
    }
    Http = HttpMock(); Http.beginOK = false;
    CHECK(!client.fetchDeviceAccessToken()); CHECK(Http.ends == 1); CHECK(Http.posts == 0);
    Http = HttpMock();
    String token(std::string(700, 't'));
    Http.body = String("{\"result\":1,\"device\":{\"name\":\"") + String(std::string(1500, 'x')) +
        "\",\"access_token\":\"" + token + "\"}}";
    CHECK(client.fetchDeviceAccessToken()); CHECK(client.getAccessToken() == token);
    CHECK(Http.ends == 1);
    ThingsCloudMQTT fixedToken("host", "token", "project");
    CHECK(!fixedToken.fetchDeviceAccessToken());
}

static void certificateRetry() {
    ThingsCloudMQTT client("host", "device", "project", "type", "http://api");
    Http = HttpMock(); Http.status = -1;
    WiFi.state = WL_CONNECTED;
    fakeMillis = 1000; client.loop();
    fakeMillis = 1501; client.loop();
    CHECK(!client._accessTokenFetched); CHECK(Http.posts == 1);
    fakeMillis = 2000; client.loop(); CHECK(Http.posts == 1);
    Http.status = 200;
    fakeMillis = 11502; client.loop();
    CHECK(client._accessTokenFetched); CHECK(Http.posts == 2);
    CHECK(client.getAccessToken() == "token");
}

static void connectionCallbacks() {
    ThingsCloudMQTT client("host", "token", "project");
    int wifiUp = 0, wifiDown = 0, mqttUp = 0, mqttDown = 0;
    CHECK(!client._handleWiFi);
    client.setOnWifiConnect([&]() { CHECK(client.isWifiConnected()); ++wifiUp; });
    client.setOnWifiDisconnect([&]() { CHECK(!client.isWifiConnected()); ++wifiDown; });
    client.setOnMQTTConnect([&]() { CHECK(client.isMqttConnected()); ++mqttUp; });
    client.setOnMQTTDisconnect([&]() { CHECK(!client.isMqttConnected()); ++mqttDown; });
    WiFi.state = WL_CONNECTED;
    client.loop(); client.loop();
    CHECK(wifiUp == 1);
    client._mqttClient.connectedState = true;
    client.loop(); client.loop(); CHECK(mqttUp == 1);
    client._mqttClient.connectedState = false;
    client.loop(); client.loop(); CHECK(mqttDown == 1);
    client._mqttClient.connectedState = true; client.loop(); CHECK(mqttUp == 2);
    WiFi.state = 0; client.loop(); client.loop();
    CHECK(wifiDown == 1); CHECK(mqttDown == 2);
    CHECK(!client.isConnected());
    online(client); client.disconnect(); client.disconnect(); CHECK(mqttDown == 3);
    client.setOnWifiConnect(nullptr); client.setOnWifiDisconnect(nullptr);
    client.setOnMQTTConnect(nullptr); client.setOnMQTTDisconnect(nullptr);
    client.onWiFiConnectionEstablished(); client.onWiFiConnectionLost();
    client.onMQTTConnectionEstablished(); client.onMQTTConnectionLost();
}

static void subscriptions() {
    ThingsCloudMQTT client("host", "token", "project"); online(client);
    int old = 0, current = 0, other = 0;
    CHECK(client.subscribe("test/a", MessageReceivedCallback([&](const String &) { ++old; })));
    CHECK(client.subscribe("test/b", MessageReceivedCallback([&](const String &) { ++other; })));
    CHECK(client.subscribe("test/a", MessageReceivedCallback([&](const String &) { ++current; })));
    receive(client, "{}"); CHECK(old == 0); CHECK(current == 1); CHECK(other == 0);
    CHECK(client.subscribe("test/a", MessageReceivedCallbackJSON([&](const JsonObject &obj) {
        CHECK(obj["v"] == 1); ++current;
    })));
    receive(client, "{\"v\":1}"); CHECK(current == 2); CHECK(other == 0);
    CHECK(client.subscribe("test/a", MessageReceivedCallbackWithTopic([&](const String &topic, const String &) {
        CHECK(topic == "test/a"); ++current;
    })));
    receive(client, "{}"); CHECK(current == 3); CHECK(other == 0);
    CHECK(client.subscribe("test/a", MessageReceivedCallbackJSONWithTopic([&](const String &topic, const JsonObject &) {
        CHECK(topic == "test/a"); ++current;
    })));
    receive(client, "{}"); CHECK(current == 4); CHECK(other == 0);
    CHECK(client._topicSubscriptionList.size() == 2);
    client._mqttClient.subscribeOK = false;
    CHECK(!client.subscribe("test/a", MessageReceivedCallback([&](const String &) { ++old; })));
    receive(client, "{}"); CHECK(current == 5); CHECK(old == 0);
    online(client); client._mqttClient.subscribeOK = true;
    CHECK(client.unsubscribe("test/a")); CHECK(client._topicSubscriptionList.size() == 1);
    char topic[] = "test/b"; uint8_t body[] = {'x', 0x7f};
    client.mqttMessageReceivedCallback(topic, body, 1); CHECK(other == 1);
}

static void messageSizes() {
    ThingsCloudMQTT client("host", "token", "project"); online(client);
    CHECK(client._mqttClient.getBufferSize() == 1024);
    CHECK(client.setMaxPacketSize(4096));
    CHECK(client.connectToMqttBroker()); CHECK(client._mqttClient.getBufferSize() == 4096);
    CHECK(client.connectToMqttBroker()); CHECK(client._mqttClient.getBufferSize() == 4096);
    CHECK(!client.setMaxPacketSize(0)); CHECK(client._mqttClient.getBufferSize() == 4096);
    int calls = 0;
    CHECK(client.subscribe("test/a", MessageReceivedCallback([&](const String &body) {
        CHECK(body.length() == 0 || body.length() == 4081); ++calls;
    })));
    receive(client, ""); receive(client, String(std::string(4081, 'x'))); CHECK(calls == 2);
    CHECK(client.subscribe("test/a", MessageReceivedCallbackJSON([&](const JsonObject &obj) {
        CHECK(obj.size() == 120); CHECK(obj["k119"] == 119); ++calls;
    })));
    std::string json = "{";
    for (int i = 0; i < 120; ++i) {
        if (i) json += ',';
        json += "\"k" + std::to_string(i) + "\":" + std::to_string(i);
    }
    json += '}'; receive(client, String(json)); CHECK(calls == 3);
    receive(client, "invalid"); receive(client, "[]"); CHECK(calls == 3);
    CHECK(client.subscribe("test/a", MessageReceivedCallbackJSONWithTopic([&](const String &, const JsonObject &obj) {
        CHECK(obj["text"].as<String>().length() == 2000); ++calls;
    })));
    receive(client, String("{\"text\":\"") + String(std::string(2000, 'x')) + "\"}");
    CHECK(calls == 4);
}

int main() {
    try {
        certificates(); std::cout << "PASS certificate validation and cleanup\n";
        certificateRetry(); std::cout << "PASS certificate retry after network failure\n";
        connectionCallbacks(); std::cout << "PASS connection callback transitions\n";
        subscriptions(); std::cout << "PASS subscription replacement and isolation\n";
        messageSizes(); std::cout << "PASS buffer persistence, payload boundaries and large JSON\n";
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
