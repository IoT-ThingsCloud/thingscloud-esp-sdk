#include <ThingsCloudWiFiManager.h>
#include <ThingsCloudMQTT.h>
#include <HTTPUpdate.h>
#include "time.h"

//======================================================
// 在 ThingsCloud 控制台的设备详情页中，复制以下设备连接信息
// https://console.thingscloud.xyz
#define THINGSCLOUD_MQTT_HOST ""                      // MQTT 主机
#define THINGSCLOUD_PROJECT_KEY ""                    // ProjectKey，同一个项目所有设备的 ProjectKey 相同
#define THINGSCLOUD_TYPE_KEY ""                       // 设备类型的 TypeKey，用于自动创建设备
#define THINGSCLOUD_API_ENDPOINT ""                   // HTTP 接入点，用于动态获取设备证书
//======================================================

ThingsCloudMQTT client(
  THINGSCLOUD_MQTT_HOST,
  "",  // DeviceKey 留空，SDK 自动为模组生成唯一标识作为 DeviceKey
  THINGSCLOUD_PROJECT_KEY,
  THINGSCLOUD_TYPE_KEY,
  THINGSCLOUD_API_ENDPOINT);

// ESP模组生成 WiFi AP，用于配网
#define WiFi_AP_SSID "ESP32_DEVICE"   // AP 的 SSID 前缀，出现在用户 App 的 WiFi 列表中，可修改为你喜欢的名称
#define WiFi_AP_PASSWORD ""           // AP 的连接密码，可以不设置
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
    //Serial.println("Failed to obtain time");
    return (0);
  }
  time(&now);
  return now;
}

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

  // 订阅属性上报的回复消息
  client.onAttributesResponse([](const String &payload) {
    Serial.println("attributes response: " + payload);
    // 记录最新的云平台回复时间
    live_resp_timer = millis();
  });

  // 订阅命令消息
  client.onCommandSend([](const String &topic, const JsonObject &obj) {
    Serial.println("recv command: " + topic);
    if (obj["method"] == "otaUpgrade") {
      handleOTA(obj["params"]);
    }
  });

  // 延迟 1 秒上报首次传感器数据
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

// OTA
void update_started(void) {
  Serial.println("OTA update process started");
}
void update_finished(void) {
  Serial.println("OTA update process finished");
}
void update_progress(int cur, int total) {
  Serial.printf("OTA update %d of %db\n", cur, total);
}
void update_error(int err) {
  Serial.printf("OTA update fatal error code %d\n", err);
}
void handleOTA(const JsonObject &options) {
  Serial.println("[OTA] check");
  httpUpdate.onStart(update_started);
  httpUpdate.onEnd(update_finished);
  httpUpdate.onProgress(update_progress);
  httpUpdate.onError(update_error);
  String url = options["url"];
  String version = options["version"];
  WiFiClient wifiClient;
  httpUpdate.update(wifiClient, url, version);
}

void loop() {
  
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
}
