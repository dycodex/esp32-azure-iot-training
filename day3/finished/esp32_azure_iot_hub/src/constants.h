#ifndef ESP32_AZURE_IOT_HUB_CONSTANTS_H
#define ESP32_AZURE_IOT_HUB_CONSTANTS_H

// TODO: provide correct SSID for WiFi
#define WIFI_SSID "Dyware 3"

// TODO: provide correct password for WiFi
#define WIFI_PASSPHRASE "xxxxxxx"

// TODO: provide correct connectiong string for Azure IoT Hub connection
#define AZURE_IOT_CONNECTION_STRING "HostName=dx-dicon02.azure-devices.net;DeviceId=esp-02;SharedAccessKey=unTpysOMqfjt8aqEAvQcyPZDWEkkLUaj/I5O6iYzfY8="

// Interval of Device to Cloud (D2C) publishing in seconds
#define TX_INTERVAL_SECOND 10

#endif