/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/* Stand-in for the Pico SDK's hardware/irq.h: declarations only. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef unsigned int uint;
typedef void (*irq_handler_t)(void);
#define PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY 0x80
void irq_add_shared_handler(uint, irq_handler_t, uint8_t);
void irq_remove_handler(uint, irq_handler_t);
void irq_set_enabled(uint, bool);
