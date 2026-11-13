#!/usr/bin/env python3
"""
Build the 1982 tileset: early-80s fantasy pixel art, drawn by an AI image
generator to a brief and finished here.

The art arrived as 39 sheets of sixteen pictures, each drawn far larger than a
tile and in roughly the right colours.  Turning them into tiles is two steps,
and only the second runs normally:

  * Cutting (`--cut`).  Each accepted picture is averaged down to 32x32 art
    pixels and snapped to the DB16 palette, with the clean-ups recipe.txt
    lists for it -- a window where the figure strayed out of its cell, a
    rectangle cleared of a neighbour's spear, a crimson that the palette would
    have turned brown sent to red instead.  The result is source.png, one
    small atlas of every tile as drawn, which is the set's source art.  The
    93 MB of sheets it comes from stay in the design brief.

  * Building (the default).  From source.png alone: the floor is drawn from
    its recipe, recolourable pictures are tinted with each thing's own colour
    from lib/gamedata, figures get their outline, rubble goes onto the floor
    and the buildings onto grass with their shop number, terrain gets its four
    lights, and graf-82.prf says where everything is.

    scripts/tiles/build-1982.py               build the sheet and the prf
    scripts/tiles/build-1982.py --check       ...and report what is unused
    scripts/tiles/build-1982.py --preview     ...and draw a mock map
    scripts/tiles/build-1982.py --cut [BRIEF] cut source.png from the sheets

Recolourable pictures are the idea Neon is built on, reached from the other
side: Neon draws a silhouette and colours it, these draw a picture whose
changing part is in four greys, and the tint keeps the shading.  One young
dragon is every young dragon, each the colour monster.txt says it is.

Deterministic: re-running produces the same sheet and prf.  Building needs
nothing but Python; cutting also needs ImageMagick, to read the sheets.
"""

import collections
import csv
import importlib.util
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
DATA = os.path.join(ROOT, 'lib', 'gamedata')
SRC = os.path.join(HERE, '1982')
OUT = os.path.join(ROOT, 'lib', 'tiles', '1982')
BRIEF = os.path.join(ROOT, '.claude', 'plans', 'design_brief_tileset_1982')

TILE = 32
COLUMNS = 32
MONSTERS = ('monster.txt', 'monster.zangband.txt', 'monster.zangbandtk.txt')
OBJECTS = ('object.txt', 'object.zangband.txt')

# Neon's readers, which already know how the data files are spelt.
_spec = importlib.util.spec_from_file_location(
    'build_neon', os.path.join(HERE, 'build-neon.py'))
neon = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(neon)
read_png, write_png = neon.read_png, neon.write_png
read_records, object_name, read_flavours = (
    neon.read_records, neon.object_name, neon.read_flavours)


# --- the palette -------------------------------------------------------------

# DB16, under the names the brief gave it.
PALETTE = collections.OrderedDict((
    ('ink', (0x14, 0x0C, 0x1C)), ('plum', (0x44, 0x24, 0x34)),
    ('navy', (0x30, 0x34, 0x6D)), ('iron', (0x4E, 0x4A, 0x4E)),
    ('brown', (0x85, 0x4C, 0x30)), ('moss', (0x34, 0x65, 0x24)),
    ('red', (0xD0, 0x46, 0x48)), ('stone', (0x75, 0x71, 0x61)),
    ('blue', (0x59, 0x7D, 0xCE)), ('orange', (0xD2, 0x7D, 0x2C)),
    ('steel', (0x85, 0x95, 0xA1)), ('green', (0x6D, 0xAA, 0x2C)),
    ('flesh', (0xD2, 0xAA, 0x99)), ('cyan', (0x6D, 0xC2, 0xCA)),
    ('yellow', (0xDA, 0xD4, 0x5E)), ('bone', (0xDE, 0xEE, 0xD6)),
))
C = PALETTE
# The four greys a recolourable picture draws its changing part in, and how
# bright each makes the colour it is tinted with.
GREYS = {0x33: .3, 0x66: .55, 0x99: .8, 0xCC: 1.0}
GREY_RGB = [(g, g, g) for g in GREYS]
# Too close to the greys to share a recolourable picture with them.
NOT_IN_RECOLOUR = ('plum', 'iron', 'stone', 'steel', 'bone')
FIXED_PAL = list(PALETTE.values())
RECOLOUR_PAL = [v for k, v in PALETTE.items() if k not in NOT_IN_RECOLOUR] + GREY_RGB


