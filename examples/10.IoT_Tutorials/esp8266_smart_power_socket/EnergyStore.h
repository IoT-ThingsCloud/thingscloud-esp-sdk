#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <flash_hal.h>
#include <math.h>
#include <stddef.h>

class EnergyStore
{
  public:
    bool ready = false;
    bool begin(double &total)
    {
        LittleFSConfig config(false); // Never silently format an existing damaged filesystem.
        LittleFS.setConfig(config);
        if (!LittleFS.begin())
        {
            // A genuinely erased new filesystem can be initialized once.
            if (!blankPartition() || !LittleFS.format() || !LittleFS.begin())
                return false;
        }
        Record a{}, b{};
        bool haveA = read("/energy-a.bin", a), haveB = read("/energy-b.bin", b);
        if (!haveA && !haveB)
        {
            if (LittleFS.exists("/energy-a.bin") || LittleFS.exists("/energy-b.bin"))
                return false;
            ready = true;
            return save(total);
        }
        bool useA = haveA && (!haveB || int32_t(a.sequence - b.sequence) > 0);
        const Record &record = useA ? a : b;
        sequence = record.sequence;
        slotA = useA;
        total = record.total;
        savedTotal = total;
        ready = true;
        return true;
    }
    bool save(double total)
    {
        if (!ready || !isfinite(total) || total < 0 || total < savedTotal)
            return false;
        if (sequence && total == savedTotal)
            return true;
        Record record{};
        record.magic = 0x454E4731;
        record.sequence = sequence + 1;
        record.total = total;
        record.crc = checksum(record);
        const char *path = slotA ? "/energy-b.bin" : "/energy-a.bin";
        File file = LittleFS.open(path, "w");
        if (!file)
            return false;
        size_t written = file.write(reinterpret_cast<const uint8_t *>(&record), sizeof(record));
        file.flush();
        file.close();
        Record verified{};
        if (written != sizeof(record) || !read(path, verified) || verified.sequence != record.sequence ||
            verified.total != total)
            return false;
        sequence = record.sequence;
        slotA = !slotA;
        savedTotal = total;
        return true;
    }

  private:
    struct Record
    {
        uint32_t magic, sequence;
        double total;
        uint32_t crc, reserved;
    };
    uint32_t sequence = 0;
    bool slotA = false;
    double savedTotal = 0;
    static uint32_t checksum(const Record &record)
    {
        uint32_t crc = 0xFFFFFFFF;
        const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&record);
        for (size_t i = 0; i < offsetof(Record, crc); ++i)
        {
            crc ^= bytes[i];
            for (uint8_t bit = 0; bit < 8; ++bit)
                crc = (crc >> 1) ^ (0xEDB88320 & (0u - (crc & 1)));
        }
        return ~crc;
    }
    bool read(const char *path, Record &record)
    {
        File file = LittleFS.open(path, "r");
        if (!file || file.size() != sizeof(record))
            return false;
        size_t n = file.read(reinterpret_cast<uint8_t *>(&record), sizeof(record));
        return n == sizeof(record) && record.magic == 0x454E4731 && record.crc == checksum(record) &&
               isfinite(record.total) && record.total >= 0;
    }
    bool blankPartition()
    {
        uint32_t buffer[64];
        for (uint32_t offset = 0; offset < FS_PHYS_SIZE; offset += sizeof(buffer))
        {
            if (!ESP.flashRead(FS_PHYS_ADDR + offset, buffer, sizeof(buffer)))
                return false;
            for (uint32_t word : buffer)
                if (word != 0xFFFFFFFF)
                    return false;
            yield();
        }
        return true;
    }
};
