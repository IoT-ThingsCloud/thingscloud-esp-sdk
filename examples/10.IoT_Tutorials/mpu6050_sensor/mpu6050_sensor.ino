/**
 * MPU6050 六轴传感器示例
 *
 * 功能：采集三轴加速度、三轴陀螺仪和温度数据，并上报到 ThingsCloud 物联网平台
 *
 * 所需库（通过 Arduino IDE 库管理器安装）：
 * - Adafruit MPU6050
 * - Adafruit Unified Sensor
 * - Adafruit BusIO
 *
 * ============================================================
 * MPU6050 与 ESP32 接线方式
 * ============================================================
 *
 *   MPU6050        ESP32
 *   --------       -----
 *   VCC     ->     3.3V
 *   GND     ->     GND
 *   SCL     ->     GPIO 22 (默认 I2C SCL)
 *   SDA     ->     GPIO 21 (默认 I2C SDA)
 *   XDA     ->     不连接
 *   XCL     ->     不连接
 *   AD0     ->     不连接（默认地址 0x68，接 GND 也是 0x68，接 VCC 则为 0x69）
 *   INT     ->     不连接
 *
 * ============================================================
 */

#include <ThingsCloudWiFiManager.h>
#include <ThingsCloudMQTT.h>
// 安装第三方依赖：Adafruit MPU6050, Adafruit Unified Sensor, Adafruit BusIO
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

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

// 创建 MPU6050 对象
Adafruit_MPU6050 mpu;

// 传感器事件对象
sensors_event_t a, g, temp;

/**
 * @brief 初始化 MPU6050 传感器
 *
 * 尝试初始化 MPU6050 传感器，如果失败则在串口输出错误信息。
 */
void initMPU()
{
  if (!mpu.begin())
  {
    Serial.println("Failed to find MPU6050 chip");
    while (1)
    {
      delay(10);
    }
  }
  Serial.println("MPU6050 Found!");

  // 设置加速度计量程（可选：MPU6050_RANGE_2_G, MPU6050_RANGE_4_G, MPU6050_RANGE_8_G, MPU6050_RANGE_16_G）
  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);

  // 设置陀螺仪量程（可选：MPU6050_RANGE_250_DEG, MPU6050_RANGE_500_DEG, MPU6050_RANGE_1000_DEG, MPU6050_RANGE_2000_DEG）
  mpu.setGyroRange(MPU6050_RANGE_250_DEG);

  // 设置带宽（可选：MPU6050_BAND_260_HZ, MPU6050_BAND_184_HZ, MPU6050_BAND_94_HZ, MPU6050_BAND_44_HZ, MPU6050_BAND_21_HZ, MPU6050_BAND_10_HZ, MPU6050_BAND_5_HZ）
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
}

void setup()
{
  Serial.begin(115200);

  // 初始化 MPU6050 传感器
  initMPU();

  // 允许 SDK 的日志输出
  client.enableDebuggingMessages();

  // 连接 WiFi AP
  client.setWifiCredentials(ssid, password);
}

/**
 * @brief 读取并上报 MPU6050 传感器数据到 ThingsCloud
 *
 * 该函数从 MPU6050 传感器读取加速度、陀螺仪和温度数据，
 * 并通过 MQTT 协议上报到 ThingsCloud 平台。
 */
void pubSensors()
{
  // 获取传感器数据
  mpu.getEvent(&a, &g, &temp);

  // 串口打印加速度数据（单位：m/s²）
  Serial.print(F("Acceleration X: "));
  Serial.print(a.acceleration.x);
  Serial.print(F(", Y: "));
  Serial.print(a.acceleration.y);
  Serial.print(F(", Z: "));
  Serial.print(a.acceleration.z);
  Serial.println(F(" m/s^2"));

  // 串口打印陀螺仪数据（单位：rad/s）
  Serial.print(F("Rotation X: "));
  Serial.print(g.gyro.x);
  Serial.print(F(", Y: "));
  Serial.print(g.gyro.y);
  Serial.print(F(", Z: "));
  Serial.print(g.gyro.z);
  Serial.println(F(" rad/s"));

  // 串口打印温度数据（单位：°C）
  Serial.print(F("Temperature: "));
  Serial.print(temp.temperature);
  Serial.println(F(" degC"));

  Serial.println();

  // 生成属性 JSON
  DynamicJsonDocument obj(512);
  // 加速度数据（m/s²）
  obj["acc_x"] = a.acceleration.x;
  obj["acc_y"] = a.acceleration.y;
  obj["acc_z"] = a.acceleration.z;
  // 陀螺仪数据（rad/s）
  obj["gyro_x"] = g.gyro.x;
  obj["gyro_y"] = g.gyro.y;
  obj["gyro_z"] = g.gyro.z;
  // 温度数据（°C）
  obj["temperature"] = temp.temperature;

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
