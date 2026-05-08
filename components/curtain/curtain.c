#include "curtain.h"

#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SERVO_PIN      9

#define LEDC_TIMER     LEDC_TIMER_1
#define LEDC_MODE      LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL   LEDC_CHANNEL_1

#define LEDC_FREQ      50
#define LEDC_RES       LEDC_TIMER_14_BIT

#define SERVO_MIN_US   500
#define SERVO_MAX_US   2500

static uint32_t current_angle = 0;

static uint32_t angle_to_duty(uint32_t angle)
{
    uint32_t us = SERVO_MIN_US +
                  ((SERVO_MAX_US - SERVO_MIN_US) * angle / 180);

    uint32_t duty = (us * ((1 << 14) - 1)) / 20000;

    return duty;
}

void curtain_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_RES,
        .freq_hz = LEDC_FREQ,
        .clk_cfg = LEDC_AUTO_CLK
    };

    ledc_timer_config(&timer);

    ledc_channel_config_t channel = {
        .gpio_num = SERVO_PIN,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .timer_sel = LEDC_TIMER,
        .duty = 0,
        .hpoint = 0
    };

    ledc_channel_config(&channel);
}

void curtain_set_angle(uint32_t angle)
{
    if (angle > 180)
        angle = 180;

    current_angle = angle;

    uint32_t duty = angle_to_duty(angle);

    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);

    printf("Curtain angle: %ld\n", angle);
}

uint32_t curtain_get_angle(void)
{
    return current_angle;
}