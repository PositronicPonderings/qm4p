/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/*
 * test_qs.c - QS4P's engine and tones, on a PC.
 *
 *     ./test_qs <folder for the .wav files>
 *
 * The engine (qs_engine.c) and the tones (qs_tone.c) run unchanged, against
 * a stand-in backend that does what the PWM backend does with its two
 * buffers and its interrupt, except that "playing" a buffer means adding it
 * to a recording. The recording is what the speaker would have been given,
 * sample by sample, and every test measures it: pitch, length, loudness,
 * how a stopped sound fades, and when the amplifier is switched on and off.
 * Each test also writes its recording as a .wav file (16-bit mono), to
 * listen to or look at, and the test suite checks those files against
 * golden_sounds.sha256.
 *
 * The amplifier switches are recorded to the sample. The stand-in fills
 * each buffer with a marker value the engine never produces (-32768) before
 * handing it over, so when the engine switches the amp, the first marker
 * left in the buffer is exactly where the engine had got to.
 */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "qs_internal.h"

#define N       QS_BUFFER_SAMPLES
#define RATE    22050u
#define CEILING 75u
#define MARK    (-32768)                     /* never a sample: see above     */
#define MAX_LEN (RATE * 30u)                 /* 30 s of recording per test    */

static const char *out_dir = ".";
static int failures;

static void fail(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    printf("  FAIL  ");
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
    failures++;
}
#define CHECK(cond, ...)  do { if (!(cond)) fail(__VA_ARGS__); } while (0)

/* ========================================================================== */
/*  The stand-in backend                                                      */
/* ========================================================================== */

static int16_t  rec[MAX_LEN];                /* what the speaker was given    */
static uint32_t len;                         /* samples recorded so far       */

static int16_t  buf[2][N];
static uint32_t buf_at[2];                   /* where each buffer will play   */
static unsigned playing;                     /* which buffer is playing now   */
static bool     running, idle;
static int16_t *filling;                     /* the buffer being filled ...   */
static uint32_t filling_at;                  /* ... and where it will play    */
static uint32_t fills, starts, ends, stopped_at;

typedef struct { uint32_t at; bool on; } amp_event_t;
static amp_event_t amp_log[64];
static int amp_n;

static bool fill(unsigned k, uint32_t at)
{
    for (int i = 0; i < N; i++) buf[k][i] = MARK;
    filling = buf[k];
    filling_at = buf_at[k] = at;
    const bool more = qs_int_fill(buf[k], N);
    filling = NULL;
    fills++;
    for (int i = 0; i < N; i++) {
        if (buf[k][i] == MARK) { fail("the engine left sample %d of a buffer unfilled", i); break; }
    }
    return more;
}

static void fake_start(void)
{
    CHECK(!running, "start() while already running");
    running = true;
    starts++;
    playing = 0;
    fill(0, len);
    idle = !fill(1, len + N);
}

static void fake_amp(bool on)
{
    uint32_t at = len;
    if (filling != NULL) {                   /* where the engine had got to   */
        uint32_t i = 0;
        while (i < N && filling[i] != MARK) i++;
        at = filling_at + i;
    }
    if (amp_n < 64) amp_log[amp_n++] = (amp_event_t){ at, on };
}

/*
 * One buffer's worth of time. The buffer playing finishes, which is when
 * the PWM backend's interrupt runs; this is the same logic as its
 * on_dma_irq(). With the backend stopped, a buffer's worth of silence.
 */
static void tick(void)
{
    if (len + N > MAX_LEN) { fail("recording full: a sound that never ends?"); exit(1); }
    if (!running) {
        memset(rec + len, 0, sizeof buf[0]);
        len += N;
        return;
    }
    CHECK(buf_at[playing] == len, "buffer due at %u played at %u", buf_at[playing], len);
    memcpy(rec + len, buf[playing], sizeof buf[0]);
    len += N;

    const unsigned done = playing, next = done ^ 1u;
    if (idle && !qs_int_active()) {
        qs_int_stopped();
        running = false;
        stopped_at = len;
        return;
    }
    playing = next;
    idle = !fill(done, buf_at[next] + N);
}

static void fake_wait(void) { tick(); }

