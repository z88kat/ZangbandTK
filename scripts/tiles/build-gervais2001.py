#!/usr/bin/env python3
"""
Build the Gervais 2001 tileset: David Gervais' art as ZAngbandTk 2.4 drew it,
beside the Gervais set Angband ships.

Two steps, and only the second runs normally:

  * Importing (`--import ARCHIVE`).  The 2001 sheets are GIFs in archive/Tk,
    which is not in the repository.  This decodes the ones the set uses into
    gervais2001/*.png, which are committed and are the set's source art.
    ARCHIVE is the archive/Tk directory.

  * Building (the default).  From those PNGs and lib/tiles/gervais, writes
    lib/tiles/gervais-2001: the sheet, the pref files and edges.txt.

    scripts/tiles/build-gervais2001.py                   build
    scripts/tiles/build-gervais2001.py --check           fail if the output
                                                         is not what a build
                                                         makes
    scripts/tiles/build-gervais2001.py --import ARCHIVE  re-import the GIFs

What it makes
-------------

The sheet is Angband's Gervais sheet, as lib/tiles/gervais has it, with rows
of 2001 art below.  Every monster, object and spell in it is the same picture
in both sets, and Angband's own pref for that sheet was converted from
ZAngbandTk's dg32-assign in the first place, so the new set copies the
Gervais prefs unchanged and only says where the ground, the walls, the doors
and the shops differ.  Mode 3 is not touched.

Each 2001 picture goes in three times, side by side: as drawn, then at 0.75
and 0.5 of its brightness.  That is the game's torch-and-seen, remembered and
unlit, and the same factors Gervais used for the walls and ground that have
them drawn (measured: 0.75 and 0.50, pixel for pixel, on the floor).

edges.txt is for the Tk front end, which draws a square's terrain by looking
at its neighbours: shorelines between grass, sand and water, roads that join
up, and roofs on the town's buildings.  ZAngbandTk never did this while you
played.  Its shoreline and roof pieces were laid by hand in its vault editor,
and none of the shipped towns used them.  Every decision is made here, in the
tables: the front end works out which neighbours count and looks the answer
up, so the rules can be read and tested without the game.

Deterministic.  Needs nothing but Python.
"""

import importlib.util
import os
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
SRC = os.path.join(HERE, 'gervais2001')
BASE = os.path.join(ROOT, 'lib', 'tiles', 'gervais')
OUT = os.path.join(ROOT, 'lib', 'tiles', 'gervais-2001')

TILE = 32
DIM = (1.0, 0.75, 0.5)
SHEET = '32x32.png'
PRF = 'graf-2001.prf'
EDGES = 'edges.txt'
# Copied from lib/tiles/gervais unchanged; graf-2001.prf loads graf-dvg.prf,
# which loads the rest.
COPIED = ('graf-dvg.prf', 'flvr-dvg.prf', 'xtra-dvg.prf', 'graf-ztk.prf',
          'feat-ztk.prf', 'serv-ztk.prf')

# make-terrain.py's PNG writer is reused for the small source sheets.
_spec = importlib.util.spec_from_file_location(
    'make_terrain', os.path.join(HERE, 'make-terrain.py'))
mt = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(mt)


# --- importing ---------------------------------------------------------------

# What is taken from archive/Tk, and from where.  David Gervais' sheets come
# from image/dg, and Gervais' own tile library publishes all of them under
# Creative Commons Attribution 3.0.  The last three carry nobody's name: Tim
# Baker's as far as anything says, taken on the reading LICENSE.md records.
# They are kept as source but nothing in this set draws them yet: the game has
# no Pattern, the veins are the Adam Bolt modes' and the town front is one
# building of a fixed shape.
IMPORTS = (
    ('image/dg/dg_grounds32.gif', 'grounds'),
    ('image/dg/dg_dungeon32.gif', 'dungeon'),
    ('image/dg/dg_features32.gif', 'features'),
    ('image/dg/dg_extra132.gif', 'extra1'),
    ('image/dg/dg_edging132.gif', 'edging1'),
    ('image/dg/dg_edging232.gif', 'edging2'),
    ('image/dg/dg_edging332.gif', 'edging3'),
) + tuple(('image/dg/dg_town%d32.gif' % i, 'town%d' % i) for i in range(10)) + (
    ('image/town32.gif', 'roofs'),
    ('image/feature32.gif', 'veins'),
    ('ZAngbandTk-240r5/ZAngbandTk/tk/image/pattern16.gif', 'pattern16'),
)


