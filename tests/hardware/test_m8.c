/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    test_m8.c
 * @brief   Hardware test M8: the framebuffer (BUF8) backend.
 *
 * WHAT IT CHECKS
 *   Screen A stays DIRECT and screen B becomes a BUF8 framebuffer screen,
 *   so the two can be compared side by side: the same animation flickering
 *   on one and not the other, whole-scene redraws, PAINT and POINT (which
 *   need a framebuffer), palette animation, and scrolling, each timed.
 *
 * HARDWARE
 *   Screens A and B on the shared bus, as in test_board.h. Screen B's
 *   framebuffer takes 150 KB of RAM (320 x 480 bytes; the 2.8" board uses
 *   the first 240 x 320 of it). A USB serial terminal shows the steps.
 *
 * WHAT TO LOOK FOR, STEP BY STEP (then they repeat)
 *   (At start, serial lists screen B as BUF8 and times one full flush.)
 *   1  Flicker     bouncing dice, the same code on both screens: they
 *                  flicker on A (DIRECT) and are solid on B (BUF8), even
 *                  where dice overlap. Serial compares the time per frame.
 *   2  Full scene  B redraws the landscape, the dice and a frame counter
 *                  every frame with no flashing, in the landscape's own
 *                  colours. Serial splits draw and flush time.
 *   3  Paint       on B, circle and box regions fill one by one in different
 *                  colours, then the background turns grey, and POINT reads
 *                  "BLUE, WHITE"; A reports QG_ERR_UNSUPPORTED and QG_NONE
 *   4  Palette     rainbow rings on B flowing outward smoothly, without a
 *                  pixel redrawn; serial gives frames per second
 *   5  Scroll      a log scrolling on both screens; serial compares times
 *
 * Program: qg4p_test_m8 (build/tests/hardware/qg4p_test_m8.uf2).
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "qg4p.h"
#include "test_setup.h"
#include "test_images.h"
#define TEST_TAG "M8"
#include "test_log.h"

/* Screen B's framebuffer: one byte per pixel. Sized for the 3.5"
 * board; the 2.8" board simply uses the first 240 x 320 bytes of it.       */
static uint8_t fb_b[320 * 480];

static qg_font_t f_body  = QG_FONT_INIT(qg_font_sans_16,      QG_DEFAULT, 1);
static qg_font_t f_title = QG_FONT_INIT(qg_font_sans_bold_24, QG_YELLOW,  1);
static qg_font_t f_mono  = QG_FONT_INIT(qg_font_mono_12,      QG_DEFAULT, 1);

static qg_screen_t *const screens[2] = { &scr_a, &scr_b };
static qg_image_t d20, landscape;

static void title(qg_screen_t *s, const char *t)
{
    qg_cls(s, QG_BLACK);
    qg_locate(s, 6, 4);
    qg_println(s, t);
}

/* ========================================================================== */
/*  Bouncing dice (pages 1 and 2)                                             */
/* ========================================================================== */
typedef struct { int16_t x, y, vx, vy; } sprite_t;

static void sprites_init(sprite_t *sp, int n, const qg_screen_t *s)
{
    for (int i = 0; i < n; i++) {
        sp[i].x  = (int16_t)(20 + i * (qg_screen_width(s) - 100) / n);
        sp[i].y  = (int16_t)(60 + i * 37);
        sp[i].vx = (int16_t)(3 + i);
        sp[i].vy = (int16_t)(2 + (i * 2) % 5);
    }
}

static void sprite_move(sprite_t *p, const qg_screen_t *s, int16_t top)
{
    p->x = (int16_t)(p->x + p->vx);
    p->y = (int16_t)(p->y + p->vy);
    if (p->x < 0 || p->x + 64 > qg_screen_width(s))  { p->vx = (int16_t)-p->vx; p->x = (int16_t)(p->x + 2 * p->vx); }
    if (p->y < top || p->y + 64 > qg_screen_height(s)) { p->vy = (int16_t)-p->vy; p->y = (int16_t)(p->y + 2 * p->vy); }
}

#define N_DICE 3
#define PAGE1_FRAMES 150

