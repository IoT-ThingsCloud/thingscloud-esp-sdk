#include <ThingsCloudWiFiManager.h>
#include <ThingsCloudMQTT.h>
// 安装第三方依赖
// DHT_sensor_library
// Adafruit_Unified_Sensor
#include "DHT.h"

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

// 上报数据的间隔时间计时器
unsigned long timer1 = millis();
// 设置定时上报数据的时间间隔，单位是 ms。免费版项目请务必大于30秒，否则设备可能会被限连。
const int report_interval = 1000 * 60 * 5;

// 设置DHT11的数据引脚
#define DHTPIN 4
// 设置使用的DHT类型
#define DHTTYPE DHT11   // DHT 11
//#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

DHT dht(DHTPIN, DHTTYPE);

void setup()
{
  Serial.begin(115200);

  // 允许 SDK 的日志输出
  client.enableDebuggingMessages();

  // 连接 WiFi AP
  client.setWifiCredentials(ssid, password);

  dht.begin();
}

// 读取并发布传感器数据到 ThingsCloud
void pubSensors()
{
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  // 检查是否读取到传感器数据
  if (isnan(h) || isnan(t)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  // 串口打印数据日志
  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("%  Temperature: "));
  Serial.print(t);
  Serial.println();

  // 生成属性 JSON
  DynamicJsonDocument obj(512);
  obj["temperature"] = t;
  obj["humidity"] = h;
  char attributes[512];
  serializeJson(obj, attributes);
  // 调用属性上报方法
  client.reportAttributes(attributes);
}

// 必须实现这个回调函数，当 MQTT 连接成功后执行该函数。
void onMQTTConnect()
{
  // 延迟 5 秒上报首次传感器数据
  client.executeDelayed(1000 * 5, []()
  {
    pubSensors();
  });
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