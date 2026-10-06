/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/*
 * Runs one example (compiled in with -Dmain=example_main) against stand-in
 * screens, stops it after STOP calls to sleep_ms() (or when it settles into
 * an idle loop), and saves each screen as a PPM.
 *   Usage: render_example_<name> <stop> <out_prefix>
 *
 * FRAMES=first:last:step also saves the screens at every step-th call to
 * sleep_ms() from first to last, as <out_prefix>_NNNN_a.ppm and _b.ppm
 * (NNNN = the call's number), for animations. An example that sleeps once
 * per frame, like the showcase, gives one picture per frame.
 *
 * DRAWS=first:last prints, for each call to sleep_ms() from first to last,
 * how many drawing operations reached the screens since the call before:
 * "draws NNNN COUNT" on stderr. A finished picture can be right while the
 * program keeps redrawing it, which flickers on a real DIRECT screen; this
 * is how a host test sees that.
 */
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "board.h"
#include "qg_internal.h"
#include "qa4p.h"

int example_main(void);

/* ---- the stand-in board ---- */
qg_screen_t screen_a, screen_b;
qg_font_t font_body  = QG_FONT_INIT(qg_font_sans_16,      QG_DEFAULT, 1);
qg_font_t font_title = QG_FONT_INIT(qg_font_sans_bold_24, QG_YELLOW,  1);
qg_font_t font_small = QG_FONT_INIT(qg_font_mono_12,      QG_DEFAULT, 1);
/* Panels are stored row by row at the screen's CURRENT width, so a rotated
 * (sideways) screen is stored the right way round.                          */
static uint16_t panel_a[BOARD_A_HEIGHT * BOARD_A_WIDTH], panel_b[BOARD_B_HEIGHT * BOARD_B_WIDTH];
static int has_b; static long oob, draws;
static uint16_t *px(qg_screen_t *s, int x, int y) { return (s == &screen_a ? panel_a : panel_b) + y * s->width + x; }
static void dff(qg_screen_t *s, int16_t x, int16_t y, int16_t w, int16_t h, qg_color_t c)
{ draws++; if (x < 0 || y < 0 || x + w > s->width || y + h > s->height) { oob++; return; }
  for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) *px(s, i, j) = s->palette[c]; }
static void dwr(qg_screen_t *s, int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *p)
{ draws++; if (x < 0 || y < 0 || x + w > s->width || y + h > s->height) { oob++; return; }
  for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) *px(s, x + i, y + j) = p[j * w + i]; }
static const qg_backend_t direct = { .name = "DIRECT", .fill_rect = dff, .write_rgb565 = dwr };

