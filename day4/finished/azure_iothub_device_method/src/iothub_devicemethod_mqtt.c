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

typedef enum MCUSTOM_ESP32_STOP_REASON_TAG
{
    CUSTOM_ESP32_STOP_REASON_REBOOT = 0,
    CUSTOM_ESP32_STOP_REASON_UNKNOWN = -1,
} CustomESP32StopReason;

static int callbackCounter;
static char msgText[1024];
static bool should_stop = false;
static CustomESP32StopReason stop_reason = CUSTOM_ESP32_STOP_REASON_UNKNOWN;

typedef struct EVENT_INSTANCE_TAG
{
    IOTHUB_MESSAGE_HANDLE messageHandle;
    size_t messageTrackingId; // For tracking the messages within the user callback.
} EVENT_INSTANCE;

static int DeviceMethodCallback(const char *method_name, const unsigned char *payload, size_t size, unsigned char **response, size_t *response_size, void *context)
{
    printf("received method invocation. method name: %s\r\n", method_name);

    int result;
    if (strcmp("reboot", method_name) == 0)
    {
        // handle reboot invoid
        const char method_response[] = "{\"Response\": \"OK\"}";
        *response_size = sizeof(method_response) - 1;
        *response = malloc(*response_size);

        if (*response != NULL)
        {
            memcpy(*response, method_response, *response_size);
            result = 200;

            should_stop = true;
            stop_reason = CUSTOM_ESP32_STOP_REASON_REBOOT;
        }
        else
        {
            result = -1;
        }
    }
    else
    {
        const char method_response[] = "{\"Response\": \"METHOD_UNKNOWN\"}";
        *response_size = sizeof(method_response) - 1;
        *response = malloc(*response_size);

        if (*response != NULL)
        {
            memcpy(*response, method_response, *response_size);
        }

        result = -1;
    }

    return result;
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

void iothub_devicemethod_mqtt(void)
{
    IOTHUB_CLIENT_TRANSPORT_PROVIDER protocol = MQTT_Protocol;
    IOTHUB_DEVICE_CLIENT_LL_HANDLE iotHubClientHandle;
    EVENT_INSTANCE message;

    srand((unsigned int)time(NULL));
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

    bool traceOn = true;
    IoTHubDeviceClient_LL_SetOption(iotHubClientHandle, OPTION_LOG_TRACE, &traceOn);
    IoTHubDeviceClient_LL_SetConnectionStatusCallback(iotHubClientHandle, ConnectionStatusCallback, NULL);
    IoTHubDeviceClient_LL_SetDeviceMethodCallback(iotHubClientHandle, DeviceMethodCallback, NULL);

    int iterator = 0;
    time_t sent_time = 0;
    time_t current_time = 0;

    while (!should_stop)
    {
        time(&current_time);

        if (difftime(current_time, sent_time) > TX_INTERVAL_SECOND)
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

    for (int i = 0; i < 3; i++)
    {
        IoTHubDeviceClient_LL_DoWork(iotHubClientHandle);
        ThreadAPI_Sleep(10);
    }

    IoTHubDeviceClient_LL_Destroy(iotHubClientHandle);

    if (stop_reason == CUSTOM_ESP32_STOP_REASON_REBOOT)
    {
        platform_deinit();
        esp_restart();
    }

exit:
    platform_deinit();
}