def read_gif(path):
    """The first image of a GIF, as rows of RGBA tuples.

    The 2001 sheets are plain GIF87a/89a: one image, a global or local colour
    table, perhaps a transparent index.  Interlacing is handled for
    completeness; none of them uses it.
    """
    data = open(path, 'rb').read()
    assert data[:6] in (b'GIF87a', b'GIF89a'), path + ' is not a GIF'
    width, height, flags = struct.unpack('<HHB', data[6:11])
    at = 13
    table = None
    if flags & 0x80:
        size = 3 << ((flags & 7) + 1)
        table = data[at:at + size]
        at += size
    transparent = None
    while True:
        kind = data[at]
        if kind == 0x21:                        # an extension
            label = data[at + 1]
            at += 2
            while data[at]:
                block = data[at + 1:at + 1 + data[at]]
                if label == 0xF9 and block[0] & 1:
                    transparent = block[3]
                at += 1 + data[at]
            at += 1
        elif kind == 0x2C:                      # the image
            x, y, w, h, iflags = struct.unpack('<HHHHB', data[at + 1:at + 10])
            at += 10
            if iflags & 0x80:
                size = 3 << ((iflags & 7) + 1)
                table = data[at:at + size]
                at += size
            interlaced = bool(iflags & 0x40)
            break
        else:
            raise ValueError('%s: unexpected block 0x%02x' % (path, kind))
    assert (x, y, w, h) == (0, 0, width, height), path + ' has a framed image'
    minimum = data[at]
    at += 1
    stream = bytearray()
    while data[at]:
        stream += data[at + 1:at + 1 + data[at]]
        at += 1 + data[at]
    indices = lzw_decode(bytes(stream), minimum, w * h)
    order = list(range(h))
    if interlaced:
        order = (list(range(0, h, 8)) + list(range(4, h, 8))
                 + list(range(2, h, 4)) + list(range(1, h, 2)))
    rows = [None] * h
    for n, row in enumerate(order):
        line = []
        for i in indices[n * w:(n + 1) * w]:
            r, g, b = table[i * 3:i * 3 + 3]
            line.append((r, g, b, 0 if i == transparent else 255))
        rows[row] = line
    return rows


def lzw_decode(stream, minimum, count):
    clear, end = 1 << minimum, (1 << minimum) + 1
    out = bytearray()
    width = minimum + 1
    table = [bytes([i]) for i in range(clear)] + [b'', b'']
    previous = None
    bits = value = 0
    for byte in stream:
        value |= byte << bits
        bits += 8
        while bits >= width:
            code = value & ((1 << width) - 1)
            value >>= width
            bits -= width
            if code == clear:
                width = minimum + 1
                table = table[:end + 1]
                previous = None
                continue
            if code == end:
                return bytes(out[:count])
            if previous is None:
                entry = table[code]
            elif code < len(table):
                entry = table[code]
                table.append(previous + entry[:1])
            else:
                entry = previous + previous[:1]
                table.append(entry)
            out += entry
            previous = entry
            if len(table) == (1 << width) and width < 12:
                width += 1
    return bytes(out[:count])


def import_archive(archive):
    os.makedirs(SRC, exist_ok=True)
    for rel, name in IMPORTS:
        pixels = read_gif(os.path.join(archive, rel))
        mt.write_png(os.path.join(SRC, name + '.png'), pixels)
        print('%-10s %dx%d  from %s' % (name, len(pixels[0]), len(pixels), rel))


# --- pixels, as bytes --------------------------------------------------------
#
# The Gervais sheet is 4096x992.  As Python tuples that is four million
# objects; as one bytearray it is 16MB and copies a row at a time.

