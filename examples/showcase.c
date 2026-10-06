/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    showcase.c
 * @brief   Example 17: the showcase. Two screens become one wide picture,
 *          and six short scenes show what the library can do.
 *
 * Target: qg4p_showcase.   Screens: A and B, both DIRECT.
 * Pictures: docs/img/showcase_*.png and docs/img/showcase.gif (the README's)
 *
 * Wire up both screens, set examples/board.h (including BOARD_LEFT_SCREEN
 * and BOARD_GAP_PX), flash this, and watch. At power-up it says which
 * screen it thinks is on the left, for 3 seconds. Then, about 30 seconds a
 * loop, forever:
 *
 *   1  Title           the backlights fade in; the title appears
 *   2  Shapes          lines, circles and arcs build a pattern
 *   3  Bouncing ball   a ball crosses from screen to screen, through the gap
 *   4  Scrolling text  a message slides across both screens
 *   5  Dice roll       a d6 on each screen tumbles, settles, and adds up
 *   6  Closing card    the library names and where to find them; fade out
 *
 * Only built-in drawing is used: no images, no asset pack.
 *
 * ---------------------------------------------------------------------------
 *  THREE IDEAS THIS FILE IS BUILT ON
 * ---------------------------------------------------------------------------
 *
 *  1. ONE WIDE WORLD. The two screens, and the gap between them, become one
 *     coordinate system:
 *
 *         [ left screen ][ gap ][ right screen ]
 *
 *     A few small helpers (w_box, w_line, w_print...) take WORLD
 *     coordinates and draw on whichever screen, or both, the shape touches.
 *     Anything in the gap isn't drawn, like a real object passing
 *     behind the bezel. Screens of different heights are centred against
 *     each other, so their middles line up.
 *
 *  2. FRAMES, NOT CLOCKS. Each scene is a function that draws frame n,
 *     knowing what it drew for frame n - 1. Scenes never sleep or read the
 *     time: only main() does, aiming for a steady 30 frames per second. So a
 *     scene is the same sequence of pictures every time, on any hardware,
 *     and on the PC that renders the README's pictures.
 *
 *  3. NO FRAMEBUFFER. Both screens are DIRECT: every call goes straight to
 *     the glass, which fits in memory on any wiring. Moving things are
 *     erased by painting the background back over exactly where they were,
 *     and each scene is built so as little as possible is ever drawn twice.
 *
 * ---------------------------------------------------------------------------
 *  HOW QG4P CLIPS (what makes the wide world possible)
 * ---------------------------------------------------------------------------
 *  Every shape is broken into horizontal runs and rectangles, and every one
 *  of those passes through a single gateway that trims it to the screen in
 *  32-bit arithmetic. So any shape may hang off any edge, at negative
 *  coordinates too, as long as its numbers fit an int16_t (-32768..32767).
 *  The world helpers lean on that: they move the shape into each
 *  screen's own coordinates and let the library cut it off.
 *
 *  Text goes through the same gateway, one glyph at a time, so it's cut off
 *  to the pixel as well, at both edges, with two things to know:
 *    - Word wrap must be off (qg_screen_set_wrap(s, false)). With it on, a
 *      line reaching the right edge wraps onto the next line instead of
 *      running off the screen. w_print() turns it off while it prints.
 *    - QG4P 1.1.0 placed text that started LEFT of the screen one pixel
 *      too far right (it rounded negative positions toward zero), so a
 *      scrolling line would stall for a frame at each left edge. Fixed in
 *      QG4P 1.1.1, found while writing this example.
 *  Opaque text (qg_screen_set_text_bg) sends each whole character as one
 *  block when it's entirely on the screen; a character cut off by an edge
 *  is drawn in two passes (background, then the letter), equally clipped.
 *
 *  What's NOT here: PAINT. Flood-filling needs to read pixels back, which
 *  only a framebuffer (BUF8) screen can do, and this example uses plain
 *  DIRECT screens throughout. See the paint example (paint.c) for PAINT.
 */
#include <math.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "board.h"

#define FPS          30                     /* frames per second, the aim    */
#define FRAME_US     (1000000 / FPS)
#define STARTUP_MS   3000                   /* the LEFT/RIGHT check          */
#define LOOP_SEED    20261006u              /* same dice every loop          */

/* ========================================================================== */
/*  Random numbers you can repeat                                             */
/* ========================================================================== */
/*
 * xorshift32: three shifts and three XORs per number. Seeded with the same
 * value at the start of every loop, it gives the same "random" numbers in
 * the same order every time, so the dice land the same way every loop. For
 * a demo that's fine, and for the README's pictures it's essential.
 * (board_random() in board.c is seeded from the clock: different each run.)
 */
static uint32_t rng_state;

static void rng_seed(uint32_t seed) { rng_state = seed ? seed : 1u; }

static uint32_t rng(uint32_t n)            /* 0 .. n - 1 */
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return n ? rng_state % n : 0;
}

/* ========================================================================== */
/*  The world: two screens side by side                                       */
/* ========================================================================== */

typedef struct {
    qg_screen_t *s;
    const char  *label;        /* "Screen A" or "Screen B"                    */
    int32_t      x0, y0;       /* where the screen's (0, 0) is, in the world  */
    int32_t      w, h;         /* its size in pixels                          */
} panel_t;

static panel_t panels[2];      /* [0] the left screen, [1] the right one     */
static int32_t world_w, world_h;
static int32_t band_top, band_bottom;   /* world rows BOTH screens show      */

