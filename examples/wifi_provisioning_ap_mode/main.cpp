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

void setup()
{
    Serial.begin(115200);

    ThingsCloudWiFiManager wm("ESP32_DEVICE", "");
    wm.linkMQTTClient(&client);

    if (!wm.autoConnect())
    {
        Serial.println("\nWiFi provisioning failed, will restart to retry");
        delay(1000);
        ESP.restart();
    }

    client.enableDebuggingMessages();
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
