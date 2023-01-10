#include <ThingsCloudWiFiManager.h>
#include <ThingsCloudMQTT.h>

//======================================================
#define THINGSCLOUD_MQTT_HOST ""
#define THINGSCLOUD_PROJECT_KEY ""
#define THINGSCLOUD_TYPE_KEY ""
#define THINGSCLOUD_API_ENDPOINT ""
//======================================================

ThingsCloudMQTT client(
    THINGSCLOUD_MQTT_HOST,
    "",
    THINGSCLOUD_PROJECT_KEY,
    THINGSCLOUD_TYPE_KEY,
    THINGSCLOUD_API_ENDPOINT);

ThingsCloudWiFiManager wm("ESP32_DEVICE", "");

void onWifiConnectionLost();

const int LED_PIN = 3;
const int RESET_BUTTON_PIN = 4;
int lastState = LOW;
int currentState;
unsigned long pressedTime = 0;
unsigned long releasedTime = 0;

void setup()
{
    Serial.begin(115200);

    // set reset button gpio input mode
    pinMode(RESET_BUTTON_PIN, INPUT_PULLDOWN);

    // set led output mode
    pinMode(LED_PIN, OUTPUT);

    // show led blink
    for (int i = 0; i < 5; ++i)
    {
        digitalWrite(LED_PIN, HIGH);
        delay(200);
        digitalWrite(LED_PIN, LOW);
        delay(200);
    }
    // show wifi init
    digitalWrite(LED_PIN, HIGH);

    wm.linkMQTTClient(&client);

    if (!wm.autoConnect())
    {
        Serial.println("\nWiFi provisioning failed, will restart to retry");
        delay(1000);
        ESP.restart();
    }

    // show wifi connected
    digitalWrite(LED_PIN, LOW);

    client.enableDebuggingMessages();

    // client.setWifiCredentials(ssid, password);

    client.setOnMQTTDisconnect(onWifiConnectionLost);
}

void onWifiConnectionLost()
{
}

void onMQTTConnect()
{
    // Subscribe topics
    client.onAttributesResponse([](const String &payload)
                                { Serial.println("attributes response: " + payload); });

    client.onAttributesPush([](const String &payload)
                            { Serial.println("attributes push: " + payload); });
                            
    client.onCommandSend([](const String &topic, const String &payload)
                         { Serial.println("command send topic: " + topic + ", payload: " + payload); });

    client.onAttributesGetResponse([](const String &topic, const String &payload)
                                   { Serial.println("attributes get response: " + topic + ", payload: " + payload); });

    client.getAttributes();

    // delay report attributes
    client.executeDelayed(5 * 1000, []()
                          { client.reportAttributes("{\"start\":1}"); });
}

unsigned long timer1 = millis();
unsigned long timer2 = millis();

void pubSensors()
{
    DynamicJsonDocument obj(512);
    obj["temperature"] = 31.2;
    obj["humidity"] = 62.5;
    obj["co2"] = 2321;
    obj["light"] = 653;
    char attributes[512];
    serializeJson(obj, attributes);
    client.reportAttributes(attributes);
}

void pubEvent()
{
    DynamicJsonDocument obj(512);
    DynamicJsonDocument params(512);
    params["type"] = "start";
    obj["method"] = "eventTest";
    obj["params"] = params;
    char event[512];
    serializeJson(obj, event);
    client.reportEvent(1000, event);
}

void loop()
{
    currentState = digitalRead(RESET_BUTTON_PIN);
    if (currentState == HIGH && lastState == LOW)
    {
        pressedTime = millis();
        lastState = HIGH;
    }
    else if (currentState == HIGH && lastState == HIGH)
    {
        if (pressedTime > 0 && millis() - pressedTime > 2000)
        {
            // long press for 2000ms
            digitalWrite(LED_PIN, HIGH);
            Serial.println("\nWiFi reseting...");
            wm.resetSettings();
            delay(1000);
            Serial.println("\nESP restart...");
            ESP.restart();
        }
    }
    else
    {
        pressedTime = 0;
        lastState = LOW;
    }

    client.loop();

    while (millis() - timer1 > 1000 * 60 * 2)
    {
        timer1 = millis();
        pubSensors();
    }
    while (millis() - timer2 > 1000 * 60 * 5)
    {
        timer2 = millis();
        pubEvent();
    }
}
