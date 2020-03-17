
#include "esp_log.h"
#include "iot_mpu6050.h"

#define I2C_MASTER_SCL_IO 26        /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO 25        /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM I2C_NUM_1    /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE 0 /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0 /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ 100000   /*!< I2C master clock frequency */

static i2c_bus_handle_t i2c_bus = NULL;
static mpu6050_handle_t mpu6050 = NULL;

static const char *TAG = "MPU6050";

void i2c_sensor_mpu6050_init()
{
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_bus = iot_i2c_bus_create(i2c_master_port, &conf);
    mpu6050 = iot_mpu6050_create(i2c_bus, MPU6050_I2C_ADDRESS);
}

void mpu6050_test_task(void *pvParameters)
{
    uint8_t mpu6050_deviceid;
    mpu6050_acce_value_t acce;
    mpu6050_gyro_value_t gyro;

    iot_mpu6050_get_deviceid(mpu6050, &mpu6050_deviceid);
    ESP_LOGI(TAG, "Device ID: 0x%02x\n", mpu6050_deviceid);

    iot_mpu6050_wake_up(mpu6050);
    iot_mpu6050_set_acce_fs(mpu6050, ACCE_FS_4G);
    iot_mpu6050_set_gyro_fs(mpu6050, GYRO_FS_500DPS);

    while (1)
    {
        iot_mpu6050_get_acce(mpu6050, &acce);
        ESP_LOGI(TAG, "acce_x:%.2f, acce_y:%.2f, acce_z:%.2f", acce.acce_x, acce.acce_y, acce.acce_z);
        iot_mpu6050_get_gyro(mpu6050, &gyro);
        ESP_LOGI(TAG, "gyro_x:%.2f, gyro_y:%.2f, gyro_z:%.2f", gyro.gyro_x, gyro.gyro_y, gyro.gyro_z);
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}

void app_main()
{
    i2c_sensor_mpu6050_init();
    vTaskDelay(1000 / portTICK_RATE_MS);
    xTaskCreate(mpu6050_test_task, "mpu6050_test_task", 1024 * 2, NULL, 10, NULL);
}
