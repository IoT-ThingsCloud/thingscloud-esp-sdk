// HLW8032 UART frame decoder, based on manufacturer user manual rev 1.6.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

class Hlw8032Decoder
{
  public:
    uint32_t frames = 0, checksumErrors = 0, parameterErrors = 0;
    uint8_t state = 0;
    double voltageRatio = 0, currentRatio = 0, powerRatio = 0;
    uint32_t voltageAt = 0, currentAt = 0, powerAt = 0;
    bool haveVoltage = false, haveCurrent = false, havePower = false;
    bool energyValid = false;
    uint32_t pulseCount17 = 0, powerParameter = 0;

    bool feed(uint8_t byte, uint32_t now)
    {
        if (used == sizeof(buffer))
        {
            memmove(buffer, buffer + 1, sizeof(buffer) - 1);
            --used;
        }
        buffer[used++] = byte;
        if (used != sizeof(buffer))
            return false;
        if (buffer[1] != 0x5A || !(buffer[0] == 0x55 || buffer[0] == 0xAA || (buffer[0] & 0xF0) == 0xF0))
            return false;
        uint8_t sum = 0;
        for (size_t i = 2; i < 23; ++i)
            sum += buffer[i];
        if (sum != buffer[23])
        {
            ++checksumErrors;
            return false;
        }
        used = 0;
        ++frames;
        energyValid = false;
        state = buffer[0];
        if (state == 0xAA || ((state & 0xF0) == 0xF0 && (state & 1)))
        {
            ++parameterErrors;
            haveVoltage = haveCurrent = havePower = false;
            return true;
        }
        bool statusNormal = state == 0x55;
        pulseCount17 = ((buffer[20] & 0x80) ? 65536u : 0u) | (uint32_t(buffer[21]) << 8) | buffer[22];
        powerParameter = u24(14);
        energyValid = powerParameter != 0;
        update(2, 5, 0x40, !statusNormal && (state & 0x08), now, voltageRatio, voltageAt, haveVoltage);
        update(8, 11, 0x20, !statusNormal && (state & 0x04), now, currentRatio, currentAt, haveCurrent);
        update(14, 17, 0x10, !statusNormal && (state & 0x02), now, powerRatio, powerAt, havePower);
        return true;
    }
    bool fresh(uint32_t now) const
    {
        return haveVoltage && haveCurrent && havePower && uint32_t(now - voltageAt) <= 3000 &&
               uint32_t(now - currentAt) <= 3000 && uint32_t(now - powerAt) <= 3000;
    }

  private:
    uint8_t buffer[24] = {};
    size_t used = 0;
    uint32_t u24(size_t offset) const
    {
        return (uint32_t(buffer[offset]) << 16) | (uint32_t(buffer[offset + 1]) << 8) | buffer[offset + 2];
    }
    void update(size_t parameterOffset, size_t valueOffset, uint8_t mask, bool overflow, uint32_t now, double &ratio,
                uint32_t &timestamp, bool &valid)
    {
        // Register overflow denotes a value near zero, per the manufacturer.
        if (overflow)
        {
            ratio = 0;
            timestamp = now;
            valid = true;
            return;
        }
        if (!(buffer[20] & mask))
            return;
        uint32_t parameter = u24(parameterOffset), value = u24(valueOffset);
        if (!parameter || !value)
        {
            valid = false;
            return;
        }
        ratio = double(parameter) / value;
        timestamp = now;
        valid = true;
    }
};
