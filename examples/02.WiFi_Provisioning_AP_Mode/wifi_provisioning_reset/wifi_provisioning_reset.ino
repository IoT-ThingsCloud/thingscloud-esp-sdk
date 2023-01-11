#include <ThingsCloudWiFiManager.h>
#include <ThingsCloudMQTT.h>

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
#define WiFi_AP_SSID "ESP32_DEVICE" // AP 的 SSID
#define WiFi_AP_PASSWORD ""         // AP 的连接密码，可以不设置
ThingsCloudWiFiManager wm(WiFi_AP_SSID, WiFi_AP_PASSWORD);

// 设置 LED GPIO 引脚
const int LED_PIN = 3;

// 用于检测重置配网的按键，长按 2 秒
const int RESET_BUTTON_PIN = 4;
int lastState = LOW;
int currentState;
unsigned long pressedTime = 0;
unsigned long releasedTime = 0;

// 上报数据的间隔时间计时器
unsigned long timer1 = millis();
// 设置定时上报数据的时间间隔，单位是 ms。免费版项目请务必大于30秒，否则设备可能会被限连。
const int report_interval = 1000 * 60 * 5;

void pubSensors();

void setup()
{
    Serial.begin(115200);

    // 配网重置按键，默认下拉
    pinMode(RESET_BUTTON_PIN, INPUT_PULLDOWN);

    // 开机闪烁 LED 几次后常亮，表示未接入 WiFi
    pinMode(LED_PIN, OUTPUT);
    for (int i = 0; i < 5; ++i)
    {
        digitalWrite(LED_PIN, LOW);
        delay(200);
        digitalWrite(LED_PIN, HIGH);
        delay(200);
    }

    // 允许 SDK 的日志输出
    client.enableDebuggingMessages();

    // 关联 MQTT 客户端和配网管理器
    wm.linkMQTTClient(&client);

    // 如果设备未配网，则启动 AP 配网模式，等待 ThingsX App 为设备配网
    // 如果已配网，则直接连接 WiFi
    if (!wm.autoConnect())
    {
        Serial.println("\nWiFi provisioning failed, will restart to retry");
        delay(1000);
        ESP.restart();
    }
}

// 必须实现这个回调函数，当 MQTT 连接成功后执行该函数。
void onMQTTConnect()
{
    // MQTT 连接成功后，熄灭 LED
    digitalWrite(LED_PIN, LOW);

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

void checkResetButton()
{
    currentState = digitalRead(RESET_BUTTON_PIN);
    if (currentState == HIGH && lastState == LOW)
    {
        pressedTime = millis();
        lastState = HIGH;
    }
    else if (currentState == HIGH && lastState == HIGH)
    {
        if (pressedTime > 0 && millis() - pressedTime > 2000)
        {
            // 检测到按键长按 2s，点亮 LED
            digitalWrite(LED_PIN, HIGH);
            Serial.println("\nWiFi reseting...");
            // 清空配网信息
            wm.resetSettings();
            delay(1000);
            // 重启设备，将重新进入等待配网状态
            Serial.println("\nESP restart...");
            ESP.restart();
        }
    }
    else
    {
        pressedTime = 0;
        lastState = LOW;
    }
}

void loop()
{
    // 检查配网重置按键
    checkResetButton();

    client.loop();

    // 按间隔时间上报传感器数据
    if (millis() - timer1 > report_interval)
    {
        timer1 = millis();
        pubSensors();
    }
}
