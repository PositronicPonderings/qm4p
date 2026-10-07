/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qs_engine.c
 * @brief   The sound engine: what's playing, how loud, and when the
 *          amplifier is on. Fills buffers of samples for the backend.
 *
 * LAYER:   Engine (between the sources above and the backend below)
 * DEPENDS: qs_internal.h, hardware/sync.h (switching interrupts off briefly)
 *
 * ---------------------------------------------------------------------------
 *  TWO SIDES, ONE VOICE
 * ---------------------------------------------------------------------------
 *  This file is used from two places at once:
 *
 *    the program     qs_sound(), qs_stop(), qs_set_volume() ... change what
 *                    should play
 *    the interrupt   qs_int_fill(), called by the backend each time a buffer
 *                    has played, works out the next buffer from it
 *
 *  The interrupt can arrive between any two instructions of the program. So
 *  whenever the program changes the voice, it switches interrupts off for
 *  the few instructions that takes (save_and_disable_interrupts), and the
 *  interrupt never sees a half-changed sound. Flags the program waits on
 *  are `volatile`, so the compiler reads them afresh every time instead of
 *  trusting a copy it made earlier.
 *
 *  The interrupt side has rules of its own: it runs often (86 times a second
 *  at the defaults) and must be finished long before the next buffer is
 *  due, so it never prints, never waits, and uses whole numbers only (the
 *  Cortex-M33 has a floating-point unit, but saving its registers on every
 *  interrupt costs time for nothing).
 *
 * ---------------------------------------------------------------------------
 *  THE AMPLIFIER
 * ---------------------------------------------------------------------------
 *  Switching the amplifier on or off while a sound is moving the speaker
 *  makes a pop, so the amp is switched in silence, in four states:
 *
 *      OFF --a sound starts--> WAKING --QS_AMP_ON_MS--> ON
 *       ^                     (amp on, silence)           | nothing playing
 *       |                                                 v
 *       +-----QS_AMP_TAIL_MS of silence----------------- TAIL
 *                (amp off)              (a new sound: back to ON)
 *
 *  Time here is counted in samples as they're filled, which runs one or
 *  two buffers ahead of the speaker. That's why the tail is never shorter
 *  than two buffers: the amp must not go off under sound still waiting in
 *  the backend's buffers.
 */
#include <stddef.h>
#include "hardware/sync.h"
#include "qs_internal.h"

#if QS_VOICES != 1
#error "QS4P plays one voice at a time for now: QS_VOICES must be 1"
#endif

typedef enum { AMP_OFF, AMP_WAKING, AMP_ON, AMP_TAIL } amp_state_t;

static const qs_backend_t *s_be;           /* NULL until initialised           */
static uint32_t s_rate;                    /* samples a second                 */
static uint8_t  s_ceiling;                 /* volume ceiling, 1..100 %         */
static volatile uint8_t s_volume;          /* 0..100 %                         */

/* The voice. Changed by the program only with interrupts off.              */
static qs_voice_t s_voice;
static volatile bool s_sounding;           /* s_voice has a sound in it        */
static volatile bool s_running;            /* the backend is playing buffers   */
static uint32_t s_fade_left, s_fade_len;   /* qs_stop()'s fade, in samples     */

/* The amplifier (interrupt side only, after start).                        */
static amp_state_t s_amp;
static uint32_t s_amp_count;               /* samples left in WAKING or TAIL   */
static uint32_t s_on_samples, s_tail_samples, s_fade_samples;

/* ========================================================================== */
/*  Setting up                                                                */
/* ========================================================================== */

void qs_int_attach(const qs_backend_t *be, uint32_t rate, uint8_t max_volume)
{
    s_be       = be;
    s_rate     = rate;
    s_ceiling  = max_volume;
    s_volume   = 100;
    s_voice    = (qs_voice_t){ 0 };
    s_sounding = false;
    s_running  = false;
    s_fade_left = s_fade_len = 0;
    s_amp      = AMP_OFF;

    /* Milliseconds to samples, once, here, so the interrupt never divides. */
    s_on_samples   = QS_AMP_ON_MS * rate / 1000u;
    s_tail_samples = QS_AMP_TAIL_MS * rate / 1000u;
    if (s_tail_samples < 2u * QS_BUFFER_SAMPLES) s_tail_samples = 2u * QS_BUFFER_SAMPLES;
    s_fade_samples = QS_FADE_MS * rate / 1000u;
    if (s_fade_samples < 1u) s_fade_samples = 1u;
}

bool     qs_int_ready(void) { return s_be != NULL; }
uint32_t qs_int_rate(void)  { return s_rate; }

void qs_deinit(void)
{
    if (s_be == NULL) return;
    uint32_t irq = save_and_disable_interrupts();
    s_voice.source = NULL;
    s_sounding = false;
    restore_interrupts(irq);
    s_be->end();                   /* amp off, then the hardware handed back */
    s_be = NULL;
    s_running = false;
    s_amp = AMP_OFF;
}

/* ========================================================================== */
/*  The program's side                                                        */
/* ========================================================================== */

void qs_int_play(const qs_voice_t *v, uint32_t flags)
{
    bool start = false;
    uint32_t irq = save_and_disable_interrupts();
    s_voice = *v;
    if (s_voice.remaining == 0) s_voice.source = NULL;
    s_sounding  = (s_voice.source != NULL);
    s_fade_left = s_fade_len = 0;          /* a new sound cancels a fade      */
    if (s_sounding && !s_running) {        /* idle: the backend needs a start */
        s_running = true;
        start = true;
    }
    restore_interrupts(irq);

    if (start) s_be->start();              /* outside: it fills two buffers   */
    if ((flags & QS_BG) == 0) qs_wait();
}

