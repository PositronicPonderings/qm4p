#!/usr/bin/env python3
# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
"""
check_disclosure.py - every source file declares its AI involvement.

    python3 tests/host/check_disclosure.py

Every .c, .h, .py, .sh and CMakeLists.txt in the repository (apart from
third-party files listed below) must have an SPDX-AI-Disclosure tag near its
top, with one of the four levels of the ai-disclosure convention (see
AI_DISCLOSURE.md), and a SPDX-AI-Model tag unless the level is "none";
the repository-level AI_DISCLOSURE.md must exist. Each library folder
(qg4p, qa4p, qs4p) must turn up in the search, so a library that's been
moved or missed fails here instead of passing unchecked.
"""
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
LEVELS = {"none", "ai-assisted", "ai-generated", "autonomous"}
THIRD_PARTY = {"pico_sdk_import.cmake"}           # Raspberry Pi's, unmodified
TAG = re.compile(r"SPDX-AI-Disclosure:\s*([a-z-]+)")
LIBRARIES = ("qg4p", "qa4p", "qs4p")

problems, checked, per_folder = [], 0, {}
if not os.path.exists(os.path.join(ROOT, "AI_DISCLOSURE.md")):
    problems.append("AI_DISCLOSURE.md is missing")
for root, dirs, files in os.walk(ROOT):
    dirs[:] = [d for d in dirs if d not in ("build", ".git", "__pycache__")]
    for f in files:
        if f in THIRD_PARTY or not (f.endswith((".c", ".h", ".py", ".sh")) or f == "CMakeLists.txt"):
            continue
        path = os.path.join(root, f)
        head = "".join(open(path, encoding="utf-8").readlines()[:8])
        m = TAG.search(head)
        checked += 1
        rel = os.path.relpath(path, ROOT)
        top = rel.split(os.sep)[0]
        per_folder[top] = per_folder.get(top, 0) + 1
        if not m:
            problems.append("%s: no SPDX-AI-Disclosure tag in its first lines" % rel)
        elif m.group(1) not in LEVELS:
            problems.append("%s: unknown disclosure level '%s'" % (rel, m.group(1)))
        elif m.group(1) != "none" and "SPDX-AI-Model:" not in head:
            problems.append("%s: no SPDX-AI-Model tag" % rel)

for lib in LIBRARIES:
    if per_folder.get(lib, 0) == 0:
        problems.append("%s/: no source files found" % lib)

print("%d source files checked for an AI disclosure tag (%s)" %
      (checked, ", ".join("%s/ %d" % (lib, per_folder.get(lib, 0)) for lib in LIBRARIES)))
for p in problems:
    print("MISSING  " + p)
sys.exit(1 if problems else 0)
