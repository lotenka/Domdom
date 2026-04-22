#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "dht.h"

#include "mqtt_client.h"

#include "wifi_manager.h"



static const char *TAG = "example";

#define DHT_TYPE DHT_TYPE_AM2301
#define DHT_DATA_GPIO 11

#define RELAY_PIN 5             // GPIO для реле
#define AC_PIN 6

#define MQTT_BROKER "mqtt://broker.hivemq.com"



int ledPin = 10;
int sensorPin = 9;
volatile int state = 0;         // ОБЩАЯ переменная. Меняется в vRequeest
volatile int ledState = 0;
float g_temperature = 0;        //Температура



static esp_mqtt_client_handle_t client;

void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                        int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id)
    {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI("MQTT", "Connected");
            esp_mqtt_client_subscribe(client, "home/boiler/set", 0);
            esp_mqtt_client_subscribe(client, "home/ac/set", 0);
            break;

        case MQTT_EVENT_DATA:
        {
            char topic[64];
            char data[64];

            snprintf(topic, event->topic_len + 1, "%s", event->topic);
            snprintf(data, event->data_len + 1, "%s", event->data);

            ESP_LOGI("MQTT", "Topic: %s Data: %s", topic, data);

            if (strcmp(topic, "home/boiler/set") == 0)
            {
                gpio_set_level(RELAY_PIN, atoi(data));
            }
            else if (strcmp(topic, "home/ac/set") == 0)
            {
                gpio_set_level(AC_PIN, atoi(data));
            }
            break;
        }

        default:
            break;
    }
}

void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}


void setup()
{
    gpio_set_direction(RELAY_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(RELAY_PIN, 0); // котёл выкл
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
            g_temperature = temperature;

            
            //Публикация данных mqtt
            char msg[50];
            snprintf(msg, sizeof(msg), "%.1f", temperature);
            esp_mqtt_client_publish(client, "home/temperature", msg, 0, 1, 0);

            snprintf(msg, sizeof(msg), "%.1f", humidity);
            esp_mqtt_client_publish(client, "home/humidity", msg, 0, 1, 0);
            


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

        
        //Публикация данных mqtt
        char motion_msg[10];
        snprintf(motion_msg, sizeof(motion_msg), "%d", state);
        esp_mqtt_client_publish(client, "home/motion", motion_msg, 0, 1, 0);
        
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


//Управление котлом и кондиционером
void vClimate(void *pvParameters)
{
    while (1)
    {
        if (g_temperature > 25.0)
        {
            // включить кондиционер
            gpio_set_level(AC_PIN, 1);
            gpio_set_level(RELAY_PIN, 0);
            printf("AC ON\n");
        }
        else if (g_temperature < 22.0)
        {
            // включить котёл
            gpio_set_level(RELAY_PIN, 1);
            gpio_set_level(AC_PIN, 0);
            printf("HEATER ON\n");
        }
        else
        {
            // всё выключено
            gpio_set_level(RELAY_PIN, 0);
            gpio_set_level(AC_PIN, 0);
            printf("IDLE\n");
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void)
{
    setup();
    wifi_init("MyWiFi", "MyPassword");
    mqtt_app_start();
    xTaskCreate(vRequest, "Request", 2048, NULL, 2, NULL);
    xTaskCreate(vLight,   "Light",   2048, NULL, 1, NULL);
    xTaskCreate(vDHT_read, "DHT_read", configMINIMAL_STACK_SIZE * 3, NULL, 2, NULL);
    xTaskCreate(vClimate, "Climate", 2048, NULL, 2, NULL);
}
