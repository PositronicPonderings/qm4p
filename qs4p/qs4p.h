/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qs4p.h
 * @brief   QS4P, QuickSound 4 Pico: the one header a program includes.
 *
 * In development (QS4P 0.1.0): beeps and tones through PWM. More to come.
 *
 *     #include "qs4p.h"
 *
 *     static const qs_pwm_config_t cfg = {
 *         .pin = 2, .shutdown_pin = 3, .sample_rate = 0,   // 0: the defaults
 *         .max_volume = 75, .pwm_bits = 8,
 *     };
 *     qs_init_pwm(&cfg);
 *     qs_beep();                                    // BEEP
 *     qs_sound(440, QS_TICKS(9), QS_FG);            // SOUND 440, 9
 *
 * ---------------------------------------------------------------------------
 *  HOW IT FITS TOGETHER
 * ---------------------------------------------------------------------------
 *
 *   your program       qs_beep, qs_sound, qs_stop, qs_set_volume ...
 *        |
 *   sources            qs_tone.c: tones, made from whole-number arithmetic
 *        |
 *   engine             qs_engine.c: what's playing, volume, fades, and when
 *        |             the amplifier is on; fills buffers of samples
 *   backend            qs_pwm.c: turns samples into PWM duty, paced by DMA
 *
 * Each layer knows nothing about the ones above it. A sample is a signed
 * 16-bit number: 0 is silence, +32767 and -32768 are the speaker pushed as
 * far out and in as it goes.
 *
 * PAY FOR WHAT YOU USE: each part is its own .c file, and the linker leaves
 * out a file nothing calls. A program that never calls qs_sound() or
 * qs_beep() doesn't carry the tone code; one that never calls
 * qs_init_pwm() doesn't carry the PWM backend.
 *
 * ONE CORE: call every qs_ function from the core that called
 * qs_init_pwm(). The refill runs in that core's DMA interrupt.
 *
 * QS4P needs only the Pico SDK: nothing from QG4P or QA4P.
 */
#ifndef QS4P_H
#define QS4P_H

#include <stdbool.h>
#include <stdint.h>
#include "qs_config.h"

/** Every function that can fail returns one of these. 0 always means OK. */
typedef enum {
    QS_OK              =  0,
    QS_ERR_ARG         = -1,  /**< a bad setting in the config                     */
    QS_ERR_RANGE       = -2,  /**< a frequency qs_sound() can't play               */
    QS_ERR_NOT_READY   = -3,  /**< qs_init_pwm() hasn't succeeded yet              */
    QS_ERR_BUSY        = -4,  /**< already initialised: qs_deinit() first          */
    QS_ERR_NO_HARDWARE = -5,  /**< no free DMA channel or DMA pacing timer         */
} qs_err_t;

/**
 * How the sound hardware is wired. Fill one in and pass it to qs_init_pwm().
 * A 0 in sample_rate, max_volume or pwm_bits means "the default" from
 * qs_config.h.
 */
typedef struct {
    uint8_t  pin;           /**< audio PWM pin: GP2 in this project              */
    int8_t   shutdown_pin;  /**< amplifier SD pin (high = on), or -1 if none     */
    uint32_t sample_rate;   /**< samples a second, 4000..50000; 0 = QS_DEFAULT_RATE */
    uint8_t  max_volume;    /**< volume ceiling, 1..100 %; 0 = QS_DEFAULT_MAX_VOLUME */
    uint8_t  pwm_bits;      /**< 8 or 10; 0 = QS_DEFAULT_PWM_BITS                */
} qs_pwm_config_t;

/* -------------------------------------------------------------------------- */
/*  Starting and stopping                                                     */
/* -------------------------------------------------------------------------- */

/**
 * Start sound through PWM on one pin, with an amplifier behind a low-pass
 * filter (see tests/hardware/test_s0.c for the wiring).
 *
 * In order: the amplifier's shutdown pin goes low (some amp boards hold it
 * high until told, and an amp that's on while the pin settles plays the
 * settling); the PWM starts at 50%, which is silence; then it waits
 * QS_AMP_ON_MS (20 ms) so the filter and the amp's input settle before the
 * amp is ever switched on. A DMA channel and a DMA pacing timer are claimed,
 * and a shared handler is added to DMA interrupt line QS_DMA_IRQ. Nothing
 * runs until the first sound.
 *
 * @return QS_OK; QS_ERR_ARG for a bad config (NULL, a pin out of range, the
 *         shutdown pin the same as the audio pin, a rate, ceiling or
 *         resolution out of range); QS_ERR_BUSY if already initialised;
 *         QS_ERR_NO_HARDWARE if no DMA channel or timer is free (then
 *         nothing has been claimed, and the amp stays off).
 */
