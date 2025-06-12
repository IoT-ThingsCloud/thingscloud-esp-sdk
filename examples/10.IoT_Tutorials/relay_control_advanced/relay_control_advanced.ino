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

// 设置继电器的输入引脚
#define RELAY_PIN 5

void setup()
{
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  // 初始断开继电器，输出高电平
  digitalWrite(RELAY_PIN, HIGH);

  // 允许 SDK 的日志输出
  client.enableDebuggingMessages();

  // 连接 WiFi AP
  client.setWifiCredentials(ssid, password);
}

/**
 * @brief 根据传入的状态控制继电器的开关
 * 
 * 该函数依据传入的布尔值 state 控制继电器引脚的电平，从而实现继电器的闭合与断开，
 * 同时会在串口打印当前继电器的状态信息。
 * 
 * @param state 继电器的目标状态，true 表示闭合继电器，false 表示断开继电器
 */
void controlRelay(bool state)
{
  if (state)
  {
    // 闭合继电器，输出低电平
    digitalWrite(RELAY_PIN, LOW);
    // 打印继电器闭合状态信息
    Serial.println("relay switch to ON");
  }
  else
  {
    // 断开继电器，输出高电平
    digitalWrite(RELAY_PIN, HIGH);
    // 打印继电器断开状态信息
    Serial.println("relay switch to OFF");
  }
}

void handleAttributes(const JsonObject &obj)
{
  if (obj.containsKey("relay"))
  {
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
void handleCommand(const JsonObject &command)
{
  if (command.containsKey("method") && command["method"] == "switch_relay")
  {
    JsonObject params = command["params"];
    bool state = params["state"];
    controlRelay(state);

    if (params.containsKey("delay_reverse") && params["delay_reverse"].is<int>()) {
      int delaySeconds = params["delay_reverse"];
      // 将秒转换为毫秒
      int delayMillis = delaySeconds * 1000; 
      // 使用 executeDelayed 函数在指定时间后反转继电器状态
      client.executeDelayed(delayMillis, [state]() {
        controlRelay(!state);
      });
    }
  }
}

// 必须实现这个回调函数，当 MQTT 连接成功后执行该函数。
void onMQTTConnect()
{

  // 订阅获取属性的回复消息
  client.onAttributesGetResponse([](const String &topic, const JsonObject &obj)
                                 {
    if (obj["result"] == 1) {
      handleAttributes(obj["attributes"]);
    } });

  // 订阅云平台下发属性的消息
  client.onAttributesPush([](const JsonObject &obj)
                          { handleAttributes(obj); });

  // 订阅云平台下发命令的消息
  // 订阅命令消息
  client.onCommandSend([](const String &topic, const JsonObject &obj)
                       {
    Serial.println("recv command: " + topic);
      handleCommand(obj); });

  // 读取设备在云平台上的属性，用于初始化继电器状态。
  // 云平台回复的属性会进入上边 client.onAttributesGetResponse 接收的消息。
  client.getAttributes();
}

void loop()
{
  client.loop();
}
