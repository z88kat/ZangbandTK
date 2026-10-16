#!/usr/bin/env python3
"""
Bring Adam Bolt's Zangband unique tiles into Adam Bolt's Angband sheet -- and
the objects Zangband drew that this game has and Angband had not, which so far
is one, the magical figurine, into his sheet (3.129.0) and the 8x8 one
(3.129.1).

Sixty-five of the named characters this project imports were drawn, by the
artist who drew the rest of the set, for the game they came from.  The art is
in `archive/zangband/lib/xtra/graf/16x16.bmp` and the names are in
`archive/zangband/lib/pref/graf-new.prf`.  Angband's copy of the same tileset
does not have them -- it is the same sheet, grown in a different direction --
so they have been ASCII letters in a tiled game for no better reason than that
nobody went and got them.

This is the third mechanism in scripts/tiles/README, and a better one than
either: not borrowed, not generated, *the actual tile*.

=== Why this is allowed, and why it is not portrait-borrowing ===

Adam Bolt's tiles may be "redistributed and used for any purpose, with or
without modification" -- quoted in lib/tiles/README, which also records that we
have already relied on it: make-terrain.py appends a row of generated
wilderness to this same sheet.  Zangband's own changelog credits the additions
to Adam Bolt himself ("Adam Bolt's tiles are now finished and every
monster/object/spell effect has a tile", z_update.txt), so the Zangband rows
are his work under his terms, not a third party's.

And the uniques policy is not breached, it is satisfied.  The rule is that a
picture may not assert an identity it does not have.  Borrowing the Mouth of
Sauron's portrait for Oberon asserts a falsehood; Adam Bolt's drawing of Robin
Hood, used for Robin Hood, asserts exactly what is true.  See DEC-18.

=== Which sets, and why ===

Checked, all four:

  adam-bolt   764 named assignments in the archive, 65 of our missing uniques
  old         `graf-xxx.prf` carries three named *monster* assignments in
              total; the 8x8 art exists but nothing says which cell is which
              monster, and the Tk-side configs that might are Tim Baker's,
              under a not-for-profit licence this project cannot ship.  It does
              name its object cells -- `# Magical Figurine` above `K:567` --
              so the 8x8 set takes objects and no monsters
  nomad       a modern set.  There is no 2001 counterpart to take from
  gervais     mapped only, and byte-identical to Angband's.  Keeping it that
              way is worth more than sixty-five tiles

=== How the two sheets relate ===

They share a layout.  Of 407 monsters assigned in both prfs, 390 (95.8%) sit at
identical coordinates -- this is one tileset that grew two ways, not two
tilesets.  But the cells the Zangband uniques occupy are *not* free in ours:
all 65 hold different Angband art today, because 4.2 reused them.  So this
copies pixels into new rows rather than mapping coordinates, and that is why it
is a modification and had to be licensed rather than merely noted.

=== Transparency ===

The archive is a 256-colour BMP plus a 1-bit `mask.bmp`, where **black means
opaque**.  That is calibrated, not assumed: across three control tiles that
exist in both sheets, mask-black agreed with our sheet's alpha==255 on 256 of
256 pixels, and mask-white agreed on none.

The 8x8 sheet has no mask.  Its tiles sit on a flat grey, (48, 48, 48), and
**that grey is what is transparent** -- calibrated the same way: of the 41
object cells both prfs name, 34 are the same drawing, and on every one of their
2176 pixels the grey is exactly where our sheet's alpha is zero.  The other
seven are scrolls, which this game draws by flavour, so the archive's scroll
names point at other pictures.
"""

import importlib.util
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
TILES = os.path.join(ROOT, 'lib', 'tiles')
DATA = os.path.join(ROOT, 'lib', 'gamedata')
ARCHIVE = os.path.join(ROOT, 'archive', 'zangband', 'lib')

# The PNG codec and the monster reader live in their siblings.  Same pattern
# make-services.py uses: one implementation, imported, rather than a third copy
# that can drift.
def _load(name, mod):
    spec = importlib.util.spec_from_file_location(mod, os.path.join(HERE, name))
    out = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(out)
    return out