bool qs_busy(void)
{
    return s_be != NULL && s_sounding;
}

void qs_wait(void)
{
    if (s_be == NULL) return;
    while (s_sounding) s_be->wait();
}

void qs_stop(void)
{
    if (s_be == NULL) return;
    uint32_t irq = save_and_disable_interrupts();
    if (s_voice.source != NULL && s_fade_len == 0) {
        if (s_amp == AMP_ON) {
            s_fade_left = s_fade_len = s_fade_samples;   /* fade out ...      */
        } else {
            s_voice.source = NULL;         /* ... or, if none of it has been */
            s_sounding = false;            /* filled yet, drop it            */
        }
    }
    restore_interrupts(irq);
}

void qs_set_volume(uint8_t percent)
{
    if (s_be == NULL) return;
    s_volume = (percent > 100u) ? 100u : percent;
}

uint8_t qs_get_volume(void)
{
    return (s_be == NULL) ? 0u : s_volume;
}

const char *qs_err_str(qs_err_t err)
{
    switch (err) {
    case QS_OK:              return "OK";
    case QS_ERR_ARG:         return "bad setting in the sound config";
    case QS_ERR_RANGE:       return "frequency out of range";
    case QS_ERR_NOT_READY:   return "sound not initialised";
    case QS_ERR_BUSY:        return "sound already initialised";
    case QS_ERR_NO_HARDWARE: return "no free DMA channel or DMA timer";
    default:                 return "unknown error";
    }
}

/* ========================================================================== */
/*  The interrupt's side                                                      */
/* ========================================================================== */

bool qs_int_active(void)
{
    return s_voice.source != NULL || s_amp != AMP_OFF;
}

void qs_int_stopped(void)
{
    s_running = false;
}

static void silence(int16_t *out, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) out[i] = 0;
}

/*
 * The voice's last sample has been filled, so as far as the program is
 * concerned the sound is over (qs_busy, qs_wait), though the speaker has up
 * to two buffers of it still to play. That's on purpose: a program playing
 * one note after another hears about the end in time to hand over the next
 * note before the buffers run dry, so notes follow each other closely, as
 * QuickBasic's did.
 */
static void end_voice(void)
{
    s_voice.source = NULL;
    s_sounding = false;
    s_fade_left = s_fade_len = 0;
}

/*
 * Play the voice into out[0..n): at most until it ends, or its fade does.
 * Returns how many samples were written.
 *
 * THE FADE is a straight line from the sound's level down to nothing: the
 * k-th sample from the end of the fade is multiplied by k / (fade length).
 * Five milliseconds is too short to hear as a fade, and long enough that
 * the speaker isn't yanked back to the middle in one step.
 */
static uint32_t play_voice(int16_t *out, uint32_t n, int32_t amp)
{
    uint32_t m = (n < s_voice.remaining) ? n : s_voice.remaining;
    if (s_fade_len != 0 && m > s_fade_left) m = s_fade_left;

    s_voice.source(&s_voice, out, m, amp);
    s_voice.remaining -= m;

    if (s_fade_len != 0) {
        for (uint32_t j = 0; j < m; j++) {
            out[j] = (int16_t)((int32_t)out[j] * (int32_t)(s_fade_left - j) / (int32_t)s_fade_len);
        }
        s_fade_left -= m;
        if (s_fade_left == 0) end_voice();
    }
    if (s_voice.remaining == 0) end_voice();
    return m;
}

bool qs_int_fill(int16_t *out, uint32_t n)
{
    /* The loudest a sample may be in this buffer: full scale, times the
     * volume, times the ceiling. Read once, so a volume change mid-buffer
     * can't make one buffer two different sizes.                         */
    const int32_t amp = 32767 * (int32_t)s_volume * (int32_t)s_ceiling / 10000;
    uint32_t i = 0;

    while (i < n) {
        const uint32_t room = n - i;
        const bool sound = (s_voice.source != NULL);
        uint32_t k;

        switch (s_amp) {
        case AMP_OFF:
            if (!sound) {                      /* idle: the rest is silence  */
                silence(out + i, room);
                return false;
            }
            s_be->amp(true);                   /* switch on in silence ...   */
            s_amp = AMP_WAKING;
            s_amp_count = s_on_samples;
            break;

        case AMP_WAKING:                       /* ... and give it time       */
            k = (room < s_amp_count) ? room : s_amp_count;
            silence(out + i, k);
            i += k;
            s_amp_count -= k;
            if (s_amp_count == 0) s_amp = AMP_ON;
            break;

        case AMP_ON:
            if (!sound) {
                s_amp = AMP_TAIL;
                s_amp_count = s_tail_samples;
                break;
            }
            i += play_voice(out + i, room, amp);
            break;

        case AMP_TAIL:
            if (sound) {                       /* another sound came along  */
                s_amp = AMP_ON;
                break;
            }
            k = (room < s_amp_count) ? room : s_amp_count;
            silence(out + i, k);
            i += k;
            s_amp_count -= k;
            if (s_amp_count == 0) {            /* long enough: off, quietly */
                s_be->amp(false);
                s_amp = AMP_OFF;
            }
            break;
        }
    }
    return true;
}
