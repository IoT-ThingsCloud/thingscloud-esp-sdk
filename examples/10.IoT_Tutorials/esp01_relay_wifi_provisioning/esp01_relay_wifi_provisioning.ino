/**
 * 功能介绍：ESP-01 高级继电器控制，支持 WiFi 配网和动态获取证书，支持属性下发和命令控制继电器，并实现延时自动反转状态。
 */

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
#define WiFi_AP_SSID "ESP01_RELAY" // AP 的 SSID 前缀，出现在用户 App 的 WiFi 列表中，可修改为你喜欢的名称
#define WiFi_AP_PASSWORD ""        // AP 的连接密码，可以不设置
ThingsCloudWiFiManager wm(WiFi_AP_SSID, WiFi_AP_PASSWORD);

// 设置控制继电器的GPIO输出引脚，可根据实际情况调整
#define RELAY_PIN 0

// 定义全局变量用于存储延迟任务信息
struct DelayTask {
  unsigned long startTime;
  unsigned long delayDuration;
  bool curState;
  bool active;
};
DelayTask delayTask = { 0, 0, false, false };

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  // 初始断开继电器，输出高电平
  digitalWrite(RELAY_PIN, HIGH);

  // 允许 SDK 的日志输出
  client.enableDebuggingMessages();

  // 关联 MQTT 客户端和配网管理器
  wm.linkMQTTClient(&client);

  // 调试配网可以取消以下注释，用于清空之前保存的 WiFi 配网信息，每次设备开机都需要重新配网。
  // 实际产品中，将清空配网信息放在例如设备的按键事件中，由用户操作触发设备重新进入配网状态。
  // wm.resetSettings();

  // 如果设备未配网，则启动 AP 配网模式，等待 ThingsX App 为设备配网
  // 如果已配网，则直接连接 WiFi
  if (!wm.autoConnect()) {
    Serial.println("\nWiFi provisioning failed, will restart to retry");
    delay(1000);
    ESP.restart();
  }
}

/**
 * @brief 根据传入的状态控制继电器的开关
 *
 * 该函数依据传入的布尔值 state 控制继电器引脚的电平，从而实现继电器的闭合与断开，
 * 同时会在串口打印当前继电器的状态信息。
 *
 * @param state 继电器的目标状态，true 表示闭合继电器，false 表示断开继电器
 */
void controlRelay(bool state) {
  if (state) {
    // 闭合继电器，输出低电平
    digitalWrite(RELAY_PIN, LOW);
    // 打印继电器闭合状态信息
    Serial.println("relay switch to ON");
  } else {
    // 断开继电器，输出高电平
    digitalWrite(RELAY_PIN, HIGH);
    // 打印继电器断开状态信息
    Serial.println("relay switch to OFF");
  }
}

void handleAttributes(const JsonObject &obj) {
  if (obj.containsKey("relay")) {
    // 接收到下发的 relay 属性
    controlRelay(obj["relay"]);
  }
}

/**
 * @brief 处理云平台下发的命令
 *
 * 该函数会检查命令中的方法是否为 "switch_relay"，若满足条件则获取继电器状态并控制继电器，
 * 若存在 "delay_reverse" 字段，会在指定秒数后反转继电器状态。
 *
 * @param command 包含命令信息的 Json 对象
 */
void handleCommand(const JsonObject &command) {
  if (command.containsKey("method") && command["method"] == "switch_relay") {
    JsonObject params = command["params"];
    bool state = params["state"];
    controlRelay(state);

    if (params.containsKey("delay_reverse") && params["delay_reverse"].is<int>() && params["delay_reverse"] > 0) {
      int delaySeconds = params["delay_reverse"];
      // 将秒转换为毫秒
      int delayMillis = delaySeconds * 1000;
      // 记录延迟任务信息
      delayTask.startTime = millis();
      delayTask.delayDuration = delayMillis;
      delayTask.curState = state;
      delayTask.active = true;
    }
  }
}

// 必须实现这个回调函数，当 MQTT 连接成功后执行该函数。
void onMQTTConnect() {

  // 订阅获取属性的回复消息
  client.onAttributesGetResponse([](const String &topic, const JsonObject &obj) {
    if (obj["result"] == 1) {
      handleAttributes(obj["attributes"]);
    }
  });

  // 订阅云平台下发属性的消息
  client.onAttributesPush([](const JsonObject &obj) {
    handleAttributes(obj);
  });

  // 订阅云平台下发命令的消息
  client.onCommandSend([](const String &topic, const JsonObject &obj) {
    Serial.println("recv command: " + topic);
    handleCommand(obj);
  });

  // 读取设备在云平台上的属性，用于初始化继电器状态。
  // 云平台回复的属性会进入上边 client.onAttributesGetResponse 接收的消息。
  client.getAttributes();
}

void loop() {
  client.loop();

  // 检查是否有延迟任务需要执行
  if (delayTask.active) {
    unsigned long curTime = millis();
    if (curTime - delayTask.startTime >= delayTask.delayDuration) {
      // 执行反转操作
      controlRelay(!delayTask.curState);
      // 标记任务完成
      delayTask.active = false;
    }
  }
}
