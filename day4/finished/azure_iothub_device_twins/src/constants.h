#ifndef ESP32_AZURE_IOT_HUB_CONSTANTS_H
#define ESP32_AZURE_IOT_HUB_CONSTANTS_H

// TODO: provide correct SSID for WiFi
#define WIFI_SSID "Dyware 3"

// TODO: provide correct password for WiFi
#define WIFI_PASSPHRASE "p@ssw0rd"

// TODO: provide correct connectiong string for Azure IoT Hub connection
#define AZURE_IOT_CONNECTION_STRING "HostName=dycodexiothub1.azure-devices.net;DeviceId=espwrover;SharedAccessKey=ay5ERnaA+1Yvhrzxssff+S/WXyp7VLeYUdqOsc33RyE="

// Interval of Device to Cloud (D2C) publishing in seconds
#define TX_INTERVAL_SECOND 60

#endif