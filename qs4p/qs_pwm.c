/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qs_pwm.c
 * @brief   The PWM backend: plays samples as PWM duty, fed by DMA.
 *
 * LAYER:   Backend (the bottom: the only part that touches hardware)
 * DEPENDS: qs_internal.h; pico-sdk hardware_pwm, hardware_dma, hardware_irq,
 *          hardware_gpio, hardware_clocks
 *
 * Linked only into programs that call qs_init_pwm().
 *
 * ---------------------------------------------------------------------------
 *  FROM SAMPLES TO SOUND
 * ---------------------------------------------------------------------------
 *  The audio pin switches between 0 V and 3.3 V hundreds of thousands of
 *  times a second (the carrier: 586 kHz at 8 bits). How long it stays high
 *  in each cycle is the sample: half the time is silence, more pushes the
 *  speaker out, less pulls it in. A resistor and a capacitor smooth the
 *  switching into a wave, and the amplifier makes it loud.
 *
 *  A sample (-32768..32767) becomes a duty level by adding 32768 and keeping
 *  the top 8 (or 10) bits: 0 becomes exactly half of 256 (or 1024).
 *
 * ---------------------------------------------------------------------------
 *  WHO MOVES THE SAMPLES: DMA, ON A TIMER
 * ---------------------------------------------------------------------------
 *  22,050 times a second, the next level has to be written to the PWM's
 *  compare register. Doing that in an interrupt per sample would work (test
 *  S0 does it), but it's 22,050 interrupts a second. Instead a DMA channel
 *  copies levels from a buffer to the register by itself, and a DMA PACING
 *  TIMER says when: it ticks at
 *
 *      clk_sys x X / Y        (X and Y whole numbers up to 65,535)
 *
 *  and each tick lets the DMA move one level. 22,050 / 150,000,000 isn't a
 *  fraction with numbers that small, so init searches for the closest one:
 *  7 / 47,619 gives 22,050.022 a second, one part in a million fast. The CPU
 *  only hears about it when a whole buffer is done.
 *
 *  TWO BUFFERS take turns. While the DMA plays one, the other already holds
 *  the next samples. When a buffer finishes, its interrupt points the DMA
 *  at the other one straight away, then asks the engine to refill the
 *  finished one: the refill has a whole buffer's time (11.6 ms) to happen.
 *
 *          DMA plays:   [ buffer 0 ][ buffer 1 ][ buffer 0 ][ buffer 1 ] ...
 *          CPU fills:               [ buffer 0 ][ buffer 1 ][ buffer 0 ] ...
 *
 *  The new level lands in the compare register at the PWM's next wrap, so
 *  a cycle is never cut short.
 *
 *  THE INTERRUPT LINE: the DMA has four (0 to 3). QG4P uses none, since it
 *  waits for its own DMA by polling, so QS4P takes QS_DMA_IRQ (1) and adds a
 *  SHARED handler that checks its own channel and leaves anyone else's
 *  alone. See docs/RESOURCES.md.
 *
 *  WHILE IDLE, the DMA stops and the interrupt stops firing. The PWM keeps
 *  running at 50%, so the next sound never starts with a jump.
 */
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "qs_internal.h"

#define N QS_BUFFER_SAMPLES

/* The two buffers, as the DMA writes them to the compare register: 32 bits,
 * the level in this pin's half (channel A low, B high). Writing all 32 bits
 * also sets the slice's other output, so that output is no use to anyone
 * else while sound is playing: see docs/RESOURCES.md.                     */
static uint32_t s_dma_buf[2][N];
static int16_t  s_samples[N];              /* the engine's samples, before   */

static struct {
    int       dma_chan;                    /* claimed at init                */
    int       timer;                       /* DMA pacing timer, claimed      */
    uint      slice;                       /* PWM slice of the audio pin     */
    uint      level_shift;                 /* 16 - bits                      */
    uint      half_shift;                  /* 0 for channel A, 16 for B      */
    uint8_t   pin;
    int8_t    shutdown_pin;
    volatile uint playing;                 /* which buffer the DMA is on     */
    volatile bool idle;                    /* the last refill was silence,   */
} s;                                       /* with the amp already off       */

