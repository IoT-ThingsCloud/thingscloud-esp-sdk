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

    // 订阅属性上报的回复消息
    client.onAttributesResponse([](const String &payload)
                                { Serial.println("attributes response: " + payload); });

    // 订阅属性获取的回复消息
    client.onAttributesGetResponse([](const String &topic, const String &payload)
                                   { Serial.println("attributes get response: " + topic + ", payload: " + payload); });

    // 订阅云平台下发属性的消息
    client.onAttributesPush([](const String &payload)
                            {
                                Serial.println("attributes push: " + payload);
                                DynamicJsonDocument doc(512);
                                DeserializationError error = deserializeJson(doc, payload);
                                if (error)
                                {
                                    Serial.printf("deserialize error: %s\n", error.f_str());
                                    return;
                                }
                                JsonObject obj = doc.as<JsonObject>();
                                if (obj.containsKey("relay1"))
                                {
                                    if (obj["relay1"] == true)
                                    {
                                        Serial.println("relay1 ON");
                                        // todo 输出 GPIO 控制继电器
                                    }
                                    else
                                    {
                                        Serial.println("relay1 OFF");
                                        // todo 输出 GPIO 控制继电器
                                    }
                                } });

    // 读取设备在云平台上的属性
    client.getAttributes();

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