static void world_setup(void)
{
    qg_screen_t *left  = (BOARD_LEFT_SCREEN == BOARD_SCREEN_B) ? &screen_b : &screen_a;
    qg_screen_t *right = (left == &screen_a) ? &screen_b : &screen_a;
    qg_screen_t *order[2] = { left, right };

    /* The world is as wide as both screens plus the gap, and as tall as the
     * taller screen. Each screen is centred vertically in it, so a 240x320
     * and a 320x480 screen line up through their middles:
     *
     *       world x:  0        240  280                600
     *     world y 0   .         .    +-------------------+
     *                 .         .    |                   |
     *            80   +---------+    |                   |
     *                 |  left   |gap |      right        |
     *           400   +---------+    |                   |
     *                 .         .    |                   |
     *           480   .         .    +-------------------+                */
    int32_t wl = qg_screen_width(left),  hl = qg_screen_height(left);
    int32_t wr = qg_screen_width(right), hr = qg_screen_height(right);
    world_w = wl + BOARD_GAP_PX + wr;
    world_h = (hl > hr) ? hl : hr;

    for (int i = 0; i < 2; i++) {
        panel_t *p = &panels[i];
        p->s = order[i];
        p->label = (p->s == &screen_a) ? "Screen A" : "Screen B";
        p->w = qg_screen_width(p->s);
        p->h = qg_screen_height(p->s);
        p->x0 = (i == 0) ? 0 : wl + BOARD_GAP_PX;
        p->y0 = (world_h - p->h) / 2;
    }
    /* The rows both screens can show: the shorter screen's rows.          */
    band_top    = (panels[0].y0 > panels[1].y0) ? panels[0].y0 : panels[1].y0;
    band_bottom = (panels[0].y0 + panels[0].h < panels[1].y0 + panels[1].h)
                ?  panels[0].y0 + panels[0].h - 1 : panels[1].y0 + panels[1].h - 1;
}

/* Does the world rectangle x1..x2, y1..y2 (inclusive) touch this screen? A
 * quick test to skip screens a shape can't reach. (Drawing it anyway would
 * be harmless, since the library clips, only slower.)                     */
static bool touches(const panel_t *p, int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
    return x2 >= p->x0 && x1 < p->x0 + p->w && y2 >= p->y0 && y1 < p->y0 + p->h;
}

/* The world helpers. Each one moves its shape into every touched screen's
 * own coordinates (world minus the screen's corner) and draws it there; the
 * library cuts off whatever falls outside.                                */
static void w_box(int32_t x1, int32_t y1, int32_t x2, int32_t y2, qg_color_t stroke, qg_color_t fill)
{
    for (int i = 0; i < 2; i++) {
        const panel_t *p = &panels[i];
        if (!touches(p, x1 < x2 ? x1 : x2, y1 < y2 ? y1 : y2, x1 < x2 ? x2 : x1, y1 < y2 ? y2 : y1)) continue;
        qg_box(p->s, (int16_t)(x1 - p->x0), (int16_t)(y1 - p->y0),
               (int16_t)(x2 - p->x0), (int16_t)(y2 - p->y0), stroke, fill);
    }
}

static void w_line(int32_t x1, int32_t y1, int32_t x2, int32_t y2, qg_color_t c)
{
    for (int i = 0; i < 2; i++) {
        const panel_t *p = &panels[i];
        if (!touches(p, x1 < x2 ? x1 : x2, y1 < y2 ? y1 : y2, x1 < x2 ? x2 : x1, y1 < y2 ? y2 : y1)) continue;
        qg_line(p->s, (int16_t)(x1 - p->x0), (int16_t)(y1 - p->y0),
                (int16_t)(x2 - p->x0), (int16_t)(y2 - p->y0), c);
    }
}

static void w_pset(int32_t x, int32_t y, qg_color_t c)
{
    for (int i = 0; i < 2; i++) {
        const panel_t *p = &panels[i];
        if (touches(p, x, y, x, y)) qg_pset(p->s, (int16_t)(x - p->x0), (int16_t)(y - p->y0), c);
    }
}

/* Text at a world position: (x, y) is the top-left of the line, as for
 * qg_print_at(). Word wrap goes off while it prints, so text runs off the
 * edge and is cut off there, instead of wrapping (see the top of the file). */
static void w_print(int32_t x, int32_t y, const char *text, qg_color_t color, const qg_font_t *font)
{
    int16_t tw, th;
    qg_text_measure(panels[0].s, text, font, 0, &tw, &th);
    for (int i = 0; i < 2; i++) {
        const panel_t *p = &panels[i];
        if (!touches(p, x, y, x + tw - 1, y + th - 1)) continue;
        qg_screen_set_wrap(p->s, false);
        qg_print_at(p->s, (int16_t)(x - p->x0), (int16_t)(y - p->y0), text, color, font);
        qg_screen_set_wrap(p->s, true);
    }
}

/* The same thing on both screens: set a line width, or clear to a colour. */
static void both_line_width(uint8_t w)
{
    qg_screen_set_line_width(&screen_a, w);
    qg_screen_set_line_width(&screen_b, w);
}

static void both_cls(qg_color_t c)
{
    qg_cls(&screen_a, c);
    qg_cls(&screen_b, c);
}

static void both_brightness(int percent)
{
    qg_screen_set_brightness(&screen_a, (uint8_t)percent);
    qg_screen_set_brightness(&screen_b, (uint8_t)percent);
}

/* A colour by RGB: the nearest of the 255 palette colours (both screens
 * start with the same standard palette, so either one's will do).          */
