/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    beep.c
 * @brief   Sound example 1: BEEP, then a three-note jingle that plays while
 *          the program gets on with something else.
 *
 * Target: qs4p_beep.   Screens: none.   Sound: the amplifier and speaker.
 *
 * WIRING (the pins are in board.h: BOARD_AUDIO_PIN, BOARD_AMP_SD_PIN)
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
 *
 * WHAT IT DOES
 *   1. qs_init_pwm() starts the sound: the amplifier stays off until there's
 *      something to play, and goes off again by itself afterwards.
 *   2. qs_beep() is QuickBasic's BEEP: 800 Hz for a quarter of a second. It
 *      waits until the beep is over (QS_FG), as BEEP did.
 *   3. The jingle is three SOUND statements with QS_BG: each returns at
 *      once, and the note plays while the program carries on. Here the
 *      program only counts while it waits, to show it's free; a game would
 *      draw its next frame.
 *
 * In QuickBasic the sound part of this program is two lines:
 *
 *     BEEP
 *     SOUND 523, 3: SOUND 659, 3: SOUND 784, 6
 *
 * The C version spends its extra lines on the wiring, which QuickBasic never
 * had to ask about: the PC speaker was soldered to the motherboard, so there
 * was nothing to ask.
 *
 * To hear it again, unplug the Pico and plug it in again. Serial output over
 * USB says what happened.
 */
#include <stdio.h>
#include "pico/stdlib.h"
#define BOARD_NO_GRAPHICS                   /* the sound settings only       */
#include "board.h"
#include "qs4p.h"

int main(void)
{
    /* Sound first: the amplifier is switched off before anything else, in
     * case the amp board's own pull-up has switched it on.                 */
    static const qs_pwm_config_t sound = {
        .pin = BOARD_AUDIO_PIN, .shutdown_pin = BOARD_AMP_SD_PIN,
        .max_volume = BOARD_MAX_VOLUME,     /* the rest: 0, the defaults     */
    };
    const qs_err_t err = qs_init_pwm(&sound);

    stdio_init_all();
    if (err != QS_OK) {
        printf("No sound: %s. Check board.h.\n", qs_err_str(err));
        while (true) tight_loop_contents();
    }

    qs_beep();                              /* BEEP                          */
    sleep_ms(500);

    /* The jingle: C, E, G, the last one twice as long. QS_TICKS turns
     * QuickBasic's clock ticks (18.2 a second) into milliseconds.          */
    static const struct { uint16_t hz; uint32_t ms; } jingle[] = {
        { 523, QS_TICKS(3) }, { 659, QS_TICKS(3) }, { 784, QS_TICKS(6) },
    };
    uint32_t counted = 0;
    for (unsigned i = 0; i < 3; i++) {
        qs_sound(jingle[i].hz, jingle[i].ms, QS_BG);    /* returns at once   */
        while (qs_busy()) counted++;        /* the program is free meanwhile */
    }
    printf("While the jingle played, the program counted to %lu.\n", (unsigned long)counted);

    while (true) tight_loop_contents();     /* the amp switches itself off   */
}
