/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/*
 * test_qs_pwm.c - QS4P's PWM backend (qs_pwm.c), on a PC.
 *
 * The backend runs unchanged against a pretend Pico SDK: every SDK function
 * it calls is written here, and each one writes a line to a log
 * ("gpio_put 3 1") instead of touching hardware. The tests then read the
 * log: is the amplifier switched off before anything else, does the PWM sit
 * at 50% before the pin is handed to it, is the right interrupt line shared,
 * is everything handed back by qs_deinit()?
 *
 * The DMA is pretend too: "playing" a buffer reads its 256 words from where
 * the backend pointed the channel, then calls the backend's interrupt
 * handler, as the real DMA would when the buffer is done.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "qs_internal.h"

#define N QS_BUFFER_SAMPLES

static int failures;
#define CHECK(cond, ...)  do { if (!(cond)) { printf("  FAIL  "); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

/* ========================================================================== */
/*  The pretend SDK                                                           */
/* ========================================================================== */

static char log_[400][64];
static int  log_n;

static void say(const char *fmt, ...)
{
    if (log_n >= 400) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(log_[log_n++], sizeof log_[0], fmt, ap);
    va_end(ap);
}

/* Where `line` first appears in the log, or -1. */
static int at(const char *line)
{
    for (int i = 0; i < log_n; i++) if (strcmp(log_[i], line) == 0) return i;
    return -1;
}
static int count(const char *line)
{
    int c = 0;
    for (int i = 0; i < log_n; i++) if (strcmp(log_[i], line) == 0) c++;
    return c;
}
static int count_prefix(const char *start)
{
    int c = 0;
    for (int i = 0; i < log_n; i++) if (strncmp(log_[i], start, strlen(start)) == 0) c++;
    return c;
}

/* What's free, and what the backend has asked for. */
static bool channel_free = true, timer_free = true;
static int  channels_claimed, timers_claimed;
#define CHAN   5                              /* the channel we hand out      */
#define TIMER  2                              /* the timer we hand out        */
#define DMA_IRQ_1_NUM 11                      /* DMA_IRQ_1 on the RP2350      */

static irq_handler_t handler;                 /* the backend's, once added    */
static bool irq_status;                       /* our channel's interrupt flag */
static const uint32_t *dma_from;              /* the buffer the DMA is on     */
static bool dma_busy;
static volatile void *dma_to;
static uint32_t dma_count, dreq_given;
static int dma_size;
static bool dma_read_inc, dma_write_inc;
static uint32_t pwm_wrap, pwm_div;

void gpio_init(uint p)                   { say("gpio_init %u", p); }
void gpio_set_dir(uint p, bool out)      { say("gpio_set_dir %u %d", p, out); }
void gpio_put(uint p, bool v)            { say("gpio_put %u %d", p, v); }
void gpio_set_function(uint p, int f)    { say("gpio_set_function %u %d", p, f); }
void sleep_ms(uint32_t ms)               { say("sleep_ms %u", ms); }
uint32_t clock_get_hz(enum clock_index c) { (void)c; return 150000000u; }

int dma_claim_unused_channel(bool required)
{
    say("dma_claim_unused_channel %d", required);
    if (!channel_free) return -1;
    channel_free = false; channels_claimed++;
    return CHAN;
}
void dma_channel_unclaim(uint c)         { say("dma_channel_unclaim %u", c); channel_free = true; channels_claimed--; }
int dma_claim_unused_timer(bool required)
{
    say("dma_claim_unused_timer %d", required);
    if (!timer_free) return -1;
    timer_free = false; timers_claimed++;
    return TIMER;
}
void dma_timer_unclaim(uint t)           { say("dma_timer_unclaim %u", t); timer_free = true; timers_claimed--; }
void dma_timer_set_fraction(uint t, uint16_t x, uint16_t y) { say("dma_timer_set_fraction %u %u %u", t, x, y); }
uint dma_get_timer_dreq(uint t)          { return 59u + t; }        /* DREQ_DMA_TIMER0 = 59 */

