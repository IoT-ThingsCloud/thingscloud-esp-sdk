/**
 * ESP32S3-CAM OV3660 摄像头图像上传示例
 *
 * 功能：通过 OV3660 摄像头采集图像，支持 BOOT 按键拍照和平台下发命令拍照，
 *       通过 HTTP 上传到 ThingsCloud。支持平台下发属性配置摄像头分辨率、
 *       JPEG 压缩质量、亮度、对比度、饱和度、锐度、降噪、白平衡、曝光等
 *       OV3660 特有画质参数，大部分参数可立即生效。
 *
 * ============================================================
 *  Arduino IDE 开发板配置要求（烧录前必须检查）
 * ============================================================
 *  开发板 (Board):    ESP32S3 Dev Module
 *  PSRAM:             OPI PSRAM       ← 必须开启，否则摄像头无法使用 PSRAM
 *  Partition Scheme:  8MB with spiffs (3MB APP/1.5MB SPIFFS)
 *                     或 Huge APP (3MB No OTA/1MB SPIFFS)
 *                     ← 必须选择大于默认分区的方案，否则固件可能无法编译或运行
 *  Flash Mode:        QIO 80MHz
 *  Flash Size:        8MB (64Mb)
 *  Upload Speed:      921600
 * ============================================================
 *  依赖库版本要求:
 *    ThingsCloud_ESP_SDK >= 1.0.15
 * ============================================================
 *  验证通过的硬件:
 *    ESP32S3-EYE + OV3660 摄像头
 * ============================================================
 */

#include <ThingsCloudWiFiManager.h>
#include <ThingsCloudMQTT.h>
#include "esp_camera.h"
#include "Arduino.h"
#include <WiFi.h>
#include <HTTPClient.h>

#define CAMERA_MODEL_ESP32S3_EYE
#include "camera_pins.h"

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

// ThingsCloud 图片上传配置（企业版支持设备图片上传，请联系技术支持获取 endpoint）
const char *THINGSCLOUD_ENDPOINT = "";
const char *THINGSCLOUD_IDENTIFIER = "";
const char *THINGSCLOUD_REGION = "";

// 使用 HTTPS 还是 HTTP（HTTP 更省内存和 CPU，推荐内网/测试用）
#define THINGSCLOUD_USE_HTTPS false

#define BOOT_BUTTON_PIN 0

// ===================== 摄像头运行参数 =====================
// 默认分辨率（OV3660 支持 UXGA 1600x1200）
framesize_t current_frame_size = FRAMESIZE_UXGA;
// JPEG 压缩质量（0~63，数值越小质量越高，推荐 6~12）
int current_jpeg_quality = 6;

// OV3660 画质参数（可通过平台属性实时调节）
int current_brightness = 1;       // 亮度：-2 ~ +2
int current_contrast = 1;         // 对比度：-2 ~ +2
int current_saturation = 0;       // 饱和度：-2 ~ +2
int current_sharpness = 2;        // 锐度：0 ~ 3（OV3660 特有）
int current_denoise = 1;          // 降噪：0 ~ 3
int current_vflip = 1;            // 垂直翻转：0/1
int current_hmirror = 0;          // 水平镜像：0/1
int current_wb_mode = 0;          // 白平衡模式：0=自动, 1=日光, 2=阴天, 3=办公室, 4=家居
int current_ae_level = 0;         // 曝光等级：-2 ~ +2
int current_special_effect = 0;   // 特殊效果：0=关闭, 1=负片, 2=灰度, 3=红色色调, 4=绿色色调, 5=蓝色色调, 6=复古
int current_dcw = 0;              // 降采样(DCW)：0/1

bool camera_initialized = false;

// ===================== ThingsCloud MQTT =====================
ThingsCloudMQTT client(
    THINGSCLOUD_MQTT_HOST,
    THINGSCLOUD_DEVICE_ACCESS_TOKEN,
    THINGSCLOUD_PROJECT_KEY);

// ===================== 辅助函数：分辨率字符串 ↔ 枚举 =====================
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
    if (strcmp(str, "SXGA") == 0)
        return FRAMESIZE_SXGA; // 1280x1024
    if (strcmp(str, "UXGA") == 0)
        return FRAMESIZE_UXGA; // 1600x1200
    // 默认返回 UXGA
    return FRAMESIZE_UXGA;
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
    case FRAMESIZE_SXGA:
        return "SXGA";
    case FRAMESIZE_UXGA:
        return "UXGA";
    default:
        return "UXGA";
    }
}

