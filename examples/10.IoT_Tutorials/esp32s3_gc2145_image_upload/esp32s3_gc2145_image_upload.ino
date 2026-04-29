/**
 * ============================================================
 *  Arduino IDE 开发板配置要求（烧录前必须检查）
 * ============================================================
 *  开发板 (Board):    ESP32S3 Dev Module
 *  PSRAM:             OPI PSRAM       ← 必须开启，否则摄像头无法使用 PSRAM
 *  Partition Scheme:  8MB with spiffs (3MB APP/1.5MB SPIFFS)
 *                     或 Huge APP (3MB No OTA/1MB SPIFFS)
 *                     ← 必须选择大于默认分区的方案，否则固件可能无法编译或运行
 *  Flash Mode:        QIO 80MHz
 *  Upload Speed:      921600
 * ============================================================
 *  依赖库版本要求:
 *    ThingsCloud_ESP_SDK >= 1.0.15
 * ============================================================
 *  验证通过的硬件:
 *    ESP32S3-CAM + GC2145 摄像头
 * ============================================================
 */

#include <ThingsCloudWiFiManager.h>
#include <ThingsCloudMQTT.h>
#include "esp_camera.h"
#include "Arduino.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "img_converters.h"

// ===================== 用户配置区域 =====================
// 请把下面的占位符替换为您的实际信息

// WiFi 配置
const char *WIFI_SSID = "";
const char *WIFI_PASSWORD = "";

// ThingsCloud MQTT 配置（一机一密）
// 在 ThingsCloud 控制台的设备详情页中，复制以下设备连接信息
// https://console.thingscloud.xyz
#define THINGSCLOUD_MQTT_HOST ""
#define THINGSCLOUD_DEVICE_ACCESS_TOKEN ""
#define THINGSCLOUD_PROJECT_KEY ""

// ThingsCloud 图片上传配置（请联系技术支持获取 endpoint）
const char *THINGSCLOUD_ENDPOINT = "";
const char *THINGSCLOUD_IDENTIFIER = "";
const char *THINGSCLOUD_REGION = "";

// 使用 HTTPS 还是 HTTP（HTTP 更省内存和 CPU，推荐内网/测试用）
#define THINGSCLOUD_USE_HTTPS false

// =================== ESP-S3-EYE 引脚定义 ===================
#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 15
#define SIOD_GPIO_NUM 4
#define SIOC_GPIO_NUM 5
#define Y9_GPIO_NUM 16
#define Y8_GPIO_NUM 17
#define Y7_GPIO_NUM 18
#define Y6_GPIO_NUM 12
#define Y5_GPIO_NUM 10
#define Y4_GPIO_NUM 8
#define Y3_GPIO_NUM 9
#define Y2_GPIO_NUM 11
#define VSYNC_GPIO_NUM 6
#define HREF_GPIO_NUM 7
#define PCLK_GPIO_NUM 13

#define BOOT_BUTTON_PIN 0

// =================== 摄像头运行参数 ===================
// 默认分辨率（GC2145 原生 4:3，16:9/9:16 会初始化失败）
framesize_t current_frame_size = FRAMESIZE_XGA; // 1024x768
// fmt2jpg 压缩质量（10~100，默认 80）
int current_jpeg_quality = 80;

bool camera_initialized = false;

// =================== ThingsCloud MQTT ===================
ThingsCloudMQTT client(
    THINGSCLOUD_MQTT_HOST,
    THINGSCLOUD_DEVICE_ACCESS_TOKEN,
    THINGSCLOUD_PROJECT_KEY);

// =================== 辅助函数：分辨率字符串 ↔ 枚举 ===================
framesize_t parseFrameSize(const char *str)
{
    if (strcmp(str, "QQVGA") == 0)
        return FRAMESIZE_QQVGA; // 160x120
    if (strcmp(str, "QVGA") == 0)
        return FRAMESIZE_QVGA; // 320x240
    if (strcmp(str, "CIF") == 0)
        return FRAMESIZE_CIF; // 400x296
    if (strcmp(str, "VGA") == 0)
        return FRAMESIZE_VGA; // 640x480
    if (strcmp(str, "SVGA") == 0)
        return FRAMESIZE_SVGA; // 800x600
    if (strcmp(str, "XGA") == 0)
        return FRAMESIZE_XGA; // 1024x768
    // 默认返回 XGA
    return FRAMESIZE_XGA;
}

const char *frameSizeToString(framesize_t size)
{
    switch (size)
    {
    case FRAMESIZE_QQVGA:
        return "QQVGA";
    case FRAMESIZE_QVGA:
        return "QVGA";
    case FRAMESIZE_CIF:
        return "CIF";
    case FRAMESIZE_VGA:
        return "VGA";
    case FRAMESIZE_SVGA:
        return "SVGA";
    case FRAMESIZE_XGA:
        return "XGA";
    default:
        return "XGA";
    }
}