mt = _load('make-terrain.py', 'make_terrain')
bt = _load('borrow-tiles.py', 'borrow_tiles')

# directory -> where its tiles come from, where they go, and what it takes.
#
# `mask` is the archive's 1-bit mask, black for opaque, or None for a sheet
# whose tiles sit on a flat `background` instead.  `monsters` is whether the
# set takes uniques as well as objects: the 8x8 archive cannot say which cell
# is which monster (see above).
TARGETS = {
    'adam-bolt': {
        'sheet': '16x16.png', 'tile': 16, 'prf': 'graf-new.prf',
        'image': 'xtra/graf/16x16.bmp', 'mask': 'xtra/graf/mask.bmp',
        'archive': 'pref/graf-new.prf', 'output': 'uniq-ztk.prf',
        'monsters': True,
        'title': "Adam Bolt's Zangband tiles, brought into Adam Bolt's sheet",
        'who': 'drawn by Adam Bolt for Zangband, for the character -- or,\n'
               '# at the end, the object --',
        'chained': "Adam Bolt's Zangband uniques",
    },
    'old': {
        'sheet': '8x8.png', 'tile': 8, 'prf': 'graf-xxx.prf',
        'image': 'xtra/graf/8x8.bmp', 'mask': None,
        'background': (48, 48, 48),
        'archive': 'pref/graf-xxx.prf', 'output': 'zang-ztk.prf',
        'monsters': False,
        'title': "Zangband's own 8x8 tiles, brought into the 8x8 sheet",
        'who': 'drawn for Zangband, in its copy of this tileset, for the\n'
               '# object',
        'chained': "Zangband's own 8x8 tiles",
    },
}

HEADER = """# %(output)s -- %(title)s.
#
# GENERATED by scripts/tiles/import-zangband-tiles.py, together with the rows
# of tiles it points at.  Do not edit either by hand: re-running the script
# overwrites both, and reclaims its own rows rather than appending more.
#
# Every tile here was %(who)s it is used for.  Nothing is borrowed from anything
# else and nothing is generated.  The comment on each line gives the cell it
# came from in archive/zangband/lib/%(image)s.
#
# Replacing one with newer art means deleting its line here and adding a
# mapping to %(prf)s; re-running will not put it back, because a line is
# only written for a thing that has no tile from any other file.
"""


def read_bmp(path):
    """8-bit palettised BMP -> rows of (r, g, b), top row first."""
    data = open(path, 'rb').read()
    assert data[:2] == b'BM', path + ' is not a BMP'
    offset = struct.unpack('<I', data[10:14])[0]
    header = struct.unpack('<I', data[14:18])[0]
    width, height = struct.unpack('<ii', data[18:26])
    depth = struct.unpack('<H', data[28:30])[0]
    assert depth == 8, '%s is %d-bit; only 8 is handled' % (path, depth)

    count = struct.unpack('<I', data[46:50])[0] or 256
    palette, at = [], 14 + header
    for _ in range(count):
        b, g, r, _unused = data[at:at + 4]
        palette.append((r, g, b))
        at += 4

    bottom_up = height > 0
    height = abs(height)
    stride = (width + 3) // 4 * 4          # rows are padded to four bytes
    rows = [[palette[data[offset + y * stride + x]] for x in range(width)]
            for y in range(height)]
    if bottom_up:
        rows.reverse()
    return rows


def archive_assignments(path):
    """name -> (row, col) from a 2001 prf.

    The file writes `#N:index:Name` above each `R:index:0xRR/0xCC`, and in a
    few places just `# Name`.  Both forms are taken: reading only the second
    loses a monster, which is the sort of thing that looks like the archive
    being incomplete when it is the reader being careless.
    """
    text = open(path, encoding='utf-8', errors='replace').read()
    found = re.findall(
        r'^#\s*(?:N:\d+:)?\s*(.+?)\s*\nR:\d+:0x([0-9A-Fa-f]{2})'
        r'/0x([0-9A-Fa-f]{2})', text, re.M)
    return {name.strip().lower(): (int(r, 16) - 0x80, int(c, 16) - 0x80)
            for name, r, c in found}


