#pragma once
#include "Arduino.h"
#include "WiFiClient.h"

struct HttpMock {
    bool beginOK = true;
    int status = 200;
    String body = "{\"result\":1,\"device\":{\"access_token\":\"token\"}}";
    int ends = 0;
    int posts = 0;
};
extern HttpMock Http;

class HTTPClient {
public:
    bool begin(const char *) { return Http.beginOK; }
    bool begin(WiFiClient &, const char *) { return Http.beginOK; }
    void addHeader(const char *, const char *) {}
    int POST(const String &) { ++Http.posts; return Http.status; }
    String getString() { return Http.body; }
    void end() { ++Http.ends; }
};