static void page_flicker(void)
{
    sprite_t sp[2][N_DICE];
    TEST_STEP(1, 5, "Flicker", "bouncing dice on both screens", "flicker on A (DIRECT), none on B");

    for (int i = 0; i < 2; i++) {
        qg_screen_t *s = screens[i];
        title(s, qg_screen_is_buffered(s) ? "BUF8: no flicker" : "DIRECT: flicker");
        qg_box(s, 0, 30, (int16_t)(qg_screen_width(s) - 1), (int16_t)(qg_screen_height(s) - 1),
                QG_TRANSPARENT, QG_BLUE);
        sprites_init(sp[i], N_DICE, s);
        qg_screen_flush(s);
    }

    uint64_t t_screen[2] = { 0, 0 };
    for (int f = 0; f < PAGE1_FRAMES; f++) {
        for (int i = 0; i < 2; i++) {
            qg_screen_t *s = screens[i];
            uint64_t t0 = time_us_64();
            /* 1. Erase ALL the dice at their old positions first...
             *
             * ORDER MATTERS: erasing and redrawing one die at a time goes
             * wrong when dice overlap, because erasing a later die's old box
             * paints over an earlier die already drawn this frame. (The M8
             * demo first did exactly that; on BUF8 it showed as a blue box
             * cutting across a die.) Erase everything, then draw everything. */
            for (int k = 0; k < N_DICE; k++) {
                qg_box(s, sp[i][k].x, sp[i][k].y, (int16_t)(sp[i][k].x + 63),
                        (int16_t)(sp[i][k].y + 63), QG_TRANSPARENT, QG_BLUE);
            }
            /* 2. ...then move and draw them all. On DIRECT the viewer sees
             * the blank moment in between: that's the flicker. On BUF8 it all
             * happens in RAM, and the flush below shows only the result.    */
            for (int k = 0; k < N_DICE; k++) {
                sprite_move(&sp[i][k], s, 31);
                qg_image_draw(s, &d20, sp[i][k].x, sp[i][k].y);
            }
            qg_screen_flush(s);        /* BUF8: send the changed area */
            t_screen[i] += time_us_64() - t0;
        }
    }
    TEST_DETAIL("A (DIRECT) %lu us per frame, B (BUF8, with flush) %lu us per frame",
           (unsigned long)(t_screen[0] / PAGE1_FRAMES), (unsigned long)(t_screen[1] / PAGE1_FRAMES));
}

#define PAGE2_FRAMES 90

static void page_full_scene(void)
{
    qg_screen_t *s = &scr_b;
    sprite_t sp[N_DICE];
    const int16_t w = qg_screen_width(s), h = qg_screen_height(s);

    TEST_STEP(2, 5, "Full scene", "B redraws everything per frame", "no flashing, the true colours");
    title(&scr_a, "Full scene");
    qg_println(&scr_a, "Screen B redraws its WHOLE picture every "
                         "frame: background, dice, text. On a DIRECT screen that "
                         "would flash; in a framebuffer it can't.");

    /* Give the landscape its exact colours: copy its palette into the
     * screen's, from entry 16 up (0..15 stay the named colours).            */
    int n = qg_palette_load_image(s, &landscape, 16);
    TEST_DETAIL("loaded %d landscape colours into screen B's palette", n);
    sprites_init(sp, N_DICE, s);

    uint64_t t_draw = 0, t_flush = 0;
    for (int f = 0; f < PAGE2_FRAMES; f++) {
        uint64_t t0 = time_us_64();
        qg_image_draw_scaled(s, &landscape, 0, 0, w, h);          /* background */
        for (int k = 0; k < N_DICE; k++) {
            sprite_move(&sp[k], s, 0);
            qg_image_draw(s, &d20, sp[k].x, sp[k].y);
        }
        char buf[32];
        snprintf(buf, sizeof buf, "{f:1}Frame %d", f);
        qg_print_at(s, 8, 8, buf, QG_WHITE, NULL);
        uint64_t t1 = time_us_64();
        qg_screen_flush(s);
        t_draw  += t1 - t0;
        t_flush += time_us_64() - t1;
    }
    unsigned long per = (unsigned long)((t_draw + t_flush) / PAGE2_FRAMES);
    TEST_DETAIL("per frame: draw %lu us + flush %lu us = %lu us (%lu fps)",
           (unsigned long)(t_draw / PAGE2_FRAMES), (unsigned long)(t_flush / PAGE2_FRAMES),
           per, per ? 1000000ul / per : 0ul);
    qg_palette_reset(s);
}