def colour(word):
    """A palette name, or six hex digits (a recolour grey), as RGB."""
    if word in PALETTE:
        return PALETTE[word]
    assert len(word) == 6, 'not a colour: %r' % word
    return tuple(int(word[i:i + 2], 16) for i in (0, 2, 4))


def lum(c):
    v = [x / 255 for x in c]
    v = [x / 12.92 if x <= 0.03928 else ((x + 0.055) / 1.055) ** 2.4 for x in v]
    return 0.2126 * v[0] + 0.7152 * v[1] + 0.0722 * v[2]


def near(c, pal):
    return min(pal, key=lambda p: sum((a - b) ** 2 for a, b in zip(c, p)))


def magenta(c):
    """The sheets' background, keyed the way every batch was tested."""
    return min(c[0], c[2]) - c[1] > 90


# --- the game's colours ------------------------------------------------------

def game_colours():
    """Colour letter -> RGB, from z-color.c's two tables."""
    text = open(os.path.join(ROOT, 'src', 'z-color.c'), encoding='utf-8').read()
    import re
    table = [tuple(int(v, 16) for v in m)
             for m in re.findall(r'\{0x00, (0x[0-9a-f]{2}), (0x[0-9a-f]{2}), '
                                 r'(0x[0-9a-f]{2})\}', text)]
    out = {}
    for code, index in re.findall(r"\{'(.)',\s*\"[^\"]+\",\s*\{(\d+),", text):
        out[code] = table[int(index)]
    # Dark is black, and a black monster on a near-black floor is nothing.
    # The darkest grey a tint can still shade.
    out['d'] = (0x30, 0x30, 0x30)
    return out


GAME = game_colours()
NAMES = neon.colour_names()


def game_rgb(spec):
    """A gamedata colour, letter or name, as RGB; None if it is neither."""
    spec = spec.strip()
    if len(spec) == 1 and spec in GAME:
        return GAME[spec]
    code = NAMES.get(neon._fold(spec))
    return GAME.get(code)


# --- tiles -------------------------------------------------------------------
#
# A tile is 1024 pixels, row by row; a pixel is an RGB tuple or None for
# transparent.

def tint(t, rgb):
    return [tuple(min(255, int(c * GREYS[p[0]])) for c in rgb)
            if p is not None and p[0] == p[1] == p[2] and p[0] in GREYS else p
            for p in t]