/* Ask the engine for the next buffer of samples and turn them into levels. */
static bool refill(uint k)
{
    const bool more = qs_int_fill(s_samples, N);
    uint32_t *dst = s_dma_buf[k];
    for (uint32_t i = 0; i < N; i++) {
        uint32_t level = (uint32_t)((int32_t)s_samples[i] + 32768) >> s.level_shift;
        dst[i] = level << s.half_shift;
    }
    return more;
}

/*
 * A buffer has finished. This line may be shared, so first check that it's
 * our channel calling, and clear its flag (only its flag).
 */
static void on_dma_irq(void)
{
    if (!dma_irqn_get_channel_status(QS_DMA_IRQ, (uint)s.dma_chan)) return;
    dma_irqn_acknowledge_channel(QS_DMA_IRQ, (uint)s.dma_chan);

    const uint done = s.playing, next = done ^ 1u;

    /* Nothing left to play, and the amp is off: stop here. The buffer that
     * would have played next is silence, so nothing is lost by skipping it.
     * (The compare register keeps the last level it was given: silence.) */
    if (s.idle && !qs_int_active()) {
        qs_int_stopped();
        return;
    }
    dma_channel_set_read_addr((uint)s.dma_chan, s_dma_buf[next], true);   /* go */
    s.playing = next;
    s.idle = !refill(done);
}

/* The engine's start(): fill both buffers and set the DMA going. */
static void pwm_start(void)
{
    refill(0);
    s.idle = !refill(1);
    s.playing = 0;
    dma_channel_set_trans_count((uint)s.dma_chan, N, false);
    dma_channel_set_read_addr((uint)s.dma_chan, s_dma_buf[0], true);
}

static void pwm_amp(bool on)
{
    if (s.shutdown_pin >= 0) gpio_put((uint)s.shutdown_pin, on);
}

static void pwm_wait(void)
{
    tight_loop_contents();                 /* the interrupt does the work    */
}

/* qs_deinit(): amp off first, then the DMA, the PWM, and the claims. */
static void pwm_end(void)
{
    pwm_amp(false);
    dma_irqn_set_channel_enabled(QS_DMA_IRQ, (uint)s.dma_chan, false);
    dma_channel_abort((uint)s.dma_chan);
    dma_irqn_acknowledge_channel(QS_DMA_IRQ, (uint)s.dma_chan);
    irq_remove_handler((uint)dma_get_irq_num(QS_DMA_IRQ), on_dma_irq);
    dma_channel_unclaim((uint)s.dma_chan);
    dma_timer_unclaim((uint)s.timer);

    pwm_set_enabled(s.slice, false);
    gpio_init(s.pin);                      /* the audio pin: a plain input   */
    /* The shutdown pin stays an output, driven low: some amp boards pull SD
     * high, which would switch the amp on the moment we let go of it.     */
}

static const qs_backend_t pwm_backend = {
    .name = "PWM", .start = pwm_start, .amp = pwm_amp, .wait = pwm_wait, .end = pwm_end,
};

/*
 * The DMA pacing timer ticks at clk_sys x X / Y, with X and Y from 1 to
 * 65,535. The sample rate is tiny next to the clock (22,050 against
 * 150,000,000), so X stays small: for each X, the best Y is the one that
 * makes clk x X / Y closest to the rate, and only the first few X keep Y
 * within 65,535 (nine of them at the defaults). Try each, keep the best.
 */
static uint32_t best_fraction(uint32_t clk, uint32_t rate, uint16_t *x_out, uint16_t *y_out)
{
    uint64_t best_err = UINT64_MAX;
    uint32_t best_x = 1, best_y = 65535;
    for (uint32_t x = 1; x <= 65535u; x++) {
        uint64_t y = ((uint64_t)clk * x + rate / 2u) / rate;     /* rounded  */
        if (y > 65535u) break;                                   /* too big  */
        if (y < x) continue;
        uint64_t got = (uint64_t)clk * x, want = (uint64_t)rate * y;
        uint64_t err = (got > want ? got - want : want - got) * 65536u / y;   /* |rate error| x 65536 */
        if (err < best_err) { best_err = err; best_x = x; best_y = (uint32_t)y; }
    }
    *x_out = (uint16_t)best_x;
    *y_out = (uint16_t)best_y;
    return (uint32_t)(((uint64_t)clk * best_x + best_y / 2u) / best_y);   /* rate we got */
}