def archive_objects(path):
    """name -> (row, col) for the object kinds a 2001 prf names.

    `# Name` above each `K:index:0xRR/0xCC` -- graf-new.prf writes a slash
    between the two, as it does for monsters.  Lowercased, and compared with
    object.txt's names as lookup_sval() would see them.
    """
    text = open(path, encoding='utf-8', errors='replace').read()
    found = re.findall(
        r'^#\s*(.+?)\s*\nK:\d+:0x([0-9A-Fa-f]{2})[/:]0x([0-9A-Fa-f]{2})',
        text, re.M)
    return {name.strip().lower(): (int(r, 16) - 0x80, int(c, 16) - 0x80)
            for name, r, c in found}


def wanted_objects(directory, pref):
    """The object kinds this set has no tile for, that the archive can supply.

    Only kinds that carry their own colour: a flavoured kind is drawn as its
    flavour.  Minus what we wrote last time, for the reason wanted() gives.
    """
    have = set(bt.read_prf_chain(directory, pref)['object'])
    ours = os.path.join(TILES, directory, TARGETS[directory]['output'])
    if os.path.exists(ours):
        mine = re.findall(r'^object:(.+):0x[0-9A-Fa-f]{2}:0x[0-9A-Fa-f]{2}\s*$',
                          open(ours, encoding='utf-8').read(), re.M)
        have -= {n.lower() for n in mine}

    archive = archive_objects(os.path.join(ARCHIVE,
                                           TARGETS[directory]['archive']))
    pairs = [('%s:%s' % (tval, name), archive[name.lower()])
             for tval, name in bt.read_objects()
             if ('%s:%s' % (tval, name)).lower() not in have
             and name.lower() in archive]
    pairs.sort(key=lambda p: (p[1], p[0]))
    return pairs


def wanted(directory, pref):
    """The uniques this set has no tile for, that the archive can supply.

    Ordered by the archive's own cell order rather than by name, so the rows
    this writes stay stable when a monster is renamed.
    """
    monsters = {}
    for name in bt.MONSTERS:
        monsters.update(bt.read_monsters(name))
    uniques = [n for n, v in monsters.items() if v['unique']]

    have = {k.lower() for k in bt.read_prf_chain(directory, pref)['monster']}

    # Minus whatever *we* wrote last time.
    #
    # borrow-tiles.py's read_prf_chain skips one generated file by name -- its
    # own -- and documents why: without it a second run sees everything the
    # first run wrote as art that already exists, decides there is nothing left
    # to do, and empties the file it wrote yesterday.  Ours is a different file
    # and is not skipped, so the same trap is here and has to be sprung
    # separately.  It was: the first version of this script imported 65 tiles
    # and then zero.
    ours = os.path.join(TILES, directory, TARGETS[directory]['output'])
    if os.path.exists(ours):
        mine = re.findall(r'^monster:(.+):0x[0-9A-Fa-f]{2}:0x[0-9A-Fa-f]{2}\s*$',
                          open(ours, encoding='utf-8').read(), re.M)
        have -= {n.lower() for n in mine}

    missing = [n for n in uniques if n.lower() not in have]

    archive = archive_assignments(os.path.join(ARCHIVE,
                                               TARGETS[directory]['archive']))
    pairs = [(n, archive[n.lower()]) for n in missing if n.lower() in archive]
    pairs.sort(key=lambda p: (p[1], p[0]))
    return pairs, len(missing), len(archive)


def rows_of(directory, name):
    """The sheet rows a generated prf points at."""
    path = os.path.join(TILES, directory, name)
    if not os.path.exists(path):
        return []
    seen = re.findall(r':0x([0-9A-Fa-f]{2}):0x[0-9A-Fa-f]{2}\s*$',
                      open(path, encoding='utf-8').read(), re.M)
    return sorted({int(v, 16) - 0x80 for v in seen})


