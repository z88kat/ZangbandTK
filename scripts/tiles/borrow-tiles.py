#!/usr/bin/env python3
"""
Give the imported monsters a tile to stand in until they have their own.

ZangbandTK adds 391 monsters that Angband does not have, and not one of them
appears in any of the five tilesets -- so in graphics mode they render as ASCII
letters among the pictures.  Most of them are ordinary creatures that look like
something already drawn: a chaos hound is a hound, a serpent man is a snake.

For each imported monster this finds an Angband monster that shares its `base:`,
prefers one that also shares its `color:`, and among those picks the one nearest
in `depth:` -- so a deep wolf borrows from a wolf rather than from a puppy.  The
tile coordinates are copied verbatim from the tileset's own prf lines, never
computed, because the renderer wraps an out-of-range cell with `% pict_rows`
instead of complaining: a wrong coordinate draws the wrong picture in silence.

UNIQUES ARE DELIBERATELY LEFT ALONE.  A borrowed tile for a common monster is a
reasonable stand-in; for a unique it is a lie about the thing the player most
wants to recognise, and the lie lands badly here.  Matched by base and depth,
Oberon of Amber, Dworkin and Mandor all come out wearing the Mouth of Sauron's
face -- which is precisely the drift this project exists to undo.  A letter says
"no picture yet"; a wrong portrait says something false.

Re-runnable: it rewrites its own output file and adds its include line once.

    scripts/tiles/borrow-tiles.py [--dry-run] [--check]

--check re-reads what was written and verifies it against the two ways this
can go wrong silently: lookup_monster() falls back to a substring match, so a
misspelled name binds to whatever contains it; and the renderer takes a cell
index modulo the sheet size, so an out-of-range coordinate draws some other
picture rather than failing.  Both are checked by construction here -- names
come from the data files and coordinates are copied from existing lines -- and
the check is what proves it stayed that way.
"""

import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
TILES = os.path.join(ROOT, 'lib', 'tiles')
DATA = os.path.join(ROOT, 'lib', 'gamedata')
OUTPUT = 'graf-ztk.prf'

ANGBAND = ['monster.txt']
IMPORTED = ['monster.zangband.txt', 'monster.zangbandtk.txt']


def read_monsters(filename):
    """name -> {base, color, depth, unique}, in file order."""
    out = {}
    current = None

    with open(os.path.join(DATA, filename), encoding='utf-8', errors='replace') as f:
        for line in f:
            line = line.rstrip('\n')
            if line.startswith('name:'):
                current = line[5:].strip()
                out[current] = {'base': None, 'color': None, 'depth': 0,
                                'unique': False}
            elif current is None:
                continue
            elif line.startswith('base:'):
                out[current]['base'] = line[5:].strip()
            elif line.startswith('color:'):
                out[current]['color'] = line[6:].strip()
            elif line.startswith('depth:'):
                try:
                    out[current]['depth'] = int(line[6:].strip())
                except ValueError:
                    pass
            elif line.startswith('flags:') and 'UNIQUE' in line:
                out[current]['unique'] = True

    return out


def read_prf_chain(directory, filename, seen=None):
    """Every monster:name:attr:char in a prf and the files it includes."""
    if seen is None:
        seen = set()

    path = os.path.join(TILES, directory, filename)
    if path in seen or not os.path.exists(path):
        return {}
    seen.add(path)

    tiles = {}
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            line = line.strip()

            m = re.match(r'^monster:([^:]+):(0x[0-9A-Fa-f]+):(0x[0-9A-Fa-f]+)',
                         line)
            if m:
                tiles[m.group(1).lower()] = (m.group(2), m.group(3))
                continue

            if line.startswith('%:'):
                tiles.update(read_prf_chain(directory, line[2:].strip(), seen))

    return tiles


