#!/usr/bin/env python3
"""
Draw the isometric terrain set -- PROTOTYPE.

Pen and ink on paper, after the 1978 Dungeons & Dragons books: hatching for
rock and for shadow, masonry ruled in on the faces of walls, and the ten-foot
grid on the floor the way the module maps had it.  Unexplored ground is the
paper itself, so the dungeon is drawn in as it is explored.

Every tile is drawn at one size, 64x80: a 64x32 floor diamond at the bottom
with 48 pixels above it for walls, trees and peaks to stand in.  Walls are
blocks a uniform 24 pixels high.  A wall's outline is left off wherever it
meets another wall, so a run of wall reads as one mass rather than a row of
cubes: that is sixteen drawings per material, one for each combination of
walls to the west, north, east and south.

=== Inks and washes ===

The drawing is made in named inks and coloured only when it is written.  Four
are in every sheet -- clear, paper, ink, and a light ink for the grid -- and
the rest are washes: wood, stone, roof tiles, water, grass and so on.  Each
wash says what it is in a sheet without colour, paper or ink, and what colour
it is in one with.  So one drawing gives three sheets:

    iso-ink.png      black on cream, as the rulebooks were printed
    iso-blue.png     TSR's blue on white, as the module maps were
    iso-colour.png   the blue map coloured in by hand, its rock dark stone

All three are offered, and the player chooses.

The shops' doors take the game's own colour for each shop, read from
lib/gamedata/terrain.txt and src/z-color.c, so the Alchemist's door is the
blue its 5 is on the flat map.

=== Patterns across squares ===

Neighbouring tiles sit 32 pixels apart across and 16 down, so anything drawn by
position -- hatching, the roofs' ribs, the road's dot screen, the waves -- has
a period dividing 16, or it breaks at every join.

    scripts/tiles/make-iso.py OUTDIR          the sheets, the letters and the map

Deterministic: the only randomness is make-terrain.py's fixed generator,
seeded per tile.
"""

import importlib.util
import math
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))

_spec = importlib.util.spec_from_file_location(
    'make_terrain', os.path.join(HERE, 'make-terrain.py'))
mt = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(mt)

TW, TH = 64, 80          # one tile
CX, CY = 32, 64          # the centre of its floor diamond
HW, HH = 32, 16          # half the diamond's width and height
WALL_H = 24              # how high a wall stands
CUT_H = 6                # a wall cut away so the character can be seen
COLS = 16                # tiles per row of the sheet

# --- inks -------------------------------------------------------------------

T, P, I, G = 0, 1, 2, 3
WASHES = []              # (name, plain, colour, strength), coded from 4 up


def wash(name, plain, colour, strength):
    """
    A wash: printed as `plain` (paper or ink) in a sheet without colour, and in
    one with as `colour` laid over the paper at `strength`, 0 to 1.  A colour
    of None is the ink itself, in every sheet.
    """
    WASHES.append((name, plain, colour, strength))
    return 3 + len(WASHES)


# The top of solid rock: the ink of the map, and in colour dark stone.
ROCK = wash('rock', I, (82, 80, 78), 1)
# The faces of walls.
STONE = wash('stone', P, (178, 160, 132), .8)
DRESSED = wash('dressed stone', P, (146, 152, 174), .8)
RUST = wash('magma', P, (172, 90, 58), .8)
QUARTZ = wash('quartz', P, (206, 192, 234), .85)
GOLD_P = wash('gold on paper', P, (232, 176, 32), .95)
GOLD_I = wash('gold on ink', I, (232, 176, 32), .95)
# Doors.
WOOD = wash('wood', P, (178, 116, 60), .65)
# Buildings on the surface: plaster walls under a tiled roof.
ROOF = wash('roof', I, (172, 68, 48), .9)
RIB = wash('roof rib', P, (216, 132, 98), .85)
PLASTER = wash('plaster', P, (236, 210, 156), .8)
# Ground.
GRASS = wash('grass', P, (146, 196, 104), .35)
LEAF = wash('leaf', P, (88, 156, 66), .7)
BARK = wash('bark', P, (126, 86, 52), .8)
WATER = wash('water', P, (118, 170, 226), .45)
DEEP = wash('deep water', P, (70, 122, 204), .6)
SEA = wash('sea', I, (36, 76, 156), .9)
FOAM = wash('foam', P, (112, 154, 218), .6)
CRUST = wash('lava crust', I, (92, 28, 18), .95)
GLOW = wash('lava glow', P, (255, 146, 36), .9)
SAND = wash('sand', P, (236, 212, 138), .5)
EARTH = wash('earth', P, (194, 160, 116), .4)
MUD = wash('mud', P, (138, 116, 82), .5)
ROAD = wash('road', P, (198, 192, 180), .5)
PEAK = wash('peak', P, (164, 146, 124), .55)
BOULDER = wash('boulder', P, (168, 158, 148), .6)
STEP = wash('step', P, (196, 190, 180), .5)
MOUND = wash('mound', P, (146, 168, 96), .5)

