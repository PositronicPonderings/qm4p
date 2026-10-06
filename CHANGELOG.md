# Changelog

## QM4P 0.1.0 (2026-10-06)

The repository becomes **QM4P, QuickMedia 4 Pico**: an umbrella for small, self-contained Pico libraries, each in its own folder. Library versions in this release: **QG4P 1.1.0** (graphics; no graphics changes) and **QA4P 1.0.0** (asset packs; new). QS4P (sound) is planned.

### Changed
- **The asset reader is now its own library, QA4P** (`qa4p/`, CMake target `qa4p`, header `qa4p.h`), moved out of `qg4p/assets/`. It needs only the Pico SDK, nothing from QG4P, and QG4P needs nothing from it: copy `qa4p/` only if you use an asset pack. The `qg4p_assets` target is gone; link `qa4p` instead.
- **Packs are handles.** The reader keeps no state of its own: each open pack is a `qa_pack_t` your program owns, passed to every function, so several packs (graphics, sound, ...) can be open at once. A `qa_pack_t` must start as zeros (declare it `static`, or `= {0}`). Every function given a pack that isn't open (never opened, failed to open, or `NULL`) returns `QA_ERR_NOT_READY`, or 0 for the count and size.
- **New prefix, `qa_`.** To migrate:

  | QG4P 1.0 | QM4P 0.1.0 |
  |---|---|
  | `#include "qg_asset.h"`, link `qg4p_assets` | `#include "qa4p.h"`, link `qa4p` |
  | `qg_asset_init()` | `qa_open(&pack, QA_DEFAULT_OFFSET)` |
  | `qg_asset_init_at(addr)` | `qa_open_at(&pack, addr)` |
  | `qg_asset_find(name, &a)` | `qa_find(&pack, name, &f)` |
  | `qg_asset_count()` | `qa_count(&pack)` |
  | `qg_asset_get(i, &a)` | `qa_get(&pack, i, &f)` |
  | `qg_asset_pack_size()` | `qa_size(&pack)` |
  | `qg_asset_verify()` | `qa_verify(&pack)` |
  | `qg_asset_err_str(err)` | `qa_err_str(err)` |
  | `qg_asset_t` | `qa_file_t` |
  | `qg_asset_err_t` | `qa_err_t` |
  | `QG_ASSET_PACK_OFFSET` | `QA_DEFAULT_OFFSET` (still `0x100000`; a convenience to pass to `qa_open`, not an assumption) |
  | `QG_ASSET_NAME_MAX` | `QA_NAME_MAX` |
  | `QG_ASSET_TYPE_*` | `QA_TYPE_*` |
  | `QG_ASSET_FLAG_TRANSPARENT` | `QA_FLAG_TRANSPARENT` |
  | `QG_ASSET_OK`, `QG_ASSET_ERR_*` | `QA_OK`, `QA_ERR_*` |
  | `QG_ASSET_HOST_TEST` | `QA_HOST_TEST` |

- **The asset pack's identifying bytes changed from `QGPK` to `QAPK`: rebuild packs with `tools/mkpack.py`.** An old pack now reads as "no asset pack in flash". The layout is otherwise identical (pack version still 1).
- **No more 3-second pause at start-up.** The hardware tests and examples no longer `sleep_ms(3000)` to give a serial terminal time to connect. Instead they're built with `PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=2000`: `stdio_init_all()` waits until a terminal connects, at most 2 seconds, so with a terminal open they start at once and lose nothing, and without one they still start. (It costs 96 bytes of flash in every program, the SDK's waiting loop.)
- The top-level CMake project is now `qm4p`; it builds `qg4p`, `qa4p`, the examples and the hardware tests. Program names stay `qg4p_*`: they are graphics programs, and sound programs will be `qs4p_*`.
- The manual (still in `docs/manual`) documents QA4P in [Asset packs](docs/manual/reference/assets.md), with a new section on two packs open at once, and says to copy `qa4p/` alongside `qg4p/` when using a pack. The quick reference, A-to-Z index, AI guide and README follow.

