/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef unsigned int uint;
typedef struct { uint32_t csr, div, top; } pwm_config;
pwm_config pwm_get_default_config(void);
void pwm_config_set_clkdiv_int(pwm_config*, uint);
void pwm_config_set_wrap(pwm_config*, uint16_t);
void pwm_init(uint, pwm_config*, bool);
uint pwm_gpio_to_slice_num(uint);
void pwm_set_gpio_level(uint, uint16_t);
/* For qs4p (QuickSound) */
enum pwm_chan { PWM_CHAN_A = 0, PWM_CHAN_B = 1 };
uint pwm_gpio_to_channel(uint);
void pwm_set_chan_level(uint, uint, uint16_t);
void pwm_set_enabled(uint, bool);
typedef struct { volatile uint32_t csr, div, ctr, cc, top; } pwm_slice_hw_t;
typedef struct { pwm_slice_hw_t slice[12]; } pwm_hw_t;
extern pwm_hw_t *pwm_hw;
