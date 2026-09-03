#!/usr/bin/env python3.13
"""Put one realm's books into class.txt, as the converter emits them.

Two operations, because the two are not the same risk:

**Replace** (the default) swaps books a class already has. That is what the four
mapped realms needed under DEC-50 -- Arcane, Life, Nature and Death already
carried Angband's content -- and it moves every spell index after the block,
which is why DEC-50 had to license it.

**Append** (`--append`) adds a realm a class does not yet carry, at the end of
its book list. Every index that already existed keeps meaning what it meant, so
a saved character's known spells are untouched. Sorcery and Chaos arrived this
way, and DEC-57's entitlement completion does too.

Usage:
  apply_realm.py <realm> [Class ...]            replace
  apply_realm.py --append <realm> [Class ...]   append

With no classes named, replace acts on every class that already holds a book of
that realm, and append refuses (there is no sensible "every class" for it). The
`magic:` book count is recomputed from what the block ends up with either way.
"""

import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import realms as R

ROOT = pathlib.Path(__file__).resolve().parent.parent.parent


def append_realm(blk: str, cls: str, realm: str, want: list[str]) -> str:
    """The class block with `realm`'s books added after the ones it has."""
    # Everything from the last book line to the end of the generated region.
    starts = [m.start() for m in
              re.finditer(R.BOOK_LINE, blk)]
    assert starts, cls

    tail = blk[starts[-1]:]
    lines = tail.rstrip("\n").split("\n")

    # A trailing banner comment belongs to the file, not to the class.
    trailing = []
    while lines and (not lines[-1].strip()
                     or lines[-1].lstrip().startswith("#")):
        trailing.insert(0, lines.pop())

    body = blk[:starts[-1]] + "\n".join(lines) + "\n\n" + "\n".join(want)
    if trailing:
        body += "\n" + "\n".join(trailing)
    return body.rstrip("\n") + "\n"


def main() -> int:
    args = sys.argv[1:]
    appending = False
    if args and args[0] == "--append":
        appending = True
        args = args[1:]

    if not args:
        print(__doc__)
        return 2

    realm = args[0]
    only = args[1:]

    if appending and not only:
        print("--append needs the classes named explicitly")
        return 2

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

        if only and cls not in only:
            out.append(blk)
            continue

        if appending:
            if have:
                print("  %-14s already carries %s -- that is a replace"
                      % (cls, realm))
                out.append(blk)
                continue
        elif not have:
            out.append(blk)
            continue

        want = R.emit_books(src, cls, realm, spellmap)
        if not want:
            print("  %-14s holds %s but the table gives it none -- skipped"
                  % (cls, realm))
            out.append(blk)
            continue

        # A directive that has nothing to do with books can sit inside the
        # book region -- the Druid's `equip:nature book:[Lesser Charms]` lives
        # between its first book's properties and that book's first spell -- and
        # replacing the region wholesale deletes it. Carry anything that is not
        # part of the book grammar across, after the first new book.
        grammar = ("book:", "book-graphics:", "book-properties:", "spell:",
                   "effect:", "effect-yx:", "effect-msg:", "dice:", "expr:",
                   "desc:")
        strays = [l for l in have if l.strip() and not l.startswith(grammar)]

        if appending:
            blk = append_realm(blk, cls, realm, want)

            books = len(re.findall(r"(?m)^book:", blk))
            mm = re.search(r"(?m)^magic:(\d+):(\d+):(\d+)$", blk)
            assert mm, cls
            blk = (blk[:mm.start()]
                   + "magic:%s:%s:%d" % (mm.group(1), mm.group(2), books)
                   + blk[mm.end():])

            print("  %-14s +%-8s %d lines, now %d books, %d spells"
                  % (cls, realm, len(want), books,
                     len(re.findall(r"(?m)^spell:", blk))))
            out.append(blk)
            continue

        old = "\n".join(have)
        assert blk.count(old) == 1, cls

        # A stray stays where it was, immediately after the first book's
        # properties. It cannot be moved out: `equip:nature book:[...]` names an
        # object kind that the `book:` directive itself creates, so an equip
        # line ahead of its book is "unrecognized sval" and the game refuses to
        # start. Which is why the Druid's was in there in the first place.
        new_lines = list(want)
        for stray in strays:
            print("  %-14s kept in the book region: %s" % (cls, stray))
            new_lines.insert(3, stray)
        blk = blk.replace(old, "\n".join(new_lines))

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