static qg_color_t rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return qg_color_from_rgb(r, g, b, screen_a.palette);
}

/* A rainbow: k = 0..steps-1 walks once round the colour wheel. */
static qg_color_t rainbow(int k, int steps)
{
    int h = (k % steps) * 6 * 255 / steps, seg = h / 255, t = h % 255;
    switch (seg) {
    case 0:  return rgb(255, (uint8_t)t, 0);
    case 1:  return rgb((uint8_t)(255 - t), 255, 0);
    case 2:  return rgb(0, 255, (uint8_t)t);
    case 3:  return rgb(0, (uint8_t)(255 - t), 255);
    case 4:  return rgb((uint8_t)t, 0, 255);
    default: return rgb(255, 0, (uint8_t)(255 - t));
    }
}

/* ========================================================================== */
/*  Start-up check: which screen is on the left?                              */
/* ========================================================================== */
/*
 * Once, at power-up. Each screen says LEFT or RIGHT in the biggest letters
 * that fit, which screen it is, and what to change if that's wrong. The same
 * goes to USB serial.
 */
static void startup_check(void)
{
    static const char *const hint = "Wrong way round? Change BOARD_LEFT_SCREEN in examples/board.h";
    const qg_color_t bg = rgb(0, 0, 96);

    for (int i = 0; i < 2; i++) {
        const panel_t *p = &panels[i];
        qg_screen_t *s = p->s;
        const char *side = i == 0 ? "LEFT" : "RIGHT";
        char buf[64];

        qg_cls(s, bg);

        /* LEFT or RIGHT, scaled up as far as it fits: try 4, then 3... */
        qg_font_t big = qg_font_create(&qg_font_sans_bold_24, QG_YELLOW, 4);
        int16_t tw, th;
        for (;;) {
            qg_text_measure(s, side, &big, 0, &tw, &th);
            if (tw <= p->w - 16 || big.scale == 1) break;
            big.scale--;
        }
        int16_t y = (int16_t)(p->h / 5);
        qg_print_at(s, (int16_t)((p->w - tw) / 2), y, side, QG_DEFAULT, &big);
        y = (int16_t)(y + th + 8);

        /* An arrow pointing at the other screen, under the word. */
        int16_t ax = (int16_t)(p->w / 2), ay = (int16_t)(y + 12), dir = (int16_t)(i == 0 ? 1 : -1);
        qg_screen_set_line_width(s, 5);
        qg_line(s, (int16_t)(ax - 40 * dir), ay, (int16_t)(ax + 40 * dir), ay, QG_WHITE);
        qg_line(s, (int16_t)(ax + 40 * dir), ay, (int16_t)(ax + 24 * dir), (int16_t)(ay - 14), QG_WHITE);
        qg_line(s, (int16_t)(ax + 40 * dir), ay, (int16_t)(ax + 24 * dir), (int16_t)(ay + 14), QG_WHITE);
        qg_screen_set_line_width(s, 1);
        y = (int16_t)(ay + 32);

        /* Which screen this is: "Screen A  ST7789  240x320". */
        snprintf(buf, sizeof buf, "%s  %s  %dx%d", p->label, s->drv->name, (int)p->w, (int)p->h);
        qg_print_align(s, y, buf, QG_ALIGN_CENTER);
        y = (int16_t)(y + qg_font_line_height(&font_body) + 16);

        /* And the fix, wrapped to fit, in grey. */
        qg_screen_set_colors(s, QG_LIGHTGRAY, QG_DEFAULT);
        qg_print_box(s, 10, y, (int16_t)(p->w - 20), hint, QG_ALIGN_CENTER);
        qg_screen_set_colors(s, QG_WHITE, QG_DEFAULT);

        printf("showcase: %-5s is %s  %s  %dx%d\n", side, p->label, s->drv->name, (int)p->w, (int)p->h);
    }
    printf("showcase: %s\n", hint);
    printf("showcase: gap %d px, so the world is %ld x %ld pixels\n",
           BOARD_GAP_PX, (long)world_w, (long)world_h);
}

/* ========================================================================== */
/*  Scene 1: title                                                            */
/* ========================================================================== */
/*
 * The backlights start dark and fade in over a second, on a starry sky that
 * spans both screens (stars "in the gap" aren't drawn). Then the title
 * appears a line at a time on each screen: markup for colours and fonts,
 * and alignment left, centre and right. Last, a shooting star crosses from
 * one screen to the other, its path carrying straight on through the gap.
 */
#define TITLE_FADE 30
#define STAR_FIRST (TITLE_FADE + 70)     /* the shooting star's frames */
#define STAR_LAST  (TITLE_FADE + 100)

/* Where the shooting star's head is at step k (0 .. STAR_LAST - STAR_FIRST):
 * a straight line, low on the left to high on the right, above the text. */
static void star_at(int k, int32_t *x, int32_t *y)
{
    const int n = STAR_LAST - STAR_FIRST;
    *x = world_w * 5 / 100 + (world_w * 90 / 100) * k / n;
    *y = band_top + 50 - 90 * k / n;
}

