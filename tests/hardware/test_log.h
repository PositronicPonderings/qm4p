/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    test_log.h
 * @brief   The one format every hardware test prints in, over USB serial.
 *
 * Every line starts with the test's tag in square brackets, so a terminal
 * that has seen several tests still says which one wrote what. A pass of
 * test M2 reads like this:
 *
 *   [M2] qg4p_test_m2: lines, boxes, circles, arcs, the palette, clipping
 *   [M2] Screen A: ST7789   240 x 320  SPI 37500000 Hz  DIRECT
 *   [M2] Screen B: ST7796S  320 x 480  SPI 37500000 Hz  DIRECT
 *   [M2] 1/6 Lines: a starburst, then widths 1 to 8 -- look for ...
 *   [M2]     drawn in A 3.1 ms, B 6.2 ms
 *   ...
 *   [M2] Pass complete. Did every step look right? (answer in run_all.sh)
 *   [M2] Repeating...
 *
 *   - At the start: the program, what it covers, and the screens.
 *   - Each STEP is one line: "n/total Name: what's on the screens -- look
 *     for what a pass looks like", under 100 characters.
 *   - Anything a step measures follows on lines indented four spaces.
 *   - The end of each pass says so, and the next pass says it's repeating.
 *
 * tests/host/check_serial.py checks every test against these rules (and
 * that run_all.sh lists the same steps), so they stay true.
 *
 * Each test defines its tag before including this file:
 *     #define TEST_TAG "M2"
 *     #include "test_log.h"
 *
 * HOW THE MACROS WORK: "[" TEST_TAG "] " followed by the format string is
 * several string literals in a row, which C joins into one, so
 * TEST_LOG("%d fps", n) becomes printf("[M2] %d fps", n). That's why the
 * first argument of TEST_LOG and TEST_DETAIL must be a string in quotes.
 */
#ifndef TEST_LOG_H
#define TEST_LOG_H

#include <stdio.h>

#ifndef TEST_TAG
#error "Define TEST_TAG (for example #define TEST_TAG \"M2\") before including test_log.h"
#endif

/** One line, tagged: TEST_LOG("Checksum: %s", text). Adds the newline. */
#define TEST_LOG(...)     (printf("[" TEST_TAG "] " __VA_ARGS__), printf("\n"))

/** A measurement or detail belonging to the step above, indented. */
#define TEST_DETAIL(...)  TEST_LOG("    " __VA_ARGS__)

/** One step: number, how many, a short name, what's drawn, what to look for. */
#define TEST_STEP(n, total, name, what, look) \
    TEST_LOG("%d/%d %s: %s -- look for %s", (n), (total), (name), (what), (look))

/** The end of one pass through every step. */
#define TEST_PASS_DONE()  TEST_LOG("Pass complete. Did every step look right? (answer in run_all.sh)")

/** The start of every pass after the first. */
#define TEST_REPEAT()     TEST_LOG("Repeating...")

#endif /* TEST_LOG_H */
