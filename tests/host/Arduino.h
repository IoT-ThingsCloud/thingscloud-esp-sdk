#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

#define HEX 16
#define WIFI_STA 1
#define WL_CONNECTED 3
#define WL_CONNECT_FAILED 4

class String {
    std::string value;
public:
    String() = default;
    String(const char *text) : value(text ? text : "") {}
    String(const std::string &text) : value(text) {}
    template<typename T> String(T number, int base = 10) {
        std::ostringstream stream;
        if (base == HEX) stream << std::hex;
        stream << number;
        value = stream.str();
    }
    const char *c_str() const { return value.c_str(); }
    size_t length() const { return value.size(); }
    bool reserve(unsigned int size) { value.reserve(size); return true; }
    bool concat(const char *data, unsigned int size) { value.append(data, size); return true; }
    bool concat(const char *data) { value.append(data); return true; }
    bool equals(const String &other) const { return value == other.value; }
    bool startsWith(const String &other) const { return value.compare(0, other.length(), other.value) == 0; }
    bool endsWith(const String &other) const {
        return value.size() >= other.value.size() &&
            value.compare(value.size() - other.value.size(), other.value.size(), other.value) == 0;
    }
    int indexOf(char ch) const {
        auto pos = value.find(ch);
        return pos == std::string::npos ? -1 : static_cast<int>(pos);
    }
    String substring(size_t from) const { return substring(from, length()); }
    String substring(size_t from, size_t to) const {
        if (from > to) std::swap(from, to);
        from = std::min(from, length());
        to = std::min(to, length());
        return value.substr(from, to - from);
    }
    void toUpperCase() {
        for (auto &ch : value) if (ch >= 'a' && ch <= 'z') ch -= 'a' - 'A';
    }
    String &operator+=(const String &other) { value += other.value; return *this; }
    friend String operator+(const String &a, const String &b) { return a.value + b.value; }
    friend bool operator==(const String &a, const String &b) { return a.value == b.value; }
    friend bool operator!=(const String &a, const String &b) { return !(a == b); }
};

struct SerialMock {
    template<typename... Args> void printf(const char *, Args...) {}
    template<typename T> void print(const T &) {}
    template<typename T> void println(const T &) {}
};
extern SerialMock Serial;
extern unsigned long fakeMillis;
inline unsigned long millis() { return fakeMillis; }

struct EspMock {
    uint64_t getEfuseMac() const { return UINT64_C(0xABCDEF123456); }
    uint32_t getChipId() const { return 0x123456; }
    void restart() {}
    void reset() {}
};
extern EspMock ESP;
