#!/usr/bin/env python3
"""
Draw the six service buildings for the tilesets that have none.

ZangbandTK adds six places you walk into that are not shops -- a mage tower, a
chaos tower, a healer, an inn, a magesmith and a recharger -- and no shipped
tileset has art for any of them, because Angband has none of them.  Until this
they rendered as ASCII digits and plus signs in the middle of a drawn town.

Borrowing was the alternative and it is not good enough here.  The shops are
drawn in every set, so each service could have taken a shopfront -- but colour
is painted into the art in these sets, so all six would have been the same
picture, and "a building you enter" is not the question the player is asking
when they are looking for the healer.

=== Why generated rather than drawn ===

The same argument make-terrain.py makes about ground: a rule survives being
asked for it at 8x8 and again at 32x32, and a picture does not.  Four sheets at
three tile sizes is four times the drawing and four chances to be inconsistent.
So a building here is a rule -- roof, body, door, emblem -- evaluated at
whatever size the sheet uses.

The palette is sampled from the sheet, as it is there, so a set's own reds and
violets are used rather than this script's idea of them.  make-terrain.py's
family() deliberately declines to classify red and purple, because the tests
that would catch them also catch brick-coloured earth; the two families this
needs are sampled here instead, and nothing about that file changes.

=== Distinguishing six things at eight pixels ===

Silhouette first: towers are narrow with a pointed roof, the four services are
wide with a pitched one.  Then an emblem, three pixels across at the smallest:
a cross for the healer, a tankard for the inn, an anvil for the magesmith, a
bolt for the recharger.  The two towers are the closest pair and are separated
by their crowns -- one conical, one broken -- and by shade.

    scripts/tiles/make-services.py            draw them into the sheets
    scripts/tiles/make-services.py --preview  write a picture to look at first
    scripts/tiles/make-services.py --check    verify what was written

Deterministic: no randomness, so re-running produces byte-identical sheets.

Re-runnable: the row this appends is reclaimed on the next run, exactly as
make-terrain.py reclaims its own, so running it twice is the same as once.
IF make-terrain.py IS RE-RUN IT MUST BE RUN FIRST -- it truncates the sheet
back to its own row, which is above this one, and --check will tell you so.
"""

import importlib.util
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
TILES = os.path.join(ROOT, 'lib', 'tiles')
DATA = os.path.join(ROOT, 'lib', 'gamedata')
OUTPUT = 'serv-ztk.prf'

# The PNG reader and writer, the palette sampler and the lighting ramp all
# already exist next door.  Imported rather than copied so there is one of each.
_spec = importlib.util.spec_from_file_location(
    'make_terrain', os.path.join(HERE, 'make-terrain.py'))
mt = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(mt)

TARGETS = {
    'old': ('8x8.png', 8, 8),
    'adam-bolt': ('16x16.png', 16, 16),
    'nomad': ('8x16.png', 16, 16),
    'gervais': ('32x32.png', 32, 32),
}

BLACK = (0, 0, 0, 0)

# code, roof, emblem, hue family, and shades to fall back on when the sheet
# has none of that family.  The fallbacks are only reached for a sheet with no
# red or no violet in it at all.
SERVICES = (
    ('MAGETOWER',  'cone', 'dot',   'violet',
     [(52, 28, 78), (96, 56, 140), (150, 108, 198)]),
    ('CHAOSTOWER', 'jag',  'star',  'violet+',
     [(84, 24, 96), (146, 48, 160), (204, 104, 214)]),
    ('HEALER',     'peak', 'cross', 'green',
     [(28, 52, 20), (52, 100, 40), (92, 156, 72)]),
    ('INN',        'peak', 'mug',   'brown',
     [(56, 36, 22), (100, 68, 40), (148, 108, 68)]),
    ('MAGESMITH',  'peak', 'anvil', 'red',
     [(72, 20, 18), (136, 40, 34), (194, 78, 66)]),
    ('RECHARGER',  'peak', 'bolt',  'blue',
     [(24, 44, 96), (40, 84, 168), (96, 148, 224)]),
)