dma_channel_config dma_channel_get_default_config(uint c) { (void)c; return (dma_channel_config){ 0 }; }
void channel_config_set_transfer_data_size(dma_channel_config *c, enum dma_channel_transfer_size s) { (void)c; dma_size = s; }
void channel_config_set_dreq(dma_channel_config *c, uint d)          { (void)c; dreq_given = d; }
void channel_config_set_read_increment(dma_channel_config *c, bool b)  { (void)c; dma_read_inc = b; }
void channel_config_set_write_increment(dma_channel_config *c, bool b) { (void)c; dma_write_inc = b; }
void dma_channel_configure(uint c, const dma_channel_config *cfg, volatile void *to,
                           const volatile void *from, uint32_t n, bool trigger)
{
    (void)cfg; (void)from;
    say("dma_channel_configure %u %u %d", c, n, trigger);
    dma_to = to; dma_count = n;
}
void dma_channel_set_read_addr(uint c, const volatile void *from, bool trigger)
{
    say("dma_channel_set_read_addr %u %d", c, trigger);
    dma_from = (const uint32_t *)from;
    if (trigger) dma_busy = true;
}
void dma_channel_set_trans_count(uint c, uint32_t n, bool trigger) { say("dma_channel_set_trans_count %u %u %d", c, n, trigger); dma_count = n; }
void dma_channel_abort(uint c)           { say("dma_channel_abort %u", c); dma_busy = false; }
void dma_irqn_set_channel_enabled(uint n, uint c, bool e) { say("dma_irqn_set_channel_enabled %u %u %d", n, c, e); }
bool dma_irqn_get_channel_status(uint n, uint c) { (void)n; (void)c; return irq_status; }
void dma_irqn_acknowledge_channel(uint n, uint c) { say("dma_irqn_acknowledge_channel %u %u", n, c); irq_status = false; }
int dma_get_irq_num(uint n)              { return (n == 1u) ? DMA_IRQ_1_NUM : 10; }
void dma_channel_wait_for_finish_blocking(uint c) { (void)c; }

void irq_add_shared_handler(uint num, irq_handler_t h, uint8_t prio) { say("irq_add_shared_handler %u %u", num, prio); handler = h; }
void irq_remove_handler(uint num, irq_handler_t h) { say("irq_remove_handler %u", num); if (h == handler) handler = NULL; }
void irq_set_enabled(uint num, bool e)   { say("irq_set_enabled %u %d", num, e); }

static pwm_hw_t pwm_regs;
pwm_hw_t *pwm_hw = &pwm_regs;
pwm_config pwm_get_default_config(void) { return (pwm_config){ 0 }; }
void pwm_config_set_clkdiv_int(pwm_config *c, uint d) { (void)c; pwm_div = d; }
void pwm_config_set_wrap(pwm_config *c, uint16_t w)   { (void)c; pwm_wrap = w; }
void pwm_init(uint s, pwm_config *c, bool start)      { (void)c; say("pwm_init %u %d", s, start); }
uint pwm_gpio_to_slice_num(uint p)       { return (p >> 1u) & 7u; }
uint pwm_gpio_to_channel(uint p)         { return p & 1u; }
void pwm_set_chan_level(uint s, uint ch, uint16_t v) { say("pwm_set_chan_level %u %u %u", s, ch, v); }
void pwm_set_enabled(uint s, bool e)     { say("pwm_set_enabled %u %d", s, e); }
void pwm_set_gpio_level(uint p, uint16_t v) { (void)p; (void)v; }

/* ========================================================================== */
/*  Playing buffers                                                           */
/* ========================================================================== */

static uint32_t words[22050u * 4u];          /* every word the DMA wrote      */
static uint32_t words_n;

/*
 * The DMA plays the buffer it's on, then raises its interrupt. Returns
 * false if the DMA wasn't running (the backend had stopped it).
 */
