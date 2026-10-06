#!/usr/bin/env python3
# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
"""
make_readme_images.py - the README's pictures of the showcase, made on a PC.

EXAMPLE
    python3 tools/make_readme_images.py

WHAT IT MAKES (in docs/img/)
    showcase_<name>.png   still moments of examples/showcase.c, both screens
                          side by side at 1:1, as they sit on the bench: the
                          gap between them as a dark bezel, a thin outline
                          round each screen. Which moments: the list in
                          tests/host/showcase_stills.txt.
    showcase.gif          the bouncing ball scene and the start of the
                          scrolling text, looping forever.

HOW
    The showcase runs unchanged on the PC against stand-in screens, the same
    way tests/host/run_tests.sh renders every example (render_example.c):
    this script compiles it with gcc, stops it at each chosen moment, and
    saves each screen. Then it lays the two screens out with Pillow, using
    examples/board.h's BOARD_LEFT_SCREEN and BOARD_GAP_PX, just as the
    showcase does on the hardware.

    The showcase is driven by frame numbers and seeds its random numbers
    the same way every loop, so the pictures come out the same every time.
    The screens behind the stills are fingerprinted in tests/host/golden.sha256,
    so a change to how the showcase looks fails the host tests until the
    fingerprints (and these pictures) are made again. The GIF isn't
    fingerprinted: Pillow versions encode GIFs differently.

NEEDS
    gcc, Python 3 and Pillow (pip install pillow). No Pico.
"""
import os
import re
import shutil
import subprocess
import sys

from PIL import Image, ImageDraw

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
HOST = os.path.join(ROOT, "tests", "host")
BUILD = os.path.join(HOST, "build", "readme")
OUT = os.path.join(ROOT, "docs", "img")

BEZEL = (26, 26, 30)          # the dark frame round and between the screens
OUTLINE = (110, 110, 120)     # the thin line round each screen
MARGIN = 16                   # bezel round the outside, in pixels

# The GIF: which sleep_ms() calls to capture (see showcase_stills.txt for the
# numbering), every GIF_STEP-th one. The ball scene is calls 302-481 (6 s);
# the scrolling text starts at 482, and the GIF shows its first 5 seconds.
# Every frame, as on the screens: about 300 KB. If it ever grows past
# GIF_MAX_BYTES, take every 2nd frame (GIF_STEP 2), or fewer colours.
GIF_FIRST, GIF_LAST, GIF_STEP = 302, 631, 1
GIF_COLOURS = 64
GIF_MAX_BYTES = 1500 * 1024


def run(cmd, **kw):
    subprocess.run(cmd, check=True, **kw)


def build_renderer():
    """Compile the showcase against the stand-in screens: build/readme/r_showcase."""
    os.makedirs(BUILD, exist_ok=True)
    lib, qa, ex = (os.path.join(ROOT, d) for d in ("qg4p", "qa4p", "examples"))
    flags = ["-std=c11", "-O1", "-w", "-I" + os.path.join(HOST, "stubs_examples"),
             "-I" + os.path.join(HOST, "stubs"), "-I" + lib, "-I" + qa, "-I" + ex, "-DQA_HOST_TEST"]
    sources = [os.path.join(lib, f) for f in (
        "qg_draw.c", "qg_draw_pct.c", "qg_block.c", "qg_palette.c", "qg_text.c", "qg_image.c",
        "fonts/qg_font_mono_12.c", "fonts/qg_font_sans_16.c", "fonts/qg_font_sans_bold_24.c",
        "backend/qg_backend_buf8.c")]
    sources += [os.path.join(qa, "qa4p.c"), os.path.join(ex, "example_art.c")]
    obj = os.path.join(BUILD, "showcase.o")
    run(["gcc"] + flags + ["-Dmain=example_main", "-c", os.path.join(ex, "showcase.c"), "-o", obj])
    exe = os.path.join(BUILD, "r_showcase")
    run(["gcc"] + flags + ["-o", exe, os.path.join(HOST, "render_example.c"), obj] + sources + ["-lm"])
    return exe


