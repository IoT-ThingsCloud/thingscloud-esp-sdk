/**
 * 功能介绍：ESP8266 电源插座，支持 SoftAP 厂商配网、本地按键、云端开关控制、
 *           HLW8032 电压/电流/功率采集及累计电量持久化上报。
 * 适用芯片：ESP8266（参考硬件为 ESP-12F，4MB Flash）。不适用于 ESP32。
 * 开发板设置：Generic ESP8266 Module，Flash Size 4MB (FS:1MB)，串口 115200。
 * 硬件引脚：继电器 GPIO4、按键 GPIO12、指示灯 GPIO13、HLW8032 UART RX GPIO3。
 * 配网操作：ThingsX App 配网；长按按键超过 5 秒，闪灯三次并清除配网与接入凭据。
 * 注意事项：烧录和串口调试时必须断开市电；计量参考系数需要按实际硬件校准。
 */

#if !defined(ESP8266)
#error "This smart power socket example requires ESP8266."
#endif

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ThingsCloudMQTT.h>
#include <ThingsCloudWiFiManager.h>
#include <SoftwareSerial.h>
#include "Hlw8032Decoder.h"
#include <math.h>
#include "EnergyCounter.h"
#include "EnergyStore.h"
#include <time.h>
#include "ReportSchedule.h"

//======================================================
// 填写 ThingsCloud 控制台中设备类型的 ProductKey。
// WiFi、设备 API 和 MQTT 接入地址由 ThingsX App 配网时下发。
#define THINGSCLOUD_PRODUCT_KEY ""
//======================================================

// ESP8266 插座硬件配置：继电器高电平吸合，LED 高电平点亮，按键低电平有效。
#define RELAY_ACTIVE_HIGH 1
#define LED_ACTIVE_LOW 0
#define BUTTON_ACTIVE_LOW 1
#define WIFI_AP_PREFIX "SmartPower"
#define WIFI_AP_PASSWORD ""
#define WIFI_RESET_HOLD_MS 5000

// HLW8032：GPIO3 接收 4800 baud / 8E1 UART 数据。
// 下面是该参考板的暂定系数，实际产品应根据电阻参数和标准仪表校准。
#define METER_ENABLED 1
#define METER_REPORT_ENABLED 1
#define METER_CALIBRATION_CONFIRMED 0
#define METER_VOLTAGE_COEFFICIENT 1.88
#define METER_CURRENT_COEFFICIENT 1.0 // R001（0.001Ω）分流电阻的参考系数。

EspSoftwareSerial::UART meterSerial;
Hlw8032Decoder meter;
EnergyCounter energy;
EnergyStore energyStore;
uint32_t energySavedAt = 0, meterLogAt = 0;
WiFiEventHandler wifiDisconnectHandler;

void pollMeter()
{
#if METER_ENABLED
    while (meterSerial.available())
    {
        if (meter.feed(uint8_t(meterSerial.read()), millis()))
        {
            if (meter.energyValid)
                energy.observe(meter.pulseCount17, meter.powerParameter, millis(), METER_VOLTAGE_COEFFICIENT,
                               METER_CURRENT_COEFFICIENT);
            else
                energy.seen = false;
        }
    }
    if (millis() - meterLogAt >= 10000)
    {
        meterLogAt = millis();
        Serial.printf("HLW8032 frames=%lu checksum_errors=%lu parameter_errors=%lu fresh=%d state=0x%02X\n",
                      (unsigned long)meter.frames, (unsigned long)meter.checksumErrors,
                      (unsigned long)meter.parameterErrors, meter.fresh(millis()), meter.state);
        Serial.printf("Energy total=%.9f kWh storage=%d rejected_deltas=%lu\n", energy.totalKwh, energyStore.ready,
                      (unsigned long)energy.rejected);
        if (meter.fresh(millis()))
            Serial.printf("Meter raw ratios U=%.6f I=%.6f P=%.6f calibrated=%d\n", meter.voltageRatio,
                          meter.currentRatio, meter.powerRatio, METER_CALIBRATION_CONFIRMED);
    }
#endif
}

