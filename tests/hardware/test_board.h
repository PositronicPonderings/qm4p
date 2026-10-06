/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    test_board.h
 * @brief   The hardware test programs' wiring and panel settings, in one place.
 *
 * Every hardware test reads its numbers from here: the pins, the SPI
 * speeds, and each screen's panel settings. test_setup.c (used by M2
 * onward) builds both screens from them, and test_m0.c and test_m1.c, which
 * spell out the setup step by step on purpose, use the same numbers. Change
 * something here and every test follows.
 *
 * (The examples have their own settings file, examples/board.h, so they
 * can be copied out of this repository without the tests.)
 *
 * The values are the development board's, confirmed on hardware in M1:
 * see docs/WIRING.md for the wiring and for how each panel setting was
 * found.
 */
#ifndef TEST_BOARD_H
#define TEST_BOARD_H

#include "qg4p.h"

/* ========================================================================== */
/*  Which board is screen B?                                                  */
/* ========================================================================== */
/* Screen B can be either of two boards. They share every pin, so swapping
 * one for the other is just this one line (and a rebuild).                  */
#define SCREEN_B_ILI9341  1   /* 2.8" red board,  240 x 320                  */
#define SCREEN_B_ST7796   2   /* 3.5" blue board, 320 x 480                  */

#define SCREEN_B_BOARD    SCREEN_B_ST7796

/* ========================================================================== */
/*  Wiring (see docs/WIRING.md; the colours are the dev board's jumpers)      */
/* ========================================================================== */
/* The shared SPI bus: every screen is on these four wires.                  */
#define PIN_SCK    18   /* SPI0 SCK -> each board's SCL/SCK      YELLOW      */
#define PIN_MOSI   19   /* SPI0 TX  -> each board's SDA/SDI      ORANGE      */
#define PIN_DC     20   /* data/command, shared                  BLUE        */
#define PIN_RST    21   /* reset, shared                         WHITE       */

/* One chip-select and one backlight per screen.                             */
#define PIN_CS_A   17   /* screen A's CS                         GREEN       */
#define PIN_CS_B   22   /* screen B's CS                         GREEN       */
#define PIN_BL_A   16   /* screen A's backlight (PWM)            PURPLE      */
#define PIN_BL_B   15   /* screen B's backlight (PWM), pin 20    PURPLE      */

/* ========================================================================== */
/*  SPI speeds                                                                */
/* ========================================================================== */
/* 40 MHz asks for the fastest "safe" rate; the Pico 2 delivers 37.5 MHz.
 * Each screen has its own speed, and the bus switches between them. On long
 * breadboard jumpers, try 20000000u if you see speckles or shifted pixels.  */
#define A_SPI_HZ   40000000u
#define B_SPI_HZ   40000000u

/* ========================================================================== */
/*  Panel settings                                                            */
/* ========================================================================== */
/* What each setting fixes (test_m0.c and test_m1.c show how to find them):
 *   INVERT     the first colour shows WHITE instead of BLACK -> flip it
 *   BGR        RED shows as BLUE and BLUE as RED             -> flip it
 *   MIRROR_X   the picture is mirrored left/right            -> flip it
 *   MIRROR_Y   the picture is mirrored top/bottom            -> flip it   */

/* Screen A: the 2.0" ST7789 (GMT020-02 board). */
#define A_DRIVER    QG_DRIVER_ST7789
#define A_WIDTH     240
#define A_HEIGHT    320
#define A_INVERT    true
#define A_BGR       false
#define A_MIRROR_X  false
#define A_MIRROR_Y  false

#if SCREEN_B_BOARD == SCREEN_B_ILI9341
  /* Screen B: the 2.8" ILI9341. Settings from the working proof of concept
   * (MADCTL 0x48, no inversion), confirmed on hardware in M1.              */
  #define B_DRIVER    QG_DRIVER_ILI9341
  #define B_WIDTH     240
  #define B_HEIGHT    320
  #define B_INVERT    false
  #define B_BGR       true
  #define B_MIRROR_X  true
  #define B_MIRROR_Y  false
#elif SCREEN_B_BOARD == SCREEN_B_ST7796
  /* Screen B: the 3.5" ST7796S, confirmed on hardware in M1. (The same
   * flags as the ILI9341, as it happens; only the chip and size differ.)    */
  #define B_DRIVER    QG_DRIVER_ST7796
  #define B_WIDTH     320
  #define B_HEIGHT    480
  #define B_INVERT    false
  #define B_BGR       true
  #define B_MIRROR_X  true
  #define B_MIRROR_Y  false
#else
  #error "SCREEN_B_BOARD must be SCREEN_B_ILI9341 or SCREEN_B_ST7796"
#endif

#endif /* TEST_BOARD_H */