qs_err_t qs_init_pwm(const qs_pwm_config_t *cfg)
{
    if (qs_int_ready()) return QS_ERR_BUSY;
    if (cfg == NULL || cfg->pin >= NUM_BANK0_GPIOS) return QS_ERR_ARG;
    if (cfg->shutdown_pin >= 0 &&
        (cfg->shutdown_pin >= (int)NUM_BANK0_GPIOS || cfg->shutdown_pin == (int)cfg->pin)) return QS_ERR_ARG;

    const uint32_t rate = cfg->sample_rate ? cfg->sample_rate : QS_DEFAULT_RATE;
    const uint8_t  ceiling = cfg->max_volume ? cfg->max_volume : QS_DEFAULT_MAX_VOLUME;
    const uint8_t  bits = cfg->pwm_bits ? cfg->pwm_bits : QS_DEFAULT_PWM_BITS;
    if (rate < 4000u || rate > 50000u || ceiling > 100u || (bits != 8u && bits != 10u)) return QS_ERR_ARG;

    /* 1. THE AMPLIFIER OFF, before anything else. */
    s.shutdown_pin = cfg->shutdown_pin;
    if (s.shutdown_pin >= 0) {
        gpio_init((uint)s.shutdown_pin);            /* output value 0 ...    */
        gpio_set_dir((uint)s.shutdown_pin, GPIO_OUT);   /* ... now driven    */
    }

    /* 2. A DMA CHANNEL AND A PACING TIMER, claimed so nobody else takes
     *    them. `false`: if there's none, say so instead of stopping.      */
    s.dma_chan = dma_claim_unused_channel(false);
    if (s.dma_chan < 0) return QS_ERR_NO_HARDWARE;
    s.timer = dma_claim_unused_timer(false);
    if (s.timer < 0) {
        dma_channel_unclaim((uint)s.dma_chan);
        return QS_ERR_NO_HARDWARE;
    }

    /* 3. THE PWM, counting at the full system clock, starting at 50%. The
     *    level is set before the pin is handed to the PWM, so the pin goes
     *    straight to silence.                                              */
    s.pin = cfg->pin;
    s.slice = pwm_gpio_to_slice_num(s.pin);
    s.level_shift = 16u - bits;
    s.half_shift = (pwm_gpio_to_channel(s.pin) == PWM_CHAN_B) ? 16u : 0u;
    pwm_config pc = pwm_get_default_config();
    pwm_config_set_clkdiv_int(&pc, 1);
    pwm_config_set_wrap(&pc, (uint16_t)((1u << bits) - 1u));
    pwm_init(s.slice, &pc, false);
    pwm_set_chan_level(s.slice, pwm_gpio_to_channel(s.pin), (uint16_t)(1u << (bits - 1u)));
    pwm_set_enabled(s.slice, true);
    gpio_set_function(s.pin, GPIO_FUNC_PWM);

    /* 4. THE TIMER, as close to the sample rate as its fraction allows.   */
    uint16_t x, y;
    const uint32_t actual = best_fraction(clock_get_hz(clk_sys), rate, &x, &y);
    dma_timer_set_fraction((uint)s.timer, x, y);

    /* 5. THE DMA CHANNEL: 32-bit levels from a buffer, one per timer tick,
     *    all to the same place (the compare register), N at a time.       */
    dma_channel_config dc = dma_channel_get_default_config((uint)s.dma_chan);
    channel_config_set_transfer_data_size(&dc, DMA_SIZE_32);
    channel_config_set_read_increment(&dc, true);
    channel_config_set_write_increment(&dc, false);
    channel_config_set_dreq(&dc, dma_get_timer_dreq((uint)s.timer));
    dma_channel_configure((uint)s.dma_chan, &dc, &pwm_hw->slice[s.slice].cc, s_dma_buf[0], N, false);

    /* 6. THE INTERRUPT: a shared handler on DMA line QS_DMA_IRQ.          */
    dma_irqn_set_channel_enabled(QS_DMA_IRQ, (uint)s.dma_chan, true);
    const uint irq = (uint)dma_get_irq_num(QS_DMA_IRQ);
    irq_add_shared_handler(irq, on_dma_irq, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    irq_set_enabled(irq, true);

    /* 7. Let the filter and the amp's input settle at 50% before the amp
     *    can ever be switched on.                                          */
    sleep_ms(QS_AMP_ON_MS);

    qs_int_attach(&pwm_backend, actual, ceiling);
    return QS_OK;
}