// =================== WiFi 连接 ===================
void connectWiFi()
{
    Serial.print("[日志] 连接 WiFi: ");
    Serial.println(WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 30)
    {
        delay(500);
        Serial.print(".");
        retry++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println();
        Serial.print("[成功] WiFi 已连接，IP: ");
        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.println();
        Serial.println("[错误] WiFi 连接失败，请检查 SSID/密码，或重启设备");
    }
}

// =================== 上报当前摄像头属性到平台 ===================
void reportCameraAttributes()
{
    StaticJsonDocument<256> doc;
    doc["frame_size"] = frameSizeToString(current_frame_size);
    doc["jpeg_quality"] = current_jpeg_quality;
    String payload;
    serializeJson(doc, payload);
    client.reportAttributes(payload);
    Serial.printf("[日志] 上报属性 -> %s\n", payload.c_str());
}

// =================== 处理平台属性（下发或初始读取） ===================
// 注意：为避免运行时反复初始化摄像头导致 PSRAM 碎片化，
// frame_size 属性只在重启后生效（写入全局变量，上报平台）。
// jpeg_quality 可立即生效（仅影响 fmt2jpg 参数）。
void handleAttributes(const JsonObject &obj)
{
    bool frame_size_changed = false;

    if (obj.containsKey("frame_size"))
    {
        const char *fs = obj["frame_size"];
        framesize_t new_size = parseFrameSize(fs);
        if (new_size != current_frame_size)
        {
            Serial.printf("[日志] 更新 frame_size: %s -> %s（重启后生效）\n",
                          frameSizeToString(current_frame_size), fs);
            current_frame_size = new_size;
            frame_size_changed = true;
        }
    }

    if (obj.containsKey("jpeg_quality"))
    {
        int new_quality = obj["jpeg_quality"];
        if (new_quality >= 10 && new_quality <= 100)
        {
            if (new_quality != current_jpeg_quality)
            {
                Serial.printf("[日志] 更新 jpeg_quality: %d -> %d（立即生效）\n",
                              current_jpeg_quality, new_quality);
                current_jpeg_quality = new_quality;
            }
        }
        else
        {
            Serial.println("[警告] jpeg_quality 超出有效范围(10~100)，忽略");
        }
    }

    // 上报当前实际属性，保持平台与设备状态同步
    reportCameraAttributes();
}

// =================== 拍照并上传到 ThingsCloud ===================
void takePhotoAndUpload()
{
    // 拍照（RGB565）
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
    {
        Serial.println("[错误] 拍照失败，返回空帧");
        return;
    }

    Serial.printf("[成功] 拍照完成：%dx%d | 格式：RGB565 | 大小：%d 字节\n",
                  fb->width, fb->height, fb->len);

    // ThingsCloud 只支持 JPEG/PNG/GIF/WebP/BMP，不支持 RGB565
    // 所以必须转成 JPEG 再上传
    uint8_t *jpg_buf = NULL;
    size_t jpg_len = 0;
    bool jpg_ok = fmt2jpg(fb->buf, fb->len, fb->width, fb->height,
                          PIXFORMAT_RGB565, current_jpeg_quality, &jpg_buf, &jpg_len);

    // 释放摄像头帧缓冲（转换完成后即可释放）
    esp_camera_fb_return(fb);

    if (jpg_ok && jpg_buf != NULL)
    {
        Serial.printf("[成功] JPEG 转换完成：%d 字节\n", jpg_len);

        // ThingsCloud 单张图片限制 500KB
        if (jpg_len > 500 * 1024)
        {
            Serial.println("[警告] 图片超过 500KB，ThingsCloud 可能拒绝上传");
        }

        // 只有在 WiFi 连接时才上传
        if (WiFi.status() == WL_CONNECTED)
        {
            uploadToThingsCloud(jpg_buf, jpg_len);
        }
        else
        {
            Serial.println("[错误] WiFi 未连接，跳过上传");
        }

        free(jpg_buf);
    }
    else
    {
        Serial.println("[错误] JPEG 转换失败，可能内存不足");
    }
}

// =================== 处理云平台下发的命令 ===================
void handleCommand(const JsonObject &command)
{
    if (command.containsKey("method"))
    {
        String method = command["method"].as<String>();
        if (method == "take_photo")
        {
            Serial.println("[日志] 收到命令: take_photo，开始拍照上传...");
            if (camera_initialized)
            {
                takePhotoAndUpload();
            }
            else
            {
                Serial.println("[错误] 摄像头未初始化，无法拍照");
            }
            // 不回复命令结果
        }
        else if (method == "restart")
        {
            Serial.println("[日志] 收到命令: restart，断开 MQTT 并重启...");
            client.disconnect();
            delay(500);
            ESP.restart();
        }
    }
}

