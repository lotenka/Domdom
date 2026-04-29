#include "light.h"
#include "driver/ledc.h"
#include <stdio.h>

#define LED_PIN       8
#define LEDC_CHANNEL  LEDC_CHANNEL_0
#define LEDC_TIMER    LEDC_TIMER_0
#define LEDC_MODE     LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES LEDC_TIMER_8_BIT
#define LEDC_FREQ     5000

static uint32_t brightness = 0;

void light_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = LEDC_FREQ,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t channel = {
        .gpio_num = LED_PIN,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .timer_sel = LEDC_TIMER,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&channel);
}

void set_brightness(uint32_t duty)
{
    if (duty > 255) duty = 255;

    brightness = duty;

    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, brightness);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);

    printf("Brightness: %ld\n", brightness);
}

uint32_t get_brightness(void)
{
    return brightness;
}