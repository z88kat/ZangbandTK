#!/usr/bin/env python3
"""
Draw the wilderness terrain for the tilesets that have none.

ZangbandTK adds seventeen terrain features -- grass, earth, sand, mud, water to
three depths, road, tree, mountainside -- and Angband's tilesets have art for
none of them, because Angband has no wilderness.  David Gervais' 32x32 set turned
out to contain one already, unmapped, so `lib/tiles/gervais/feat-ztk.prf` is a
mapping and no art was needed.  The other three sets have nothing to map.

So this draws it.  Ten features x three lighting variants = thirty tiles, appended
as one new row to each sheet, with the matching `feat-ztk.prf` written beside it.

=== Why generated rather than drawn, or borrowed ===

Nine of the ten are *textures*: grass is mottled greens, sand is mottled tans,
deep water is a blue that barely varies.  A texture is a rule, not a picture, and
a rule survives being asked for it at 16x16 and again at 8x8 -- where a downscaled
32x32 tile is mush.  That is the whole argument for generating rather than
shrinking Gervais' set into three more sheets, and it has a licensing dividend:
the art here is this project's own, so no attribution follows it around.

=== Why the palette is sampled and not chosen ===

A green picked by hand is this script's green.  A green taken from the tileset is
that tileset's green, and the tile sits in the sheet it was made for rather than
beside it.  So `sample_palette` reads every opaque pixel already in the sheet,
sorts them into hue families, and returns the most-used colours of each -- Adam
Bolt's greens for Adam Bolt, the 8x8 set's greens for the 8x8 set.  Nothing here
knows what colour grass is; it knows to ask.

=== The lighting ramp ===

Angband asks for three: `torch` (a light right beside you), `lit` (ambient), and
`dark` (remembered).  Measured across the fifteen features Gervais maps three
ways, brightness runs torch > lit > dark, and the ratios cluster near 1.00 /
0.82 / 0.62.  Those are the multipliers used here, so a generated tileset dims
the way a drawn one does.

Deterministic: the same seed every run, so re-running produces byte-identical
sheets and the diff is empty unless something actually changed.
"""

import os
import re
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
TILES = os.path.normpath(os.path.join(HERE, '..', '..', 'lib', 'tiles'))

# tileset directory -> (sheet, tile width, tile height)
TARGETS = {
    'adam-bolt': ('16x16.png', 16, 16),
    'nomad':     ('8x16.png', 16, 16),
    'old':       ('8x8.png', 8, 8),
}

# torch, lit, dark -- see the module docstring
LIGHTING = (1.00, 0.82, 0.62)


# --- PNG, without a dependency ---------------------------------------------
#
# Pillow is not installed on the machine this is developed on and adding it to
# get at thirty tiles would be a poor trade.  These handle exactly what the
# tilesets are: 8-bit, non-interlaced, palette or RGB or RGBA in, RGBA out.

def read_png(path):
    data = open(path, 'rb').read()
    assert data[:8] == b'\x89PNG\r\n\x1a\n', path + ' is not a PNG'
    pos, idat, plte, trns = 8, b'', None, None
    width = height = depth = colour = interlace = 0

    while pos < len(data):
        length, kind = struct.unpack('>I4s', data[pos:pos + 8])
        pos += 8
        body = data[pos:pos + length]
        pos += length + 4
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
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[colour]
    raw = zlib.decompress(idat)
    stride = width * channels
    out = bytearray(height * stride)
    prev = bytearray(stride)
    p = 0

    for y in range(height):
        f = raw[p]
        p += 1
        line = bytearray(raw[p:p + stride])
        p += stride
        if f == 1:
            for i in range(channels, stride):
                line[i] = (line[i] + line[i - channels]) & 255
        elif f == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 255
        elif f == 3:
            for i in range(stride):
                a = line[i - channels] if i >= channels else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 255
        elif f == 4:
            for i in range(stride):
                a = line[i - channels] if i >= channels else 0
                b = prev[i]
                c = prev[i - channels] if i >= channels else 0
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 255
        out[y * stride:(y + 1) * stride] = line
        prev = line

    pixels = [[(0, 0, 0, 0)] * width for _ in range(height)]
    for y in range(height):
        base = y * stride
        row = pixels[y]
        for x in range(width):
            i = base + x * channels
            if colour == 6:
                row[x] = (out[i], out[i + 1], out[i + 2], out[i + 3])
            elif colour == 2:
                row[x] = (out[i], out[i + 1], out[i + 2], 255)
            elif colour == 0:
                v = out[i]
                row[x] = (v, v, v, 255)
            elif colour == 4:
                v = out[i]
                row[x] = (v, v, v, out[i + 1])
            else:
                idx = out[i]
                r, g, b = plte[idx * 3:idx * 3 + 3]
                a = trns[idx] if trns and idx < len(trns) else 255
                row[x] = (r, g, b, a)
    return pixels


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