static void fake_end(void)
{
    fake_amp(false);                         /* as pwm_end(): amp off first   */
    running = false;
    ends++;
}

static const qs_backend_t fake = {
    .name = "fake", .start = fake_start, .amp = fake_amp, .wait = fake_wait, .end = fake_end,
};

/* Pass `ms` of time a buffer at a time, as a program busy with something
 * else would (the interrupt keeps the sound going meanwhile). */
static void pass_ms(uint32_t ms)
{
    const uint32_t until = len + ms * RATE / 1000u;
    while (len < until) tick();
}

/* A fresh start: nothing recorded, the engine attached to the stand-in. */
static void begin(const char *name, uint32_t rate, uint8_t ceiling)
{
    printf("%s\n", name);
    qs_deinit();
    len = 0; amp_n = 0; fills = starts = ends = stopped_at = 0;
    running = idle = false;
    qs_int_attach(&fake, rate, ceiling);
}

/* ========================================================================== */
/*  Measuring the recording                                                   */
/* ========================================================================== */

#define ON_SAMPLES    (QS_AMP_ON_MS * RATE / 1000u)       /* 441  */
#define TAIL_SAMPLES  (QS_AMP_TAIL_MS * RATE / 1000u)     /* 2205 */
#define FADE_SAMPLES  (QS_FADE_MS * RATE / 1000u)         /* 110  */

/* The first and last non-zero samples at or after `from` (none: first > last). */
static void sound_span(uint32_t from, uint32_t *first, uint32_t *last)
{
    *first = 1; *last = 0;
    uint32_t i = from;
    while (i < len && rec[i] == 0) i++;
    if (i == len) return;
    *first = i;
    for (uint32_t j = i; j < len; j++) if (rec[j] != 0) *last = j;
}

static int peak(uint32_t from, uint32_t to)
{
    int p = 0;
    for (uint32_t i = from; i < to; i++) if (abs(rec[i]) > p) p = abs(rec[i]);
    return p;
}

/* The pitch of [from, to), from the first and last upward crossings of zero. */
static double pitch(uint32_t from, uint32_t to)
{
    long first = -1, last = -1, crossings = 0;
    for (uint32_t i = from + 1; i < to; i++) {
        if (rec[i - 1] < 0 && rec[i] > 0) {
            if (first < 0) first = (long)i;
            last = (long)i;
            crossings++;
        }
    }
    if (crossings < 2) return 0.0;
    return (double)(crossings - 1) * RATE / (double)(last - first);
}

static void check_pitch(const char *what, uint32_t from, uint32_t to, double want)
{
    const double got = pitch(from, to);
    CHECK(fabs(got - want) <= want / 100.0, "%s: pitch %.2f Hz, wanted %.2f (within 1%%)", what, got, want);
}

/*
 * The amplifier's rules, checked over the whole recording:
 *   - no sound while the amp is off;
 *   - at least QS_AMP_ON_MS of silence after it goes on, before any sound;
 *   - at least QS_AMP_TAIL_MS of silence after the sound, before it goes off;
 *   - off at the end, with the backend stopped within three buffers of that.
 */
static void check_amp(void)
{
    int e = 0;
    bool on = false;
    uint32_t on_at = 0, last_sound = 0;
    bool any_sound = false;
    for (uint32_t i = 0; i < len; i++) {
        while (e < amp_n && amp_log[e].at == i) {
            if (amp_log[e].on) {
                CHECK(!on, "amp switched on twice (sample %u)", i);
                on = true; on_at = i;
            } else {
                CHECK(on, "amp switched off twice (sample %u)", i);
                if (any_sound && last_sound >= on_at) {
                    CHECK(i - last_sound - 1 >= TAIL_SAMPLES,
                          "amp off %u samples after the sound, wanted %u", i - last_sound - 1, TAIL_SAMPLES);
                }
                on = false;
            }
            e++;
        }
        if (rec[i] != 0) {
            CHECK(on, "sound with the amp off (sample %u)", i);
            CHECK(i - on_at >= ON_SAMPLES, "sound %u samples after the amp went on, wanted %u",
                  i - on_at, ON_SAMPLES);
            last_sound = i; any_sound = true;
        }
    }
    CHECK(!on, "the amp is still on at the end");
    CHECK(!running, "the backend is still running at the end");
    if (amp_n > 0) {
        const uint32_t off_at = amp_log[amp_n - 1].at;
        CHECK(stopped_at <= off_at + 3u * N, "backend stopped %u samples after the amp, wanted at most %u",
              stopped_at - off_at, 3u * N);
    }
}

