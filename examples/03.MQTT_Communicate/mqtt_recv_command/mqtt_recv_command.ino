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

    // 订阅云平台下发的命令消息
    client.onCommandSend([](const String &topic, const String &payload)
                         { 
                            Serial.println("command send topic: " + topic + ", payload: " + payload);
                                DynamicJsonDocument doc(512);
                                DeserializationError error = deserializeJson(doc, payload);
                                if (error)
                                {
                                    Serial.printf("deserialize error: %s\n", error.f_str());
                                    return;
                                }
                                JsonObject obj = doc.as<JsonObject>();
                                // 判断命令的标识符
                                if (obj.containsKey("method") && obj["method"] == "restart")
                                {
                                    // 重启设备，将重新进入等待配网状态
                                    Serial.println("\nESP restart...");
                                    delay(1000);
                                    ESP.restart();
                                } });
}

void loop()
{
    client.loop();
}
