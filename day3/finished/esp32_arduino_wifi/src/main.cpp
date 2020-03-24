#include <Arduino.h>
#include <WiFi.h>

// TODO: provide correct WiFi SSID
#define WIFI_SSID ""

// TODO: provide correct WiFi passphrase
#define WIFI_PASSPHRASE ""

void setup()
{
    Serial.begin(115200);

    WiFi.begin(WIFI_SSID, WIFI_PASSPHRASE);

    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(500);
    }

    Serial.println();

    Serial.println("Connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

void loop()
{

}