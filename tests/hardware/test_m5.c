/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    test_m5.c
 * @brief   Hardware test M5: markup, tabs, word wrap, alignment, measuring
 *          and scrolling.
 *
 * WHAT IT CHECKS
 *   Markup tags changing colour, font and size inside one line (all on one
 *   shared baseline), "{{" and unknown tags; word wrap at the right edge;
 *   centred and right-aligned lines; measuring text to size a box; tab stops
 *   and {x:} columns; and QuickBasic-style scrolling when printing runs past
 *   the bottom of a DIRECT screen.
 *
 * HARDWARE
 *   Screens A and B on the shared bus, as in test_board.h, both DIRECT. A
 *   USB serial terminal shows the steps and the timings.
 *
 * WHAT TO LOOK FOR, STEP BY STEP (then they repeat)
 *   1  Markup   coloured words by name and number; "BIG" and the big green
 *               "20" sitting on the same baseline as the small text around
 *               them; mono and bold words mid-line; "{like this}" printed
 *               with its brace and the nonsense tag invisible; cyan carried
 *               over a line break, then plain text on the next call
 *   2  Wrap     the story wrapped at whole words, the red words in place; a
 *               centred and a right-aligned line; a blue box that fits its
 *               wrapped, centred message. Serial gives the measured size.
 *   3  Tabs     a 4-column table lined up with tabs; two rows lined up with
 *               {x:}
 *   4  Scroll   40 log lines: the screen scrolls up line by line, grey labels
 *               and coloured results intact, no gaps at the top. Serial
 *               gives the time per line.
 *
 * Program: qg4p_test_m5 (build/tests/hardware/qg4p_test_m5.uf2).
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "qg4p.h"
#include "test_setup.h"
#define TEST_TAG "M5"
#include "test_log.h"

static qg_font_t f_body  = QG_FONT_INIT(qg_font_sans_16,      QG_DEFAULT, 1);
static qg_font_t f_title = QG_FONT_INIT(qg_font_sans_bold_24, QG_YELLOW,  1);
static qg_font_t f_mono  = QG_FONT_INIT(qg_font_mono_12,      QG_DEFAULT, 1);

static qg_screen_t *const screens[2] = { &scr_a, &scr_b };
static const char   *const names[2]   = { "A", "B" };

static uint32_t rng_state = 2024;
static int roll(int sides)
{
    rng_state = rng_state * 1103515245u + 12345u;
    return (int)((rng_state >> 16) % (uint32_t)sides) + 1;
}

/* Fresh page: black, text defaults, fonts in slots 0 (body), 1 (title),
 * 2 (mono).                                                                 */
static void fresh(qg_screen_t *s)
{
    qg_screen_set_colors(s, QG_WHITE, QG_BLACK);
    qg_screen_set_text_bg(s, QG_TRANSPARENT);
    qg_screen_set_wrap(s, true);
    qg_screen_set_scroll(s, true);
    qg_cls(s, QG_BLACK);
    qg_locate(s, 6, 4);
}

/* ========================================================================== */
/*  Page 1: markup                                                            */
/* ========================================================================== */
static void page_markup(qg_screen_t *s)
{
    fresh(s);
    qg_println(s, "{f:1}Markup");
    qg_println(s, "Colours: {c:LIGHTRED}red {c:lightgreen}green {c:12}index 12{c:} plain");
    qg_println(s, "Size: small {s:2}BIG{s:} small");
    qg_println(s, "Fonts: {f:2}mono {f:1}Bold {f:}back");
    qg_println(s, "Braces: {{like this} and {nonsense:tag}skipped");
    qg_println(s, "Rolled {c:LIGHTGREEN}{f:1}{s:2}20{s:}{f:}{c:} {c:YELLOW}critical!");
    qg_println(s, "{c:CYAN}Colours carry\nacross lines{c:} within a call.");
    qg_println(s, "...but each call starts fresh.");
}

/* ========================================================================== */
/*  Page 2: wrap, alignment, measuring                                        */
/* ========================================================================== */
static const char *const story =
    "The door creaks open. Beyond it, a {c:LIGHTRED}red glow{c:} flickers "
    "across piles of old bones, and something large shifts in the dark.";

