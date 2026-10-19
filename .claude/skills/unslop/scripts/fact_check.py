#!/usr/bin/env python3
"""Show what a prose edit added or removed besides wording.

Usage:
    python3 -I fact_check.py [--base REV] FILE ...

Compares each file with its version at REV (default HEAD) and lists the
numbers, inline literals, roles, links, quoted text, headings, labels and
directive lines that differ.  A rewrite that only changes wording shows
nothing.  Anything listed is either a fact that moved (fine, if it is still
there), a fact that was lost or invented (put it back), or a heading whose
anchor other pages may link to (check with grep).

Exit status is 1 when anything is listed, so it can gate a script.
"""

import argparse
import collections
import re
import subprocess
import sys

NUMBER_WORDS = ("one two three four five six seven eight nine ten eleven twelve "
                "thirteen fourteen fifteen sixteen seventeen eighteen nineteen twenty "
                "thirty forty fifty sixty seventy eighty ninety hundred thousand "
                "dozen half quarter twice once").split()

STRICT = [
    ("number", re.compile(r"(?<![\w.])\d+(?:[.,:]\d+)*(?:\s?%|×)?")),
    ("literal", re.compile(r"``.+?``")),
    ("role", re.compile(r":[\w:]+:`[^`]+`")),
    ("link", re.compile(r"`[^`]+`__?|https?://[^\s<>`]*[^\s<>`.,;:!?)]")),
    ("quoted", re.compile(r"\"(?:[^\"\n]|\n(?![ \t]*\n)[ \t]*){2,80}\"")),  # may wrap, not span paragraphs
    ("label", re.compile(r"^\.\. _[^:]+:.*$", re.M)),
    ("directive", re.compile(r"^\s*\.\. [\w:-]+::.*$|^\s+:[\w-]+:(?:\s.*)?$", re.M)),
]
INFO = [
    ("number word", re.compile(r"\b(?:%s)(?:-(?:%s))?\b" % ("|".join(NUMBER_WORDS), "|".join(NUMBER_WORDS)), re.I)),
    ("emphasis", re.compile(r"(?<![*\w'\"])\*[^*\s'\"][^*\n]{0,60}?(?<!\s)\*(?![*\w])")),
]
ADORN = re.compile(r"^([=\-~^\"'`#*+])\1{2,}\s*$")


def headings(text):
    lines = text.splitlines()
    out = []
    for i in range(1, len(lines)):
        if ADORN.match(lines[i]) and lines[i - 1].strip() and not ADORN.match(lines[i - 1]):
            out.append(lines[i - 1].strip())
    return out


def tokens(text, kinds):
    bag = collections.Counter()
    plain = re.sub(r"``.+?``", "``", text)  # so ``*`` can't pair with *emphasis*
    for name, rx in kinds:
        for m in rx.finditer(plain if name == "emphasis" else text):
            bag[(name, " ".join(m.group(0).split()))] += 1
    return bag


def show(path, before, after):
    found = False
    for title, kinds in (("CHECK", STRICT), ("info", INFO)):
        b, a = tokens(before, kinds), tokens(after, kinds)
        if title == "CHECK":
            b.update(("heading", h) for h in headings(before))
            a.update(("heading", h) for h in headings(after))
        lost, gained = b - a, a - b
        for label, bag in (("lost", lost), ("gained", gained)):
            for (kind, tok), n in sorted(bag.items()):
                print("%s  %-5s %-6s %-11s %s%s" % (path, title, label, kind, tok,
                                                   "" if n == 1 else "  (x%d)" % n))
                found = found or title == "CHECK"
    return found


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--base", default="HEAD")
    ap.add_argument("files", nargs="+")
    args = ap.parse_args()
    bad = False
    for path in args.files:
        try:
            before = subprocess.run(["git", "show", "%s:%s" % (args.base, path)],
                                    capture_output=True, text=True, check=True).stdout
        except subprocess.CalledProcessError:
            print("%s: not in %s, skipped" % (path, args.base))
            continue
        with open(path, encoding="utf-8") as f:
            after = f.read()
        bad = show(path, before, after) or bad
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
