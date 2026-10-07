/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qs_config.h
 * @brief   Compile-time settings for QS4P.
 *
 * LAYER:   Configuration (read by every other part)
 * DEPENDS: nothing
 *
 * Every value here is a knob you may want to turn without touching the
 * library's code. Each is wrapped in #ifndef, so you can also set it from
 * CMake, for example:
 *
 *     target_compile_definitions(qs4p PUBLIC QS_BUFFER_SAMPLES=128)
 */
#ifndef QS_CONFIG_H
#define QS_CONFIG_H

/**
 * Samples per second, when the config passed to qs_init_pwm() says 0.
 * 22,050 is half a CD's rate: plenty for beeps, tunes and sound effects,
 * and it keeps the work per second small.
 */
#ifndef QS_DEFAULT_RATE
#define QS_DEFAULT_RATE 22050u
#endif

/**
 * PWM resolution in bits, when the config says 0: 8 or 10.
 *
 * The PWM counter runs at the full system clock (150 MHz), so the number of
 * steps decides the carrier, the rate the pin switches at:
 *   8 bits:  256 steps,  150 MHz / 256  = 586 kHz
 *   10 bits: 1024 steps, 150 MHz / 1024 = 146 kHz
 * More steps mean finer sound, but a carrier closer to what the filter and
 * the amplifier have to throw away. Hardware test S0 compares the two by ear.
 */
#ifndef QS_DEFAULT_PWM_BITS
#define QS_DEFAULT_PWM_BITS 8
#endif

/**
 * Volume ceiling in percent, when the config says 0. Full volume
 * (qs_set_volume(100)) plays at this fraction of the loudest the hardware can
 * do. A 1 W speaker on a class-D amp running from 5 V can be asked for more
 * than it can take; 75% amplitude is about 56% of the power. Hardware test S0
 * finds the loudest level your speaker takes cleanly.
 */
#ifndef QS_DEFAULT_MAX_VOLUME
#define QS_DEFAULT_MAX_VOLUME 75
#endif

/**
 * Samples in each of the two buffers the PWM backend plays from. While one
 * plays, the other is refilled, so a refill has one buffer's time to finish:
 * 256 samples at 22,050 a second is 11.6 ms. Smaller buffers react to
 * qs_stop() and new sounds sooner and use less RAM (4 bytes per sample per
 * buffer, plus 2 for the work buffer: 2.5 KB at 256), at the price of more
 * interrupts a second.
 */
#ifndef QS_BUFFER_SAMPLES
#define QS_BUFFER_SAMPLES 256
#endif

/**
 * How many sounds can play at once. 1 for now: starting a sound stops the
 * one playing. (Mixing several voices arrives in a later milestone; the
 * engine is laid out for it.)
 */
#ifndef QS_VOICES
#define QS_VOICES 1
#endif

/**
 * Which of the DMA's interrupt lines the PWM backend uses: 0 to 3 on the
 * RP2350. QG4P uses none (it waits for its DMA by polling), and many SDK
 * examples and libraries default to line 0, so QS4P takes line 1. It adds a
 * SHARED handler, which checks its own channel and leaves the rest alone,
 * so anything else may use line 1 as well.
 */
#ifndef QS_DMA_IRQ
#define QS_DMA_IRQ 1
#endif

/**
 * Amplifier switching, to keep the speaker from popping.
 *
 * QS_AMP_ON_MS    silence, with the PWM at 50%, before a sound when the amp
 *                 was off: the amp is switched on during it, so it has
 *                 settled by the time the sound starts. qs_init_pwm() also
 *                 waits this long with the PWM at 50% before returning.
 * QS_AMP_TAIL_MS  silence after the last sound before the amp is switched
 *                 off again (and the DMA stopped, so nothing runs while
 *                 idle). A new sound in the meantime keeps it on. At least
 *                 two buffers' worth is used, whatever this says, so the amp
 *                 is never switched off under sound still waiting to play.
 */
#ifndef QS_AMP_ON_MS
#define QS_AMP_ON_MS 20u
#endif
#ifndef QS_AMP_TAIL_MS
#define QS_AMP_TAIL_MS 100u
#endif

/**
 * qs_stop() fades the sound out over this long instead of cutting it, since
 * a wave cut off mid-swing is a click.
 */
#ifndef QS_FADE_MS
#define QS_FADE_MS 5u
#endif

/** The lowest frequency qs_sound() plays, as in QuickBasic's SOUND. */
#ifndef QS_MIN_HZ
#define QS_MIN_HZ 37u
#endif

#endif /* QS_CONFIG_H */
