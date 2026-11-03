#pragma once
#include <stdint.h>
enum class ReportKind
{
    NONE,
    SWITCH_EVENT,
    PERIODIC_FULL,
    ONLINE_FULL
};

class OnlineReportGate
{
  public:
    bool pending = false;
    uint32_t startedAt = 0;
    void begin(uint32_t now)
    {
        startedAt = now;
        pending = true;
    }
    bool ready(uint32_t now, bool freshMeasurements) const
    {
        uint32_t elapsed = now - startedAt;
        // Leave time for cloud switch lookup and new UART frames; don't wait for a minute.
        return pending && elapsed >= 500 && (freshMeasurements || elapsed >= 3000);
    }
};

class PeriodicReportClock
{
  public:
    bool due = false;
    uint32_t generation = 0;
    explicit PeriodicReportClock(uint32_t interval, uint32_t start = 0) : intervalMs(interval), tickAt(start) {}
    void update(uint32_t now)
    {
        uint32_t elapsed = now - tickAt;
        if (elapsed < intervalMs)
            return;
        uint32_t intervals = elapsed / intervalMs;
        tickAt += intervals * intervalMs;
        generation += intervals;
        due = true; // Coalesce offline intervals; never replay old measurements.
    }
    void acknowledge(uint32_t sentGeneration)
    {
        if (sentGeneration == generation)
            due = false;
    }

  private:
    uint32_t intervalMs, tickAt;
};
