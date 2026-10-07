#!/bin/sh
# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
# ---------------------------------------------------------------------------
#  run_all.sh - flash every hardware test in turn, and ask what you saw.
#
#  Run:   sh tests/hardware/run_all.sh [--group G] [--pack] [--from m4] [build-dir]
#
#    --group G    which tests: graphics (m0 ... m8, new_commands, the
#                 qg4p_test_ programs), sound (s0, s1, the qs4p_test_
#                 programs; needs the amplifier and speaker), or all
#                 (the default: graphics, then sound)
#    --pack       first load the asset pack qg4p_test_m7 needs, made beforehand by
#                 python3 tools/mkpack.py tests/hardware/pack --out build/assets
#    --from NAME  start part-way through the chosen group, e.g. m4 or s1
#    build-dir    where the build put the programs (default: build)
#
#  Needs picotool on the PATH, a finished build, and the Pico plugged in.
#  Each test loads with "picotool load -f -x": -f reboots the running program
#  into BOOTSEL mode over USB, so there's no button to press; -x starts the
#  new one. (A Pico running something without USB serial needs BOOTSEL held
#  while plugging in, once.)
#
#  Serial output: the Pico's USB serial port disconnects and reconnects on
#  every load. A terminal that reconnects by itself is easiest, e.g.
#  "tio /dev/ttyACM0" in another window; minicom may need restarting after
#  each load. The tests wait up to 2 s for a terminal before starting.
#  Each test prints one line per step, tagged with its name, e.g.
#    [M2] 3/6 Circles: outlines, fills, thick rings -- look for round, ...
#  the same lines this script shows before loading it, and at the end of
#  each pass: "[M2] Pass complete. Did every step look right?" Answer here.
#
#  At the end, a table of passes, fails and skips; the exit status is
#  non-zero if anything failed or was skipped.
# ---------------------------------------------------------------------------
GRAPHICS="m0 m1 m2 m3 m4 m5 m6 m7 m8 new_commands"
SOUND="s0"
PACK=0; FROM=""; BUILD=""; GROUP=all
while [ $# -gt 0 ]; do
    case "$1" in
        --group) [ $# -gt 1 ] || { echo "--group needs graphics, sound or all"; exit 2; }; shift; GROUP="$1" ;;
        --pack) PACK=1 ;;
        --from) [ $# -gt 1 ] || { echo "--from needs a test name"; exit 2; }; shift; FROM="$1" ;;
        -h|--help) sed -n '7,36p' "$0"; exit 0 ;;
        -*) echo "Unknown option: $1 (try --help)"; exit 2 ;;
        *) BUILD="$1" ;;
    esac
    shift
done
case "$GROUP" in
    graphics) TESTS="$GRAPHICS" ;;
    sound)    TESTS="$SOUND" ;;
    all)      TESTS="$GRAPHICS $SOUND" ;;
    *) echo "--group: '$GROUP' isn't graphics, sound or all"; exit 2 ;;
esac
[ -n "$BUILD" ] || BUILD="$(dirname "$0")/../../build"
HW="$BUILD/tests/hardware"
PACKFILE="$BUILD/assets.bin"

# Each test's program: qs4p_test_ for sound (s0, s1 ...), qg4p_test_ for
# graphics.
prog() { case $1 in s[0-9]*) echo "qs4p_test_$1" ;; *) echo "qg4p_test_$1" ;; esac; }

# --from: keep the list from that test on.
if [ -n "$FROM" ]; then
    case " $TESTS " in *" $FROM "*) ;; *) echo "--from: no test called '$FROM' (one of: $TESTS)"; exit 2 ;; esac
    TESTS="$FROM${TESTS#*"$FROM"}"
fi

# --- What's needed, all checked before anything is flashed -------------------
missing=""
command -v picotool > /dev/null 2>&1 ||
    missing="$missing\n  picotool, on the PATH (the Pico VS Code extension keeps one in ~/.pico-sdk/picotool/)"
