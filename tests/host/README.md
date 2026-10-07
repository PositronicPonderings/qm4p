# Host tests

The libraries' drawing, text, image, asset, framebuffer and sound code, run on a PC. Screens are replaced by fake ones that record pixels instead of sending them over SPI; the sound hardware by a fake that records samples instead of playing them; the Pico SDK by small stand-in headers in `stubs/`; and flash, for the asset library, by a pretend 4 MB flash chip in RAM (`QA_HOST_TEST`, see `qa4p/qa4p.h`).

```
sh tests/host/run_tests.sh
```

Needs gcc, Python 3, Pillow and numpy. Each check prints PASS or FAIL; logs are in `tests/host/build/`.

| Check | What it proves |
|---|---|
| text_units | Kerning (both forms), all four character-map forms, UTF-8 decoding |
| line_widths | Lines of width 1 to 8 are exactly that wide |
| text_edges | Text starting off the left or top edge lands exactly where it should, pixel for pixel: one glyph at x and y = 0, -1, -8, -15, -16, -17 (transparent, opaque, scaled), and centred lines wider than the screen (QG4P 1.1.0 put text at negative x one pixel too far right) |
| scroll_exact | After 40 lines of scrolling, both screen sizes match an unscrolled reference pixel for pixel (transparent and opaque text) |
| images_vs_pillow | 77 image draws (RLE8 and plain; scaled, stretched, clipped) match Pillow's decoder |
| images_rle_delta | RLE8 delta codes match the BMP specification (checked by hand: Pillow has a bug here) |
| asset_pack | Lookups, alignment, checksum, damage handling; two packs (`packs/one`, `packs/two`) open at once without mixing up their files; every function refusing a pack that isn't open; `qa_open` refusing a pack that runs past the end of flash, or that the firmware has grown into |
| mkpack_max_size | `mkpack.py --max-size` refuses a pack that's too big, with a non-zero exit status, and writes nothing |
| copy_the_folder | Each library stands alone: every `qg4p/` source compiles (warnings as errors) with only `qg4p/` on the include path, `qa4p/qa4p.c` with only `qa4p/`, and every `qs4p/` source with only `qs4p/` |
| buf8_* | Framebuffer screens draw exactly what direct screens do; flushes send only what changed; flood fill; scrolling; images; overlapping sprites |
| render_pages | Every page of every hardware test program renders with nothing drawn off-screen |
| render_examples | All 17 examples run unchanged against stand-in screens (`render_example.c`), each stopped at a representative moment (the dice roller also mid-roll, where leftover dice would show; the showcase at each moment in `showcase_stills.txt`), with nothing drawn off-screen |
| manual_examples | Every example in the manual (`docs/manual`) compiles, and runs without drawing off the screen. `python3 tests/host/doc_examples.py` (without `--check`) also refreshes the manual's pictures |
| manual_links | Every link and picture in the documentation points at a file that exists, and every `#anchor` at a real heading; every path a source file mentions (`see tests/hardware/test_s0.c`) exists |
| ai_disclosure | Every source file carries an `SPDX-AI-Disclosure` tag with a valid level, `AI_DISCLOSURE.md` exists, and each library folder (`qg4p/`, `qa4p/`, `qs4p/`) is among those checked |
| quick_reference | The one-page quick reference names every public function, and nothing that doesn't exist |
| golden_images | Every rendered page and example screen (95 in all, the showcase stills included) matches its recorded SHA-256 fingerprint |
| qs_engine | QS4P's engine and tones against a stand-in backend that records every sample (`test_qs.c`): pitch within 1% from 37 Hz to 11,024 Hz; lengths within one buffer; the peak at 100% volume is the ceiling's level, and 25/50/75% scale with it; bad frequencies give `QS_ERR_RANGE` and play nothing; `qs_stop()` fades to nothing within 5 ms plus two buffers; `QS_TICKS(18)` is 989; and in every test, the amplifier switches on in silence, waits 20 ms, stays on 100 ms after the sound, and the backend stops once it's off. Each test's sound is written to `out/*.wav` to listen to |
| qs_pwm_backend | The PWM backend against a pretend SDK that logs every call (`test_qs_pwm.c`): bad configs touch nothing; amp off before anything else; the PWM at 50% before the pin is handed over; the timer at 7/47,619 of 150 MHz (22,050 a second); DMA to the right compare register; a shared handler on DMA_IRQ_1 that ignores other channels; the levels written; `qs_deinit()` switching the amp off first and handing everything back |
| golden_sounds | Every `out/*.wav` matches its recorded SHA-256 fingerprint (`golden_sounds.sha256`) |
| qs_integer_only | QS4P's code has no `float`, `double` or decimal point outside its comments: the sound runs in an interrupt, on whole numbers |
| showcase_still | While the showcase's dice lie still and its closing card is up, nothing is sent to the screens except the two things that appear (the total, the card). A finished picture can be right while being redrawn every frame, which flickers on real glass; `render_example.c` counts what's sent per frame (`DRAWS=first:last`) |
| test_board_only | The hardware tests' settings live in one place: no file but `tests/hardware/test_board.h` defines `SCREEN_B_BOARD` or a `PIN_` setting |
| serial_format | Every hardware test prints through `tests/hardware/test_log.h`: one tagged line per step, under 100 characters, steps 1/N to N/N, each pass ending and repeating; and `run_all.sh` lists the same steps (`check_serial.py`) |

**Reference pictures** for people comparing real screens (`tests/hardware/expected/`, `examples/expected/`) are made from these renders: `python3 tests/host/make_expected.py`. The README's showcase pictures (`docs/img/`) come from `python3 tools/make_readme_images.py`, which renders the moments in `showcase_stills.txt` (and, for the GIF, every frame of a stretch: `render_example.c`'s `FRAMES=first:last:step`) and lays the two screens side by side.

**When a golden image changes on purpose** (you changed a test page or improved drawing), inspect the new image in `build/`, then replace its line in `golden.sha256` with the output of `sha256sum build/<name>.ppm`. Sounds work the same way: listen to the new `out/<name>.wav`, then replace its line in `golden_sounds.sha256` with the output of `sha256sum <name>.wav`, run in `out/`.
