/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    test_m1.c
 * @brief   Hardware test M1: two screens on one shared bus, each with its own
 *          driver, SPI speed and backlight brightness.
 *
 * WHAT IT CHECKS
 *   That two different screens can share one SPI bus: each keeps its own
 *   chip driver, panel settings and SPI speed (the bus switches speed
 *   whenever it changes screens), and each backlight dims on its own. It is
 *   also where screen B's panel settings are found. Like M0, the setup is
 *   spelled out step by step below rather than hidden in test_setup.c.
 *
 * HARDWARE
 *   Both screens on the shared bus, wired as in test_board.h:
 *     screen A   2.0" ST7789     CS GP17, backlight GP16
 *     screen B   3.5" ST7796S    CS GP22, backlight GP15
 *                (or the 2.8" ILI9341: set SCREEN_B_BOARD in test_board.h)
 *   A USB serial terminal shows the steps.
 *
 * WHAT TO LOOK FOR, STEP BY STEP (then the steps repeat)
 *   1  Colours      Both screens clear to the 16 named colours together,
 *                   1 second each, and must show the SAME colour at the same
 *                   time. If screen B disagrees, tune its settings:
 *                     - WHITE where BLACK is expected       -> flip B_INVERT
 *                     - RED and BLUE swapped                -> flip B_BGR
 *   2  Orientation  M0's pattern on both screens, in each rotation: RED
 *                   top-left, GREEN top-right, BLUE bottom-left, YELLOW
 *                   bottom-right, a CYAN bar marking the top, a full white
 *                   border.
 *                     - Corners swapped left/right          -> flip B_MIRROR_X
 *                     - Corners swapped top/bottom          -> flip B_MIRROR_Y
 *                   The two screens may turn opposite ways for 90 and 270;
 *                   that's a property of each panel, and fine as long as
 *                   the pattern itself is right.
 *   3  Brightness   Each backlight fades down and back up while the other
 *                   stays steady; then screen A steps 100%, 50%, 10%, goes
 *                   off, and comes back at 10%. Both changing together means
 *                   the backlight pins are mis-wired.
 *   4  Hand-over    Full-screen clears alternate between the screens, so
 *                   the SPI speed switches on every clear. They end on solid
 *                   colours, screen A GREEN and screen B BLUE, with no
 *                   stripes or garbage.
 *   The settings to flip (B_INVERT and the rest) are in test_board.h.
 *
 * Program: qg4p_test_m1 (build/tests/hardware/qg4p_test_m1.uf2).
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "qg4p.h"
#include "qg_internal.h"   /* test-only: raw rectangle fill until M2's qg_box() */
#include "test_board.h"    /* the pins, SPI speeds and panel settings         */
#define TEST_TAG "M1"
#include "test_log.h"      /* the serial line format                          */

static qg_bus_t    bus;
static qg_screen_t scr_a;
static qg_screen_t scr_b;

static const char *const rot_names[4] = { "0", "90", "180", "270" };

/* -------------------------------------------------------------------------- */
static void halt(const char *msg, int err)
{
    TEST_LOG("%s (error %d): check the wiring and test_board.h", msg, err);
    while (true) tight_loop_contents();
}

static void print_screen_info(const char *label, const qg_screen_t *s)
{
    TEST_LOG("Screen %s: %-8s %3d x %3d  SPI %8lu Hz  %s", label, s->drv->name,
             qg_screen_width(s), qg_screen_height(s),
             (unsigned long)s->dev.hz_actual, s->backend->name);
    TEST_DETAIL("SPI requested %lu Hz, actual %lu Hz",
                (unsigned long)s->dev.hz_requested, (unsigned long)s->dev.hz_actual);
}

/* -------------------------------------------------------------------------- */
static void test_named_colors(void)
{
    TEST_STEP(1, 4, "Colours", "16 colours on both screens", "the same colour on both at once");
    for (qg_color_t c = QG_BLACK; c <= QG_WHITE; c++) {
        TEST_DETAIL("%2u  %s", (unsigned)c, qg_color_name(c));
        qg_cls(&scr_a, c);
        qg_cls(&scr_b, c);
        sleep_ms(1000);
    }
}

/* -------------------------------------------------------------------------- */
static void draw_orientation_pattern(qg_screen_t *s)
{
    const int16_t w  = qg_screen_width(s);
    const int16_t h  = qg_screen_height(s);
    const int16_t sq = 30;

    qg_cls(s, QG_BLACK);

    qg_int_fill_rect(s, 0,     0,     w, 1, QG_WHITE);
    qg_int_fill_rect(s, 0,     h - 1, w, 1, QG_WHITE);
    qg_int_fill_rect(s, 0,     0,     1, h, QG_WHITE);
    qg_int_fill_rect(s, w - 1, 0,     1, h, QG_WHITE);

    qg_int_fill_rect(s, 2,          2,          sq, sq, QG_RED);
    qg_int_fill_rect(s, w - sq - 2, 2,          sq, sq, QG_GREEN);
    qg_int_fill_rect(s, 2,          h - sq - 2, sq, sq, QG_BLUE);
    qg_int_fill_rect(s, w - sq - 2, h - sq - 2, sq, sq, QG_YELLOW);

    qg_int_fill_rect(s, w / 2 - 30, 2, 60, 10, QG_CYAN);
    qg_int_fill_rect(s, w - 20, h / 2 - 20, 40, 40, QG_LIGHTMAGENTA);
}

static void test_orientation(void)
{
    TEST_STEP(2, 4, "Orientation", "M0's pattern on both, 4 rotations", "RED top-left on both");
    for (int r = 0; r < 4; r++) {
        qg_screen_set_rotation(&scr_a, (qg_rotation_t)r);
        qg_screen_set_rotation(&scr_b, (qg_rotation_t)r);
        TEST_DETAIL("rotation %-3s -> screen A %d x %d, screen B %d x %d", rot_names[r],
               qg_screen_width(&scr_a), qg_screen_height(&scr_a),
               qg_screen_width(&scr_b), qg_screen_height(&scr_b));
        draw_orientation_pattern(&scr_a);
        draw_orientation_pattern(&scr_b);
        sleep_ms(3000);
    }
    qg_screen_set_rotation(&scr_a, QG_ROT_0);
    qg_screen_set_rotation(&scr_b, QG_ROT_0);
}

/* -------------------------------------------------------------------------- */
static void fade(qg_screen_t *s)
{
    for (int p = 100; p >= 0; p -= 2) { qg_screen_set_brightness(s, (uint8_t)p); sleep_ms(20); }
    for (int p = 0;   p <= 100; p += 2) { qg_screen_set_brightness(s, (uint8_t)p); sleep_ms(20); }
}

static void test_brightness(void)
{
    TEST_STEP(3, 4, "Brightness", "each backlight fades in turn", "the other screen staying steady");
    qg_cls(&scr_a, QG_WHITE);
    qg_cls(&scr_b, QG_WHITE);

    TEST_DETAIL("screen A fades, screen B steady");
    fade(&scr_a);
    sleep_ms(500);

    TEST_DETAIL("screen B fades, screen A steady");
    fade(&scr_b);
    sleep_ms(500);

    TEST_DETAIL("fixed levels: screen A 100%%, 50%%, 10%%; screen B stays at 100%%");
    const uint8_t levels[] = { 100, 50, 10 };
    for (unsigned i = 0; i < sizeof(levels); i++) {
        qg_screen_set_brightness(&scr_a, levels[i]);
        sleep_ms(1000);
    }

    TEST_DETAIL("off, then on: screen A should come back at 10%%");
    qg_screen_backlight(&scr_a, false);
    sleep_ms(1000);
    qg_screen_backlight(&scr_a, true);
    sleep_ms(1500);

    qg_screen_set_brightness(&scr_a, 100);
}

/* -------------------------------------------------------------------------- */
static void test_bus_handover(void)
{
    const int rounds = 16;

    TEST_STEP(4, 4, "Hand-over", "clears alternate between screens", "A solid GREEN, B solid BLUE");

    uint64_t t0 = time_us_64();
    for (int i = 0; i < rounds; i++) {
        qg_cls(&scr_a,     (qg_color_t)(16 + (i * 11) % 216));
        qg_cls(&scr_b, (qg_color_t)(16 + (i * 13) % 216));
    }
    uint64_t us = time_us_64() - t0;

    /* Finish on known colours so the result is easy to judge by eye. */
    qg_cls(&scr_a, QG_GREEN);
    qg_cls(&scr_b, QG_BLUE);

    TEST_DETAIL("%d rounds (%d clears) in %lu us, %lu us per round",
           rounds, rounds * 2, (unsigned long)us, (unsigned long)(us / rounds));
    TEST_DETAIL("expect screen A solid GREEN, screen B solid BLUE, no stripes");
    sleep_ms(3000);
}

/* ========================================================================== */
int main(void)
{
    stdio_init_all();   /* USB serial: waits up to 2 s for a terminal to connect
                           (PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS, set in
                           CMakeLists.txt), then carries on without one     */

    printf("\n");
    TEST_LOG("qg4p_test_m1: two screens, one bus: colours, rotation, backlights");

    /* --- Shared bus --------------------------------------------------------- */
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
    qg_err_t err = qg_bus_init(&bus, &bus_cfg);
    if (err != QG_OK) halt("Bus init FAILED", err);

    /* --- Screen A ---------------------------------------------------------- */
    const qg_screen_config_t cfg_a = {
        .driver = A_DRIVER, .cs_pin = PIN_CS_A,
        .bl_pin = PIN_BL_A, .bl_active_high = true, .spi_hz = A_SPI_HZ,
        .width = A_WIDTH, .height = A_HEIGHT, .x_offset = 0, .y_offset = 0,
        .bgr = A_BGR, .invert = A_INVERT, .mirror_x = A_MIRROR_X, .mirror_y = A_MIRROR_Y,
        .rotation = QG_ROT_0, .backend = QG_BACKEND_DIRECT,
    };
    err = qg_screen_init(&scr_a, &bus, &cfg_a);
    if (err != QG_OK) halt("Screen A init FAILED", err);

    /* --- Screen B ------------------------------------------------------ */
    const qg_screen_config_t cfg_b = {
        .driver = B_DRIVER, .cs_pin = PIN_CS_B,
        .bl_pin = PIN_BL_B, .bl_active_high = true, .spi_hz = B_SPI_HZ,
        .width = B_WIDTH, .height = B_HEIGHT, .x_offset = 0, .y_offset = 0,
        .bgr = B_BGR, .invert = B_INVERT,
        .mirror_x = B_MIRROR_X, .mirror_y = B_MIRROR_Y,
        .rotation = QG_ROT_0, .backend = QG_BACKEND_DIRECT,
    };
    err = qg_screen_init(&scr_b, &bus, &cfg_b);
    if (err != QG_OK) halt("Screen B init FAILED", err);

    print_screen_info("A", &scr_a);
    print_screen_info("B", &scr_b);

    /* --- Loop the steps forever --------------------------------------------- */
    for (int pass = 0; ; pass++) {
        if (pass > 0) TEST_REPEAT();
        test_named_colors();
        test_orientation();
        test_brightness();
        test_bus_handover();
        TEST_PASS_DONE();
    }
}