def warm_shades(pixels, fam, fallback):
    """Three shades of red or violet, sampled from the sheet.

    make-terrain.py's family() returns None for both on purpose -- the test
    that admits red also admits brick-coloured earth, and the test that admits
    purple also admits deep water.  These two run after that one has had its
    say, over the colours it declined, so nothing it classifies changes.
    """
    counts = {}
    for row in pixels:
        for r, g, b, a in row:
            if a < 200 or max(r, g, b) < 40:
                continue
            if mt.family(r, g, b) is not None:
                continue
            mn = min(r, g, b)
            if fam == 'red' and r - max(g, b) > 45:
                pass
            elif fam == 'violet' and r - g > 25 and b - g > 25 and mn < 160:
                pass
            else:
                continue
            key = (r >> 4 << 4, g >> 4 << 4, b >> 4 << 4)
            counts[key] = counts.get(key, 0) + 1

    if len(counts) < 3:
        return fallback

    top = sorted(counts.items(), key=lambda kv: -kv[1])[:6]
    got = sorted(((r + 8, g + 8, b + 8) for (r, g, b), _ in top),
                 key=lambda c: 0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2])
    return [got[0], got[len(got) // 2], got[-1]]


def emblem(kind, w, h):
    """The cells of a w-by-h box an emblem fills.

    Deliberately blocky.  At three pixels across an emblem is a suggestion, and
    the thing that separates a cross from a bolt at that size is which way the
    ink runs, not how finely it is drawn.
    """
    cx, cy = w // 2, h // 2
    out = set()

    if kind == 'cross':
        out |= {(cy, x) for x in range(w)} | {(y, cx) for y in range(h)}
    elif kind == 'anvil':
        out |= {(0, x) for x in range(w)}
        out |= {(h - 1, x) for x in range(w)}
        out |= {(y, cx) for y in range(h)}
    elif kind == 'mug':
        body = max(1, w - 2)
        out |= {(y, 0) for y in range(h)}
        out |= {(y, body) for y in range(h)}
        out |= {(h - 1, x) for x in range(body + 1)}
        out |= {(0, x) for x in range(body + 1)}
        # The handle, standing off the side, which is the whole silhouette.
        out |= {(cy - 1, w - 1), (cy, w - 1), (cy + 1, w - 1)}
    elif kind == 'bolt':
        for y in range(h):
            x = (w - 1) - (y * (w - 1)) // max(1, h - 1)
            out.add((y, x))
            if y == cy:
                out |= {(y, min(w - 1, x + 1)), (y, max(0, x - 1))}
    elif kind == 'slash':
        for y in range(h):
            out.add((y, (y * (w - 1)) // max(1, h - 1)))
            if y % 2 == 0:
                out.add((y, min(w - 1, 1 + (y * (w - 1)) // max(1, h - 1))))
    elif kind == 'star':
        out |= {(cy, x) for x in range(w)} | {(y, cx) for y in range(h)}
        for i in range(min(w, h)):
            out |= {(i, i), (i, w - 1 - i)}
    elif kind == 'dot':
        out |= {(cy, cx)}
        if w >= 5:
            out |= {(cy - 1, cx), (cy + 1, cx), (cy, cx - 1), (cy, cx + 1)}

    return {(y, x) for (y, x) in out if 0 <= y < h and 0 <= x < w}


def front(size, roof, mark, cols):
    """One building, drawn at whatever size the sheet uses."""
    dark, mid, light = cols
    art = [[BLACK] * size for _ in range(size)]

    tower = roof in ('cone', 'jag')
    inset = max(1, size // 3) if tower else max(0, size // 8)
    left, right = inset, size - 1 - inset
    roof_h = max(2, (size * 3) // 8)

    def put(y, x, c):
        if 0 <= y < size and 0 <= x < size:
            art[y][x] = c + (255,) if len(c) == 3 else c

    # Roof: a triangle from an apex, narrow for a tower and wide for a shop.
    apex = (left + right) // 2
    for y in range(roof_h):
        reach = ((y + 1) * ((right - left) // 2 + 1)) // roof_h
        for x in range(apex - reach, apex + reach + 1):
            put(y, x, light)
    if roof == 'jag':
        # A broken crown, not a dither.  Notching alternate columns of the
        # whole roof looked like a badly dithered gradient at every size; what
        # reads as broken is taking bites out of the roof's own edge.
        step = max(1, size // 8)
        for y in range(roof_h):
            reach = ((y + 1) * ((right - left) // 2 + 1)) // roof_h
            for x in range(apex - reach, apex + reach + 1):
                if ((x // step) % 2 == 0) and y < roof_h - step:
                    put(y, x, BLACK)

    # Body.
    for y in range(roof_h, size):
        for x in range(left, right + 1):
            put(y, x, mid)
    for y in range(roof_h, size):                  # outline, so it reads
        put(y, left, dark)
        put(y, right, dark)
    for x in range(left, right + 1):
        put(size - 1, x, dark)

    # Door, centred at the foot.
    dw = max(1, (right - left) // 3)
    dh = max(1, size // 4)
    for y in range(size - dh, size):
        for x in range(apex - dw // 2, apex - dw // 2 + dw):
            put(y, x, dark)

    # Emblem, in the body above the door.
    if mark:
        band = size - dh - roof_h
        ew = min(right - left - 1, max(3, (right - left) // 2 + 1))
        eh = min(band, max(3, band - 1))
        if ew >= 3 and eh >= 3:
            oy = roof_h + max(0, (band - eh) // 2)
            ox = apex - ew // 2
            for (y, x) in emblem(mark, ew, eh):
                put(oy + y, ox + x, light)

    return art


def build(size, pixels):
    """The six, in the order they go into the sheet."""
    palette = mt.sample_palette(pixels)
    out = []
    for code, roof, mark, fam, fallback in SERVICES:
        # `violet+` is the same family taken hot, so the chaos tower is not
        # the mage tower in a slightly different purple.
        hot = fam.endswith('+')
        fam = fam.rstrip('+')
        if fam in ('red', 'violet'):
            cols = warm_shades(pixels, fam, fallback)
        else:
            cols = mt.shades(palette, fam, fallback)
        if hot:
            cols = [mt.scale(c, 1.45)[:3] for c in cols]
        out.append((code, front(size, roof, mark, cols)))
    return out


def already_drawn(directory):
    """Terrain codes this set draws, from every prf in it except ours."""
    got = set()
    for name in sorted(os.listdir(os.path.join(TILES, directory))):
        if not name.endswith('.prf') or name == OUTPUT:
            continue
        with open(os.path.join(TILES, directory, name), encoding='utf-8',
                  errors='replace') as f:
            for line in f:
                if line.startswith('feat:'):
                    got.add(line.split(':')[1])
    return got


HEADER = """# %s -- the six service buildings, for a set that had none.
#
# GENERATED by scripts/tiles/make-services.py, together with the row of tiles
# it points at.  Do not edit either by hand: re-running the script overwrites
# both, and reclaims this row rather than appending a second one.
#
# ZangbandTK adds six places that are not shops.  No shipped tileset has art
# for any of them, and borrowing a shopfront would have made all six the same
# picture, because colour is painted into the art in these sets.  So they are
# drawn here: towers are narrow with a pointed roof, services are wide with a
# pitched one, and each carries an emblem -- a cross, a tankard, an anvil, a
# bolt.  The colours are the sheet's own, sampled from it.
#
# If make-terrain.py is re-run it must be run before this, because it
# truncates the sheet back to its own row, which is above this one.
"""


def emit(directory, sheet, tile_w, tile_h):
    path = os.path.join(TILES, directory, sheet)
    pixels = mt.read_png(path)
    height, width = len(pixels), len(pixels[0])
    cols = width // tile_w

    drawn = already_drawn(directory)
    features = [(c, a) for c, a in build(tile_w, pixels) if c not in drawn]
    if not features:
        return None

    need = len(features) * len(mt.LIGHTING)
    assert need <= cols, ('%s has %d columns; %d services need %d'
                          % (directory, cols, len(features), need))

    # Reclaim our own row if a previous run left one, so running this twice is
    # the same as running it once and no coordinate ever moves underneath a
    # preference file that is already written.
    row = height // tile_h
    previous = os.path.join(TILES, directory, OUTPUT)
    if os.path.exists(previous):
        import re
        seen = re.findall(r'^feat:\w+:\w+:0x([0-9A-Fa-f]{2}):',
                          open(previous, encoding='utf-8').read(), re.M)
        if seen:
            row = min(int(v, 16) for v in seen) - 0x80
            del pixels[row * tile_h:]
            height = row * tile_h

    assert row + 0x80 <= 0xFF and row < 128, \
        '%s has no row left below 128 for these tiles' % directory

    for _ in range(tile_h):
        pixels.append([(0, 0, 0, 0)] * width)

    lines = [HEADER % OUTPUT]
    for n, (code, art) in enumerate(features):
        lines.append('')
        for k, factor in enumerate(mt.LIGHTING):
            col = n * len(mt.LIGHTING) + k
            for y in range(tile_h):
                for x in range(tile_w):
                    pixels[row * tile_h + y][col * tile_w + x] = \
                        mt.scale(art[y % len(art)][x % len(art[0])], factor)
            if k == 0:
                lines.append('feat:%s:torch:0x%02X:0x%02X'
                             % (code, row + 0x80, col + 0x80))
                lines.append('feat:%s:los:0x%02X:0x%02X'
                             % (code, row + 0x80, col + 0x80))
            else:
                lines.append('feat:%s:%s:0x%02X:0x%02X'
                             % (code, ('torch', 'lit', 'dark')[k],
                                row + 0x80, col + 0x80))

    mt.write_png(path, pixels)
    open(os.path.join(TILES, directory, OUTPUT), 'w',
         encoding='utf-8').write('\n'.join(lines) + '\n')
    return row, len(features), width, height + tile_h


def chain(directory):
    """Make sure the tileset's main prf loads the new file, once."""
    for name in sorted(os.listdir(os.path.join(TILES, directory))):
        if not name.startswith('graf-') or name.endswith('-ztk.prf'):
            continue
        path = os.path.join(TILES, directory, name)
        text = open(path, encoding='utf-8').read()
        if OUTPUT in text:
            return False
        open(path, 'w', encoding='utf-8').write(
            text.rstrip('\n')
            + "\n\n# ZangbandTK's service buildings, generated by"
              " scripts/tiles/make-services.py\n%%:%s\n" % OUTPUT)
        return True
    return False


def preview(zoom=6):
    """A picture to look at, at actual size and enlarged.

    Brightness and shape cannot be judged from an enlargement alone: a mark
    three pixels across is a clear emblem at 6x and a smudge at 1x, and 1x is
    where the game draws it.
    """
    out_dir = os.environ.get('SCRATCH', '/tmp')
    rows = []
    for directory in sorted(TARGETS):
        sheet, tw, th = TARGETS[directory]
        pixels = mt.read_png(os.path.join(TILES, directory, sheet))
        rows.append((directory, tw, build(tw, pixels)))

    pad = 4
    width = pad + max(len(f) * (tw * zoom + pad)
                      for _, tw, f in rows)
    width = max(width, 6 * (32 * zoom + pad) + pad)
    height = pad + sum(tw * zoom + tw + 3 * pad for _, tw, _ in rows)
    img = [[(20, 20, 24, 255)] * width for _ in range(height)]

    y0 = pad
    for directory, tw, features in rows:
        x0 = pad
        for code, art in features:
            for y in range(tw):                     # actual size
                for x in range(tw):
                    c = art[y][x]
                    if c[3] if len(c) > 3 else 255:
                        img[y0 + y][x0 + x] = c[:3] + (255,)
            for y in range(tw * zoom):              # enlarged
                for x in range(tw * zoom):
                    c = art[y // zoom][x // zoom]
                    if len(c) > 3 and c[3] == 0:
                        continue
                    img[y0 + tw + pad + y][x0 + x] = c[:3] + (255,)
            x0 += tw * zoom + pad
        y0 += tw * zoom + tw + 3 * pad

    path = os.path.join(out_dir, 'services.png')
    mt.write_png(path, img)
    print('  preview: %s (%dx%d)' % (path, width, height))


def check():
    """Every coordinate written is inside the sheet, and every code is real."""
    codes = {l[5:].strip() for l
             in open(os.path.join(DATA, 'terrain.txt'), encoding='utf-8',
                     errors='replace') if l.startswith('code:')}
    import re
    problems = 0
    for directory in sorted(TARGETS):
        sheet, tw, th = TARGETS[directory]
        path = os.path.join(TILES, directory, OUTPUT)
        if not os.path.exists(path):
            print('  %-10s nothing generated' % directory)
            continue
        pixels = mt.read_png(os.path.join(TILES, directory, sheet))
        rows, cols = len(pixels) // th, len(pixels[0]) // tw
        n = 0
        for line in open(path, encoding='utf-8'):
            m = re.match(r'^feat:(\w+):\w+:0x([0-9A-Fa-f]{2}):0x([0-9A-Fa-f]{2})',
                         line)
            if not m:
                continue
            n += 1
            code, r, c = m.group(1), int(m.group(2), 16) & 0x7f, \
                int(m.group(3), 16) & 0x7f
            if code not in codes:
                print('  %s: no such terrain %s' % (directory, code))
                problems += 1
            if r >= rows or c >= cols:
                print('  %s: %s at row %d col %d is off a %dx%d sheet'
                      ' -- re-run this after make-terrain.py'
                      % (directory, code, r, c, rows, cols))
                problems += 1
        print('  %-10s %2d lines, sheet is %d rows x %d cols'
              % (directory, n, rows, cols))
    print('\n%d problems' % problems)
    return 1 if problems else 0


def main():
    if '--check' in sys.argv:
        return check()
    if '--preview' in sys.argv:
        preview()
        return 0

    for directory in sorted(TARGETS):
        sheet, tw, th = TARGETS[directory]
        got = emit(directory, sheet, tw, th)
        if not got:
            print('  %-10s already draws all six' % directory)
            continue
        row, n, w, h = got
        added = chain(directory)
        print('  %-10s %d services into row %d (0x%02X); sheet now %dx%d%s'
              % (directory, n, row, row + 0x80, w, h,
                 '; include added' if added else ''))
    return 0


if __name__ == '__main__':
    sys.exit(main())
