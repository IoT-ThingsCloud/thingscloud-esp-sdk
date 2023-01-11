#include <ThingsCloudWiFiManager.h>
#include <ThingsCloudMQTT.h>

//======================================================
// 设置 ssid / password，连接到你的 WiFi AP
const char *ssid = "";
const char *password = "";
// 在 ThingsCloud 控制台的设备详情页中，复制以下设备连接信息
// https://console.thingscloud.xyz
#define THINGSCLOUD_MQTT_HOST ""        // MQTT 服务器域名
#define THINGSCLOUD_DEVICE_KEY ""       // 设备的唯一标识 DeviceKey，用于获取设备证书。可在设备详情页编辑
#define THINGSCLOUD_PROJECT_KEY ""      // 同一个项目所有设备的 ProjectKey 相同
#define THINGSCLOUD_TYPE_KEY ""         // 设备类型的 TypeKey，用于自动创建设备
#define THINGSCLOUD_API_ENDPOINT ""     // 用于获取设备证书以及自动创建设备
//======================================================

ThingsCloudMQTT client(
    THINGSCLOUD_MQTT_HOST,
    THINGSCLOUD_DEVICE_KEY,
    THINGSCLOUD_PROJECT_KEY,
    THINGSCLOUD_TYPE_KEY,
    THINGSCLOUD_API_ENDPOINT);

// 设置 LED GPIO 引脚
const int LED_PIN = 3;
// 上报数据的间隔时间计时器
unsigned long timer1 = millis();
// 设置定时上报数据的时间间隔，单位是 ms。免费版项目请务必大于30秒，否则设备可能会被限连。
const int report_interval = 1000 * 60 * 5;

void pubSensors();

void setup()
{
    Serial.begin(115200);

    // 拉低 LED
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // 允许 SDK 的日志输出
    client.enableDebuggingMessages();

    // 连接 WiFi AP
    client.setWifiCredentials(ssid, password);
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
    client.loop();

    // 按间隔时间上报传感器数据
    if (millis() - timer1 > report_interval)
    {
        timer1 = millis();
        pubSensors();
    }
}
