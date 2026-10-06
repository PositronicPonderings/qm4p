/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    test_m7.c
 * @brief   Hardware test M7: images and text from an asset pack (QA4P).
 *
 * WHAT IT CHECKS
 *   That QA4P finds the asset pack in flash, checks it, and finds files in
 *   it by name; that QG4P draws images straight from the pack; and that a
 *   missing pack or a missing file is handled gracefully. Unlike M6, no
 *   images are compiled into this program: everything on screen comes from
 *   the pack.
 *
 * HARDWARE
 *   Screens A and B on the shared bus, as in test_board.h, both DIRECT, and
 *   the test asset pack loaded into flash, separately from the program:
 *       python3 tools/mkpack.py tests/hardware/pack --out build/assets
 *   then hold BOOTSEL, plug in, and drag build/assets.uf2 onto the drive,
 *   or: picotool load -f build/assets.bin -o 0x10100000
 *   (tests/hardware/run_all.sh --pack does the picotool step for you.)
 *   A USB serial terminal shows the steps.
 *
 * WHAT TO LOOK FOR, STEP BY STEP (then they repeat)
 *   (Run it once BEFORE loading the pack: both screens say "No asset pack",
 *   with instructions, and it stops there.)
 *   At start, serial gives the firmware's size (well under 1024 KB), "Asset
 *   pack: OK", "Checksum: OK" and the time per lookup.
 *   1  Contents   screen A lists the 6 files (sizes, types, which are
 *                 transparent); screen B shows the welcome text
 *   2  Images     the banner, landscape, two d20s and a potion, all from the
 *                 pack, on both screens
 *   3  Errors     on screen A, "not found" in red for dice/d21.bmp and for
 *                 Dice/d20.bmp (capital D: names are case-sensitive), and OK
 *                 in green for dice/d20.bmp
 *
 * Program: qg4p_test_m7 (build/tests/hardware/qg4p_test_m7.uf2).
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "qg4p.h"
#include "qa4p.h"
#include "test_setup.h"
#define TEST_TAG "M7"
#include "test_log.h"

static qg_font_t f_body  = QG_FONT_INIT(qg_font_sans_16,      QG_DEFAULT, 1);
static qg_font_t f_title = QG_FONT_INIT(qg_font_sans_bold_24, QG_YELLOW,  1);
static qg_font_t f_mono  = QG_FONT_INIT(qg_font_mono_12,      QG_DEFAULT, 1);

static qg_screen_t *const screens[2] = { &scr_a, &scr_b };

/* The asset pack: a handle this program owns, filled in by qa_open(). */
static qa_pack_t pack;

extern char __flash_binary_end;   /* end of this firmware in flash (pico-sdk) */

/* -------------------------------------------------------------------------- */
/*  Helper: open an image from the pack in one call                           */
/* -------------------------------------------------------------------------- */
/*
 * This small bridge is all it takes to join the two modules: the pack says
 * where the bytes are and whether they're transparent; qg4p draws them.
 */
static bool open_asset_image(const char *name, qg_image_t *img)
{
    qa_file_t a;
    qa_err_t err = qa_find(&pack, name, &a);
    if (err != QA_OK) {
        TEST_DETAIL("%s: %s", name, qa_err_str(err));
        return false;
    }
    uint8_t flags = (a.flags & QA_FLAG_TRANSPARENT) ? QG_IMAGE_TRANSPARENT : 0;
    return qg_image_open(img, a.data, a.size, flags) == QG_OK;
}

/* -------------------------------------------------------------------------- */
static void show_no_pack(qa_err_t err)
{
    char buf[160];
    for (int i = 0; i < 2; i++) {
        qg_screen_t *s = screens[i];
        qg_cls(s, QG_BLACK);
        qg_locate(s, 6, 6);
        qg_println(s, "{f:1}{c:LIGHTRED}No asset pack");
        snprintf(buf, sizeof buf, "{c:YELLOW}%s", qa_err_str(err));
        qg_println(s, buf);
        qg_println(s, "");
        if (err == QA_ERR_OVERLAP) {
            qg_println(s, "The firmware is bigger than 1 MB. Move the pack "
                           "(the offset given to qa_open and mkpack.py --offset).");
        } else {
            qg_println(s, "On the PC:");
            qg_println(s, "{f:2}python3 tools/mkpack.py tests/hardware/pack --out build/assets");
            qg_println(s, "");
            qg_println(s, "Then hold BOOTSEL, plug in, and drag {c:LIGHTGREEN}assets.uf2{c:} onto the drive.");
        }
    }
}

/* ========================================================================== */
/*  Page 1: contents                                                          */
/* ========================================================================== */
static void page_contents(void)
{
    static const char *const type_names[] = { "other", "image", "sound", "text" };
    char buf[80];
    qa_file_t a;

    /* screen A: the file list. */
    qg_screen_t *s = &scr_a;
    qg_cls(s, QG_BLACK);
    qg_locate(s, 4, 4);
    qg_println(s, "{f:1}Asset pack");
    snprintf(buf, sizeof buf, "{f:2}%u files, %lu bytes", qa_count(&pack),
             (unsigned long)qa_size(&pack));
    qg_println(s, buf);
    qg_println(s, "");
    for (uint16_t i = 0; i < qa_count(&pack); i++) {
        qa_get(&pack, i, &a);
        snprintf(buf, sizeof buf, "{f:2}{c:WHITE}%s", a.name);
        qg_println(s, buf);
        snprintf(buf, sizeof buf, "{f:2}{c:DARKGRAY}  %lu B, %s%s", (unsigned long)a.size,
                 a.type < 4 ? type_names[a.type] : "?",
                 (a.flags & QA_FLAG_TRANSPARENT) ? ", transparent" : "");
        qg_println(s, buf);
    }

    /* Screen B: a text file, printed straight from flash. The pack doesn't add
     * a terminating NUL, so copy it into a buffer first (or print it with a
     * length-limited loop).                                                 */
    s = &scr_b;
    qg_cls(s, QG_BLACK);
    qg_locate(s, 6, 6);
    qg_println(s, "{f:1}From text/welcome.txt:");
    if (qa_find(&pack, "text/welcome.txt", &a) == QA_OK) {
        char text[200];
        uint32_t n = a.size < sizeof(text) - 1 ? a.size : sizeof(text) - 1;
        for (uint32_t i = 0; i < n; i++) text[i] = (char)a.data[i];
        text[n] = '\0';
        qg_println(s, text);
    }
}