/* framebuffer flushes land on the panels, through stand-in HAL calls */
static qg_screen_t *cur; static int wx0, wx1, wcx, wcy;
void qg_hal_begin(qg_hal_device_t *d) { cur = (d == &screen_a.dev) ? &screen_a : &screen_b; }
void qg_hal_end(qg_hal_device_t *d) { (void)d; }
void qg_hal_write_cmd(qg_hal_device_t *d, uint8_t c) { (void)d; (void)c; }
void qg_hal_write_data(qg_hal_device_t *d, const uint8_t *p, size_t n) { (void)d; (void)p; (void)n; }
void qg_driver_set_window(qg_hal_device_t *d, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{ (void)d; (void)y1; wx0 = x0; wx1 = x1; wcx = x0; wcy = y0; }
void qg_hal_stream_begin(qg_hal_device_t *d) { (void)d; }
void qg_hal_stream_pixels(qg_hal_device_t *d, const uint16_t *p, uint32_t n)
{ (void)d; draws++; for (uint32_t i = 0; i < n; i++) { *px(cur, wcx, wcy) = p[i]; if (++wcx > wx1) { wcx = wx0; wcy++; } } }
void qg_hal_stream_end(qg_hal_device_t *d) { (void)d; }
void qg_screen_flush(qg_screen_t *s) { if (s->backend->flush) s->backend->flush(s); }
void qg_screen_flush_all(qg_screen_t *s) { if (!s->fb) return; qg_int_dirty_all(s); s->backend->flush(s); }
void qg_screen_set_line_width(qg_screen_t *s, uint8_t w) { s->line_width = w < 1 ? 1 : w; }
void qg_screen_set_colors(qg_screen_t *s, qg_color_t fg, qg_color_t bg) { if (fg <= 254) s->fg_color = fg; if (bg <= 254) s->bg_color = bg; }
void qg_screen_set_brightness(qg_screen_t *s, uint8_t p) { s->brightness = p; }
qg_err_t qg_screen_set_rotation(qg_screen_t *s, qg_rotation_t r)
{ int w = s == &screen_a ? BOARD_A_WIDTH : BOARD_B_WIDTH, h = s == &screen_a ? BOARD_A_HEIGHT : BOARD_B_HEIGHT;
  s->rotation = r; if (r & 1) { s->width = (int16_t)h; s->height = (int16_t)w; } else { s->width = (int16_t)w; s->height = (int16_t)h; }
  qg_view_reset(s); { static qg_text_line_t qg_hist_pool[4][QG_TEXT_HISTORY_LINES]; static const void *qg_hist_owner[4]; int k_ = 0; while (k_ < 3 && qg_hist_owner[k_] && qg_hist_owner[k_] != (const void *)(s)) k_++; qg_hist_owner[k_] = (s); (s)->hist = qg_hist_pool[k_]; (s)->hist_cap = QG_TEXT_HISTORY_LINES; } return QG_OK; }

static const qg_driver_t fake_drv_a = { .name = "ST7789" }, fake_drv_b = { .name = "ST7796S" };
static void mk(qg_screen_t *s, int w, int h, uint8_t *fb, const qg_driver_t *drv)
{
    memset(s, 0, sizeof *s);
    s->width = (int16_t)w; s->height = (int16_t)h; s->ready = true; s->drv = drv;
    s->fg_color = QG_WHITE; s->bg_color = QG_BLACK; s->line_width = 1; s->line_style = 0xFFFF;
    s->text_bg = QG_TRANSPARENT; s->tab_width = QG_TAB_WIDTH; s->wrap = true; s->scroll = true; s->brightness = 100;
    s->dev.hz_actual = 37500000u;
    qg_palette_copy_standard(s->palette); qg_view_reset(s); { static qg_text_line_t qg_hist_pool[4][QG_TEXT_HISTORY_LINES]; static const void *qg_hist_owner[4]; int k_ = 0; while (k_ < 3 && qg_hist_owner[k_] && qg_hist_owner[k_] != (const void *)(s)) k_++; qg_hist_owner[k_] = (s); (s)->hist = qg_hist_pool[k_]; (s)->hist_cap = QG_TEXT_HISTORY_LINES; }
    if (fb) { s->fb = fb; s->backend = &qg_backend_buf8; } else s->backend = &direct;
    qg_screen_set_font(s, 0, &font_body); qg_screen_set_font(s, 1, &font_title); qg_screen_set_font(s, 2, &font_small);
}
const qg_color_adjust_t board_adjust_a = BOARD_A_ADJUST, board_adjust_b = BOARD_B_ADJUST;
static uint32_t rng = 12345u;
void board_init_fb(uint8_t *fb, uint32_t size) { (void)size; mk(&screen_a, BOARD_A_WIDTH, BOARD_A_HEIGHT, fb, &fake_drv_a); }
void board_init(void) { board_init_fb(NULL, 0); }
void board_init_two_fb(uint8_t *fb, uint32_t size)
{ (void)size; has_b = 1; mk(&screen_a, BOARD_A_WIDTH, BOARD_A_HEIGHT, NULL, &fake_drv_a); mk(&screen_b, BOARD_B_WIDTH, BOARD_B_HEIGHT, fb, &fake_drv_b); }
void board_init_two(void) { board_init_two_fb(NULL, 0); }
uint32_t board_random(uint32_t n) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return n ? rng % n : 0; }