def outline(t, force=None):
    """One pixel round the shape, outside it and inside the tile: ink, or
    stone round a figure nine-tenths darker than the floor's iron, which an
    ink outline does nothing for (the owner's ruling, 7 November 2026)."""
    px = [p for p in t if p is not None]
    if not px:
        return list(t)
    dark = sum(lum(p) < lum(C['iron']) for p in px) >= .9 * len(px)
    edge = force or (C['stone'] if dark else C['ink'])
    out = list(t)
    for j in range(TILE):
        for i in range(TILE):
            if t[j * TILE + i] is None and any(
                    0 <= i + di < TILE and 0 <= j + dj < TILE
                    and t[(j + dj) * TILE + i + di] is not None
                    for di, dj in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                out[j * TILE + i] = edge
    return out


def on(t, ground):
    return [p if p is not None else g for p, g in zip(t, ground)]


def draw_floor():
    """The bevelled slab the owner chose on 2 November 2026 (floor.png in the
    brief), drawn from recipe.txt's description of it."""
    t = [C['iron']] * (TILE * TILE)

    def put(x, y, name):
        t[y * TILE + x] = C[name]
    for k in range(TILE):
        put(31, k, 'ink')
        put(k, 31, 'ink')
    for k in range(31):
        put(k, 0, 'stone')
        put(0, k, 'stone')
    for k in range(1, 30):
        put(k, 30, 'plum')
        put(30, k, 'plum')
    for x, y in ((0, 0), (30, 0), (0, 30), (30, 30)):
        put(x, y, 'ink')
    put(1, 0, 'iron')
    put(0, 1, 'iron')
    return t


# The shop numbers and the services' letters, 3x5, 1 = bone (recipe.txt).
GLYPHS = {
    '1': '010110010010111', '2': '111001111100111', '3': '111001111001111',
    '4': '101101111001001', '5': '111100111001111', '6': '111100111101111',
    '7': '111001010010010', '8': '111101111101111', '9': '111101111001111',
    '0': '111101101101111', 'H': '101101111101101', 'I': '111010010010111',
    'M': '101111111101101', 'R': '110101110101101', 'C': '111100100100111',
}


def stamp(t, glyph, x0=14, y0=6):
    t = list(t)
    for y in range(y0 - 1, y0 + 6):
        for x in range(x0 - 1, x0 + 4):
            t[y * TILE + x] = C['ink']
    for n, bit in enumerate(GLYPHS[glyph]):
        if bit == '1':
            t[(y0 + n // 3) * TILE + x0 + n % 3] = C['bone']
    return t


# The game draws each terrain feature four ways (map_info, cave-map.c):
#
#   los    seen and lit -- as drawn, which is how the floor and walls were judged
#   torch  seen and lit within the player's light, but only with the option
#          "Illuminate torchlight in yellow" on: warmed, as by a flame
#   lit    every other square, which mostly means remembered but not in view:
#          dimmed and a little greyed, so the map you know reads apart from the
#          room you are in.  Not "lit" in the everyday sense, and the first
#          build had it at full brightness, which made a remembered corridor
#          look like the room beside it.
#   dark   only for a character with UNLIGHT and almost no light: dimmer still
#
# A thing standing on the terrain has one tile; the Tk front end dims it when
# the square is remembered (main-tcl.c).
#
# The torch is a warm light multiplied in rather than a colour mixed in, so it
# warms the floor without washing it out: the slab goes from 2.4:1 to 2.9:1
# against black, under the 3:1 at which figures start to sink into it.
TORCH = (1.3, 1.15, .85)
DIM = {'lit': .62, 'dark': .42}


def light(t, how):
    if how == 'los':
        return t
    out = []
    for p in t:
        if how == 'torch':
            out.append(tuple(min(255, round(c * k)) for c, k in zip(p, TORCH)))
        else:
            grey = .3 * p[0] + .59 * p[1] + .11 * p[2]
            out.append(tuple(round((c * .7 + grey * .3) * DIM[how]) for c in p))
    return out


# --- the source atlas --------------------------------------------------------

def read_source():
    """id -> (tile, kind), from source.png and source.txt."""
    rows = read_png(os.path.join(SRC, 'source.png'))
    out = {}
    for line in open(os.path.join(SRC, 'source.txt'), encoding='utf-8'):
        if not line.strip() or line.startswith('#'):
            continue
        n, ident, kind = line.split()
        n = int(n)
        ox, oy = (n % COLUMNS) * TILE, (n // COLUMNS) * TILE
        t = []
        for y in range(TILE):
            for x in range(TILE):
                r, g, b, a = rows[oy + y][ox + x]
                t.append((r, g, b) if a else None)
        out[ident] = (t, kind)
    return out


def write_source(tiles):
    """tiles: [(id, kind, tile)] -> source.png and source.txt."""
    rows_n = (len(tiles) + COLUMNS - 1) // COLUMNS
    px = [[(0, 0, 0, 0)] * (COLUMNS * TILE) for _ in range(rows_n * TILE)]
    with open(os.path.join(SRC, 'source.txt'), 'w', encoding='utf-8') as f:
        f.write('# source.txt -- what is where in source.png: the index of each\n'
                '# 32x32 tile, its id and whether it is fixed or recolourable.\n'
                '# GENERATED by build-1982.py --cut.  Do not edit.\n')
        for n, (ident, kind, t) in enumerate(tiles):
            f.write('%d %s %s\n' % (n, ident, kind))
            ox, oy = (n % COLUMNS) * TILE, (n // COLUMNS) * TILE
            for y in range(TILE):
                for x in range(TILE):
                    p = t[y * TILE + x]
                    px[oy + y][ox + x] = (0, 0, 0, 0) if p is None else p + (255,)
    write_png(os.path.join(SRC, 'source.png'), px)


# --- cutting -----------------------------------------------------------------

class SheetImage:
    def __init__(self, path, cache):
        ppm = os.path.join(cache, '%d.ppm' % abs(hash(path)))
        subprocess.run(['magick', path, '-depth', '8', ppm], check=True)
        head = open(ppm, 'rb').read().split(b'\n', 3)
        self.w = int(head[1].split()[0])
        self.px = head[3]
        self._shapes = None

    def at(self, x, y):
        q = (y * self.w + x) * 3
        return self.px[q], self.px[q + 1], self.px[q + 2]

    def shapes(self, s=4):
        """Every drawing on the sheet, labelled at a quarter of its size: a
        pixel is in one when two of its four samples are not magenta, and
        pixels touching on a side or a corner are one shape."""
        if self._shapes:
            return self._shapes
        w = self.w // s
        mask = bytearray(w * w)
        for y in range(w):
            for x in range(w):
                n = 0
                for yy in range(y * s, y * s + s, 2):
                    for xx in range(x * s, x * s + s, 2):
                        n += not magenta(self.at(xx, yy))
                mask[y * w + x] = n >= 2
        lab, boxes, nxt = [0] * (w * w), {}, 0
        for s0 in range(w * w):
            if not mask[s0] or lab[s0]:
                continue
            nxt += 1
            lab[s0] = nxt
            stack, xs, ys = [s0], [], []
            while stack:
                v = stack.pop()
                y, x = divmod(v, w)
                xs.append(x)
                ys.append(y)
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        yy, xx = y + dy, x + dx
                        if 0 <= yy < w and 0 <= xx < w:
                            u = yy * w + xx
                            if mask[u] and not lab[u]:
                                lab[u] = nxt
                                stack.append(u)
            boxes[nxt] = (sum(xs) * s // len(xs), sum(ys) * s // len(ys), len(xs))
        self._shapes = (lab, w, s, boxes)
        return self._shapes


def parse_recipe():
    """id -> (sheet, cell, steps), the last line for an id winning."""
    out = collections.OrderedDict()
    for line in open(os.path.join(SRC, 'recipe.txt'), encoding='utf-8'):
        words = line.split('#')[0].split()
        if not words:
            continue
        ident, sheet, cell = words[0], words[1], int(words[2])
        out.pop(ident, None)
        out[ident] = (sheet, cell, words[3:])
    return out


def cut_one(img, cell, steps, recolour):
    """recipe.txt's steps for one tile, in the order its header gives."""
    window, own, thin, extra = None, False, None, {}
    swaps, ordered = {}, []
    for step in steps:
        if step.startswith('at:'):
            window = tuple(int(v) for v in step[3:].split(','))
        elif step == 'own':
            own = True
        elif step.startswith('thin:'):
            thin = float(step[5:])
        elif step.startswith('+'):
            hexa, name = step[1:].split('>')
            extra[colour(hexa)] = colour(name)
        elif step.startswith('clear:') or step == 'largest':
            ordered.append(step)
        else:
            a, b = step.split('>')
            swaps[colour(a)] = None if b == 'none' else colour(b)

    cw = img.w // 4
    if window:
        x0, y0, size = window
    else:
        x0, y0, size = (cell - 1) % 4 * cw, (cell - 1) // 4 * cw, cw
    block = size // TILE
    mine = None
    if own:
        lab, w, s, boxes = img.shapes()
        mine = {k for k, (cx, cy, area) in boxes.items()
                if area >= 3 and cx // cw == (cell - 1) % 4 and cy // cw == (cell - 1) // 4}
    pal = (RECOLOUR_PAL if recolour else FIXED_PAL) + list(extra)

    t = []
    for j in range(TILE):
        for i in range(TILE):
            acc, n, m = [0, 0, 0], 0, 0
            for y in range(y0 + j * block, y0 + (j + 1) * block):
                for x in range(x0 + i * block, x0 + (i + 1) * block):
                    if not (0 <= x < img.w and 0 <= y < img.w):
                        m += 1
                        continue
                    if mine is not None and lab[(y // s) * w + x // s] not in mine:
                        m += 1
                        continue
                    c = img.at(x, y)
                    if magenta(c):
                        m += 1
                        continue
                    acc[0] += c[0]
                    acc[1] += c[1]
                    acc[2] += c[2]
                    n += 1
            keep = n >= thin * (n + m) if thin is not None else n > m
            if not (keep and n):
                t.append(None)
                continue
            p = near(tuple(a // n for a in acc), pal)
            t.append(extra.get(p, p))

    for step in ordered:
        if step == 'largest':
            t = largest(t)
        else:
            xs, ys = step[6:].split(',')
            ax, bx = map(int, xs.split('-'))
            ay, by = map(int, ys.split('-'))
            t = [None if ax <= n % TILE <= bx and ay <= n // TILE <= by else p
                 for n, p in enumerate(t)]
    return [swaps.get(p, p) if p is not None else None for p in t]


def largest(t):
    seen, best = set(), []
    for s0 in range(TILE * TILE):
        if t[s0] is None or s0 in seen:
            continue
        stack, comp = [s0], []
        seen.add(s0)
        while stack:
            v = stack.pop()
            comp.append(v)
            y, x = divmod(v, TILE)
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    yy, xx = y + dy, x + dx
                    u = yy * TILE + xx
                    if 0 <= yy < TILE and 0 <= xx < TILE and t[u] is not None \
                            and u not in seen:
                        seen.add(u)
                        stack.append(u)
        if len(comp) > len(best):
            best = comp
    keep = set(best)
    return [p if n in keep else None for n, p in enumerate(t)]


def cut(brief):
    """Cut every accepted tile from the brief's sheets into source.png, and
    write monsters.txt from its monsters.csv."""
    recolour = set()
    for name in ('sprites.csv', 'second-set.csv'):
        for r in csv.DictReader(open(os.path.join(brief, name), encoding='utf-8')):
            if r['kind'] == 'recolour':
                recolour.add(r['id'])
    recipe = parse_recipe()
    tiles = []
    with tempfile.TemporaryDirectory() as cache:
        images = {}
        for ident, (sheet, cell, steps) in recipe.items():
            if sheet not in images:
                print('  %s' % sheet)
                images[sheet] = SheetImage(os.path.join(brief, sheet), cache)
            kind = 'recolour' if ident in recolour else 'fixed'
            tiles.append((ident, kind, cut_one(images[sheet], cell, steps,
                                               kind == 'recolour')))
    write_source(tiles)
    print('  %d tiles -> %s' % (len(tiles), os.path.join(SRC, 'source.png')))

    # The floor is drawn here rather than cut; prove the recipe draws exactly
    # the slab the owner chose.
    with tempfile.TemporaryDirectory() as cache:
        ref = SheetImage(os.path.join(brief, 'floor.png'), cache)
        drawn = draw_floor()
        bad = sum(ref.at(n % TILE, n // TILE) != drawn[n] for n in range(TILE * TILE))
        assert not bad, 'the floor recipe differs from floor.png in %d pixels' % bad

    write_monsters(brief)


def write_monsters(brief):
    rows = list(csv.DictReader(open(os.path.join(brief, 'monsters.csv'),
                                    encoding='utf-8')))
    # Each base's commonest family tile; a base all of whose monsters now
    # have pictures of their own keeps the family tile it was drawn with.
    by_base = collections.defaultdict(collections.Counter)
    for r in rows:
        if r['plan'] == 'family':
            by_base[r['family']][r['own_tile']] += 1
    for r in rows:
        if r['family'] not in by_base:
            by_base[r['family']][r['first_set_tile']] += 1
    with open(os.path.join(SRC, 'monsters.txt'), 'w', encoding='utf-8') as f:
        f.write(MONSTERS_HEADER)
        f.write('\n# --- each monster base\'s commonest tile, for monsters added '
                'later ---\n')
        for base in sorted(by_base):
            f.write('base:%s:%s\n' % (base, by_base[base].most_common(1)[0][0]))
        for plan, title in (('done', 'a picture of its own'),
                            ('shared', 'a picture shared with look-alikes'),
                            ('repoint', 'a tile drawn for something else'),
                            ('family', 'its family\'s picture, in its colour')):
            f.write('\n# --- %s ---\n' % title)
            for r in rows:
                if r['plan'] == plan:
                    f.write('monster:%s:%s\n' % (r['name'], r['own_tile']))
    print('  %d monsters -> %s' % (len(rows), os.path.join(SRC, 'monsters.txt')))


MONSTERS_HEADER = """\
# monsters.txt -- which 1982 tile each monster is drawn with.
#
# GENERATED by build-1982.py --cut from the brief's monsters.csv, where each
# monster's tile was decided.  The owner ruled on 7 November 2026 that only the
# uniques and the monsters that look clearly unlike their family get a picture
# of their own; the rest are their family's picture in their own colour.  A
# monster not listed takes its base's line.
"""


# --- the manifest ------------------------------------------------------------

def read_manifest():
    m = {'feat': {}, 'tval': {}, 'object': {}, 'trap': {}, 'gf': {},
         'player': {}}
    for line in open(os.path.join(SRC, 'manifest.txt'), encoding='utf-8'):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        kind, rest = line.split(':', 1)
        if kind == 'feat':
            bits = rest.split(':')
            m['feat'][bits[0]] = bits[1:]
        elif kind == 'object':
            tval, name, tile = rest.split(':')
            m['object'][(tval, name.lower())] = tile
        else:
            key, tile = rest.rsplit(':', 1)
            m[kind][key] = tile
    return m


def read_monster_map():
    names, bases = {}, {}
    for line in open(os.path.join(SRC, 'monsters.txt'), encoding='utf-8'):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        kind, rest = line.split(':', 1)
        key, tile = rest.rsplit(':', 1)
        (names if kind == 'monster' else bases)[key.lower()] = tile
    return names, bases


# --- the sheet ---------------------------------------------------------------

class Sheet:
    """Finished tiles, each stored once however many things use it."""

    def __init__(self):
        self.tiles, self.index = [], {}

    def add(self, t):
        key = tuple(t)
        if key not in self.index:
            self.index[key] = len(self.tiles)
            self.tiles.append(t)
        n = self.index[key]
        return 0x80 | (n // COLUMNS), 0x80 | (n % COLUMNS)

    def render(self):
        rows_n = (len(self.tiles) + COLUMNS - 1) // COLUMNS
        assert rows_n <= 128, 'a sheet is at most 128 rows of tiles'
        px = [[(0, 0, 0, 0)] * (COLUMNS * TILE) for _ in range(rows_n * TILE)]
        for n, t in enumerate(self.tiles):
            ox, oy = (n % COLUMNS) * TILE, (n // COLUMNS) * TILE
            for y in range(TILE):
                for x in range(TILE):
                    p = t[y * TILE + x]
                    if p is not None:
                        px[oy + y][ox + x] = tuple(p) + (255,)
        return px


# --- building ----------------------------------------------------------------

LIGHTS = ('torch', 'los', 'lit', 'dark')


def build():
    source = read_source()
    m = read_manifest()
    mon_names, mon_bases = read_monster_map()
    sheet = Sheet()
    lines, counts, used = [], collections.Counter(), set()
    # A base's tile is there for monsters added later, so it counts as used
    # even when every monster of that base today has a picture of its own.
    used.update(mon_bases.values())
    floor = draw_floor()
    grass = source['feat-grass'][0]
    grounds = {'floor': floor, 'grass': grass}
    finished = {}

    def figure(ident, rgb=None):
        """A thing that stands on the terrain: tinted if it is
        recolourable, outlined, and transparent round its outline."""
        key = (ident, rgb)
        if key not in finished:
            t, kind = source[ident]
            if kind == 'recolour':
                assert rgb, '%s is recolourable and has no colour' % ident
                t = tint(t, rgb)
            finished[key] = outline(t)
        used.add(ident)
        return finished[key]

    def emit(text, t):
        row, col = sheet.add(t)
        lines.append('%s:0x%02X:0x%02X' % (text, row, col))

    # Terrain.  Four lines each, one per light.
    lines.append('\n##### Terrain #####\n')
    for record in read_records('terrain.txt', ('name', 'graphics'), start='code'):
        code = record['code']
        entry = m['feat'].get(code)
        if not entry:
            continue
        ident, extra = entry[0], entry[1:]
        if ident == 'black':
            t = [(0, 0, 0)] * (TILE * TILE)
        elif ident == 'feat-floor':
            t = floor
        else:
            t, _ = source[ident]
            used.add(ident)
            if 'outline' in extra:
                t = outline(t, C['ink'])
            ground = next((grounds[e] for e in extra if e in grounds), None)
            if ground:
                t = on(t, ground)
            assert None not in t, '%s has a hole and no ground under it' % code
            for e in extra:
                if e in GLYPHS:
                    t = stamp(t, e)
        for how in LIGHTS:
            emit('feat:%s:%s' % (code, how), light(t, how))
            counts['terrain'] += 1

    # Monsters.  Each one's own colour, from its own record.
    lines.append('\n##### Monsters #####\n')
    for filename in MONSTERS:
        for record in read_records(filename, ('base', 'color')):
            name = record['name']
            ident = mon_names.get(name.lower()) or mon_bases.get(
                record.get('base', '').lower())
            if not ident:
                continue
            rgb = game_rgb(record.get('color', 'w')[:1])
            emit('monster:%s' % name, figure(ident, rgb))
            counts['monsters'] += 1

    # Objects, flavoured ones excepted: before it is identified a potion is
    # its flavour, so those come from flavor.txt below.
    lines.append('\n##### Objects #####\n')
    flavoured = {t for _, _, t in read_flavours()}

    def object_tile(tval, name):
        tile = m['object'].get((tval, name.lower()), m['tval'].get(tval))
        return None if tile in (None, '-') else tile

    for filename in OBJECTS:
        for record in read_records(filename, ('type', 'graphics')):
            tval = record.get('type')
            if tval in flavoured:
                continue
            name = object_name(record['name'])
            ident = object_tile(tval, name)
            if not ident:
                continue
            rgb = game_rgb(record.get('graphics', ':w').split(':')[-1])
            emit('object:%s:%s' % (tval, name), figure(ident, rgb))
            counts['objects'] += 1

    # The books are made from class.txt, coloured by their object base.
    base_colour = {r['name'].split(':')[0]: r.get('graphics', 'w')
                   for r in read_records('object_base.txt', ('graphics',))}
    for line in open(os.path.join(DATA, 'class.txt'), encoding='utf-8'):
        if not line.startswith('book:'):
            continue
        _, tval, _, name = line.strip().split(':')[:4]
        ident = object_tile(tval, name)
        if ident:
            emit('object:%s:%s' % (tval, name),
                 figure(ident, game_rgb(base_colour.get(tval, 'w'))))
            counts['objects'] += 1

    lines.append('\n##### Flavours #####\n')
    for index, code, tval in read_flavours():
        ident = m['tval'].get(tval)
        if ident and ident != '-':
            emit('flavor:%d' % index, figure(ident, GAME.get(code)))
            counts['flavours'] += 1

    # Traps.  Keyed on trap.txt's code, so the four dart traps are one drawing.
    # A trap stands on the floor like an object, so it is one tile in every
    # light and the front end dims it when remembered.
    lines.append('\n##### Traps #####\n')
    for record in read_records('trap.txt', ('graphics',)):
        if ':' not in record['name']:
            continue
        code, specific = record['name'].split(':', 1)
        ident = m['trap'].get(code)
        if not ident:
            continue
        rgb = game_rgb(record.get('graphics', ':w').split(':')[-1])
        t = figure(ident, rgb)
        for how in LIGHTS:
            emit('trap:%s:%s' % (specific, how), t)
        counts['traps'] += 1

    # Spell effects, five drawings tinted with each projection's colour.
    lines.append('\n##### Spell effects #####\n')
    for record in read_records('projection.txt', ('color',), start='code'):
        rgb = game_rgb(record.get('color', 'White'))
        for direction, ident in m['gf'].items():
            emit('GF:%s:%s' % (record['code'], direction), figure(ident, rgb))
            counts['effects'] += 1

    # The player: a hero and a heroine for every class.
    lines.append('\n##### The player #####\n')
    emit('monster:<player>', figure(m['player']['default']))
    for record in read_records('class.txt', ()):
        cls = record['name']
        for sex, suffix in (('Male', ''), ('Female', '-f')):
            ident = 'player-%s%s' % (cls.lower(), suffix)
            if ident not in source:
                continue
            lines.append('?:[AND [EQU $CLASS %s] [EQU $SEX %s] ]' % (cls, sex))
            emit('monster:<player>', figure(ident))
            counts['player'] += 1
    lines.append('?:1')

    return sheet, lines, counts, used, source


HEADER = """\
# graf-82.prf -- the 1982 tileset.
#
# GENERATED by scripts/tiles/build-1982.py, together with 32x32.png beside it.
# Do not edit either by hand: re-running the script overwrites both.  The art
# is scripts/tiles/1982/source.png; which tile each thing gets is that
# directory's manifest.txt and monsters.txt.
#
# Anything absent has no tile and renders as its ASCII letter.
"""


def main():
    if '--cut' in sys.argv:
        at = sys.argv.index('--cut')
        brief = sys.argv[at + 1] if len(sys.argv) > at + 1 and \
            not sys.argv[at + 1].startswith('--') else BRIEF
        cut(brief)

    sheet, lines, counts, used, source = build()
    os.makedirs(OUT, exist_ok=True)
    write_png(os.path.join(OUT, '32x32.png'), sheet.render())
    with open(os.path.join(OUT, 'graf-82.prf'), 'w', encoding='utf-8') as f:
        f.write(HEADER + '\n'.join(lines) + '\n')
    print('  %d tiles in a %dx%d sheet' % (len(sheet.tiles), COLUMNS * TILE,
                                           ((len(sheet.tiles) + COLUMNS - 1)
                                            // COLUMNS) * TILE))
    for kind in ('terrain', 'monsters', 'objects', 'flavours', 'traps',
                 'effects', 'player'):
        print('  %-9s %d' % (kind, counts[kind]))

    status = 0
    if '--check' in sys.argv:
        unused = sorted(set(source) - used)
        print('\n  source tiles nothing uses: %s' % (', '.join(unused) or 'none'))
        status = 1 if unused else 0
    if '--preview' in sys.argv:
        make_preview(sheet, lines)
    return status


# --- the preview -------------------------------------------------------------

def make_preview(sheet, lines):
    """A mock dungeon room and a town street, at 1x and 3x, built from the
    prf's own lines so it shows what the game will draw."""
    where = {}
    for line in lines:
        if line.startswith(('feat:', 'monster:', 'object:', 'trap:', 'flavor:')):
            text, a, c = line.rsplit(':', 2)
            where.setdefault(text, ((int(a, 16) & 0x7F) * COLUMNS
                                    + (int(c, 16) & 0x7F)))

    def tile(text):
        return sheet.tiles[where[text]]

    def lay(canvas, w, t, gx, gy):
        for y in range(TILE):
            for x in range(TILE):
                p = t[y * TILE + x]
                if p is not None:
                    canvas[gy * TILE + y][gx * TILE + x] = tuple(p) + (255,)

    room = [
        'GGGGGGGGGGGGGGGG',
        'G..............G',
        'G.@..o....D....G',
        'G......!.......G',
        'G..^.....T...>.G',
        'G..............G',
        'GGGGGGG+GGGGGGGG',
    ]
    things = {
        '@': 'monster:<player>', 'o': 'monster:snaga', 'D': 'monster:baby red dragon',
        'T': 'monster:Gerard, Strongman of Amber', '!': 'flavor:194',
        '^': 'trap:fire trap:los', '>': 'feat:MORE:los', '+': 'feat:CLOSED:los',
        'G': 'feat:GRANITE:los', '.': 'feat:FLOOR:los',
    }
    h, w = len(room), len(room[0])
    canvas = [[(0, 0, 0, 255)] * (w * TILE) for _ in range(h * TILE)]
    for gy, row in enumerate(room):
        for gx, ch in enumerate(row):
            ground = 'feat:GRANITE:los' if ch == 'G' else 'feat:FLOOR:los'
            if ch in '>+':
                ground = things[ch]
            lay(canvas, w, tile(ground), gx, gy)
            if ch in things and ch not in 'G.>+':
                key = things[ch]
                if key in where:
                    lay(canvas, w, tile(key), gx, gy)
    for zoom, name in ((1, 'preview-actual-size.png'), (3, 'preview.png')):
        out = [[p for p in row for _ in range(zoom)]
               for row in canvas for _ in range(zoom)]
        write_png(os.path.join(SRC, name), out)
    print('\n  preview: %s' % os.path.join(SRC, 'preview.png'))


if __name__ == '__main__':
    sys.exit(main())
