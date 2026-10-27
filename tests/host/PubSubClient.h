#pragma once
#include "Arduino.h"
#include "WiFiClient.h"

class PubSubClient {
    uint16_t bufferSize = 256;
public:
    bool connectedState = false;
    bool subscribeOK = true;
    int subscriptions = 0;
    PubSubClient(const char *, short, WiFiClient &) {}
    void setCallback(std::function<void(char *, uint8_t *, unsigned int)>) {}
    bool setBufferSize(uint16_t size) {
        if (!size) return false;
        bufferSize = size;
        return true;
    }
    uint16_t getBufferSize() const { return bufferSize; }
    void loop() {}
    bool connected() const { return connectedState; }
    void disconnect() { connectedState = false; }
    bool subscribe(const char *, uint8_t) { ++subscriptions; return subscribeOK; }
    bool unsubscribe(const char *) { return true; }
    bool publish(const char *, const char *, bool) { return true; }
    bool publish(const char *, const uint8_t *, unsigned int) { return true; }
    void setServer(const char *, short) {}
    void setKeepAlive(unsigned int) {}
    void setSocketTimeout(unsigned int) {}
    bool connect(const char *, const char *, const char *, const char *, int, bool, const char *, bool) {
        connectedState = true;
        return true;
    }
    int state() const { return 0; }
};