for t in $TESTS; do [ -f "$HW/$(prog $t).uf2" ] || missing="$missing\n  $HW/$(prog $t).uf2  (build first)"; done
[ $PACK = 0 ] || [ -f "$PACKFILE" ] ||
    missing="$missing\n  $PACKFILE  (python3 tools/mkpack.py tests/hardware/pack --out $BUILD/assets)"
[ -z "$missing" ] || { printf 'Missing, so nothing was flashed:%b\n' "$missing"; exit 1; }

# --- What to look for: the steps each test prints over serial -----------------
# The same words, line for line, as each test's TEST_STEP lines (see
# tests/hardware/test_log.h); tests/host/check_serial.py checks they match.
about() {
    case $1 in
    m0) echo "1/3 Colours: the 16 named colours, 1 s each -- look for BLACK first, RED really red"
        echo "2/3 Orientation: corner pattern, 4 rotations -- look for RED top-left, all 4 edges white"
        echo "3/3 Speed: 32 full-screen clears -- look for about 33 ms per clear at 37.5 MHz" ;;
    m1) echo "1/4 Colours: 16 colours on both screens -- look for the same colour on both at once"
        echo "2/4 Orientation: M0's pattern on both, 4 rotations -- look for RED top-left on both"
        echo "3/4 Brightness: each backlight fades in turn -- look for the other screen staying steady"
        echo "4/4 Hand-over: clears alternate between screens -- look for A solid GREEN, B solid BLUE" ;;
    m2) echo "1/6 Lines: starburst, widths 1-8 -- look for no gaps at the centre, even widths"
        echo "2/6 Boxes: outline/filled/both, widths 1,3,6 -- look for every box the same outer size"
        echo "3/6 Circles: outlines, fills, thick rings -- look for round, outlines hugging fills"
        echo "4/6 Arcs: a dial filling, 4 quarter arcs -- look for green-yellow-red, quarters meeting"
        echo "5/6 Palette: RGB swatches, grey ramp, 2 bars -- look for bar A GREY and bar B GOLD"
        echo "6/6 Clipping: shapes off every edge -- look for clean cuts, nothing wrapping round" ;;
    m3) echo "1/3 Layout: percent layout, upright, then sideways -- look for ticks touching each corner"
        echo "2/3 Gauge: M2's gauge, no pauses -- look for the time per slice (M2: ~12 ms on A)"
        echo "3/3 Dial: a marker spinning for 4 s -- look for a smooth clockwise spin on both" ;;
    m4) echo "1/3 Fonts: 3 fonts, scale 2, symbols -- look for ° ± ×, '?' for the euro, descenders"
        echo "2/3 Opaque: banner text, two counters -- look for both reaching 200 cleanly"
        echo "3/3 Cursor: a roll log, HP corner, a big result -- look for the list lined up, number centred" ;;
    m5) echo "1/4 Markup: colours, sizes, fonts mid-line -- look for one shared baseline, '{' printed"
        echo "2/4 Wrap: a story, aligned lines, a box -- look for whole words; the box fits its text"
        echo "3/4 Tabs: a table with tabs and {x:} -- look for straight columns"
        echo "4/4 Scroll: 40 log lines on each screen -- look for smooth scrolling, nothing lost" ;;
    m6) echo "1/4 1:1: banner, landscape, two d20s -- look for the right d20 on MAGENTA (no flag)"
        echo "2/4 Scaling: potions x1 to x8, stretched d20s -- look for crisp, blocky pixels"
        echo "3/4 Fit: the landscape in 5 boxes -- look for no distortion; left, centre, right"
        echo "4/4 Speed: timed draws, d20s off each corner -- look for clean cuts at all 4 corners" ;;
    m7) echo "1/3 Contents: file list on A, a text file on B -- look for 6 files listed, the welcome text"
        echo "2/3 Images: every image, from the pack -- look for banner, landscape, 2 d20s, a potion"
        echo "3/3 Errors: 3 names looked up on A -- look for 2 red 'not found', dice/d20.bmp OK" ;;
    m8) echo "1/5 Flicker: bouncing dice on both screens -- look for flicker on A (DIRECT), none on B"
        echo "2/5 Full scene: B redraws everything per frame -- look for no flashing, the true colours"
        echo "3/5 Paint: flood fills on B, refusals on A -- look for regions filling one by one"
        echo "4/5 Palette: rainbow rings on B -- look for rings flowing outward, none redrawn"
        echo "5/5 Scroll: a log on both screens -- look for both logs scrolling cleanly" ;;
    new_commands) echo "1/4 VIEW: shapes clipped by views on A -- look for nothing drawn outside a view"
                  echo "2/4 Styles: styled lines and boxes on A -- look for even dashes, thin and thick"
                  echo "3/4 GET/PUT: a sprite on B, slid with XOR -- look for the background intact behind it"
                  echo "4/4 PRESET: a dotted line erased on A -- look for every dot gone; POS/CSRLIN shown" ;;
    s0) echo "1/6 Silence: amp on, PWM at 50%, 2 s -- look for no whine (carrier); a little hiss is fine"
        echo "2/6 8-bit: 1 kHz sine, 586 kHz carrier, 2 s -- look for a clean, steady tone"
        echo "3/6 10-bit: 1 kHz sine, 146 kHz carrier, 2 s -- look for which was cleaner, step 2 or 3?"
        echo "4/6 Sweep: 100 Hz to 8 kHz over 4 s -- look for where it goes quiet, buzzy or rattly"
        echo "5/6 Beep: 800 Hz square, 250 ms, three times -- look for three crisp beeps, QB's BEEP"
        echo "6/6 Volume: 1 kHz at 25, 50, 75, 100%, 1 s each -- look for the loudest step with no rattle" ;;
    esac
}

