#include <Arduino.h>
#include <AzureIoTLiteClient.h>
#include <WiFiClientSecure.h>

const char *ssidName = "";         // your network SSID (name of wifi network)
const char *ssidPass = "";         // your network password
const char *connectionString = ""; // your device's connection string

AzureIoTConfig_t iotconfig(connectionString);
WiFiClientSecure wifiClient;
AzureIoTLiteClient iotclient(wifiClient);

bool isConnected = false;

static bool connectWiFi()
{
    Serial.printf("Connecting WIFI to SSID: %s\r\n", ssidName);

    WiFi.begin(ssidName, ssidPass);

    // Attempt to connect to Wifi network:
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        // wait 1 second for re-trying
        delay(1000);
    }

    Serial.printf("Connected to %s\r\n", ssidName);
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    return true;
}

void executeCommand(const char *cmdName, char *payload)
{
    if (strcmp(cmdName, "setLED") == 0)
    {
        LOG_VERBOSE("Executing setLED -> value: %s", payload);
        int ledVal = atoi(payload);
        digitalWrite(2, ledVal);
    }
}

void onEvent(const AzureIoTCallbacks_e cbType, const AzureIoTCallbackInfo_t *callbackInfo)
{
    // ConnectionStatus
    if (strcmp(callbackInfo->eventName, "ConnectionStatus") == 0)
    {
        LOG_VERBOSE("Is connected ? %s (%d)", callbackInfo->statusCode == AzureIoTConnectionOK ? "YES" : "NO", callbackInfo->statusCode);
        isConnected = callbackInfo->statusCode == AzureIoTConnectionOK;
        return;
    }

    // payload buffer doesn't have a null ending.
    // add null ending in another buffer before print
    AzureIOT::StringBuffer buffer;
    if (callbackInfo->payloadLength > 0)
    {
        buffer.initialize(callbackInfo->payload, callbackInfo->payloadLength);
    }

    LOG_VERBOSE("[%s] event was received. Payload => %s\n", callbackInfo->eventName, buffer.getLength() ? *buffer : "EMPTY");

    if (strcmp(callbackInfo->eventName, "Command") == 0)
    {
        LOG_VERBOSE("Command name was => %s\r\n", callbackInfo->tag);
        executeCommand(callbackInfo->tag, *buffer);
    }
}

void setup()
{
    delay(2000);
    Serial.begin(115200);
    Serial.println("It begins");

    if (!connectWiFi())
    {
        while (1)
            ;
    }

    // Prepare LED on WROVER
    pinMode(0, OUTPUT);
    pinMode(2, OUTPUT);
    pinMode(4, OUTPUT);

    // Turn off all LEDs on WROVER
    digitalWrite(0, LOW);
    digitalWrite(2, LOW);
    digitalWrite(4, LOW);

    // Add callbacks
    iotclient.setCallback(AzureIoTCallbackConnectionStatus, onEvent);
    iotclient.setCallback(AzureIoTCallbackMessageSent, onEvent);
    iotclient.setCallback(AzureIoTCallbackCommand, onEvent);
    iotclient.setCallback(AzureIoTCallbackSettingsUpdated, onEvent);

    iotclient.begin(&iotconfig);
}

unsigned long lastTick = 0, loopId = 0;
void loop()
{

    if (!iotclient.run())
    {
        return; //No point to continue
    }

    unsigned long ms = millis();
    if (ms - lastTick > 10000)
    { // send telemetry every 10 seconds
        char msg[64] = {0};
        int pos = 0;
        bool errorCode = false;

        lastTick = ms;

        if (loopId++ % 2 == 0)
        {
            // Send telemetry
            long tempVal = random(35, 40);
            pos = snprintf(msg, sizeof(msg) - 1, "{\"temperature\": %.2f}", tempVal * 1.0f);
            msg[pos] = 0;
            errorCode = iotclient.sendTelemetry(msg, pos);

            // Send property
            pos = snprintf(msg, sizeof(msg) - 1, "{\"temperatureAlert\": %d}", (tempVal >= 38));
            msg[pos] = 0;
            errorCode = iotclient.sendProperty(msg, pos);
        }
        else
        {
            // Send property
            pos = snprintf(msg, sizeof(msg) - 1, "{\"wornMask\": %d}", (random(0, 100) % 2 == 0));
            msg[pos] = 0;
            errorCode = iotclient.sendProperty(msg, pos);
        }

        if (!errorCode)
        {
            LOG_ERROR("Sending message has failed with error code %d", errorCode);
        }
    }
}