static void scene_title(int frame)
{
    static qg_color_t sky;

    if (frame == 0) {
        both_brightness(0);                  /* dark before anything shows */
        sky = rgb(0, 0, 48);
        both_cls(sky);
        for (int k = 0; k < 160; k++) {     /* 160 stars, scattered over the world */
            int32_t x = (int32_t)rng((uint32_t)world_w), y = (int32_t)rng((uint32_t)world_h);
            static const qg_color_t star[4] = { QG_WHITE, QG_LIGHTGRAY, QG_DARKGRAY, QG_LIGHTCYAN };
            w_pset(x, y, star[rng(4)]);
        }
    }
    if (frame <= TITLE_FADE) {               /* 0% to 100% over TITLE_FADE frames */
        both_brightness(frame * 100 / TITLE_FADE);
    }

    /* One line of the title every 18 frames after the fade, on both screens. */
    for (int i = 0; i < 2; i++) {
        qg_screen_t *s = panels[i].s;
        const int16_t h = (int16_t)panels[i].h;
        switch (frame) {
        case TITLE_FADE + 6:                 /* {f:1} is the bold 24 font, slot 1 */
            qg_print_align(s, (int16_t)(h * 22 / 100), "{f:1}{c:WHITE}Quick{c:YELLOW}Graphics", QG_ALIGN_CENTER);
            break;
        case TITLE_FADE + 24:                /* {s:2}: twice the size */
            qg_print_align(s, (int16_t)(h * 33 / 100), "{f:1}{s:2}{c:LIGHTGREEN}4 {c:LIGHTRED}Pico", QG_ALIGN_CENTER);
            break;
        case TITLE_FADE + 42:                /* the body font; "\n" starts a new line */
            qg_screen_set_colors(s, QG_LIGHTCYAN, QG_DEFAULT);
            qg_print_align(s, (int16_t)(h * 56 / 100), "graphics for the\nRaspberry Pi Pico 2", QG_ALIGN_CENTER);
            qg_screen_set_colors(s, QG_WHITE, QG_DEFAULT);
            break;
        case TITLE_FADE + 60:                /* left- and right-aligned in a column */
            /* qg_print_box aligns inside a column, here 6 pixels in from
             * each edge; {f:2} is the small mono font. */
            qg_print_box(s, 6, (int16_t)(h - 24), (int16_t)(panels[i].w - 12),
                         "{f:2}{c:LIGHTGRAY}QM4P 0.2.0", QG_ALIGN_LEFT);
            qg_print_box(s, 6, (int16_t)(h - 24), (int16_t)(panels[i].w - 12),
                         "{f:2}{c:LIGHTGRAY}MIT-0", QG_ALIGN_RIGHT);
            break;
        default:
            break;
        }
    }

    /* The shooting star: each frame draws the newest piece of its path in
     * white and repaints the pieces behind it dimmer, then in the sky's own
     * colour, so a short, fading streak moves along. One w_line() per
     * piece, in world coordinates: the helper finds the screens.          */
    if (frame >= STAR_FIRST && frame <= STAR_LAST + 3) {
        static const qg_color_t tail[3] = { QG_WHITE, QG_LIGHTGRAY, QG_DARKGRAY };
        for (int age = 3; age >= 0; age--) {           /* oldest piece first */
            int k = frame - STAR_FIRST - age;
            if (k < 1 || k > STAR_LAST - STAR_FIRST) continue;
            int32_t x1, y1, x2, y2;
            star_at(k - 1, &x1, &y1);
            star_at(k, &x2, &y2);
            w_line(x1, y1, x2, y2, age == 3 ? sky : tail[age]);
        }
    }
}

/* ========================================================================== */
/*  Scene 2: shapes                                                           */
/* ========================================================================== */
/*
 * A pattern built up a few shapes a frame, drawn with the _pct functions so
 * each screen gets the same pattern fitted to its own size:
 *   frames   1-40  "string art": straight lines whose ends walk along two
 *                  edges, so together they trace curves, in all 4 corners
 *   frames  41-64  rings shrinking into the middle, each a little smaller
 *   frames  65-100 a rainbow of short thick arcs around them
 *   frames 101-124 boxes, outlined and filled, in the corners
 * (No PAINT: see the top of the file.)
 */
static void scene_shapes(int frame)
{
    if (frame == 0) {
        both_cls(QG_BLACK);
        both_line_width(1);
        return;
    }
    for (int i = 0; i < 2; i++) {
        qg_screen_t *s = panels[i].s;

        if (frame <= 40) {
            /* Line k joins a point k/40 of the way along one edge to a point
             * k/40 of the way along the next: the envelope is a curve.   */
            uint8_t t = (uint8_t)(frame * 100 / 40), u = (uint8_t)(100 - t);
            qg_color_t c = rainbow(frame, 40);
            qg_line_pct(s, 0, t, t, 100, c);         /* bottom left  */
            qg_line_pct(s, 100, u, u, 0, c);         /* top right    */
            qg_line_pct(s, t, 0, 0, u, c);           /* top left     */
            qg_line_pct(s, u, 100, 100, t, c);       /* bottom right */
        } else if (frame <= 64) {
            /* Rings from 36% of the smaller side down to 4%, every other one
             * filled, so the middle becomes a target.                     */
            int k = frame - 41;                       /* 0..23 */
            uint8_t r = (uint8_t)(36 - k * 32 / 23);
            qg_screen_set_line_width(s, 2);
            qg_circle_pct(s, 50, 50, r, rainbow(k * 3, 72), (k % 4 == 3) ? QG_BLACK : QG_TRANSPARENT);
            qg_screen_set_line_width(s, 1);
        } else if (frame <= 100) {
            /* 36 arcs of 10 degrees: a rainbow ring around the rings. */
            int k = frame - 65;                       /* 0..35 */
            int16_t r = qg_pct_r(s, 44);
            qg_screen_set_line_width(s, 8);
            qg_arc(s, qg_pct_x(s, 50), qg_pct_y(s, 50), r, r,
                   (int16_t)(90 - k * 10 - 8), (int16_t)(90 - k * 10), rainbow(k, 36));
            qg_screen_set_line_width(s, 1);
        } else if (frame <= 124 && frame % 6 == 5) {
            /* Every 6th frame, a box in one corner: outline, fill, both. */
            int k = (frame - 101) / 6;                /* 0..3 */
            static const uint8_t box[4][4] = {
                {  2,  2, 14, 10 }, { 86,  2, 98, 10 }, {  2, 90, 14, 98 }, { 86, 90, 98, 98 } };
            qg_screen_set_line_width(s, 2);
            qg_box_pct(s, box[k][0], box[k][1], box[k][2], box[k][3],
                       QG_WHITE, (k % 2) ? QG_BLUE : QG_RED);
            qg_screen_set_line_width(s, 1);
        }
    }
}

