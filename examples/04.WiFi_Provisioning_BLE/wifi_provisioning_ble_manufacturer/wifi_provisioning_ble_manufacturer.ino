/**
 * 功能介绍：通过蓝牙厂商模式配网，由 SDK 自动接入 ThingsCloud，定时上报模拟传感器数据。
 * 适用芯片：ESP32（经典系列）、ESP32-S3、ESP32-C3、ESP32-C5、ESP32-C6，需同时具备WiFi和BLE。
 * 不适用：ESP8266、ESP32-S2（无BLE）；ESP32-H2、ESP32-P4（无片上WiFi）。
 * 补充说明：ESP32-C2、ESP32-C61需额外的Arduino-ESP32构建支持，本示例暂未验证。
 * 编译要求：Arduino-ESP32需包含本SDK所需的蓝牙配网组件；本示例不包含外接无线芯片方案。
 * Flash 与分区：本示例在经典 ESP32、Arduino-ESP32 3.3.8 下的程序占用约为 1.40MB，超过默认应用分区。
 * 使用 4MB Flash 时，请选择 Partition Scheme > Minimal SPIFFS（1.9MB APP with OTA/128KB SPIFFS），保留 OTA。
 * 使用 8MB Flash 可为业务扩展、OTA 固件增长和文件存储提供更大余量；请按实际容量设置 Flash Size，
 * 并选择匹配的分区方案。Flash 总容量不等于单个应用分区容量，仅增加 Flash 容量不会自动扩大应用分区。
 * 分区选项随开发板而异，其他芯片需以实际编译结果为准；详细配置与四种配网方案体积对比见 README。
 */

#include <ThingsCloudMQTT.h>
#include "ProvisioningButton.h"
#include <ThingsCloudBLEProvisioning.h>

// 按键配置：内部上拉输入（INPUT_PULLUP），低电平有效。
// 接线方式：常开瞬时按键一端接此 GPIO，另一端接 GND。
// 电平状态：松开时为高电平（HIGH），按下接地时为低电平（LOW）。
// 配网清除按键 GPIO，可根据实际硬件修改，避免与串口或 LED 引脚冲突。
const int BUTTON_PIN = 3;
ProvisioningButton resetButton;

static_assert(TC_HAS_BLE_PROVISIONING,
              "Select a WiFi+BLE chip and supported Arduino-ESP32 build");

//======================================================
// 填写 ThingsCloud 控制台中设备类型的 ProductKey 和蓝牙 Service UUID
#define THINGSCLOUD_PRODUCT_KEY ""
#define BLE_SERVICE_UUID "" // 完整的 128 位 UUID，与平台配置一致
//======================================================

TCManufacturerConfig manufacturerConfig = {
    "", // DeviceKey 留空，SDK 自动为模组生成唯一标识作为 DeviceKey
    THINGSCLOUD_PRODUCT_KEY};
ThingsCloudMQTT client(manufacturerConfig);
ThingsCloudBLEProvisioning provisioning;
bool provisioningReady = false;

// 上报数据的间隔时间计时器
unsigned long timer1 = 0;
// 上报间隔为 5 分钟，免费版项目请务必大于 30 秒。
const unsigned long report_interval = 1000UL * 60 * 5;

void pubSensors();
void resetProvisioning();

void setup()
{
    Serial.begin(115200);

    if (!resetButton.begin(BUTTON_PIN))
    {
        Serial.println("Provisioning button initialization failed");
        delay(1000);
        ESP.restart();
    }


    // 允许 SDK 的日志输出
    client.enableDebuggingMessages();

    ThingsCloudBLEProvisioningOptions options;
    options.provisioning_mode = TCProvisioningMode::Manufacturer;
    options.product_key = THINGSCLOUD_PRODUCT_KEY;
    options.service_uuid = BLE_SERVICE_UUID;
    // 0 表示无限等待用户配网；单次握手、联网和取证仍保留超时保护。
    options.provisioning_window_ms = 0;

    // 无配置时启动蓝牙配网；已有 WiFi 时后台连接，离线不会重新配网。
    // begin() 返回初始化结果，不表示 WiFi 已连接。
    // SDK 自动获取并保存接入凭据，再连接 MQTT。
    auto result = provisioning.begin(client, options);
    if (!result.ok())
    {
        Serial.printf("Provisioning initialization failed: %d\n", (int)result.code);
        return;
    }
    provisioningReady = true;
    Serial.println("BLE provisioning initialized");
}

// 必须实现这个回调函数，当 MQTT 连接成功后执行该函数。
void onMQTTConnect()
{
    Serial.println("MQTT connected");
    timer1 = millis();

    // 延迟 5 秒上报首次传感器数据
    client.executeDelayed(1000 * 5, []()
                          { pubSensors(); });
}

void pubSensors()
{
    // 这个示例模拟传感器数值，实际项目中可读取传感器真实数据。
    DynamicJsonDocument obj(512);
    obj["temperature"] = 31.2;
    obj["humidity"] = 62.5;
    obj["co2"] = 2321;
    obj["light"] = 653;
    char attributes[512];
    serializeJson(obj, attributes);
    client.reportAttributes(attributes);
    timer1 = millis();
}

void loop()
{
    // 按住3秒后松开，在主循环中清除配置并重启。
    if (resetButton.takeRequest())
    {
        resetProvisioning();
        return;
    }

    if (!provisioningReady)
    {
        delay(10);
        return;
    }

    // 在线、离线、配网期间都必须调用，SDK 维护超时和退避重连。
    // 连接失败后等待 5/10/20/30 秒并随机错峰，持续重试直至恢复。
    provisioning.loop();
    client.loop();

    // MQTT 在线时，按间隔时间上报传感器数据
    if (client.isConnected() && millis() - timer1 >= report_interval)
    {
        pubSensors();
    }

    delay(2);
}

// 项目模式仅清除WiFi；厂商模式还会清除已保存的平台接入凭据。
void resetProvisioning()
{
    provisioning.resetWifi();
    auto result = client.clearPlatformCredentials();
    if (!result.ok())
    {
        Serial.println("Failed to clear platform credentials; long press to retry");
        return;
    }
    Serial.println("Provisioning settings cleared, restarting");
    delay(100);
    ESP.restart();
}
// 固件中的DeviceKey、ProductKey等不变；平台解绑需另行操作。
