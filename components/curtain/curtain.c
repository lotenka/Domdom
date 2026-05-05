#include "curtain.h"
#include "driver/ledc.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SERVO_PIN      13

#define LEDC_CHANNEL   LEDC_CHANNEL_1
#define LEDC_TIMER     LEDC_TIMER_1
#define LEDC_MODE      LEDC_LOW_SPEED_MODE
#define LEDC_FREQ      50
#define LEDC_RES       LEDC_TIMER_14_BIT

static uint32_t current_angle = 0;

// преобразование угла → duty
static uint32_t angle_to_duty(uint32_t angle)
{
    // 0.5ms–2.5ms из периода 20ms
    // duty = (pulse_width / period) * 2^16

    float min_us = 500.0;
    float max_us = 2500.0;

    float pulse = min_us + (angle / 180.0) * (max_us - min_us);

    uint32_t duty = (pulse / 20000.0) * 65535;

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

// плавное движение
void curtain_set_angle(uint32_t target)
{
    if (target > 180) target = 180;

    while (current_angle != target)
    {
        if (current_angle < target)
            current_angle++;
        else
            current_angle--;

        uint32_t duty = angle_to_duty(current_angle);

        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);

        vTaskDelay(pdMS_TO_TICKS(20)); // плавность
    }

    printf("Curtain angle: %ld\n", current_angle);
}

uint32_t curtain_get_angle(void)
{
    return current_angle;
}