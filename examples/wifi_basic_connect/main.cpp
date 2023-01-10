#include <ThingsCloudMQTT.h>

//======================================================
const char *ssid = "";
const char *password = "";
#define THINGSCLOUD_MQTT_HOST ""
#define THINGSCLOUD_DEVICE_ACCESS_TOKEN ""
#define THINGSCLOUD_PROJECT_KEY ""
//======================================================

ThingsCloudMQTT client(
    THINGSCLOUD_MQTT_HOST,
    THINGSCLOUD_DEVICE_ACCESS_TOKEN,
    THINGSCLOUD_PROJECT_KEY);

void setup()
{
    Serial.begin(115200);

    // Connect to Wifi AP
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi ..");
    while (WiFi.status() != WL_CONNECTED)
    {
      Serial.print('.');
      delay(1000);
    }
    Serial.println("\nWiFi Connected!");
    Serial.println("IP Address: " + WiFi.localIP().toString());

    client.enableDebuggingMessages(); // Enable debugging messages sent to serial output
}

// This function is called once everything is connected (Wifi and MQTT)
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

    // Get device attributes from cloud
    client.getAttributes();

    // delay report attributes
    client.executeDelayed(5 * 1000, []()
                          { client.reportAttributes("{\"start\":1}"); });
}

unsigned long curTimer = millis();

void loop()
{
    client.loop();

    while (millis() - curTimer > 1000 * 60 * 5)
    {
        curTimer = millis();
        client.publish("attributes", "{\"temperature\":32}");
    }
}

