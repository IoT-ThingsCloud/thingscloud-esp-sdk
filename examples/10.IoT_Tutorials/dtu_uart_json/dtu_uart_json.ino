#include <ThingsCloudWiFiManager.h>
#include <ThingsCloudMQTT.h>
#include <HardwareSerial.h>
#include "time.h"

//======================================================
// 在 ThingsCloud 控制台的设备详情页中，复制以下设备连接信息
// https://console.thingscloud.xyz
#define THINGSCLOUD_MQTT_HOST ""     // MQTT 主机
#define THINGSCLOUD_PROJECT_KEY ""   // ProjectKey，同一个项目所有设备的 ProjectKey 相同
#define THINGSCLOUD_TYPE_KEY ""      // 设备类型的 TypeKey，用于自动创建设备
#define THINGSCLOUD_API_ENDPOINT ""  // HTTP 接入点，用于动态获取设备证书
//======================================================

ThingsCloudMQTT client(
  THINGSCLOUD_MQTT_HOST,
  "",  // DeviceKey 留空，SDK 自动为模组生成唯一标识作为 DeviceKey
  THINGSCLOUD_PROJECT_KEY,
  THINGSCLOUD_TYPE_KEY,
  THINGSCLOUD_API_ENDPOINT);

// ESP模组生成 WiFi AP，用于配网
#define WiFi_AP_SSID "ESP32_DEVICE"  // AP 的 SSID 前缀，出现在用户 App 的 WiFi 列表中，可修改为你喜欢的名称
#define WiFi_AP_PASSWORD ""          // AP 的连接密码，可以不设置
ThingsCloudWiFiManager wm(WiFi_AP_SSID, WiFi_AP_PASSWORD);

// 固件版本
#define VERSION "1.0.0"

// NTP server to request epoch time
const char *ntpServer = "pool.ntp.org";
// Variable to save current epoch time
unsigned long epochTime;
// Function that gets current epoch time
unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    // Serial.println("Failed to obtain time");
    return (0);
  }
  time(&now);
  return now;
}

// 使用 UART1 串口
HardwareSerial SerialPort(1);
// 不同型号模组 UART1 引脚不同
// ESP32C3
const int RX_PIN = 6;
const int TX_PIN = 7;
// ESP32WROON
// const int RX_PIN = 5;
// const int TX_PIN = 4;

// 设置 LED 指示灯 GPIO 引脚
const int LED_PIN = 3;

// 用于检测重置配网的按键，拉高 2 秒初始化设备重新进入 WiFi 配网
const int RESET_BUTTON_PIN = 18;
int lastState = LOW;
int currentState;
unsigned long pressedTime = 0;
unsigned long releasedTime = 0;

// 上报活跃消息的间隔时间计时器
unsigned long live_timer = millis();
// 设置定时上报数据的时间间隔，单位是 ms。免费版项目请务必大于30秒，否则设备可能会被限连。
const int live_report_interval = 1000 * 60 * 10;
// 上报活跃时间云平台回应计时器
unsigned long live_resp_timer = millis();
// 回应超时时间
const int live_resp_timeout = 1000 * 60 * 30;

void setup() {
  Serial.begin(115200);

  SerialPort.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);

  // 配网重置按键，默认下拉
  pinMode(RESET_BUTTON_PIN, INPUT_PULLDOWN);

  // 开机闪烁 LED 几次后常亮，表示未接入 WiFi
  pinMode(LED_PIN, OUTPUT);
  for (int i = 0; i < 5; ++i) {
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
  if (!wm.autoConnect()) {
    Serial.println("\nWiFi provisioning failed, will restart to retry");
    delay(1000);
    ESP.restart();
  }

  configTime(0, 0, ntpServer);
}

// 必须实现这个回调函数，当 MQTT 连接成功后执行该函数。
void onMQTTConnect() {
  // MQTT 连接成功后，熄灭 LED
  digitalWrite(LED_PIN, LOW);

  // 订阅属性上报的回复消息
  client.onAttributesResponse([](const String &payload) {
    Serial.println("attributes response: " + payload);
    // 记录最新的云平台回复时间
    live_resp_timer = millis();
  });

  // 订阅云平台下发属性的消息
  client.onAttributesPush([](const String &payload) {
    Serial.println("attributes push: " + payload);
    SerialPort.write(payload.c_str(), payload.length());
  });

  // 延迟 1 秒上报首次设备信息
  client.executeDelayed(1000 * 1, []() {
    pubStartInfo();
  });
}

void pubStartInfo() {
  // 上报开机初始信息
  DynamicJsonDocument obj(512);
  obj["version"] = VERSION;
  obj["chip_id"] = wm.getEspChipUniqueId();
  obj["wifi_ssid"] = wm.getWiFiSSID();
  obj["ip"] = WiFi.localIP().toString();
  obj["start_ts"] = getTime();
  char attributes[512];
  serializeJson(obj, attributes);
  // 调用属性上报方法
  client.reportAttributes(attributes);
}

void pubLiveInfo() {
  // 上报活跃信息
  DynamicJsonDocument obj(512);
  obj["live_ts"] = getTime();
  char attributes[512];
  serializeJson(obj, attributes);
  // 调用属性上报方法
  client.reportAttributes(attributes);
}

void checkResetButton() {
  currentState = digitalRead(RESET_BUTTON_PIN);
  if (currentState == HIGH && lastState == LOW) {
    pressedTime = millis();
    lastState = HIGH;
  } else if (currentState == HIGH && lastState == HIGH) {
    if (pressedTime > 0 && millis() - pressedTime > 2000) {
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
  } else {
    pressedTime = 0;
    lastState = LOW;
  }
}

void loop() {

  // 检查配网重置按键
  checkResetButton();

  client.loop();

  // 按间隔时间上报活跃消息
  if (millis() - live_timer > live_report_interval) {
    live_timer = millis();
    pubLiveInfo();
  }

  if (millis() - live_resp_timer > live_resp_timeout) {
    // 云平台回应超时，重启模组
    Serial.println("live resp timeout, restart");
    ESP.restart();
  }

  if (SerialPort.available()) {
    char buffer[512];
    size_t size = SerialPort.read(buffer, 512);
    if (size > 0) {
      buffer[size] = '\0';
      String bufferStr((char *)buffer);
      Serial.println("SerialPort.read " + bufferStr);
      // report to cloud
      client.reportAttributes(bufferStr);
    }
  }
}
