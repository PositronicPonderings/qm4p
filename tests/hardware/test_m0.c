/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    test_m0.c
 * @brief   Hardware test M0: bring up screen A, the 2.0" ST7789, on the
 *          shared bus.
 *
 * WHAT IT CHECKS
 *   That the library can start one screen and talk to it: the panel
 *   settings (colour inversion, red/blue order, mirroring), the orientation
 *   and RAM offsets in all four rotations, clipping at an edge, and how fast
 *   a whole screen can be filled. It's the first test to run on a new board.
 *   The setup is spelled out step by step below, on purpose, rather than
 *   hidden in test_setup.c: this is the program to read to see how a
 *   screen comes to life.
 *
 * HARDWARE
 *   Screen A (2.0" ST7789, 240 x 320) on the shared SPI bus, wired as in
 *   test_board.h. Screen B can be connected or not: its CS pin is held high,
 *   so it ignores everything. A USB serial terminal shows the steps.
 *
 * WHAT TO LOOK FOR, STEP BY STEP (then the steps repeat)
 *   1  Colours      Screen A clears to each of the 16 QuickBasic colours,
 *                   1 second each, while serial names it.
 *                     - First colour WHITE, not BLACK?      -> flip A_INVERT
 *                     - RED looks BLUE and BLUE looks RED?  -> flip A_BGR
 *   2  Orientation  In each rotation, for 3 seconds: a 1-pixel white border
 *                   (all four edges must show; a missing edge means the
 *                   panel offsets are wrong) and coloured corner squares:
 *                          RED ........ GREEN
 *                           .   (cyan)   .      <- cyan bar marks the TOP
 *                           .            .
 *                          BLUE ....... YELLOW
 *                   Corners swapped left/right: flip A_MIRROR_X; swapped
 *                   top/bottom: flip A_MIRROR_Y. A magenta square hanging
 *                   off the right edge shows only its left half, with
 *                   nothing wrapping round to the left side: clipping works.
 *   3  Speed        32 full-screen clears through the colour cube. Serial
 *                   gives the time per clear: about 33 ms at 37.5 MHz.
 *   The settings to flip (A_INVERT and the rest) are in test_board.h.
 *
 * SERIAL OUTPUT (one line per step, in the format set by test_log.h)
 *   Keep a terminal open: VS Code's serial monitor, or `tio /dev/ttyACM0`,
 *   which reconnects by itself each time the Pico restarts. At start-up the
 *   program waits up to 2 seconds for a terminal to connect, and starts at
 *   once when one does; with none, it starts anyway. The steps loop
 *   forever, so a late terminal sees everything on the next pass.
 *
 * Program: qg4p_test_m0 (build/tests/hardware/qg4p_test_m0.uf2).
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "qg4p.h"
#include "qg_internal.h"   /* test-only: raw rectangle fill until M2's qg_box() */
#include "test_board.h"    /* the pins, SPI speed and panel settings          */
#define TEST_TAG "M0"
#include "test_log.h"      /* the serial line format                          */

static qg_bus_t    bus;
static qg_screen_t scr_a;

static const char *const rot_names[4] = { "0", "90", "180", "270" };

/* -------------------------------------------------------------------------- */
static void test_named_colors(void)
{
    TEST_STEP(1, 3, "Colours", "the 16 named colours, 1 s each", "BLACK first, RED really red");
    for (qg_color_t c = QG_BLACK; c <= QG_WHITE; c++) {
        TEST_DETAIL("%2u  %s", (unsigned)c, qg_color_name(c));
        qg_cls(&scr_a, c);
        sleep_ms(1000);
    }
}

/* -------------------------------------------------------------------------- */
static void draw_orientation_pattern(qg_screen_t *s)
{
    const int16_t w  = qg_screen_width(s);
    const int16_t h  = qg_screen_height(s);
    const int16_t sq = 30;   /* corner square size */

    qg_cls(s, QG_BLACK);

    /* 1-pixel border: top, bottom, left, right. */
    qg_int_fill_rect(s, 0,     0,     w, 1, QG_WHITE);
    qg_int_fill_rect(s, 0,     h - 1, w, 1, QG_WHITE);
    qg_int_fill_rect(s, 0,     0,     1, h, QG_WHITE);
    qg_int_fill_rect(s, w - 1, 0,     1, h, QG_WHITE);

    /* Corner squares, inset by 2 px so the border stays visible. */
    qg_int_fill_rect(s, 2,          2,          sq, sq, QG_RED);
    qg_int_fill_rect(s, w - sq - 2, 2,          sq, sq, QG_GREEN);
    qg_int_fill_rect(s, 2,          h - sq - 2, sq, sq, QG_BLUE);
    qg_int_fill_rect(s, w - sq - 2, h - sq - 2, sq, sq, QG_YELLOW);

    /* "This way up" marker: a cyan bar centred along the top edge. */
    qg_int_fill_rect(s, w / 2 - 30, 2, 60, 10, QG_CYAN);

    /* Clipping check: this square hangs off the right edge by half. If
     * clipping works, you see only its left half and nothing wraps around
     * to the left side of the screen.                                        */
    qg_int_fill_rect(s, w - 20, h / 2 - 20, 40, 40, QG_LIGHTMAGENTA);
}

static void test_orientation(void)
{
    TEST_STEP(2, 3, "Orientation", "corner pattern, 4 rotations", "RED top-left, all 4 edges white");
    TEST_DETAIL("expect RED top-left, GREEN top-right, BLUE bottom-left, YELLOW bottom-right,");
    TEST_DETAIL("a CYAN bar at the top, and a magenta square cut off at the right edge");

    for (int r = 0; r < 4; r++) {
        qg_screen_set_rotation(&scr_a, (qg_rotation_t)r);
        TEST_DETAIL("rotation %-3s -> %d x %d  (RAM offset x=%u y=%u)",
               rot_names[r],
               qg_screen_width(&scr_a), qg_screen_height(&scr_a),
               (unsigned)scr_a.ram_x_off, (unsigned)scr_a.ram_y_off);
        draw_orientation_pattern(&scr_a);
        sleep_ms(3000);
    }
    qg_screen_set_rotation(&scr_a, QG_ROT_0);
}

/* -------------------------------------------------------------------------- */
static void test_speed(void)
{
    const int frames = 32;

    TEST_STEP(3, 3, "Speed", "32 full-screen clears", "about 33 ms per clear at 37.5 MHz");

    uint64_t t0 = time_us_64();
    for (int i = 0; i < frames; i++) {
        /* Walk through the 216-colour cube (indices 16..231). */
        qg_cls(&scr_a, (qg_color_t)(16 + (i * 7) % 216));
    }
    uint64_t us = time_us_64() - t0;

    unsigned long us_per = (unsigned long)(us / frames);
    TEST_DETAIL("%d clears in %lu us -> %lu.%02lu ms per clear, ~%lu fps",
           frames, (unsigned long)us, us_per / 1000, (us_per % 1000) / 10,
           us_per ? 1000000ul / us_per : 0ul);
}

/* ========================================================================== */
int main(void)
{
    stdio_init_all();   /* USB serial: waits up to 2 s for a terminal to connect
                           (PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS, set in
                           CMakeLists.txt), then carries on without one     */

    printf("\n");
    TEST_LOG("qg4p_test_m0: screen A alone: colours, orientation, fill speed");

    /* --- The shared bus ---------------------------------------------------- */
    static const int8_t all_cs[] = { PIN_CS_A, PIN_CS_B };

    const qg_bus_config_t bus_cfg = {
        .spi      = spi0,
        .sck_pin  = PIN_SCK,
        .mosi_pin = PIN_MOSI,
        .dc_pin   = PIN_DC,
        .rst_pin  = PIN_RST,
        .cs_pins  = all_cs,
        .cs_count = (uint8_t)(sizeof(all_cs) / sizeof(all_cs[0])),
    };

    if (qg_bus_init(&bus, &bus_cfg) != QG_OK) {
        TEST_LOG("Bus init FAILED: check the pins in test_board.h");
        while (true) tight_loop_contents();
    }

    /* --- Screen A: 2.0" ST7789 240x320 -------------------------------- */
    const qg_screen_config_t cfg_a = {
        .driver         = A_DRIVER,
        .cs_pin         = PIN_CS_A,
        .bl_pin         = PIN_BL_A,
        .bl_active_high = true,
        .spi_hz         = A_SPI_HZ,
        .width          = A_WIDTH,
        .height         = A_HEIGHT,
        .x_offset       = 0,
        .y_offset       = 0,
        .bgr            = A_BGR,
        .invert         = A_INVERT,
        .mirror_x       = A_MIRROR_X,
        .mirror_y       = A_MIRROR_Y,
        .rotation       = QG_ROT_0,
        .backend        = QG_BACKEND_DIRECT,
    };

    qg_err_t err = qg_screen_init(&scr_a, &bus, &cfg_a);
    if (err != QG_OK) {
        TEST_LOG("Screen A init FAILED (error %d): check the wiring and test_board.h", (int)err);
        while (true) tight_loop_contents();
    }

    TEST_LOG("Screen A: %-8s %3d x %3d  SPI %8lu Hz  %s", scr_a.drv->name,
             qg_screen_width(&scr_a), qg_screen_height(&scr_a),
             (unsigned long)scr_a.dev.hz_actual, scr_a.backend->name);
    TEST_DETAIL("SPI requested %lu Hz, actual %lu Hz",
                (unsigned long)scr_a.dev.hz_requested,
                (unsigned long)scr_a.dev.hz_actual);

    /* --- Loop the steps forever -------------------------------------------- */
    for (int pass = 0; ; pass++) {
        if (pass > 0) TEST_REPEAT();
        test_named_colors();
        test_orientation();
        test_speed();
        TEST_PASS_DONE();
        sleep_ms(2000);
    }
}