def emit(directory):
    target = TARGETS[directory]
    sheet, tile, output = target['sheet'], target['tile'], target['output']
    if target['monsters']:
        pairs, missing, named = wanted(directory, target['prf'])
    else:
        pairs, missing, named = [], 0, 0
    things = ([('monster', n, at) for n, at in pairs]
              + [('object', n, at)
                 for n, at in wanted_objects(directory, target['prf'])])

    src = read_bmp(os.path.join(ARCHIVE, target['image']))
    alpha = (read_bmp(os.path.join(ARCHIVE, target['mask']))
             if target['mask'] else None)

    def opaque(y, x):
        if alpha:
            return alpha[y][x] == (0, 0, 0)
        return src[y][x] != target['background']

    pixels = mt.read_png(os.path.join(TILES, directory, sheet))
    width = len(pixels[0])
    cols = width // tile

    # Reclaim our own rows, exactly as make-terrain.py does, so running this
    # twice is the same as running it once.
    #
    # Unlike make-terrain.py this is safe to truncate at, because these rows
    # are the last ones in the sheet and nothing is written below them.  That
    # is a standing requirement, not an accident: see --check.
    start = len(pixels) // tile
    previous = rows_of(directory, output)
    if previous:
        start = min(previous)
        del pixels[start * tile:]

    need = (len(things) + cols - 1) // cols
    for _ in range(need * tile):
        pixels.append([(0, 0, 0, 0)] * width)

    lines = [HEADER % dict(target, output=output)]
    for n, (what, name, (sr, sc)) in enumerate(things):
        row, col = start + n // cols, n % cols
        for y in range(tile):
            for x in range(tile):
                rgb = src[sr * tile + y][sc * tile + x]
                pixels[row * tile + y][col * tile + x] = \
                    ((rgb[0], rgb[1], rgb[2], 255)
                     if opaque(sr * tile + y, sc * tile + x) else (0, 0, 0, 0))
        lines.append('# from the Zangband sheet at row %d, col %d' % (sr, sc))
        lines.append('%s:%s:0x%02X:0x%02X'
                     % (what, name, row + 0x80, col + 0x80))

    mt.write_png(os.path.join(TILES, directory, sheet), pixels)
    open(os.path.join(TILES, directory, output), 'w',
         encoding='utf-8').write('\n'.join(lines) + '\n')
    return len(pairs), missing, named, start, need


def chain(directory):
    """Make sure the tileset's main prf loads the new file, once."""
    target = TARGETS[directory]
    path = os.path.join(TILES, directory, target['prf'])
    text = open(path, encoding='utf-8').read()
    if target['output'] in text:
        return False
    open(path, 'w', encoding='utf-8').write(
        text.rstrip('\n')
        + "\n\n# %s, imported by scripts/tiles/import-zangband-tiles.py\n"
          "%%:%s\n" % (target['chained'], target['output']))
    return True


