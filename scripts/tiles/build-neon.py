#!/usr/bin/env python3
"""
Build the Neon tileset: neon signs at 32x32, drawn by an AI image generator to
a brief, and coloured here with each thing's own colour.

A tile's colour is data, as it was in the hand-typed 16x16 silhouettes this
set replaced in November 2026.  Every picture was drawn as a white neon sign
on black, in four greys, and the greys take the thing's own colour from
lib/gamedata, through palette.txt's hues and tones.  One drawing of a young
dragon is every young dragon, each the colour monster.txt says.  A picture
may also carry up to three fixed colours from seven -- fire, gold, leaves, a
cloak -- which the game colour never touches.

The art arrived as 34 sheets of sixteen pictures.  Turning them into tiles is
two steps, and only the second runs normally:

  * Cutting (`--cut`).  Each accepted picture (recipe.txt) is reduced to 32x32:
    every 16x16 block of the sheet becomes one pixel, its brightness the
    block's 75th percentile (the mean dims a thin tube to glow, the maximum
    fills a figure in) and its colour the average of the brightest quarter.
    The pixel then snaps to black, a grey level from one to four, or one of the
    seven fixed colours at one of four tones.  A bright grey ringed by one
    colour becomes that colour's pale core, since every neon tube has a white
    heart.  A picture drawn too dim is lifted a grey level.  The result is
    source.png, the set's source art; the sheets stay in the design brief.

  * Building (the default).  From source.png alone: the walls, floors, bolts
    and the other plain things are drawn here, every picture is tinted with
    each thing's colour, terrain gets its four lights, liquids are quieted,
    the shops are stamped with their numbers, and graf-neo.prf says where
    everything is.  The colour-blind sheet is the same layout in palette.txt's
    cbhue colours.

    scripts/tiles/build-neon.py               build the sheets and the prf
    scripts/tiles/build-neon.py --check       ...and check them
    scripts/tiles/build-neon.py --preview     ...and draw a mock map
    scripts/tiles/build-neon.py --cut [BRIEF] cut source.png from the sheets

The shimmer still works.  The engine's graf_cycle_attr() moves a tile up and
down the sheet in blocks of forty rows, ten hues by four tones, so each
picture a shimmering monster uses is laid out down one column in all ten hues
at its tone, and each glinting treasure in all four tones of its hue.
Everything else is stored once, in whatever slot is free.

Deterministic: re-running produces the same sheets and prf.  Building needs
nothing but Python; cutting also needs ImageMagick, to read the sheets.
"""

import collections
import colorsys
import math
import multiprocessing
import os
import re
import struct
import subprocess
import sys
import tempfile
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
DATA = os.path.join(ROOT, 'lib', 'gamedata')
SRC = os.path.join(HERE, 'neon')
OUT = os.path.join(ROOT, 'lib', 'tiles', 'neon')
BRIEF = os.path.join(ROOT, 'archive', 'design_brief_tileset_neon')
SHEET, CB_SHEET, PRF = '32x32.png', '32x32-cb.png', 'graf-neo.prf'

TILE = 32
COLUMNS = 32
ROWS = 128          # all a prf line can name: 0x80 | row
MONSTERS = ('monster.txt', 'monster.zangband.txt', 'monster.zangbandtk.txt')
OBJECTS = ('object.txt', 'object.zangband.txt')

TONES = ('pale', 'neon', 'dim', 'shadow')  # brightest first: a light is a step
GREYS = (0x33, 0x66, 0xAA, 0xFF)    # grey levels 1 to 4 in the source atlas
# The seven fixed colours a picture may carry.  Teal and purple are left out:
# too near cyan and blue for the generator to keep apart.  They still reach
# the screen through the tint.
ACCENTS = ('red', 'orange', 'yellow', 'green', 'cyan', 'blue', 'magenta')


# --- the readers ---------------------------------------------------------------
#
# build-1982.py and neon-colourblind.py load this file for these, so their
# names and what they return stay as they are: read_palette() is palette.txt
# as written, in hex, and load_palette() below is this set's view of it.
# read_png/write_png are duplicated in make-terrain.py; one shared module would
# be better, and is a change to make with that script in front of you.

# --- PNG, without a dependency -----------------------------------------------

def read_png(path):
    """Only the flavour of PNG write_png emits: RGBA, depth 8, filter 0."""
    data = open(path, 'rb').read()
    assert data[:8] == b'\x89PNG\r\n\x1a\n', path + ' is not a PNG'
    idat, width, height = b'', 0, 0
    at = 8
    while at < len(data):
        length = struct.unpack('>I', data[at:at + 4])[0]
        kind = data[at + 4:at + 8]
        body = data[at + 8:at + 8 + length]
        if kind == b'IHDR':
            width, height, depth, colour, _, _, interlace = \
                struct.unpack('>IIBBBBB', body)
            assert depth == 8 and colour == 6 and interlace == 0, 'unsupported'
        elif kind == b'IDAT':
            idat += body
        at += 12 + length
    raw = zlib.decompress(idat)
    stride = width * 4
    rows = []
    for y in range(height):
        start = y * (stride + 1)
        assert raw[start] == 0, 'unsupported PNG filter'
        line = raw[start + 1:start + 1 + stride]
        rows.append([tuple(line[x * 4:x * 4 + 4]) for x in range(width)])
    return rows


def write_png(path, pixels):
    height, width = len(pixels), len(pixels[0])
    raw = bytearray()
    for row in pixels:
        raw.append(0)
        for p in row:
            raw += bytes(p)

    def chunk(kind, body):
        return (struct.pack('>I', len(body)) + kind + body
                + struct.pack('>I', zlib.crc32(kind + body) & 0xffffffff))

    out = b'\x89PNG\r\n\x1a\n'
    out += chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
    out += chunk(b'IDAT', zlib.compress(bytes(raw), 9))
    out += chunk(b'IEND', b'')
    open(path, 'wb').write(out)


# --- colour ------------------------------------------------------------------

def rgb(h):
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def _lin(c):
    c /= 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def luminance(h):
    r, g, b = rgb(h)
    return 0.2126 * _lin(r) + 0.7152 * _lin(g) + 0.0722 * _lin(b)


def contrast_on_black(h):
    """A tile is one colour on black, so its legibility *is* this number."""
    return (luminance(h) + 0.05) / 0.05






# --- palette.txt and the data files --------------------------------------------

def read_palette():
    """hues in file order, the game-colour map, and the lighting map."""
    hues, attrs, lighting, trap_lighting = {}, {}, {}, {}
    for line in open(os.path.join(SRC, 'palette.txt'), encoding='utf-8'):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        bits = line.split(':')
        if bits[0] == 'hue':
            hues[bits[1]] = dict(zip(TONES, bits[2:2 + len(TONES)]))
        elif bits[0] == 'attr':
            attrs[bits[1]] = (bits[2], bits[3])
        elif bits[0] == 'lighting':
            lighting[bits[1]] = int(bits[2])
        elif bits[0] == 'trap-lighting':
            trap_lighting[bits[1]] = int(bits[2])
    return hues, attrs, lighting, trap_lighting


def read_cb_hues():
    """palette.txt's cbhue lines: the same hues, recoloured for colour blindness."""
    cb = {}
    for line in open(os.path.join(SRC, 'palette.txt'), encoding='utf-8'):
        bits = line.strip().split(':')
        if bits[0] == 'cbhue':
            cb[bits[1]] = dict(zip(TONES, bits[2:2 + len(TONES)]))
    return cb