/* ========================================================================== */
/*  Page 3: PAINT and POINT                                                   */
/* ========================================================================== */
static void page_paint(void)
{
    qg_screen_t *s = &scr_b;
    const int16_t w = qg_screen_width(s);
    char buf[80];

    TEST_STEP(3, 5, "Paint", "flood fills on B, refusals on A", "regions filling one by one");
    title(s, "{f:1}PAINT");

    /* Outlines only: a circle split by lines, a box with slanted walls. */
    const int16_t cx = w / 2, cy = 150, r = (int16_t)(w / 2 - 20);
    qg_circle(s, cx, cy, r, QG_WHITE, QG_TRANSPARENT);
    qg_line(s, (int16_t)(cx - r), cy, (int16_t)(cx + r), cy, QG_WHITE);
    qg_line(s, cx, (int16_t)(cy - r), cx, (int16_t)(cy + r), QG_WHITE);
    qg_line(s, (int16_t)(cx - r / 2), (int16_t)(cy - r + 12), (int16_t)(cx + r / 2), (int16_t)(cy + r - 12), QG_WHITE);
    qg_box(s, 20, 290, (int16_t)(w - 21), 400, QG_WHITE, QG_TRANSPARENT);
    for (int k = 1; k < 5; k++) {
        qg_line(s, (int16_t)(20 + k * (w - 40) / 5), 290, (int16_t)(40 + k * (w - 40) / 5), 400, QG_WHITE);
    }
    qg_screen_flush(s);
    sleep_ms(800);

    /* Fill every enclosed region, one at a time, so you can watch.
     *
     * Rather than working out a starting point for each region by hand,
     * scan a grid of points inside the shapes and use qg_point() to find
     * any that are still black, i.e. regions not yet painted. Each one found
     * gets filled with the next colour. POINT and PAINT working together.  */
    const qg_color_t cols[] = { QG_RED, QG_GREEN, QG_BLUE, QG_YELLOW, QG_MAGENTA,
                                 QG_CYAN, QG_BROWN, QG_LIGHTBLUE, QG_LIGHTRED, QG_LIGHTGREEN };
    int filled = 0;
    for (int16_t gy = 20; gy < 400; gy = (int16_t)(gy + 6)) {
        for (int16_t gx = 20; gx < w - 20; gx = (int16_t)(gx + 6)) {
            int32_t dx = gx - cx, dy = gy - cy;
            bool in_circle = dx * dx + dy * dy < (r - 3) * (r - 3);
            bool in_box    = gx > 22 && gx < w - 23 && gy > 292 && gy < 398;
            if ((!in_circle && !in_box) || qg_point(s, gx, gy) != QG_BLACK) continue;

            uint64_t t0 = time_us_64();
            qg_err_t e = qg_paint(s, gx, gy, cols[filled % 10], QG_WHITE);
            uint64_t us = time_us_64() - t0;
            qg_screen_flush(s);
            TEST_DETAIL("fill %d at (%d,%d): %s in %lu us", filled + 1, gx, gy,
                   e == QG_OK ? "OK" : "error", (unsigned long)us);
            filled++;
            sleep_ms(300);
        }
    }
    const int16_t probe_x = (int16_t)(cx - r / 2), probe_y = (int16_t)(cy - r / 3);

    /* Bucket mode: recolour the whole outside background (black -> grey). */
    qg_paint(s, 2, 420, QG_DARKGRAY, QG_DEFAULT);
    qg_screen_flush(s);

    /* POINT: read colours back from the framebuffer. */
    qg_color_t p1 = qg_point(s, probe_x, probe_y);
    qg_color_t p2 = qg_point(s, cx, cy);
    snprintf(buf, sizeof buf, "{f:2}POINT: %s, %s", qg_color_name(p1) ? qg_color_name(p1) : "?",
             qg_color_name(p2) ? qg_color_name(p2) : "?");
    qg_print_at(s, 20, 410, buf, QG_WHITE, NULL);
    qg_screen_flush(s);

    /* The same calls on the DIRECT screen. */
    title(&scr_a, "{f:1}On DIRECT");
    qg_err_t e = qg_paint(&scr_a, 10, 10, QG_RED, QG_WHITE);
    snprintf(buf, sizeof buf, "qg_paint -> %s\ngfx_point -> %s",
             e == QG_ERR_UNSUPPORTED ? "QG_ERR_UNSUPPORTED" : "?",
             qg_point(&scr_a, 10, 10) == QG_NONE ? "QG_NONE" : "?");
    qg_println(&scr_a, buf);
    qg_println(&scr_a, "{c:DARKGRAY}Reading pixels back needs a framebuffer.");
    sleep_ms(3000);
}

/* ========================================================================== */
/*  Page 4: palette animation                                                 */
/* ========================================================================== */
#define RING_FIRST 200     /* palette entries 200..229 become the rainbow    */
#define RING_COUNT 30
#define PAGE4_FRAMES 120