/* Let it all finish: sound, tail, the backend stopping. */
static void settle(void)
{
    qs_wait();
    pass_ms(QS_AMP_TAIL_MS + 100u);
}

static void write_wav(const char *name)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s.wav", out_dir, name);
    FILE *f = fopen(path, "wb");
    if (f == NULL) { fail("can't write %s", path); return; }
    const uint32_t data = len * 2u, rate = qs_int_rate(), bytes_per_s = rate * 2u;
    unsigned char h[44] = { 'R','I','F','F', 0,0,0,0, 'W','A','V','E', 'f','m','t',' ',
                            16,0,0,0, 1,0, 1,0, 0,0,0,0, 0,0,0,0, 2,0, 16,0,
                            'd','a','t','a', 0,0,0,0 };
    const uint32_t riff = 36u + data;
    for (int i = 0; i < 4; i++) {
        h[4 + i]  = (unsigned char)(riff >> (8 * i));
        h[24 + i] = (unsigned char)(rate >> (8 * i));
        h[28 + i] = (unsigned char)(bytes_per_s >> (8 * i));
        h[40 + i] = (unsigned char)(data >> (8 * i));
    }
    fwrite(h, 1, sizeof h, f);
    for (uint32_t i = 0; i < len; i++) {     /* little-endian on any PC       */
        const unsigned char b[2] = { (unsigned char)((uint16_t)rec[i] & 0xFF), (unsigned char)((uint16_t)rec[i] >> 8) };
        fwrite(b, 1, 2, f);
    }
    fclose(f);
}

/* ========================================================================== */
/*  The tests                                                                 */
/* ========================================================================== */

static void test_ticks(void)
{
    printf("ticks\n");
    CHECK(QS_TICKS(18) == 989, "QS_TICKS(18) = %u, wanted 989", QS_TICKS(18));
    CHECK(QS_TICKS(1) == 55, "QS_TICKS(1) = %u, wanted 55", QS_TICKS(1));
    CHECK(QS_TICKS(0) == 0, "QS_TICKS(0) = %u, wanted 0", QS_TICKS(0));
    CHECK(QS_TICKS(182) == 10000, "QS_TICKS(182) = %u, wanted 10000", QS_TICKS(182));
    CHECK(QS_TICKS(65535) == 3600824, "QS_TICKS(65535) = %u, wanted 3600824", QS_TICKS(65535));
}

static void test_not_ready(void)
{
    printf("not_ready\n");
    qs_deinit();
    CHECK(qs_sound(440, 100, QS_FG) == QS_ERR_NOT_READY, "qs_sound before init didn't say NOT_READY");
    CHECK(!qs_busy(), "busy before init");
    CHECK(qs_get_volume() == 0, "volume before init is %u, wanted 0", qs_get_volume());
    qs_beep(); qs_stop(); qs_wait(); qs_set_volume(50); qs_deinit();   /* nothing happens */
    CHECK(starts == 0 && amp_n == 0, "something played before init");
    CHECK(strcmp(qs_err_str(QS_ERR_RANGE), "frequency out of range") == 0, "qs_err_str(QS_ERR_RANGE)");
    CHECK(strcmp(qs_err_str((qs_err_t)-99), "unknown error") == 0, "qs_err_str(-99)");
}

/* BEEP: 800 Hz for 250 ms, after the amp's 20 ms of silence. */
static void test_beep(void)
{
    begin("beep", RATE, CEILING);
    qs_beep();
    CHECK(!qs_busy(), "busy after BEEP returned");
    CHECK(len + 2u * N >= 250u * RATE / 1000u + ON_SAMPLES,
          "BEEP returned %u samples early: more than two buffers", 250u * RATE / 1000u + ON_SAMPLES - len);
    settle();
    uint32_t first, last;
    sound_span(0, &first, &last);
    CHECK(amp_n >= 1 && first == amp_log[0].at + ON_SAMPLES,
          "sound starts at %u, wanted the amp's switch-on + %u", first, ON_SAMPLES);
    const uint32_t want = 250u * RATE / 1000u, got = last - first + 1;
    CHECK(got + N > want && got < want + N, "BEEP lasts %u samples, wanted %u (within one buffer)", got, want);
    check_pitch("BEEP", first, last + 1, 800.0);
    CHECK(peak(first, last + 1) == 32767 * (int)CEILING / 100, "BEEP's peak is %d, wanted %d",
          peak(first, last + 1), 32767 * (int)CEILING / 100);
    check_amp();
    CHECK(starts == 1, "the backend started %u times, wanted once", starts);
    write_wav("beep");
}

