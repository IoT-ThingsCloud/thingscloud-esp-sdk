#pragma once
#include <stdint.h>

// 只在稳定松开后产生一次事件；忽略短按、抖动和上电时已按住的状态。
class ProvisioningButtonState
{
    bool initialized = false, raw = false, stable = false;
    bool ready = false, tracking = false;
    uint32_t changedAt = 0, pressedAt = 0;
public:
    bool update(bool pressed, uint32_t now)
    {
        if (!initialized)
        {
            initialized = true;
            raw = stable = pressed;
            ready = !pressed;
            changedAt = now;
            return false;
        }
        if (pressed != raw)
        {
            raw = pressed;
            changedAt = now;
        }
        if ((uint32_t)(now - changedAt) < 40 || raw == stable)
            return false;
        stable = raw;
        if (stable)
        {
            tracking = ready;
            pressedAt = changedAt;
            ready = false;
            return false;
        }
        const bool requested = tracking && (uint32_t)(changedAt - pressedAt) >= 3000;
        tracking = false;
        ready = true;
        return requested;
    }
};

#if defined(ESP32) || defined(ESP8266)
#include <Arduino.h>
#include <Ticker.h>
#if defined(ESP32)
#include <atomic>
#else
#include <interrupts.h>
#endif

// 定时器只采样和记录事件；清除配置在示例的loop()中执行。
class ProvisioningButton
{
    int pin = 0;
    Ticker timer;
    ProvisioningButtonState state;
#if defined(ESP32)
    std::atomic<bool> pending{false};
#else
    volatile bool pending = false;
#endif
    static void sample(ProvisioningButton* self)
    {
        if (self->state.update(digitalRead(self->pin) == LOW, millis()))
        {
#if defined(ESP32)
            self->pending.store(true, std::memory_order_release);
#else
            self->pending = true;
#endif
        }
    }
public:
    bool begin(int buttonPin)
    {
        pin = buttonPin;
        pinMode(pin, INPUT_PULLUP);
        timer.attach_ms(10, sample, this);
        return timer.active();
    }
    bool takeRequest()
    {
#if defined(ESP32)
        return pending.exchange(false, std::memory_order_acq_rel);
#else
        esp8266::InterruptLock lock;
        bool requested = pending;
        pending = false;
        return requested;
#endif
    }
};
#endif
