/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/*
 * Text that starts off the left or top edge of the screen.
 *
 * Text positions are worked out in 1/16 pixel and rounded to whole pixels.
 * QG4P 1.1.0 rounded with C's division, which rounds toward zero, so for a
 * NEGATIVE position it rounded the wrong way: text printed at x = -1 landed
 * on x = 0, and everything left of the screen sat one pixel too far right.
 * None of the golden fingerprints caught it, because no test page puts text
 * off the left edge. This test does.
 *
 * Each check draws once fully on screen, as a REFERENCE, then again at the
 * position under test, and requires every pixel of the second drawing to be
 * the reference moved by exactly the difference: nothing out by a pixel,
 * nothing missing, nothing extra.
 *   1. one glyph at x = 0, -1, -8, -15, -16, -17 (transparent, opaque, x2)
 *   2. the same glyph at y = 0, -1, -8, -15, -16, -17
 *   3. a centred line wider than the screen (wrap off), so the room left
 *      over is negative and odd: the odd pixel must fall on the same side
 *      as it does when the room is positive.
 * (Kerning, the third place a negative number is divided, is checked in
 * test_text_units.c, which can reach the function directly.)
 */
#include <stdio.h>
#include <string.h>
#include "qg4p.h"

#define W 200                       /* the biggest fake screen */
#define H 120
static uint16_t pix[2][H][W];       /* two screens' pixels, as RGB565 */
static qg_screen_t s0, s1;
static int fails, checks;

static uint16_t (*buf(qg_screen_t *s))[W] { return pix[s == &s1]; }
static void ff(qg_screen_t *s, int16_t x, int16_t y, int16_t w, int16_t h, qg_color_t c)
{
    if (x < 0 || y < 0 || x + w > s->width || y + h > s->height) { printf("FAIL  write off the screen\n"); fails++; return; }
    for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) buf(s)[j][i] = s->palette[c];
}
static void wr(qg_screen_t *s, int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *p)
{
    if (x < 0 || y < 0 || x + w > s->width || y + h > s->height) { printf("FAIL  write off the screen\n"); fails++; return; }
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) buf(s)[y + j][x + i] = p[j * w + i];
}
static const qg_backend_t direct = { .name = "test", .fill_rect = ff, .write_rgb565 = wr };
static qg_font_t body  = QG_FONT_INIT(qg_font_sans_16, QG_WHITE, 1);
static qg_font_t big   = QG_FONT_INIT(qg_font_sans_bold_24, QG_WHITE, 1);
static qg_font_t big2  = QG_FONT_INIT(qg_font_sans_bold_24, QG_WHITE, 2);

static void mk(qg_screen_t *s, int w, int h)
{
    memset(s, 0, sizeof *s);
    s->width = (int16_t)w; s->height = (int16_t)h; s->ready = true; s->backend = &direct;
    s->fg_color = QG_WHITE; s->bg_color = QG_BLACK; s->line_width = 1; s->line_style = 0xFFFF;
    s->text_bg = QG_TRANSPARENT; s->tab_width = 40; s->wrap = false; s->scroll = false;
    qg_palette_copy_standard(s->palette); qg_view_reset(s);
    qg_screen_set_font(s, 0, &body);
    memset(buf(s), 0, sizeof pix[0]);
}

/* Is screen s1 exactly screen s0 moved by (dx, dy)? Pixels that would come
 * from outside s0 must be black. Also reports the first lit column/row.     */
static void same_moved(int dx, int dy, const char *what)
{
    int bad = 0;
    for (int j = 0; j < s1.height; j++) for (int i = 0; i < s1.width; i++) {
        int si = i - dx, sj = j - dy;
        uint16_t want = (si >= 0 && sj >= 0 && si < s0.width && sj < s0.height) ? pix[0][sj][si] : 0;
        if (pix[1][j][i] != want) bad++;
    }
    checks++;
    if (bad) { printf("FAIL  %s: %d pixels differ from the reference moved by (%d, %d)\n", what, bad, dx, dy); fails++; }
}

/* First lit column (or row) of a screen, -1 if blank. */
static int first_lit(int which, int by_row)
{
    qg_screen_t *s = which ? &s1 : &s0;
    int n = by_row ? s->height : s->width, m = by_row ? s->width : s->height;
    for (int a = 0; a < n; a++) for (int b = 0; b < m; b++)
        if ((by_row ? pix[which][a][b] : pix[which][b][a]) != 0) return a;
    return -1;
}

int main(void)
{
    static const int at[] = { 0, -1, -8, -15, -16, -17 };
    static const struct { const qg_font_t *f; bool opaque; const char *name; } kinds[] = {
        { &big, false, "glyph" }, { &big, true, "opaque glyph" }, { &big2, false, "glyph x2" } };
    char what[96];

    /* 1 and 2: one glyph, moved left and moved up. The reference sits at
     * (60, 40), well inside the screen.                                    */
    for (unsigned k = 0; k < sizeof kinds / sizeof kinds[0]; k++) {
        for (int axis = 0; axis < 2; axis++) {
            for (unsigned i = 0; i < sizeof at / sizeof at[0]; i++) {
                int x = axis == 0 ? at[i] : 60, y = axis == 0 ? 40 : at[i];
                mk(&s0, W, H); mk(&s1, W, H);
                if (kinds[k].opaque) { qg_screen_set_text_bg(&s0, QG_BLUE); qg_screen_set_text_bg(&s1, QG_BLUE); }
                qg_print_at(&s0, 60, 40, "W", QG_DEFAULT, kinds[k].f);
                qg_print_at(&s1, (int16_t)x, (int16_t)y, "W", QG_DEFAULT, kinds[k].f);
                snprintf(what, sizeof what, "%s at %c = %d", kinds[k].name, axis ? 'y' : 'x', at[i]);
                same_moved(x - 60, y - 40, what);
                /* The edge that's still on screen, stated plainly. */
                int ref = first_lit(0, axis), got = first_lit(1, axis);
                int want = ref + at[i] - (axis ? 40 : 60);
                if (want >= 0 && got != want) {
                    printf("FAIL  %s: first lit %s %d, expected %d\n", what, axis ? "row" : "column", got, want);
                    fails++;
                }
            }
        }
    }

    /* 3: a centred line wider than the screen. Measure it, make the screen
     * 3 pixels narrower, and the room left over is -3: centring puts it at
     * floor(-3 / 2) = -2, two pixels hanging off the left and one off the
     * right, the way +3 of room puts 1 pixel left of the text and 2 right. */
    const char *line = "Centre me, please";
    int16_t tw = 0, th = 0;
    mk(&s0, W, H);
    qg_text_measure(&s0, line, NULL, 0, &tw, &th);
    for (int room = -3; room <= 3; room += 2) {
        mk(&s0, W, H); mk(&s1, tw + room, H);
        qg_print_at(&s0, 20, 10, line, QG_DEFAULT, NULL);
        qg_print_align(&s1, 10, line, QG_ALIGN_CENTER);
        int shift = room >= 0 ? room / 2 : -((1 - room) / 2);       /* floor(room / 2) */
        snprintf(what, sizeof what, "centred line with %+d pixels of room", room);
        same_moved(shift - 20, 0, what);
    }

    if (fails) printf("%d FAILED of %d checks\n", fails, checks);
    else       printf("text at the edges: all %d checks pass\n", checks);
    return fails != 0;
}