static bool play_buffer(void)
{
    if (!dma_busy) return false;
    for (uint32_t i = 0; i < dma_count && words_n < sizeof words / sizeof words[0]; i++) {
        words[words_n++] = dma_from[i];
    }
    dma_busy = false;                        /* a channel stops at the end    */
    irq_status = true;
    if (handler != NULL) handler();
    return true;
}

/* Play until the backend stops the DMA (or 4 s, if it never does). */
static int play_all(void)
{
    int buffers = 0;
    while (play_buffer() && buffers < 4 * 22050 / N) buffers++;
    return buffers;
}

static void reset_log(void) { log_n = 0; }

static const qs_pwm_config_t good = { .pin = 2, .shutdown_pin = 3, .sample_rate = 0, .max_volume = 0, .pwm_bits = 0 };

/* ========================================================================== */
/*  The tests                                                                 */
/* ========================================================================== */

static void test_bad_config(void)
{
    printf("bad_config\n");
    static const qs_pwm_config_t bad[] = {
        { .pin = 48, .shutdown_pin = 3 },
        { .pin = 2,  .shutdown_pin = 2 },
        { .pin = 2,  .shutdown_pin = 48 },
        { .pin = 2,  .shutdown_pin = 3, .sample_rate = 3999 },
        { .pin = 2,  .shutdown_pin = 3, .sample_rate = 50001 },
        { .pin = 2,  .shutdown_pin = 3, .max_volume = 101 },
        { .pin = 2,  .shutdown_pin = 3, .pwm_bits = 9 },
        { .pin = 2,  .shutdown_pin = 3, .pwm_bits = 16 },
    };
    reset_log();
    CHECK(qs_init_pwm(NULL) == QS_ERR_ARG, "NULL config wasn't QS_ERR_ARG");
    for (unsigned k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        CHECK(qs_init_pwm(&bad[k]) == QS_ERR_ARG, "bad config %u wasn't QS_ERR_ARG", k);
    }
    CHECK(log_n == 0, "a bad config touched the hardware (%s)", log_n ? log_[0] : "");
}

static void test_no_hardware(void)
{
    printf("no_hardware\n");
    reset_log();
    channel_free = false;
    CHECK(qs_init_pwm(&good) == QS_ERR_NO_HARDWARE, "no DMA channel wasn't QS_ERR_NO_HARDWARE");
    CHECK(at("gpio_init 3") == 0 && at("gpio_set_dir 3 1") == 1, "the amp wasn't held off");
    CHECK(count_prefix("gpio_put 3 1") == 0, "the amp was switched on");
    channel_free = true;

    reset_log();
    timer_free = false;
    CHECK(qs_init_pwm(&good) == QS_ERR_NO_HARDWARE, "no DMA timer wasn't QS_ERR_NO_HARDWARE");
    CHECK(channels_claimed == 0, "the DMA channel was kept after a failed init");
    timer_free = true;
    CHECK(count_prefix("pwm_") == 0 && count_prefix("irq_") == 0, "a failed init went on to set things up");
    CHECK(qs_sound(440, 100, QS_FG) == QS_ERR_NOT_READY, "sound works after a failed init");
}

