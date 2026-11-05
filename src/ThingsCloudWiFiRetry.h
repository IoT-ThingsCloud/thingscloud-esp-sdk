#pragma once
#include <stdint.h>

// Shared by the provisioning transports. All time comparisons tolerate millis()
// rollover. The owner executes actions in its loop, never in WiFi callbacks.
class TCWiFiRetry {
public:
    enum class Action { None, Connect, Disconnect };

    void reset() { _state = State::Idle; _nextDelay = 5000; }
    void seed(uint32_t value) { _random = value ? value : 1; }
    void started(uint32_t now) { _state = State::Connecting; _since = now; }

    Action update(uint32_t now, bool connected, uint32_t timeout = 15000) {
        if (connected) {
            reset();
            _state = State::Online;
            return Action::None;
        }
        if (_state == State::Idle) {
            started(now);
            return Action::Connect;
        }
        if (_state == State::Online) {
            schedule(now);
        } else if (_state == State::Connecting && uint32_t(now - _since) >= timeout) {
            schedule(now);
            return Action::Disconnect;
        } else if (_state == State::Waiting && uint32_t(now - _since) >= _wait) {
            started(now);
            return Action::Connect;
        }
        return Action::None;
    }

private:
    enum class State { Idle, Connecting, Waiting, Online };
    State _state = State::Idle;
    uint32_t _since = 0, _wait = 0, _nextDelay = 5000, _random = 1;
    void schedule(uint32_t now) {
        _random ^= _random << 13;
        _random ^= _random >> 17;
        _random ^= _random << 5;
        _wait = _nextDelay + _random % 1001; // 0..1s fleet restart jitter
        _nextDelay = _nextDelay < 15000 ? _nextDelay * 2 : 30000;
        _since = now;
        _state = State::Waiting;
    }
};