/* A scale, as QuickBasic wrote it: SOUND f, 4 for each note. */
static void test_scale(void)
{
    static const uint16_t notes[] = { 262, 294, 330, 349, 392, 440, 494, 523 };
    begin("scale", RATE, CEILING);
    for (unsigned k = 0; k < 8; k++) {
        CHECK(qs_sound(notes[k], QS_TICKS(4), QS_FG) == QS_OK, "SOUND %u failed", notes[k]);
    }
    settle();
    check_amp();
    CHECK(amp_n == 2, "amp switched %d times, wanted on once and off once", amp_n);
    const uint32_t each = QS_TICKS(4) * RATE / 1000u;
    uint32_t at = 0, first, last;
    for (unsigned k = 0; k < 8; k++) {
        sound_span(at, &first, &last);
        if (first > last) { fail("note %u missing", k); return; }
        char what[32];
        snprintf(what, sizeof what, "note %u (%u Hz)", k + 1, notes[k]);
        check_pitch(what, first, first + each, notes[k]);
        CHECK(first + each <= len && rec[first + each - 1] != 0, "%s is short", what);
        if (k > 0) CHECK(first - at <= N, "%s starts %u samples after the last, wanted at most %u",
                         what, first - at, N);
        at = first + each;
    }
    write_wav("scale");
}

/* Pitch across the range, at both ends and between. */
static void test_pitches(void)
{
    static const uint16_t hz[] = { 37, 100, 440, 1000, 4000, 8000, 11024 };
    begin("pitches", RATE, CEILING);
    for (unsigned k = 0; k < sizeof hz / sizeof hz[0]; k++) {
        const uint32_t from = len;
        CHECK(qs_sound(hz[k], 600, QS_FG) == QS_OK, "SOUND %u failed", hz[k]);
        pass_ms(50);                         /* a gap, so they don't run on   */
        uint32_t first, last;
        sound_span(from, &first, &last);
        char what[32];
        snprintf(what, sizeof what, "%u Hz", hz[k]);
        check_pitch(what, first, last + 1, hz[k]);
        const uint32_t want = 600u * RATE / 1000u, got = last - first + 1;
        CHECK(got + N > want && got < want + N, "%s lasts %u samples, wanted %u", what, got, want);
    }
    settle();
    check_amp();
    write_wav("pitches");
}

/* 25, 50, 75 and 100%: 100% is the ceiling, not the hardware's loudest. */
static void test_volume(void)
{
    static const uint8_t steps[] = { 25, 50, 75, 100 };
    begin("volume", RATE, CEILING);
    CHECK(qs_get_volume() == 100, "volume after init is %u, wanted 100", qs_get_volume());
    for (unsigned k = 0; k < 4; k++) {
        qs_set_volume(steps[k]);
        CHECK(qs_get_volume() == steps[k], "volume reads %u, wanted %u", qs_get_volume(), steps[k]);
        const uint32_t from = len;
        qs_sound(1000, 300, QS_FG);
        pass_ms(50);
        const int want = 32767 * steps[k] * (int)CEILING / 10000;
        CHECK(peak(from, len) == want, "peak at %u%% is %d, wanted %d", steps[k], peak(from, len), want);
    }
    CHECK(peak(0, len) == 32767 * (int)CEILING / 100, "peak at 100%% is %d, wanted the ceiling's %d",
          peak(0, len), 32767 * (int)CEILING / 100);
    qs_set_volume(150);
    CHECK(qs_get_volume() == 100, "volume 150 reads %u, wanted 100", qs_get_volume());
    qs_set_volume(0);
    const uint32_t from = len;
    qs_sound(1000, 100, QS_FG);
    CHECK(peak(from, len) == 0, "volume 0 isn't silent");
    settle();
    check_amp();
    write_wav("volume");

    begin("volume_ceiling_100", RATE, 100);
    qs_sound(1000, 100, QS_FG);
    CHECK(peak(0, len) == 32767, "peak with a 100%% ceiling is %d, wanted 32767", peak(0, len));
    settle();
    check_amp();
}