def read_rgba(path):
    """Any 8-bit, non-interlaced PNG, as (width, height, RGBA bytearray)."""
    data = open(path, 'rb').read()
    assert data[:8] == b'\x89PNG\r\n\x1a\n', path + ' is not a PNG'
    pos, idat, plte, trns = 8, b'', None, None
    width = height = depth = colour = interlace = 0
    while pos < len(data):
        length, kind = struct.unpack('>I4s', data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b'IHDR':
            width, height, depth, colour, _, _, interlace = struct.unpack(
                '>IIBBBBB', body)
        elif kind == b'PLTE':
            plte = body
        elif kind == b'tRNS':
            trns = body
        elif kind == b'IDAT':
            idat += body
        elif kind == b'IEND':
            break
    assert depth == 8 and interlace == 0, 'unsupported PNG in ' + path
    ch = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[colour]
    raw = zlib.decompress(idat)
    stride = width * ch
    out = bytearray(width * height * 4)
    prev = bytearray(stride)
    p = 0
    for y in range(height):
        f = raw[p]
        line = bytearray(raw[p + 1:p + 1 + stride])
        p += 1 + stride
        if f == 1:
            for i in range(ch, stride):
                line[i] = (line[i] + line[i - ch]) & 255
        elif f == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 255
        elif f == 3:
            for i in range(stride):
                a = line[i - ch] if i >= ch else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 255
        elif f == 4:
            for i in range(stride):
                a = line[i - ch] if i >= ch else 0
                b = prev[i]
                c = prev[i - ch] if i >= ch else 0
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc
                                      else b if pb <= pc else c)) & 255
        prev = line
        o = y * width * 4
        if colour == 6:
            out[o:o + width * 4] = line
            continue
        for x in range(width):
            i = x * ch
            if colour == 2:
                px = (line[i], line[i + 1], line[i + 2], 255)
            elif colour == 0:
                px = (line[i],) * 3 + (255,)
            elif colour == 4:
                px = (line[i],) * 3 + (line[i + 1],)
            else:
                k = line[i]
                px = tuple(plte[k * 3:k * 3 + 3]) + (
                    trns[k] if trns and k < len(trns) else 255,)
            out[o + x * 4:o + x * 4 + 4] = bytes(px)
    return width, height, out


def write_rgba(path, width, height, pixels):
    raw = bytearray()
    stride = width * 4
    for y in range(height):
        raw.append(0)
        raw += pixels[y * stride:(y + 1) * stride]

    def chunk(kind, body):
        return (struct.pack('>I', len(body)) + kind + body
                + struct.pack('>I', zlib.crc32(kind + body) & 0xffffffff))

    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk(b'IHDR', struct.pack('>IIBBBBB', width, height,
                                           8, 6, 0, 0, 0)))
        f.write(chunk(b'IDAT', zlib.compress(bytes(raw), 9)))
        f.write(chunk(b'IEND', b''))


class Source:
    """One 2001 sheet, cut into 32x32 cells numbered across then down, the
    way ZAngbandTk's configs number them."""

    def __init__(self, name):
        self.name = name
        self.w, self.h, self.px = read_rgba(os.path.join(SRC, name + '.png'))
        self.cols = self.w // TILE

    def cell(self, i):
        r, c = divmod(i, self.cols)
        out = bytearray()
        for y in range(TILE):
            o = ((r * TILE + y) * self.w + c * TILE) * 4
            out += self.px[o:o + TILE * 4]
        return out


def keyed(cell):
    """White is the 2001 sheets' transparent colour."""
    out = bytearray(cell)
    for i in range(0, len(out), 4):
        if out[i] == out[i + 1] == out[i + 2] == 255:
            out[i:i + 4] = b'\0\0\0\0'
    return out


def over(top, under):
    """`top` laid on `under`, source over, for 0-or-255 alpha."""
    out = bytearray(under)
    for i in range(0, len(top), 4):
        if top[i + 3]:
            out[i:i + 4] = top[i:i + 4]
    return out


