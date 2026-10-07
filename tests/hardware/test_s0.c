/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    test_s0.c
 * @brief   Hardware test S0: sound bring-up, with no sound library at all.
 *
 * WHAT IT CHECKS
 *   That the amplifier is wired right and makes clean sound from PWM, and
 *   settles three things by ear before QS4P is built on them: the filter
 *   capacitor, the PWM resolution (8 or 10 bits), and the loudest volume the
 *   speaker takes without complaint. Everything here is plain Pico SDK: a
 *   PWM slice on GP2, a repeating timer that writes the next sample 22,050
 *   times a second, a sine table, and GP3 for the amplifier's shutdown pin.
 *   If this test sounds wrong, the library would too, so it starts here.
 *
 * HARDWARE (the pins are in test_board.h)
 *
 *   | Part                       | Connection                                 |
 *   |----------------------------|--------------------------------------------|
 *   | PAM8302 mono class-D amp   | VIN -> VSYS (pin 39), GND -> GND (pin 38)  |
 *   | Audio signal               | GP2 (pin 4) -> 1 kOhm -> amp A+;           |
 *   |                            | capacitor from A+ to GND (10 to 22 nF);    |
 *   |                            | amp A- -> GND                              |
 *   | Amp shutdown               | GP3 (pin 5) -> amp SD. High = on, low = off|
 *   | Speaker                    | 1 W, 8 Ohm, on the amp's two output        |
 *   |                            | terminals                                  |
 *
 *   THE AMPLIFIER'S OUTPUTS ARE BRIDGED. Each speaker terminal is driven,
 *   in opposite directions, and neither is ground. Never connect either one
 *   to GND (or to a scope probe's ground clip): that shorts an output stage.
 *   A USB serial terminal shows the steps.
 *
 * HOW PWM BECOMES SOUND
 *   GP2 switches between 0 V and 3.3 V hundreds of thousands of times a
 *   second (the "carrier"). The fraction of each cycle it spends high is the
 *   sample: half high is silence, more is "push the speaker out", less is
 *   "pull it in". The 1 kOhm resistor and the capacitor are a low-pass
 *   filter that smooths the switching into the wave the amplifier hears.
 *
 * CHANGING THE CAPACITOR
 *   The filter's corner is 1 / (2 x pi x R x C): 10 nF gives about 16 kHz,
 *   22 nF about 7 kHz. A bigger capacitor removes more of the carrier (less
 *   whine and hiss) and also more of the top end (duller high notes and a
 *   quieter top of the sweep). Unplug the Pico first, swap the capacitor
 *   between A+ and GND, and run this test again; the steps are made for
 *   comparing.
 *
 * WHAT TO LISTEN FOR, STEP BY STEP (then the steps repeat)
 *   The amplifier is switched on and off around every step with the
 *   pop-free sequence below, so listen for pops at the start and end too.
 *   1  Silence   the amp on for 2 s with the PWM at 50%: silence. Hiss is
 *                normal for a cheap amp at a low level; a high whine means
 *                the carrier is leaking through (try the bigger capacitor).
 *   2  8-bit     a 1 kHz sine for 2 s, PWM wrap 255: a 586 kHz carrier.
 *   3  10-bit    the same at wrap 1023: a 146 kHz carrier. Finer steps, but
 *                the carrier is closer to hearing range. Which was cleaner?
 *   4  Sweep     100 Hz to 8 kHz over 4 s, at SWEEP_BITS (below). Note
 *                where it goes quiet (the filter), buzzy, or rattly (the
 *                speaker).
 *   5  Beep      an 800 Hz square wave for 250 ms, three times: QuickBasic's
 *                BEEP.
 *   6  Volume    1 kHz at 25%, 50%, 75% and 100% of full scale, 1 s each.
 *                The loudest step with no distortion or rattle becomes the
 *                library's volume ceiling (AUDIO_MAX_VOLUME in test_board.h).
 *
 * POP AVOIDANCE
 *   On:  SD low, start the PWM at 50% (silence), wait 20 ms for the filter
 *        and the amp's input to settle, then SD high.
 *   Off: SD low first, then the PWM off.
 *   Switch the amp while its input is moving and the speaker hears the jump.
 *
 * Program: qs4p_test_s0 (build/tests/hardware/qs4p_test_s0.uf2).
 */
#include <math.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "test_board.h"
#define TEST_TAG "S0"
#include "test_log.h"

#define SWEEP_BITS   8          /* step 4 uses the cleaner of steps 2 and 3: 8 or 10 */
#define SAMPLE_RATE  22050      /* samples a second                                  */
#define AMP_WAIT_MS  20         /* PWM at 50% this long before the amp comes on      */

enum { SINE, SQUARE };

static int16_t           sine[256];          /* one cycle, full scale           */
static volatile uint32_t phase, phase_inc;   /* where we are in the wave, 2^32 = one cycle */
static volatile int32_t  amplitude;          /* 0 (silent) .. 32767 (full scale)*/
static volatile int      wave = SINE;
static uint              slice, channel, shift;
static uint32_t          frac;               /* see on_sample()                 */
static repeating_timer_t timer;

/*
 * THE SAMPLE CLOCK: called 22,050 times a second, it works out the next
 * sample and sets the PWM duty to match.
 *
 * Samples as signed 16-bit numbers (-32768..32767) become duty levels by
 * adding 32768 (0..65535) and keeping the top `bits` bits: 0 becomes
 * exactly half, which is silence. The new level takes effect at the PWM's
 * next wrap, so it never cuts a cycle short.
 *
 * One sample lasts 1,000,000 / 22,050 = 45.35 microseconds, but the timer
 * counts whole microseconds. So most periods are 45 and some are 46: frac
 * collects the leftover 7,750/22,050 of a microsecond each time, and each
 * time it passes a whole one, the next period is 46. On average, exactly
 * 22,050 a second. (A negative delay means "after the previous start", so
 * the time the callback itself takes doesn't add up.)
 */
static bool on_sample(repeating_timer_t *t)
{
    uint32_t p = phase += phase_inc;
    int32_t  s = (wave == SQUARE) ? ((p & 0x80000000u) ? 32767 : -32767) : sine[p >> 24];
    s = s * amplitude / 32768;
    pwm_set_chan_level(slice, channel, (uint16_t)((uint32_t)(s + 32768) >> shift));

    frac += 1000000 % SAMPLE_RATE;
    t->delay_us = -45;
    if (frac >= SAMPLE_RATE) { frac -= SAMPLE_RATE; t->delay_us = -46; }
    return true;
}

/* Frequency in Hz -> how far through a cycle each sample moves, out of 2^32. */
static void set_tone(uint32_t hz)
{
    phase_inc = (uint32_t)(((uint64_t)hz << 32) / SAMPLE_RATE);
}

/* PWM at 50%, wait, amp on; then start the sample clock. */
static void amp_on(int bits)
{
    gpio_put(PIN_AMP_SD, 0);                       /* the amp stays off ...      */
    slice   = pwm_gpio_to_slice_num(PIN_AUDIO);
    channel = pwm_gpio_to_channel(PIN_AUDIO);
    shift   = 16u - (uint)bits;

    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv_int(&cfg, 1);            /* count at the full 150 MHz  */
    pwm_config_set_wrap(&cfg, (uint16_t)((1u << bits) - 1u));
    pwm_init(slice, &cfg, false);
    pwm_set_chan_level(slice, channel, (uint16_t)(1u << (bits - 1)));   /* 50% */
    pwm_set_enabled(slice, true);
    gpio_set_function(PIN_AUDIO, GPIO_FUNC_PWM);   /* ... while its input settles */
    sleep_ms(AMP_WAIT_MS);
    gpio_put(PIN_AMP_SD, 1);

    amplitude = 0;
    frac = 0;
    add_repeating_timer_us(-45, on_sample, NULL, &timer);
    TEST_DETAIL("PWM %d-bit: wrap %u, carrier %lu Hz", bits, (1u << bits) - 1u,
                (unsigned long)(clock_get_hz(clk_sys) >> bits));
}

/* Amp off first, then the PWM, then the pin back to a plain input. */
static void amp_off(void)
{
    amplitude = 0;
    gpio_put(PIN_AMP_SD, 0);
    cancel_repeating_timer(&timer);
    pwm_set_enabled(slice, false);
    gpio_init(PIN_AUDIO);
}

static void play(int w, uint32_t hz, int percent, uint32_t ms)
{
    wave = w;
    set_tone(hz);
    amplitude = 32767 * percent / 100;
    sleep_ms(ms);
    amplitude = 0;
}

int main(void)
{
    /* The amp first, before anything else: some amp boards hold SD high
     * until told otherwise, and an amp that's on while GP2 floats at start-up
     * plays whatever the pin picks up.                                       */
    gpio_init(PIN_AMP_SD);
    gpio_set_dir(PIN_AMP_SD, GPIO_OUT);
    gpio_put(PIN_AMP_SD, 0);

    stdio_init_all();
    for (int i = 0; i < 256; i++) {
        sine[i] = (int16_t)lroundf(32767.0f * sinf((float)i * 6.2831853f / 256.0f));
    }
    printf("\n");
    TEST_LOG("qs4p_test_s0: PWM sound on GP2 with no library: tones, sweep, beep, volume");
    TEST_LOG("Amp: SD on GP%d, audio on GP%d at %d samples a second", PIN_AMP_SD, PIN_AUDIO, SAMPLE_RATE);

    for (int pass = 0; ; pass++) {
        if (pass > 0) TEST_REPEAT();

        TEST_STEP(1, 6, "Silence", "amp on, PWM at 50%, 2 s", "no whine (carrier); a little hiss is fine");
        amp_on(8);
        sleep_ms(2000);
        amp_off();
        sleep_ms(500);

        TEST_STEP(2, 6, "8-bit", "1 kHz sine, 586 kHz carrier, 2 s", "a clean, steady tone");
        amp_on(8);
        play(SINE, 1000, 100, 2000);
        amp_off();
        sleep_ms(500);

        TEST_STEP(3, 6, "10-bit", "1 kHz sine, 146 kHz carrier, 2 s", "which was cleaner, step 2 or 3?");
        amp_on(10);
        play(SINE, 1000, 100, 2000);
        amp_off();
        sleep_ms(500);

        TEST_STEP(4, 6, "Sweep", "100 Hz to 8 kHz over 4 s", "where it goes quiet, buzzy or rattly");
        amp_on(SWEEP_BITS);
        wave = SINE;
        amplitude = 32767;
        for (int ms = 0; ms <= 4000; ms++) {       /* even steps on a log scale */
            set_tone((uint32_t)(100.0f * powf(80.0f, (float)ms / 4000.0f)));
            sleep_ms(1);
        }
        amplitude = 0;
        amp_off();
        sleep_ms(500);

        TEST_STEP(5, 6, "Beep", "800 Hz square, 250 ms, three times", "three crisp beeps, QB's BEEP");
        amp_on(SWEEP_BITS);
        for (int k = 0; k < 3; k++) {
            play(SQUARE, 800, 100, 250);
            sleep_ms(250);
        }
        amp_off();
        sleep_ms(500);

        TEST_STEP(6, 6, "Volume", "1 kHz at 25, 50, 75, 100%, 1 s each", "the loudest step with no rattle");
        amp_on(SWEEP_BITS);
        for (int percent = 25; percent <= 100; percent += 25) {
            TEST_DETAIL("%d%%", percent);
            play(SINE, 1000, percent, 1000);
            sleep_ms(300);
        }
        amp_off();

        TEST_PASS_DONE();
        sleep_ms(2000);
    }
}