constexpr uint8_t RELAY_PIN = 4, BUTTON_PIN = 12, LED_PIN = 13;
constexpr uint32_t REPORT_MS = 60000, GET_RETRY_MS = 10000;
PeriodicReportClock reportClock(REPORT_MS);
OnlineReportGate onlineReport;
ReportKind pendingKind = ReportKind::NONE;
TCManufacturerConfig manufacturerConfig = {"", THINGSCLOUD_PRODUCT_KEY};
ThingsCloudMQTT client(manufacturerConfig);
ThingsCloudWiFiManager wm(WIFI_AP_PREFIX, WIFI_AP_PASSWORD);
bool switchOn = false, dirty = false, synchronized = false;
bool pendingGet = false, pendingReport = false;
uint32_t lastGet = 0, lastReportAttempt = 0;
uint32_t stateVersion = 0, reportVersion = 0;
uint32_t pendingPeriodicGeneration = 0, switchRetryAt = 0, periodicRetryAt = 0;
uint32_t onlineRetryAt = 0;
uint32_t nextRequestId = 1000;
String expectedResponseTopic;

void provisioningBlinkDelay(uint32_t duration)
{
    uint32_t startedAt = millis();
    while (millis() - startedAt < duration)
    {
        pollMeter(); // Keep draining the metering UART during the confirmation flashes.
        delay(1);
    }
}

void blinkProvisioningReset()
{
    const uint8_t on = LED_ACTIVE_LOW ? LOW : HIGH;
    const uint8_t off = LED_ACTIVE_LOW ? HIGH : LOW;
    digitalWrite(LED_PIN, off);
    provisioningBlinkDelay(150);
    for (uint8_t flash = 0; flash < 3; ++flash)
    {
        digitalWrite(LED_PIN, on);
        provisioningBlinkDelay(200);
        digitalWrite(LED_PIN, off);
        provisioningBlinkDelay(200);
    }
}

void resetProvisioning()
{
    if (!energyStore.save(energy.totalKwh))
    {
        Serial.println("WiFi reset cancelled: energy checkpoint failed");
        return;
    }
    blinkProvisioningReset();
    energyStore.save(energy.totalKwh);
    Serial.println("WiFi credentials reset; energy retained; restarting into Soft AP");
    client.disconnect();
    if (wm.getConfigPortalActive())
        wm.stopConfigPortal();
    wm.resetSettings();
    if (!client.clearPlatformCredentials().ok())
    {
        Serial.println("Platform credential clear failed; hold button to retry");
        return;
    }
    delay(100);
    ESP.restart();
}

String telemetryNumber(double value)
{
    if (!isfinite(value))
        return "0";
    String number(value, 3);
    if (number.indexOf('.') >= 0)
    {
        while (number.endsWith("0"))
            number.remove(number.length() - 1);
        if (number.endsWith("."))
            number.remove(number.length() - 1);
    }
    return number == "-0" ? String("0") : number;
}

String measurementNumber(bool valid, double value)
{
    // 未读取到有效计量值时，按示例约定上报数值 0。
    // UART diagnostics still expose freshness and frame validity separately.
    return valid ? telemetryNumber(value) : String("0");
}

void fillPeriodicReport(JsonDocument &report, bool state, const Hlw8032Decoder &sample, double totalKwh,
                        bool totalValid, uint32_t now)
{
    report["switch"] = state;
    report["voltage"] = serialized(measurementNumber(sample.haveVoltage && now - sample.voltageAt <= 3000,
                                                     sample.voltageRatio * METER_VOLTAGE_COEFFICIENT));
    report["current"] = serialized(measurementNumber(sample.haveCurrent && now - sample.currentAt <= 3000,
                                                     sample.currentRatio * METER_CURRENT_COEFFICIENT));
    report["power"] =
        serialized(measurementNumber(sample.havePower && now - sample.powerAt <= 3000,
                                     sample.powerRatio * METER_VOLTAGE_COEFFICIENT * METER_CURRENT_COEFFICIENT));
    report["energy_total"] = serialized(measurementNumber(totalValid, totalKwh));
}

