/**
 * 功能介绍：通过 AP 配网模式连接 WiFi，支持 ThingsX App 配网并自动创建设备，定时上报模拟传感器数据。
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
// 在 ThingsCloud 控制台的设备详情页中，复制以下设备连接信息
// https://console.thingscloud.xyz
#define THINGSCLOUD_MQTT_HOST ""        // MQTT 服务器域名
#define THINGSCLOUD_PROJECT_KEY ""      // 同一个项目所有设备的 ProjectKey 相同
#define THINGSCLOUD_TYPE_KEY ""         // 设备类型的 TypeKey，用于自动创建设备
#define THINGSCLOUD_API_ENDPOINT ""     // 用于获取设备证书以及自动创建设备
//======================================================

ThingsCloudMQTT client(
    THINGSCLOUD_MQTT_HOST,
    "", // DeviceKey 留空，SDK 自动为模组生成唯一标识作为 DeviceKey
    THINGSCLOUD_PROJECT_KEY,
    THINGSCLOUD_TYPE_KEY,
    THINGSCLOUD_API_ENDPOINT);

// ESP模组生成 WiFi AP，用于配网
#define WiFi_AP_SSID "ESP32_DEVICE" // AP 的 SSID 前缀，出现在用户 App 的 WiFi 列表中，可修改为你喜欢的名称
#define WiFi_AP_PASSWORD ""         // AP 的连接密码，可以不设置
ThingsCloudWiFiManager wm(WiFi_AP_SSID, WiFi_AP_PASSWORD);
bool provisioningReady = false;

// 设置 LED GPIO 引脚
const int LED_PIN = 2;
// 上报数据的间隔时间计时器
unsigned long timer1 = millis();
// 设置定时上报数据的时间间隔，单位是 ms。免费版项目请务必大于30秒，否则设备可能会被限连。
const int report_interval = 1000 * 60 * 5;

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


    // 拉低 LED
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // 允许 SDK 的日志输出
    client.enableDebuggingMessages();

    // 关联 MQTT 客户端和配网管理器
    wm.linkMQTTClient(&client);

    // 使用非阻塞门户，等待配网时也能响应长按按键。
    wm.setConfigPortalBlocking(false);
    wm.setConfigPortalTimeout(0); // 未配网时无限等待用户配网。

    // 调试配网可以取消以下注释，用于清空之前保存的 WiFi 配网信息，每次设备开机都需要重新配网。
    // 实际产品中，将清空配网信息放在例如设备的按键事件中，由用户操作触发设备重新进入配网状态。
    // wm.resetSettings();

    // 无配置时启动 AP；已有 WiFi 时后台连接，离线不会重新配网。
    // begin() 返回初始化结果，不表示 WiFi 已连接。
    if (!wm.begin())
    {
        Serial.println("WiFi manager initialization failed");
        return;
    }
    provisioningReady = true;
}

// 必须实现这个回调函数，当 MQTT 连接成功后执行该函数。
void onMQTTConnect()
{
    // 这里点亮 LED
    digitalWrite(LED_PIN, HIGH);

    // 延迟 5 秒上报首次传感器数据
    client.executeDelayed(1000 * 5, []()
                          { pubSensors(); });
}

void pubSensors()
{
    // 这个示例模拟传感器数值，仅用于演示如何生成 JSON。实际项目中可读取传感器真实数据。
    DynamicJsonDocument obj(512);
    obj["temperature"] = 31.2;
    obj["humidity"] = 62.5;
    obj["co2"] = 2321;
    obj["light"] = 653;
    char attributes[512];
    serializeJson(obj, attributes);
    // 调用属性上报方法
    client.reportAttributes(attributes);
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
    wm.process();
    if (wm.getConfigPortalActive())
    {
        delay(2);
        return;
    }

    client.loop();

    // 按间隔时间上报传感器数据
    if (client.isConnected() && millis() - timer1 >= report_interval)
    {
        timer1 = millis();
        pubSensors();
    }
}
// 项目模式仅清除WiFi；厂商模式还会清除已保存的平台接入凭据。
void resetProvisioning()
{
    if (wm.getConfigPortalActive()) wm.stopConfigPortal();
    wm.resetSettings();
    Serial.println("Provisioning settings cleared, restarting");
    delay(100);
    ESP.restart();
}