/* ========================================================================== */
/*  Scene 3: a bouncing ball                                                  */
/* ========================================================================== */
/*
 * One ball, one world: it bounces off the outer edges of the two screens and
 * off the top and bottom of the rows both screens share, crossing the gap on
 * the way. On a taller screen the rows outside that band are walls.
 *
 * ERASING WITHOUT A FRAMEBUFFER, AND WITHOUT FLICKER
 * The background is one plain colour, so the ball can be erased by painting
 * that colour where it was. But painting the old ball out and then the new
 * one in would leave a moment with no ball: flicker. Instead, each frame
 * draws two rings that never overlap:
 *
 *        halo:  a ring of BACKGROUND, radius R + HALO, HALO thick
 *        ball:  the ball itself, radius R (white rim, red inside)
 *
 * The ball moves less than HALO pixels a frame, so wherever it was last
 * frame is now inside the halo, which paints it out; and every pixel is
 * sent once, in its final colour. (Circle outlines grow inward, so the
 * halo's hole is exactly the ball's size: see qg_screen_set_line_width.)
 * A view (QuickBasic's VIEW) keeps the halo from biting into the walls.
 */
#define BALL_R     32
#define BALL_VX    7
#define BALL_VY    5
#define BALL_HALO  10        /* > the ball's step, sqrt(7*7 + 5*5) = 8.6, plus 1 */

static void scene_ball(int frame)
{
    static int32_t x, y, vx, vy;
    static qg_color_t bg, wall;

    if (frame == 0) {
        bg = rgb(16, 40, 96);
        wall = rgb(40, 40, 56);
        both_cls(bg);
        for (int i = 0; i < 2; i++) {        /* walls above and below the band */
            const panel_t *p = &panels[i];
            int16_t top = (int16_t)(band_top - p->y0), bottom = (int16_t)(band_bottom - p->y0);
            if (top > 0) {
                qg_box(p->s, 0, 0, (int16_t)(p->w - 1), (int16_t)(top - 1), QG_TRANSPARENT, wall);
                qg_box(p->s, 0, (int16_t)(bottom + 1), (int16_t)(p->w - 1), (int16_t)(p->h - 1), QG_TRANSPARENT, wall);
                qg_screen_set_line_style(p->s, 0xF0F0);
                qg_line(p->s, 0, (int16_t)(top - 1), (int16_t)(p->w - 1), (int16_t)(top - 1), QG_LIGHTGRAY);
                qg_line(p->s, 0, (int16_t)(bottom + 1), (int16_t)(p->w - 1), (int16_t)(bottom + 1), QG_LIGHTGRAY);
                qg_screen_set_line_style(p->s, 0xFFFF);
                if (top >= 30) {
                    qg_print_at(p->s, 8, (int16_t)(top / 2 - 8), "One ball, two screens", QG_LIGHTGRAY, NULL);
                }
            }
        }
        x = BALL_R + 4;  y = band_top + BALL_R + 20;
        vx = BALL_VX;    vy = BALL_VY;
    } else {
        /* Move, then bounce: a ball past an edge is reflected back inside. */
        x += vx;  y += vy;
        if (x < BALL_R)                      { x = 2 * BALL_R - x;                       vx = -vx; }
        if (x > world_w - 1 - BALL_R)        { x = 2 * (world_w - 1 - BALL_R) - x;       vx = -vx; }
        if (y < band_top + BALL_R)           { y = 2 * (band_top + BALL_R) - y;          vy = -vy; }
        if (y > band_bottom - BALL_R)        { y = 2 * (band_bottom - BALL_R) - y;       vy = -vy; }
    }

    const int32_t reach = BALL_R + BALL_HALO;
    for (int i = 0; i < 2; i++) {
        const panel_t *p = &panels[i];
        if (!touches(p, x - reach, y - reach, x + reach, y + reach)) continue;
        int16_t lx = (int16_t)(x - p->x0), ly = (int16_t)(y - p->y0);
        qg_view(p->s, 0, (int16_t)(band_top - p->y0), (int16_t)(p->w - 1),
                (int16_t)(band_bottom - p->y0), false);           /* the band only */
        qg_screen_set_line_width(p->s, BALL_HALO);
        qg_circle(p->s, lx, ly, (int16_t)reach, bg, QG_TRANSPARENT);      /* halo */
        qg_screen_set_line_width(p->s, 3);
        qg_circle(p->s, lx, ly, BALL_R, QG_WHITE, QG_LIGHTRED);           /* ball */
        qg_screen_set_line_width(p->s, 1);
        qg_view_reset(p->s);
    }
}