bool retryReady(uint32_t now, uint32_t at)
{
    return at == 0 || int32_t(now - at) >= 0;
}

void reportFailure(ReportKind kind, uint32_t delayMs)
{
    if (kind == ReportKind::SWITCH_EVENT)
    {
        dirty = true;
        switchRetryAt = millis() + delayMs;
    }
    else if (kind == ReportKind::PERIODIC_FULL)
    {
        reportClock.due = true;
        periodicRetryAt = millis() + delayMs;
    }
    else if (kind == ReportKind::ONLINE_FULL)
    {
        onlineReport.pending = true;
        onlineRetryAt = millis() + delayMs;
    }
}

void processReports()
{
    reportClock.update(millis());
    if (!client.isConnected() || !synchronized)
        return;
    if (pendingReport && millis() - lastReportAttempt >= GET_RETRY_MS)
    {
        reportFailure(pendingKind, 1000);
        pendingReport = false;
        pendingKind = ReportKind::NONE;
    }
    if (pendingReport)
        return;

    ReportKind kind = ReportKind::NONE;
    if (onlineReport.ready(millis(), meter.fresh(millis())) && retryReady(millis(), onlineRetryAt))
        kind = ReportKind::ONLINE_FULL;
    else if (dirty && retryReady(millis(), switchRetryAt))
        kind = ReportKind::SWITCH_EVENT;
    else if (!onlineReport.pending && reportClock.due && retryReady(millis(), periodicRetryAt))
        kind = ReportKind::PERIODIC_FULL;
    if (kind == ReportKind::NONE)
        return;

    DynamicJsonDocument report(2048);
    if (kind == ReportKind::SWITCH_EVENT)
        report["switch"] = switchOn;
    else
    {
        // A storage failure must not silently remove energy_total from the packet.
        if (!energyStore.save(energy.totalKwh))
            Serial.println("Energy checkpoint failed before periodic report");
        fillPeriodicReport(report, switchOn, meter, energy.totalKwh, energyStore.ready, millis());
    }
    String payload;
    serializeJson(report, payload);
    lastReportAttempt = millis();
    if (client.reportAttributes(payload))
    {
        pendingReport = true;
        pendingKind = kind;
        reportVersion = stateVersion;
        pendingPeriodicGeneration = reportClock.generation;
        if (kind == ReportKind::ONLINE_FULL)
        {
            Serial.printf("Online full report after %lu ms: %s\n", (unsigned long)(millis() - onlineReport.startedAt),
                          payload.c_str());
        }
        else
            Serial.println(String(kind == ReportKind::PERIODIC_FULL ? "Periodic report " : "Switch event ") + payload);
    }
    else
        reportFailure(kind, 1000);
}

void updateOutputs()
{
    digitalWrite(RELAY_PIN, (switchOn == bool(RELAY_ACTIVE_HIGH)) ? HIGH : LOW);
    digitalWrite(LED_PIN, (switchOn != bool(LED_ACTIVE_LOW)) ? HIGH : LOW);
}

void applySwitch(bool value, const char *source)
{
    bool changed = value != switchOn || !synchronized;
    switchOn = value;
    updateOutputs();
    synchronized = true;
    pendingGet = false;
    if (changed)
    {
        ++stateVersion;
        bool cloudRestore = strcmp(source, "cloud-get") == 0 || strcmp(source, "cloud-empty-initialize") == 0;
        // The online full report acknowledges boot restoration; avoid a switch-only boot packet.
        if (!(cloudRestore && onlineReport.pending))
        {
            dirty = true;
            switchRetryAt = 0;
        }
    }
    Serial.printf("SWITCH=%s source=%s GPIO4=%d GPIO13=%d\n", value ? "ON" : "OFF", source, digitalRead(RELAY_PIN),
                  digitalRead(LED_PIN));
}

