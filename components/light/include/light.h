#pragma once
#include <stdint.h>

void light_init(void);
void set_brightness(uint32_t duty);
uint32_t get_brightness(void);