def check():
    """Verify what is on disk, writing nothing."""
    monsters = {}
    for name in bt.MONSTERS:
        monsters.update(bt.read_monsters(name))
    known = {n.lower(): v for n, v in monsters.items()}

    problems = 0
    for directory in sorted(TARGETS):
        target = TARGETS[directory]
        sheet, tile, output = target['sheet'], target['tile'], target['output']
        path = os.path.join(TILES, directory, output)
        if not os.path.exists(path):
            print('  %-10s no %s -- nothing imported yet'
                  % (directory, output))
            continue

        pixels = mt.read_png(os.path.join(TILES, directory, sheet))
        rows, cols = len(pixels) // tile, len(pixels[0]) // tile
        text = open(path, encoding='utf-8').read()
        lines = re.findall(r'^monster:(.+):0x([0-9A-Fa-f]{2}):'
                           r'0x([0-9A-Fa-f]{2})\s*$', text, re.M)
        objects = re.findall(r'^object:(.+):0x([0-9A-Fa-f]{2}):'
                             r'0x([0-9A-Fa-f]{2})\s*$', text, re.M)
        kinds = {('%s:%s' % k).lower() for k in bt.read_objects()}

        bad_name = ([n for n, _, _ in lines if n.lower() not in known]
                    + [n for n, _, _ in objects if n.lower() not in kinds])
        not_unique = [n for n, _, _ in lines
                      if n.lower() in known and not known[n.lower()]['unique']]
        outside = [(n, int(r, 16) - 0x80, int(c, 16) - 0x80)
                   for n, r, c in lines + objects
                   if int(r, 16) - 0x80 >= rows or int(c, 16) - 0x80 >= cols]
        blank = []
        for n, r, c in lines + objects:
            row, col = int(r, 16) - 0x80, int(c, 16) - 0x80
            if row >= rows or col >= cols:
                continue
            cell = [p for y in range(tile)
                    for p in pixels[row * tile + y][col * tile:(col + 1) * tile]]
            if not any(p[3] for p in cell):
                blank.append(n)

        # Ours must be the last rows in the sheet.  make-terrain.py reclaims
        # its row by truncating back to it, so anything below a generated row
        # is deleted when that generator runs -- which is why the house rule is
        # terrain, then services, then this.
        mine = rows_of(directory, output)
        others = (rows_of(directory, 'feat-ztk.prf')
                  + rows_of(directory, 'serv-ztk.prf'))
        below = [r for r in others if mine and r > max(mine)]

        for label, items in (('names nothing in the game has', bad_name),
                             ('names a monster that is not unique', not_unique),
                             ('coordinates outside the sheet',
                              ['%s (%d,%d)' % t for t in outside]),
                             ('point at a cell with no pixels in it', blank),
                             ('generated rows sit below ours',
                              [str(r) for r in below])):
            if items:
                problems += len(items)
                print('  %-10s %d %s: %s%s'
                      % (directory, len(items), label,
                         ', '.join(sorted(items)[:4]),
                         ' ...' if len(items) > 4 else ''))

        # And the set's own prf has to load it, or the tiles ship unused.
        unchained = ([] if output in open(os.path.join(
            TILES, directory, target['prf']), encoding='utf-8').read()
            else [target['prf']])
        if unchained:
            problems += 1
            print('  %-10s %s does not load %s' % (directory, unchained[0],
                                                   output))

        if not (bad_name or not_unique or outside or blank or below
                or unchained):
            print('  %-10s %d uniques and %d objects on rows %s; sheet is %d '
                  'rows x %d cols'
                  % (directory, len(lines), len(objects),
                     '-'.join(str(r) for r in (min(mine), max(mine)))
                     if mine else '-', rows, cols))

    print('\n%d problems' % problems)
    return 1 if problems else 0


USAGE = """\
scripts/tiles/import-zangband-tiles.py --check    verify; writes nothing
scripts/tiles/import-zangband-tiles.py --write    import the tiles

--write appends rows to a tileset's sheet and must be the last generator to
run, because make-terrain.py reclaims its own row by truncating the sheet back
to it and would take these with it.  The order is make-terrain, make-services,
then this.  --check asserts that it still holds.
"""


def main():
    if '--check' in sys.argv:
        return check()
    if '--write' not in sys.argv:
        sys.stdout.write(USAGE)
        return 2

    for directory in sorted(TARGETS):
        got, missing, named = emit(directory)[:3]
        linked = chain(directory)
        print('  %-10s %d of %d missing uniques imported, from %d named in'
              ' the archive, and %d objects%s'
              % (directory, got, missing, named,
                 len(re.findall(r'^object:', open(os.path.join(
                     TILES, directory, TARGETS[directory]['output']),
                     encoding='utf-8').read(), re.M)),
                 '; prf chained' if linked else ''))
    print('\n  Run scripts/tiles/audit-tiles.py to see the gap count fall.')
    print('  A new prf also has to be added by hand to the set\'s Makefile'
          '\n  DATA line, or a packaged build installs the sheet without it:'
          '\n  the tiles ship and nothing references them.  '
          'scripts/check-build-lists\n  is what says so, and it is the'
          ' authoritative list rather than this one.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