bool parseSwitch(const String &payload, bool snapshot)
{
    if (payload.length() > 1024)
    {
        Serial.println("JSON too large");
        return false;
    }
    DynamicJsonDocument doc(2048);
    if (deserializeJson(doc, payload) || !doc.is<JsonObject>())
    {
        Serial.println("Invalid JSON");
        return false;
    }
    if (snapshot && (!doc["result"].is<int>() || doc["result"].as<int>() != 1))
    {
        Serial.println("Cloud get failed; retrying without overwriting cloud state");
        return false;
    }
    if (snapshot && doc["attributes"].is<JsonObject>() && !doc["attributes"].as<JsonObject>().containsKey("switch"))
    {
        // A successful empty query means the device has never stored switch.
        // Seed the current local state only after confirming no cloud value exists.
        Serial.println("Cloud switch not initialized; publishing current local state");
        applySwitch(switchOn, "cloud-empty-initialize");
        return true;
    }
    JsonVariantConst value;
    if (snapshot)
        value = doc["attributes"]["switch"];
    else
        value = doc["switch"];
    if (!value.is<bool>())
    {
        Serial.println("No boolean switch; waiting for valid cloud state or local button");
        return false;
    }
    applySwitch(value.as<bool>(), snapshot ? "cloud-get" : "cloud-push");
    return true;
}

void requestSwitch()
{
    if (!client.isConnected())
        return;
    uint32_t id = nextRequestId++;
    if (nextRequestId > 999999)
        nextRequestId = 1000;
    expectedResponseTopic = "attributes/get/response/" + String(id);
    pendingGet = true;
    lastGet = millis();
    if (!client.publish("attributes/get/" + String(id), "{\"keys\":[\"switch\"]}"))
    {
        pendingGet = false;
    }
    Serial.println("Requested latest cloud switch");
}

void onMQTTConnect()
{
    Serial.println("MQTT connected");
    synchronized = false;
    dirty = false;
    onlineReport.begin(millis());
    onlineRetryAt = 0;
    pendingGet = false;
    pendingReport = false;
    pendingKind = ReportKind::NONE;
    bool pushed = client.subscribe("attributes/push",
                                   MessageReceivedCallback([](const String &payload) { parseSwitch(payload, false); }));
    bool fetched =
        client.subscribe("attributes/get/response/+", MessageReceivedCallbackWithTopic(
                                                          [](const String &topic, const String &payload)
                                                          {
                                                              if (pendingGet && topic == expectedResponseTopic)
                                                                  parseSwitch(payload, true);
                                                          }));
    bool acked = client.subscribe("attributes/response",
                                  MessageReceivedCallback(
                                      [](const String &payload)
                                      {
                                          if (!pendingReport)
                                              return;
                                          DynamicJsonDocument doc(2048);
                                          if (deserializeJson(doc, payload))
                                              return;
                                          ReportKind kind = pendingKind;
                                          pendingReport = false;
                                          pendingKind = ReportKind::NONE;
                                          if (doc["result"].as<int>() == 1)
                                          {
                                              if (stateVersion == reportVersion)
                                                  dirty = false;
                                              if (kind == ReportKind::PERIODIC_FULL)
                                                  reportClock.acknowledge(pendingPeriodicGeneration);
                                              if (kind == ReportKind::ONLINE_FULL)
                                              {
                                                  onlineReport.pending = false;
                                                  // If reconnecting after a missed minute, this full packet also covers
                                                  // it.
                                                  reportClock.acknowledge(pendingPeriodicGeneration);
                                              }
                                              Serial.println("Cloud confirmed attributes report");
                                          }
                                          else
                                          {
                                              reportFailure(kind, 35000);
                                              Serial.println("Cloud rejected attributes report: " + payload);
                                          }
                                      }));
    if (pushed && fetched && acked)
        requestSwitch();
    else
    {
        Serial.println("Subscription failed; reconnecting");
        client.disconnect();
    }
}