def halves(first, second, vertical):
    """A cell made of `first`'s far half and `second`'s near half: the two
    sides of a roof meeting at a ridge."""
    out = bytearray(len(first))
    half = TILE // 2
    for y in range(TILE):
        for x in range(TILE):
            if vertical:        # left from first's right half
                src, sx = (first, x + half) if x < half else (second, x - half)
                sy = y
            else:               # top from first's lower half
                src, sy = (first, y + half) if y < half else (second, y - half)
                sx = x
            o, s = (y * TILE + x) * 4, (sy * TILE + sx) * 4
            out[o:o + 4] = src[s:s + 4]
    return out


def dim(cell, k):
    if k == 1.0:
        return bytearray(cell)
    out = bytearray(cell)
    for i in range(0, len(out), 4):
        for j in range(3):
            out[i + j] = int(out[i + j] * k + 0.5)
    return out


def mean(cell, x0, y0, n=3):
    t = [0, 0, 0]
    for y in range(y0, y0 + n):
        for x in range(x0, x0 + n):
            o = (y * TILE + x) * 4
            for j in range(3):
                t[j] += cell[o + j]
    return [v / (n * n) for v in t]


def opaque_in(cell, x0, y0, x1, y1):
    return any(cell[(y * TILE + x) * 4 + 3]
               for y in range(y0, y1) for x in range(x0, x1))


# --- the sheet ----------------------------------------------------------------