def read_cycle_declaration():
    """list.txt's `cycle:` for this set: hues, tones and the rainbow's span."""
    path = os.path.join(ROOT, 'lib', 'tiles', 'list.txt')
    for line in open(path, encoding='utf-8'):
        if line.startswith('cycle:'):
            bits = [int(v) for v in line.strip().split(':')[1:4]]
            return tuple(bits)
    return (0, 0, 0)






def read_records(filename, fields, start='name'):
    """Records from a gamedata file, in file order.

    `start` is the field that begins a record, and it is not always `name:`:
    terrain.txt puts `code:` first, and a reader that assumes otherwise hangs
    every feature's code on the feature before it -- quietly, and with a
    plausible-looking result.
    """
    out, current = [], None
    path = os.path.join(DATA, filename)
    for line in open(path, encoding='utf-8', errors='replace'):
        line = line.rstrip('\n')
        if line.startswith(start + ':'):
            current = {start: line[len(start) + 1:].strip()}
            out.append(current)
        elif current is not None:
            for field in fields:
                if line.startswith(field + ':'):
                    current[field] = line[len(field) + 1:].strip()
    return out


def object_name(fmt):
    """object.txt's `name:` as lookup_sval() will compare it.

    lookup_sval() runs each kind's name through obj_desc_name_format() before
    comparing, so `& Small wooden chest~` is looked up as "Small wooden
    chest".  Emitting the raw form instead produces a preference file that
    parses cleanly and resolves nothing -- there is no error for an sval that
    does not exist, so 157 object lines simply do nothing.  Ported from
    obj_desc_name_format() in src/obj-desc.c with pluralise false and no
    modstr, which is how lookup_sval() calls it.
    """
    out, at = [], 0
    while at < len(fmt):
        ch = fmt[at]
        if ch == '&':
            while at < len(fmt) and fmt[at] in ' &':
                at += 1
            continue
        if ch == '~':                       # singular: drop it
            at += 1
            continue
        if ch == '|':                       # kni|fe|ves| -> knife
            rest = fmt[at + 1:]
            first = rest.find('|')
            second = rest.find('|', first + 1) if first >= 0 else -1
            if first < 0 or second < 0:
                break
            out.append(rest[:first])
            at = at + 1 + second + 1
            continue
        out.append(ch)
        at += 1
    return ''.join(out)


def read_flavours():
    """(index, colour code) for every flavour, plus its tval."""
    names = colour_names()
    out, tval = [], None
    index = 0
    for line in open(os.path.join(DATA, 'flavor.txt'), encoding='utf-8'):
        line = line.strip()
        if line.startswith('kind:'):
            tval = line.split(':')[1]
        elif line.startswith('flavor:') or line.startswith('fixed:'):
            bits = line.split(':')
            index = int(bits[1])
            attr = bits[3] if line.startswith('fixed:') else bits[2]
            code = names.get(_fold(attr))
            if code:
                out.append((index, code, tval))
    return out


def _fold(name):
    """Normalise a colour name for lookup.

    flavor.txt says "Light Green" and projection.txt says "Light dark",
    "ORANGE" and "L_DARK" for the same three ideas.  Folding rather than
    matching exactly is what stops a data file's capitalisation from quietly
    costing a tile -- which it did: one potion flavour was missing until
    `T` (Light Teal) was mapped, and these spellings would have cost more.
    """
    name = name.strip().upper().replace('_', ' ')
    if name.startswith('L '):
        name = 'LIGHT ' + name[2:]
    return ' '.join(name.split())


def colour_names():
    """"Light Green" -> 'G', read from the engine's own table."""
    text = open(os.path.join(ROOT, 'src', 'z-color.c'), encoding='utf-8').read()
    return {_fold(name): code
            for code, name in re.findall(r"\{'(.)',\s*\"([^\"]+)\"", text)}


# --- the palette -------------------------------------------------------------

def load_palette():
    """The hues as RGB by tone index, both sets of them, the game colours as
    (hue, tone index), and the two lighting tables."""
    hues, attrs, lighting, trap_lighting = read_palette()

    def table(h):
        return collections.OrderedDict(
            (name, [rgb(v[t]) for t in TONES]) for name, v in h.items())
    attrs = {code: (hue, TONES.index(tone)) for code, (hue, tone) in attrs.items()}
    return table(hues), table(read_cb_hues()), attrs, lighting, trap_lighting


MAIN, CB, ATTRS, LIGHTING, TRAP_LIGHTING = load_palette()
HUE_ORDER = list(MAIN)              # the cycle order, grey last
NAMES = colour_names()


def game_colour(spec):
    """A gamedata colour, letter or name, as its code; None if neither."""
    spec = spec.strip()
    if spec in ATTRS:
        return spec
    code = NAMES.get(_fold(spec))
    return code if code in ATTRS else None


# --- tiles -------------------------------------------------------------------
#
# A tile is a tuple of 1024 pixels, row by row.  A pixel is None (black, or
# transparent in a figure), a grey level 1-4 (the part the game colours), or
# (hue, tone index) for a fixed colour.

def clamp(t):
    return max(0, min(3, t))


def lift(t, steps):
    """Every grey `steps` levels brighter: for a picture drawn too dim."""
    return tuple(min(4, p + steps) if isinstance(p, int) else p for p in t)


def quiet(t, steps):
    """Every grey and colour `steps` darker, what falls off the bottom going
    black: liquids, which at full brightness drown what stands in them."""
    out = []
    for p in t:
        if isinstance(p, int):
            out.append(p - steps if p - steps >= 1 else None)
        elif p is not None:
            out.append((p[0], p[1] + steps) if p[1] + steps <= 3 else None)
        else:
            out.append(None)
    return tuple(out)


def shift_colours(t, steps):
    """The fixed colours moved by the light, as the tint is."""
    if not steps:
        return t
    return tuple((p[0], clamp(p[1] + steps)) if isinstance(p, tuple) else p
                 for p in t)


def paint(layers, pal, opaque):
    """RGBA pixels of a finished tile: each layer is (tile, hue, tone), its
    greys tinted so that level 3 lands on the tone and each level up or down
    is a tone brighter or darker, drawn bottom first."""
    out = [None] * (TILE * TILE)
    for t, hue, tone in layers:
        ramp = pal[hue]
        for n, p in enumerate(t):
            if p is None:
                continue
            if isinstance(p, int):
                out[n] = ramp[clamp(tone + 3 - p)]
            else:
                out[n] = pal[p[0]][p[1]]
    blank = (0, 0, 0, 255) if opaque else (0, 0, 0, 0)
    return [blank if c is None else c + (255,) for c in out]


# --- the source atlas --------------------------------------------------------

def encode(p):
    if p is None:
        return (0, 0, 0, 0)
    if isinstance(p, int):
        return (GREYS[p - 1],) * 3 + (255,)
    return MAIN[p[0]][p[1]] + (255,)


DECODE = {(g, g, g): n + 1 for n, g in enumerate(GREYS)}
DECODE.update({MAIN[h][t]: (h, t) for h in ACCENTS for t in range(4)})
assert len(DECODE) == 4 + 7 * 4, 'two of the source colours are the same'