void setup()
{
    digitalWrite(RELAY_PIN, RELAY_ACTIVE_HIGH ? LOW : HIGH);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(LED_PIN, LED_ACTIVE_LOW ? HIGH : LOW);
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    // Keep TX for 115200 debug; reserve GPIO3 RX exclusively for the meter.
    Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
    Serial.println("\nESP8266 smart power socket (SoftAP manufacturer provisioning)");
    bool restored = energyStore.begin(energy.totalKwh);
    Serial.printf("Energy storage=%s restored=%.9f kWh\n", restored ? "READY" : "ERROR", energy.totalKwh);
    Serial.println("Boot OFF; waiting for latest cloud switch");
    Serial.println("Meter reporting enabled with PROVISIONAL coefficients U=1.88 I=1.0; not calibrated");
#if METER_ENABLED
    meterSerial.begin(4800, SWSERIAL_8E1, 3, -1, false, 256);
#endif
    WiFi.persistent(false);
    wifiDisconnectHandler =
        WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected &event)
                                       { Serial.printf("WiFi disconnected reason=%u\n", unsigned(event.reason)); });
    client.enableDebuggingMessages(false); // SDK debug output can contain credentials.
    client.setOnMQTTDisconnect(
        []()
        {
            Serial.println("MQTT disconnected");
            pendingGet = false;
            pendingReport = false;
        });
    configTime(0, 0, "ntp.aliyun.com", "pool.ntp.org", "time.cloudflare.com");
    wm.setDebugOutput(false);
    wm.linkMQTTClient(&client);
    wm.setConfigPortalBlocking(false);
    wm.setConfigPortalTimeout(0);
    wm.setConnectTimeout(15);
    wm.setSaveConnectTimeout(15);
    wm.setAPCallback(
        [](ThingsCloudWiFiManager *manager)
        {
            Serial.println("Soft AP ready: " + manager->getConfigPortalSSID());
            Serial.println("Use ThingsX App WiFi provisioning; short press switches, hold 5s resets WiFi");
        });
    // Only missing WiFi opens provisioning; saved networks retry in the background.
    if (!wm.begin()) Serial.println("WiFi manager initialization failed");
}

void handleButton()
{
    static bool previous = false, stable = false, armed = false;
    static uint32_t changedAt = 0, pressedAt = 0;
    static bool longHandled = false;
    bool pressed = digitalRead(BUTTON_PIN) == (BUTTON_ACTIVE_LOW ? LOW : HIGH);
    if (pressed != previous)
    {
        previous = pressed;
        changedAt = millis();
    }
    if (millis() - changedAt >= 40)
    {
        if (!pressed)
            armed = true;
        if (pressed != stable)
        {
            stable = pressed;
            if (pressed && armed)
            {
                pressedAt = millis();
                longHandled = false;
            }
            if (!pressed && armed && pressedAt)
            {
                if (!longHandled)
                    applySwitch(!switchOn, "button");
                pressedAt = 0;
            }
        }
        if (stable && armed && pressedAt && !longHandled && millis() - pressedAt > WIFI_RESET_HOLD_MS)
        {
            longHandled = true;
            resetProvisioning();
        }
    }
}

void loop()
{
    pollMeter();
    wm.process();
    if (!wm.getConfigPortalActive())
        client.loop();
    pollMeter();
    handleButton();
    if (millis() - energySavedAt >= 60000)
    {
        energySavedAt = millis();
        if (!energyStore.save(energy.totalKwh))
            Serial.println("Energy checkpoint failed");
    }
    static uint32_t statusAt = 0;
    if (millis() - statusAt >= 10000)
    {
        statusAt = millis();
        Serial.printf("Connection status: wifi=%d mqtt=%d synced=%d heap=%u\n", int(WiFi.status()),
                      client.isConnected(), synchronized, ESP.getFreeHeap());
    }
    static bool wifiWasUp = false;
    bool wifiUp = WiFi.status() == WL_CONNECTED;
    if (wifiUp != wifiWasUp)
    {
        wifiWasUp = wifiUp;
        Serial.printf("WiFi=%s IP=%s RSSI=%d\n", wifiUp ? "UP" : "DOWN", WiFi.localIP().toString().c_str(),
                      WiFi.RSSI());
    }
    if (client.isConnected() && !synchronized && millis() - lastGet >= GET_RETRY_MS)
        requestSwitch();
    processReports();

    delay(1);
}
