#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "dht.h"
#include "curtain.h"

#include "mqtt_client.h"

#include "wifi_manager.h"

#include "light.h"

static const char *TAG = "example";

#define DHT_TYPE DHT_TYPE_AM2301
#define DHT_DATA_GPIO 4

#define RELAY_PIN 5             // GPIO для реле (котел)
#define AC_PIN 14
#define HUMIDIFIER_PIN 12   // GPIO увлажнитель


#define MQTT_BROKER "mqtt://broker.hivemq.com"


int sensorPin = 7;
volatile int state = 0;         // ОБЩАЯ переменная. Меняется в vRequeest
volatile int ledState = 0;
int auto_mode = 1; // 1 = по датчику, 0 = вручную LEDC

// --- Климат ---
#define AC_ON_TEMP     25.0
#define AC_OFF_TEMP    23.0

#define HEAT_ON_TEMP   21.0
#define HEAT_OFF_TEMP  23.0

float g_temperature = 0;

// режим:
// 0 = manual
// 1 = auto
int climate_mode = 0;

// состояние:
// 0 = idle
// 1 = heating
// 2 = cooling
int climate_state = 0;

//Увлажнитель
float g_humidity = 0;

int humidifier_state = 0; // 0 = OFF, 1 = ON

#define HUM_LOW  40.0
#define HUM_HIGH 50.0

#define SWITCH_DELAY_MS 10000  // 10 сек защита от дребезга

TickType_t last_switch_time = 0;


static bool mqtt_connected = false;

static esp_mqtt_client_handle_t client;

