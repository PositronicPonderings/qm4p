/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    test_m3.c
 * @brief   Hardware test M3: relative (percentage) coordinates, and the
 *          faster arcs.
 *
 * WHAT IT CHECKS
 *   That a layout written only in percentages (the _pct functions) fits
 *   itself to every screen size and orientation, that 0% and 100% land
 *   exactly on the first and last pixels, and how fast arcs draw: a timed
 *   gauge, and a spinning dial animated on unbuffered screens by repainting
 *   only what changed.
 *
 * HARDWARE
 *   Screens A and B on the shared bus, as in test_board.h, both DIRECT. A
 *   USB serial terminal shows the steps and the timings.
 *
 * WHAT TO LOOK FOR, STEP BY STEP (then they repeat)
 *   1  Layout   a mock dice-roller screen drawn only with _pct calls, the
 *               same on both screens, upright for 4 s and then turned 90
 *               degrees for 4 s. The yellow meter stays round around the
 *               die; white corner ticks touch each corner exactly.
 *   2  Gauge    M2's gauge, drawn with no pauses. Serial gives the time per
 *               10-degree slice: compare it with M2's, roughly 12 ms on A.
 *   3  Dial     a lock-style dial whose yellow marker spins clockwise,
 *               smoothly, on both screens for 4 s; serial gives the frames
 *               per second.
 *
 * Program: qg4p_test_m3 (build/tests/hardware/qg4p_test_m3.uf2).
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "qg4p.h"
#include "test_setup.h"
#define TEST_TAG "M3"
#include "test_log.h"

static qg_screen_t *const screens[2] = { &scr_a, &scr_b };
static const char   *const names[2]   = { "A", "B" };


/* ========================================================================== */
/*  Page 1: a layout in percentages                                           */
/* ========================================================================== */

/* Everything below is in percent. Nothing refers to a pixel size. */
static void draw_layout(qg_screen_t *s)
{
    qg_cls(s, QG_BLACK);

    /* Title bar across the top 12%. */
    qg_screen_set_line_width(s, 1);
    qg_box_pct(s, 0, 0, 100, 12, QG_TRANSPARENT, QG_BLUE);

    /* The "die": a circle in the upper middle, a quarter of the smaller
     * dimension in radius, with a thick outline.                            */
    qg_screen_set_line_width(s, 4);
    qg_circle_pct(s, 50, 45, 25, QG_WHITE, QG_DARKGRAY);

    /* A "roll meter" arc hugging the die, running from lower-left over the
     * top to lower-right.
     *
     * CHOOSING THE RIGHT HELPER: qg_arc_pct() measures rx against the width
     * and ry against the height, so on a sideways (landscape) screen it
     * squashes into a flat ellipse. This meter must stay ROUND around a round
     * die, so both radii come from qg_pct_r() (the smaller dimension) and
     * the absolute qg_arc() does the drawing. Mixing the two styles like
     * this is exactly what the qg_pct_*() helpers are for.                 */
    qg_screen_set_line_width(s, 3);
    int16_t meter_r = qg_pct_r(s, 32);
    qg_arc(s, qg_pct_x(s, 50), qg_pct_y(s, 45), meter_r, meter_r, -45, 225, QG_YELLOW);

    /* Three buttons along the bottom. */
    qg_screen_set_line_width(s, 2);
    qg_box_pct(s,  4, 80, 30, 95, QG_LIGHTGREEN, QG_GREEN);
    qg_box_pct(s, 37, 80, 63, 95, QG_LIGHTCYAN,  QG_CYAN);
    qg_box_pct(s, 70, 80, 96, 95, QG_LIGHTRED,   QG_RED);

    /* Corner ticks: lines from each corner 10% inward, to show 0 and 100
     * land exactly on the first and last pixels.                            */
    qg_screen_set_line_width(s, 1);
    qg_line_pct(s,   0,   0,  10,  10, QG_WHITE);
    qg_line_pct(s, 100,   0,  90,  10, QG_WHITE);
    qg_line_pct(s,   0, 100,  10,  90, QG_WHITE);
    qg_line_pct(s, 100, 100,  90,  90, QG_WHITE);

    /* Single pixels at the four "exact" corners: 0 and 100 in both axes. */
    qg_pset_pct(s,   0,   0, QG_YELLOW);
    qg_pset_pct(s, 100,   0, QG_YELLOW);
    qg_pset_pct(s,   0, 100, QG_YELLOW);
    qg_pset_pct(s, 100, 100, QG_YELLOW);
}

static void page_layout(void)
{
    TEST_STEP(1, 3, "Layout", "percent layout, upright, then sideways", "ticks touching each corner");

    for (int rot = 0; rot <= 1; rot++) {
        uint64_t us[2];
        for (int i = 0; i < 2; i++) {
            qg_screen_set_rotation(screens[i], rot ? QG_ROT_90 : QG_ROT_0);
            uint64_t t0 = time_us_64();
            draw_layout(screens[i]);
            us[i] = time_us_64() - t0;
        }
        TEST_DETAIL("rotation %2d: drawn in A %lu.%lu ms, B %lu.%lu ms", rot ? 90 : 0,
                    (unsigned long)(us[0] / 1000), (unsigned long)((us[0] % 1000) / 100),
                    (unsigned long)(us[1] / 1000), (unsigned long)((us[1] % 1000) / 100));
        sleep_ms(4000);
    }
    for (int i = 0; i < 2; i++) {
        qg_screen_set_rotation(screens[i], QG_ROT_0);
    }
}