static void test_init(void)
{
    printf("init\n");
    reset_log();
    CHECK(qs_init_pwm(&good) == QS_OK, "init failed");

    /* 1. Amp off first of all. */
    CHECK(at("gpio_init 3") == 0 && at("gpio_set_dir 3 1") == 1,
          "the amp's pin wasn't the first thing set (log starts %s)", log_[0]);
    CHECK(count_prefix("gpio_put 3") == 0, "init switched the amp");

    /* 2. The PWM at 50% before the pin is handed to it. GP2 is slice 1 A. */
    const int pin = at("gpio_set_function 2 4");
    CHECK(pin > 0, "GP2 never handed to the PWM");
    CHECK(at("pwm_set_chan_level 1 0 128") >= 0 && at("pwm_set_chan_level 1 0 128") < pin, "50%% wasn't set first");
    CHECK(at("pwm_set_enabled 1 1") >= 0 && at("pwm_set_enabled 1 1") < pin, "the PWM wasn't running first");
    CHECK(pwm_wrap == 255 && pwm_div == 1, "PWM wrap %u, divider %u: wanted 255 and 1", pwm_wrap, pwm_div);

    /* 3. The pacing timer: 150 MHz x 7 / 47,619 = 22,050.02 a second. */
    CHECK(at("dma_timer_set_fraction 2 7 47619") >= 0, "timer fraction isn't 7/47619");
    CHECK(qs_int_rate() == 22050, "rate is %u, wanted 22050", qs_int_rate());

    /* 4. The DMA: 32-bit words, from the buffer, to slice 1's compare register. */
    CHECK(dma_size == DMA_SIZE_32 && dma_read_inc && !dma_write_inc, "DMA set up wrongly");
    CHECK(dreq_given == 59u + TIMER, "DMA paced by DREQ %u, wanted the timer's %u", dreq_given, 59u + TIMER);
    CHECK(dma_to == &pwm_hw->slice[1].cc, "DMA doesn't write slice 1's compare register");
    CHECK(at("dma_channel_configure 5 256 0") >= 0, "DMA not set for 256 words, untriggered");

    /* 5. A shared handler on DMA_IRQ_1, for our channel only. */
    CHECK(at("dma_irqn_set_channel_enabled 1 5 1") >= 0, "channel 5's interrupt not enabled on line 1");
    CHECK(at("irq_add_shared_handler 11 128") >= 0, "no shared handler on DMA_IRQ_1 at the default priority");
    CHECK(at("irq_set_enabled 11 1") >= 0, "DMA_IRQ_1 not enabled");

    /* 6. Then the wait, last of all, with the PWM already at 50%. */
    CHECK(at("sleep_ms 20") == log_n - 1, "init doesn't end by letting things settle");
    CHECK(!dma_busy, "the DMA runs before any sound");

    CHECK(qs_init_pwm(&good) == QS_ERR_BUSY, "a second init wasn't QS_ERR_BUSY");
}

static void test_play(void)
{
    printf("play\n");
    reset_log();
    words_n = 0;
    CHECK(qs_sound(440, 200, QS_BG) == QS_OK, "qs_sound failed");
    CHECK(dma_busy, "the DMA didn't start");
    CHECK(at("gpio_put 3 1") >= 0, "the amp wasn't switched on");

    const int buffers = play_all();
    CHECK(!dma_busy, "the DMA never stopped (%d buffers)", buffers);
    CHECK(count("gpio_put 3 1") == 1 && count("gpio_put 3 0") == 1, "the amp went on %d times and off %d",
          count("gpio_put 3 1"), count("gpio_put 3 0"));
    CHECK(at("gpio_put 3 0") > at("gpio_put 3 1"), "the amp went off before it went on");

    /* The levels: silence is 128, the tone 128 +- 95 (75% ceiling). */
    uint32_t silent = 0, high = 0, low = 0, other = 0;
    for (uint32_t i = 0; i < words_n; i++) {
        switch (words[i]) {
        case 128: silent++; break;
        case 223: high++;   break;           /* (+24575 + 32768) >> 8 */
        case 32:  low++;    break;           /* (-24575 + 32768) >> 8 */
        default:  other++;  break;
        }
    }
    CHECK(other == 0, "%u levels that are neither silence nor the tone", other);
    CHECK(high + low == 200u * 22050u / 1000u, "%u samples of tone, wanted %u", high + low, 200u * 22050u / 1000u);
    CHECK(words[0] == 128 && words[words_n - 1] == 128, "doesn't start and end at 50%%");
    uint32_t lead = 0;
    while (lead < words_n && words[lead] == 128) lead++;
    CHECK(lead >= 441, "the tone starts %u samples in, before the amp's 20 ms", lead);

    /* Someone else's interrupt on the same line: not ours, left alone. */
    reset_log();
    irq_status = false;
    if (handler != NULL) handler();
    CHECK(log_n == 0, "the handler acted on another channel's interrupt (%s)", log_[0]);

    /* And it starts again after stopping. (QS_BG: with a pretend DMA,
     * nothing would ever end a wait.) */
    reset_log();
    CHECK(qs_sound(800, 250, QS_BG) == QS_OK && dma_busy && at("gpio_put 3 1") >= 0, "no second start");
    play_buffer();
    play_buffer();
}