qs_err_t qs_init_pwm(const qs_pwm_config_t *cfg);

/**
 * Stop all sound and hand everything back: amp off first, then the DMA and
 * the PWM, then the DMA channel, the timer and the interrupt handler.
 * Afterwards qs_init_pwm() may be called again. Does nothing if not
 * initialised.
 */
void qs_deinit(void);

/* -------------------------------------------------------------------------- */
/*  Playing (QuickBasic: BEEP, SOUND)                                         */
/* -------------------------------------------------------------------------- */

/** qs_sound(): return as soon as the sound has finished (the default). */
#define QS_FG  0u
/** qs_sound(): return at once, and let the sound play in the background. */
#define QS_BG  (1u << 0)

/**
 * QuickBasic's clock ticks (18.2 a second) to milliseconds, rounded:
 * QS_TICKS(18) is 989, QS_TICKS(1) is 55. SOUND's durations were in ticks,
 * so old programs carry over: SOUND 440, 9 is qs_sound(440, QS_TICKS(9), QS_FG).
 * Good up to 429,496 ticks (6.5 hours), far past QuickBasic's 65,535.
 */
#define QS_TICKS(n)  ((((uint32_t)(n)) * 10000u + 91u) / 182u)

/**
 * BEEP: an 800 Hz tone for 250 ms, and wait for it to finish. Stops anything
 * playing first. Does nothing if not initialised.
 */
void qs_beep(void);

/**
 * SOUND: a tone of `hz` for `ms` milliseconds. Starting a sound stops the
 * one playing. (QS4P 0.1.0 plays square waves; more waveforms are coming.)
 *
 * @param hz     QS_MIN_HZ (37, QuickBasic's lowest) up to, but not including,
 *               half the sample rate (11,025 at 22,050 samples a second)
 * @param ms     how long; 0 stops the sound playing (as SOUND f, 0 did)
 * @param flags  QS_FG: return when the sound has finished (as qs_wait);
 *               QS_BG: return at once while it plays
 * @return QS_OK; QS_ERR_RANGE for a frequency outside the range above (then
 *         nothing changes, not even with ms = 0); QS_ERR_NOT_READY before
 *         qs_init_pwm().
 *
 * After a silence the first sound starts about QS_AMP_ON_MS (20 ms) late,
 * while the amplifier switches on; sounds that follow each other closely
 * start at once.
 */
qs_err_t qs_sound(uint16_t hz, uint32_t ms, uint32_t flags);

/**
 * True from the moment a sound is started until its last sample (or the
 * last of qs_stop()'s fade) has been handed to the hardware. The speaker is
 * up to two buffers behind (23 ms at the defaults), which is what lets the
 * next note follow on closely. False before init.
 */
bool qs_busy(void);

/** Wait until qs_busy() is false. Returns at once if not initialised. */
void qs_wait(void);

/**
 * Stop the sound playing, now. It fades out over QS_FADE_MS (5 ms) instead
 * of being cut, since cutting a wave mid-swing clicks. The fade starts with
 * the next buffer to be filled, so the speaker goes quiet within about
 * QS_FADE_MS plus two buffers (28 ms at the defaults). Returns at once;
 * qs_busy() turns false once the fade has been handed to the hardware.
 * Does nothing if nothing is playing, or before init.
 */
void qs_stop(void);

/* -------------------------------------------------------------------------- */
/*  Volume                                                                    */
/* -------------------------------------------------------------------------- */

/**
 * Volume, 0..100 percent (more than 100 counts as 100). 100 plays at the
 * ceiling set by the config's max_volume, not at the hardware's loudest:
 * the ceiling is there to protect small speakers. Takes effect from the next
 * buffer, also on a sound already playing. 100 after init. Does nothing
 * before init.
 */
void qs_set_volume(uint8_t percent);

/** The volume set with qs_set_volume(): 0..100. 0 before init. */
uint8_t qs_get_volume(void);

/** A short, plain description of an error code, for printing. */
const char *qs_err_str(qs_err_t err);

#endif /* QS4P_H */