def board_layout():
    """(left screen 'a' or 'b', gap in pixels) from examples/board.h."""
    text = open(os.path.join(ROOT, "examples", "board.h"), encoding="utf-8").read()
    left = re.search(r"#define\s+BOARD_LEFT_SCREEN\s+(\w+)", text).group(1)
    gap = int(re.search(r"#define\s+BOARD_GAP_PX\s+(\d+)", text).group(1))
    return ("b" if left in ("BOARD_SCREEN_B", "1") else "a"), gap


def stills():
    """[(name, stop)] from tests/host/showcase_stills.txt."""
    out = []
    for line in open(os.path.join(HOST, "showcase_stills.txt"), encoding="utf-8"):
        line = line.split("#")[0].split()
        if len(line) >= 2:
            out.append((line[0], int(line[1])))
    return out


def side_by_side(a, b, left, gap):
    """Both screens on the bench: the left one, the gap, the right one, each
    centred vertically against the other, on a dark bezel."""
    l, r = (a, b) if left == "a" else (b, a)
    world_w, world_h = l.width + gap + r.width, max(l.height, r.height)
    img = Image.new("RGB", (world_w + 2 * MARGIN, world_h + 2 * MARGIN), BEZEL)
    draw = ImageDraw.Draw(img)
    for screen, x0 in ((l, 0), (r, l.width + gap)):
        x, y = MARGIN + x0, MARGIN + (world_h - screen.height) // 2
        draw.rectangle([x - 1, y - 1, x + screen.width, y + screen.height], outline=OUTLINE)
        img.paste(screen, (x, y))
    return img


def main():
    left, gap = board_layout()
    exe = build_renderer()
    os.makedirs(OUT, exist_ok=True)
    work = os.path.join(BUILD, "frames")
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)

    # The stills.
    for name, stop in stills():
        prefix = os.path.join(work, "showcase_" + name)
        run([exe, str(stop), prefix], stdout=subprocess.DEVNULL)
        a, b = Image.open(prefix + "_a.ppm"), Image.open(prefix + "_b.ppm")
        path = os.path.join(OUT, "showcase_%s.png" % name)
        side_by_side(a, b, left, gap).save(path, optimize=True)
        print("%-28s %4d x %3d  %6d bytes" % (os.path.relpath(path, ROOT), *Image.open(path).size,
                                              os.path.getsize(path)))

    # The animation: one run of the showcase, saving the frames on the way.
    prefix = os.path.join(work, "gif")
    env = dict(os.environ, FRAMES="%d:%d:%d" % (GIF_FIRST, GIF_LAST, GIF_STEP))
    run([exe, str(GIF_LAST), prefix], env=env, stdout=subprocess.DEVNULL)
    frames = []
    for n in range(GIF_FIRST, GIF_LAST + 1, GIF_STEP):
        a = Image.open("%s_%04d_a.ppm" % (prefix, n))
        b = Image.open("%s_%04d_b.ppm" % (prefix, n))
        frames.append(side_by_side(a, b, left, gap))
    # One shared palette for every frame, picked from a few frames spread
    # through the clip (the screens use only a few dozen colours). With the
    # same palette throughout, frames differ only where something moved, and
    # the GIF stores just those parts.
    sheet = Image.new("RGB", (frames[0].width, frames[0].height * 4))
    for i, k in enumerate((0, len(frames) // 3, 2 * len(frames) // 3, len(frames) - 1)):
        sheet.paste(frames[k], (0, i * frames[0].height))
    palette = sheet.quantize(colors=GIF_COLOURS, method=Image.Quantize.MEDIANCUT)
    gif = [f.quantize(palette=palette, dither=Image.Dither.NONE) for f in frames]
    path = os.path.join(OUT, "showcase.gif")
    gif[0].save(path, save_all=True, append_images=gif[1:], loop=0,
                duration=1000 * GIF_STEP // 30, optimize=True, disposal=1)
    size = os.path.getsize(path)
    print("%-28s %4d x %3d  %6d bytes, %d frames at %d fps" % (
        os.path.relpath(path, ROOT), frames[0].width, frames[0].height, size, len(gif), 30 // GIF_STEP))
    if size > GIF_MAX_BYTES:
        print("The GIF is over %d KB: raise GIF_STEP or lower GIF_COLOURS." % (GIF_MAX_BYTES // 1024))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