// =================== 上传图片到 ThingsCloud ===================
bool uploadToThingsCloud(uint8_t *image_buf, size_t image_len)
{
    HTTPClient http;

    String url = String(THINGSCLOUD_USE_HTTPS ? "https://" : "http://") + THINGSCLOUD_ENDPOINT + "/device/v1/" + THINGSCLOUD_DEVICE_ACCESS_TOKEN + "/" + THINGSCLOUD_IDENTIFIER + "/image";

    Serial.print("[日志] 上传图片到 ThingsCloud...\n       URL: ");
    Serial.println(url);

    http.begin(url);
    http.addHeader("Content-Type", "application/octet-stream");
    http.addHeader("Region", THINGSCLOUD_REGION);
    http.addHeader("Project-Key", THINGSCLOUD_PROJECT_KEY);

    int httpCode = http.POST(image_buf, image_len);
    String response = http.getString();
    http.end();

    if (httpCode == 200)
    {
        Serial.print("[成功] 上传完成，响应: ");
        Serial.println(response);
        return true;
    }
    else
    {
        Serial.print("[错误] 上传失败，HTTP 状态码: ");
        Serial.println(httpCode);
        Serial.print("[错误] 响应: ");
        Serial.println(response);
        return false;
    }
}

// =================== 必须实现的 MQTT 连接成功回调 ===================
void onMQTTConnect()
{
    // 订阅获取属性的回复消息（设备上电时主动读取平台属性）
    client.onAttributesGetResponse([](const String &topic, const JsonObject &obj)
                                   {
        if (obj["result"] == 1) {
            Serial.println("[日志] 收到平台属性初始值");
            handleAttributes(obj["attributes"]);
        } });

    // 订阅云平台下发属性的消息（平台实时修改属性）
    client.onAttributesPush([](const JsonObject &obj)
                            {
        Serial.println("[日志] 收到平台属性下发");
        handleAttributes(obj); });

    // 订阅云平台下发命令的消息
    client.onCommandSend([](const String &topic, const JsonObject &obj)
                         {
        Serial.println("[日志] 收到平台命令: " + topic);
        handleCommand(obj); });

    // 读取设备在云平台上的属性，用于初始化摄像头参数
    client.getAttributes();
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("===== ESP32S3-CAM MQTT + 按键拍照 + ThingsCloud 上传 =====");
    Serial.println();

    // 1. 连接 WiFi（保持和原始代码相同的顺序：先 WiFi，后摄像头）
    connectWiFi();
    Serial.println();

    // 2. 配置 ThingsCloud MQTT
    client.enableDebuggingMessages();

    // 3. 检查 PSRAM
    if (psramFound())
    {
        Serial.printf("[信息] PSRAM 已检测到，大小: %d MB\n", ESP.getPsramSize() / (1024 * 1024));
    }
    else
    {
        Serial.println("[警告] 未检测到 PSRAM！");
    }
    Serial.println();

    // 4. 配置 BOOT 按键
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

    // 5. 初始化摄像头（只在 setup 中调用一次，运行时不重新初始化）
    camera_config_t config = {};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.sccb_i2c_port = 1;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_RGB565;
    config.frame_size = current_frame_size;
    config.jpeg_quality = 0;
    config.fb_count = 2;
    config.fb_location = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
    config.grab_mode = CAMERA_GRAB_LATEST;

    Serial.println("[日志] 摄像头初始化开始...");
    delay(100);

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK)
    {
        Serial.printf("[错误] 摄像头初始化失败，错误码：0x%x\n", err);
        camera_initialized = false;
    }
    else
    {
        camera_initialized = true;
        Serial.println("[成功] 摄像头初始化完成！");
        sensor_t *s = esp_camera_sensor_get();
        if (s)
        {
            Serial.printf("[信息] 传感器 PID: 0x%04X (GC2145)\n", s->id.PID);
        }
    }

    Serial.println();
    Serial.println("==============================");
    Serial.println("[就绪] 请按 BOOT 键 (IO0) 或平台下发命令拍照并上传");
    Serial.println("==============================");
}

void loop()
{
    // MQTT 主循环（处理 WiFi / MQTT 连接、消息收发）
    client.loop();

    // 如果摄像头未初始化，跳过按键检测
    if (!camera_initialized)
    {
        delay(100);
        return;
    }

    // WiFi 断线时尝试重连（保持和原始代码相同的逻辑）
    if (WiFi.status() != WL_CONNECTED)
    {
        static unsigned long last_reconnect = 0;
        if (millis() - last_reconnect > 10000)
        {
            Serial.println("[日志] WiFi 已断开，尝试重连...");
            WiFi.reconnect();
            last_reconnect = millis();
        }
    }

    // 检测按键按下（低电平有效）
    if (digitalRead(BOOT_BUTTON_PIN) == LOW)
    {
        delay(50); // 消抖

        if (digitalRead(BOOT_BUTTON_PIN) == LOW)
        {
            Serial.println();
            Serial.println("[日志] 按键已按下，开始拍照...");

            takePhotoAndUpload();

            // 等待按键释放，防止连拍和限流（ThingsCloud 10秒限1次）
            Serial.println("[日志] 等待按键释放...");
            while (digitalRead(BOOT_BUTTON_PIN) == LOW)
            {
                delay(10);
            }
            Serial.println("[日志] 按键已释放，可以再次拍照");
        }
    }

    delay(10);
}
