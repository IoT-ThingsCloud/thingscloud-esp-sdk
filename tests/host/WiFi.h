#pragma once
#include "Arduino.h"
#include "WiFiClient.h"

struct WiFiMock {
    int state = 0;
    int status() const { return state; }
    void disconnect(bool) { state = 0; }
    void mode(int) {}
    void setHostname(const char *) {}
    void hostname(const char *) {}
    void begin(const char *, const char *) {}
    struct IP { String toString() const { return "127.0.0.1"; } };
    IP localIP() const { return {}; }
};
extern WiFiMock WiFi;
