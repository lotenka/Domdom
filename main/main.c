#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "dht.h"

static const char *TAG = "example";


#define DHT_TYPE DHT_TYPE_AM2301
#define DHT_DATA_GPIO 11


int ledPin = 10;
int sensorPin = 9;
volatile int state = 0;   // ОБЩАЯ переменная. Меняется в vRequeest
volatile int ledState = 0;

void setup()
{
    gpio_set_direction(ledPin, GPIO_MODE_OUTPUT);
    gpio_set_direction(sensorPin, GPIO_MODE_INPUT);
}

void vDHT_read(void *pvParameters)
{
    float temperature, humidity;
    while(1)
    {
        if (dht_read_float_data(DHT_TYPE, DHT_DATA_GPIO, &humidity, &temperature) == ESP_OK)
        {
            printf("Humidity: %.1f%% Temp: %.1fC\n", humidity, temperature);
        }
        else
        {
            printf("Could not read data from sensor\n");
        }
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

void vRequest(void *pvParameters)
{
    while (1)
    {
        state = gpio_get_level(sensorPin);
        if (state == 1)
        {
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

void vLight(void *pvParameters)
{
    while (1)
    {
        if (state == 1)
        {
            ledState = !ledState;
            gpio_set_level(ledPin, ledState);
            if (ledState == 1)
            {
                printf("Lights on!\n");
            }
            else
            {
                printf("Lights off!\n");
            }
            vTaskDelay(pdMS_TO_TICKS(3000));
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(700));
        }
    }
}

void app_main(void)
{
    setup();

    xTaskCreate(vRequest, "Request", 2048, NULL, 2, NULL);
    xTaskCreate(vLight,   "Light",   2048, NULL, 1, NULL);
    xTaskCreate(vDHT_read, "DHT_read", configMINIMAL_STACK_SIZE * 3, NULL, 2, NULL);
}