PALETTES = {
    # Rulebook: brown-black ink on aged paper.
    'ink': {'paper': (236, 226, 199), 'ink': (38, 30, 24), 'grid': 0.42},
    # Module map: TSR's blue on white.
    'blue': {'paper': (247, 247, 242), 'ink': (44, 104, 176), 'grid': 0.42},
    # The module map coloured in.
    'colour': {'paper': (247, 247, 242), 'ink': (44, 104, 176), 'grid': 0.42,
               'washes': True},
}


def game_colours():
    """The game's colour letters and their colours, from src/z-color.c."""
    src = open(os.path.join(ROOT, 'src', 'z-color.c')).read()
    letters = re.findall(r"\{'(.)', \"", src)
    table = src[src.index('angband_color_table[MAX_COLORS][4] ='):]
    table = table[:table.index('};')]
    rgb = re.findall(r'\{0x00, 0x([0-9a-f]{2}), 0x([0-9a-f]{2}), 0x([0-9a-f]{2})\}',
                     table)
    # The table runs one past the letters: COLOUR_SHADE has none.
    assert len(rgb) >= len(letters), 'z-color.c has changed shape'
    return {l: tuple(int(h, 16) for h in c) for l, c in zip(letters, rgb)}


def terrain_colours():
    """Each terrain's colour letter, from lib/gamedata/terrain.txt."""
    out, name = {}, None
    for line in open(os.path.join(ROOT, 'lib', 'gamedata', 'terrain.txt')):
        line = line.strip()
        if line.startswith('name:'):
            name = line[5:]
        elif line.startswith('graphics:') and name:
            out[name] = line.rsplit(':', 1)[1]
    return out


class Tile:
    def __init__(self):
        self.px = [bytearray(TW) for _ in range(TH)]

    def get(self, x, y):
        if 0 <= x < TW and 0 <= y < TH:
            return self.px[y][x]
        return T

    def set(self, x, y, v):
        if 0 <= x < TW and 0 <= y < TH:
            self.px[y][x] = v


def in_diamond(x, y, cx, cy, hw=HW, hh=HH):
    return abs(x + .5 - cx) / hw + abs(y + .5 - cy) / hh < 1


# --- outlines ---------------------------------------------------------------
#
# Shapes are drawn as a map of labelled regions and the ink goes on every
# boundary between two labels, one pixel wide, on the inside of the shape.
# keep(a, b, x, y, where) may veto a boundary, which is how a wall leaves off
# the edges it shares with its neighbours.

def outline(t, lab, keep=None, code=I):
    def lab_at(x, y):
        if 0 <= x < TW and 0 <= y < TH:
            return lab[y][x]
        return 0

    for y in range(-1, TH):
        for x in range(-1, TW):
            a = lab_at(x, y)
            for nx, ny, where in ((x + 1, y, 'h'), (x, y + 1, 'v')):
                b = lab_at(nx, ny)
                if a == b:
                    continue
                # Ink goes on the shape's side; between two parts of a
                # shape, on the first.
                if a == 0:
                    px, py, own, other, side = nx, ny, b, a, where + '-'
                else:
                    px, py, own, other, side = x, y, a, b, where + '+'
                if keep and not keep(own, other, px, py, side):
                    continue
                t.set(px, py, code)


# --- ground -----------------------------------------------------------------

def ground(t, pattern=None, base=P):
    """The floor diamond, in a wash or a pattern, and the grid."""
    for y in range(TH):
        for x in range(TW):
            if in_diamond(x, y, CX, CY):
                t.set(x, y, pattern(x, y) if pattern else base)
    grid(t)


def grid(t):
    """
    The ten-foot grid: the diamond's two upper edges only.  Its lower edges
    are its neighbours' upper ones, so drawing all four would rule every line
    twice.
    """
    for y in range(TH):
        for x in range(TW):
            if not in_diamond(x, y, CX, CY) or y + .5 > CY:
                continue
            if not in_diamond(x, y - 1, CX, CY) or \
                    not in_diamond(x - 1 if x < CX else x + 1, y, CX, CY):
                t.set(x, y, G)


def stipple(rng, density, base=P):
    dots = set()
    for y in range(TH):
        for x in range(TW):
            if rng.below(1000) < density:
                dots.add((x, y))
    return lambda x, y: I if (x, y) in dots else base


# --- boxes ------------------------------------------------------------------
#
# Everything solid is a box on the grid: a rectangle of the square, a = 0..1
# west to east and b = 0..1 north to south, standing from height zb to zt in
# pixels.  The square's north corner is at (CX, CY - HH) on the tile.

def to_screen(a, b, z=0):
    return CX + (a - b) * HW, CY - HH + (a + b) * HH - z