### Added
- `qa_open` checks the pack fits in flash: `QA_ERR_TOO_BIG` ("asset pack runs past the end of flash") if its offset plus its size passes the end of the chip (`PICO_FLASH_SIZE_BYTES`). It still refuses a pack the firmware has grown into (`QA_ERR_OVERLAP`). `qa_open_at`, for packs in RAM or compiled-in arrays, makes no flash checks. Every other error message is unchanged.
- `mkpack.py --max-size BYTES`: the largest pack allowed, by default the space from `--offset` to the end of a 4 MB flash. A bigger pack is refused, with both sizes in the message and a non-zero exit status. The summary now prints the flash range the pack occupies, and the 4 KB sectors loading it will erase, so you can check two packs don't overlap.
- `tests/hardware/run_all.sh`: flashes each hardware test in turn with `picotool load -f -x` (no BOOTSEL button), says what to look for, asks pass / fail / reload / quit, and ends with a table. `--pack` loads M7's asset pack first; `--from m4` starts part-way through.
- `docs/RESOURCES.md`: the hardware resource register. Every SPI block, DMA channel, PWM slice, pin, flash area and fixed buffer each library uses, how each is chosen, the flash plan (firmware from 0, graphics pack from 1 MB, sound pack from 2.5 MB), pins reserved for QS4P (GP2, GP3, GP9 to GP11) and kept free for the ADC (GP26 to GP28), and the known clashes. Every future library or feature updates it in the same commit.
- Host tests: two packs open at once without mixing up their files; every function refusing a pack that isn't open; `QA_ERR_TOO_BIG` and `QA_ERR_OVERLAP` from `qa_open` (the host build gives `qa_open` a pretend 4 MB flash to read); `mkpack.py --max-size` refusing an oversized pack; and the copy-the-folder promise (every `qg4p/` source compiles with only `qg4p/` on the include path, and `qa4p.c` with only `qa4p/`). The asset example and the manual's pack examples now run through the real `qa_open`, with no stand-in function.

## 1.0.1 (2026-10-01)

### Fixed
- `docs/WIRING.md`: the screens are screen A and screen B (not "DM" and "player"), the setting for screen B's board is `SCREEN_B_BOARD` (was `PLAYER_BOARD`), and the guide no longer refers to the development chats ("your photo", "your POC"); it now gives general power and jumper advice.
- Host tests: internal file names and labels now use A/B. No drawing changed: every fingerprint still matches.
## 1.0.0 (2026-09-30)

The first public release.

