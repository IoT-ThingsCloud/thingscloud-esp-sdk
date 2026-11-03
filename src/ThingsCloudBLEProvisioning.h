#pragma once
#include "ThingsCloudMQTT.h"
#if defined(ARDUINO_ARCH_ESP32)
#include <soc/soc_caps.h>
#include <sdkconfig.h>
#if SOC_WIFI_SUPPORTED && SOC_BLE_SUPPORTED && CONFIG_NETWORK_PROV_NETWORK_TYPE_WIFI && \
    __has_include(<protocomm_security1.h>) && __has_include(<network_provisioning/network_config.h>)
#define TC_HAS_BLE_PROVISIONING 1
#endif
#endif
#ifndef TC_HAS_BLE_PROVISIONING
#define TC_HAS_BLE_PROVISIONING 0
#endif

// 由应用声明使用 BLE，避免 Arduino 启动时释放控制器内存。
// SDK 自身编译此头文件时不声明，保持 SoftAP 程序的原有内存行为。
#if TC_HAS_BLE_PROVISIONING && !defined(THINGSCLOUD_BLE_IMPLEMENTATION) && __has_include(<esp32-hal-alloc-ble-mem.h>)
#include <esp32-hal-alloc-ble-mem.h>
#endif

class ThingsCloudBLEProvisioning {
public:
    ThingsCloudBLEProvisioning();
    ~ThingsCloudBLEProvisioning();
    ThingsCloudBLEProvisioning(const ThingsCloudBLEProvisioning&) = delete;
    ThingsCloudBLEProvisioning& operator=(const ThingsCloudBLEProvisioning&) = delete;
    static bool supported() { return TC_HAS_BLE_PROVISIONING; }
    TCProvResult begin(ThingsCloudMQTT& client, const ThingsCloudBLEProvisioningOptions& options);
    TCProvResult start();
    void loop();
    void stop();
    TCProvState state() const;
    TCProvisioningMode provisioningMode() const;
    void onEvent(TCProvEventCallback callback);
    // Stops provisioning and clears only saved WiFi. The caller restarts.
    void resetWifi();
private:
    struct Impl;
    Impl* _impl;
};