# --- a repeatable random ----------------------------------------------------
#
# Python's `random` is seeded reproducibly but its internals are free to change
# between releases, and a tileset that redraws itself when somebody upgrades
# Python is a diff nobody asked for.  This is fixed for good.

class Rand:
    def __init__(self, seed):
        self.s = seed & 0xffffffff

    def next(self):
        self.s = (1103515245 * self.s + 12345) & 0x7fffffff
        return self.s

    def below(self, n):
        return self.next() % n

    def pick(self, seq):
        return seq[self.below(len(seq))]


# --- palette ----------------------------------------------------------------

def family(r, g, b):
    """
    Which hue family a colour belongs to, or None if it is not clearly in one.

    The tests are narrower than they look like they need to be, and each margin
    is here because a looser one put the wrong colour in the bucket on the first
    run:

    * **brown wants green a real fraction of the way from blue to red.**
      `r >= g >= b` alone admits pure red -- (200, 30, 30) passes it -- so the
      first generated earth came out scarlet in two of the three tilesets.
      Requiring `g` merely to clear `b` was not enough either: (110, 45, 35)
      passes that and is still brick red. Earth sits on the orange axis, so `g`
      has to be at least a quarter of the way up from `b` towards `r`.
    * **blue must not admit purple.** `b > r and b > g` alone admits (128, 0,
      200), so the first open sea was violet. Water has no red in it: `r` may
      not stand above `g`.
    * **grey excludes the ends.** Sampling every grey in a sheet finds the
      near-black outlines and the near-white highlights, which are the two most
      common greys in any tileset and make a mountainside out of static.
    * **green must be greener than both its neighbours.** `g >= r and g >= b`
      admits yellow, where red and green are equal -- Nomad's brightest "green"
      came back (248, 248, 8) -- and teal, where green and blue are: Adam Bolt's
      came back (88, 136, 136), which is why its first tree canopy was blue.

    Every one of these was found by dumping the sampled palette and reading it,
    after two rounds of looking at the rendered tiles and guessing wrong about
    why they were off. The palette is three numbers per colour; the tile is a
    thousand pixels. Read the smaller thing.
    """
    mx, mn = max(r, g, b), min(r, g, b)
    if mx < 24:
        return None
    if mx - mn < 22:
        return 'grey' if 48 < mx < 210 else None
    if g - r > 12 and g - b > 12:
        return 'green'
    if b > g and b - g > 18 and r <= g + 12:
        return 'blue'
    if r > g > b and r - b > 30 and (g - b) * 10 >= (r - b) * 3:
        return 'sand' if (r > 150 and g > 105) else 'brown'
    return None


def sample_palette(pixels):
    """The most-used colours of each hue family, darkest first."""
    counts = {}
    for row in pixels:
        for r, g, b, a in row:
            if a < 200:
                continue
            fam = family(r, g, b)
            if fam is None:
                continue
            # quantise, so near-identical shades reinforce each other
            key = (fam, r >> 4 << 4, g >> 4 << 4, b >> 4 << 4)
            counts[key] = counts.get(key, 0) + 1

    out = {}
    for (fam, r, g, b), n in counts.items():
        out.setdefault(fam, []).append((n, (r + 8, g + 8, b + 8)))
    for fam in out:
        top = sorted(out[fam], reverse=True)[:6]
        out[fam] = sorted((c for _, c in top),
                          key=lambda c: 0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2])
    return out


