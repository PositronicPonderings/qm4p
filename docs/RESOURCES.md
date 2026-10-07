<!-- SPDX-License-Identifier: MIT-0 -->
<!-- SPDX-AI-Disclosure: ai-generated -->
<!-- SPDX-AI-Model: claude-opus-5-5 -->
<!-- SPDX-AI-Provider: Anthropic -->
# Hardware resource register

Every QM4P library uses some of the Pico's hardware: pins, an SPI block, DMA channels, PWM slices, flash, RAM. This page lists all of it in one place, so the libraries (and your own code) can share one chip without quietly fighting over a piece of it. The chip is the Pico 2's RP2350A.

A resource is chosen in one of three ways:

- **claimed at run time**: the library asks the Pico SDK for a free one (for example `dma_claim_unused_channel`). Two libraries that both claim can't collide.
- **fixed**: the library always uses the same one.
- **set by config**: your program chooses, in a config struct or a setting.

Defaults are the dev board's wiring (layout B in [getting started](manual/07-getting-started.md#layouts-for-two-screens)), as set in `examples/board.h` and `tests/hardware/test_board.h`.

---

## QG4P (graphics), `qg4p/`

| Resource | How it's chosen | Default | Notes |
|---|---|---|---|
| SPI block | set by config: `qg_bus_config_t.spi` | `spi0` | One per bus; several screens share a bus. A second bus can use `spi1` (layout C). Transmit only: no MISO pin. Each transaction sets the baud rate and frame size (8 or 16 bits) for its screen, so another device on the same SPI block would have its settings changed under it |
| SPI pins: SCK, MOSI | set by config | GP18, GP19 | Must belong to that SPI block: GP n is SPI (n / 8) mod 2, SCK if n mod 4 = 2, MOSI if 3. `qg_bus_init` checks |
| GPIO: DC, RST | set by config | GP20, GP21 | Plain outputs, shared by every screen on the bus. RST may be `QG_PIN_NONE` |
| GPIO: CS, one per screen | set by config (`cs_pins` on the bus, `cs_pin` per screen) | GP17 (A), GP22 (B) | Plain outputs driven by the library, not the SPI block's own CSn, so CS stays low for a whole transaction |
| DMA channel | claimed at run time (`dma_claim_unused_channel(true)`) | one per bus | Stops with a panic at start-up if none is free. Waited on by polling: no DMA interrupt |
| PWM: backlights | set by config (`bl_pin`); the slice and channel follow from the pin | GP16 = slice 0 A (screen A), GP15 = slice 7 B (screen B) | 10 kHz (`QG_BL_PWM_HZ`), 100 steps (wrap 99), integer divider worked out from `clk_sys` when the slice is first set up. Each slice is set up once. `QG_PIN_NONE`: no backlight control. See [known clashes](#known-clashes) |
| PIO | not used | | |
| IRQs and handlers | none | | The library installs no interrupt handlers |
| Timers and alarms | none claimed | | Start-up waits (bus reset about 135 ms, each screen's start-up sequence) use `sleep_ms()`, which borrows the SDK's default alarm pool (hardware alarm 3) like any sleep |
| Core | whichever core calls it | core 0 | Not thread-safe: drawing shares static work buffers. Call every `qg_` function from one core |
| Flash | code and constant data only | in the firmware | Fonts and compiled-in images live with the code. Never writes or erases flash |
| RAM: buffers inside the library | set by config (`qg4p/qg_config.h`) | about 22 KB if every feature is used | Each is linked only if its feature is used: text cell 4 KB (`QG_TEXT_CELL_PIXELS`), image buffers about 7.5 KB (`QG_IMAGE_MAX_WIDTH`, `QG_IMAGE_BLOCK_PIXELS`), flush chunks 4 KB (`QG_BUF8_CHUNK_PIXELS`), flood fill 4 KB (`QG_PAINT_STACK`), and smaller ones. Full list: [`SIZES.md`](SIZES.md) |
| RAM: declared by your program | yours | | Screens (684 B each), framebuffers (width x height), text history (126 B a line), colour adjustment (1,290 B). See [`SIZES.md`](SIZES.md#memory-you-provide) |

## QA4P (asset packs), `qa4p/`

| Resource | How it's chosen | Default | Notes |
|---|---|---|---|
| Flash: pack areas | set by config: the offset given to `qa_open` (the same as `mkpack.py --offset`) | 1 MB in, `0x10100000` (`QA_DEFAULT_OFFSET`) | Read in place through XIP (flash appears in memory); never writes or erases. `qa_open` checks each pack starts after the firmware (`__flash_binary_end`) and ends inside the chip (`PICO_FLASH_SIZE_BYTES`). It can't see your other packs: see the [flash plan](#flash-plan) |
| RAM: pack handles | declared by your program | | `qa_pack_t`, 12 bytes per open pack. The library itself keeps no variables |
| RAM: CRC table | fixed | 1 KB | Only in programs that call `qa_verify()` |
| SPI, DMA, PWM, PIO, IRQs, timers | not used | | |
| Core | whichever core calls it | | Lookups only read flash and the handle |

## QS4P (sound), `qs4p/`

In development. Until the library arrives (milestone S1), the bring-up test S0 (`tests/hardware/test_s0.c`) drives the sound hardware directly, with no library:

| Resource | How it's chosen | Default | Notes |
|---|---|---|---|
| PWM: audio | set in `tests/hardware/test_board.h` (`PIN_AUDIO`); the slice follows from the pin | GP2 = slice 1 A | Divider 1, so the counter runs at the full 150 MHz: wrap 255 (8 bits) gives a 586 kHz carrier, wrap 1023 (10 bits) 146 kHz |
| GPIO: amplifier shutdown | set in `test_board.h` (`PIN_AMP_SD`) | GP3 | A plain output: high = amp on. GP3 is PWM slice 1 B, so while it's the shutdown pin, slice 1 B can't do PWM for anything else; see [known clashes](#known-clashes) |
| Timer | a repeating timer from the SDK's default alarm pool | | 22,050 callbacks a second, each writing one sample to the PWM. The default alarm pool is the one `sleep_ms()` uses (hardware alarm 3) |
| DMA, PIO, IRQs of its own | not used | | |

---

## Flash plan

Flash appears in memory from `0x10000000`. A Pico 2 has 4 MB.

| Area | Offset | Address | Size | Status |
|---|---|---|---|---|
| Firmware | 0 | `0x10000000` | up to 1 MB | in use |
| Graphics pack | `0x100000` (1 MB) | `0x10100000` | up to 1.5 MB | in use: `QA_DEFAULT_OFFSET`, the examples' and tests' packs |
| Sound pack | `0x280000` (2.5 MB) | `0x10280000` | up to 1.5 MB | planned (QS4P) |
| End of flash | `0x400000` (4 MB) | `0x10400000` | | |

- `qa_open` catches a firmware that has grown into a pack, and a pack that runs off the end of flash. **Packs overlapping each other are up to you:** `mkpack.py` prints the flash range each pack occupies, so compare them.
- Loading a pack erases whole 4 KB sectors, so a pack really claims everything up to the end of its last sector; `mkpack.py` prints that too.
- `mkpack.py`'s default `--max-size` runs to the end of flash. With a sound pack planned, build the graphics pack with `--max-size 0x180000` (1.5 MB) so it's refused before it grows into the sound area.

---

## Pins at a glance

The dev board's wiring, and what's being kept free.

| GP | Used by | For |
|---|---|---|
| 2 | sound (test S0; QS4P from S1) | PWM audio (slice 1 A) |
| 3 | sound (test S0; QS4P from S1) | amplifier shutdown: a plain GPIO, so PWM slice 1 B is taken |
| 9, 10, 11 | reserved: QS4P | I2S (on the RP2350, I2S is done with PIO) |
| 15 | QG4P | backlight B (PWM slice 7 B) |
| 16 | QG4P | backlight A (PWM slice 0 A) |
| 17 | QG4P | CS, screen A |
| 18, 19 | QG4P | SCK, MOSI (`spi0`) |
| 20, 21 | QG4P | DC, RST |
| 22 | QG4P | CS, screen B |
| 26, 27, 28 | kept free | ADC: battery monitoring |

---

## Known clashes

**Two pins on one PWM slice share its frequency and wrap.** They dim independently (each output has its own level), but they can't run at different PWM frequencies. Which slice and output a pin drives is fixed by its number: **slice = (GP / 2) mod 8; output A for an even GP, B for an odd one.** So GP2 is slice 1 A, GP3 is 1 B, GP15 is 7 B, GP16 is 0 A, GP26 is 5 A. QG4P sets up a backlight's slice once, at 10 kHz, and knows nothing of other libraries: if sound used the other output of a backlight's slice, whichever set the slice up last would decide the frequency for both. Keep audio PWM on a slice no backlight uses. (On the 48-pin RP2350B, GP32 and up drive extra slices 8 to 11.)

**GP2 (PWM slice 1 A) is reserved for QS4P audio.** The default dev-board wiring, layout B, leaves it free. Layouts A and C in [getting started](manual/07-getting-started.md#layouts-for-two-screens) use GP2 as SCK: choose layout B if sound is planned.

**GP9 to GP11 are reserved for QS4P's I2S, and GP3 for the amplifier's shutdown pin.** Layout A uses GP3 (MOSI) and GP9 (backlight B); layout C uses GP3, GP10 and GP11.

**GP3 takes PWM slice 1 B out of service.** It's the other output of the audio slice (GP2 is 1 A), used as a plain GPIO for the amplifier's shutdown pin. The slice's counter runs at the audio carrier rate, so its B output couldn't dim a backlight anyway: keep backlights and anything else that needs PWM off slice 1.

**GP26 to GP28 stay free for the ADC** (battery monitoring). They are the only pins that can read a voltage.

**DMA channels are always claimed, never fixed.** Every QM4P library asks the SDK for a free channel, so they can't collide with each other. Code that uses a fixed channel number should claim it first (`dma_channel_claim`) before any library starts, or it may take one a library already has.

**A QG4P bus owns its SPI block.** A different SPI device (an SD card, say) belongs on the other SPI block, not on a screen bus.

---

**Keeping this page true:** every future library or feature that uses a pin, a peripheral, a DMA channel, a PWM slice, a PIO state machine, an interrupt, a timer, flash or a fixed buffer updates this file **in the same commit**.