/* Frequencies it can't play: an error, and nothing changes. */
static void test_range(void)
{
    begin("range", RATE, CEILING);
    static const uint16_t bad[] = { 0, 1, 36, 11025, 11026, 20000, 65535 };
    for (unsigned k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        CHECK(qs_sound(bad[k], 100, QS_FG) == QS_ERR_RANGE, "%u Hz wasn't QS_ERR_RANGE", bad[k]);
        CHECK(qs_sound(bad[k], 100, QS_BG) == QS_ERR_RANGE, "%u Hz (BG) wasn't QS_ERR_RANGE", bad[k]);
    }
    CHECK(starts == 0 && amp_n == 0 && !qs_busy(), "a bad frequency played something");
    CHECK(qs_sound(37, 50, QS_FG) == QS_OK && qs_sound(11024, 50, QS_FG) == QS_OK, "37 or 11024 Hz refused");

    /* A bad frequency with ms = 0 doesn't stop what's playing either. */
    qs_sound(440, 1000, QS_BG);
    pass_ms(100);
    CHECK(qs_sound(20000, 0, QS_BG) == QS_ERR_RANGE, "20000 Hz with ms 0 wasn't QS_ERR_RANGE");
    pass_ms(100);
    CHECK(qs_busy() && rec[len - 1] != 0, "a bad frequency with ms 0 stopped the sound");
    qs_stop();
    settle();
    check_amp();

    begin("range_8000", 8000u, CEILING);
    CHECK(qs_sound(3999, 50, QS_FG) == QS_OK, "3999 Hz at 8000 samples/s refused");
    CHECK(qs_sound(4000, 50, QS_FG) == QS_ERR_RANGE, "4000 Hz at 8000 samples/s wasn't QS_ERR_RANGE");
    settle();
}

/*
 * qs_stop() partway through a long tone: quiet within QS_FADE_MS plus two
 * buffers (qs4p.h), fading down, never cut.
 */
static void test_stop(void)
{
    begin("stop", RATE, CEILING);
    qs_sound(440, 3000, QS_BG);
    pass_ms(500);
    const uint32_t t = len;                  /* the moment qs_stop() is called */
    qs_stop();
    qs_stop();                               /* twice is fine                  */
    CHECK(qs_busy(), "not busy straight after qs_stop(): the fade is still to come");
    qs_wait();
    settle();
    uint32_t first, last;
    sound_span(0, &first, &last);
    const uint32_t limit = 2u * N + FADE_SAMPLES;
    CHECK(last < t + limit, "still sounding %u samples after qs_stop(), wanted under %u", last - t, limit);
    for (uint32_t i = last + 1 - FADE_SAMPLES + 1; i <= last; i++) {
        if (abs(rec[i]) > abs(rec[i - 1])) { fail("the fade gets louder at sample %u", i); break; }
    }
    CHECK(abs(rec[last]) < 32767 * (int)CEILING / 100 / 50, "the fade ends at %d, not near 0", rec[last]);
    check_amp();
    write_wav("stop");

    /* SOUND f, 0 stops the same way. */
    begin("stop_ms0", RATE, CEILING);
    qs_sound(440, 3000, QS_BG);
    pass_ms(300);
    const uint32_t t0 = len;
    CHECK(qs_sound(440, 0, QS_BG) == QS_OK, "qs_sound(440, 0) failed");
    settle();
    sound_span(0, &first, &last);
    CHECK(last < t0 + limit, "SOUND f, 0: still sounding %u samples later", last - t0);
    check_amp();

    /* Stopping with nothing playing does nothing. */
    begin("stop_idle", RATE, CEILING);
    qs_stop();
    pass_ms(100);
    CHECK(starts == 0 && amp_n == 0, "qs_stop() with nothing playing played something");
}