/* ========================================================================== */
/*  Page 2: gauge benchmark                                                   */
/* ========================================================================== */
static void page_gauge(void)
{
    TEST_STEP(2, 3, "Gauge", "M2's gauge, no pauses", "the time per slice (M2: ~12 ms on A)");

    for (int i = 0; i < 2; i++) {
        qg_screen_t *s = screens[i];
        const int16_t w = qg_screen_width(s);
        const int16_t r = (int16_t)(w / 2 - 12);
        const int16_t cx = w / 2, cy = (int16_t)(r + 8);

        qg_cls(s, QG_BLACK);
        qg_screen_set_line_width(s, 10);
        qg_arc(s, cx, cy, r, r, -45, 225, QG_DARKGRAY);

        int slices = 0;
        uint64_t t0 = time_us_64();
        for (int deg = 225; deg > -45; deg -= 10) {
            qg_color_t c = (deg > 45) ? QG_LIGHTGREEN : (deg > 0 ? QG_YELLOW : QG_LIGHTRED);
            qg_arc(s, cx, cy, r, r, (int16_t)(deg - 10), (int16_t)deg, c);
            slices++;
        }
        uint64_t us = time_us_64() - t0;

        TEST_DETAIL("%s: %d slices in %lu.%lu ms -> %lu us per slice", names[i],
               slices, (unsigned long)(us / 1000), (unsigned long)((us % 1000) / 100),
               (unsigned long)(us / (uint64_t)slices));
    }
    sleep_ms(3000);
}

/* ========================================================================== */
/*  Page 3: spinning dial                                                     */
/* ========================================================================== */

/*
 * HOW THE DIAL ANIMATES ON AN UNBUFFERED SCREEN
 * The marker moves clockwise by DIAL_STEP_DEG each frame. The old and new
 * positions overlap:
 *
 *      new marker:  [next ........ next+14]
 *      old marker:          [angle ........ angle+14]      (angle = next+6)
 *                                   ^^^^^^^ trailing strip: now uncovered
 *
 * Erasing the WHOLE old marker would blank the overlap for a moment, which
 * the eye sees as flicker. So each frame we only paint the trailing strip
 * back to the track colour, then draw the new marker. Nothing that should
 * stay yellow is ever blanked, and only two small slices are sent per frame.
 * (With a BUF8 framebuffer in M8, you could redraw the whole scene instead.)
 */
#define DIAL_MARKER_DEG 14    /* marker size         */
#define DIAL_STEP_DEG    6    /* movement per frame  */
#define DIAL_SECONDS     4

static void page_dial(void)
{
    TEST_STEP(3, 3, "Dial", "a marker spinning for 4 s", "a smooth clockwise spin on both");

    int16_t cx[2], cy[2], r[2];
    for (int i = 0; i < 2; i++) {
        qg_screen_t *s = screens[i];
        cx[i] = qg_pct_x(s, 50);
        cy[i] = qg_pct_y(s, 50);
        r[i]  = qg_pct_r(s, 40);

        qg_cls(s, QG_BLACK);
        qg_screen_set_line_width(s, 12);
        qg_arc(s, cx[i], cy[i], r[i], r[i], 0, 0, QG_DARKGRAY);   /* full ring */

        /* Tick marks every 30 degrees, like a combination lock: short, thick
         * arcs just outside the ring. (Arc outlines grow inward, so a width
         * of 7 at radius r+15 covers r+9 to r+15.)                           */
        qg_screen_set_line_width(s, 7);
        for (int t = 0; t < 360; t += 30) {
            qg_arc(s, cx[i], cy[i], (int16_t)(r[i] + 15), (int16_t)(r[i] + 15),
                    (int16_t)(t - 2), (int16_t)(t + 2), QG_WHITE);
        }
        qg_screen_set_line_width(s, 12);
    }

    int angle = 90;            /* start at 12 o'clock, spin clockwise */
    for (int i = 0; i < 2; i++) {
        qg_arc(screens[i], cx[i], cy[i], r[i], r[i],
                (int16_t)angle, (int16_t)(angle + DIAL_MARKER_DEG), QG_YELLOW);
    }

    int      frames = 0;
    uint64_t t_end  = time_us_64() + (uint64_t)DIAL_SECONDS * 1000000u;
    uint64_t t0     = time_us_64();

    while (time_us_64() < t_end) {
        int next = angle - DIAL_STEP_DEG;
        for (int i = 0; i < 2; i++) {
            /* Erase only the trailing strip, then draw the new marker. */
            qg_arc(screens[i], cx[i], cy[i], r[i], r[i],
                    (int16_t)(next + DIAL_MARKER_DEG), (int16_t)(angle + DIAL_MARKER_DEG),
                    QG_DARKGRAY);
            qg_arc(screens[i], cx[i], cy[i], r[i], r[i],
                    (int16_t)next, (int16_t)(next + DIAL_MARKER_DEG), QG_YELLOW);
        }
        angle = next;
        frames++;
    }
    uint64_t us = time_us_64() - t0;

    unsigned long fps10 = (unsigned long)((uint64_t)frames * 10000000u / us);
    TEST_DETAIL("%d frames (both screens each frame) -> %lu.%lu frames per second",
           frames, fps10 / 10, fps10 % 10);

    for (int i = 0; i < 2; i++) {
        qg_screen_set_line_width(screens[i], 1);
    }
    sleep_ms(1500);
}

/* ========================================================================== */
int main(void)
{
    test_setup(TEST_TAG, "qg4p_test_m3: layouts in percentages, arc speed, a spinning dial");

    for (int pass = 0; ; pass++) {
        if (pass > 0) TEST_REPEAT();
        page_layout();
        page_gauge();
        page_dial();
        TEST_PASS_DONE();
    }
}
