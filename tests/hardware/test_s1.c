/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    test_s1.c
 * @brief   Hardware test S1: QS4P's first sounds, BEEP and SOUND, with the
 *          screens busy.
 *
 * WHAT IT CHECKS
 *   The sound library on real hardware: BEEP, SOUND with QuickBasic's clock
 *   ticks, a tune playing in the background while QG4P draws on both
 *   screens, stopping a sound partway, the volume steps, and the amplifier
 *   switching itself off when nothing is playing. S0 checked the wiring
 *   with no library; this checks the library on the same wiring.
 *
 * HARDWARE (the pins and sound settings are in test_board.h)
 *
 *   | Part                       | Connection                                 |
 *   |----------------------------|--------------------------------------------|
 *   | PAM8302 mono class-D amp   | VIN -> VSYS (pin 39), GND -> GND (pin 38)  |
 *   | Audio signal               | GP2 (pin 4) -> 4.7 kOhm -> amp A+;         |
 *   |                            | 1 kOhm from A+ to GND, and a capacitor     |
 *   |                            | from A+ to GND (10 to 22 nF);              |
 *   |                            | amp A- -> GND                              |
 *   | Amp shutdown               | GP3 (pin 5) -> amp SD. High = on, low = off|
 *   | Speaker                    | 1 W, 8 Ohm, on the amp's two output        |
 *   |                            | terminals                                  |
 *   | Screens                    | both, as for the graphics tests            |
 *
 *   THE AMPLIFIER'S OUTPUTS ARE BRIDGED. Each speaker terminal is driven,
 *   in opposite directions, and neither is ground. Never connect either one
 *   to GND (or to a scope probe's ground clip): that shorts an output stage.
 *   A USB serial terminal shows the steps.
 *
 * BEFORE YOU RUN IT
 *   Put S0's answers in test_board.h: AUDIO_PWM_BITS (8 or 10, whichever
 *   was cleaner in S0's steps 2 and 3) and AUDIO_MAX_VOLUME (the loudest
 *   clean step of S0's step 6).
 *
 * WHAT TO LISTEN FOR, STEP BY STEP (then the steps repeat)
 *   1  BEEP        three BEEPs, a quarter of a second apart: three crisp
 *                  beeps, like S0's step 5. Between them the amp switches
 *                  off and on again, so listen for pops too.
 *   2  Scale       C major up and down, each note SOUND f, 4 (four clock
 *                  ticks, 220 ms) in the foreground: even notes, no clicks.
 *   3  Background  a 4 s tune while both screens draw as fast as they can.
 *                  Each note is started by the clock, half a second after
 *                  the last, and is long enough to run into the next, which
 *                  replaces it. So the tune has no gaps, and any crackle,
 *                  wobble or stutter you hear is the drawing disturbing the
 *                  sound. Listen to this one closely. (The notes may change
 *                  a frame early or late: that's the program noticing the
 *                  time between frames, not the sound.)
 *   4  Stop        a 5 s tone, stopped by qs_stop() after 1.5 s: a quick
 *                  fade, no click.
 *   5  Volume      1 kHz at 25, 50, 75 and 100% of the ceiling: four steps
 *                  up, the last as loud as S0's step 6 said was clean. If it
 *                  rattles here, lower AUDIO_MAX_VOLUME.
 *   6  Idle        3 s with nothing playing: the amp is off, so not even
 *                  hiss (compare S0's step 1).
 *
 * The amplifier switches itself: on, in silence, 20 ms before a sound; off
 * 100 ms after the last one. Nothing in this test switches it by hand.
 *
 * Program: qs4p_test_s1 (build/tests/hardware/qs4p_test_s1.uf2).
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "qg4p.h"
#include "qs4p.h"
#include "test_board.h"
#include "test_setup.h"
#define TEST_TAG "S1"
#include "test_log.h"

static qg_font_t f_title = QG_FONT_INIT(qg_font_sans_bold_24, QG_YELLOW, 1);
static qg_font_t f_body  = QG_FONT_INIT(qg_font_sans_16,      QG_DEFAULT, 1);

static qg_screen_t *const screens[2] = { &scr_a, &scr_b };

static uint32_t rng_state = 1987;
static uint32_t random_below(uint32_t n)
{
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) % n;
}

static uint32_t now_ms(void) { return to_ms_since_boot(get_absolute_time()); }

/* Both screens: black, the step's name, and a line about it. */
static void show(const char *title, const char *line)
{
    for (int i = 0; i < 2; i++) {
        qg_screen_t *s = screens[i];
        qg_cls(s, QG_BLACK);
        qg_screen_set_font(s, 0, &f_title);
        qg_print_align(s, 20, title, QG_ALIGN_CENTER);
        qg_screen_set_font(s, 0, &f_body);
        qg_print_align(s, 60, line, QG_ALIGN_CENTER);
    }
}

/* Stop here if the sound didn't start: the rest would be silent anyway. */
static void halt(const char *what, qs_err_t err)
{
    TEST_LOG("%s failed: %s: check the wiring and test_board.h", what, qs_err_str(err));
    while (true) tight_loop_contents();
}

/* --- Step 3's busy screens: boxes of random size and colour, every frame. */
static void draw_frame(qg_screen_t *s)
{
    const int16_t w = qg_screen_width(s), h = qg_screen_height(s);
    for (int k = 0; k < 6; k++) {
        const int16_t bw = (int16_t)(20 + random_below(60)), bh = (int16_t)(20 + random_below(60));
        const int16_t x = (int16_t)random_below((uint32_t)(w - bw)), y = (int16_t)(90 + random_below((uint32_t)(h - 90 - bh)));
        const qg_color_t c = (qg_color_t)(1 + random_below(15));
        qg_box(s, x, y, (int16_t)(x + bw - 1), (int16_t)(y + bh - 1), c, c);
    }
}

int main(void)
{
    /* Sound first: qs_init_pwm() switches the amplifier off before anything
     * else, and the screens' set-up below waits up to 2 s for a terminal.  */
    static const qs_pwm_config_t sound = {
        .pin = PIN_AUDIO, .shutdown_pin = PIN_AMP_SD, .sample_rate = 0,
        .max_volume = AUDIO_MAX_VOLUME, .pwm_bits = AUDIO_PWM_BITS,
    };
    const qs_err_t err = qs_init_pwm(&sound);

    test_setup(TEST_TAG, "qs4p_test_s1: BEEP, SOUND, a tune under busy screens, stop, volume");
    if (err != QS_OK) halt("qs_init_pwm", err);
    TEST_LOG("Sound: GP%d, amp SD on GP%d, %d-bit PWM, ceiling %d%%",
             PIN_AUDIO, PIN_AMP_SD, AUDIO_PWM_BITS, AUDIO_MAX_VOLUME);

    for (int pass = 0; ; pass++) {
        if (pass > 0) TEST_REPEAT();

        /* --- 1 ------------------------------------------------------------ */
        TEST_STEP(1, 6, "BEEP", "qs_beep() three times, 1/4 s apart", "three crisp beeps, no pops");
        show("BEEP", "three times");
        for (int k = 0; k < 3; k++) {
            const uint32_t t = now_ms();
            qs_beep();
            TEST_DETAIL("BEEP %d returned after %lu ms", k + 1, (unsigned long)(now_ms() - t));
            sleep_ms(250);
        }
        sleep_ms(500);

        /* --- 2 ------------------------------------------------------------ */
        TEST_STEP(2, 6, "Scale", "C major up and down, SOUND f, 4 each", "even notes, no clicks");
        show("Scale", "SOUND f, 4");
        static const uint16_t scale[] = { 262, 294, 330, 349, 392, 440, 494, 523,
                                          494, 440, 392, 349, 330, 294, 262 };
        for (unsigned k = 0; k < sizeof scale / sizeof scale[0]; k++) {
            qs_sound(scale[k], QS_TICKS(4), QS_FG);       /* SOUND f, 4         */
        }
        sleep_ms(500);

        /* --- 3 ------------------------------------------------------------ */
        TEST_STEP(3, 6, "Background", "a 4 s tune while both screens draw", "no crackle or stutter");
        show("Background", "a tune under the drawing");
        static const uint16_t tune[] = { 523, 659, 784, 1047, 784, 659, 523, 392 };
        const unsigned notes = sizeof tune / sizeof tune[0];
        uint32_t frames = 0, longest = 0, start = now_ms();
        unsigned next = 0;
        while (next < notes || qs_busy()) {
            const uint32_t t = now_ms();
            if (next < notes && t - start >= 500u * next) {
                /* Long enough to run into the next note, except the last. */
                qs_sound(tune[next], (next + 1 < notes) ? 700u : 500u, QS_BG);
                next++;
            }
            draw_frame(&scr_a);
            draw_frame(&scr_b);
            frames++;
            if (now_ms() - t > longest) longest = now_ms() - t;
        }
        TEST_DETAIL("%lu frames on both screens, the longest %lu ms",
                    (unsigned long)frames, (unsigned long)longest);
        sleep_ms(500);

        /* --- 4 ------------------------------------------------------------ */
        TEST_STEP(4, 6, "Stop", "a 5 s tone, qs_stop() after 1.5 s", "a quick fade, no click");
        show("Stop", "qs_stop() after 1.5 s");
        qs_sound(440, 5000, QS_BG);
        sleep_ms(1500);
        const uint64_t t_stop = time_us_64();
        qs_stop();
        qs_wait();
        TEST_DETAIL("not busy %lu us after qs_stop(); the speaker is up to 23 ms behind",
                    (unsigned long)(time_us_64() - t_stop));
        sleep_ms(500);

        /* --- 5 ------------------------------------------------------------ */
        TEST_STEP(5, 6, "Volume", "1 kHz at 25, 50, 75, 100% of ceiling", "four steps up, no rattle");
        show("Volume", "25, 50, 75, 100%");
        for (int v = 25; v <= 100; v += 25) {
            qs_set_volume((uint8_t)v);
            TEST_DETAIL("%d%% of the ceiling: %d%% of full scale", qs_get_volume(), v * AUDIO_MAX_VOLUME / 100);
            qs_sound(1000, 1000, QS_FG);
            sleep_ms(300);
        }
        qs_set_volume(100);

        /* --- 6 ------------------------------------------------------------ */
        TEST_STEP(6, 6, "Idle", "3 s with nothing playing, the amp off", "silence: not even hiss");
        show("Idle", "the amp is off");
        sleep_ms(QS_AMP_TAIL_MS + 50u);    /* the tail after step 5's last tone */
        TEST_DETAIL("amp %s, sound %s", gpio_get(PIN_AMP_SD) ? "ON (it should be off)" : "off",
                    qs_busy() ? "busy (it should be idle)" : "idle");
        sleep_ms(3000);

        TEST_PASS_DONE();
        sleep_ms(2000);
    }
}
