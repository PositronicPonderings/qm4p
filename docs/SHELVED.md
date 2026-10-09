<!-- SPDX-License-Identifier: MIT-0 -->
<!-- SPDX-AI-Disclosure: ai-generated -->
<!-- SPDX-AI-Model: claude-opus-5-5 -->
<!-- SPDX-AI-Provider: Anthropic -->

# QS4P: shelved

**QuickSound 4 Pico** was meant to be QM4P's sound library. It was set aside on **2026-10-09**, after milestones S0 and S1. Sound didn't fit the product. It drew too much power for the sound the small speakers produced, larger speakers would make the device too big, and the beeps sounded cheap next to the rest of the product.

This page records the plan, the requirements as they stood, what was built, and what was learned, so the work can be picked up again without starting from scratch.

- **Where the work is:** this branch, `attack-of-the-square-waves`, and the tag `qs4p-shelved` on its tip, which keeps it reachable even if the branch is deleted.
- **What `main` knows about it:** nothing. QM4P 0.2.1 removed every mention of sound from `main`, including the reserved pins and the planned flash area. Resuming means putting those back (see [Picking it up again](#picking-it-up-again)).

---

## 1. What it was for

The first user is the **Dice Roller**: a two-screen tabletop dice roller with a small speaker on the player-facing side.

- **Purpose:** short sounds, 2 to 3 seconds, not loud: roll results, success and failure cues, small fun effects. Sound is there for accessibility and fun, since a sound carries further across a big table than a small screen does.
- **Power:** the device runs 4 to 6 hours on a battery, so the amplifier must be off whenever nothing is playing.
- **Storage:** no SD card. Sounds live in onboard flash, alongside the graphics.

QS4P was also meant to stand on its own, for projects that want sound without graphics, or graphics without sound.

## 2. Requirements as they stood

**Design rules, the same as QG4P's:**
- QuickBasic-inspired, in C on the Pico SDK, needing nothing else.
- **If you don't use it, you don't pay for it.** A feature a program doesn't call costs no flash, RAM or time.
- Meshes with QG4P: the two run side by side without fighting over hardware. Shared resources are recorded in `docs/RESOURCES.md`.
- Heavily commented, as a teaching tool.

**Features wanted:**
- `BEEP`, `SOUND` and `PLAY`, in the spirit of QuickBasic.
- Sound files (WAV) played in the foreground (wait until done) or in the background (return at once).
- One-shots and loops: a number of times, for a length of time, or until stopped.
- Nice to have: instrument sounds and several voices at once, Commodore 64 SID style.
- Both PWM output (one pin, cheap) and I2S output (a digital amp board, better sound), behind one API.

**The MVP:**
- mono PWM audio;
- BEEP;
- short (2 to 5 second) mono WAV one-shots;
- one sound at a time (a new sound stops the old).

## 3. Decisions made

| Question | Decision | Why |
|---|---|---|
| Build or adopt? | Build our own, borrowing ideas from Raspberry Pi's `pico-extras` audio code (its buffer pool and swappable output backends) | `pico-extras` is BSD-licensed (it would bring copyright notices into an MIT-0 repo), a separate install, officially "work in progress" for years, has a frequency-limited PWM backend, and covers only the output side: no BEEP, PLAY, WAV or looping |
| Name | QS4P, QuickSound 4 Pico, prefix `qs_` | Matches QG4P and QA4P |
| Where it lives | `qs4p/` in the QM4P umbrella repo | Hardware clashes between libraries are visible in one place |
| Durations | Milliseconds, with `QS_TICKS(n)` for QuickBasic's 18.2-per-second clock ticks | Milliseconds are what people think in; QB code still ports |
| `PLAY` syntax | QuickBasic-compatible (`T`, `O`, `L`, `MN/ML/MS`, `MF/MB`...) plus the common MML extensions: `V` volume, `@` instrument, `&` tie | QB's PLAY strings are one dialect of MML; follow the shared extensions rather than inventing new ones |
| Modern notation | RTTTL (Nokia ringtones) in its own function, `qs_play_rtttl()`, feeding the same note player | Widely used, thousands of tunes exist; its own function so its parser is only linked when used |
| Sound files | In a separate sound pack, read with QA4P's pack handles (several packs open at once) | Art and sounds update independently |
| WAV formats | PCM 8- and 16-bit, plus IMA ADPCM (4:1 compression) | ADPCM is cheap to decode and quarters the flash used |
| Voices | One at first; a `QS_VOICES` setting from day one | Mixing can arrive later without changing the API |
| Arithmetic | Whole numbers only, from the configuration to the interrupt | No floating point in an interrupt handler; checked by a host test |
| Manual | Written once the library is finished; tests and examples along the way | Avoids rewriting the docs at every milestone |

**Flash plan** (Pico 2, 4 MB):

| Area | Offset | Size |
|---|---|---|
| Firmware | 0 | up to 1 MB |
| Graphics pack | 1 MB (`0x100000`) | up to 1.5 MB |
| Sound pack | 2.5 MB (`0x280000`) | up to 1.5 MB |

**Sizes to expect:** a speaker this small plays almost nothing above about 8 kHz, so 11 to 16 kHz sample rates lose nothing audible. A 3-second clip is about 33 KB at 8-bit and 11 kHz, 66 KB at 8-bit and 22 kHz, 33 KB as IMA ADPCM at 22 kHz, and 132 KB at 16-bit and 22 kHz.

## 4. The milestones

| | Milestone | Status |
|---|---|---|
| P0 | Restructure into QM4P; the asset reader becomes QA4P with pack handles | **Done**, on `main` (QM4P 0.1.0) |
| S0 | Hardware bring-up with no library: tones, sweep, beeps and volume steps to settle the filter, PWM resolution and volume ceiling by ear | **Built and run on hardware.** First run found clipping (see section 6); the wiring was changed |
| S1 | Library core: PWM backend, engine, `qs_beep`, `qs_sound`, foreground and background, stop, busy and wait, volume ceiling, amp switching | **Built**; host tests pass. Hardware test `qs4p_test_s1` written |
| S2 | `PLAY`: full QuickBasic syntax including `MF`/`MB`, plus `V`, `@`, `&`; waveform choice for `@` (square, pulse widths, triangle, sawtooth, noise) | Not started |
| S3 | PCM WAV (8- and 16-bit) from compiled-in arrays; loops by count, by time, and until stopped | Not started |
| S4 | Converter script (`wav2qs.py`: any WAV, MP3 or OGG in, trimmed bass, chosen rate and format out) and the sound pack | Not started |
| S5 | IMA ADPCM | Not started |
| S6 | RTTTL | Not started |
| S7 | I2S backend for the MAX98357A, the same programs working with only the init call changed | Not started |
| S8 | Wrap-up: manual, examples, size report, release | Not started |

**Later, beyond S8:** envelopes, mixing several voices, filters, and MP3 playback (`minimp3`, public domain, a good fit for MIT-0).

## 5. What was built

### The API (QS4P 0.1.0)

| QuickBasic | QS4P |
|---|---|
| | `qs_init_pwm(&cfg)`, `qs_deinit()` |
| `BEEP` | `qs_beep()`: 800 Hz, 250 ms, foreground |
| `SOUND f, d` | `qs_sound(hz, ms, QS_FG)`, or `QS_BG` to return at once. `ms` = 0 stops. `QS_TICKS(d)` turns clock ticks into milliseconds (`QS_TICKS(18)` = 989) |
| | `qs_busy()`, `qs_wait()`, `qs_stop()` (a 5 ms fade, not a cut) |
| | `qs_set_volume(percent)`, `qs_get_volume()`, `qs_err_str(err)` |

- **Config** (`qs_pwm_config_t`): audio pin, amp shutdown pin (or -1), sample rate (4,000 to 50,000; default 22,050), volume ceiling (default 60%), PWM bits (8 or 10; default 8).
- **Errors:** `QS_ERR_ARG`, `QS_ERR_RANGE` (frequency below 37 Hz, QuickBasic's limit, or at or above half the sample rate), `QS_ERR_NOT_READY`, `QS_ERR_BUSY`, `QS_ERR_NO_HARDWARE`.
- **Settings** in `qs4p/qs_config.h`: `QS_DEFAULT_RATE`, `QS_DEFAULT_PWM_BITS`, `QS_DEFAULT_MAX_VOLUME`, `QS_BUFFER_SAMPLES` (256), `QS_VOICES` (1), `QS_DMA_IRQ`, `QS_AMP_ON_MS`.

### How it works

Three layers, each knowing nothing about the ones above:

1. **Backend** (`qs_pwm.c`): the PWM slice runs at the full 150 MHz system clock. A DMA channel, paced by a DMA timer at the sample rate, copies levels from two 256-sample buffers into the PWM compare register. When one buffer finishes, a shared handler on `DMA_IRQ_1` restarts the DMA on the other and asks the engine to refill the first. The DMA channel and the timer are claimed at run time. 22,050 Hz is clk_sys × 7 / 47,619, which gives 22,050.022.
2. **Engine** (`qs_engine.c`): one sound at a time, volume scaling, and amp power. The amp is switched only in silence: on 20 ms before a sound, off 100 ms after the last. While idle, the DMA is stopped so no interrupt fires.
3. **Sources** (`qs_tone.c`): square waves from an integer phase accumulator. There's a clean place for other waveforms (S2).

**Size:** about 3 KB of flash and 2.6 KB of RAM (2,969 and 2,645 bytes in `qs4p_beep`), and only in programs that call `qs_init_pwm`.

### Tests and examples

- **`tests/hardware/test_s0.c`** (`qs4p_test_s0`): drives the pins directly, using a repeating timer instead of DMA. Steps:
  1. silence;
  2. 1 kHz at 8-bit;
  3. 1 kHz at 10-bit;
  4. a sweep from 100 Hz to 8 kHz;
  5. three beeps;
  6. a 1 kHz square at 25, 35, 50, 70 and 100%, each step 3 dB louder.
- **`tests/hardware/test_s1.c`** (`qs4p_test_s1`):
  1. BEEP three times;
  2. a scale in clock ticks;
  3. a 4-second tune in the background **while both screens draw**;
  4. `qs_stop()` mid-tone;
  5. volume steps;
  6. 3 seconds idle with the amp off.
- **`examples/beep.c`** (`qs4p_beep`): BEEP, then a three-note jingle in the background. It links QS4P only (`BOARD_NO_GRAPHICS`).
- **`run_all.sh --group graphics|sound|all`.**
- **Host tests:**
  - `qs_engine` checks pitch (within 1%), length (within one buffer), loudness at the ceiling, range errors, the stop fade, `QS_TICKS`, and the amp switching. It writes every test's output to `tests/host/out/*.wav` so it can be listened to on a PC.
  - `golden_sounds` fingerprints those files.
  - `qs_pwm_backend` runs the backend against a pretend SDK that logs every call.
  - `qs_integer_only` finds no floating point anywhere in QS4P.
  - `copy_the_folder` compiles `qs4p/` with only `qs4p/` on the include path.

### Hardware resources (from `docs/RESOURCES.md` on this branch)

| Resource | Use |
|---|---|
| GP2, PWM slice 1 A | Audio out |
| GP3 | Amp shutdown, as a plain output. Because the DMA writes the slice's whole 32-bit compare register, **slice 1 B can't be used** by anything else |
| DMA channel and DMA pacing timer | One each, claimed at run time |
| `DMA_IRQ_1`, shared handler | Fires once per buffer, about 86 times a second, only while sound plays. QG4P uses no DMA interrupt; line 0 is left for SDK examples that default to it |
| GP9 to GP11 | Reserved for I2S (S7), on PIO |
| Core | Whichever core calls `qs_init_pwm`; call every `qs_` function from that core |

## 6. What we learned

**Amplifier input level is the first thing to get right.**
- The PAM8302 has about 24 dB of gain (×16) and runs from about 5 V, so it clips with only about 0.6 V peak to peak at its input.
- GP2 swings 3.3 V. Fed through a single 1 kΩ resistor, the amp clipped above about a fifth of full scale. The symptoms: louder volume steps sounded no louder, notes turned gravelly, and **the screens dimmed**, because a clipping class-D amp driving a square wave draws enough current to sag the shared supply.
- The fix in the last commit: a divider, GP2 → 4.7 kΩ → A+, with 1 kΩ and the filter capacitor from A+ to GND. Full scale becomes about 0.58 V.

**The filter.** The corner frequency is 1 / (2π × R × C), with R being the two resistors in parallel as the capacitor sees them (825 Ω). 10 nF gives about 19 kHz; 22 nF about 9 kHz. A bigger capacitor means less carrier whine and hiss, but duller high notes.

**PWM carrier.** With the counter at 150 MHz:
- 8-bit gives a 586 kHz carrier;
- 10-bit gives a finer 146 kHz carrier, closer to what the filter must remove.

**Speaker safety.** A square wave at full scale through this wiring puts up to about 2.7 W into 8 Ω, well past a 1 W speaker's rating. Power goes with the square of the amplitude, so a 60% ceiling is about 1 W. Keep a volume ceiling in any future backend.

**The PAM8302's outputs are bridged.** Neither speaker wire may go to ground, and that includes a scope's ground clip.

**Pop avoidance works in this order:**
- **on:** SD low, PWM at 50% (silence), wait about 20 ms, then SD high;
- **off:** SD low first, then stop the PWM.

**Small speakers:** the 1 W cavity speakers play almost no bass and are loudest in the upper midrange. Beeps and chiptune-style sounds carry well; voices and booms sound thin. Trim the bass when converting files, since it only adds distortion.

**Shared power.**
- The amp, the screens' backlights and the Pico share one supply. Even after the divider fix, the screens dimmed and their output became unstable while sound played.
- Clipping made it worse, but it wasn't the whole cause: a class-D amp driving a speaker draws real current in bursts, and the supply sagged under it.
- Any future attempt needs the amp's current in the product's power budget from the start, and probably its own supply decoupling (a bulk capacitor, 100 to 470 µF, close to its VIN) or its own regulator.

### The verdict

The final judgement was made **after** the divider fix (commit `2e37427`), on hardware. Sound was set aside for four reasons, roughly in order of weight:

1. **Power for too little sound.** The amp's draw dimmed the screens and made their output unstable. That's a lot of power for what a 1 W micro speaker gives back.
2. **Size.** Better sound needs bigger speakers, and bigger speakers make the device too big. A table tool has to stay small.
3. **Power headroom.** A larger power source to feed a louder amp brings complications of its own (size, charging, heat, safety). Future peripherals (an accessory port, keypads, puzzle controllers) will also need their share of the same budget.
4. **Feel.** The beeps and notes sounded cheap. That's faithful to QuickBasic, which was the point, but it didn't match the feel of the product.

**The MAX98357A (I2S) was considered and not tried.** It would give a cleaner signal, but it doesn't change the speaker size or the power, which were the real limits. Its output stage is class-D like the PAM8302's.

## 7. Picking it up again

**What would have to change first.** The code wasn't the problem, so a better backend alone won't bring sound back. Revisit when at least one of these is true:
- **The product has a power budget with room for an amp,** including peripherals. Ideally the amp has its own regulator or rail, so sound can't disturb the screens.
- **There's physical room for a larger speaker,** or a different product, one that isn't a small table tool, wants sound.
- **There's a sound design that fits the product's feel.** Recorded samples (S3 to S5: WAV and ADPCM) rather than square-wave tones, with the converter's bass trimming. Note that samples still need the same amp and speaker, so they fix the feel but not the power or the size.

**Where to start then:**
- **Measure before building.** Put the amp on its own supply (or a bench supply) and log the current at the ceiling volume with the product's real speaker. Then decide whether the battery and the peripherals can afford it.
- **Choose the output by the speaker and the power, not the signal path.** The MAX98357A boards on hand (I2S, untested) give cleaner sound with no PWM carrier, filter or divider. The board's gain is set by its GAIN pin (several steps from about 3 to 15 dB), and it can put about 3 W into 4 Ω from 5 V, so keep a volume ceiling for small speakers. It needs three GPIOs (planned: GP9 to GP11) and a small PIO program; in the plan, that's S7. The PAM8302 wiring with the divider works and is the cheaper option.
- **For simple cues only,** consider whether a piezo element or a small magnetic buzzer is enough. It costs little power and space, but it can only beep, which is the "cheap" sound this product didn't want.

**Steps:**
1. Bring this branch up to date with `main` (merge or rebase). Expect conflicts in `docs/RESOURCES.md`, `README.md`, `docs/AI_GUIDE.md`, `CHANGELOG.md` and the showcase's closing card, all of which `main` changed in QM4P 0.2.1 when sound was removed.
2. Renumber: this branch's CHANGELOG calls its release QM4P 0.3.0, so pick the next number after whatever `main` has reached.
3. Put the reservations back in `docs/RESOURCES.md`: GP2, GP3, GP9 to GP11, and the sound-pack flash area. Check them against anything `main` has added since, as pins may have been taken.
4. Run `sh tests/host/run_tests.sh` and a full build.
5. On hardware, run `qs4p_test_s0`, then `qs4p_test_s1`.

**Commits on this branch:**

| Commit | Date | What |
|---|---|---|
| `591a379` | 2026-10-07 | S0: bring-up test with no library |
| `12d70df` | 2026-10-07 | QS4P 0.1.0 core: engine, PWM backend, tones |
| `4d16d9e` | 2026-10-07 | Host tests for QS4P |
| `cbaa165` | 2026-10-07 | S1 hardware test and the `qs4p_beep` example |
| `4ca9a7e` | 2026-10-07 | QM4P 0.3.0: CHANGELOG, README, size tools, RESOURCES |
| `2e37427` | 2026-10-09 | The input divider, after S0 found clipping |
