#!/usr/bin/env python3
# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
"""
check_serial.py - the hardware tests all print in one format, and
run_all.sh says what they say.

    python3 tests/host/check_serial.py           check, list any problems
    python3 tests/host/check_serial.py --show    also print every step line

The format is set by tests/hardware/test_log.h. For each test program
(tests/hardware/test_m0.c ... test_m8.c, test_new_commands.c) this checks:

  - it defines TEST_TAG and prints only through the test_log.h macros (a
    bare printf("\\n") for a blank line is the one exception);
  - every TEST_STEP has plain numbers and quoted strings, so its line can be
    worked out here, and that line is under 100 characters;
  - its steps run 1/N, 2/N ... N/N, each once;
  - it ends each pass with TEST_PASS_DONE() and says TEST_REPEAT();
  - its opening line (test_setup's "about", or M0/M1's first TEST_LOG) and
    every other TEST_LOG/TEST_DETAIL line fit in 100 characters, counting a
    generous width for each number or string filled in at run time;
  - run_all.sh lists exactly the same steps, in the same words.
"""
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
HW = os.path.join(ROOT, "tests", "hardware")
LIMIT = 100
TESTS = ["m0", "m1", "m2", "m3", "m4", "m5", "m6", "m7", "m8", "new_commands"]

problems = []


def calls(src, name):
    """Every call name(...) in src, as a list of its top-level arguments."""
    out = []
    for m in re.finditer(r"\b%s\s*\(" % name, src):
        i, depth, arg, args, in_str = m.end(), 1, "", [], False
        while depth:
            c = src[i]
            if in_str:
                arg += c
                if c == "\\":
                    arg += src[i + 1]; i += 1
                elif c == '"':
                    in_str = False
            elif c == '"':
                in_str = True; arg += c
            elif c in "([":
                depth += 1; arg += c
            elif c in ")]":
                depth -= 1
                if depth: arg += c
            elif c == "," and depth == 1:
                args.append(arg.strip()); arg = ""
            else:
                arg += c
            i += 1
        if arg.strip(): args.append(arg.strip())
        out.append(args)
    return out


def literal(arg):
    """The text of one or more adjacent C string literals, or None."""
    parts = re.findall(r'"((?:[^"\\]|\\.)*)"', arg)
    rest = re.sub(r'"((?:[^"\\]|\\.)*)"', "", arg).strip()
    if not parts or rest:
        return None
    s = "".join(parts)
    return s.replace('\\"', '"').replace("\\\\", "\\").replace("\\n", "\n")


def estimate(fmt):
    """Length of a printf format once filled in, allowing generously for
    each value: numbers 5 to 10 characters, strings 12, or the given width."""
    def width(m):
        w = int(m.group(2)) if m.group(2) else 0
        conv = m.group(4)
        if conv == "%": return "%"
        guess = {"d": 5, "i": 5, "u": 5, "x": 8, "c": 1, "s": 12}.get(conv, 10)
        if m.group(3) in ("l", "ll"): guess = max(guess, 10)
        return "#" * max(w, guess)
    return len(re.sub(r"%([-+ #0]*)(\d+)?(?:\.\d+)?(l|ll|h|hh|z)?([diuxXcsfp%])", width, fmt))


def check_test(t):
    path = os.path.join(HW, "test_%s.c" % t)
    src = open(path, encoding="utf-8").read()
    code = re.sub(r"/\*.*?\*/", "", src, flags=re.S)          # comments out
    rel = os.path.relpath(path, ROOT)
    m = re.search(r'#define\s+TEST_TAG\s+"([^"]+)"', code)
    if not m:
        problems.append("%s: no #define TEST_TAG" % rel)
        return []
    tag = "[%s] " % m.group(1)

    for p in re.finditer(r"\bprintf\s*\(([^;]*)\);", code):
        if p.group(1).strip() != r'"\n"':
            problems.append("%s: prints with printf(%s), not test_log.h" % (rel, p.group(1).strip()[:30]))
    for need in ("TEST_PASS_DONE", "TEST_REPEAT"):
        if not re.search(r"\b%s\s*\(\s*\)" % need, code):
            problems.append("%s: never calls %s()" % (rel, need))

    steps, lines = [], []
    for args in calls(code, "TEST_STEP"):
        if len(args) != 5 or not args[0].isdigit() or not args[1].isdigit():
            problems.append("%s: TEST_STEP(%s) needs plain numbers and three strings" % (rel, ", ".join(args)[:40]))
            continue
        name, what, look = (literal(a) for a in args[2:])
        if None in (name, what, look):
            problems.append("%s: TEST_STEP %s/%s: name, what and look must be quoted strings" % (rel, args[0], args[1]))
            continue
        text = "%s/%s %s: %s -- look for %s" % (args[0], args[1], name, what, look)
        steps.append((int(args[0]), int(args[1]), text))
        lines.append(tag + text)
    totals = {s[1] for s in steps}
    if len(totals) != 1 or sorted(s[0] for s in steps) != list(range(1, len(steps) + 1)) \
            or len(steps) != next(iter(totals), -1):
        problems.append("%s: steps should run 1/N ... N/N once each, got %s"
                        % (rel, ", ".join("%d/%d" % s[:2] for s in steps)))

    for args in calls(code, "test_setup") + [a[:2] for a in calls(code, "test_setup_ex")]:
        about = literal(args[1]) if len(args) > 1 else None
        if about is None:
            problems.append("%s: test_setup's \"about\" must be a quoted string" % rel)
        else:
            lines.append(tag + about)
    for kind, indent in (("TEST_LOG", ""), ("TEST_DETAIL", "    ")):
        for args in calls(code, kind):
            fmt = literal(args[0]) if args else None
            if fmt is None:
                problems.append("%s: %s's first argument must be a quoted format" % (rel, kind))
                continue
            n = len(tag) + len(indent) + estimate(fmt)
            if n >= LIMIT:
                problems.append("%s: %s(\"%s\") could reach %d characters" % (rel, kind, fmt, n))

    for line in lines:
        if len(line) >= LIMIT:
            problems.append("%s: %d characters: %s" % (rel, len(line), line))
    if "--show" in sys.argv:
        for line in lines:
            print("%3d  %s" % (len(line), line))
    return [s[2] for s in sorted(steps)]


def run_all_steps():
    """What run_all.sh's about() says for each test: {name: [lines]}."""
    src = open(os.path.join(HW, "run_all.sh"), encoding="utf-8").read()
    block = src[src.index("about() {"):]
    block = block[:block.index("\n}\n")]
    out, cur = {}, None
    for line in block.splitlines():
        m = re.match(r"\s*(\w+)\)\s*(.*)$", line)
        if m and m.group(1) in TESTS:
            cur, line = m.group(1), m.group(2)
            out[cur] = []
        for e in re.findall(r'echo "((?:[^"\\]|\\.)*)"', line):
            if cur: out[cur].append(e.replace('\\"', '"'))
    return out


said = run_all_steps()
for t in TESTS:
    steps = check_test(t)
    if said.get(t) != steps:
        problems.append("run_all.sh: the steps for %s don't match test_%s.c's TEST_STEP lines" % (t, t))
        for s in steps:
            if s not in said.get(t, []):
                problems.append("  expected: %s" % s)

print("%d hardware tests checked for the serial format" % len(TESTS))
for p in problems:
    print("PROBLEM  " + p)
sys.exit(1 if problems else 0)
