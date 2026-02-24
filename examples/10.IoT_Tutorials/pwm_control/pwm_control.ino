/**
 * ESP32 PWM 控制示例
 *
 * 功能：通过 ThingsCloud 物联网平台下发命令控制 ESP32 的 PWM 输出，实现 LED 亮度调节
 *
 * ============================================================
 * LED 与 ESP32 接线方式
 * ============================================================
 *
 *   LED            ESP32
 *   ---            -----
 *   正极(长脚)  ->  GPIO 16 (或任何支持 PWM 的引脚)
 *   负极(短脚)  ->  220Ω 电阻 -> GND
 *
 * 注意：ESP32 的所有 GPIO 引脚都支持 PWM 输出（除仅输入引脚外）
 *
 * ============================================================
 * ThingsCloud 下发命令格式
 * ============================================================
 *
 * 方式一：通过属性下发（推荐用于简单的亮度控制）
 * {
 *   "pwm_duty": 128
 * }
 * pwm_duty 范围：0-255（0=关闭，255=最亮）
 *
 * 方式二：通过命令下发（支持更多控制参数）
 * {
 *   "method": "set_pwm",
 *   "params": {
 *     "duty": 128,
 *     "frequency": 5000
 *   }
 * }
 *
 * ============================================================
 */

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

// PWM 输出引脚（ESP32 几乎所有引脚都支持 PWM）
#define PWM_PIN 16

// PWM 参数配置
#define PWM_FREQ 5000      // PWM 频率（Hz）
#define PWM_RESOLUTION 8   // PWM 分辨率（8位 = 0-255）

// 当前 PWM 占空比（0-255）
int currentDuty = 0;

/**
 * @brief 初始化 PWM
 *
 * 使用 ESP32 Arduino Core 3.x 的 LEDC API 初始化 PWM 输出
 */
void initPWM()
{
  // 将引脚附加到 LEDC 通道
  // 参数：引脚号、频率、分辨率（位数）
  ledcAttach(PWM_PIN, PWM_FREQ, PWM_RESOLUTION);

  // 初始设置为关闭
  ledcWrite(PWM_PIN, 0);

  Serial.println("PWM initialized successfully");
  Serial.printf("Pin: %d, Frequency: %d Hz, Resolution: %d bits\n",
                PWM_PIN, PWM_FREQ, PWM_RESOLUTION);
}

/**
 * @brief 设置 PWM 占空比
 *
 * @param duty 占空比值（0-255）
 */
void setPWM(int duty)
{
  // 限制占空比范围
  if (duty < 0) duty = 0;
  if (duty > 255) duty = 255;

  currentDuty = duty;
  ledcWrite(PWM_PIN, duty);

  Serial.printf("PWM duty set to: %d (brightness: %.1f%%)\n",
                duty, (duty / 255.0) * 100);
}

/**
 * @brief 设置 PWM 频率和占空比
 *
 * @param freq 频率（Hz）
 * @param duty 占空比（0-255）
 */
void setPWMWithFreq(int freq, int duty)
{
  if (freq > 0 && freq <= 40000000)  // ESP32 PWM 最大频率约 40MHz
  {
    // 重新配置 PWM
    ledcDetach(PWM_PIN);
    ledcAttach(PWM_PIN, freq, PWM_RESOLUTION);
    setPWM(duty);
    Serial.printf("PWM frequency updated to: %d Hz\n", freq);
  }
}

/**
 * @brief 处理云平台下发的属性
 *
 * @param obj 包含属性的 Json 对象
 */
void handleAttributes(const JsonObject &obj)
{
  if (obj.containsKey("pwm_duty"))
  {
    int duty = obj["pwm_duty"];
    setPWM(duty);

    // 上报当前 PWM 状态
    reportPWMState();
  }
}

/**
 * @brief 处理云平台下发的命令
 *
 * @param command 包含命令信息的 Json 对象
 */
void handleCommand(const JsonObject &command)
{
  if (command.containsKey("method"))
  {
    String method = command["method"].as<String>();

    if (method == "set_pwm")
    {
      JsonObject params = command["params"];

      if (params.containsKey("duty"))
      {
        int duty = params["duty"];

        // 如果指定了频率，则同时更新频率
        if (params.containsKey("frequency"))
        {
          int freq = params["frequency"];
          setPWMWithFreq(freq, duty);
        }
        else
        {
          setPWM(duty);
        }

        // 上报当前 PWM 状态
        reportPWMState();
      }
    }
    else if (method == "pwm_fade")
    {
      // 渐变效果示例
      JsonObject params = command["params"];
      int targetDuty = params["target"];
      int duration = params.containsKey("duration") ? params["duration"] : 1000;

      fadePWM(targetDuty, duration);
    }
  }
}

/**
 * @brief 上报当前 PWM 状态到 ThingsCloud
 */
void reportPWMState()
{
  DynamicJsonDocument obj(256);
  obj["pwm_duty"] = currentDuty;
  obj["pwm_brightness"] = (int)((currentDuty / 255.0) * 100);  // 亮度百分比

  char attributes[256];
  serializeJson(obj, attributes);

  client.reportAttributes(attributes);
}

/**
 * @brief PWM 渐变效果（从当前值渐变到目标值）
 *
 * @param targetDuty 目标占空比（0-255）
 * @param duration 渐变持续时间（毫秒）
 */
void fadePWM(int targetDuty, int duration)
{
  if (targetDuty < 0) targetDuty = 0;
  if (targetDuty > 255) targetDuty = 255;

  int startDuty = currentDuty;
  int steps = abs(targetDuty - startDuty);
  int stepDelay = steps > 0 ? duration / steps : 0;

  Serial.printf("PWM fading from %d to %d over %d ms\n", startDuty, targetDuty, duration);

  if (targetDuty > startDuty)
  {
    // 渐亮
    for (int i = startDuty; i <= targetDuty; i++)
    {
      ledcWrite(PWM_PIN, i);
      delay(stepDelay);
    }
  }
  else
  {
    // 渐暗
    for (int i = startDuty; i >= targetDuty; i--)
    {
      ledcWrite(PWM_PIN, i);
      delay(stepDelay);
    }
  }

  currentDuty = targetDuty;

  // 上报最终状态
  reportPWMState();
}

void setup()
{
  Serial.begin(115200);

  // 初始化 PWM
  initPWM();

  // 允许 SDK 的日志输出
  client.enableDebuggingMessages();

  // 连接 WiFi AP
  client.setWifiCredentials(ssid, password);
}

// 必须实现这个回调函数，当 MQTT 连接成功后执行该函数。
void onMQTTConnect()
{
  // 订阅获取属性的回复消息
  client.onAttributesGetResponse([](const String &topic, const JsonObject &obj)
  {
    if (obj["result"] == 1)
    {
      handleAttributes(obj["attributes"]);
    }
  });

  // 订阅云平台下发属性的消息
  client.onAttributesPush([](const JsonObject &obj)
  {
    handleAttributes(obj);
  });

  // 订阅云平台下发命令的消息
  client.onCommandSend([](const String &topic, const JsonObject &obj)
  {
    Serial.println("recv command: " + topic);
    handleCommand(obj);
  });

  // 读取设备在云平台上的属性，用于初始化 PWM 状态
  // 云平台回复的属性会进入上边 client.onAttributesGetResponse 接收的消息
  client.getAttributes();

  // 延迟 3 秒后上报当前 PWM 状态
  client.executeDelayed(1000 * 3, []()
  {
    reportPWMState();
  });
}

void loop()
{
  client.loop();
}
