#pragma once
#include <stdint.h>
#include <math.h>

class EnergyCounter
{
  public:
    double totalKwh = 0;
    uint32_t rejected = 0;
    bool seen = false;
    void observe(uint32_t count17, uint32_t powerParameter, uint32_t now, double voltageCoefficient,
                 double currentCoefficient)
    {
        const double perPulse = double(powerParameter) * voltageCoefficient * currentCoefficient / 3600000000000.0;
        if (!powerParameter || !isfinite(perPulse) || perPulse <= 0)
        {
            seen = false;
            return;
        }
        if (!seen)
        {
            baseline(count17, powerParameter, now);
            return;
        }
        uint32_t elapsed = now - lastAt;
        uint32_t delta = (count17 - lastCount) & 0x1FFFF;
        // Never bridge a long/ambiguous sampling gap or a change of calibration registers.
        // 5kW is a generous sanity ceiling for this 10A socket, not a protection limit.
        double maximum = 5000.0 * elapsed / 3600000000.0 + 2 * perPulse;
        double increment = delta * perPulse;
        if (elapsed > 60000 || powerParameter != lastParameter || increment > maximum)
        {
            ++rejected;
        }
        else
        {
            totalKwh += increment;
        }
        baseline(count17, powerParameter, now);
    }

  private:
    uint32_t lastCount = 0, lastParameter = 0, lastAt = 0;
    void baseline(uint32_t count, uint32_t parameter, uint32_t now)
    {
        lastCount = count;
        lastParameter = parameter;
        lastAt = now;
        seen = true;
    }
};
