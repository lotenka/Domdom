#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "example";

/* Use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
#define BLINK_GPIO CONFIG_BLINK_GPIO

#include "freertos/task.h"
#include "driver/gpio.h"
#include <stdio.h>

int ledPin = 10;
int sensorPin = 9;

volatile int state = 0;   // ОБЩАЯ переменная
volatile int ledState = 0;

void setup()
{
    gpio_set_direction(ledPin, GPIO_MODE_OUTPUT);
    gpio_set_direction(sensorPin, GPIO_MODE_INPUT);
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
}
