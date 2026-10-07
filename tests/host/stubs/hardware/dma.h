/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef unsigned int uint;
typedef struct { uint32_t ctrl; } dma_channel_config;
enum dma_channel_transfer_size { DMA_SIZE_8, DMA_SIZE_16, DMA_SIZE_32 };
dma_channel_config dma_channel_get_default_config(uint);
void channel_config_set_transfer_data_size(dma_channel_config*, enum dma_channel_transfer_size);
void channel_config_set_dreq(dma_channel_config*, uint);
void channel_config_set_read_increment(dma_channel_config*, bool);
void channel_config_set_write_increment(dma_channel_config*, bool);
void dma_channel_configure(uint, const dma_channel_config*, volatile void*, const volatile void*, uint32_t, bool);
void dma_channel_wait_for_finish_blocking(uint);
int dma_claim_unused_channel(bool);
/* For qs4p (QuickSound) */
void dma_channel_unclaim(uint);
int dma_claim_unused_timer(bool);
void dma_timer_unclaim(uint);
void dma_timer_set_fraction(uint, uint16_t, uint16_t);
uint dma_get_timer_dreq(uint);
void dma_channel_set_read_addr(uint, const volatile void*, bool);
void dma_channel_set_trans_count(uint, uint32_t, bool);
void dma_channel_abort(uint);
void dma_irqn_set_channel_enabled(uint, uint, bool);
bool dma_irqn_get_channel_status(uint, uint);
void dma_irqn_acknowledge_channel(uint, uint);
int dma_get_irq_num(uint);