/*
 * A sequence of background tones while the program does something else: a
 * stand-in for S1's step 3. Each "frame" of the program takes a buffer's
 * worth of time, and it starts the next note when qs_busy() says the last
 * one has finished.
 */
static void test_background(void)
{
    static const struct { uint16_t hz; uint32_t ms; } tune[] = {
        { 523, 250 }, { 659, 250 }, { 784, 250 }, { 1047, 500 },
        { 784, 250 }, { 659, 250 }, { 523, 500 }, { 392, 1000 },
    };
    const unsigned count = sizeof tune / sizeof tune[0];
    begin("background", RATE, CEILING);
    unsigned next = 0, frames = 0;
    while (next < count || qs_busy()) {
        if (!qs_busy() && next < count) {
            const uint32_t before = len;
            CHECK(qs_sound(tune[next].hz, tune[next].ms, QS_BG) == QS_OK, "background note failed");
            CHECK(len == before, "QS_BG waited");
            next++;
        }
        tick();                              /* the program draws a frame     */
        frames++;
    }
    settle();
    check_amp();
    CHECK(amp_n == 2, "amp switched %d times, wanted on once and off once", amp_n);
    uint32_t at = 0, first, last;
    for (unsigned k = 0; k < count; k++) {
        const uint32_t each = tune[k].ms * RATE / 1000u;
        sound_span(at, &first, &last);
        if (first > last) { fail("background note %u missing", k); break; }
        char what[40];
        snprintf(what, sizeof what, "background note %u (%u Hz)", k + 1, tune[k].hz);
        check_pitch(what, first, first + each, tune[k].hz);
        at = first + each;
    }
    write_wav("background");
}

/* A new sound replaces the one playing; QS_BG and qs_busy() behave. */
static void test_replace(void)
{
    begin("replace", RATE, CEILING);
    qs_sound(440, 2000, QS_BG);
    CHECK(qs_busy(), "not busy after a QS_BG sound");
    pass_ms(300);
    const uint32_t t = len;
    qs_sound(880, 300, QS_FG);
    CHECK(!qs_busy(), "busy after a QS_FG sound returned");
    settle();
    uint32_t first, last;
    sound_span(t, &first, &last);
    check_pitch("before the change", 0, t, 440.0);
    check_pitch("after the change", t + 2u * N, last + 1, 880.0);
    CHECK(last + 1 - t <= 2u * N + 300u * RATE / 1000u, "the 440 Hz tone went on after the 880 Hz one");
    check_amp();
}

/* After the tail the amp is off and the backend stops: nothing runs. */
static void test_idle(void)
{
    begin("idle", RATE, CEILING);
    qs_beep();
    settle();
    const uint32_t f = fills;
    pass_ms(3000);
    CHECK(fills == f, "%u buffers filled while idle", fills - f);
    CHECK(!running, "the backend is running while idle");
    check_amp();

    /* And it starts again from idle, waking the amp in silence again. */
    qs_beep();
    settle();
    check_amp();
    CHECK(amp_n == 4 && starts == 2, "after idle: %d amp switches and %u starts, wanted 4 and 2", amp_n, starts);
}

/* qs_deinit() stops everything; afterwards it's as before init. */
static void test_deinit(void)
{
    begin("deinit", RATE, CEILING);
    qs_sound(440, 2000, QS_BG);
    pass_ms(200);
    const uint32_t e = ends;
    qs_deinit();
    CHECK(ends == e + 1, "qs_deinit() didn't end the backend");
    CHECK(amp_n >= 2 && !amp_log[amp_n - 1].on, "qs_deinit() left the amp on");
    CHECK(!qs_busy() && qs_sound(440, 100, QS_FG) == QS_ERR_NOT_READY, "sound still works after qs_deinit()");
    qs_deinit();                             /* twice is fine                 */
    CHECK(ends == e + 1, "a second qs_deinit() ended the backend again");
}

int main(int argc, char **argv)
{
    if (argc > 1) out_dir = argv[1];
    test_ticks();
    test_not_ready();
    test_beep();
    test_scale();
    test_pitches();
    test_volume();
    test_range();
    test_stop();
    test_background();
    test_replace();
    test_idle();
    test_deinit();
    printf("%d failures\n", failures);
    return failures ? 1 : 0;
}