def shades(palette, fam, fallback):
    """Three shades of a family: dark, mid, light. Falls back if the sheet has none."""
    got = palette.get(fam, [])
    if len(got) < 3:
        return fallback
    return [got[0], got[len(got) // 2], got[-1]]


def scale(colour, factor):
    return tuple(min(255, max(0, int(v * factor))) for v in colour[:3]) + (255,)


def lighten(colour, factor):
    """
    Brighten without draining the colour out of it.

    Multiplying every channel is fine going down and wrong going up: the first
    channel to reach 255 stops while the others keep climbing, so the hue slides
    towards white. Adam Bolt's tree canopy came out pale blue-grey that way. This
    caps the factor at whatever the brightest channel can take, which keeps the
    ratios -- and so the hue -- intact.
    """
    top = max(colour[:3]) or 1
    return scale(colour, min(factor, 255.0 / top))


# --- the tiles --------------------------------------------------------------

def mottle(size, cols, rng, weights=(5, 3, 2)):
    """Noise in three shades -- what grass, earth, sand and rock all are."""
    bag = []
    for c, wgt in zip(cols, weights):
        bag += [c] * wgt
    return [[rng.pick(bag) for _ in range(size)] for _ in range(size)]


def ripples(size, cols, rng):
    """Water: banded horizontally, because still water reads as horizontal."""
    grid = []
    for y in range(size):
        band = cols[1] if (y // max(1, size // 8)) % 2 else cols[2]
        row = []
        for x in range(size):
            row.append(cols[0] if rng.below(9) == 0 else
                       (cols[2] if rng.below(4) == 0 else band))
        grid.append(row)
    return grid


def flat(size, cols, rng):
    """Open sea: almost nothing. There is nothing out there and nothing to read."""
    return [[cols[0] if rng.below(14) == 0 else cols[1] for _ in range(size)]
            for _ in range(size)]


def cobbles(size, cols, rng):
    """Road: stones with mortar between them, so it reads as a made thing."""
    step = max(3, size // 4)
    grid = [[cols[0] for _ in range(size)] for _ in range(size)]
    for gy in range(0, size, step):
        # stagger alternate courses, or the mortar lines run straight through
        # and the road reads as tiled bathroom floor
        offset = (step // 2) if (gy // step) % 2 else 0
        for gx in range(-step, size + step, step):
            shade = cols[1] if rng.below(2) else cols[2]
            x0 = gx + offset + rng.below(2)
            y0 = gy + rng.below(2)
            for y in range(max(0, y0), min(y0 + step - 1, size)):
                for x in range(max(0, x0), min(x0 + step - 1, size)):
                    grid[y][x] = shade
    return grid


def tree(size, greens, browns, rng):
    """
    A canopy over a trunk, standing on grass, so a wood is trees in a meadow.

    The canopy is deliberately not painted in the grass's own greens. It was at
    first, and the tree vanished: a shape drawn in the colours of the thing
    behind it is not a shape. So the canopy is lifted well clear of the ground
    and shaded on one side, which is the least a blob can do and still read as a
    tree at sixteen pixels.
    """
    grid = mottle(size, greens, rng, weights=(4, 3, 2))
    canopy = lighten(greens[2], 1.45)[:3]
    shadow = scale(greens[1], 0.80)[:3]
    cx, cy = size // 2, max(1, int(size * 0.38))
    rad = size * 0.36
    for y in range(size):
        for x in range(size):
            dx, dy = x - cx, y - cy
            if dx * dx + dy * dy <= rad * rad:
                grid[y][x] = canopy if dx + dy < 0 else shadow
    trunk = max(1, size // 7)
    bark = scale(browns[0], 0.85)[:3]
    for y in range(cy, size):
        for x in range(cx - trunk // 2, cx - trunk // 2 + trunk):
            if 0 <= x < size:
                grid[y][x] = bark
    return grid


def build(size, palette, rng):
    """The ten features, in the order they are written to the sheet."""
    greens = shades(palette, 'green', [(28, 52, 20), (52, 84, 32), (86, 124, 52)])
    browns = shades(palette, 'brown', [(56, 36, 22), (92, 62, 38), (130, 96, 62)])
    sands = shades(palette, 'sand', [(140, 106, 56), (186, 148, 88), (218, 186, 130)])
    greys = shades(palette, 'grey', [(64, 64, 66), (110, 110, 112), (156, 156, 158)])
    blues = shades(palette, 'blue', [(24, 52, 110), (40, 92, 168), (96, 156, 220)])

    dark_browns = [scale(c, 0.62)[:3] for c in browns]
    pale_blues = [scale(c, 1.25)[:3] for c in blues]

    return [
        ('GRASS', mottle(size, greens, rng)),
        ('DIRT', mottle(size, browns, rng)),
        ('SAND', mottle(size, sands, rng)),
        ('MUD', mottle(size, dark_browns, rng)),
        ('ROAD', cobbles(size, greys, rng)),
        ('TREE', tree(size, greens, browns, rng)),
        ('ROCK', mottle(size, greys, rng, weights=(4, 4, 1))),
        ('WATER', ripples(size, pale_blues, rng)),
        ('DEEP_WATER', ripples(size, blues, rng)),
        ('WORLD_EDGE', flat(size, [blues[0], blues[1]], rng)),
    ]


# --- writing it out ---------------------------------------------------------

HEADER = """\
# feat-ztk.prf -- ZangbandTK's terrain for the %s tileset.
#
# GENERATED by scripts/tiles/make-terrain.py, together with the row of tiles it
# points at.  Do not edit either by hand: re-running the script overwrites both.
#
# Angband has no wilderness, so no Angband tileset has art for one.  Gervais'
# 32x32 set happened to contain an unmapped one and is mapped rather than drawn;
# this set had nothing to map, so the thirty tiles below -- ten features, three
# lighting variants each -- were generated into a row appended to %s.
#
# The colours are not this script's. They are sampled from the sheet itself, so
# the grass here is %s's own green.
"""


def emit(directory, sheet, tile_w, tile_h):
    path = os.path.join(TILES, directory, sheet)
    pixels = read_png(path)
    height, width = len(pixels), len(pixels[0])
    cols = width // tile_w
    palette = sample_palette(pixels)
    rng = Rand(0x5A46)

    features = build(tile_w, palette, rng)
    assert len(features) * len(LIGHTING) + 1 <= cols, \
        '%s has only %d columns; needs %d' % (directory, cols,
                                              len(features) * len(LIGHTING) + 1)

    # A row on the bottom, so nothing already in the sheet is touched.
    #
    # Re-running must not grow the sheet again. The first version appended
    # unconditionally, so a second run put a second row on and moved every
    # coordinate -- which makes a generated file that changes every time you
    # regenerate it, and a diff nobody can read. If a previous run left a
    # feat-ztk.prf, its row is reclaimed: the sheet is truncated back to it and
    # the row rewritten in place, so running this twice is the same as once.
    row = height // tile_h
    previous = os.path.join(TILES, directory, 'feat-ztk.prf')
    if os.path.exists(previous):
        seen = re.findall(r'^feat:\w+:\w+:0x([0-9A-Fa-f]{2}):',
                          open(previous, encoding='utf-8').read(), re.M)
        if seen:
            row = min(int(v, 16) for v in seen) - 0x80
            del pixels[row * tile_h:]
            height = row * tile_h

    for _ in range(tile_h):
        pixels.append([(0, 0, 0, 0)] * width)

    lines = [HEADER % (directory, sheet, directory)]
    for n, (name, art) in enumerate(features):
        lines.append('')
        for k, factor in enumerate(LIGHTING):
            col = n * len(LIGHTING) + k
            for y in range(tile_h):
                for x in range(tile_w):
                    pixels[row * tile_h + y][col * tile_w + x] = \
                        scale(art[y % len(art)][x % len(art[0])], factor)
            keys = ('torch', 'lit', 'dark')
            if k == 0:
                lines.append('feat:%s:torch:0x%02X:0x%02X'
                             % (name, row + 0x80, col + 0x80))
                lines.append('feat:%s:los:0x%02X:0x%02X'
                             % (name, row + 0x80, col + 0x80))
            else:
                lines.append('feat:%s:%s:0x%02X:0x%02X'
                             % (name, keys[k], row + 0x80, col + 0x80))

    # One more tile, and it is not terrain: an opaque black square for the grid
    # the player has not seen.
    #
    # Angband points FEAT_NONE at cell (0,0), which in two of these three sheets
    # is fully transparent -- so what an unexplored square looks like is decided
    # by whatever the front end happens to leave behind a transparent tile, and
    # that is not the same everywhere. On macOS it came out white in Nomad's
    # set: a field of snow with the map drawn in it, which reads as a fault
    # rather than as unexplored ground.
    #
    # An opaque tile has no such argument with the front end. It is black
    # because black is what "you have not been here" has meant in this game
    # since it was ASCII, and one lighting variant because an unseen square is
    # not lit three different ways.
    none_col = len(features) * len(LIGHTING)
    for y in range(tile_h):
        for x in range(tile_w):
            pixels[row * tile_h + y][none_col * tile_w + x] = (0, 0, 0, 255)
    lines.append('')
    lines.append('# The unexplored grid, opaque so no front end has to guess.')
    lines.append('feat:NONE:*:0x%02X:0x%02X' % (row + 0x80, none_col + 0x80))

    write_png(path, pixels)
    out = os.path.join(TILES, directory, 'feat-ztk.prf')
    open(out, 'w', encoding='utf-8').write('\n'.join(lines) + '\n')
    return row, len(features), width, height + tile_h


def chain(directory):
    """Make sure the tileset's main prf loads the new file, once."""
    for name in os.listdir(os.path.join(TILES, directory)):
        if not name.startswith('graf-') or name.endswith('-ztk.prf'):
            continue
        path = os.path.join(TILES, directory, name)
        text = open(path, encoding='utf-8').read()
        if 'feat-ztk.prf' in text:
            continue
        text = text.rstrip('\n') + (
            "\n\n# ZangbandTK's own terrain (WLD), generated by"
            " scripts/tiles/make-terrain.py\n%:feat-ztk.prf\n")
        open(path, 'w', encoding='utf-8').write(text)


def main():
    for directory in sorted(TARGETS):
        sheet, tw, th = TARGETS[directory]
        row, n, w, h = emit(directory, sheet, tw, th)
        chain(directory)
        print('  %-10s %d features into new row %d (0x%02X); sheet now %dx%d'
              % (directory, n, row, row + 0x80, w, h))
    return 0


if __name__ == '__main__':
    sys.exit(main())
