#!/usr/bin/env python3
# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
"""
doc_links.py - check every link and picture in the documentation.

    python3 tests/host/doc_links.py

For every Markdown file in docs/ and the READMEs, every relative link
([text](path#anchor)) and picture (<img src="...">) must point at a file
or folder that exists, and every #anchor at a real heading in that file,
worked out the way GitHub turns headings into anchors. Web links are skipped.

Source files point at other files too, in their comments ("see
tests/hardware/test_s0.c for the wiring"). Every path like that in a .c, .h,
.py, .sh or CMakeLists.txt, starting with one of the project's folders,
must name a file or folder that exists. A path ending in "_" or with NAME
in it is a pattern, not a file, and is skipped.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
LINK = re.compile(r"(?<!!)\[[^\]]*\]\(([^)\s]+)\)|!\[[^\]]*\]\(([^)\s]+)\)|<img[^>]*\ssrc=\"([^\"]+)\"")
FENCE = re.compile(r"^```.*?^```", re.S | re.M)
FOLDERS = ("docs", "examples", "tests", "tools", "qg4p", "qa4p", "qs4p")
SOURCE_PATH = re.compile(r"(?<![\w./-])((?:%s)/[\w./-]*\w)" % "|".join(FOLDERS))
SKIP_DIRS = ("build", ".git", "__pycache__", "out")


def slug(heading):
    """GitHub's anchor for a heading: lower case, punctuation dropped, spaces to hyphens."""
    h = re.sub(r"`", "", heading.strip().lower())
    h = re.sub(r"[^\w\- ]", "", h)
    return h.replace(" ", "-")


def anchors(path, cache={}):
    if path not in cache:
        seen, found = {}, set()
        text = FENCE.sub("", open(path, encoding="utf-8").read())
        for m in re.finditer(r"^#{1,6}\s+(.*)$", text, re.M):
            s = slug(m.group(1))
            n = seen.get(s, 0)
            found.add(s if n == 0 else "%s-%d" % (s, n))
            seen[s] = n + 1
        cache[path] = found
    return cache[path]


def main():
    files = []
    for top in FOLDERS:
        for root, _d, fs in os.walk(os.path.join(ROOT, top)):
            if "build" in root.split(os.sep):
                continue
            files += [os.path.join(root, f) for f in fs if f.endswith(".md")]
    files += [os.path.join(ROOT, f) for f in ("README.md", "CHANGELOG.md")]

    problems, checked = [], 0
    for path in sorted(files):
        text = FENCE.sub("", open(path, encoding="utf-8").read())
        for m in LINK.finditer(text):
            target = m.group(1) or m.group(2) or m.group(3)
            if re.match(r"^[a-z]+:", target):
                continue                              # web and mail links
            checked += 1
            file_part, _, anchor = target.partition("#")
            dest = path if not file_part else os.path.normpath(os.path.join(os.path.dirname(path), file_part))
            where = os.path.relpath(path, ROOT)
            if not os.path.exists(dest):
                problems.append("%s: %s -> no such file" % (where, target))
            elif anchor and dest.endswith(".md") and anchor not in anchors(dest):
                problems.append("%s: %s -> no heading '#%s' in %s" % (where, target, anchor, os.path.relpath(dest, ROOT)))

    sources = 0
    for root, dirs, fs in os.walk(ROOT):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        for f in fs:
            if not (f.endswith((".c", ".h", ".py", ".sh")) or f == "CMakeLists.txt"):
                continue
            path = os.path.join(root, f)
            sources += 1
            for m in SOURCE_PATH.finditer(open(path, encoding="utf-8").read()):
                target = m.group(1)
                if target.endswith("_") or re.search(r"(^|/)NAME\b", target):
                    continue                          # a pattern, not a file
                checked += 1
                if not os.path.exists(os.path.join(ROOT, target)):
                    problems.append("%s: %s -> no such file" % (os.path.relpath(path, ROOT), target))

    print("%d links, pictures and paths checked in %d documents and %d source files" % (checked, len(files), sources))
    for p in problems:
        print("BROKEN  " + p)
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()