/* ========================================================================== */
/*  Page 2: images                                                            */
/* ========================================================================== */
static void page_images(void)
{
    qg_image_t banner, landscape, d20, potion, d20_prebuilt;
    bool ok = open_asset_image("art/banner.bmp",      &banner)
            & open_asset_image("art/landscape.bmp",   &landscape)
            & open_asset_image("dice/d20.bmp",        &d20)
            & open_asset_image("items/potion.bmp",    &potion)
            & open_asset_image("ui/d20_prebuilt.bmp", &d20_prebuilt);
    if (!ok) return;

    for (int i = 0; i < 2; i++) {
        qg_screen_t *s = screens[i];
        const int16_t w = qg_screen_width(s);
        qg_cls(s, QG_BLACK);
        qg_image_draw_scaled(s, &banner, 0, 0, w, 48);
        qg_image_draw_fit(s, &landscape, 0, 50, w, 160, QG_ALIGN_CENTER);
        qg_image_draw(s, &d20, 8, 220);
        qg_image_draw(s, &d20_prebuilt, 80, 220);
        qg_image_draw_scaled(s, &potion, 156, 220, 64, 64);
    }
}

/* ========================================================================== */
/*  Page 3: errors                                                            */
/* ========================================================================== */
static void page_errors(void)
{
    qg_screen_t *s = &scr_a;
    qa_file_t a;
    char buf[96];

    qg_cls(s, QG_BLACK);
    qg_locate(s, 6, 6);
    qg_println(s, "{f:1}Missing files");
    const char *names[] = { "dice/d21.bmp", "Dice/d20.bmp", "dice/d20.bmp" };
    for (int i = 0; i < 3; i++) {
        qa_err_t err = qa_find(&pack, names[i], &a);
        snprintf(buf, sizeof buf, "%s\n  {c:%s}%s", names[i],
                 err == QA_OK ? "LIGHTGREEN" : "LIGHTRED", qa_err_str(err));
        qg_println(s, buf);
    }
    qg_println(s, "{c:DARKGRAY}(names are case-sensitive)");

    qg_cls(&scr_b, QG_BLACK);
}

/* ========================================================================== */
int main(void)
{
    test_setup(TEST_TAG, "qg4p_test_m7: images and text from the asset pack");
    for (int i = 0; i < 2; i++) {
        qg_screen_set_font(screens[i], 0, &f_body);
        qg_screen_set_font(screens[i], 1, &f_title);
        qg_screen_set_font(screens[i], 2, &f_mono);
    }

    /* Where things are in flash. */
    uintptr_t fw_end = (uintptr_t)&__flash_binary_end;
    TEST_LOG("Firmware: 0x10000000 - 0x%08lx (%lu KB)", (unsigned long)fw_end,
           (unsigned long)((fw_end - 0x10000000u) / 1024));
    TEST_LOG("Pack area starts at 0x%08lx", (unsigned long)(0x10000000u + QA_DEFAULT_OFFSET));

    qa_err_t err = qa_open(&pack, QA_DEFAULT_OFFSET);
    TEST_LOG("Asset pack: %s", qa_err_str(err));
    if (err != QA_OK) {
        TEST_LOG("No pack: load it (see test_m7.c), and the Pico restarts into this test");
        show_no_pack(err);
        while (true) tight_loop_contents();   /* load the pack; the Pico restarts */
    }

    uint64_t t0 = time_us_64();
    err = qa_verify(&pack);
    TEST_LOG("Checksum: %s (%lu bytes checked in %lu us)", qa_err_str(err),
           (unsigned long)qa_size(&pack), (unsigned long)(time_us_64() - t0));
    if (err != QA_OK) {
        show_no_pack(err);
        while (true) tight_loop_contents();
    }

    qa_file_t a;
    t0 = time_us_64();
    for (int i = 0; i < 1000; i++) qa_find(&pack, "items/potion.bmp", &a);
    TEST_LOG("qa_find: %lu ns per lookup", (unsigned long)(time_us_64() - t0));

    for (int pass = 0; ; pass++) {
        if (pass > 0) TEST_REPEAT();

        TEST_STEP(1, 3, "Contents", "file list on A, a text file on B", "6 files listed, the welcome text");
        page_contents();
        sleep_ms(5000);

        TEST_STEP(2, 3, "Images", "every image, from the pack", "banner, landscape, 2 d20s, a potion");
        page_images();
        sleep_ms(5000);

        TEST_STEP(3, 3, "Errors", "3 names looked up on A", "2 red 'not found', dice/d20.bmp OK");
        page_errors();
        sleep_ms(4000);

        TEST_PASS_DONE();
    }
}