### Added
- **"Too busy to read a manual"** (`docs/manual/quick-reference.md`): every function on one page, with set-up, colours, markup and the eight things that bite. Kept complete by `tests/host/check_quickref.py`.
- Published by [Positronic Ponderings](https://github.com/PositronicPonderings), now the copyright holder in `LICENSE`.
- **AI disclosure:** `AI_DISCLOSURE.md`, and an `SPDX-AI-Disclosure` tag (with model and provider) in every source file, following the [ai-disclosure convention](https://github.com/ggfevans/ai-disclosure) (v0.1). Checked by `tests/host/check_disclosure.py`, part of `run_tests.sh`.
- **The manual** (`docs/manual`): quick answers; all 16 examples; a reference entry for every public function, each with a working example and a picture of what it draws; the tools; troubleshooting; getting started (wiring, choosing pins, three two-screen layouts, CMake in five minutes); adding a new display chip; how it works; and appendices (error codes, settings, markup card, memory and speed, glossary, history).
- The manual's 79 examples are compiled (and 70 run and pictured) by `tests/host/doc_examples.py`, and every link and picture checked by `tests/host/doc_links.py`; both are part of `run_tests.sh`.
- `qg_bus_init` now refuses SCK or MOSI pins the named SPI can't use (they used to give a silent black screen).
- **VIEW:** `qg_view()`, `qg_view_pct()`, `qg_view_reset()`, `qg_view_width()`, `qg_view_height()`. Clip all drawing to a rectangle, optionally moving the origin (and percentages) to it. `qg_cls` clears just the view; text wraps at its edge.
- **GET/PUT:** `qg_get()`, `qg_put()` with PSET, PRESET, AND, OR, XOR and TRANSPARENT modes (`qg_block.h`). GET and the bitwise modes need a framebuffer screen.
- **LINE styles:** `qg_screen_set_line_style()`, a 16-bit pattern for lines and box outlines, thin or thick.
- **PRESET:** `qg_preset()`. **CSRLIN/POS:** `qg_csrlin()`, `qg_pos()`.
- **Colour adjustment:** `qg_screen_set_color_adjust()`, a gain and gamma for each of red, green and blue, per screen, applied where colours leave for the panel (everything else still sees the colours asked for). No per-pixel cost; 1.3 KB per screen.
- Example 16, `calibrate`: tune a panel's adjustment live from the USB serial monitor; prints a line for `board.h`, whose new `BOARD_A_ADJUST` / `BOARD_B_ADJUST` settings every example applies.
- `qg_palette_get()`: read a palette entry back as 8-bit RGB, exactly as the screen stores and sends it.
- `qg_palette_set()` now accepts entry 255, for framebuffer pixels that bitwise PUT leaves at 255.
- `tests/hardware/new_commands_demo.c` (target `qg4p_new_commands`); `tests/host/test_new_commands.c` (29 checks).
- **Examples:** 14 programs in `examples/`, from `hello` to a two-screen dice roller, sharing one wiring file (`board.h`). Each has a reference picture in `examples/expected/`, and each is run on every host-test pass.
- **Reference pictures** for every hardware test page, in `tests/hardware/expected/`.
- Example 15, `colour_check`: a colour test pattern (named colours with the RGB values sent, labelled pure colours, ramps, corner labels), with a "Help, my red looks blue" guide in `examples/README.md`.
- `tools/size_audit.py`: flash and RAM for every program in a build, which library files each includes, a JSON record, and comparison against an earlier build. For spotting features linked where they aren't used.
- `tools/vscode/qg4p_tasks.json`: a VS Code task that asks which program to run, compiles, and loads it with `picotool load -f -x` (no BOOTSEL button needed once a QG4P program is running).

### Removed
- Personal credits and the project history from the documentation; the AI disclosure records how the code was produced instead. Source notes (the fonts, and the published start-up sequences two drivers follow) remain.
- `QG_ERR_STATE`: an error code no function returned.
- `QG_MAX_SCREENS`: a setting nothing used. There's no limit on the number of screens.
- `tests/host/render_hello.c`: `hello.c` now uses `board.h` like the other examples, and the example harness (`render_example.c`) renders it along with the rest. Its fingerprint and reference picture moved with it (`ex/hello.ppm`, `examples/expected/hello.png`).

### Fixed
- `dice_roller` example: the previous roll's dice were never erased, so the new dice chipped pieces out of them as they tumbled past. The table is now redrawn at the start of each roll, and the tests now also check a frame mid-roll, where this kind of leftover shows.
- Thick styled lines: one-pixel dashes (dots) were drawn as squares as wide as the line, so neighbouring dots merged into a solid line.
- `img2bmp8.py` could make an image *bigger* by compressing it: busy or dithered pixels give RLE8 few runs to work with. It now keeps whichever of RLE8 and plain is smaller, and says so.

### Changed
- **Pay only for what you use.** An audit found features whose memory or code every program paid for, used or not:
  - Text scrolling history (4 KB per screen) is now memory you provide in the screen config (`.text_history`, `.text_history_lines`). Without it, printing past the bottom clears and starts again at the top. Framebuffer screens never needed it.
  - Colour adjustment tables (1.3 KB per screen) are now a `qg_color_adjust_state_t` you pass to `qg_screen_set_color_adjust()`; unadjusted screens carry nothing.
  - The framebuffer backend (its code and 4 KB of flush buffers) was linked into every program, because `qg_screen_init()` named it. `QG_BACKEND_DIRECT` and `QG_BACKEND_BUF8` are now references to the backends themselves (same spelling in your code), so only programs that mention `QG_BACKEND_BUF8` include it. The framebuffer's image colour-match cache (1 KB) moved into it too.
  - `QG_TEXT_CELL_PIXELS 0` now removes the opaque-text cell buffer (4 KB) entirely.
  - All three chip drivers were linked into every program (about 950 bytes of flash), through a lookup table naming them all. `QG_DRIVER_ST7789` / `QG_DRIVER_ILI9341` / `QG_DRIVER_ST7796` are now references to the drivers themselves (same spelling in your code), so a program carries only the drivers it names. `qg_driver_get()` is gone. Found with `tools/size_audit.py`.
  - Result: a screen object shrank from 5,992 to 684 bytes, and a two-screen DIRECT program uses about 17 KB instead of about 25 KB (details in `docs/SIZES.md`).
- `tests/hardware/demo_setup.c`: likewise, only `demo_setup_ex()` mentions the framebuffer backend, so tests that don't use one don't carry it (found by a size report of `qg4p_m5`: 968 bytes of flash and 5 KB of RAM of unused framebuffer).
- `examples/board.h`: `board_init()` / `board_init_fb()` / `board_init_two()` / `board_init_two_fb()` replace passing NULL for "no framebuffer", so examples without framebuffers don't link the framebuffer code. New settings `BOARD_x_ADJUSTED` and `BOARD_x_TEXT_HISTORY` declare that memory only when wanted.
- The library is now **QG4P**. Everything is renamed: `gfx_*` to `qg_*`, `GFX_*` to `QG_*`, `gfx.h` to `qg4p.h`, the `gfx/` folder and CMake target to `qg4p`; the asset reader is `qg_asset_*` / `QG_ASSET_*` in the `qg4p_assets` library.
- The asset pack's identifying bytes changed from `DRPK` to `QGPK`: rebuild existing packs with `tools/mkpack.py`.
- `tools/ttf2gfx.py` is now `tools/ttf2qg.py`.
- Repository reorganised: `examples/`, `tests/hardware/` (formerly `demo/`, one build target per program), `tests/host/`.
- Licence: MIT-0; bundled fonts under their DejaVu licence.

Verified: every host test passes, and 53 of 58 rendered test pages are byte-identical to the pre-rename library (and stayed so after the new commands were added, so they change nothing for existing code). The other 5 differ only in their deliberately changed on-screen text.

## Development history (pre-release)

Milestones M0 to M9 built and tested the library as the graphics layer of a tabletop dice roller; see `docs/MILESTONES.md`.