# --- The pack, if asked for --------------------------------------------------
if [ $PACK = 1 ]; then
    echo "Loading the asset pack for qg4p_test_m7 at 0x10100000..."
    picotool load -f "$PACKFILE" -o 0x10100000 || { echo "picotool couldn't load the pack."; exit 1; }
fi

# --- Each test in turn -------------------------------------------------------
pass=0; fail=0; skip=0; quit=0; table=""
for t in $TESTS; do
    p=$(prog $t)
    if [ $quit = 1 ]; then table="$table\n  $p\tskipped"; skip=$((skip + 1)); continue; fi
    echo
    echo "=== $p ==="
    echo "  Steps (the serial lines say the same, as each one starts):"
    about $t | sed 's/^/    /'
    [ $t = m7 ] && [ $PACK = 0 ] && echo "  (Reminder: M7 needs its pack loaded first: --pack, or see test_m7.c.)"
    [ $t = s0 ] && echo "  (Sound: the amp and speaker wired as in test_s0.c. The amp's outputs are bridged: neither speaker wire goes to GND.)"
    answer=r
    while [ $answer = r ]; do
        picotool load -f -x "$HW/$p.uf2" || echo "  picotool couldn't load it. Stuck? Hold BOOTSEL, replug, [r]eload."
        answer=""
        while [ -z "$answer" ]; do
            printf 'Pass? [Y]es / [n]o / [r]eload / [q]uit: '
            read reply || reply=q                     # end of input counts as quit
            case "$reply" in
                ""|[yY]*) answer=y ;;  [nN]*) answer=n ;;  [rR]*) answer=r ;;  [qQ]*) answer=q ;;
                *) echo "  Please type y, n, r or q." ;;
            esac
        done
    done
    case $answer in
        y) table="$table\n  $p\tPASS"; pass=$((pass + 1)) ;;
        n) table="$table\n  $p\tFAIL"; fail=$((fail + 1)) ;;
        q) table="$table\n  $p\tskipped"; skip=$((skip + 1)); quit=1 ;;
    esac
done

# --- Summary -----------------------------------------------------------------
echo
printf 'Summary%b\n' "$table" | expand -t 28
echo "$pass passed, $fail failed, $skip skipped"
[ $fail -eq 0 ] && [ $skip -eq 0 ]
