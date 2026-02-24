/**
 * 舵机控制示例
 *
 * 功能：通过 ThingsCloud 物联网平台下发命令控制舵机角度
 *
 * 所需库（通过 Arduino IDE 库管理器安装）：
 * - ESP32Servo by Kevin Harrington
 *
 * ============================================================
 * 舵机与 ESP32 接线方式
 * ============================================================
 *
 *   舵机线缆        ESP32
 *   --------       -----
 *   棕色/黑色(GND) -> GND
 *   红色(VCC)      -> 5V (外部电源推荐) 或 3.3V (小舵机)
 *   橙色/黄色(信号) -> GPIO 18 (或任何 PWM 支持的引脚)
 *
 * 注意事项：
 * 1. 普通舵机工作电压通常为 4.8V-6V，建议使用外部 5V 电源
 * 2. 大扭矩舵机务必使用外部电源，避免从 ESP32 取电
 * 3. 如果使用 ESP32 供电，确保舵机电流不超过 USB 供电能力
 *
 * ============================================================
 * ThingsCloud 下发命令格式
 * ============================================================
 *
 * 方式一：通过属性下发（推荐用于简单角度控制）
 * {
 *   "servo_angle": 90
 * }
 * servo_angle 范围：0-180（度）
 *
 * 方式二：通过命令下发（支持更多控制参数）
 * {
 *   "method": "set_servo",
 *   "params": {
 *     "angle": 90,
 *     "speed": 10
 *   }
 * }
 * speed: 角度变化速度（度/秒），可选参数
 *
 * 方式三：连续旋转命令
 * {
 *   "method": "servo_sweep",
 *   "params": {
 *     "start": 0,
 *     "end": 180,
 *     "speed": 30
 *   }
 * }
 *
 * ============================================================
 */

#include <ThingsCloudWiFiManager.h>
#include <ThingsCloudMQTT.h>
// 安装第三方依赖：ESP32Servo by Kevin Harrington
#include <ESP32Servo.h>

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

// 舵机信号引脚（ESP32 几乎所有引脚都支持 PWM）
#define SERVO_PIN 18

// 舵机角度范围
#define SERVO_MIN_ANGLE 0
#define SERVO_MAX_ANGLE 180

// 舵机脉冲宽度范围（微秒），根据舵机型号可能需要调整
#define SERVO_MIN_PULSE 500
#define SERVO_MAX_PULSE 2500

// 创建舵机对象
Servo servo;

// 当前舵机角度
int currentAngle = 90;

// 舵机扫描状态
struct SweepState {
  bool active;
  int startAngle;
  int endAngle;
  int speed;       // 度/秒
  bool direction;  // true = 增加, false = 减少
  unsigned long lastUpdate;
};
SweepState sweepState = { false, 0, 0, 0, true, 0 };

/**
 * @brief 初始化舵机
 */
