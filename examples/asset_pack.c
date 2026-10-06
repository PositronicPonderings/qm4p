/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    asset_pack.c
 * @brief   Example 7: images and text loaded BY NAME from an asset pack,
 *          which lives in flash separately from the program.
 *
 * Target: qg4p_asset_pack (links qa4p, the asset library).   Screens: A.
 * Expected: examples/expected/asset_pack.png (and _nopack.png without one)
 *
 * Build and load the pack (examples/pack/ holds its files):
 *     python3 tools/mkpack.py examples/pack --out build/example_assets
 * then hold BOOTSEL, plug in, and drag build/example_assets.uf2 onto the
 * drive. Or: picotool load build/example_assets.bin -o 0x10100000
 *
 * Changing the art now means rebuilding the PACK, not the program. Run this
 * once before loading the pack to see the polite "no pack" screen.
 */
#include "pico/stdlib.h"
#include "board.h"
#include "qa4p.h"

/* The pack. It's a small "handle" this program owns: qa_open() fills it in,
 * and every lookup says which pack to look in. Static, so it starts as
 * zeros ("not open") and lasts as long as the program.                    */
static qa_pack_t art;

/* Find a file in the pack and open it as an image: the pack says where the
 * bytes are and whether they're see-through; the graphics library draws.  */
static bool open_image(const char *name, qg_image_t *img)
{
    qa_file_t f;
    if (qa_find(&art, name, &f) != QA_OK) return false;
    return qg_image_open(img, f.data, f.size,
                         (f.flags & QA_FLAG_TRANSPARENT) ? QG_IMAGE_TRANSPARENT : 0) == QG_OK;
}

int main(void)
{
    board_init();
    qg_screen_t *s = &screen_a;
    qg_cls(s, QG_BLACK);

    /* The pack lives 1 MB into flash: mkpack.py's default --offset. */
    qa_err_t err = qa_open(&art, QA_DEFAULT_OFFSET);
    if (err != QA_OK) {
        qg_locate(s, 6, 6);
        qg_println(s, "{f:1}{c:LIGHTRED}No asset pack");
        qg_println(s, "{c:YELLOW}The program is fine. It just has nothing to show.");
        qg_println(s, "");
        qg_println(s, "{f:2}python3 tools/mkpack.py examples/pack --out build/example_assets");
        qg_println(s, "");
        qg_println(s, "Then hold BOOTSEL, plug in, and drag {c:LIGHTGREEN}example_assets.uf2{c:} onto the drive.");
        while (true) tight_loop_contents();
    }

    /* Images, straight from flash. */
    qg_image_t scene, star;
    if (open_image("art/scene.bmp", &scene)) qg_image_draw_fit(s, &scene, 0, 0, 240, 160, QG_ALIGN_CENTER);
    if (open_image("icons/star.bmp", &star)) {
        for (int k = 0; k < 5; k++) qg_image_draw(s, &star, (int16_t)(10 + k * 45), 20);
    }

    /* A text file, printed. Files aren't NUL-terminated, so copy it into a
     * buffer first (or print a length-limited slice).                     */
    qa_file_t note;
    if (qa_find(&art, "text/note.txt", &note) == QA_OK) {
        static char buf[256];
        uint32_t n = note.size < sizeof buf - 1 ? note.size : sizeof buf - 1;
        for (uint32_t i = 0; i < n; i++) buf[i] = (char)note.data[i];
        buf[n] = '\0';
        qg_print_box(s, 8, 166, 224, buf, QG_ALIGN_LEFT);
    }

    /* Asking for something that isn't there is not a crash, just a no. */
    qa_file_t missing;
    err = qa_find(&art, "icons/unicorn.bmp", &missing);
    qg_print_at(s, 8, 303, "icons/unicorn.bmp:", QG_DARKGRAY, &font_small);
    qg_print_at(s, 150, 303, qa_err_str(err), QG_LIGHTRED, &font_small);

    while (true) tight_loop_contents();
}