static void rainbow(int i, uint8_t *r, uint8_t *g, uint8_t *b)
{
    /* Walk around the colour wheel in six straight stretches. */
    int h = (i % RING_COUNT) * 6 * 256 / RING_COUNT, seg = h / 256, t = h % 256;
    switch (seg) {
    case 0:  *r = 255;             *g = (uint8_t)t;         *b = 0;   break;
    case 1:  *r = (uint8_t)(255 - t); *g = 255;             *b = 0;   break;
    case 2:  *r = 0;               *g = 255;                *b = (uint8_t)t; break;
    case 3:  *r = 0;               *g = (uint8_t)(255 - t); *b = 255; break;
    case 4:  *r = (uint8_t)t;      *g = 0;                  *b = 255; break;
    default: *r = 255;             *g = 0;                  *b = (uint8_t)(255 - t); break;
    }
}

static void page_palette(void)
{
    qg_screen_t *s = &scr_b;
    const int16_t cx = qg_screen_width(s) / 2, cy = qg_screen_height(s) / 2;

    TEST_STEP(4, 5, "Palette", "rainbow rings on B", "rings flowing outward, none redrawn");
    title(&scr_a, "Palette animation");
    qg_println(&scr_a, "The rings on screen B are drawn ONCE. Each "
                         "frame only changes 30 palette entries, and the flush "
                         "shows every ring in its new colour.");

    /* Draw concentric rings once, each in its own palette entry. */
    qg_cls(s, QG_BLACK);
    qg_screen_set_line_width(s, 6);
    for (int k = 0; k < RING_COUNT; k++) {
        qg_circle(s, cx, cy, (int16_t)(8 + k * 5), (qg_color_t)(RING_FIRST + k), QG_TRANSPARENT);
    }
    qg_screen_set_line_width(s, 1);

    uint64_t t0 = time_us_64();
    for (int f = 0; f < PAGE4_FRAMES; f++) {
        for (int k = 0; k < RING_COUNT; k++) {
            uint8_t r, g, b;
            rainbow(k + f, &r, &g, &b);
            qg_palette_set(s, (qg_color_t)(RING_FIRST + k), r, g, b);
        }
        qg_screen_flush(s);     /* a palette change marks the whole screen */
    }
    unsigned long per = (unsigned long)((time_us_64() - t0) / PAGE4_FRAMES);
    TEST_DETAIL("%lu us per frame (%lu fps), no pixels redrawn", per, per ? 1000000ul / per : 0ul);
    qg_palette_reset(s);
}

/* ========================================================================== */
/*  Page 5: scrolling                                                         */
/* ========================================================================== */
static void page_scroll(void)
{
    TEST_STEP(5, 5, "Scroll", "a log on both screens", "both logs scrolling cleanly");
    char buf[48];
    for (int i = 0; i < 2; i++) {
        qg_screen_t *s = screens[i];
        title(s, qg_screen_is_buffered(s) ? "{f:1}BUF8 log" : "{f:1}DIRECT log");
        uint64_t t0 = time_us_64();
        for (int n = 1; n <= 40; n++) {
            snprintf(buf, sizeof buf, "Line %02d: {c:%d}the quick brown fox", n, 9 + n % 7);
            qg_println(s, buf);
            qg_screen_flush(s);    /* BUF8: show each line as it's printed */
        }
        TEST_DETAIL("%-6s 40 lines in %lu ms", qg_screen_is_buffered(s) ? "BUF8" : "DIRECT",
               (unsigned long)((time_us_64() - t0) / 1000));
    }
    sleep_ms(2000);
}

/* ========================================================================== */
int main(void)
{
    test_setup_ex(TEST_TAG, "qg4p_test_m8: the framebuffer (BUF8) on screen B", fb_b, sizeof fb_b);

    for (int i = 0; i < 2; i++) {
        qg_screen_set_font(screens[i], 0, &f_body);
        qg_screen_set_font(screens[i], 1, &f_title);
        qg_screen_set_font(screens[i], 2, &f_mono);
    }
    qg_image_open(&d20, img_d20, img_d20_size, QG_IMAGE_TRANSPARENT);
    qg_image_open(&landscape, img_landscape, img_landscape_size, 0);

    /* A full-screen flush on its own, for reference. */
    uint64_t t0 = time_us_64();
    qg_screen_flush_all(&scr_b);
    TEST_LOG("Full-screen flush: %lu us", (unsigned long)(time_us_64() - t0));

    for (int pass = 0; ; pass++) {
        if (pass > 0) TEST_REPEAT();
        page_flicker();
        page_full_scene();
        page_paint();
        page_palette();
        page_scroll();
        TEST_PASS_DONE();
    }
}
