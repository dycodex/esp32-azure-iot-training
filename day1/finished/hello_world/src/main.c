
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main()
{
    while (1)
    {
        printf("Hello World\r\n");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