/* ========================================================================== */
/*  Scene 4: scrolling text                                                   */
/* ========================================================================== */
/*
 * A message enters at the right edge of the right screen, slides across the
 * gap, and leaves past the left edge of the left one: one qg_print_at() per
 * screen per frame, at a position that's often off the screen (negative on
 * the left), cut off by the library to the pixel.
 *
 * Opaque text (a text background colour) paints each character's whole
 * box, so printing the message SPEED pixels further left covers the old
 * one completely, except a strip SPEED pixels wide at its right end, which
 * a box in the background colour paints out. No flicker, nothing left over.
 */
static const char *const message =
    "{c:YELLOW}QM4P{c:} - {c:LIGHTCYAN}QuickMedia 4 Pico{c:} - "
    "graphics for the {c:LIGHTGREEN}Raspberry Pi Pico 2{c:} -";

#define SCROLL_FRAMES 180

static void scene_scroll(int frame)
{
    static int32_t x, y, speed;
    static int16_t tw, th;
    static qg_color_t bg;

    if (frame == 0) {
        bg = rgb(24, 0, 40);
        both_cls(bg);
        qg_text_measure(panels[0].s, message, &font_title, 0, &tw, &th);
        /* Fast enough to cross the whole world in the scene's time. */
        speed = (world_w + tw + SCROLL_FRAMES - 2) / (SCROLL_FRAMES - 1);
        x = world_w;
        y = (band_top + band_bottom) / 2 - th / 2;
        /* Rails above and below the text, across both screens. */
        w_box(0, y - 14, world_w - 1, y - 11, QG_TRANSPARENT, QG_DARKGRAY);
        w_box(0, y + th + 10, world_w - 1, y + th + 13, QG_TRANSPARENT, QG_DARKGRAY);
        qg_screen_set_text_bg(&screen_a, bg);
        qg_screen_set_text_bg(&screen_b, bg);
    } else {
        x -= speed;
        w_box(x + tw, y, x + tw + speed - 1, y + th - 1, QG_TRANSPARENT, bg);  /* the trailing strip */
    }
    w_print(x, y, message, QG_WHITE, &font_title);

    if (frame == SCROLL_FRAMES - 1) {      /* back to see-through text after */
        qg_screen_set_text_bg(&screen_a, QG_TRANSPARENT);
        qg_screen_set_text_bg(&screen_b, QG_TRANSPARENT);
    }
}

/* ========================================================================== */
/*  Scene 5: a dice roll                                                      */
/* ========================================================================== */
/*
 * The project this library was built for was a dice roller, so: a d6 on each
 * screen tumbles through a few faces, bouncing lower each time, settles, and
 * the total appears.
 *
 * A die is drawn from boxes and circles: two overlapping boxes make a square
 * with its corners cut, and a circle in each corner rounds them. To look
 * like it's tumbling, it's squashed: the width follows |cos| of a spin
 * angle, so it narrows to an edge and widens again, showing a new face.
 *
 * Moving it on a DIRECT screen: the parts of last frame's die that the new
 * one won't cover (strips above, below and to the sides, and the four
 * rounded-off corners) are painted back to the table colour, then the new
 * die goes on top. Pixels that stay white are sent white again, so the body
 * holds steady while it moves; the pips and corners blink a little, which
 * reads as tumbling.
 *
 * Once a die stops, it isn't drawn again. Redrawing a die that hasn't moved
 * still paints its corners green and its pips white before putting them
 * back, and 30 times a second that's a flicker on a die that's supposed to
 * be lying still. So a die is drawn only when its position, width or face
 * has changed since the last frame. (Every finished picture was right all
 * along, which is all a PC renderer looks at; it took real glass to show
 * the die drawing itself twice. The host check showcase_still now counts
 * what's sent while a scene sits still.)
 */
#define DIE_SIZE     96
#define DICE_SETTLE  84           /* the frame the dice come to rest */

typedef struct { int16_t x1, y1, x2, y2; } rect16_t;

