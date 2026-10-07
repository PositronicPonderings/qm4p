/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qs_internal.h
 * @brief   How QS4P's layers talk to each other. Not for application code.
 *
 * Three layers, each knowing nothing about the ones above it:
 *
 *   sources (qs_tone.c)   describe a sound as a voice, hand it to the engine
 *   engine (qs_engine.c)  keeps the voice, fills buffers of samples from it
 *   backend (qs_pwm.c)    plays the buffers, asks the engine for more
 *
 * The engine meets its backend only through the qs_backend_t below. That's
 * what lets the host tests (tests/host/test_qs.c) run the engine and the
 * sources on a PC against a stand-in backend that collects the samples
 * instead of playing them.
 */
#ifndef QS_INTERNAL_H
#define QS_INTERNAL_H

#include "qs4p.h"

/* -------------------------------------------------------------------------- */
/*  Voices: what the engine plays                                             */
/* -------------------------------------------------------------------------- */

typedef struct qs_voice qs_voice_t;

/**
 * A source writes `n` samples of its sound into `out`, at amplitude `amp`
 * (0..32767). Called from the backend's interrupt: it must be quick, use
 * whole numbers only, and never wait or print.
 */
typedef void (*qs_source_fn)(qs_voice_t *v, int16_t *out, uint32_t n, int32_t amp);

/** One sound being played. */
struct qs_voice {
    qs_source_fn source;     /**< makes the samples; NULL = this voice is silent */
    uint32_t     remaining;  /**< samples still to play                          */
    uint32_t     phase;      /**< tone: where in the cycle, 2^32 = one cycle     */
    uint32_t     step;       /**< tone: how far each sample moves the phase      */
};

/**
 * Start playing `v` (copied, so it may live on the caller's stack),
 * replacing whatever was playing. QS_FG waits until it has finished.
 * The engine starts the backend if it was idle.
 */
void qs_int_play(const qs_voice_t *v, uint32_t flags);

/** True once qs_init_pwm() (or a test's qs_int_attach()) has succeeded. */
bool qs_int_ready(void);

/** The sample rate the backend actually plays at. */
uint32_t qs_int_rate(void);

/* -------------------------------------------------------------------------- */
/*  Backends: what plays the samples                                          */
/* -------------------------------------------------------------------------- */

/**
 * A backend is a small table of functions (C's version of an interface):
 *
 *   start   begin playing: fill both buffers with qs_int_fill() and go.
 *           Called by the engine, outside interrupts, when a sound starts
 *           while nothing is playing.
 *   amp     switch the amplifier on or off (its shutdown pin). Called by the
 *           engine from qs_int_fill(), so from the interrupt, and quick.
 *   wait    pass a moment while the program waits for a sound to finish
 *           (qs_wait). The hardware lets interrupts do the work; the host
 *           tests' stand-in plays the next buffer.
 *   end     stop everything and hand the hardware back (qs_deinit).
 */
typedef struct qs_backend {
    const char *name;
    void (*start)(void);
    void (*amp)(bool on);
    void (*wait)(void);
    void (*end)(void);
} qs_backend_t;

/**
 * Called by a backend once it's ready: from now on the engine plays through
 * `be` at `rate` samples a second, with `max_volume` (1..100 %) as the volume
 * ceiling. The amplifier must be off, and nothing playing.
 */
void qs_int_attach(const qs_backend_t *be, uint32_t rate, uint8_t max_volume);

/**
 * Called by the backend to fill a buffer with the next `n` samples (from its
 * interrupt, or from start()). Returns false when the amplifier has been
 * switched off and nothing is waiting to play: the backend may then stop
 * once this buffer is due, unless qs_int_active() says otherwise by then.
 *
 * A backend keeps two buffers: one playing, one filled and waiting. start()
 * fills both; after that, each buffer is refilled the moment it finishes
 * playing. So what's filled now reaches the speaker one to two buffers later.
 */
bool qs_int_fill(int16_t *out, uint32_t n);

/**
 * True if the engine has anything to do: a sound playing, or the amplifier
 * still on. A backend checks this before stopping, because a new sound may
 * have started since the last fill returned false.
 */
bool qs_int_active(void);

/** Called by the backend, from its interrupt, when it has stopped playing. */
void qs_int_stopped(void);

#endif /* QS_INTERNAL_H */