def read_source():
    """id -> tile, from source.png and source.txt."""
    rows = read_png(os.path.join(SRC, 'source.png'))
    out = {}
    for line in open(os.path.join(SRC, 'source.txt'), encoding='utf-8'):
        if not line.strip() or line.startswith('#'):
            continue
        n, ident = line.split()[:2]
        n = int(n)
        ox, oy = (n % COLUMNS) * TILE, (n // COLUMNS) * TILE
        t = []
        for y in range(TILE):
            for x in range(TILE):
                r, g, b, a = rows[oy + y][ox + x]
                if not a:
                    t.append(None)
                    continue
                assert (r, g, b) in DECODE, (
                    '%s: %02X%02X%02X at %d,%d is not a source colour (has '
                    'palette.txt changed?  Re-cut.)' % (ident, r, g, b, x, y))
                t.append(DECODE[(r, g, b)])
        out[ident] = tuple(t)
    return out


def write_source(tiles):
    """tiles: [(id, tile)] -> source.png and source.txt."""
    rows_n = (len(tiles) + COLUMNS - 1) // COLUMNS
    px = [[(0, 0, 0, 0)] * (COLUMNS * TILE) for _ in range(rows_n * TILE)]
    with open(os.path.join(SRC, 'source.txt'), 'w', encoding='utf-8') as f:
        f.write('# source.txt -- what is where in source.png: the index of each\n'
                '# 32x32 tile and its id in the brief.\n'
                '# GENERATED by build-neon.py --cut.  Do not edit.\n')
        for n, (ident, t) in enumerate(tiles):
            f.write('%d %s\n' % (n, ident))
            ox, oy = (n % COLUMNS) * TILE, (n // COLUMNS) * TILE
            for k, p in enumerate(t):
                px[oy + k // TILE][ox + k % TILE] = encode(p)
    write_png(os.path.join(SRC, 'source.png'), px)


# --- cutting -----------------------------------------------------------------

def _angle(c):
    return colorsys.rgb_to_hsv(*(x / 255 for x in c))[0] * 360


ANGLE = {h: _angle(MAIN[h][1]) for h in ACCENTS}


def _d2(a, b):
    return sum((x - y) ** 2 for x, y in zip(a, b))


def classify(c):
    """One averaged block as a source pixel."""
    mx, mn = max(c), min(c)
    if mx < 0x1A:
        return None
    if (mx - mn) / mx < 0.30:
        level = min(range(5), key=lambda i: abs(((0,) + GREYS)[i] - mx))
        return level or None
    h = _angle(c)
    hue = min(ACCENTS, key=lambda n: min(abs(h - ANGLE[n]), 360 - abs(h - ANGLE[n])))
    tone = min(range(4), key=lambda t: _d2(c, MAIN[hue][t]))
    # Nearer black than the nearest tone of its hue: the glow round a tube.
    if sum(v * v for v in c) < _d2(c, MAIN[hue][tone]):
        return None
    return (hue, tone)


def fix_cores(t):
    """A bright grey ringed by one colour is that colour's pale core."""
    out = list(t)
    for k, p in enumerate(t):
        if not isinstance(p, int) or p < 3:
            continue
        y, x = divmod(k, TILE)
        ring, grey = collections.Counter(), 0
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                if (dx or dy) and 0 <= y + dy < TILE and 0 <= x + dx < TILE:
                    q = t[(y + dy) * TILE + x + dx]
                    if isinstance(q, tuple):
                        ring[q[0]] += 1
                    elif isinstance(q, int) and q >= 2:
                        grey += 1
        if ring:
            hue, count = ring.most_common(1)[0]
            if count >= 3 and count > grey:
                out[k] = (hue, 0)
    return tuple(out)


class SheetImage:
    def __init__(self, path, cache):
        ppm = os.path.join(cache, os.path.basename(os.path.dirname(path)) + '.ppm')
        subprocess.run(['magick', path, '-depth', '8', ppm], check=True)
        head = open(ppm, 'rb').read().split(b'\n', 3)
        self.w = int(head[1].split()[0])
        self.px = head[3]

    def cut(self, cell, q=0.75):
        """Cell `cell` (1-16) as a tile; `q` is the percentile that sets each
        pixel's brightness."""
        size = self.w // 4
        block = size // TILE
        x0, y0 = (cell - 1) % 4 * size, (cell - 1) // 4 * size
        t = []
        for j in range(TILE):
            for i in range(TILE):
                vals = []
                for y in range(y0 + j * block, y0 + (j + 1) * block):
                    at = (y * self.w + x0 + i * block) * 3
                    row = self.px[at:at + block * 3]
                    vals.extend(zip(row[0::3], row[1::3], row[2::3]))
                vals.sort(key=max)
                k = int(len(vals) * q)
                top = vals[k:]
                avg = [sum(v[c] for v in top) / len(top) for c in range(3)]
                m = max(avg) or 1
                t.append(classify(tuple(min(255, a * max(vals[k]) / m)
                                        for a in avg)))
        return fix_cores(t)


def parse_recipe():
    """id -> (sheet, cell, {'cut': q, 'lift': n}), in file order."""
    out = collections.OrderedDict()
    for line in open(os.path.join(SRC, 'recipe.txt'), encoding='utf-8'):
        words = line.split('#')[0].split()
        if not words:
            continue
        ident, sheet, cell = words[0], words[1], int(words[2])
        steps = {}
        for w in words[3:]:
            key, value = w.split(':')
            assert key in ('cut', 'lift'), '%s: unknown step %r' % (ident, w)
            steps[key] = float(value) if key == 'cut' else int(value)
        out.pop(ident, None)
        out[ident] = (sheet, cell, steps)
    return out


def _cut_sheet(job):
    brief, sheet, cells = job
    with tempfile.TemporaryDirectory() as cache:
        img = SheetImage(os.path.join(brief, sheet, 'sheet.jpeg'), cache)
        return [(ident, lift(img.cut(cell, steps.get('cut', 0.75)),
                             steps.get('lift', 0)))
                for ident, cell, steps in cells]


def cut_tiles(brief, recipe):
    """id -> tile for every recipe line, the sheets cut in parallel."""
    jobs = collections.OrderedDict()
    for ident, (sheet, cell, steps) in recipe.items():
        jobs.setdefault(sheet, []).append((ident, cell, steps))
    out = {}
    # Forked, so that the workers have this module however it was loaded.
    with multiprocessing.get_context('fork').Pool() as pool:
        for done in pool.imap(_cut_sheet, [(brief, s, c) for s, c in jobs.items()]):
            out.update(done)
    return out


def cut(brief):
    recipe = parse_recipe()
    tiles = cut_tiles(brief, recipe)
    write_source([(ident, tiles[ident]) for ident in recipe])
    print('  %d tiles -> %s' % (len(tiles), os.path.join(SRC, 'source.png')))


# --- what is drawn here ------------------------------------------------------
#
# The plain things the generator could not draw evenly: a wall's courses came
# back with seams, the floor as an uneven grid, every bolt with a burst on its
# end.  Each is a few lines here, in the same four greys, and tiles without a
# seam because it repeats on a 32-pixel torus.

def blank():
    return [None] * (TILE * TILE)


def put(t, x, y, v):
    t[(y % TILE) * TILE + x % TILE] = v


def doubled(rows):
    """A 16x16 pattern, each pixel a 2x2 block: the 16x16 set's shapes at
    this size."""
    t = blank()
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            if ch != '.':
                for dy in (0, 1):
                    for dx in (0, 1):
                        put(t, 2 * x + dx, 2 * y + dy, int(ch))
    return t


def courses(w, h, mortar, edge, fill, stagger):
    """Blocks w by h with `mortar` pixels between them, each outlined in grey
    `edge` and filled with `fill`."""
    t = blank()
    for y in range(TILE):
        row, yy = divmod(y, h)
        off = stagger * (row % 2)
        for x in range(TILE):
            xx = (x + off) % w
            if yy >= h - mortar or xx >= w - mortar:
                continue
            rim = yy in (0, h - mortar - 1) or xx in (0, w - mortar - 1)
            put(t, x, y, edge if rim else fill)
    return t


def stones(seeds, metric, gap, edge, fill, round_to=None):
    """Irregular blocks: each pixel belongs to its nearest seed on the torus,
    with mortar where two seeds are nearly as near, and outside `round_to`
    of the seed if given."""
    def far(a, b):
        d = abs(a - b)
        return min(d, TILE - d)
    owner = []
    for y in range(TILE):
        for x in range(TILE):
            ds = sorted((metric(far(x, sx), far(y, sy)), n)
                        for n, (sx, sy) in enumerate(seeds))
            inside = ds[1][0] - ds[0][0] >= gap and (
                round_to is None or ds[0][0] <= round_to)
            owner.append(ds[0][1] if inside else None)
    t = blank()
    for k, o in enumerate(owner):
        if o is None:
            continue
        y, x = divmod(k, TILE)
        rim = any(owner[((y + dy) % TILE) * TILE + (x + dx) % TILE] != o
                  for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
        t[k] = edge if rim else fill
    return t


def gems(t, spots, level=4):
    t = list(t)
    for x, y in spots:
        for dx in (0, 1):
            for dy in (0, 1):
                put(t, x + dx, y + dy, level)
    return t


def line(t, x0, y0, x1, y1, v):
    n = max(abs(x1 - x0), abs(y1 - y0)) or 1
    for s in range(n + 1):
        put(t, round(x0 + (x1 - x0) * s / n), round(y0 + (y1 - y0) * s / n), v)


def beam(distance):
    """A bolt: a straight beam in the four greys, the core brightest, the
    same thickness edge to edge so that a row of them is one line."""
    t = blank()
    for y in range(TILE):
        for x in range(TILE):
            d = distance(x + .5, y + .5)
            if d < 4:
                t[y * TILE + x] = 4 - int(d)
    return t


# The 16x16 set's floor and grass, doubled: the grit and tufts the owner knows.
FLOOR16 = [
    '................', '....3.......3...', '................', '.........3......',
    '..3.............', '................', '.......3....3...', '................',
    '....3...........', '.............3..', '................', '..3.......3.....',
    '................', '........3.......', '....3...........', '................',
]
GRASS16 = [
    '................', '................', '.......3........', '......333.......',
    '.......3........', '................', '................', '..3.........3...',
    '.333.......333..', '..3.........3...', '................', '................',
    '.........3......', '........333.....', '.........3......', '................',
]
DIRT16 = [
    '................', '..2.....3.......', '...........2....', '.....3........3.',
    '.2..............', '.........2......', '...3.........2..', '................',
    '.......2...3....', '..3.............', '..........2.....', '.....2.......3..',
    '.3..............', '.........3......', '....2.......2...', '...........3....',
]

MAGMA_SEEDS = ((5, 6), (18, 3), (28, 10), (10, 17), (23, 20), (4, 27), (16, 28))
QUARTZ_SEEDS = ((8, 4), (24, 8), (14, 16), (30, 22), (4, 21), (20, 28))
ROAD_SEEDS = ((4, 4), (14, 2), (24, 5), (9, 12), (20, 13), (29, 15), (3, 20),
              (14, 22), (24, 23), (8, 29), (19, 30), (29, 28))


def euclid(dx, dy):
    return math.hypot(dx, dy)


def manhattan(dx, dy):
    return dx + dy


def draw_code():
    """id -> tile for everything drawn here rather than cut."""
    out = {}
    out['feat-floor'] = doubled(FLOOR16)
    out['feat-grass'] = doubled(GRASS16)
    out['feat-dirt'] = doubled(DIRT16)
    # Walls: bricks; the permanent wall riveted plates; the veins rounded
    # stones (magma) and faceted crystal (quartz), and with treasure in them
    # the same with bright specks.
    out['feat-granite'] = courses(16, 8, 2, 3, 2, 8)
    out['feat-perm'] = gems(courses(16, 16, 2, 3, 2, 0),
                            [(6, 6), (22, 6), (6, 22), (22, 22)])
    magma = stones(MAGMA_SEEDS, euclid, 1.6, 3, 2, round_to=8.5)
    quartz = stones(QUARTZ_SEEDS, manhattan, 2, 3, 2)
    out['feat-magma'] = magma
    out['feat-quartz'] = quartz
    out['feat-magma-k'] = gems(magma, [(9, 7), (24, 18), (14, 26)])
    out['feat-quartz-k'] = gems(quartz, [(7, 3), (13, 15), (22, 27)])
    # The mountainside: a map-maker's mountain, two peaks, the far one first.
    # Each is outlined, its sunlit face filled and its shadowed face left
    # black, with snow on the top.
    rock = blank()
    for apex, half, base in (((23, 10), 9, 29), ((12, 3), 12, 29)):
        ax, ay = apex
        for y in range(ay, base + 1):
            w = half * (y - ay) / (base - ay)
            for x in range(TILE):
                d = x - ax
                if abs(d) > w + .5:
                    continue
                edge = abs(d) > w - 1.5 or y >= base - 1
                if edge:
                    v = 4 if y - ay < 5 else 3
                elif d < 0:
                    v = 4 if y - ay < 5 else 2
                else:
                    v = None
                put(rock, x, y, v)
    out['feat-rock'] = rock
    # Sand: ripples.  Mud: flat puddles.  Road: flagstones, outlined only.
    sand = blank()
    for x0, y0 in ((3, 5), (18, 2), (10, 13), (24, 17), (2, 23), (16, 27)):
        for x in range(9):
            y = y0 + (0 if 2 <= x <= 6 else 1)
            put(sand, x0 + x, y, 3)
            put(sand, x0 + x, y + 1, 2)
    out['feat-sand'] = sand
    mud = blank()
    for cx, cy, rx, ry in ((8, 7, 6, 2.2), (23, 14, 7, 2.4), (10, 23, 6, 2.2),
                           (26, 28, 4, 1.6)):
        for y in range(TILE):
            for x in range(TILE):
                d = ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2
                if d <= 1:
                    put(mud, x, y, 3 if d > .45 else 1)
    out['feat-mud'] = mud
    # The road: cobbles, outlined only, so it stays quieter than a wall.
    out['feat-road'] = stones(ROAD_SEEDS, euclid, 1.6, 3, None, round_to=5.5)
    # The bolts and the burst.
    c = TILE / 2
    out['fx-bolt-horizontal'] = beam(lambda x, y: abs(y - c))
    out['fx-bolt-vertical'] = beam(lambda x, y: abs(x - c))
    out['fx-bolt-rising'] = beam(lambda x, y: abs(x + y - TILE) / math.sqrt(2))
    out['fx-bolt-falling'] = beam(lambda x, y: abs(x - y) / math.sqrt(2))
    burst = blank()
    for y in range(TILE):
        for x in range(TILE):
            dx, dy = x + .5 - c, y + .5 - c
            r = math.hypot(dx, dy)
            a = (math.degrees(math.atan2(dy, dx)) + 360) % 45
            ray = min(a, 45 - a) * r * math.pi / 180     # off the nearest ray, px
            if r < 4:
                burst[y * TILE + x] = 4
            elif r < 7:
                burst[y * TILE + x] = 3
            elif r < 14 and ray < 1.2:
                burst[y * TILE + x] = 3 if r < 10 else 2
            elif 9 <= r < 10.5 and ray >= 3:
                burst[y * TILE + x] = 1
    out['fx-burst'] = burst
    return {k: tuple(v) for k, v in out.items()}


# The shop numbers and the services' letters, 3x5, as the 1982 set has them.
GLYPHS = {
    '1': '010110010010111', '2': '111001111100111', '3': '111001111001111',
    '4': '101101111001001', '5': '111100111001111', '6': '111100111101111',
    '7': '111001010010010', '8': '111101111101111', '9': '111101111001111',
    '0': '111101101101111', 'H': '101101111101101', 'I': '111010010010111',
    'M': '101111111101101', 'R': '110101110101101', 'C': '111100100100111',
}


def stamp(t, glyph, x0, y0):
    """The glyph in the brightest grey on a black plate, so it takes the
    building's own colour.  x0, y0 is its top left corner; the plate is a
    pixel wider all round."""
    t = list(t)
    for y in range(y0 - 1, y0 + 6):
        for x in range(x0 - 1, x0 + 4):
            t[y * TILE + x] = None
    for n, bit in enumerate(GLYPHS[glyph]):
        if bit == '1':
            t[(y0 + n // 3) * TILE + x0 + n % 3] = 4
    return tuple(t)


# --- the maps ----------------------------------------------------------------

def read_manifest():
    """feat: code -> (tile, ground, glyph, tone, quiet); the rest as 1982's."""
    m = {'feat': {}, 'tval': {}, 'object': {}, 'trap': {}, 'gf': {},
         'player': {}}
    for line in open(os.path.join(SRC, 'manifest.txt'), encoding='utf-8'):
        line = line.split('#')[0].strip()
        if not line:
            continue
        kind, rest = line.split(':', 1)
        if kind == 'feat':
            bits = rest.split(':')
            # A shop's number goes on the plain roof, right of the sign.
            entry = {'tile': bits[1], 'ground': None, 'glyph': None,
                     'tone': None, 'quiet': 0, 'at': (21, 3)}
            for b in bits[2:]:
                if b in ('floor', 'grass'):
                    entry['ground'] = b
                elif b in GLYPHS:
                    entry['glyph'] = b
                elif b.startswith('tone='):
                    entry['tone'] = TONES.index(b[5:])
                elif b.startswith('quiet='):
                    entry['quiet'] = int(b[6:])
                elif b.startswith('at='):
                    entry['at'] = tuple(int(v) for v in b[3:].split(','))
                else:
                    raise ValueError('feat:%s: what is %r?' % (bits[0], b))
            m['feat'][bits[0]] = entry
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


def read_flags(filename):
    """name -> every flag on every `flags:` line of the record."""
    out, name = {}, None
    for line in open(os.path.join(DATA, filename), encoding='utf-8', errors='replace'):
        if line.startswith('name:'):
            name = line[5:].strip()
            out[name] = set()
        elif line.startswith('flags:') and name is not None:
            out[name].update(f.strip() for f in line[6:].split('|'))
    return out


SHIMMERS = ('ATTR_MULTI', 'ATTR_FLICKER', 'ATTR_RAND')


# --- the sheet ---------------------------------------------------------------

class Sheet:
    """Slots of 32 columns by up to 128 rows.  A finished tile is
    (layers, opaque), and is stored once however many things use it.

    A picture that shimmers needs more than one slot: graf_cycle_attr() walks
    a tile through the hues by moving it down the sheet a tone-group at a
    time, inside blocks of forty rows, so the picture must be at the same
    column in every hue of its block.  graf_glint_attr() does the same through
    the tones of one hue.  Those are placed first; everything else fills the
    slots left."""

    def __init__(self):
        self.slots = {}             # (row, col) -> tile
        self.home = {}              # tile -> the first (row, col) holding it
        self.wants = []             # tiles placed anywhere, in order asked
        self.groups = collections.OrderedDict()   # key -> (rows, tiles)
        self.spot = {}              # group key -> (block, column)
        self.block = len(HUE_ORDER) * len(TONES)

    def want(self, tile):
        self.wants.append(tile)
        return ('tile', tile)

    def _group(self, rows, tiles, index):
        key = (tuple(rows), tuple(tiles))
        self.groups.setdefault(key, (rows, tiles))
        return ('group', key, index)

    def want_cycle(self, picture, hue, tone):
        """A shimmering monster: the picture at `tone` in every hue."""
        rows = [h * len(TONES) + tone for h in range(len(HUE_ORDER))]
        tiles = [(((picture, h, tone),), False) for h in HUE_ORDER]
        return self._group(rows, tiles, HUE_ORDER.index(hue))

    def want_glint(self, picture, hue, tone):
        """Glinting treasure: the picture in every tone of its hue."""
        h = HUE_ORDER.index(hue)
        rows = [h * len(TONES) + t for t in range(len(TONES))]
        tiles = [(((picture, hue, t),), False) for t in range(len(TONES))]
        return self._group(rows, tiles, tone)

    def _put(self, rc, tile):
        self.slots[rc] = tile
        self.home.setdefault(tile, rc)

    def place(self):
        for key, (rows, tiles) in self.groups.items():
            for b in range(ROWS // self.block):
                col = next((c for c in range(COLUMNS) if all(
                    self.slots.get((b * self.block + r, c), t) == t
                    for r, t in zip(rows, tiles))), None)
                if col is not None:
                    break
            else:
                raise RuntimeError('no room left for a shimmering picture')
            self.spot[key] = (b, col)
            for r, t in zip(rows, tiles):
                self._put((b * self.block + r, col), t)
        free = ((r, c) for r in range(ROWS) for c in range(COLUMNS))
        for tile in self.wants:
            if tile in self.home:
                continue
            rc = next(free, None)
            while rc is not None and rc in self.slots:
                rc = next(free, None)
            assert rc is not None, 'the sheet is full: 128 rows of 32'
            self._put(rc, tile)

    def where(self, handle):
        """The (row, column) a line points at.  A shimmering picture's line
        points into its own group, whatever else holds the same tile, since
        the engine walks the column from there."""
        if handle[0] == 'tile':
            return self.home[handle[1]]
        _, key, index = handle
        b, col = self.spot[key]
        return b * self.block + key[0][index], col

    def rows_used(self):
        return max(r for r, _ in self.slots) + 1

    def render(self, pal):
        h = self.rows_used() * TILE
        px = [[(0, 0, 0, 0)] * (COLUMNS * TILE) for _ in range(h)]
        for (r, c), (layers, opaque) in self.slots.items():
            for n, p in enumerate(paint(layers, pal, opaque)):
                px[r * TILE + n // TILE][c * TILE + n % TILE] = p
        return px


# --- building ----------------------------------------------------------------

LIGHTS = ('torch', 'los', 'lit', 'dark')


def build():
    source = read_source()
    code = draw_code()
    pictures = dict(source)
    pictures.update(code)
    m = read_manifest()
    mon_names, mon_bases = read_monster_map()
    sheet = Sheet()
    lines, counts, used = [], collections.Counter(), set()
    used.update(mon_bases.values())
    notes = collections.defaultdict(list)
    moving = []                 # (how, line, handle, picture, hue, tone)
    # The palette slot each kind of thing sits on in its own colour, for the
    # colour-blind check: a creature must be visible, lit terrain a little
    # less so, and terrain remembered in the dark is meant to be dim.
    slots = {}

    def uses(hue, tone, kind):
        slots.setdefault((hue, tone), set()).add(kind)

    def picture(ident):
        used.add(ident)
        return pictures[ident]

    def emit(text, handle):
        lines.append((text, handle))

    def figure(ident, colour):
        hue, tone = ATTRS[colour]
        return sheet.want((((picture(ident), hue, tone),), False))

    # Terrain.  Each feature's own colour; a feature that would sit at the
    # top of the ramp under ambient light sits a tone down, as in the 16x16
    # set, so that a torch has somewhere brighter to take it.  Four lines
    # each, one per light, and the light moves the fixed colours too.
    feats = {r['code']: r for r in read_records('terrain.txt', ('name', 'graphics'),
                                                 start='code')}

    def base(code_):
        entry = m['feat'][code_]
        colour = game_colour(feats[code_]['graphics'].split(':')[-1])
        hue, tone = ATTRS[colour]
        if entry['tone'] is not None:
            tone = entry['tone']
        elif tone == 0:
            tone = 1
        return hue, tone

    lines.append(('\n##### Terrain #####\n', None))
    for code_, record in feats.items():
        entry = m['feat'].get(code_)
        if not entry or 'graphics' not in record:
            continue
        if entry['tile'] == 'black':
            for how in LIGHTS:
                emit('feat:%s:%s' % (code_, how), sheet.want(((), True)))
                counts['terrain'] += 1
            continue
        hue, tone = base(code_)
        t = quiet(picture(entry['tile']), entry['quiet'])
        if entry['glyph']:
            t = stamp(t, entry['glyph'], *entry['at'])
        ground = None
        if entry['ground']:
            g = {'floor': 'FLOOR', 'grass': 'GRASS'}[entry['ground']]
            ground = (picture(m['feat'][g]['tile']),) + base(g)
        for how in LIGHTS:
            s = LIGHTING[how]
            uses(hue, clamp(tone + s), 'unlit' if how == 'dark' else 'terrain')
            layers = ((shift_colours(t, s), hue, clamp(tone + s)),)
            if ground:
                layers = ((ground[0], ground[1], clamp(ground[2] + s)),) + layers
            emit('feat:%s:%s' % (code_, how), sheet.want((layers, True)))
            counts['terrain'] += 1

    # Monsters.  Each one's own colour, from its own record; the ones that
    # shimmer are laid out to.
    lines.append(('\n##### Monsters #####\n', None))
    for filename in MONSTERS:
        flags = read_flags(filename)
        for record in read_records(filename, ('base', 'color')):
            name = record['name']
            ident = mon_names.get(name.lower()) or mon_bases.get(
                record.get('base', '').lower())
            colour = game_colour(record.get('color', 'w'))
            if not ident or not colour:
                continue
            hue, tone = ATTRS[colour]
            uses(hue, tone, 'monster')
            t = picture(ident)
            if flags.get(name, set()) & set(SHIMMERS):
                handle = sheet.want_cycle(t, hue, tone)
                moving.append(('cycle', name, handle, t, hue, tone))
                counts['shimmer'] += 1
            else:
                handle = sheet.want((((t, hue, tone),), False))
            emit('monster:%s' % name, handle)
            counts['monsters'] += 1
            own = sum(1 for p in t if isinstance(p, tuple) and p[0] == hue)
            if own:
                notes['colour'].append('%s (%s): %d px of its own %s'
                                       % (name, colour, own, hue))

    # Objects, flavoured ones excepted: before it is identified a potion is
    # its flavour, so those come from flavor.txt below.
    lines.append(('\n##### Objects #####\n', None))
    flavoured = {t for _, _, t in read_flavours()}

    def object_tile(tval, name):
        tile = m['object'].get((tval, name.lower()), m['tval'].get(tval))
        return None if tile in (None, '-') else tile

    for filename in OBJECTS:
        flags = read_flags(filename)
        for record in read_records(filename, ('type', 'graphics')):
            tval = record.get('type')
            if tval in flavoured:
                continue
            name = object_name(record['name'])
            ident = object_tile(tval, name)
            colour = game_colour(record.get('graphics', ':w').split(':')[-1])
            if not ident or not colour:
                continue
            uses(*ATTRS[colour], 'object')
            if 'SHIMMER' in flags.get(record['name'], set()):
                hue, tone = ATTRS[colour]
                handle = sheet.want_glint(picture(ident), hue, tone)
                moving.append(('glint', name, handle, picture(ident), hue, tone))
                counts['glint'] += 1
            else:
                handle = figure(ident, colour)
            emit('object:%s:%s' % (tval, name), handle)
            counts['objects'] += 1

    # The books are made from class.txt, coloured by their object base.
    base_colour = {r['name'].split(':')[0]: r.get('graphics', 'w')
                   for r in read_records('object_base.txt', ('graphics',))}
    for line in open(os.path.join(DATA, 'class.txt'), encoding='utf-8'):
        if not line.startswith('book:'):
            continue
        _, tval, _, name = line.strip().split(':')[:4]
        ident = object_tile(tval, name)
        colour = game_colour(base_colour.get(tval, 'w'))
        if ident and colour:
            uses(*ATTRS[colour], 'object')
            emit('object:%s:%s' % (tval, name), figure(ident, colour))
            counts['objects'] += 1

    lines.append(('\n##### Flavours #####\n', None))
    for index, colour, tval in read_flavours():
        ident = m['tval'].get(tval)
        if ident and ident != '-' and colour in ATTRS:
            uses(*ATTRS[colour], 'flavour')
            emit('flavor:%d' % index, figure(ident, colour))
            counts['flavours'] += 1

    # Traps.  Keyed on trap.txt's code, so the four dart traps are one
    # drawing.  A trap is a thing to spot, so only the remembered one dims.
    lines.append(('\n##### Traps #####\n', None))
    for record in read_records('trap.txt', ('graphics',)):
        if ':' not in record['name']:
            continue
        code_, specific = record['name'].split(':', 1)
        ident = m['trap'].get(code_)
        colour = game_colour(record.get('graphics', ':w').split(':')[-1])
        if not ident or not colour:
            continue
        hue, tone = ATTRS[colour]
        t = picture(ident)
        for how in LIGHTS:
            s = TRAP_LIGHTING[how]
            uses(hue, clamp(tone + s), 'unlit' if how == 'dark' else 'trap')
            emit('trap:%s:%s' % (specific, how), sheet.want(
                (((shift_colours(t, s), hue, clamp(tone + s)),), False)))
        counts['traps'] += 1

    # Spell effects: five drawings, each projection's colour.
    lines.append(('\n##### Spell effects #####\n', None))
    for record in read_records('projection.txt', ('color',), start='code'):
        colour = game_colour(record.get('color', 'White'))
        if not colour:
            continue
        uses(*ATTRS[colour], 'effect')
        for direction, ident in m['gf'].items():
            emit('GF:%s:%s' % (record['code'], direction), figure(ident, colour))
            counts['effects'] += 1

    # The player, in white: a hero and a heroine for every class.
    lines.append(('\n##### The player #####\n', None))
    emit('monster:<player>', figure(m['player']['default'], 'w'))
    for record in read_records('class.txt', ()):
        cls = record['name']
        for sex, suffix in (('Male', ''), ('Female', '-f')):
            ident = 'player-%s%s' % (cls.lower(), suffix)
            if ident not in pictures:
                continue
            lines.append(('?:[AND [EQU $CLASS %s] [EQU $SEX %s] ]' % (cls, sex), None))
            emit('monster:<player>', figure(ident, 'w'))
            counts['player'] += 1
    lines.append(('?:1', None))

    sheet.place()
    text = []
    for words, handle in lines:
        if handle is None:
            text.append(words)
        else:
            r, c = sheet.where(handle)
            text.append('%s:0x%02X:0x%02X' % (words, 0x80 | r, 0x80 | c))
    unused = sorted(set(pictures) - used)
    notes['moving'] = [(how, name, sheet.where(handle), t, hue, tone)
                       for how, name, handle, t, hue, tone in moving]
    notes['slots'] = slots
    return sheet, text, counts, unused, notes


# graf_cycle_attr() and graf_glint_attr() (src/grafmode.c), on a row: what
# the engine will do to a shimmering tile, for --check to follow.
CYCLE_SPAN = read_cycle_declaration()[2]
GLINT_TONES = 3


def cycle_row(row, step):
    block, slot = divmod(row, len(HUE_ORDER) * len(TONES))
    hue, tone = divmod(slot, len(TONES))
    if hue >= CYCLE_SPAN:
        return row
    return block * len(HUE_ORDER) * len(TONES) + (hue + step) % CYCLE_SPAN * len(TONES) + tone


def glint_row(row, step):
    block, slot = divmod(row, len(HUE_ORDER) * len(TONES))
    hue, tone = divmod(slot, len(TONES))
    if tone >= GLINT_TONES:
        return row
    return block * len(HUE_ORDER) * len(TONES) + hue * len(TONES) + (tone + step) % GLINT_TONES


def check_moving(sheet, moving):
    """Walk every shimmering line as the engine will and say where it lands
    on anything but its own picture, in the colour it should be."""
    bad = []
    for how, name, (row, col), t, hue, tone in moving:
        h = HUE_ORDER.index(hue)
        for step in range(CYCLE_SPAN if how == 'cycle' else GLINT_TONES):
            if how == 'cycle':
                r = cycle_row(row, step)
                want = (HUE_ORDER[(h + step) % CYCLE_SPAN] if h < CYCLE_SPAN
                        else hue, tone)
            else:
                r = glint_row(row, step)
                want = (hue, (tone + step) % GLINT_TONES if tone < GLINT_TONES
                        else tone)
            got = sheet.slots.get((r, col))
            if not got or got[0][0][0] != t or got[0][0][1:] != want:
                bad.append('%s %s, step %d: row %d' % (how, name, step, r))
    return bad


def check_colourblind(slots, plain, cb_px):
    """The colour-blind sheet: as legible as Neon's, its meanings kept, and
    the same picture.

    How much it saves is for neon-colourblind.py to say.  This is what must
    hold whatever the numbers: every slot something sits on in its own colour
    over the contrast floor for what it draws (3:1 for a figure, 2.5:1 for lit
    terrain), the pairs the game gives a meaning to apart under every
    simulated colour blindness, and the sheet the same picture as Neon's with
    only the colours changed.  Returns what failed, after printing.
    """
    import importlib.util
    hues, attrs = read_palette()[:2]
    cb = read_cb_hues()
    failed = []
    print('\n  --- the colour-blind sheet ---')
    if list(cb) != list(hues):
        failed.append('the cbhue lines must name the hues, in hue order')

    for (h, t), kinds in sorted(slots.items()):
        if kinds & {'monster', 'object', 'flavour', 'trap', 'effect'}:
            floor = 3.0
        elif 'terrain' in kinds:
            floor = 2.5
        else:
            continue
        hexs = cb[h][TONES[t]]
        if contrast_on_black(hexs) < floor:
            failed.append('too dark: %s %s #%s at %.1f:1, under %.1f'
                          % (h, TONES[t], hexs, contrast_on_black(hexs), floor))
    if not failed:
        print("    every slot clears its floor, as Neon's own do")

    spec = importlib.util.spec_from_file_location(
        'check_palettes', os.path.join(ROOT, 'scripts', 'palettes',
                                       'check-palettes.py'))
    cp = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(cp)
    meaning = [('G', 'y'), ('y', 'r'), ('G', 'r'), ('G', 'R'), ('g', 'r'),
               ('g', 'R'), ('y', 'o'), ('o', 'R'), ('o', 'r'), ('R', 'r'),
               ('G', 'o')]

    def colour(table, code):
        h, t = attrs[code]
        return '#' + table[h][t]

    worst = min((cp.difference(colour(cb, a), colour(cb, b), v), v or 'normal',
                 a, b)
                for v in [None] + list(cp.CVD) for a, b in meaning)
    own = min(cp.difference(colour(hues, a), colour(hues, b), v)
              for v in cp.CVD for a, b in meaning)
    print("    meaning pairs: closest %.1f (%s, %s/%s); Neon's own %.1f"
          % (worst[0], worst[1], worst[2], worst[3], own))
    if worst[0] < 12:
        failed.append('a pair with a meaning is under 12 for %s' % worst[1])

    mapping = {MAIN[h][t] + (255,): CB[h][t] + (255,)
               for h in MAIN for t in range(len(TONES))}
    if len(plain) != len(cb_px) or len(plain[0]) != len(cb_px[0]):
        failed.append('the colour-blind sheet is not the same size')
    else:
        wrong = sum(1 for a, b in zip(plain, cb_px) for p, q in zip(a, b)
                    if mapping.get(p, p) != q)
        if wrong:
            failed.append("%d pixels differ from Neon's by more than colour"
                          % wrong)
        else:
            print("    the same picture as Neon's, in the cbhue colours")
    for f in failed:
        print('    FAILED  ' + f)
    return failed


HEADER = """\
# graf-neo.prf -- the Neon tileset.
#
# GENERATED by scripts/tiles/build-neon.py, together with 32x32.png and
# 32x32-cb.png beside it.  Do not edit any of them by hand: re-running the
# script overwrites all three.  The art is scripts/tiles/neon/source.png;
# which picture each thing gets is that directory's manifest.txt and
# monsters.txt, and its colour is the thing's own, from lib/gamedata.
#
# Anything absent has no tile and renders as its ASCII letter.
"""


def main():
    if '--cut' in sys.argv:
        at = sys.argv.index('--cut')
        brief = sys.argv[at + 1] if len(sys.argv) > at + 1 and \
            not sys.argv[at + 1].startswith('--') else BRIEF
        cut(brief)

    sheet, lines, counts, unused, notes = build()
    os.makedirs(OUT, exist_ok=True)
    plain, cb_px = sheet.render(MAIN), sheet.render(CB)
    write_png(os.path.join(OUT, SHEET), plain)
    write_png(os.path.join(OUT, CB_SHEET), cb_px)
    with open(os.path.join(OUT, PRF), 'w', encoding='utf-8') as f:
        f.write(HEADER + '\n'.join(lines) + '\n')
    print('  %d tiles in a %dx%d sheet' % (len(sheet.slots), COLUMNS * TILE,
                                           sheet.rows_used() * TILE))
    for kind in ('terrain', 'monsters', 'shimmer', 'objects', 'glint',
                 'flavours', 'traps', 'effects', 'player'):
        print('  %-9s %d' % (kind, counts[kind]))

    status = 0
    if '--check' in sys.argv:
        print('\n  pictures nothing uses: %s' % (', '.join(unused) or 'none'))
        print('  monsters carrying a fixed colour in their own hue, which '
              'vanishes into the tint: %d' % len(notes['colour']))
        for n in notes['colour']:
            print('    %s' % n)
        bad = check_moving(sheet, notes['moving'])
        print('  shimmering lines that land on the wrong tile: %d of %d'
              % (len(bad), len(notes['moving'])))
        for n in bad[:10]:
            print('    %s' % n)
        failed = check_colourblind(notes['slots'], plain, cb_px)
        status = 1 if unused or bad or failed else 0
    if '--preview' in sys.argv:
        make_preview(sheet, lines, notes['moving'])
    return status


# --- the preview -------------------------------------------------------------

PREVIEW_MAP = [
    'GGGGGGGGGGGGGGGGGGGGGGGG',
    'G......................G',
    'G.@..o....D....<....%..G',
    'G......!.....=.........G',
    'G..^.....T...>...~~~~..G',
    'G.....$.........~~W~~..G',
    'G..-----*.......~~~~...G',
    'G.........LLL..........G',
    'GGGGGGG+GGGG\'GGGGPPPPGG',
    '#####mmmMMMMqqqQQQQ#####',
]
PREVIEW_TOWN = [
    'ssssssssssssssssssssssss',
    'ssssssssssss,,,sssssssss',
    ',,,,,,,,,,,,,,,,,,,,,,,,',
    ',,1,,2,,3,,4,,5,,6,,7,,8',
    ',,,,,,,,,,,,,,,,,,,,,,,,',
    'rrrrrrrrrrrrrrrrrrrrrrrr',
    ',,0,,9,,H,,I,,M,,R,,C,,>',
    ',t,,t,,,,,,::::,,,,^^^^,',
    ',,t,,,,,,,,;;;;,,,,^^^^,',
]
PREVIEW_THINGS = {
    '@': 'monster:<player>', 'o': 'monster:Snaga', 'D': 'monster:Baby red dragon',
    'T': 'monster:Gerard, Strongman of Amber', '!': 'flavor:194',
    '=': 'flavor:1', '$': 'object:gold:gold', '^': 'trap:fire trap:los',
    '-': 'GF:FIRE:90', '*': 'GF:FIRE:static', 'W': 'monster:Water spirit',
}
PREVIEW_GROUND = {
    'G': 'GRANITE', '.': 'FLOOR', '<': 'LESS', '>': 'MORE', '+': 'CLOSED',
    "'": 'OPEN', '~': 'WATER', 'L': 'LAVA', 'P': 'PERM', 'm': 'MAGMA',
    'M': 'MAGMA_K', 'q': 'QUARTZ', 'Q': 'QUARTZ_K', '#': 'GRANITE',
    'W': 'WATER', '%': 'RUBBLE',
}
TOWN_GROUND = {
    's': 'WORLD_EDGE', ',': 'GRASS', 'r': 'ROAD', 't': 'TREE', ':': 'DIRT',
    ';': 'SAND', '^': 'ROCK', '>': 'DUNGEON', '1': 'STORE_GENERAL',
    '2': 'STORE_ARMOR', '3': 'STORE_WEAPON', '4': 'STORE_BOOK',
    '5': 'STORE_ALCHEMY', '6': 'STORE_MAGIC', '7': 'STORE_BLACK', '8': 'HOME',
    '0': 'STORE_STATUE', '9': 'MAGETOWER', 'H': 'HEALER', 'I': 'INN',
    'M': 'MAGESMITH', 'R': 'RECHARGER', 'C': 'CHAOSTOWER',
}


PREVIEW_SHIMMER = ('chaos hound', 'gold')


def make_preview(sheet, lines, moving):
    """A dungeon room, the town, and a shimmering hound and gold walked the
    way the engine walks them, at 1x and 3x, in both palettes, built from the
    prf's own lines so it shows what the game will draw."""
    where = {}
    for line in lines:
        if ':0x' in line and not line.startswith('?'):
            text, a, c = line.rsplit(':', 2)
            where.setdefault(text.lower(), (int(a, 16) & 0x7F, int(c, 16) & 0x7F))
    for pal, suffix in ((MAIN, ''), (CB, '-colourblind')):
        cache = {}

        def tile(text):
            rc = where.get(text.lower())
            if rc is None:
                print('  preview: no line for %s' % text)
                return None
            if rc not in cache:
                layers, opaque = sheet.slots[rc]
                cache[rc] = paint(layers, pal, opaque)
            return cache[rc]

        rows = PREVIEW_MAP + [''] + PREVIEW_TOWN + ['', '']
        w = max(len(r) for r in rows)
        canvas = [[(0, 0, 0, 255)] * (w * TILE) for _ in range(len(rows) * TILE)]

        def lay(t, gx, gy):
            if t is None:
                return
            for n, p in enumerate(t):
                if p[3]:
                    canvas[gy * TILE + n // TILE][gx * TILE + n % TILE] = p

        for gy, row in enumerate(rows):
            town = gy > len(PREVIEW_MAP)
            for gx, ch in enumerate(row):
                grounds = TOWN_GROUND if town else PREVIEW_GROUND
                light = 'torch' if (not town and abs(gx - 2) + abs(gy - 2) <= 2) else 'los'
                if ch in grounds:
                    lay(tile('feat:%s:%s' % (grounds[ch], light)), gx, gy)
                else:
                    lay(tile('feat:%s:%s' % ('GRASS' if town else 'FLOOR', light)), gx, gy)
                if ch in PREVIEW_THINGS and not town:
                    lay(tile(PREVIEW_THINGS[ch]), gx, gy)
        # The shimmer, a frame to a square: the hound through its nine hues,
        # then the gold through its three tones.
        gx, gy = 0, len(rows) - 1
        floor = tile('feat:FLOOR:los')
        for how, name, (row, col), t, hue, tone in moving:
            if name.lower() not in [n.lower() for n in PREVIEW_SHIMMER]:
                continue
            for step in range(CYCLE_SPAN if how == 'cycle' else GLINT_TONES):
                r = cycle_row(row, step) if how == 'cycle' else glint_row(row, step)
                layers, opaque = sheet.slots[(r, col)]
                lay(floor, gx, gy)
                lay(paint(layers, pal, opaque), gx, gy)
                gx += 1
            gx += 1
        for zoom, name in ((1, 'preview-actual-size%s.png'), (3, 'preview%s.png')):
            out = [[p for p in row for _ in range(zoom)]
                   for row in canvas for _ in range(zoom)]
            write_png(os.path.join(SRC, name % suffix), out)
    print('\n  preview: %s' % os.path.join(SRC, 'preview.png'))


if __name__ == '__main__':
    sys.exit(main())