class Sheet:
    """Angband's Gervais sheet with 2001 art appended, three cells a picture.

    `put` returns the (attr, char) of the picture as drawn; the remembered and
    unlit copies are the next two chars along.  A picture is placed once
    however many things use it.
    """

    def __init__(self):
        self.w, self.h, self.base = read_rgba(os.path.join(BASE, SHEET))
        assert self.w % TILE == 0 and self.h % TILE == 0
        self.cols = self.w // TILE
        self.first_row = self.h // TILE
        self.cells = []
        self.placed = {}

    def put(self, cell):
        key = bytes(cell)
        if key in self.placed:
            return self.placed[key]
        while len(self.cells) % self.cols > self.cols - len(DIM):
            self.cells.append(bytearray(TILE * TILE * 4))
        n = len(self.cells)
        r, c = divmod(n, self.cols)
        at = (0x80 + self.first_row + r, 0x80 + c)
        assert at[0] <= 0xFF, 'the sheet has outgrown the attr byte'
        for k in DIM:
            self.cells.append(dim(cell, k))
        self.placed[key] = at
        return at

    def pixels(self):
        rows = -(-len(self.cells) // self.cols)
        h = self.h + rows * TILE
        out = bytearray(self.base) + bytearray(rows * TILE * self.w * 4)
        for n, cell in enumerate(self.cells):
            r, c = divmod(n, self.cols)
            for y in range(TILE):
                o = (((self.first_row + r) * TILE + y) * self.w + c * TILE) * 4
                out[o:o + TILE * 4] = cell[y * TILE * 4:(y + 1) * TILE * 4]
        return self.w, h, out


# --- what each feature looks like ---------------------------------------------
#
# ZAngbandTk's own choices, from its dg32-assign and dg32.cfg, where it had
# one; ZangbandTK's features it never had are given the nearest picture in the
# same sheets.  The door, stair and wall style is the one dg32.cfg falls back
# on (brick, round doors); it chose among five at random per level, which a
# pref file cannot.

FEATURES = (
    # code             sheet      cell
    ('FLOOR',          'grounds', 0),
    ('CLOSED',         'dungeon', 3),
    ('OPEN',           'dungeon', 4),
    ('BROKEN',         'dungeon', 5),
    ('LESS',           'dungeon', 15),
    ('MORE',           'dungeon', 16),
    ('SECRET',         'dungeon', 0),      # it must look like the wall
    ('GRANITE',        'dungeon', 0),
    ('PERM',           'dungeon', 0),
    ('MAGMA',          'dungeon', 51),
    ('QUARTZ',         'dungeon', 57),
    ('MAGMA_K',        'dungeon', 54),
    ('QUARTZ_K',       'dungeon', 60),
    ('LAVA',           'grounds', 36),
    ('DIRT',           'grounds', 27),
    ('TREE',           'grounds', 54),
    ('MUD',            'grounds', 21),
    ('ROCK',           'grounds', 117),
    # The shoreline pieces are drawn against these four fills, so the plain
    # ground has to be the same pictures or every shore would show a seam.
    ('GRASS',          'edging1', 121),
    ('SAND',           'edging1', 117),
    ('WATER',          'edging1', 113),
    ('DEEP_WATER',     'edging1', 114),
    ('WORLD_EDGE',     'edging1', 116),
    # Shops, in the white stone of ZAngbandTk's towns.  The first eight are
    # its own choices; the rest are ZangbandTK's, matched by emblem.
    ('STORE_GENERAL',  'town1', 3),        # a torch
    ('STORE_ARMOR',    'town1', 4),        # a shield
    ('STORE_WEAPON',   'town1', 5),        # a sword
    ('STORE_BOOK',     'town1', 9),        # a book
    ('STORE_ALCHEMY',  'town1', 7),        # a potion
    ('STORE_MAGIC',    'town1', 8),        # a ring
    ('STORE_BLACK',    'town1', 27),       # a skull
    ('HOME',           'town1', 23),       # a bed
    ('STORE_STATUE',   'town1', 15),       # a carved arm: nothing closer
    ('STORE_FIGURINE', 'town1', 26),       # a crown: nothing closer
    ('HEALER',         'town1', 6),        # the temple's cross
    ('INN',            'town1', 12),       # a tankard
    ('MAGESMITH',      'town1', 10),       # crossed swords
    ('MAGETOWER',      'town1', 17),       # a grey tower
    ('CHAOSTOWER',     'town1', 19),       # a green tower
    ('RECHARGER',      'town1', 25),       # a moon and star
    ('DUNGEON',        'town1', 29),       # stairs down
)

LIGHTS = (('torch', 0), ('los', 0), ('lit', 1), ('dark', 2))

# Bits of a neighbour mask, as edges.txt and the front end count them.
N, E, S, W, NE, SE, SW, NW = 1, 2, 4, 8, 16, 32, 64, 128

# A shoreline block in edging1, eight cells wide: where each piece sits
# relative to the block's first cell, and which neighbours are the outside
# for it.  The pond ring in the middle is the inside corners: (1,1) has the
# outside at its south-east, and so on.  The crossing pieces (a path cut
# through the bank) are not used.
SHORE_PIECES = (
    ((0, 0), N | W), ((0, 1), N), ((0, 3), N | E),
    ((1, 0), W), ((1, 3), E),
    ((3, 0), S | W), ((3, 1), S), ((3, 3), S | E),
    ((1, 1), SE), ((1, 2), SW), ((2, 1), NE), ((2, 2), NW),
)

# (inside, outside, block's first cell, inside fill, outside fill), in the
# order they are tried: a grass square by both water and sand is a beach.
SHORES = (
    (('GRASS',), ('WATER',), 0, 121, 113),
    (('GRASS',), ('DEEP_WATER', 'WORLD_EDGE'), 4, 121, 114),
    (('GRASS',), ('SAND',), 32, 121, 117),
    (('SAND',), ('WATER', 'DEEP_WATER', 'WORLD_EDGE'), 36, 117, 113),
)

SHOPS = tuple(code for code, sheet, _ in FEATURES if sheet == 'town1'
              and code != 'DUNGEON')

# The dirt road in edging3, drawn on white: cells 30 to 44 are its straights,
# ends, corners, tees and crossing.  Which is which is read off the pictures.
# A road runs into a shop's door, the dungeon's entrance and a town's gate.
ROAD_CELLS = range(30, 45)
ROAD_JOINS = ('ROAD', 'DUNGEON', 'CLOSED', 'OPEN', 'BROKEN') + SHOPS

# The road's surface, without its rims: these columns of the north-south
# straight and these rows of the east-west one.  Measured off the two pieces,
# and checked when the set is built.
ROAD_SURFACE = range(10, 20)

# A road square in quarters: the diagonal bit for each quarter, the two
# neighbours beside it, and where it sits.
QUARTERS = (
    (NW, N, W, 0, 0), (NE, N, E, 16, 0),
    (SW, S, W, 0, 16), (SE, S, E, 16, 16),
)
OPPOSITE = {N: S, S: N, E: W, W: E}

# The red tiled roof in edging2: a 3x3 frame with no middle, as ZAngbandTk's
# Building tool left it.  The middle is made from the sides.
ROOF = {'nw': 27, 'n': 28, 'ne': 29, 'w': 35, 'e': 37,
        'sw': 43, 's': 44, 'se': 45}
BUILDING = ('PERM',) + SHOPS


def shore_piece(mask):
    """The piece for a square whose outside neighbours are `mask`, or None
    where the block has no piece for it (a strip one square wide, a point)."""
    orth = mask & (N | E | S | W)
    if orth == 0:
        diag = mask & (NE | SE | SW | NW)
        return diag if diag in (NE, SE, SW, NW) else None
    if orth in (N, E, S, W, N | W, N | E, S | W, S | E):
        return orth
    return None


def road_quarters(mask):
    """Where each quarter of a road square comes from, given which of its
    eight neighbours are road: a list of (x, y, source), where the source is
    the mask of the path piece to cut it from or 'surface'.

    A quarter is decided by its corner.  With road on neither side it is the
    rounded outside of a turn; on one side, the rim of a straight; on both
    with the diagonal open, the inside of a turn; and on all three, open
    road.  A path one square wide comes out exactly as the 2001 pieces draw
    it, and ZangbandTK's roads, which are two wide, come out as one road with
    a rim on each side instead of two paths side by side."""
    out = []
    for diag, v, h, x, y in QUARTERS:
        if mask & v and mask & h:
            src = 'surface' if mask & diag else v | h
        elif mask & v:
            src = N | S
        elif mask & h:
            src = E | W
        else:
            src = OPPOSITE[v] | OPPOSITE[h]
        out.append((x, y, src))
    return out


def roof_piece(mask, wide):
    """The roof piece for a building square whose building neighbours are
    `mask`, or None.  A square takes a roof only as part of a 2x2 block of
    building, which keeps it off the town's one-square wall."""
    orth = mask & (N | E | S | W)
    if orth == N | E | S | W:
        return 'ridge-ew' if wide else 'ridge-ns'
    edges = {N | E | S: ('w', SE | NE), N | E | W: ('s', NE | NW),
             N | S | W: ('e', SW | NW), E | S | W: ('n', SE | SW)}
    if orth in edges:
        name, diagonals = edges[orth]
        return name if mask & diagonals else None
    corners = {E | S: ('nw', SE), S | W: ('ne', SW),
               N | E: ('sw', NE), N | W: ('se', NW)}
    if orth in corners:
        name, diagonal = corners[orth]
        return name if mask & diagonal else None
    return None


def build():
    """Make everything in memory: (sheet pixels, {file name: text})."""
    src = {}

    def cell(sheet, i):
        if sheet not in src:
            src[sheet] = Source(sheet)
        return src[sheet].cell(i)

    sheet = Sheet()
    tile = {}
    for code, name, i in FEATURES:
        tile[code] = sheet.put(cell(name, i))

    # Shorelines.  Each piece is checked against the fills before it is
    # used: a probe in each corner must be nearer the outside fill exactly
    # where the table says the outside reaches.  Only the corners, because
    # mid-way along an edge the beach's sand band can be either.
    rules = []
    for inside, outside, first, fin, fout in SHORES:
        cin = mean(cell('edging1', fin), 14, 14)
        cout = mean(cell('edging1', fout), 14, 14)
        table = {}
        for (r, c), want in SHORE_PIECES:
            piece = cell('edging1', first + r * 8 + c)
            seen = 0
            for bit, (x, y) in ((NE, (28, 1)), (SE, (28, 28)),
                                (SW, (1, 28)), (NW, (1, 1))):
                m = mean(piece, x, y)
                if (sum((a - b) ** 2 for a, b in zip(m, cout))
                        < sum((a - b) ** 2 for a, b in zip(m, cin))):
                    seen |= bit
            # A side or corner piece's outside reaches the corners it
            # touches; an inside corner reaches only its own.
            covers = want & (NE | SE | SW | NW)
            for bit, (a, b) in ((NE, (N, E)), (SE, (S, E)), (SW, (S, W)),
                                (NW, (N, W))):
                if want & (a | b):
                    covers |= bit
            assert seen == covers, ('edging1 cell %d: outside at %s, expected'
                                    ' %s' % (first + r * 8 + c, seen, covers))
            table[want] = sheet.put(piece)
        tiles = {}
        for mask in range(256):
            piece = shore_piece(mask)
            if piece is not None:
                tiles[mask] = table[piece]
        rules.append(('shore', 'any', 8, False, inside, outside, tiles))

    # Roads, laid on the grass fill, in quarters (see road_quarters).  A
    # piece joins the neighbours whose edge its road reaches.
    grass = cell('edging1', 121)
    path = {}
    for i in ROAD_CELLS:
        piece = keyed(cell('edging3', i))
        mask = 0
        for bit, box in ((N, (12, 0, 20, 2)), (E, (30, 12, 32, 20)),
                         (S, (12, 30, 20, 32)), (W, (0, 12, 2, 20))):
            if opaque_in(piece, *box):
                mask |= bit
        path.setdefault(mask, piece)
    for mask in (N | S, E | W, N | E, E | S, S | W, N | W):
        assert mask in path, 'edging3 has no road piece joining %d' % mask

    # Open road, for the quarters with road all round: a straight's surface
    # repeated, running the way the square's rims run, or where it has none,
    # the way the road runs further through it (the ridge bit).
    def surface(vertical):
        src = path[N | S] if vertical else path[E | W]
        out = bytearray(TILE * TILE * 4)
        span = len(ROAD_SURFACE)
        for y in range(TILE):
            for x in range(TILE):
                if vertical:
                    sx, sy = ROAD_SURFACE[0] + x % span, y
                else:
                    sx, sy = x, ROAD_SURFACE[0] + y % span
                s = (sy * TILE + sx) * 4
                assert src[s + 3] and src[s] > 150, ('road surface at %d,%d'
                                                     ' is rim' % (sx, sy))
                out[(y * TILE + x) * 4:(y * TILE + x) * 4 + 4] = src[s:s + 4]
        return out
    open_road = {True: surface(True), False: surface(False)}

    tiles = {}
    for mask in range(512):
        quarters = road_quarters(mask & 255)
        froms = [frm for _, _, frm in quarters]
        vertical = (N | S in froms or (E | W not in froms and not mask & 256))
        out = bytearray(grass)
        for x0, y0, frm in quarters:
            top = open_road[vertical] if frm == 'surface' else path[frm]
            for y in range(y0, y0 + 16):
                for x in range(x0, x0 + 16):
                    o = (y * TILE + x) * 4
                    if top[o + 3]:
                        out[o:o + 4] = top[o:o + 4]
        tiles[mask] = sheet.put(out)
    tile['ROAD'] = sheet.put(open_road[True])
    rules.append(('road', 'any', 8, True, ('ROAD',), ROAD_JOINS, tiles))

    # Roofs, on the town wall they hide.
    wall = cell('town1', 0)
    roof = {k: over(keyed(cell('edging2', i)), wall) for k, i in ROOF.items()}
    roof['ridge-ew'] = halves(roof['n'], roof['s'], vertical=False)
    roof['ridge-ns'] = halves(roof['w'], roof['e'], vertical=True)
    tiles = {}
    for mask in range(512):
        piece = roof_piece(mask & 255, bool(mask & 256))
        if piece is not None:
            tiles[mask] = sheet.put(roof[piece])
    rules.append(('roof', 'surface', 8, True, ('PERM',), BUILDING, tiles))

    # Out of doors the permanent wall is the town's white stone, where it is
    # not under a roof; underground a tree stands on the floor, not on grass.
    rules.append(('swap', 'surface', 0, False, ('PERM',), (),
                  {0: sheet.put(wall)}))
    rules.append(('swap', 'dungeon', 0, False, ('TREE',), (),
                  {0: sheet.put(cell('dungeon', 24))}))

    texts = {PRF: prf_text(tile), EDGES: edges_text(rules)}
    for name in COPIED:
        with open(os.path.join(BASE, name), encoding='utf-8') as f:
            texts[name] = f.read()
    return sheet.pixels(), texts


def prf_text(tile):
    out = [
        '# File: graf-2001.prf',
        '#',
        '# The Gervais 2001 tileset: Angband\'s Gervais prefs, with the',
        '# ground, walls, doors and shops from David Gervais\' 2001 sheets',
        '# for ZAngbandTk.  Made by scripts/tiles/build-gervais2001.py with',
        '# 32x32.png and edges.txt beside it.  Do not edit any of them by',
        '# hand: change the script and run it again.',
        '#',
        '# The 2001 art is CC BY 3.0, David Gervais; see lib/tiles/README.',
        '',
        '# Everything Angband\'s Gervais set already says',
        '%:graf-dvg.prf',
        '',
        '# What the 2001 sheets say instead',
    ]
    for code, _, _ in FEATURES + (('ROAD', None, None),):
        a, c = tile[code]
        for light, k in LIGHTS:
            out.append('feat:%s:%s:0x%02X:0x%02X' % (code, light, a, c + k))
    return '\n'.join(out) + '\n'


def edges_text(rules):
    out = [
        '# File: edges.txt',
        '#',
        '# Terrain drawn from its neighbours, for the Tk front end.  Made by',
        '# scripts/tiles/build-gervais2001.py; do not edit by hand.',
        '#',
        '# rule:<kind>:<where>:<neighbours>:<ridge>',
        '#   where is any, surface (depth 0) or dungeon; neighbours is 0, 4',
        '#   or 8; ridge 1 adds 256 to the mask when the near squares run',
        '#   at least as far east-west as north-south through the square.',
        '# grid:<feature>...  the squares the rule redraws',
        '# near:<feature>...  the neighbours that count',
        '# tile:<mask>:<attr>:<char>',
        '#   the picture as seen; remembered and unlit are the next two',
        '#   chars.  Mask bits: N 1, E 2, S 4, W 8, NE 16, SE 32, SW 64,',
        '#   NW 128.  A square counts only what the character remembers, and',
        '#   a mask with no tile leaves the square as its pref draws it.',
        '#   The first rule with a tile for its mask wins.',
    ]
    for kind, where, around, ridge, grid, near, tiles in rules:
        out.append('')
        out.append('rule:%s:%s:%d:%d' % (kind, where, around, int(ridge)))
        out.append('grid:' + ':'.join(grid))
        if near:
            out.append('near:' + ':'.join(near))
        for mask in sorted(tiles):
            a, c = tiles[mask]
            out.append('tile:%d:0x%02X:0x%02X' % (mask, a, c))
    return '\n'.join(out) + '\n'


def write(pixels, texts):
    os.makedirs(OUT, exist_ok=True)
    w, h, px = pixels
    write_rgba(os.path.join(OUT, SHEET), w, h, px)
    for name, text in texts.items():
        with open(os.path.join(OUT, name), 'w', encoding='utf-8') as f:
            f.write(text)


def check(pixels, texts):
    """Compare pixels rather than files: zlib's output is not promised to be
    the same from one version to the next, and the picture is what matters."""
    bad = []
    w, h, px = pixels
    path = os.path.join(OUT, SHEET)
    if not os.path.exists(path) or read_rgba(path) != (w, h, px):
        bad.append(SHEET)
    for name, text in texts.items():
        path = os.path.join(OUT, name)
        if (not os.path.exists(path)
                or open(path, encoding='utf-8').read() != text):
            bad.append(name)
    for name in bad:
        print('lib/tiles/gervais-2001/%s is not what the script makes' % name,
              file=sys.stderr)
    return not bad


def main():
    args = sys.argv[1:]
    if args[:1] == ['--import']:
        import_archive(args[1] if len(args) > 1
                       else os.path.join(ROOT, 'archive', 'Tk'))
        return 0
    pixels, texts = build()
    if args[:1] == ['--check']:
        return 0 if check(pixels, texts) else 1
    write(pixels, texts)
    w, h, _ = pixels
    print('lib/tiles/gervais-2001: %dx%d sheet, %d files' % (w, h,
                                                              len(texts) + 1))
    return 0


if __name__ == '__main__':
    sys.exit(main())
