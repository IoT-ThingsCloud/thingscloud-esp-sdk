#pragma once

#include <LittleFS.h>
#include <flash_hal.h>

// 完整检查分区；读取失败或发现已有数据时，均不允许格式化。
inline bool provisioningStorageIsBlank()
{
    const uint32_t start = FS_PHYS_ADDR, size = FS_PHYS_SIZE;
    const uint32_t flashSize = ESP.getFlashChipRealSize();
    uint32_t buffer[64];
    if (!size || size % sizeof(buffer) || start >= flashSize || size > flashSize - start)
        return false;
    for (uint32_t offset = 0; offset < size; offset += sizeof(buffer))
    {
        if (!ESP.flashRead(start + offset, buffer, sizeof(buffer)))
            return false;
        for (uint32_t word : buffer)
            if (word != 0xFFFFFFFF)
                return false;
        yield();
    }
    return true;
}

// 应用层负责首次格式化；已有文件系统直接复用，仅空白分区允许初始化。
inline bool prepareProvisioningStorage()
{
    LittleFS.setConfig(LittleFSConfig(false));
    if (LittleFS.begin())
        return true;
    return provisioningStorageIsBlank() && LittleFS.format() && LittleFS.begin();
}