void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                        int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id)
    {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI("MQTT", "Connected");
            esp_mqtt_client_subscribe(client, "home/living/light/set", 0);
            esp_mqtt_client_subscribe(client, "home/living/ac/set", 0);
            esp_mqtt_client_subscribe(client, "home/bedroom/humidifier/set", 0);
            esp_mqtt_client_subscribe(client, "home/kitchen/curtain/set", 0);
            esp_mqtt_client_subscribe(client, "home/boiler/set", 0);
            esp_mqtt_client_subscribe(client, "home/living/light/mode/set", 0);
            esp_mqtt_client_subscribe(client, "home/living/climate/mode/set", 0);

            mqtt_connected = true;
            break;

        case MQTT_EVENT_DATA:
        {
            char topic[64];
            char data[64];

            snprintf(topic, event->topic_len + 1, "%s", event->topic);
            snprintf(data, event->data_len + 1, "%s", event->data);

            ESP_LOGI("MQTT", "Topic: %s Data: %s", topic, data);

            // КОТЕЛ (весь дом)
            if (strcmp(topic, "home/boiler/set") == 0)
            {
                int val = atoi(data);
                gpio_set_level(RELAY_PIN, val);

                if (mqtt_connected)
                {
                    char msg[10];
                    snprintf(msg, sizeof(msg), "%d", val);
                    esp_mqtt_client_publish(client, "home/boiler/state", msg, 0, 1, 0);
                }
            }

            // КОНДИЦИОНЕР (гостиная)
            else if (strcmp(topic, "home/living/ac/set") == 0)
            {

                int val = atoi(data);
                gpio_set_level(AC_PIN, val);

                if (mqtt_connected)
                {
                    char msg[10];
                    snprintf(msg, sizeof(msg), "%d", val);
                    esp_mqtt_client_publish(client, "home/living/ac/state", msg, 0, 1, 0);
                }
            }

            // режим климата
            else if (strcmp(topic, "home/living/climate/mode/set") == 0)
            {
                climate_mode = atoi(data);
                printf("Climate mode: %d\n", climate_mode);
            }
            // УВЛАЖНИТЕЛЬ (спальня)
            else if (strcmp(topic, "home/bedroom/humidifier/set") == 0)
            {
                int val = atoi(data);
                gpio_set_level(HUMIDIFIER_PIN, val);
                humidifier_state = val;
            }

            // СВЕТ (гостиная)
            else if (strcmp(topic, "home/living/light/set") == 0)
            {
                int val = atoi(data);

                if (val < 0) val = 0;
                if (val > 255) val = 255;

                set_brightness(val);

                if (mqtt_connected)
                {
                    char msg[10];
                    snprintf(msg, sizeof(msg), "%d", val);
                    esp_mqtt_client_publish(client, "home/living/light/state", msg, 0, 1, 0);
                }
            }

            // РЕЖИМ СВЕТА (auto/manual)
            else if (strcmp(topic, "home/living/light/mode/set") == 0)
            {
                auto_mode = atoi(data);
                printf("Light mode: %d\n", auto_mode);
            }

            // ЖАЛЮЗИ (кухня)
            else if (strcmp(topic, "home/kitchen/curtain/set") == 0)
            {
                int angle = atoi(data);

                if (angle < 0) angle = 0;
                if (angle > 180) angle = 180;

                curtain_set_angle(angle);

                if (mqtt_connected)
                {
                    char msg[10];
                    snprintf(msg, sizeof(msg), "%d", angle);
                    esp_mqtt_client_publish(client, "home/kitchen/curtain/state", msg, 0, 1, 0);
                }
            }
            break;
        }
        case MQTT_EVENT_DISCONNECTED:
            mqtt_connected = false;
            break;

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
    // котёл
    gpio_set_direction(RELAY_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(RELAY_PIN, 0);

    // кондиционер
    gpio_set_direction(AC_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(AC_PIN, 0);

    // PIR
    gpio_set_direction(sensorPin, GPIO_MODE_INPUT);

    // увлажнитель
    gpio_set_direction(HUMIDIFIER_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(HUMIDIFIER_PIN, 0);
}

void vDHT_read(void *pvParameters)
{
    float temperature, humidity;
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(50));
        if (dht_read_float_data(DHT_TYPE, DHT_DATA_GPIO, &humidity, &temperature) == ESP_OK)
        {
            g_temperature = temperature;
            g_humidity = humidity;
            
            //Публикация данных mqtt
            static TickType_t last_pub = 0;

            if (mqtt_connected && (xTaskGetTickCount() - last_pub > pdMS_TO_TICKS(3000)))
            {
                last_pub = xTaskGetTickCount();

                char msg[50];

                snprintf(msg, sizeof(msg), "%.1f", temperature);
                esp_mqtt_client_publish(client, "home/living/temperature", msg, 0, 1, 0);

                snprintf(msg, sizeof(msg), "%.1f", humidity);
                esp_mqtt_client_publish(client, "home/living/humidity", msg, 0, 1, 0);
            }


            printf("Humidity: %.1f%% Temp: %.1fC\n", humidity, temperature);
        }
        else
        {
            printf("Could not read data from sensor\n");
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void vRequest(void *pvParameters)
{
    int last_state = -1;

    while (1)
    {
        state = gpio_get_level(sensorPin);

        if (state != last_state)
        {
            last_state = state;

            if (mqtt_connected)
            {
                char motion_msg[10];
                snprintf(motion_msg, sizeof(motion_msg), "%d", state);
                esp_mqtt_client_publish(client, "home/living/motion", motion_msg, 0, 1, 0);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}


//Управление котлом и кондиционером
void vClimate(void *pvParameters)
{
    while (1)
    {
        // --- AUTO режим ---
        if (climate_mode == 1)
        {
            switch (climate_state)
            {
                case 0: // IDLE
                    if (g_temperature >= AC_ON_TEMP)
                    {
                        climate_state = 2;
                        printf("AUTO -> AC ON\n");
                    }
                    else if (g_temperature <= HEAT_ON_TEMP)
                    {
                        climate_state = 1;
                        printf("AUTO -> HEAT ON\n");
                    }
                    break;

                case 1: // HEATING
                    if (g_temperature >= HEAT_OFF_TEMP)
                    {
                        climate_state = 0;
                        printf("AUTO -> IDLE\n");
                    }
                    break;

                case 2: // COOLING
                    if (g_temperature <= AC_OFF_TEMP)
                    {
                        climate_state = 0;
                        printf("AUTO -> IDLE\n");
                    }
                    break;
            }   
        

            // --- Управление выходами (всегда одно место) ---
            if (climate_state == 1)
            {
                gpio_set_level(RELAY_PIN, 1);
                gpio_set_level(AC_PIN, 0);
            }
            else if (climate_state == 2)
            {
                gpio_set_level(RELAY_PIN, 0);
                gpio_set_level(AC_PIN, 1);
            }
            else
            {
                gpio_set_level(RELAY_PIN, 0);
                gpio_set_level(AC_PIN, 0);
            }
        }
        // --- Отправка состояния ---
        if (mqtt_connected)
        {
            char msg[10];
            snprintf(msg, sizeof(msg), "%d", climate_state);
            esp_mqtt_client_publish(client, "home/living/climate/state", msg, 0, 1, 0);
            esp_mqtt_client_publish(client, "home/living/climate/mode/state", 
                        climate_mode ? "1" : "0", 0, 1, 0);


            // кондиционер (0 или 1)
            int ac_state = (climate_state == 2) ? 1 : 0;
            snprintf(msg, sizeof(msg), "%d", ac_state);
            esp_mqtt_client_publish(client, "home/living/ac/state", msg, 0, 1, 0);

            // котёл (0 или 1)
            int boiler_state = (climate_state == 1) ? 1 : 0;
            snprintf(msg, sizeof(msg), "%d", boiler_state);
            esp_mqtt_client_publish(client, "home/boiler/state", msg, 0, 1, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}


void vHumidifier(void *pvParameters)    //Увлажнитель
{
    while (1)
    {
        TickType_t now = xTaskGetTickCount();

        // --- ВКЛ ---
        if (g_humidity < HUM_LOW && humidifier_state == 0)
        {
            if (now - last_switch_time > pdMS_TO_TICKS(SWITCH_DELAY_MS))
            {
                gpio_set_level(HUMIDIFIER_PIN, 1);
                humidifier_state = 1;
                last_switch_time = now;

                printf("Humidifier ON (%.1f%%)\n", g_humidity);
            }
        }

        // --- ВЫКЛ ---
        else if (g_humidity > HUM_HIGH && humidifier_state == 1)
        {
            if (now - last_switch_time > pdMS_TO_TICKS(SWITCH_DELAY_MS))
            {
                gpio_set_level(HUMIDIFIER_PIN, 0);
                humidifier_state = 0;
                last_switch_time = now;

                printf("Humidifier OFF (%.1f%%)\n", g_humidity);
            }
        }

        if (mqtt_connected)
        {
            char msg[10];
            snprintf(msg, sizeof(msg), "%d", humidifier_state);
            esp_mqtt_client_publish(client, "home/humidifier/state", msg, 0, 1, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


void vLight(void *pvParameters)
{
    int last_state = 0;

    while (1)
    {
        if (auto_mode)
        {
            if (mqtt_connected)
            {
                esp_mqtt_client_publish(client, "home/living/light/mode/state", 
                        auto_mode ? "1" : "0", 0, 1, 0);
            }
            // Ловим ФРОНТ (0 -> 1)
            if (state == 1 && last_state == 0)
            {
                // переключаем состояние
                static int light_on = 0;
                light_on = !light_on;

                if (light_on)
                {
                    set_brightness(200);
                    printf("TOGGLE → Light ON\n");
                }
                else
                {
                    set_brightness(0);
                    printf("TOGGLE → Light OFF\n");
                }
            }
        }

        last_state = state;

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    setup();
    wifi_init("RT-GPON-AB1D", "fD2JAWsV");
    mqtt_app_start();
    light_init();
    curtain_init();     //сервопривод SG90 5V
    //0 → закрыто
    //90 → наполовину
    //180 → открыто

    climate_mode = 0; // стартуем в manual
    vTaskDelay(pdMS_TO_TICKS(100));
    xTaskCreate(vRequest, "Request", 4096, NULL, 2, NULL);
    xTaskCreate(vLight,   "Light",   4096, NULL, 1, NULL);
    xTaskCreate(vDHT_read, "DHT_read", configMINIMAL_STACK_SIZE * 3, NULL, 2, NULL);
    xTaskCreate(vClimate, "Climate", 2048, NULL, 2, NULL);
    xTaskCreate(vHumidifier, "Humidifier", 2048, NULL, 2, NULL);
}