def box(t, mat, x0=0., x1=1., y0=0., y1=1., zb=0, zt=WALL_H, hide=0, join=0,
        ground_line=True):
    """
    Draw a box.  hide says which faces have a wall in front of them and are not
    drawn (8: the south face, 4: the east); join, which of the top's edges meet
    another wall's top and are left off (1 west, 2 north, 4 east, 8 south).
    A wall uses its neighbours for both, so a run of wall is one mass.
    mat gives each face's ink from the pixel and the face's own coordinates.
    """
    lab = [[0] * TW for _ in range(TH)]
    where = {}

    for y in range(TH):
        for x in range(TW):
            px, py = x + .5, y + .5
            # The top: undo the projection at the top's height.
            s = (px - CX) / HW
            q = (py - (CY - HH) + zt) / HH
            a, b = (q + s) / 2, (q - s) / 2
            if x0 <= a <= x1 and y0 <= b <= y1:
                lab[y][x] = 1
                where[x, y] = (a, b)
                t.set(x, y, mat['top'](x, y, a, b))
                continue
            # The south face, b = y1.
            if not hide & 8:
                a = s + y1
                z = (CY - HH) + (a + y1) * HH - py
                if x0 <= a <= x1 and zb <= z <= zt:
                    lab[y][x] = 2
                    where[x, y] = (a, z)
                    t.set(x, y, mat['south'](x, y, (a - x0) * HW, zt - z))
                    continue
            # The east face, a = x1.
            if not hide & 4:
                b = x1 - s
                z = (CY - HH) + (x1 + b) * HH - py
                if y0 <= b <= y1 and zb <= z <= zt:
                    lab[y][x] = 3
                    where[x, y] = (b, z)
                    t.set(x, y, mat['east'](x, y, (b - y0) * HW, zt - z))

    near = 1.5 / HW

    def keep(own, other, x, y, side):
        if other != 0:
            return True
        if own == 1:
            a, b = where[x, y]
            d = {1: a - x0, 2: b - y0, 4: x1 - a, 8: y1 - b}
            return not join & min(d, key=d.get)
        if own == 2:
            a, z = where[x, y]
            if z - zb < 1.5:
                return ground_line
            return not (join & 1 and a - x0 < near)
        if own == 3:
            b, z = where[x, y]
            if z - zb < 1.5:
                return ground_line
            return not (join & 2 and b - y0 < near)
        return True

    outline(t, lab, keep)


def solid(code, marks=None, mark=P):
    def f(x, y, *_):
        if marks and (x, y) in marks:
            return mark
        return code
    return f


def flat(code):
    return lambda x, y, u, v: code


def ribs():
    """A roof's ribs, or a permanent wall's: every eighth line, so they meet."""
    return {(x, y) for x in range(TW) for y in range(TH) if ((x >> 1) + y) % 8 == 0}


def stones(rng, course=8, shade=False, fill=STONE):
    """
    Dressed stone: courses ruled across the face and joints at uneven
    intervals, a few chips, and on the side in shadow the engraver's hatching.
    u runs along the face and v down it from the top.
    """
    joints = []
    for _ in range(8):
        row, u = set(), rng.below(9)
        while u < HW:
            row.add(u)
            u += 8 + rng.below(8)
        joints.append(row)
    chips = {(rng.below(HW), rng.below(WALL_H)) for _ in range(5)}

    def f(x, y, u, v):
        iu, iv = int(u), int(v)
        c = iv // course
        if (iv % course == 0 and iv > 0) or (iu in joints[c % 8] and iv % course):
            return I
        if (iu, iv) in chips:
            return I
        if shade and (x + y) % 4 == 0:
            return I
        return fill
    return f


def specks(rng, n, area=(TW, TH)):
    return {(rng.below(area[0]), rng.below(area[1])) for _ in range(n)}


def stars(rng, n):
    """Treasure, marked as the maps marked it."""
    out = set()
    for _ in range(n):
        sx, sy = 8 + rng.below(48), 8 + rng.below(56)
        for dx, dy in ((0, 0), (1, 0), (-1, 0), (0, 1), (0, -1),
                       (2, 0), (-2, 0), (0, 2), (0, -2)):
            out.add((sx + dx, sy + dy))
    return out


