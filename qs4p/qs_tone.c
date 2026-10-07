/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qs_tone.c
 * @brief   Tones: QuickBasic's BEEP and SOUND.
 *
 * LAYER:   Source (describes a sound; the engine plays it)
 * DEPENDS: qs_internal.h
 *
 * Linked only into programs that call qs_beep() or qs_sound().
 *
 * ---------------------------------------------------------------------------
 *  A TONE FROM A PHASE ACCUMULATOR
 * ---------------------------------------------------------------------------
 *  A tone repeats every 1/hz seconds. We keep track of where we are in that
 *  cycle with a 32-bit counter, the phase, where 0 is the start of a cycle
 *  and 2^32 would be the start of the next. Each sample moves it on by
 *
 *      step = hz x 2^32 / sample rate
 *
 *  and it wraps round to 0 by itself, because that's what a 32-bit number
 *  does when it overflows. A 440 Hz tone at 22,050 samples a second steps
 *  85,704,562 a sample, and wraps 440 times a second, exactly as it should.
 *  No division, no fractions, and no drift: the leftover fraction of a step
 *  is carried in the phase instead of being rounded away every cycle.
 *
 *  A square wave is the phase's top bit: low for the first half of the
 *  cycle, high for the second. It's the sound of the 1980s PC speaker,
 *  which could only be pushed out or let go, so square waves were all it
 *  had.
 *
 *  More waveforms (the "@" choice) arrive in a later
 *  milestone: each will be one more source function next to square(), and
 *  qs_sound() will pick one.
 */
#include "qs_internal.h"

/* Square wave: -amp for the first half of each cycle, +amp for the second. */
static void square(qs_voice_t *v, int16_t *out, uint32_t n, int32_t amp)
{
    uint32_t phase = v->phase;
    const uint32_t step = v->step;
    for (uint32_t i = 0; i < n; i++) {
        phase += step;
        out[i] = (int16_t)((phase & 0x80000000u) ? amp : -amp);
    }
    v->phase = phase;
}

qs_err_t qs_sound(uint16_t hz, uint32_t ms, uint32_t flags)
{
    if (!qs_int_ready()) return QS_ERR_NOT_READY;

    /* Below 37 Hz is QuickBasic's limit (and a small speaker's, near
     * enough). At half the sample rate and above, a wave has fewer than two
     * samples per cycle and comes out as a different, lower note: the
     * "aliasing" that makes wagon wheels turn backwards in old films.     */
    const uint32_t rate = qs_int_rate();
    if (hz < QS_MIN_HZ || 2u * (uint32_t)hz >= rate) return QS_ERR_RANGE;

    if (ms == 0) {                       /* SOUND f, 0: stop what's playing */
        qs_stop();
        return QS_OK;
    }

    const uint64_t samples = (uint64_t)ms * rate / 1000u;
    const qs_voice_t v = {
        .source    = square,
        .remaining = (samples > 0xFFFFFFFFu) ? 0xFFFFFFFFu : (uint32_t)samples,
        .phase     = 0,
        .step      = (uint32_t)(((uint64_t)hz << 32) / rate),
    };
    qs_int_play(&v, flags);
    return QS_OK;
}

void qs_beep(void)
{
    qs_sound(800, 250, QS_FG);           /* QuickBasic's BEEP: 800 Hz, 1/4 s */
}
