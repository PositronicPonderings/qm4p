/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    test_setup.h
 * @brief   Shared hardware setup for the test programs (M2 onward).
 *
 * Every test needs the same bus and the same two screens, so that setup
 * lives here once. The wiring, the board choice and the panel settings are
 * in test_board.h: change them there, and every test follows.
 */
#ifndef TEST_SETUP_H
#define TEST_SETUP_H

#include "qg4p.h"

extern qg_bus_t    bus;
extern qg_screen_t scr_a;       /* 2.0" ST7789                              */
extern qg_screen_t scr_b;       /* 3.5" ST7796S or 2.8" ILI9341             */

/**
 * Start USB serial (waiting up to 2 s for a terminal), bring up the bus and
 * both screens, and print the test's opening lines in the test_log.h
 * format: what it covers, then each screen.
 *
 *     test_setup(TEST_TAG, "qg4p_test_m2: lines, boxes, circles, ...");
 *
 * prints
 *
 *     [M2] qg4p_test_m2: lines, boxes, circles, ...
 *     [M2] Screen A: ST7789   240 x 320  SPI 37500000 Hz  DIRECT
 *     [M2] Screen B: ST7796S  320 x 480  SPI 37500000 Hz  DIRECT
 *
 * Halts with a message if anything fails.
 */
void test_setup(const char *tag, const char *about);

/**
 * Like test_setup(), but makes screen B a framebuffer (BUF8)
 * screen using the buffer you pass in (at least width x height bytes).
 * Pass NULL for an ordinary DIRECT screen B.
 */
void test_setup_ex(const char *tag, const char *about, uint8_t *fb_b, uint32_t fb_b_size);

#endif /* TEST_SETUP_H */
