/**
 * 功能介绍：通过 SoftAP 厂商模式配网，由 SDK 自动接入 ThingsCloud，定时上报模拟传感器数据。
 * 适用芯片：ESP8266、ESP32（经典系列）、ESP32-S2、ESP32-S3、ESP32-C3、ESP32-C5、ESP32-C6。
 * 补充说明：ESP32-C2、ESP32-C61需额外的Arduino-ESP32构建支持，本示例暂未验证。
 * 不适用：ESP32-H2、ESP32-P4（无片上WiFi，本示例不包含外接无线芯片方案）。
 */

#include <ThingsCloudWiFiManager.h>
#include <ThingsCloudMQTT.h>
#include "ProvisioningButton.h"

// 按键配置：内部上拉输入（INPUT_PULLUP），低电平有效。
// 接线方式：常开瞬时按键一端接此 GPIO，另一端接 GND。
// 电平状态：松开时为高电平（HIGH），按下接地时为低电平（LOW）。
// 配网清除按键 GPIO，可根据实际硬件修改，避免与串口或 LED 引脚冲突。
const int BUTTON_PIN = 3;
ProvisioningButton resetButton;

//======================================================
// 填写 ThingsCloud 控制台中设备类型的 ProductKey
#define THINGSCLOUD_PRODUCT_KEY ""
//======================================================

TCManufacturerConfig manufacturerConfig = {
    "", // DeviceKey 留空，SDK 自动为模组生成唯一标识作为 DeviceKey
    THINGSCLOUD_PRODUCT_KEY};
ThingsCloudMQTT client(manufacturerConfig);

// ESP 模组生成 WiFi AP，用于配网
#define WiFi_AP_SSID "ESP32_DEVICE" // AP 的 SSID 前缀，可修改
#define WiFi_AP_PASSWORD ""         // AP 的连接密码，可以不设置
ThingsCloudWiFiManager wm(WiFi_AP_SSID, WiFi_AP_PASSWORD);

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

    // 关联 MQTT 客户端和配网管理器
    wm.linkMQTTClient(&client);

    // 使用非阻塞门户，等待配网时也能响应长按按键。
    wm.setConfigPortalBlocking(false);

    // 无限等待用户配网；配网成功或用户主动取消时退出。
    wm.setConfigPortalTimeout(0);

    // 调试配网时可取消以下注释，每次开机都会清除已保存的 WiFi 信息。
    // 实际产品应在用户操作触发的按键等事件中清除配网。
    // wm.resetSettings();

    // 未配网时启动 AP 等待 ThingsX App 配网，已配网时直接连接 WiFi。
    // SDK 自动获取并保存接入凭据，再连接 MQTT。
    if (!wm.autoConnect() && !wm.getConfigPortalActive())
    {
        Serial.println("\nWiFi provisioning failed, will restart to retry");
        delay(1000);
        ESP.restart();
    }
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
    if (wm.getConfigPortalActive())
    {
        const bool connected = wm.process();
        if (!wm.getConfigPortalActive() && !connected)
        {
            Serial.println("WiFi provisioning failed, will restart to retry");
            delay(1000);
            ESP.restart();
        }
        delay(2);
        return;
    }

    client.loop();

    // MQTT 在线时，按间隔时间上报传感器数据
    if (client.isConnected() && millis() - timer1 >= report_interval)
    {
        pubSensors();
    }
}

// 项目模式仅清除WiFi；厂商模式还会清除已保存的平台接入凭据。
void resetProvisioning()
{
    if (wm.getConfigPortalActive()) wm.stopConfigPortal();
    wm.resetSettings();
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