def materials():
    r = mt.Rand(1978)
    granite = {'top': solid(ROCK), 'south': stones(r),
               'east': stones(r, shade=True)}
    permanent = {
        'top': solid(ROCK, marks=ribs()),
        'south': stones(r, course=12, fill=DRESSED),
        'east': stones(r, course=12, shade=True, fill=DRESSED),
    }
    magma = {
        'top': solid(ROCK, marks=specks(r, 160)),
        'south': lambda x, y, u, v, d=specks(r, 300): I if (x, y) in d or
        (x + y) % 4 == 0 else RUST,
        'east': lambda x, y, u, v, d=specks(r, 300): RUST if (x, y) in d or
        (x + y) % 2 else I,
    }
    quartz = {
        'top': solid(QUARTZ, marks=specks(r, 220), mark=I),
        'south': lambda x, y, u, v, d=specks(r, 60): I if (x, y) in d else QUARTZ,
        'east': lambda x, y, u, v: I if (x + y) % 4 == 0 else QUARTZ,
    }
    rt = mt.Rand(1979)

    def rich(mat, gold):
        s = stars(rt, 3)
        return {k: (lambda x, y, *w, f=f: (gold if (x, y) in s else f(x, y, *w)))
                for k, f in mat.items()}

    rb = mt.Rand(1980)
    building = {
        'top': solid(ROOF, marks=ribs(), mark=RIB),
        'south': stones(rb, course=12, fill=PLASTER),
        'east': stones(rb, course=12, shade=True, fill=PLASTER),
    }
    return {
        'granite': granite,
        'permanent': permanent,
        'magma': magma,
        'quartz': quartz,
        'magma treasure': rich(magma, GOLD_P),
        'quartz treasure': rich(quartz, GOLD_I),
        'building': building,
    }


def wall(t, mat, mask, h=WALL_H):
    """A wall square: hide the faces and join the edges other walls touch."""
    box(t, mat, hide=mask & 12, join=mask, zt=h)


# --- things on the floor ----------------------------------------------------

def line(t, x0, y0, x1, y1, code=I):
    dx, dy = abs(x1 - x0), -abs(y1 - y0)
    sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1)
    err = dx + dy
    while True:
        t.set(x0, y0, code)
        if x0 == x1 and y0 == y1:
            break
        e2 = 2 * err
        if e2 >= dy:
            err += dy
            x0 += sx
        if e2 <= dx:
            err += dx
            y0 += sy


def shape(t, inside, fill=lambda x, y: P):
    """Fill a region and ink its outline."""
    lab = [[0] * TW for _ in range(TH)]
    for y in range(TH):
        for x in range(TW):
            if inside(x + .5, y + .5):
                lab[y][x] = 1
                t.set(x, y, fill(x, y))
    outline(t, lab)


def boulder(t, cx, cy, r):
    shape(t, lambda px, py: (px - cx) ** 2 + ((py - cy) * 1.3) ** 2 < r * r,
          lambda x, y: I if (x + .5 - cx + y + .5 - cy > r * .4 and x % 2 == 0)
          else BOULDER)


# Doors are wall-high blocks like the walls either side of them, with the door
# drawn on whichever face looks out: the south face in a west-east wall, the
# east face in a north-south one.  The top is the maps' door symbol, a box in
# the line of the wall, so a door reads from above as it did on the page.

def planks(shade, fill=WOOD):
    def f(x, y, u, v):
        iu, iv = int(u), int(v)
        if iu in (2, 29) or iv in (2, 3):                    # the frame
            return I
        if 2 < iu < 29 and iv > 3:
            if iv in (9, 18) or iu % 3 == 0:                 # boards, straps
                return I
            if shade and (x + y) % 2 == 0:
                return I
        return fill
    return f


def door_top(inner, fill=WOOD):
    def f(x, y, a, b):
        if inner(a, b):
            return I
        return fill
    return f


def door(t, ns, rng):
    stone = stones(rng)
    face = planks(ns)
    if ns:
        mat = {'top': door_top(lambda a, b: abs(a - .5) < .06),
               'south': stone, 'east': face}
        box(t, mat, hide=8)
    else:
        mat = {'top': door_top(lambda a, b: abs(b - .5) < .06),
               'south': face, 'east': stone}
        box(t, mat, hide=4)


def door_cut(t, ns):
    """A closed door in front of the character: its sill, and the symbol."""
    sill = {'top': door_top(lambda a, b: abs((a if ns else b) - .5) < .06),
            'south': flat(WOOD),
            'east': lambda x, y, u, v: I if (x + y) % 4 == 0 else WOOD}
    box(t, sill, hide=8 if ns else 4, zt=CUT_H)


def archway(t, ns, rng, broken=False):
    """An open doorway: the wall carried over the gap on a lintel."""
    stone = stones(rng)
    mat = {'top': solid(ROCK), 'south': stone, 'east': stones(rng, shade=True)}
    reach = .45 if broken else 1.
    if ns:
        box(t, mat, .3, .7, 0, reach, WALL_H - 6, WALL_H, join=2 | 8)
    else:
        box(t, mat, 0, reach, .3, .7, WALL_H - 6, WALL_H, join=1 | 4)
    if broken:
        for i in range(4):
            x, y = CX - 12 + i * 7, CY + 2 + (i % 2) * 4
            line(t, x, y, x + 5, y - 2)