static void draw_die(qg_screen_t *s, int16_t cx, int16_t cy, int16_t hw, int16_t hh,
                     int value, qg_color_t table, const rect16_t *old)
{
    int16_t r = (int16_t)(DIE_SIZE / 8);
    if (r > hw) r = hw;
    rect16_t now = { (int16_t)(cx - hw), (int16_t)(cy - hh), (int16_t)(cx + hw), (int16_t)(cy + hh) };

    /* 1. Paint out what the new die won't cover: strips of the old box. */
    if (old->x2 >= old->x1) {
        if (old->y1 < now.y1) qg_box(s, old->x1, old->y1, old->x2, (int16_t)(now.y1 - 1), QG_TRANSPARENT, table);
        if (old->y2 > now.y2) qg_box(s, old->x1, (int16_t)(now.y2 + 1), old->x2, old->y2, QG_TRANSPARENT, table);
        if (old->x1 < now.x1) qg_box(s, old->x1, old->y1, (int16_t)(now.x1 - 1), old->y2, QG_TRANSPARENT, table);
        if (old->x2 > now.x2) qg_box(s, (int16_t)(now.x2 + 1), old->y1, old->x2, old->y2, QG_TRANSPARENT, table);
    }
    /* ...and the new die's corners, which the corner circles round off. */
    qg_box(s, now.x1, now.y1, (int16_t)(now.x1 + r), (int16_t)(now.y1 + r), QG_TRANSPARENT, table);
    qg_box(s, (int16_t)(now.x2 - r), now.y1, now.x2, (int16_t)(now.y1 + r), QG_TRANSPARENT, table);
    qg_box(s, now.x1, (int16_t)(now.y2 - r), (int16_t)(now.x1 + r), now.y2, QG_TRANSPARENT, table);
    qg_box(s, (int16_t)(now.x2 - r), (int16_t)(now.y2 - r), now.x2, now.y2, QG_TRANSPARENT, table);

    /* 2. The body: two boxes and four circles, all white. */
    qg_box(s, (int16_t)(now.x1 + r), now.y1, (int16_t)(now.x2 - r), now.y2, QG_TRANSPARENT, QG_WHITE);
    qg_box(s, now.x1, (int16_t)(now.y1 + r), now.x2, (int16_t)(now.y2 - r), QG_TRANSPARENT, QG_WHITE);
    qg_circle(s, (int16_t)(now.x1 + r), (int16_t)(now.y1 + r), r, QG_TRANSPARENT, QG_WHITE);
    qg_circle(s, (int16_t)(now.x2 - r), (int16_t)(now.y1 + r), r, QG_TRANSPARENT, QG_WHITE);
    qg_circle(s, (int16_t)(now.x1 + r), (int16_t)(now.y2 - r), r, QG_TRANSPARENT, QG_WHITE);
    qg_circle(s, (int16_t)(now.x2 - r), (int16_t)(now.y2 - r), r, QG_TRANSPARENT, QG_WHITE);

    /* 3. The pips, on a 3x3 grid, squashed with the die. None edge-on. */
    if (hw * 10 < hh * 4) return;
    static const int8_t pips[7][6][2] = {
        {{0}},
        {{0, 0}},
        {{-1, -1}, {1, 1}},
        {{-1, -1}, {0, 0}, {1, 1}},
        {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}},
        {{-1, -1}, {1, -1}, {0, 0}, {-1, 1}, {1, 1}},
        {{-1, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {1, 1}},
    };
    int16_t sx = (int16_t)(hw * 52 / 100), sy = (int16_t)(hh * 52 / 100);
    int16_t pr = (int16_t)(DIE_SIZE / 11), prx = (int16_t)(pr * hw / hh);
    qg_color_t pip = (value == 1) ? QG_RED : QG_BLACK;
    for (int k = 0; k < value; k++) {
        qg_ellipse(s, (int16_t)(cx + pips[value][k][0] * sx), (int16_t)(cy + pips[value][k][1] * sy),
                   prx < 1 ? 1 : prx, pr, QG_TRANSPARENT, pip);
    }
}

static void scene_dice(int frame)
{
    static qg_color_t felt;
    static rect16_t   last[2];
    static int        face[2], final[2], last_face[2];
    char buf[40];

    if (frame == 0) {
        felt = rgb(0, 100, 50);
        both_cls(felt);
        for (int i = 0; i < 2; i++) {
            const panel_t *p = &panels[i];
            qg_screen_set_line_width(p->s, 6);                 /* a wooden rim */
            qg_box(p->s, 0, 0, (int16_t)(p->w - 1), (int16_t)(p->h - 1), rgb(120, 70, 30), QG_TRANSPARENT);
            qg_screen_set_line_width(p->s, 1);
            qg_print_align(p->s, 16, "{f:1}{c:WHITE}Rolling 2d6", QG_ALIGN_CENTER);
            last[i] = (rect16_t){ 0, 0, -1, -1 };              /* nothing yet */
            final[i] = 1 + (int)rng(6);                        /* decided now */
            face[i] = 1 + (int)rng(6);
        }
    }

    for (int i = 0; i < 2; i++) {
        const panel_t *p = &panels[i];
        int16_t hh = DIE_SIZE / 2, hw = hh;
        int16_t cx = (int16_t)(p->w / 2), floor_y = (int16_t)(p->h / 2 + DIE_SIZE / 2);
        int16_t lift = 0;

        if (frame < DICE_SETTLE) {
            /* Spin: the width follows |cos|, a new face each half turn. The
             * right die spins a little faster, so they don't move as one. */
            float spin = (float)frame * (i ? 0.24f : 0.20f);
            float c = cosf(spin);
            hw = (int16_t)(hh * (c < 0 ? -c : c));
            if (hw < 4) hw = 4;
            int half_turn = (int)((spin + 1.5708f) / 3.14159f);
            static int seen[2] = { -1, -1 };
            if (frame == 0) seen[i] = half_turn;
            if (half_turn != seen[i]) { seen[i] = half_turn; face[i] = 1 + (int)rng(6); }
            /* Bounce: a falling arc that gets lower with every bounce. */
            int t = frame % 28, n = frame / 28;
            lift = (int16_t)((DIE_SIZE / 2) * t * (28 - t) / 196 / (n + 1));
        } else {
            face[i] = final[i];
        }
        int16_t cy = (int16_t)(floor_y - hh - lift);
        rect16_t now = { (int16_t)(cx - hw), (int16_t)(cy - hh), (int16_t)(cx + hw), (int16_t)(cy + hh) };
        if (now.x1 != last[i].x1 || now.y1 != last[i].y1 || now.x2 != last[i].x2 ||
            now.y2 != last[i].y2 || face[i] != last_face[i]) {    /* anything changed? */
            draw_die(p->s, cx, cy, hw, hh, face[i], felt, &last[i]);
            last[i] = now;
            last_face[i] = face[i];
        }

        if (frame == DICE_SETTLE + 18) {     /* the total, under each die */
            snprintf(buf, sizeof buf, "{f:1}Roll: %d + %d = {c:YELLOW}%d", final[0], final[1], final[0] + final[1]);
            qg_print_align(p->s, (int16_t)(floor_y + 24), buf, QG_ALIGN_CENTER);
        }
    }
    if (frame == DICE_SETTLE + 18) {
        printf("showcase: Roll: %d + %d = %d\n", final[0], final[1], final[0] + final[1]);
    }
}

