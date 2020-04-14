#include <stdio.h>
#include <stdlib.h>

#include "parson.h"
#include "iothub_client.h"
#include "iothub_device_client_ll.h"
#include "iothub_client_options.h"
#include "iothub_message.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/crt_abstractions.h"
#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/shared_util_options.h"
#include "iothubtransportmqtt.h"
#include "iothub_client_options.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef MBED_BUILD_TIMESTAMP
#define SET_TRUSTED_CERT_IN_SAMPLES
#endif // MBED_BUILD_TIMESTAMP

#ifdef SET_TRUSTED_CERT_IN_SAMPLES
#include "certs.h"
#endif // SET_TRUSTED_CERT_IN_SAMPLES

/*String containing Hostname, Device Id & Device Key in the format:                         */
/*  "HostName=<host_name>;DeviceId=<device_id>;SharedAccessKey=<device_key>"                */
/*  "HostName=<host_name>;DeviceId=<device_id>;SharedAccessSignature=<device_sas_token>"    */
#include "constants.h"
static const char *connectionString = AZURE_IOT_CONNECTION_STRING;

typedef struct CONNECTIVITY_TAG
{
    char *type;
} Connectivity;

typedef struct DESIRED_PROPS_STRUCT
{
    unsigned int tx_interval;
} DesiredProps;

typedef struct REPORTED_PROPS_STRUCT
{
    Connectivity connectivity;
} ReportedProps;

typedef struct DEVICE_TAG
{
    DesiredProps desired;
    ReportedProps reported;
} Device;

static int callbackCounter;
static char msgText[1024];

typedef struct EVENT_INSTANCE_TAG
{
    IOTHUB_MESSAGE_HANDLE messageHandle;
    size_t messageTrackingId; // For tracking the messages within the user callback.
} EVENT_INSTANCE;

static char *serializeTwinToJSON(Device *device)
{
    char *result;

    JSON_Value *root_value = json_value_init_object();
    JSON_Object *root_object = json_value_get_object(root_value);

    json_object_dotset_string(root_object, "connectivity.type", device->reported.connectivity.type);

    result = json_serialize_to_string(root_value);
    json_value_free(root_value);

    return result;
}

static Device *parseTwinFromJSON(const char *json, DEVICE_TWIN_UPDATE_STATE update_state)
{
    Device *device = malloc(sizeof(Device));
    JSON_Value *root_value = NULL;
    JSON_Object *root_object = NULL;

    if (device == NULL)
    {
        printf("Failed to allocate memory for incoming device twin!\r\n");
        return NULL;
    }

    memset(device, 0, sizeof(Device));
    root_value = json_parse_string(json);
    root_object = json_value_get_object(root_value);

    JSON_Value *desiredTxInterval;

    if (update_state == DEVICE_TWIN_UPDATE_COMPLETE)
    {
        desiredTxInterval = json_object_dotget_value(root_object, "desired.tx_interval");
    }
    else
    {
        desiredTxInterval = json_object_get_value(root_object, "tx_interval");
    }

    if (desiredTxInterval != NULL)
    {
        unsigned int data = (unsigned int)json_value_get_number(desiredTxInterval);
        if (data > 0)
        {
            device->desired.tx_interval = data;
        }
    }

    json_value_free(root_value);

    return device;
}

static void DeviceTwinCallback(DEVICE_TWIN_UPDATE_STATE update_state, const unsigned char *payload, size_t size, void *userContextCallback)
{
    printf("Received device twin:\r\n");
    printf("<<<%.*s>>>\r\n", (int)size, payload);

    Device *oldDevice = (Device *)userContextCallback;
    Device *newDevice = parseTwinFromJSON((const char *)payload, update_state);

    if (newDevice == NULL)
    {
        printf("Failed parsing json from device twin\r\n");
        return;
    }

    if (newDevice->desired.tx_interval > 0 && newDevice->desired.tx_interval != oldDevice->desired.tx_interval)
    {
        printf("Received new tx_interval\r\n");
        oldDevice->desired.tx_interval = newDevice->desired.tx_interval;
    }

    free(newDevice);
}

static void ReportedStateCallback(int status_code, void *userContextCallback)
{
    printf("Device Twin reported properties update completed with result: %d\r\n", status_code);
}