static void page_wrap(qg_screen_t *s)
{
    const int16_t w = qg_screen_width(s), h = qg_screen_height(s);
    fresh(s);

    qg_println(s, story);                        /* wraps at the right edge */

    int16_t y = (int16_t)(s->cursor_y + 8);
    qg_print_align(s, y, "{c:YELLOW}Centred line", QG_ALIGN_CENTER);
    y = (int16_t)(y + qg_font_line_height(&f_body));
    qg_print_align(s, y, "{c:CYAN}Right-aligned", QG_ALIGN_RIGHT);
    y = (int16_t)(y + qg_font_line_height(&f_body) + 10);

    /* A text box that fits its text: measure the wrapped size at a chosen
     * width, draw a box around that, then print into it with the same width
     * so it wraps identically.                                              */
    const char *msg = "Roll for initiative! {c:LIGHTGREEN}Highest{c:} goes first.";
    int16_t col_w = (int16_t)(w * 60 / 100), tw, th;
    qg_text_measure(s, msg, NULL, col_w, &tw, &th);

    int16_t bx = (int16_t)((w - col_w) / 2);
    qg_screen_set_line_width(s, 2);
    qg_box(s, (int16_t)(bx - 8), y, (int16_t)(bx + col_w + 7), (int16_t)(y + th + 11),
            QG_WHITE, QG_BLUE);
    qg_screen_set_line_width(s, 1);
    qg_print_box(s, bx, (int16_t)(y + 6), col_w, msg, QG_ALIGN_CENTER);

    TEST_DETAIL("measured %d x %d px at a width of %d", tw, th, col_w);
    (void)h;
}

/* ========================================================================== */
/*  Page 3: tabs and columns                                                  */
/* ========================================================================== */
static void page_tabs(qg_screen_t *s)
{
    fresh(s);
    qg_screen_set_font(s, 0, &f_mono);
    qg_screen_set_tab_width(s, 56);

    qg_println(s, "{c:YELLOW}Name\tHP\tAC\tInit");
    qg_println(s, "Aria\t24\t15\t+3");
    qg_println(s, "Borin\t31\t18\t-1");
    qg_println(s, "Cyx\t9\t12\t+5");

    qg_println(s, "");
    qg_println(s, "{c:YELLOW}With {{x:} columns:");
    qg_println(s, "Goblin{x:90}7 hp{x:160}{c:LIGHTGREEN}alive");
    qg_println(s, "Orc{x:90}0 hp{x:160}{c:LIGHTRED}down");

    qg_screen_set_font(s, 0, &f_body);
    qg_screen_set_tab_width(s, QG_TAB_WIDTH);
}

/* ========================================================================== */
/*  Page 4: scrolling                                                         */
/* ========================================================================== */
#define LOG_LINES 40

static void page_scroll(qg_screen_t *s, const char *name)
{
    char buf[48];
    fresh(s);
    qg_println(s, "{f:1}Roll log");

    uint64_t t0 = time_us_64();
    for (int i = 1; i <= LOG_LINES; i++) {
        int r = roll(20);
        const char *col = (r == 20) ? "LIGHTGREEN" : (r == 1) ? "LIGHTRED" : "WHITE";

        /* Two calls build one line: a grey label, then the coloured result. */
        qg_screen_set_colors(s, QG_LIGHTGRAY, QG_DEFAULT);
        snprintf(buf, sizeof buf, "#%02d  d20 = ", i);
        qg_print(s, buf);
        qg_screen_set_colors(s, QG_WHITE, QG_DEFAULT);
        snprintf(buf, sizeof buf, "{c:%s}%d", col, r);
        qg_println(s, buf);
    }
    uint64_t us = time_us_64() - t0;
    TEST_DETAIL("%s: %d lines (most scrolling) in %lu ms, ~%lu ms per line",
           name, LOG_LINES, (unsigned long)(us / 1000), (unsigned long)(us / 1000 / LOG_LINES));
}

/* ========================================================================== */
int main(void)
{
    test_setup(TEST_TAG, "qg4p_test_m5: markup, wrap, alignment, tabs, scrolling");

    for (int i = 0; i < 2; i++) {
        qg_screen_set_font(screens[i], 0, &f_body);
        qg_screen_set_font(screens[i], 1, &f_title);
        qg_screen_set_font(screens[i], 2, &f_mono);
    }

    for (int pass = 0; ; pass++) {
        if (pass > 0) TEST_REPEAT();

        TEST_STEP(1, 4, "Markup", "colours, sizes, fonts mid-line", "one shared baseline, '{' printed");
        for (int i = 0; i < 2; i++) page_markup(screens[i]);
        sleep_ms(6000);

        TEST_STEP(2, 4, "Wrap", "a story, aligned lines, a box", "whole words; the box fits its text");
        for (int i = 0; i < 2; i++) page_wrap(screens[i]);
        sleep_ms(6000);

        TEST_STEP(3, 4, "Tabs", "a table with tabs and {x:}", "straight columns");
        for (int i = 0; i < 2; i++) page_tabs(screens[i]);
        sleep_ms(5000);

        TEST_STEP(4, 4, "Scroll", "40 log lines on each screen", "smooth scrolling, nothing lost");
        for (int i = 0; i < 2; i++) page_scroll(screens[i], names[i]);
        sleep_ms(4000);

        TEST_PASS_DONE();
    }
}
