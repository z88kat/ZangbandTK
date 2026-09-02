#!/usr/bin/env python3.13
"""Replace one realm's books in class.txt with what the converter emits.

Used for the four realms DEC-50 replaces, where the books already exist and are
being swapped rather than appended. Appending is safe for a new realm because it
leaves every existing spell index meaning what it meant; replacing is not, and
is what DEC-50 licenses.

Usage: apply_realm.py <realm> [Class ...]
With no classes, every class in class.txt that already holds a book of that
realm.  The `magic:` book count is recomputed from what the block ends up with.
"""

import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import realms as R

ROOT = pathlib.Path(__file__).resolve().parent.parent.parent


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 2

    realm = sys.argv[1]
    only = sys.argv[2:]

    src = (ROOT / "archive" / "zangband" / "src" / "tables.c").read_text(
        errors="replace")
    spellmap = R.read_realmmap()
    if realm not in spellmap:
        print("no [%s] section in realmmap.toml" % realm)
        return 1

    path = ROOT / "lib" / "gamedata" / "class.txt"
    text = path.read_text(encoding="utf-8")
    parts = re.split(r"(?m)^(?=name:)", text)
    head, blocks = parts[0], parts[1:]

    out = []
    for blk in blocks:
        cls = blk.split("\n")[0][len("name:"):].strip()
        have = R.extract_realm_block(blk, realm)
        if not have or (only and cls not in only):
            out.append(blk)
            continue

        want = R.emit_books(src, cls, realm, spellmap)
        if not want:
            print("  %-14s holds %s but the table gives it none -- skipped"
                  % (cls, realm))
            out.append(blk)
            continue

        old = "\n".join(have)
        assert blk.count(old) == 1, cls
        blk = blk.replace(old, "\n".join(want))

        # A class may start the game holding one of these books by name, and a
        # title that no longer exists is "unrecognized sval" at load -- the game
        # refuses to start. Found the hard way on the Rogue, whose
        # `equip:magic book:[First Spells]` outlived the book.
        titles = ["[%s]" % t for t in spellmap[realm]["books"]]
        noun = R.book_noun(realm)
        for m in list(re.finditer(
                r"(?m)^equip:%s:(\[[^]]+\])(:.*)$" % re.escape(noun), blk)):
            if m.group(1) in titles:
                continue
            fixed = "equip:%s:%s%s" % (noun, titles[0], m.group(2))
            print("  %-14s starting book %s -> %s"
                  % (cls, m.group(1), titles[0]))
            blk = blk[:m.start()] + fixed + blk[m.end():]

        books = len(re.findall(r"(?m)^book:", blk))
        mm = re.search(r"(?m)^magic:(\d+):(\d+):(\d+)$", blk)
        assert mm, cls
        blk = (blk[:mm.start()]
               + "magic:%s:%s:%d" % (mm.group(1), mm.group(2), books)
               + blk[mm.end():])

        print("  %-14s %s: %d lines -> %d, now %d books, %d spells"
              % (cls, realm, len(have), len(want), books,
                 len(re.findall(r"(?m)^spell:", blk))))
        out.append(blk)

    path.write_text(head + "".join(out), encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