void initServo()
{
  // 设置舵机参数
  servo.setPeriodHertz(50);    // 标准 50Hz 舵机频率
  servo.attach(SERVO_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);

  // 设置初始角度为 90 度（中位）
  servo.write(currentAngle);

  Serial.println("Servo initialized successfully");
  Serial.printf("Pin: %d, Frequency: 50 Hz, Range: %d-%d degrees\n",
                SERVO_PIN, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
}

/**
 * @brief 设置舵机角度
 *
 * @param angle 目标角度（0-180 度）
 */
void setServoAngle(int angle)
{
  // 限制角度范围
  if (angle < SERVO_MIN_ANGLE) angle = SERVO_MIN_ANGLE;
  if (angle > SERVO_MAX_ANGLE) angle = SERVO_MAX_ANGLE;

  currentAngle = angle;
  servo.write(angle);

  Serial.printf("Servo angle set to: %d degrees\n", angle);
}

/**
 * @brief 平滑移动舵机到目标角度
 *
 * @param targetAngle 目标角度（0-180 度）
 * @param speed 移动速度（度/秒）
 */
void moveServoSmooth(int targetAngle, int speed)
{
  // 限制角度范围
  if (targetAngle < SERVO_MIN_ANGLE) targetAngle = SERVO_MIN_ANGLE;
  if (targetAngle > SERVO_MAX_ANGLE) targetAngle = SERVO_MAX_ANGLE;

  // 限制速度范围
  if (speed < 1) speed = 1;
  if (speed > 180) speed = 180;

  // 停止任何正在进行的扫描
  sweepState.active = false;

  Serial.printf("Moving servo from %d to %d degrees at %d deg/s\n",
                currentAngle, targetAngle, speed);

  // 计算每度延迟时间（毫秒）
  int delayPerDegree = 1000 / speed;

  if (targetAngle > currentAngle)
  {
    // 顺时针旋转
    for (int angle = currentAngle; angle <= targetAngle; angle++)
    {
      servo.write(angle);
      delay(delayPerDegree);
    }
  }
  else
  {
    // 逆时针旋转
    for (int angle = currentAngle; angle >= targetAngle; angle--)
    {
      servo.write(angle);
      delay(delayPerDegree);
    }
  }

  currentAngle = targetAngle;

  // 上报最终状态
  reportServoState();
}

/**
 * @brief 开始舵机扫描（来回摆动）
 *
 * @param startAngle 起始角度
 * @param endAngle 结束角度
 * @param speed 扫描速度（度/秒）
 */
void startSweep(int startAngle, int endAngle, int speed)
{
  // 限制角度范围
  if (startAngle < SERVO_MIN_ANGLE) startAngle = SERVO_MIN_ANGLE;
  if (startAngle > SERVO_MAX_ANGLE) startAngle = SERVO_MAX_ANGLE;
  if (endAngle < SERVO_MIN_ANGLE) endAngle = SERVO_MIN_ANGLE;
  if (endAngle > SERVO_MAX_ANGLE) endAngle = SERVO_MAX_ANGLE;

  // 限制速度范围
  if (speed < 1) speed = 1;
  if (speed > 180) speed = 180;

  sweepState.startAngle = startAngle;
  sweepState.endAngle = endAngle;
  sweepState.speed = speed;
  sweepState.active = true;
  sweepState.direction = (endAngle > startAngle);
  sweepState.lastUpdate = millis();

  // 先移动到起始角度
  setServoAngle(startAngle);

  Serial.printf("Servo sweep started: %d <-> %d degrees at %d deg/s\n",
                startAngle, endAngle, speed);
}

/**
 * @brief 停止舵机扫描
 */
void stopSweep()
{
  if (sweepState.active)
  {
    sweepState.active = false;
    Serial.println("Servo sweep stopped");
    reportServoState();
  }
}

/**
 * @brief 处理舵机扫描（在 loop 中调用）
 */
void handleSweep()
{
  if (!sweepState.active) return;

  unsigned long currentTime = millis();
  int delayPerDegree = 1000 / sweepState.speed;

  // 检查是否到达更新时间
  if (currentTime - sweepState.lastUpdate >= (unsigned long)delayPerDegree)
  {
    sweepState.lastUpdate = currentTime;

    // 更新角度
    if (sweepState.direction)
    {
      currentAngle++;
      if (currentAngle >= sweepState.endAngle)
      {
        currentAngle = sweepState.endAngle;
        sweepState.direction = false;  // 反向
      }
    }
    else
    {
      currentAngle--;
      if (currentAngle <= sweepState.startAngle)
      {
        currentAngle = sweepState.startAngle;
        sweepState.direction = true;  // 反向
      }
    }

    servo.write(currentAngle);
  }
}

/**
 * @brief 上报当前舵机状态到 ThingsCloud
 */
void reportServoState()
{
  DynamicJsonDocument obj(256);
  obj["servo_angle"] = currentAngle;
  obj["servo_sweeping"] = sweepState.active;

  char attributes[256];
  serializeJson(obj, attributes);

  client.reportAttributes(attributes);
}

/**
 * @brief 处理云平台下发的属性
 *
 * @param obj 包含属性的 Json 对象
 */
void handleAttributes(const JsonObject &obj)
{
  if (obj.containsKey("servo_angle"))
  {
    int angle = obj["servo_angle"];

    // 停止扫描
    stopSweep();

    // 设置角度
    setServoAngle(angle);

    // 上报当前状态
    reportServoState();
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

    if (method == "set_servo")
    {
      JsonObject params = command["params"];

      if (params.containsKey("angle"))
      {
        int angle = params["angle"];

        if (params.containsKey("speed"))
        {
          int speed = params["speed"];
          moveServoSmooth(angle, speed);
        }
        else
        {
          // 停止扫描
          stopSweep();
          setServoAngle(angle);
          reportServoState();
        }
      }
    }
    else if (method == "servo_sweep")
    {
      JsonObject params = command["params"];

      int startAngle = params.containsKey("start") ? params["start"] : 0;
      int endAngle = params.containsKey("end") ? params["end"] : 180;
      int speed = params.containsKey("speed") ? params["speed"] : 30;

      startSweep(startAngle, endAngle, speed);
    }
    else if (method == "servo_stop")
    {
      stopSweep();
    }
  }
}

void setup()
{
  Serial.begin(115200);

  // 初始化舵机
  initServo();

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

  // 读取设备在云平台上的属性，用于初始化舵机状态
  // 云平台回复的属性会进入上边 client.onAttributesGetResponse 接收的消息
  client.getAttributes();

  // 延迟 3 秒后上报当前舵机状态
  client.executeDelayed(1000 * 3, []()
  {
    reportServoState();
  });
}

void loop()
{
  client.loop();

  // 处理舵机扫描
  handleSweep();
}