static void test_deinit(void)
{
    printf("deinit\n");
    /* The 800 Hz tone above is still playing: this is deinit mid-sound. */
    reset_log();
    qs_deinit();
    CHECK(at("gpio_put 3 0") == 0, "the amp wasn't switched off first (log starts %s)", log_n ? log_[0] : "");
    const int irq_off = at("dma_irqn_set_channel_enabled 1 5 0"), abort_ = at("dma_channel_abort 5");
    CHECK(irq_off > 0 && abort_ > irq_off, "the channel's interrupt wasn't switched off before the abort");
    CHECK(at("irq_remove_handler 11") > abort_, "handler not removed after the abort");
    CHECK(channels_claimed == 0 && timers_claimed == 0, "%d channels and %d timers still claimed",
          channels_claimed, timers_claimed);
    CHECK(at("pwm_set_enabled 1 0") >= 0 && at("gpio_init 2") > at("pwm_set_enabled 1 0"),
          "the PWM and its pin weren't handed back");
    CHECK(count_prefix("gpio_put 3 1") == 0 && count_prefix("gpio_init 3") == 0,
          "the amp's pin was let go (an amp board's pull-up would switch it on)");
    CHECK(handler == NULL && !dma_busy, "the handler or the DMA is still there");
    CHECK(qs_sound(440, 100, QS_BG) == QS_ERR_NOT_READY, "sound works after qs_deinit()");
    qs_deinit();
}

/* GP3 is slice 1's channel B: its level goes in the top half of the word. */
static void test_channel_b(void)
{
    printf("channel_b\n");
    const qs_pwm_config_t b = { .pin = 3, .shutdown_pin = -1, .pwm_bits = 10, .max_volume = 100 };
    reset_log();
    CHECK(qs_init_pwm(&b) == QS_OK, "init on GP3 failed");
    CHECK(pwm_wrap == 1023 && at("pwm_set_chan_level 1 1 512") >= 0, "10 bits: wrap %u, wanted 1023 and 50%% = 512", pwm_wrap);
    words_n = 0;
    qs_sound(1000, 50, QS_BG);
    play_all();
    uint32_t bad = 0, loud = 0;
    for (uint32_t i = 0; i < words_n; i++) {
        const uint32_t level = words[i] >> 16;
        if ((words[i] & 0xFFFFu) != 0 || level > 1023) bad++;
        if (level == 1023) loud++;           /* (32767 + 32768) >> 6 */
    }
    CHECK(bad == 0, "%u words not in channel B's half, or over 1023", bad);
    CHECK(loud > 0, "a 100%% ceiling never reached the top level");
    CHECK(count_prefix("gpio_put") == 0, "with no shutdown pin, a pin was switched");
    qs_deinit();
}

/* The timer's fraction, at other rates. */
static void test_rates(void)
{
    printf("rates\n");
    static const uint32_t rates[] = { 4000, 8000, 11025, 16000, 22050, 32000, 44100, 48000, 50000 };
    for (unsigned k = 0; k < sizeof rates / sizeof rates[0]; k++) {
        const qs_pwm_config_t c = { .pin = 2, .shutdown_pin = 3, .sample_rate = rates[k] };
        CHECK(qs_init_pwm(&c) == QS_OK, "init at %u failed", rates[k]);
        CHECK(qs_int_rate() == rates[k], "asked for %u a second, got %u", rates[k], qs_int_rate());
        qs_deinit();
    }
}

int main(void)
{
    test_bad_config();
    test_no_hardware();
    test_init();
    test_play();
    test_deinit();
    test_channel_b();
    test_rates();
    printf("%d failures\n", failures);
    return failures ? 1 : 0;
}
