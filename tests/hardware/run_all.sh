#!/bin/sh
# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
# ---------------------------------------------------------------------------
#  run_all.sh - flash every hardware test in turn, and ask what you saw.
#
#  Run:   sh tests/hardware/run_all.sh [--pack] [--from m4] [build-dir]
#
#    --pack       first load the asset pack qg4p_test_m7 needs, made beforehand by
#                 python3 tools/mkpack.py tests/hardware/pack --out build/assets
#    --from NAME  start part-way through: m0 ... m8, new_commands
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
#
#  At the end, a table of passes, fails and skips; the exit status is
#  non-zero if anything failed or was skipped.
# ---------------------------------------------------------------------------
TESTS="m0 m1 m2 m3 m4 m5 m6 m7 m8 new_commands"
PACK=0; FROM=""; BUILD=""
while [ $# -gt 0 ]; do
    case "$1" in
        --pack) PACK=1 ;;
        --from) [ $# -gt 1 ] || { echo "--from needs a test name"; exit 2; }; shift; FROM="$1" ;;
        -h|--help) sed -n '7,28p' "$0"; exit 0 ;;
        -*) echo "Unknown option: $1 (try --help)"; exit 2 ;;
        *) BUILD="$1" ;;
    esac
    shift
done
[ -n "$BUILD" ] || BUILD="$(dirname "$0")/../../build"
HW="$BUILD/tests/hardware"
PACKFILE="$BUILD/assets.bin"

# --from: keep the list from that test on.
if [ -n "$FROM" ]; then
    case " $TESTS " in *" $FROM "*) ;; *) echo "--from: no test called '$FROM' (one of: $TESTS)"; exit 2 ;; esac
    TESTS="$FROM${TESTS#*"$FROM"}"
fi

# --- What's needed, all checked before anything is flashed -------------------
missing=""
command -v picotool > /dev/null 2>&1 ||
    missing="$missing\n  picotool, on the PATH (the Pico VS Code extension keeps one in ~/.pico-sdk/picotool/)"
for t in $TESTS; do [ -f "$HW/qg4p_test_$t.uf2" ] || missing="$missing\n  $HW/qg4p_test_$t.uf2  (build first)"; done
[ $PACK = 0 ] || [ -f "$PACKFILE" ] ||
    missing="$missing\n  $PACKFILE  (python3 tools/mkpack.py tests/hardware/pack --out $BUILD/assets)"
[ -z "$missing" ] || { printf 'Missing, so nothing was flashed:%b\n' "$missing"; exit 1; }

# --- What to look for, from each test's own comments -------------------------
about() {
    case $1 in
    m0) echo "Screen A: the 16 colours in turn, the first BLACK, RED really red;"
        echo "a white border on all four edges, RED top-left, GREEN top-right, cyan bar at the top." ;;
    m1) echo "Both screens show the SAME colour at the same time, then M0's corner pattern;"
        echo "each backlight fades while the other stays steady; clears end clean, no stripes." ;;
    m2) echo "Pages on both screens: lines (widths 1-8), boxes, circles, an animated dial,"
        echo "palette swatches, and shapes cut off neatly at every edge." ;;
    m3) echo "A dice-roller layout made of percentages, upright then sideways, filling each screen;"
        echo "then a spinning lock-style dial on both." ;;
    m4) echo "The three fonts, scaled; text on a banner; two counters reaching 200 cleanly;"
        echo "a roll log with margins, a status corner and a centred result." ;;
    m5) echo "Colours and fonts changing mid-line on one baseline; wrapped, centred, right-aligned"
        echo "text; a table lined up with tabs; a roll log scrolling up at the bottom." ;;
    m6) echo "Images 1:1 (the d20 see-through over a checkerboard), scaled x2/x4/x8,"
        echo "fitted into boxes, and hanging off the screen edges." ;;
    m7) echo "From the asset pack: the file list (A) and a welcome text (B); every pack image;"
        echo "then missing names in red, dice/d20.bmp in green. Without the pack: 'No asset pack'." ;;
    m8) echo "Bouncing dice: A (DIRECT) flickers, B (framebuffer) doesn't; flood fills;"
        echo "rainbow rings moved by the palette alone; a log scrolling on both." ;;
    new_commands) echo "Shapes clipped by views; dashed and dotted lines; on B a sprite slid with XOR,"
        echo "leaving the background intact; a dotted line erased point by point." ;;
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
    if [ $quit = 1 ]; then table="$table\n  qg4p_test_$t\tskipped"; skip=$((skip + 1)); continue; fi
    echo
    echo "=== qg4p_test_$t ==="
    about $t | sed 's/^/  /'
    [ $t = m7 ] && [ $PACK = 0 ] && echo "  (Reminder: M7 needs its pack loaded first: --pack, or see test_m7.c.)"
    answer=r
    while [ $answer = r ]; do
        picotool load -f -x "$HW/qg4p_test_$t.uf2" || echo "  picotool couldn't load it. Stuck? Hold BOOTSEL, replug, [r]eload."
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
        y) table="$table\n  qg4p_test_$t\tPASS"; pass=$((pass + 1)) ;;
        n) table="$table\n  qg4p_test_$t\tFAIL"; fail=$((fail + 1)) ;;
        q) table="$table\n  qg4p_test_$t\tskipped"; skip=$((skip + 1)); quit=1 ;;
    esac
done

# --- Summary -----------------------------------------------------------------
echo
printf 'Summary%b\n' "$table" | expand -t 24
echo "$pass passed, $fail failed, $skip skipped"
[ $fail -eq 0 ] && [ $skip -eq 0 ]