FONT = {
    '0': ('111', '101', '101', '101', '111'),
    '1': ('010', '110', '010', '010', '111'),
    '2': ('111', '001', '111', '100', '111'),
    '3': ('111', '001', '011', '001', '111'),
    '4': ('101', '101', '111', '001', '001'),
    '5': ('111', '100', '111', '001', '111'),
    '6': ('111', '100', '111', '101', '111'),
    '7': ('111', '001', '010', '010', '010'),
    '8': ('111', '101', '111', '101', '111'),
    '9': ('111', '101', '111', '001', '111'),
    'A': ('010', '101', '111', '101', '101'),
    'C': ('111', '100', '100', '100', '111'),
    'F': ('111', '100', '110', '100', '100'),
    'H': ('101', '101', '111', '101', '101'),
    'I': ('111', '010', '010', '010', '111'),
    'M': ('101', '111', '111', '101', '101'),
    'R': ('110', '101', '110', '101', '101'),
    'S': ('111', '100', '111', '001', '111'),
}


def sign(t, text, cx, bottom, bg=P):
    """A numbered board, as the maps keyed their rooms, its foot at bottom."""
    w, h = 8 * len(text) + 3, 15
    x0, y0 = cx - w // 2, bottom - h + 1
    for y in range(y0, y0 + h):
        for x in range(x0, x0 + w):
            edge = y in (y0, y0 + h - 1) or x in (x0, x0 + w - 1)
            t.set(x, y, I if edge else bg)
    for i, ch in enumerate(text):
        for r, row in enumerate(FONT[ch]):
            for c, bit in enumerate(row):
                if bit == '1':
                    for dy in (0, 1):
                        for dx in (0, 1):
                            t.set(x0 + 3 + i * 8 + c * 2 + dx,
                                  y0 + 2 + r * 2 + dy, I)


def waves(t, gap, amp, fill=P, wave=I):
    """Rows of waves, a whole number of them to a square so they run on."""
    def pat(x, y):
        yy = y + amp * math.sin(x * math.pi / 8)
        return wave if int(yy) % gap == 0 else fill
    ground(t, pat)


# --- the set ----------------------------------------------------------------