def read_tilesets():
    """(id, name, directory, pref) for each tileset in list.txt."""
    out = []
    entry = {}

    with open(os.path.join(TILES, 'list.txt'), encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if line.startswith('name:'):
                if entry.get('directory'):
                    out.append(entry)
                bits = line.split(':', 2)
                entry = {'id': bits[1], 'name': bits[2]}
            elif line.startswith('directory:'):
                entry['directory'] = line[10:].strip()
            elif line.startswith('pref:'):
                entry['pref'] = line[5:].strip()

    if entry.get('directory'):
        out.append(entry)

    return out


def choose_donor(want, candidates):
    """
    An Angband monster to borrow a tile from, or None.

    Same base is required -- a snake must not borrow from a dog.  Sharing the
    colour as well is much better, because the colour is most of what a small
    tile conveys.  Nearest depth breaks the tie, which is what stops a wolf
    unique from borrowing a scruffy little dog.
    """
    same = [c for c in candidates
            if c[1]['base'] == want['base'] and c[1]['color'] == want['color']]
    exact = bool(same)

    if not same:
        same = [c for c in candidates if c[1]['base'] == want['base']]

    if not same:
        return None, False

    best = min(same, key=lambda c: abs(c[1]['depth'] - want['depth']))

    return best, exact


def check(tilesets):
    """Verify what was written: real names, and coordinates that already exist."""
    names = set()
    for filename in ANGBAND + IMPORTED:
        names.update(n.lower() for n in read_monsters(filename))

    problems = 0

    for ts in tilesets:
        directory = ts['directory']
        known = set()

        for existing in os.listdir(os.path.join(TILES, directory)):
            if not existing.endswith('.prf') or existing == OUTPUT:
                continue
            for name, coord in read_prf_chain(directory, existing).items():
                known.add((coord[0].lower(), coord[1].lower()))

        path = os.path.join(TILES, directory, OUTPUT)
        if not os.path.exists(path):
            continue

        count = 0
        with open(path, encoding='utf-8') as f:
            for line in f:
                m = re.match(r'^monster:([^:]+):(0x[0-9A-Fa-f]+):(0x[0-9A-Fa-f]+)\s*$',
                             line)
                if not m:
                    continue
                count += 1

                if m.group(1).lower() not in names:
                    print("  %s: no such monster '%s'" % (directory, m.group(1)))
                    problems += 1

                if (m.group(2).lower(), m.group(3).lower()) not in known:
                    print("  %s: %s:%s is not a cell any other line uses"
                          % (directory, m.group(2), m.group(3)))
                    problems += 1

        print("  %-12s %d lines checked" % (directory, count))

    print("\n%d problems" % problems)

    return 1 if problems else 0


def main():
    dry_run = '--dry-run' in sys.argv

    if '--check' in sys.argv:
        return check(read_tilesets())


    angband = {}
    for name in ANGBAND:
        angband.update(read_monsters(name))

    imported = {}
    for name in IMPORTED:
        imported.update(read_monsters(name))

    common = {n: v for n, v in imported.items() if not v['unique']}
    uniques = [n for n, v in imported.items() if v['unique']]

    print("%d imported monsters: %d common, %d unique (left as text)"
          % (len(imported), len(common), len(uniques)))

    for ts in read_tilesets():
        tiles = read_prf_chain(ts['directory'], ts['pref'])
        candidates = [(n, v) for n, v in angband.items() if n.lower() in tiles]

        lines, exact_n, loose_n, missed = [], 0, 0, []

        for name, want in common.items():
            if name.lower() in tiles:
                continue

            donor, exact = choose_donor(want, candidates)
            if not donor:
                missed.append(name)
                continue

            attr, char = tiles[donor[0].lower()]
            lines.append((name, donor[0], attr, char, exact))
            if exact:
                exact_n += 1
            else:
                loose_n += 1

        print("\n%-12s %3d mapped (%d on base+colour, %d on base alone), "
              "%d with nothing to borrow"
              % (ts['directory'], len(lines), exact_n, loose_n, len(missed)))
        if missed:
            print("             no donor: %s" % ', '.join(sorted(missed)[:6])
                  + (' ...' if len(missed) > 6 else ''))

        if dry_run:
            continue

        out = os.path.join(TILES, ts['directory'], OUTPUT)
        with open(out, 'w', encoding='utf-8') as f:
            f.write("# %s -- stand-in tiles for ZangbandTK's own monsters.\n"
                    "#\n"
                    "# GENERATED by scripts/tiles/borrow-tiles.py.  Do not edit by hand:\n"
                    "# re-running the script overwrites this file.  Each line borrows the\n"
                    "# picture of an Angband monster sharing the same base, preferring one\n"
                    "# of the same colour and then the nearest depth.  The monster it was\n"
                    "# taken from is named in the comment so a wrong-looking tile can be\n"
                    "# traced without running anything.\n"
                    "#\n"
                    "# These are placeholders.  Replacing one with real art means deleting\n"
                    "# its line here and adding a proper mapping to %s.\n"
                    "#\n"
                    "# Uniques are deliberately absent -- they render as text until they\n"
                    "# have art of their own, because a borrowed portrait on a named\n"
                    "# character says something false.\n\n" % (OUTPUT, ts['pref']))

            for name, donor, attr, char, exact in sorted(lines):
                f.write("# as %s%s\n" % (donor, "" if exact else " (colour differs)"))
                f.write("monster:%s:%s:%s\n" % (name, attr, char))

        # And make sure the tileset actually loads it, once.
        pref = os.path.join(TILES, ts['directory'], ts['pref'])
        with open(pref, encoding='utf-8', errors='replace') as f:
            body = f.read()

        if OUTPUT not in body:
            with open(pref, 'a', encoding='utf-8') as f:
                f.write("\n# Load ZangbandTK's stand-in tiles (generated)\n"
                        "%%:%s\n" % OUTPUT)
            print("             added %%:%s to %s" % (OUTPUT, ts['pref']))

    return 0


if __name__ == '__main__':
    sys.exit(main())