/* ========================================================================== */
/*  Scene 6: the closing card                                                 */
/* ========================================================================== */
/*
 * The library names on the left screen, where to get them on the right; then
 * the backlights fade out, and the loop starts again with the title fading
 * in.
 */
#define CLOSING_FRAMES 150
#define CLOSING_FADE   30

static void scene_closing(int frame)
{
    if (frame == 0) {
        both_cls(QG_BLACK);
        qg_screen_t *l = panels[0].s, *r = panels[1].s;
        int16_t y = (int16_t)(panels[0].h * 16 / 100);
        qg_print_align(l, y, "{f:1}{s:2}{c:YELLOW}QM4P", QG_ALIGN_CENTER);
        y = (int16_t)(y + 2 * qg_font_line_height(&font_title));
        qg_print_align(l, y, "QuickMedia 4 Pico", QG_ALIGN_CENTER);
        y = (int16_t)(y + qg_font_line_height(&font_body) + 20);
        qg_print_align(l, y, "{c:LIGHTGREEN}QG4P{c:}  QuickGraphics", QG_ALIGN_CENTER);
        y = (int16_t)(y + qg_font_line_height(&font_body) + 4);
        qg_print_align(l, y, "{c:LIGHTGREEN}QA4P{c:}  QuickAssets", QG_ALIGN_CENTER);
        y = (int16_t)(y + qg_font_line_height(&font_body) + 4);
        qg_print_align(l, y, "{c:DARKGRAY}QS4P  QuickSound, planned", QG_ALIGN_CENTER);

        /* The address in the body font if it fits the right screen's width,
         * otherwise in the small one: measure first, then print.         */
        static const char *const repo = "PositronicPonderings/qm4p";
        int16_t tw, th;
        qg_text_measure(r, repo, &font_body, 0, &tw, &th);
        const qg_font_t *f = (tw <= panels[1].w - 12) ? &font_body : &font_small;
        y = (int16_t)(panels[1].h * 30 / 100);
        qg_print_align(r, y, "{f:1}Get it", QG_ALIGN_CENTER);
        y = (int16_t)(y + qg_font_line_height(&font_title) + 10);
        qg_text_measure(r, "github.com/", f, 0, &tw, &th);
        qg_print_at(r, (int16_t)((panels[1].w - tw) / 2), y, "github.com/", QG_LIGHTCYAN, f);
        y = (int16_t)(y + th + 2);
        qg_text_measure(r, repo, f, 0, &tw, &th);
        qg_print_at(r, (int16_t)((panels[1].w - tw) / 2), y, repo, QG_LIGHTCYAN, f);
        y = (int16_t)(y + th + 14);
        qg_print_align(r, y, "{c:LIGHTGRAY}MIT-0: do what you like", QG_ALIGN_CENTER);
        qg_print_align(r, (int16_t)(panels[1].h - 30), "{f:2}{c:DARKGRAY}Loop restarts...", QG_ALIGN_CENTER);
    }
    if (frame >= CLOSING_FRAMES - CLOSING_FADE) {
        both_brightness((CLOSING_FRAMES - 1 - frame) * 100 / (CLOSING_FADE - 1));
    }
}

/* ========================================================================== */
/*  main(): the only place that knows about time                              */
/* ========================================================================== */

typedef struct {
    const char *name;
    void      (*draw)(int frame);
    int         frames;            /* at FPS: 150 frames = 5 seconds */
} scene_t;

static const scene_t scenes[] = {
    { "Title",          scene_title,   150 },
    { "Shapes",         scene_shapes,  150 },
    { "Bouncing ball",  scene_ball,    180 },
    { "Scrolling text", scene_scroll,  SCROLL_FRAMES },
    { "Dice roll",      scene_dice,    150 },
    { "Closing card",   scene_closing, CLOSING_FRAMES },
};
#define N_SCENES ((int)(sizeof scenes / sizeof scenes[0]))

int main(void)
{
    board_init_two();                 /* both screens, both DIRECT */
    world_setup();

    startup_check();
    sleep_ms(STARTUP_MS);

    /*
     * THE FRAME CLOCK
     * Each frame has a slot of FRAME_US microseconds. `next` is when the
     * next slot starts; after drawing a frame, sleep for whatever is left
     * of its slot. Counting from a fixed start (next += FRAME_US) rather
     * than "sleep 33 ms after each frame" keeps small errors from adding
     * up. A frame that runs over its slot only makes the next one start
     * late; the clock then restarts from now, rather than rushing the
     * frames after it to catch up.
     */
    uint64_t next = time_us_64();
    for (uint32_t loop = 1; ; loop++) {
        rng_seed(LOOP_SEED);          /* the same pictures every loop */
        printf("showcase: loop %lu\n", (unsigned long)loop);
        for (int i = 0; i < N_SCENES; i++) {
            printf("showcase: %d/%d %s\n", i + 1, N_SCENES, scenes[i].name);
            for (int f = 0; f < scenes[i].frames; f++) {
                scenes[i].draw(f);
                next += FRAME_US;
                uint64_t now = time_us_64();
                if (now > next) next = now;              /* running late */
                sleep_ms((uint32_t)((next - now) / 1000));
            }
        }
    }
}