def floors():
    out = {}
    t = Tile()
    ground(t)
    out['floor'] = t

    for name, density, seed, base in (('earth', 40, 3, EARTH),
                                      ('sand', 140, 4, SAND)):
        t = Tile()
        ground(t, stipple(mt.Rand(seed), density, base))
        out[name] = t

    t = Tile()
    ground(t, base=GRASS)
    rng = mt.Rand(5)
    for _ in range(6):
        x, y, s = 8 + rng.below(48), 52 + rng.below(24), 1 + rng.below(3)
        if in_diamond(x, y, CX, CY) and in_diamond(x, y - s - 1, CX, CY):
            line(t, x - s, y - s - 1, x, y)
            line(t, x, y, x + s, y - s - 1)
            t.set(x, y - s, I)
    out['grass'] = t

    t = Tile()
    rng = mt.Rand(6)
    ground(t, stipple(rng, 25, MUD))
    for _ in range(7):
        x, y = 10 + rng.below(44), 54 + rng.below(20)
        if in_diamond(x - 3, y, CX, CY) and in_diamond(x + 3, y, CX, CY):
            line(t, x - 3, y, x + 3, y)
    out['mud'] = t

    # The road: a mechanical dot screen, the tint 1978 printers laid on.
    t = Tile()
    ground(t, lambda x, y: I if y % 4 == 0 and (x + (y // 4 % 2) * 2) % 4 == 0
           else ROAD)
    out['road'] = t

    t = Tile()
    waves(t, 8, 1.0, WATER)
    out['shallow water'] = t
    t = Tile()
    waves(t, 4, 1.0, DEEP)
    out['deep water'] = t
    t = Tile()
    waves(t, 4, 1.2, SEA, FOAM)
    out['sea'] = t

    # Lava: dark crust and bright cracks -- what stays light is what burns.
    t = Tile()
    rng = mt.Rand(11)
    cracks = set()
    for _ in range(5):
        x, y = 8 + rng.below(48), 50 + rng.below(28)
        for _ in range(14):
            cracks.add((x, y))
            x += rng.below(3) - 1 + (1 if rng.below(2) else 0)
            y += rng.below(3) - 1
    ground(t, lambda x, y: GLOW if (x, y) in cracks else CRUST)
    out['lava'] = t
    return out


def features():
    out = {}
    rng = mt.Rand(20)

    # Up: a flight of four steps climbing away to the north.
    t = Tile()
    ground(t)
    step = {'top': solid(STEP), 'south': flat(STEP),
            'east': lambda x, y, u, v: I if (x + y) % 4 == 0 else STEP}
    for k in range(4):
        box(t, step, .15, .85, .1, .9 - k * .2, 0, 6 + k * 6)
    out['up staircase'] = t

    # Down: a well in the floor with the treads going down into the dark.
    t = Tile()
    ground(t)
    for y in range(TH):
        for x in range(TW):
            s, q = (x + .5 - CX) / HW, (y + .5 - (CY - HH)) / HH
            a, b = (q + s) / 2, (q - s) / 2
            if .15 <= a <= .85 and .12 <= b <= .88:
                edge = min(a - .15, .85 - a, b - .12, .88 - b) < .03
                tread = int((b - .12) * 30) % 5 == 0 and a > .22 + (b - .12) * .3
                t.set(x, y, I if edge or not tread else STEP)
    out['down staircase'] = t

    for name, stones_n, seed in (('rubble', 7, 12), ('passable rubble', 4, 13)):
        t = Tile()
        ground(t, stipple(mt.Rand(seed), 20))
        r = mt.Rand(seed)
        spots = [(CX - 12, CY - 2), (CX + 10, CY - 3), (CX, CY - 6),
                 (CX - 4, CY + 4), (CX + 12, CY + 4), (CX - 14, CY + 6),
                 (CX + 2, CY + 10)]
        for i in range(stones_n):
            x, y = spots[i]
            boulder(t, x + r.below(3), y, 6 if name == 'rubble' else 4)
        out[name] = t

    t = floors()['grass']
    shape(t, lambda px, py: abs(px - CX) < 2 and CY - 16 < py < CY + 2,
          lambda x, y: BARK)
    r = mt.Rand(14)
    bumps = [r.below(3) for _ in range(12)]

    def crown(px, py):
        dx, dy = px - CX, (py - (CY - 28)) * 1.1
        a = math.atan2(dy, dx)
        rad = 15 + bumps[int((a + math.pi) / (2 * math.pi) * 12) % 12]
        return dx * dx + dy * dy < rad * rad

    shape(t, crown, lambda x, y: I if (x - CX) - (y - CY + 28) > 2 and
          (x + y) % 4 == 0 else LEAF)
    out['tree'] = t

    t = Tile()
    ground(t, stipple(mt.Rand(15), 30, EARTH))

    def peak(cx, base, w, h):
        def inside(px, py):
            if py > base:
                return False
            return abs(px - cx) < (py - (base - h)) * w / h
        return inside

    for cx, base, w, h in ((CX - 8, CY + 4, 20, 34), (CX + 10, CY + 8, 18, 26)):
        shape(t, peak(cx, base, w, h),
              lambda x, y, cx=cx: I if x > cx and x % 2 == 0 else PEAK)
    out['mountain'] = t

    t = Tile()
    ground(t, stipple(mt.Rand(16), 30, EARTH))
    shape(t, lambda px, py: ((px - CX) / 26) ** 2 + ((py - CY - 2) / 26) ** 2 < 1
          and py < CY + 2,
          lambda x, y: I if x > CX + 4 and x % 2 == 0 else MOUND)
    shape(t, lambda px, py: ((px - CX) / 7) ** 2 + ((py - CY - 2) / 11) ** 2 < 1
          and py < CY + 2, lambda x, y: I)
    out['dungeon entrance'] = t

    for ns in (False, True):
        tag = ' ns' if ns else ' we'
        t = Tile()
        ground(t)
        door(t, ns, rng)
        out['closed door' + tag] = t

        t = Tile()
        ground(t)
        door_cut(t, ns)
        out['closed door cut' + tag] = t

        t = Tile()
        ground(t)
        archway(t, ns, rng)
        out['open door' + tag] = t

        t = Tile()
        ground(t)
        archway(t, ns, rng, broken=True)
        out['broken door' + tag] = t

    return out


SHOPS = (
    ('General Store', '1'), ('Armoury', '2'), ('Weapon Smiths', '3'),
    ('Bookseller', '4'), ('Alchemy Shop', '5'), ('Magic Shop', '6'),
    ('Black Market', '7'), ('Home', '8'), ('Statue Store', 'S'),
    ('Figurine Store', 'F'), ('magetower', 'M'), ('healer', 'H'),
    ('inn', 'I'), ('magesmith', 'A'), ('chaos tower', 'C'),
    ('rechargers', 'R'),
)


def shop_washes():
    """A door and a sign for each shop, in the shop's colour on the flat map."""
    letters, table = terrain_colours(), game_colours()
    out = {}
    for name, _ in SHOPS:
        colour = table[letters[name]]
        # White would be no colour at all: steel instead.
        if min(colour) > 220:
            colour = (176, 184, 204)
        out[name] = (wash(name + ' door', P, colour, .75),
                     wash(name + ' sign', P, colour, .45))
    return out


SHOP_WASHES = shop_washes()


def door_face(t, east, fill):
    """
    A door in one face of a wall-high square, drawn alone: it is laid over
    the building's own wall, which the renderer draws first.
    """
    lab = [[0] * TW for _ in range(TH)]
    boards = planks(east, fill)
    for y in range(TH):
        for x in range(TW):
            px, py = x + .5, y + .5
            s = (px - CX) / HW
            if east:
                u = (1 - s) * HW
                z = (CY - HH) + (2 - s) * HH - py
            else:
                u = (s + 1) * HW
                z = (CY - HH) + (s + 2) * HH - py
            v = WALL_H - z
            if 8 <= u <= 24 and 5 <= v <= WALL_H:
                lab[y][x] = 1
                t.set(x, y, boards(x, y, (u - 8) * 31 / 16, v))
    outline(t, lab)


def shop_overlays(name, mark):
    """
    What a shop adds to its building: the door on the south face, on the east
    face, or on neither -- a door in the far side of a building, which this
    view cannot see -- and in every case the shop's sign on a post above the
    roof, so the shop can be told from anywhere.
    """
    door_fill, sign_fill = SHOP_WASHES[name]
    out = []
    for face in ('south', 'east', None):
        t = Tile()
        if face:
            door_face(t, face == 'east', door_fill)
        top = CY - WALL_H
        for y in range(top - 6, top + 1):
            t.set(CX, y, I)
        sign(t, mark, CX, top - 6, sign_fill)
        out.append(t)
    return out


# Which drawing each of the game's terrain features uses.  'wall' takes the
# sixteen-tile row for its material; 'door' takes a pair, west-east first.
FEATURES = (
    ('open floor', 'tile', 'floor'),
    ('closed door', 'door', 'closed door'),
    ('open door', 'door', 'open door'),
    ('broken door', 'door', 'broken door'),
    ('up staircase', 'tile', 'up staircase'),
    ('down staircase', 'tile', 'down staircase'),
    ('secret door', 'wall', 'granite'),
    ('pile of rubble', 'tile', 'rubble'),
    ('magma vein', 'wall', 'magma'),
    ('quartz vein', 'wall', 'quartz'),
    ('magma vein with treasure', 'wall', 'magma treasure'),
    ('quartz vein with treasure', 'wall', 'quartz treasure'),
    ('granite wall', 'wall', 'granite'),
    ('permanent wall', 'wall', 'permanent'),
    ('lava', 'tile', 'lava'),
    ('pile of passable rubble', 'tile', 'passable rubble'),
    ('grass', 'tile', 'grass'),
    ('bare earth', 'tile', 'earth'),
    ('sand', 'tile', 'sand'),
    ('tree', 'tile', 'tree'),
    ('mud', 'tile', 'mud'),
    ('shallow water', 'tile', 'shallow water'),
    ('deep water', 'tile', 'deep water'),
    ('open sea', 'tile', 'sea'),
    ('mountainside', 'tile', 'mountain'),
    ('dungeon entrance', 'tile', 'dungeon entrance'),
    ('road', 'tile', 'road'),
)

# A shop is the building's wall, with its door and sign laid over it:
# shop:<terrain>:<first overlay>:<wall>:<wall cut away>.
SHOP_FEATURES = tuple(name for name, _ in SHOPS)

# On the surface, where permanent walls are the town's buildings rather than a
# vault's, they have roofs.
SURFACE = (
    ('permanent wall', 'wall', 'building'),
)


def build():
    """Every tile, in sheet order, and the index of each drawing."""
    tiles, index = [], {}
    mats = materials()
    for name in ('granite', 'permanent', 'magma', 'quartz', 'magma treasure',
                 'quartz treasure', 'building'):
        for cut, h in (('', WALL_H), (' cut', CUT_H)):
            index[name + cut] = len(tiles)
            for mask in range(16):
                t = Tile()
                wall(t, mats[name], mask, h)
                tiles.append(t)

    for name, mark in SHOPS:
        index['shop ' + name] = len(tiles)
        tiles.extend(shop_overlays(name, mark))

    loose = {}
    loose.update(floors())
    loose.update(features())
    for name in sorted(loose):
        if name.endswith(' ns'):
            continue
        if name.endswith(' we'):
            base = name[:-3]
            index[base] = len(tiles)
            tiles.append(loose[name])
            tiles.append(loose[base + ' ns'])
        else:
            index[name] = len(tiles)
            tiles.append(loose[name])
    return tiles, index


def mix(a, b, k):
    """b laid over a at strength k."""
    return tuple(int(round(x + (y - x) * k)) for x, y in zip(a, b))


def colours(pal, faded=False):
    """Every ink's colour in this palette; faded is remembered, in pencil."""
    paper, ink = pal['paper'], pal['ink']
    grid = mix(ink, paper, 1 - pal['grid'])
    out = {T: (0, 0, 0, 0), P: paper, I: ink, G: grid}
    for n, (name, plain, colour, k) in enumerate(WASHES):
        if pal.get('washes') and colour:
            out[4 + n] = mix(paper, colour, k)
        else:
            out[4 + n] = out[plain]
    for code, c in out.items():
        if code != T:
            if faded:
                c = mix(c, paper, .55)
            out[code] = c + (255,)
    return out


def sheet(tiles, pal):
    rows = (len(tiles) + COLS - 1) // COLS
    out = [[(0, 0, 0, 0)] * (COLS * TW) for _ in range(rows * TH)]
    col = colours(pal)
    for n, t in enumerate(tiles):
        ox, oy = (n % COLS) * TW, (n // COLS) * TH
        for y in range(TH):
            row = out[oy + y]
            for x in range(TW):
                row[ox + x] = col[t.px[y][x]]
    return out


# --- counters ---------------------------------------------------------------
#
# What a tile set has no picture for -- its uniques, mostly -- the game prints
# as a letter, and the isometric view stands it on the map as a counter, the
# letter on a disc, as tabletop games marked what they had no figure for.  The
# letters are the DS port's 5x8 font, read from its source, so there is one
# drawing of each and the renderer carries no font of its own.

GLYPH_W, GLYPH_H, GLYPH_FIRST, GLYPH_COUNT = 5, 8, 0x20, 96

# The ASCII view uses no sheet: it sets the map's own characters out on the
# isometric lattice, on black, a wall a stack of three.  Its spacing -- half a
# square across and down -- and how far each character in a stack rises.
# Wider than the drawings' lattice would be too sparse to read as a map, and
# closer runs the characters together; this was chosen by eye against both.
TEXT_STYLES = {'ascii': (20, 10, 9)}


def glyphs():
    """The printable ASCII characters, white on clear, side by side."""
    text = open(os.path.join(ROOT, 'src', 'nds', 'nds-font-5x8.c')).read()
    body = text[text.index('ds_subfont[] = {') + 16:]
    body = body[:body.index('};')]
    body = re.sub(r'/\*.*?\*/', '', body, flags=re.S)
    body = body.replace('EMPTY', ','.join(['0'] * GLYPH_W * GLYPH_H))
    bits = [int(n) for n in re.findall(r'\d+', body)]
    assert len(bits) == 256 * GLYPH_W * GLYPH_H, 'the DS font has changed shape'
    out = [[(0, 0, 0, 0)] * (GLYPH_W * GLYPH_COUNT) for _ in range(GLYPH_H)]
    for n in range(GLYPH_COUNT):
        base = (GLYPH_FIRST + n) * GLYPH_W * GLYPH_H
        for y in range(GLYPH_H):
            for x in range(GLYPH_W):
                if bits[base + y * GLYPH_W + x]:
                    out[y][n * GLYPH_W + x] = (255, 255, 255, 255)
    return out


def write_map(path, index):
    with open(path, 'w') as f:
        f.write('# Generated by scripts/tiles/make-iso.py.  Do not edit.\n')
        f.write('# size:<tile width>:<tile height>:<columns>\n')
        f.write('size:%d:%d:%d\n' % (TW, TH, COLS))
        # The renderer outlines the figures it stands on the map in the
        # style's ink, and fades what is remembered towards its paper.
        f.write('# style:<name>:<paper r:g:b>:<ink r:g:b>\n')
        for name, pal in PALETTES.items():
            f.write('style:%s:%d:%d:%d:%d:%d:%d\n' % ((name,) + pal['paper']
                                                     + pal['ink']))
        f.write('# text:<style>:<half width>:<half height>:<rise>\n')
        for name, (hw, hh, rise) in TEXT_STYLES.items():
            f.write('text:%s:%d:%d:%d\n' % (name, hw, hh, rise))
        f.write('# glyphs:<file>:<width>:<height>:<first char>:<count>\n')
        f.write('glyphs:glyphs.png:%d:%d:%d:%d\n' % (GLYPH_W, GLYPH_H,
                                                    GLYPH_FIRST, GLYPH_COUNT))
        f.write('# feat:<terrain>:<tile|wall|door>:<first cell>:<cut away>\n')
        f.write('# surface: the same, used instead on the surface\n')
        for prefix, rows in (('feat', FEATURES), ('surface', SURFACE)):
            for feat, kind, drawing in rows:
                cut = index.get(drawing + ' cut', index[drawing])
                f.write('%s:%s:%s:%d:%d\n' % (prefix, feat, kind,
                                              index[drawing], cut))
        f.write('# feat:<shop>:shop:<overlays: south, east, sign>:<wall>:<cut>\n')
        for feat in SHOP_FEATURES:
            f.write('feat:%s:shop:%d:%d:%d\n' % (feat, index['shop ' + feat],
                                                  index['building'],
                                                  index['building cut']))


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    tiles, index = build()
    for name, pal in PALETTES.items():
        mt.write_png(os.path.join(out, 'iso-%s.png' % name), sheet(tiles, pal))
    mt.write_png(os.path.join(out, 'glyphs.png'), glyphs())
    write_map(os.path.join(out, 'iso.txt'), index)
    print('%d tiles, %d inks' % (len(tiles), 4 + len(WASHES)))


if __name__ == '__main__':
    main()
