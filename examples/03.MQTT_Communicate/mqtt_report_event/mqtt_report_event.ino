#include <ThingsCloudWiFiManager.h>
#include <ThingsCloudMQTT.h>

//======================================================
// 设置 ssid / password，连接到你的 WiFi AP
const char *ssid = "";
const char *password = "";
// 在 ThingsCloud 控制台的设备详情页中，复制以下设备连接信息
// https://console.thingscloud.xyz
#define THINGSCLOUD_MQTT_HOST ""
#define THINGSCLOUD_DEVICE_ACCESS_TOKEN ""
#define THINGSCLOUD_PROJECT_KEY ""
//======================================================

ThingsCloudMQTT client(
    THINGSCLOUD_MQTT_HOST,
    THINGSCLOUD_DEVICE_ACCESS_TOKEN,
    THINGSCLOUD_PROJECT_KEY);

// 设置 LED GPIO 引脚
const int LED_PIN = 3;
// 上报事件的间隔时间计时器
unsigned long timer1 = millis();
// 设置定时上报数据的时间间隔，单位是 ms。免费版项目请务必大于30秒，否则设备可能会被限连。
const int report_interval = 1000 * 60 * 5;

void pubEvent();

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

    // 延迟 5 秒上报首次事件
    client.executeDelayed(1000 * 5, []()
                          { pubEvent(); });
}

void pubEvent()
{
    DynamicJsonDocument obj(512);
    DynamicJsonDocument params(512);
    params["type"] = "start";
    obj["method"] = "eventTest";
    obj["params"] = params;
    char event[512];
    serializeJson(obj, event);
    client.reportEvent(1000, event);
}

void loop()
{
    client.loop();

    // 按间隔时间上报事件
    if (millis() - timer1 > report_interval)
    {
        timer1 = millis();
        pubEvent();
    }
}