/* the asset example: its pack goes into the pretend flash (see qa4p.h) at
 * the usual 1 MB, where the example's own qa_open() finds it. NOPACK=1
 * leaves the flash empty, for the "no pack" screen.                       */
static void load_pack(void)
{
    if (getenv("NOPACK")) return;
    FILE *f = fopen("example_pack/assets.bin", "rb");
    if (!f) return;
    size_t n = fread(qa_host_flash + QA_DEFAULT_OFFSET, 1, 1 << 20, f); fclose(f); (void)n;
}

/* ---- stopping the example (and saving frames on the way, see FRAMES) ---- */
static jmp_buf done; static long sleeps, stop_at; static uint64_t fake_us;
static long f_first, f_last, f_step, d_first, d_last = -1; static const char *f_prefix;
static void dump_both(const char *prefix, const char *sep);
void sleep_ms(uint32_t ms)
{
    fake_us += (uint64_t)ms * 1000u; ++sleeps;
    if (sleeps >= d_first && sleeps <= d_last) fprintf(stderr, "draws %04ld %ld\n", sleeps, draws);
    draws = 0;
    if (f_step > 0 && sleeps >= f_first && sleeps <= f_last && (sleeps - f_first) % f_step == 0) {
        char p[160]; snprintf(p, sizeof p, "%s_%04ld", f_prefix, sleeps); dump_both(p, "_");
    }
    if (sleeps >= stop_at) longjmp(done, 1);
}
void tight_loop_contents(void) { longjmp(done, 1); }         /* an idle loop means "finished" */
uint64_t time_us_64(void) { return fake_us += 50; }
/* Serial input: keys "typed" from the QG_KEYS environment variable, then nothing. */
int getchar_timeout_us(uint32_t us)
{
    (void)us;
    static const char *keys; static int started;
    if (!started) { keys = getenv("QG_KEYS"); started = 1; }
    if (keys && *keys) return (unsigned char)*keys++;
    return PICO_ERROR_TIMEOUT;
}
bool stdio_init_all(void) { return true; }

static void dump(const char *name, qg_screen_t *s)
{
    FILE *f = fopen(name, "wb");
    fprintf(f, "P6 %d %d 255\n", s->width, s->height);
    for (int y = 0; y < s->height; y++) for (int x = 0; x < s->width; x++) {
        uint16_t p = *px(s, x, y);
        uint8_t c[3] = { (uint8_t)(((p >> 11) & 31) << 3), (uint8_t)(((p >> 5) & 63) << 2), (uint8_t)((p & 31) << 3) };
        fwrite(c, 1, 3, f);
    }
    fclose(f);
}

static void dump_both(const char *prefix, const char *sep)
{
    char name[192];
    if (has_b) {
        snprintf(name, sizeof name, "%s%sa.ppm", prefix, sep); dump(name, &screen_a);
        snprintf(name, sizeof name, "%s%sb.ppm", prefix, sep); dump(name, &screen_b);
    } else {
        snprintf(name, sizeof name, "%s.ppm", prefix); dump(name, &screen_a);
    }
}

int main(int argc, char **argv)
{
    stop_at = argc > 1 ? atol(argv[1]) : 1;
    const char *prefix = argc > 2 ? argv[2] : "out";
    f_prefix = prefix;
    if (getenv("FRAMES") && sscanf(getenv("FRAMES"), "%ld:%ld:%ld", &f_first, &f_last, &f_step) != 3) f_step = 0;
    if (getenv("DRAWS") && sscanf(getenv("DRAWS"), "%ld:%ld", &d_first, &d_last) != 2) d_last = -1;
    load_pack();
    if (!setjmp(done)) example_main();
    dump_both(prefix, "_");
    if (oob) fprintf(stderr, "%s: %ld out-of-bounds writes\n", prefix, oob);
    return oob != 0;
}