static void SendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void *userContextCallback)
{
    EVENT_INSTANCE *eventInstance = (EVENT_INSTANCE *)userContextCallback;
    size_t id = eventInstance->messageTrackingId;

    if (result == IOTHUB_CLIENT_CONFIRMATION_OK)
    {
        (void)printf("Confirmation[%d] received for message tracking id = %d with result = %s\r\n", callbackCounter, (int)id, ENUM_TO_STRING(IOTHUB_CLIENT_CONFIRMATION_RESULT, result));
        /* Some device specific action code goes here... */
        callbackCounter++;
    }
    IoTHubMessage_Destroy(eventInstance->messageHandle);
}

void ConnectionStatusCallback(IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason, void *userContextCallback)
{
    printf("\n\nConnection Status result:%s, Connection Status reason: %s\n\n", ENUM_TO_STRING(IOTHUB_CLIENT_CONNECTION_STATUS, result),
           ENUM_TO_STRING(IOTHUB_CLIENT_CONNECTION_STATUS_REASON, reason));
}

void iothub_devicewin_mqtt(void)
{
    IOTHUB_CLIENT_TRANSPORT_PROVIDER protocol = MQTT_Protocol;
    IOTHUB_DEVICE_CLIENT_LL_HANDLE iotHubClientHandle;
    EVENT_INSTANCE message;

    callbackCounter = 0;

    if (platform_init() != 0)
    {
        printf("Failed to initialize the platform\r\n");
        goto exit;
    }

    if ((iotHubClientHandle = IoTHubDeviceClient_LL_CreateFromConnectionString(connectionString, protocol)) == NULL)
    {
        printf("Failed to initialize the iothub client!\r\n");
        goto exit;
    }

    Device device;
    memset(&device, 0, sizeof(Device));
    device.reported.connectivity.type = "wifi";
    device.desired.tx_interval = TX_INTERVAL_SECOND;

    char *reportedProperties = serializeTwinToJSON(&device);
    if (reportedProperties == NULL)
    {
        printf("Failed serializing properties to JSON\r\n");
        goto exit;
    }

    bool traceOn = true;
    IoTHubDeviceClient_LL_SetOption(iotHubClientHandle, OPTION_LOG_TRACE, &traceOn);
    IoTHubDeviceClient_LL_SetConnectionStatusCallback(iotHubClientHandle, ConnectionStatusCallback, NULL);

    IoTHubDeviceClient_LL_SendReportedState(iotHubClientHandle, (const unsigned char *)reportedProperties, strlen(reportedProperties), ReportedStateCallback, NULL);
    IoTHubDeviceClient_LL_SetDeviceTwinCallback(iotHubClientHandle, DeviceTwinCallback, &device);

    int iterator = 0;
    time_t sent_time = 0;
    time_t current_time = 0;

    while (1)
    {
        time(&current_time);

        if (difftime(current_time, sent_time) > device.desired.tx_interval)
        {
            float temperature = 30.0;
            sprintf_s(msgText, sizeof(msgText), "{\"temperature\":%f}", temperature);

            if ((message.messageHandle = IoTHubMessage_CreateFromByteArray((const unsigned char *)msgText, strlen(msgText))) == NULL)
            {
                printf("ERROR: iotHubMessageHandle is NULL!\r\n");
            }
            else
            {
                message.messageTrackingId = iterator;

                if (IoTHubDeviceClient_LL_SendEventAsync(iotHubClientHandle, message.messageHandle, SendConfirmationCallback, &message) != IOTHUB_CLIENT_OK)
                {
                    (void)printf("ERROR: IoTHubDeviceClient_LL_SendEventAsync..........FAILED!\r\n");
                }
                else
                {
                    time(&sent_time);
                    (void)printf("IoTHubDeviceClient_LL_SendEventAsync accepted message [%d] for transmission to IoT Hub.\r\n", (int)iterator);
                }
            }

            iterator++;
        }

        IoTHubDeviceClient_LL_DoWork(iotHubClientHandle);
        ThreadAPI_Sleep(10);
    }

    free(reportedProperties);

exit:
    platform_deinit();
}