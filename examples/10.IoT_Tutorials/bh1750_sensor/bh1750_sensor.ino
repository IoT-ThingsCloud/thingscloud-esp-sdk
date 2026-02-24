/**
 * BH1750 光照传感器示例
 *
 * 功能：采集环境光照亮度数据并上报到 ThingsCloud 物联网平台
 *
 * 所需库（通过 Arduino IDE 库管理器安装）：
 * - BH1750 by Christopher Laws
 *
 * ============================================================
 * BH1750 与 ESP32 接线方式
 * ============================================================
 *
 *   BH1750          ESP32
 *   ------          -----
 *   VCC     ->      3.3V
 *   GND     ->      GND
 *   SCL     ->      GPIO 22 (默认 I2C SCL)
 *   SDA     ->      GPIO 21 (默认 I2C SDA)
 *   ADD     ->      不连接（默认地址 0x23，接 VCC 则地址为 0x5C）
 *
 * ============================================================
 */

#include <ThingsCloudWiFiManager.h>
#include <ThingsCloudMQTT.h>
// 安装第三方依赖：BH1750 library by Christopher Laws
#include <Wire.h>
#include <BH1750.h>

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

// 创建 BH1750 对象
BH1750 lightMeter;

// 存储光照值（单位：lux）
float lux = 0.0;

void setup()
{
  Serial.begin(115200);

  // 初始化 I2C 通信（使用 ESP32 默认 I2C 引脚：SDA=GPIO21, SCL=GPIO22）
  Wire.begin();

  // 初始化 BH1750 传感器
  // 默认使用连续高分辨率模式（1 lux 精度，120ms 测量时间）
  if (lightMeter.begin()) {
    Serial.println(F("BH1750 sensor initialized successfully"));
  } else {
    Serial.println(F("BH1750 sensor initialization failed!"));
  }

  // 允许 SDK 的日志输出
  client.enableDebuggingMessages();

  // 连接 WiFi AP
  client.setWifiCredentials(ssid, password);
}

/**
 * @brief 读取并上报光照传感器数据到 ThingsCloud
 *
 * 该函数从 BH1750 传感器读取当前环境光照值，
 * 并通过 MQTT 协议上报到 ThingsCloud 平台。
 */
void pubSensors()
{
  // 读取光照值（单位：lux）
  lux = lightMeter.readLightLevel();

  // 检查是否读取到有效数据
  if (lux < 0) {
    Serial.println(F("Failed to read from BH1750 sensor!"));
    return;
  }

  // 串口打印光照数据
  Serial.print(F("Light: "));
  Serial.print(lux);
  Serial.println(F(" lux"));

  // 生成属性 JSON
  DynamicJsonDocument obj(256);
  obj["light"] = lux;
  char attributes[256];
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