// ===================== WiFi 连接 =====================
void connectWiFi()
{
    Serial.print("[日志] 连接 WiFi: ");
    Serial.println(WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    WiFi.setSleep(false);

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

// ===================== 应用摄像头画质参数 =====================
void applyCameraSettings(sensor_t *s)
{
    if (!s)
        return;

    // 基础翻转（ESP32S3-EYE 硬件安装方向）
    s->set_vflip(s, current_vflip);
    s->set_hmirror(s, current_hmirror);

    // JPEG 质量
    s->set_quality(s, current_jpeg_quality);

    // 画质参数
    s->set_brightness(s, current_brightness);
    s->set_contrast(s, current_contrast);
    s->set_saturation(s, current_saturation);
    s->set_sharpness(s, current_sharpness);
    s->set_denoise(s, current_denoise);

    // 白平衡与曝光
    s->set_whitebal(s, 1);
    s->set_wb_mode(s, current_wb_mode);
    s->set_exposure_ctrl(s, 1);
    s->set_aec2(s, 1);
    s->set_ae_level(s, current_ae_level);

    // 自动增益
    s->set_gain_ctrl(s, 1);
    s->set_gainceiling(s, GAINCEILING_16X);

    // 像素校正
    s->set_bpc(s, 1);
    s->set_wpc(s, 1);

    // RAW Gamma
    s->set_raw_gma(s, 1);

    // 镜头校正
    s->set_lenc(s, 1);

    // 特殊效果与彩条
    s->set_special_effect(s, current_special_effect);
    s->set_colorbar(s, 0);

    // 降采样
    s->set_dcw(s, current_dcw);
}

// ===================== 上报当前摄像头属性到平台 =====================
void reportCameraAttributes()
{
    StaticJsonDocument<512> doc;
    doc["frame_size"] = frameSizeToString(current_frame_size);
    doc["jpeg_quality"] = current_jpeg_quality;
    doc["brightness"] = current_brightness;
    doc["contrast"] = current_contrast;
    doc["saturation"] = current_saturation;
    doc["sharpness"] = current_sharpness;
    doc["denoise"] = current_denoise;
    doc["vflip"] = current_vflip;
    doc["hmirror"] = current_hmirror;
    doc["wb_mode"] = current_wb_mode;
    doc["ae_level"] = current_ae_level;
    doc["special_effect"] = current_special_effect;
    doc["dcw"] = current_dcw;
    String payload;
    serializeJson(doc, payload);
    client.reportAttributes(payload);
    Serial.printf("[日志] 上报属性 -> %s\n", payload.c_str());
}

// ===================== 处理平台属性（下发或初始读取） =====================
// 注意：为避免运行时反复初始化摄像头导致 PSRAM 碎片化，
// frame_size 属性只在重启后生效（写入全局变量，上报平台）。
// jpeg_quality 及其他 sensor 参数可立即生效。
void handleAttributes(const JsonObject &obj)
{
    bool frame_size_changed = false;
    sensor_t *s = camera_initialized ? esp_camera_sensor_get() : nullptr;

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
        if (new_quality >= 0 && new_quality <= 63)
        {
            if (new_quality != current_jpeg_quality)
            {
                Serial.printf("[日志] 更新 jpeg_quality: %d -> %d\n",
                              current_jpeg_quality, new_quality);
                current_jpeg_quality = new_quality;
                if (s)
                    s->set_quality(s, new_quality);
            }
        }
        else
        {
            Serial.println("[警告] jpeg_quality 超出有效范围(0~63)，忽略");
        }
    }

    if (obj.containsKey("brightness"))
    {
        int v = obj["brightness"];
        if (v >= -2 && v <= 2 && v != current_brightness)
        {
            Serial.printf("[日志] 更新 brightness: %d -> %d\n", current_brightness, v);
            current_brightness = v;
            if (s)
                s->set_brightness(s, v);
        }
    }

    if (obj.containsKey("contrast"))
    {
        int v = obj["contrast"];
        if (v >= -2 && v <= 2 && v != current_contrast)
        {
            Serial.printf("[日志] 更新 contrast: %d -> %d\n", current_contrast, v);
            current_contrast = v;
            if (s)
                s->set_contrast(s, v);
        }
    }

    if (obj.containsKey("saturation"))
    {
        int v = obj["saturation"];
        if (v >= -2 && v <= 2 && v != current_saturation)
        {
            Serial.printf("[日志] 更新 saturation: %d -> %d\n", current_saturation, v);
            current_saturation = v;
            if (s)
                s->set_saturation(s, v);
        }
    }

    if (obj.containsKey("sharpness"))
    {
        int v = obj["sharpness"];
        if (v >= 0 && v <= 3 && v != current_sharpness)
        {
            Serial.printf("[日志] 更新 sharpness: %d -> %d\n", current_sharpness, v);
            current_sharpness = v;
            if (s)
                s->set_sharpness(s, v);
        }
    }

    if (obj.containsKey("denoise"))
    {
        int v = obj["denoise"];
        if (v >= 0 && v <= 3 && v != current_denoise)
        {
            Serial.printf("[日志] 更新 denoise: %d -> %d\n", current_denoise, v);
            current_denoise = v;
            if (s)
                s->set_denoise(s, v);
        }
    }

    if (obj.containsKey("vflip"))
    {
        int v = obj["vflip"];
        if ((v == 0 || v == 1) && v != current_vflip)
        {
            Serial.printf("[日志] 更新 vflip: %d -> %d\n", current_vflip, v);
            current_vflip = v;
            if (s)
                s->set_vflip(s, v);
        }
    }

    if (obj.containsKey("hmirror"))
    {
        int v = obj["hmirror"];
        if ((v == 0 || v == 1) && v != current_hmirror)
        {
            Serial.printf("[日志] 更新 hmirror: %d -> %d\n", current_hmirror, v);
            current_hmirror = v;
            if (s)
                s->set_hmirror(s, v);
        }
    }

    if (obj.containsKey("wb_mode"))
    {
        int v = obj["wb_mode"];
        if (v >= 0 && v <= 4 && v != current_wb_mode)
        {
            Serial.printf("[日志] 更新 wb_mode: %d -> %d\n", current_wb_mode, v);
            current_wb_mode = v;
            if (s)
                s->set_wb_mode(s, v);
        }
    }

    if (obj.containsKey("ae_level"))
    {
        int v = obj["ae_level"];
        if (v >= -2 && v <= 2 && v != current_ae_level)
        {
            Serial.printf("[日志] 更新 ae_level: %d -> %d\n", current_ae_level, v);
            current_ae_level = v;
            if (s)
                s->set_ae_level(s, v);
        }
    }

    if (obj.containsKey("special_effect"))
    {
        int v = obj["special_effect"];
        if (v >= 0 && v <= 6 && v != current_special_effect)
        {
            Serial.printf("[日志] 更新 special_effect: %d -> %d\n", current_special_effect, v);
            current_special_effect = v;
            if (s)
                s->set_special_effect(s, v);
        }
    }

    if (obj.containsKey("dcw"))
    {
        int v = obj["dcw"];
        if ((v == 0 || v == 1) && v != current_dcw)
        {
            Serial.printf("[日志] 更新 dcw: %d -> %d\n", current_dcw, v);
            current_dcw = v;
            if (s)
                s->set_dcw(s, v);
        }
    }

    // 动态修改 sensor 寄存器后，给 OV3660 一点稳定时间，
    // 避免下一帧 DMA 捕获到异常空帧。
    if (s)
    {
        delay(50);
    }

    // 上报当前实际属性，保持平台与设备状态同步
    reportCameraAttributes();
}

// ===================== 拍照并上传到 ThingsCloud =====================
void takePhotoAndUpload()
{
    // 拍照（JPEG 直出）
    // OV3660 在动态修改 sensor 参数后，可能需要丢弃 1~2 帧才能稳定，
    // 因此加入重试机制，最多尝试 3 次。
    camera_fb_t *fb = nullptr;
    for (int retry = 0; retry < 3; retry++)
    {
        fb = esp_camera_fb_get();
        if (fb)
            break;
        if (retry < 2)
        {
            Serial.printf("[警告] 拍照返回空帧，第 %d 次重试...\n", retry + 1);
            delay(150);
        }
    }
    if (!fb)
    {
        Serial.println("[错误] 拍照失败，连续 3 次返回空帧");
        return;
    }

    Serial.printf("[成功] 拍照完成：%dx%d | 格式：JPEG | 大小：%d 字节\n",
                  fb->width, fb->height, fb->len);

    // ThingsCloud 单张图片限制 500KB
    if (fb->len > 500 * 1024)
    {
        Serial.println("[警告] 图片超过 500KB，ThingsCloud 可能拒绝上传");
    }

    // 只有在 WiFi 连接时才上传
    if (WiFi.status() == WL_CONNECTED)
    {
        uploadToThingsCloud(fb->buf, fb->len);
    }
    else
    {
        Serial.println("[错误] WiFi 未连接，跳过上传");
    }

    // 释放摄像头帧缓冲
    esp_camera_fb_return(fb);
}

// ===================== 处理云平台下发的命令 =====================
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

// ===================== 上传图片到 ThingsCloud =====================
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

// ===================== 必须实现的 MQTT 连接成功回调 =====================
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
    Serial.println("===== ESP32S3-CAM OV3660 MQTT + 按键拍照 + ThingsCloud 上传 =====");
    Serial.println();

    // 1. 连接 WiFi
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
    config.pixel_format = PIXFORMAT_JPEG;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    config.fb_location = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;

    // 根据 PSRAM 情况配置分辨率与缓冲
    if (psramFound())
    {
        config.frame_size = current_frame_size;
        config.jpeg_quality = current_jpeg_quality;
        config.fb_count = 2;
        config.grab_mode = CAMERA_GRAB_LATEST;
    }
    else
    {
        // 无 PSRAM 时，如果 frame_size 过大则降级到 SVGA
        if (current_frame_size > FRAMESIZE_SVGA)
        {
            Serial.println("[警告] 无 PSRAM，frame_size 过大，自动降级到 SVGA");
            current_frame_size = FRAMESIZE_SVGA;
        }
        config.frame_size = current_frame_size;
        config.jpeg_quality = current_jpeg_quality;
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_DRAM;
    }

#if defined(CAMERA_MODEL_ESP_EYE)
    pinMode(13, INPUT_PULLUP);
    pinMode(14, INPUT_PULLUP);
#endif

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

        // 应用画质参数
        sensor_t *s = esp_camera_sensor_get();
        if (s)
        {
            Serial.printf("[信息] 传感器 PID: 0x%04X (OV3660)\n", s->id.PID);
            applyCameraSettings(s);
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

    // WiFi 断线时尝试重连
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
