#!/usr/bin/env python3
"""
Build the character portraits: every race, every class, a man and a woman.

    scripts/portraits/build-portraits.py              write the sheets and index
    scripts/portraits/build-portraits.py --check      fail if they are out of date
    scripts/portraits/build-portraits.py --review DIR also write sheets to look at
    scripts/portraits/build-portraits.py --cut [BRIEF]
                                                      cut source.png from the brief

The Character window, the Birth window, the tombstone and the Character Record
show a portrait of the character.  Twenty-eight races, fourteen classes and two
sexes make 784.  An AI image generator drew them in November 2026, sixteen to a
sheet, to a brief written for this game.  The brief, every sheet it returned
and the reasons each portrait was kept or drawn again are in
archive/design_brief_portraits/, which is not in the repository.

Turning them into the game's sheets is two steps, and only the second runs
normally:

  * Cutting (--cut).  Each accepted portrait is averaged down to 64x64 art
    pixels and snapped to the DB32 palette, with the clean-ups its sheet's
    fixes.txt gives it.  The result is source.png, every figure on nothing,
    which is the portraits' source art.  recipe.txt and fixes.txt are copies
    of the brief's: which take of each portrait was used, and what was done
    to it.

  * Building (the default).  From source.png and backdrops.txt alone:
    lib/tcl/portraits/portraits.png, the figures at 64 with the class
    backdrops on a last row; portraits-32.png, the same shrunk to 32 for the
    Character Record and the tombstone; and portraits.txt, the index the front
    ends read them by.

The sheets are committed, so a build of the game needs no Python; --check,
which the Tk workflow runs, fails when they are not what this would make.
They are build artifacts: never edit them.  Building is deterministic and
needs nothing but Python.  Cutting also needs ImageMagick, to read the
sheets, and takes a few minutes.  The game's races and classes are read from
lib/gamedata, and one without a portrait, or a portrait for one the game does
not have, is an error rather than a gap.

From 13 October 2026 until the drawn ones came, the portraits were 32x32 busts
composed from text grids (art.py, since removed; see the diary).  The class
backdrops are the gradients those had.
"""

import colorsys
import os
import shutil
import struct
import subprocess
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
OUT = os.path.join(ROOT, "lib", "tcl", "portraits")
BRIEF = os.path.join(ROOT, "archive", "design_brief_portraits")

SIZE = 64           # the portraits as drawn, and as the Character window shows
SMALL = 32          # ... and shrunk, for the Record and the tombstone


# --- PNG -------------------------------------------------------------------------

def write_png(path, pixels):
    raw = bytearray()
    for row in pixels:
        raw.append(0)
        for px in row:
            raw += bytes(px)

    def chunk(kind, data):
        return (struct.pack(">I", len(data)) + kind + data
                + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff))

    height, width = len(pixels), len(pixels[0])
    out = b"\x89PNG\r\n\x1a\n"
    out += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    out += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    out += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(out)


def read_png(path):
    """An 8-bit RGBA PNG's pixels, as write_png takes them."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("%s is not a PNG" % path)
    pos, idat, width, height = 8, b"", 0, 0
    while pos < len(data):
        length, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        if kind == b"IHDR":
            width, height, depth, colour = struct.unpack(">IIBB", body[:10])
            if (depth, colour) != (8, 6):
                raise ValueError("%s is not 8-bit RGBA" % path)
        elif kind == b"IDAT":
            idat += body
        pos += 12 + length
    raw = zlib.decompress(idat)
    stride = width * 4
    rows, prev = [], bytearray(stride)
    for y in range(height):
        kind = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        # write_png filters nothing, so kind 0 is the common case.
        for i in range(stride) if kind else ():
            a = line[i - 4] if i >= 4 else 0
            b = prev[i]
            c = prev[i - 4] if i >= 4 else 0
            if kind == 1:
                line[i] = (line[i] + a) & 0xff
            elif kind == 2:
                line[i] = (line[i] + b) & 0xff
            elif kind == 3:
                line[i] = (line[i] + (a + b) // 2) & 0xff
            elif kind == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pred = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                line[i] = (line[i] + pred) & 0xff
        rows.append([tuple(line[x * 4:x * 4 + 4]) for x in range(width)])
        prev = line
    return rows


def hexrgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


# --- what the game has -------------------------------------------------------------

def gamedata_names(filename):
    names = []
    with open(os.path.join(ROOT, "lib", "gamedata", filename),
              encoding="utf-8") as f:
        for line in f:
            if line.startswith("name:"):
                names.append(line[5:].strip())
    return names


# --- the source --------------------------------------------------------------------

def read_index(path):
    """The races and classes of source.txt, in source.png's order."""
    races, classes = [], []
    with open(path, encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            key, value = line.rstrip("\n").split("\t")
            (races if key == "race" else classes).append(value)
    return races, classes


def read_backdrops():
    """Each class's backdrop, the colours at its top and bottom."""
    grounds = {}
    with open(os.path.join(HERE, "backdrops.txt"), encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            name, top, bottom = line.split()
            grounds[name] = (hexrgb(top), hexrgb(bottom))
    return grounds


def backdrop(class_name, size, grounds=None):
    """The class's backdrop: its two colours, top to bottom."""
    top, bottom = (grounds or read_backdrops())[class_name]
    rows = []
    for y in range(size):
        t = y / (size - 1)
        colour = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
        rows.append([colour + (255,)] * size)
    return rows


def over(figure, ground):
    """A figure set on a ground: what a window shows."""
    out = []
    for frow, grow in zip(figure, ground):
        row = []
        for f, g in zip(frow, grow):
            a = f[3] / 255.0
            row.append(tuple(int(g[i] + (f[i] - g[i]) * a) for i in range(3))
                       + (255,))
        out.append(row)
    return out


def shrink(fig, size):
    """A figure box-shrunk to size: a pixel is figure where at least half of
    what it covers is, in the average of that figure's colours."""
    out = []
    n = len(fig)
    for y in range(size):
        row = []
        ya, yb = y * n // size, (y + 1) * n // size
        for x in range(size):
            xa, xb = x * n // size, (x + 1) * n // size
            px = [fig[j][i] for j in range(ya, yb) for i in range(xa, xb)]
            solid = [p for p in px if p[3]]
            if len(solid) * 2 < len(px):
                row.append((0, 0, 0, 0))
            else:
                row.append(tuple(sum(p[k] for p in solid) // len(solid)
                                 for k in range(3)) + (255,))
        out.append(row)
    return out


def cell(sheet, col, row, size):
    return [line[col * size:(col + 1) * size]
            for line in sheet[row * size:(row + 1) * size]]


# --- the sheets ---------------------------------------------------------------------

def build():
    """(problems, sheets): the sheets by file name, and the index."""
    races = gamedata_names("p_race.txt")
    classes = gamedata_names("class.txt")
    src_races, src_classes = read_index(os.path.join(HERE, "source.txt"))
    grounds = read_backdrops()

    problems = []
    for kind, what, have, want in (("race", "portrait", src_races, races),
                                   ("class", "portrait", src_classes, classes),
                                   ("class", "backdrop", list(grounds), classes)):
        for name in want:
            if name not in have:
                problems.append("no %s for the %s %s" % (what, kind, name))
        for name in have:
            if name not in want:
                problems.append("a %s for a %s the game does not have: %s"
                                % (what, kind, name))
    source = read_png(os.path.join(HERE, "source.png"))
    if (len(source[0]), len(source)) != (len(src_races) * SIZE,
                                        2 * len(src_classes) * SIZE):
        problems.append("source.png is %dx%d, not what source.txt says"
                        % (len(source[0]), len(source)))
    if problems:
        return problems, None

    sheets = {}
    for name, size in (("portraits.png", SIZE), ("portraits-32.png", SMALL)):
        sheets[name] = [[(0, 0, 0, 0)] * (len(races) * size)
                        for _ in range((2 * len(classes) + 1) * size)]

    def place(name, size, pixels, col, row):
        for y in range(size):
            sheets[name][row * size + y][col * size:(col + 1) * size] = pixels[y]

    for ci, class_name in enumerate(classes):
        for ri, race_name in enumerate(races):
            for si, sex in enumerate("MF"):
                fig = cell(source, src_races.index(race_name),
                           2 * src_classes.index(class_name) + si, SIZE)
                if not any(p[3] for line in fig for p in line):
                    problems.append("the %s %s %s is empty in source.png"
                                    % (race_name, class_name, sex))
                place("portraits.png", SIZE, fig, ri, 2 * ci + si)
                place("portraits-32.png", SMALL, shrink(fig, SMALL),
                      ri, 2 * ci + si)
        place("portraits.png", SIZE, backdrop(class_name, SIZE, grounds),
              ci, 2 * len(classes))
        place("portraits-32.png", SMALL, backdrop(class_name, SMALL, grounds),
              ci, 2 * len(classes))

    index = [
        "# The character portraits: made by scripts/portraits/build-portraits.py",
        "# from scripts/portraits/source.png.  Do not edit; run that instead.",
        "#",
        "# A race is a column and a class two rows, a man's and a woman's, in the",
        "# order listed here.  The class backdrops are the last row, by class.",
        "# portraits.png's cells are size pixels square.  A sheet line names the",
        "# same layout at another size, for the windows that show them smaller.",
        "size\t%d" % SIZE,
        "sheet\t%d\tportraits-32.png" % SMALL,
    ]
    index += ["race\t%s" % name for name in races]
    index += ["class\t%s" % name for name in classes]
    sheets["portraits.txt"] = "\n".join(index) + "\n"
    return problems, sheets


# --- cutting -----------------------------------------------------------------------
#
# The brief's tools/cut.py, which judged every sheet as it came back, cuts with
# these same functions, so what was accepted is what this makes.

# DB32 by the brief's names for it.  Pink is never snapped to: it keys out
# against the magenta the figures were drawn on.
PALETTE = [
    ("black", "000000"), ("ink", "222034"), ("plum", "45283c"),
    ("umber", "663931"), ("brown", "8f563b"), ("orange", "df7126"),
    ("tan", "d9a066"), ("flesh", "eec39a"), ("yellow", "fbf236"),
    ("lime", "99e550"), ("green", "6abe30"), ("teal", "37946e"),
    ("moss", "4b692f"), ("olive", "524b24"), ("slate", "323c39"),
    ("navy", "3f3f74"), ("sea", "306082"), ("blue", "5b6ee1"),
    ("sky", "639bff"), ("cyan", "5fcde4"), ("frost", "cbdbfc"),
    ("white", "ffffff"), ("steel", "9badb7"), ("pewter", "847e87"),
    ("iron", "696a6a"), ("stone", "595652"), ("purple", "76428a"),
    ("red", "ac3232"), ("rose", "d95763"), ("pink", "d77bba"),
    ("sage", "8f974a"), ("bronze", "8a6f30"),
]

# Colours the generator drew on every sheet that DB32 has nothing near,
# snapped to as well as the palette so they stay as drawn.
EXTRA = [
    # Royal blue, the wizard's robe (batch 1): DB32's only dark blue is navy,
    # which leans purple, and turned every wizard violet.
    "0d4d8b", "0e386d", "081a4c",
    # Warm mid-brown skin (batch 2): between tan and brown, and bronze, an
    # olive gold, took it and turned brown faces green.
    "a86a44",
    # Neutral near-black (batch 2), the dark knight's armour: DB32's dark
    # greys are all tinted, and slate, the nearest, turned it green.
    "38383f",
    # Gold, a middle and a shadow tone (batch 7): DB32 jumps from yellow to
    # bronze, and orange took everything between, so blond elves went
    # ginger and gold trim, crosses and collars went orange.
    "d8a800", "b88400",
]

# Colours one people's skin needs, snapped to in every cell of that people,
# wherever its sheet.
RACE_EXTRA = {
    # Ruddy salmon skin (batch 11): its middle tone snapped to rose, the lip
    # colour, and striped every face.
    "Gnome": ["d86c54"],
    # Very pale skin (batch 21 and 22), between flesh and white: one sheet's
    # faces went chalk white, the other's blotched white on flesh.
    "High-Elf": ["f8dcc8"],
}

SNAP = [hexrgb(h) for name, h in PALETTE if name != "pink"] \
    + [hexrgb(h) for h in EXTRA]
KEEP = 0.5      # a block is figure when at least this much of it is


def load(path):
    """width, height, and the pixels as bytes, RGB."""
    ppm = subprocess.run(["magick", path, "-depth", "8", "ppm:-"],
                         check=True, capture_output=True).stdout
    # P6\nW H\n255\n
    parts = ppm.split(b"\n", 3)
    width, height = (int(v) for v in parts[1].split())
    return width, height, parts[3]


def magenta(r, g, b):
    return r > 170 and b > 170 and g < 110 and abs(r - b) < 80


LINE = 0.9      # a row or column this dark all the way across is a grid line
NEAR = 12       # ... when it is this close to a cell boundary
FRINGE = 2      # the JPEG ringing either side of it, painted over too
SOME = 0.6      # ... or this dark, if it also crosses the magenta:
FLANK = 10      # where the rows this far either side are magenta,
CROSS = 0.8     # it is not, this often


def ungrid(width, height, data):
    """data with the grid lines between the cells painted over, and the lines
    found.  Some sheets came with a dark line between every cell: it and the
    JPEG fringe either side are painted over with the pixels just beyond, each
    side from its own cell, so magenta stays magenta and a chest running off
    the bottom edge keeps running."""
    data = bytearray(data)

    def dark(i):
        return max(data[i], data[i + 1], data[i + 2]) < 100

    found = []
    for axis, size, other in (("x", width, height), ("y", height, width)):
        cell = size // 4

        def at(a, b):
            return ((b * width + a) if axis == "x" else (a * width + b)) * 3

        # Only the boundaries between cells: at the sheet's edges, the bottom
        # row's chests run off nearly all the way across (batch 15's dark
        # cloaks passed for a line), and no sheet has had a frame.
        def mag(i):
            return magenta(data[i], data[i + 1], data[i + 2])

        def is_line(a):
            if not (1 <= round(a / cell) <= 3
                    and abs(a - round(a / cell) * cell) <= NEAR):
                return False
            frac = sum(dark(at(a, b)) for b in range(0, other, 4)) \
                / (other // 4)
            if frac >= LINE:
                return True
            if frac < SOME:
                return False
            # Batch 22's lines stop short of the sheet's edges, so are dark
            # only 0.87 of the way.  Then it must also cross the magenta:
            # where the rows FLANK either side are magenta, it isn't.  A row
            # of dark clothes runs out where the magenta begins.
            flank = [b for b in range(0, other, 4)
                     if 0 <= a - FLANK and a + FLANK < size
                     and mag(at(a - FLANK, b)) and mag(at(a + FLANK, b))]
            return len(flank) >= 20 and \
                sum(not mag(at(a, b)) for b in flank) >= CROSS * len(flank)

        lines = [a for a in range(size) if is_line(a)]
        # Group into runs, one a boundary, and paint each side from beyond it.
        runs = []
        for a in lines:
            if runs and a - runs[-1][-1] <= 1:
                runs[-1].append(a)
            else:
                runs.append([a])
        for run in runs:
            lo, hi = run[0] - FRINGE, run[-1] + FRINGE
            edge = round(run[0] / cell) * cell      # the boundary it is on
            found.append("%s %d-%d" % (axis, run[0], run[-1]))
            for a in range(max(lo, 0), min(hi, size - 1) + 1):
                src = lo - 1 if a < edge else hi + 1
                if not 0 <= src < size:          # a frame round the sheet
                    src = hi + 1 if src < 0 else lo - 1
                for b in range(other):
                    i, j = at(a, b), at(src, b)
                    data[i:i + 3] = data[j:j + 3]
    return bytes(data), found


_snapped = {}


def snap(c, extra=()):
    key = (c, extra)
    if key not in _snapped:
        def dist(p):
            rm = (c[0] + p[0]) / 2
            dr, dg, db = c[0] - p[0], c[1] - p[1], c[2] - p[2]
            return (2 + rm / 256) * dr * dr + 4 * dg * dg \
                + (2 + (255 - rm) / 256) * db * db
        _snapped[key] = min(SNAP + list(extra), key=dist)
    return _snapped[key]


def saturate(c, factor):
    h, l, s = colorsys.rgb_to_hls(*(v / 255 for v in c))
    if not 0.17 <= h <= 0.5:        # yellow-green to cyan-green
        return c
    r, g, b = colorsys.hls_to_rgb(h, l, min(1.0, s * factor))
    return (round(r * 255), round(g * 255), round(b * 255))


def read_fixes(folder):
    """A sheet's fixes.txt, by cell: see the brief's tools/cut.py for what
    each fix does."""
    fixes = {}
    path = os.path.join(folder, "fixes.txt")
    if os.path.exists(path):
        with open(path, encoding="utf-8") as f:
            for line in f:
                line = line.split("#")[0].split()
                if line:
                    fix = fixes.setdefault(int(line[0]), {"clear": []})
                    for word in line[1:]:
                        key, value = word.split(":", 1)
                        if key in ("clear", "paint", "accent"):
                            fix.setdefault(key, []).append(value)
                        else:
                            fix[key] = value
    return fixes


def read_cells(folder):
    """A sheet's cells.txt: who is in each cell."""
    cells = {}
    with open(os.path.join(folder, "cells.txt"), encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            n, race, klass, sex, col, row = line.split()
            cells[int(n)] = (race, klass, sex, int(col), int(row))
    return cells


def fixes_for(n, fixes, cells):
    """A cell's fixes, with its people's RACE_EXTRA added to its add:."""
    fix = dict(fixes.get(n, {"clear": []}))
    race = cells.get(n, (None,))[0]
    adds = RACE_EXTRA.get(race, [])
    if adds:
        fix["add"] = ",".join(([fix["add"]] if "add" in fix else []) + adds)
    return fix


def colour(word):
    for name, h in PALETTE:
        if name == word:
            word = h
    return hexrgb(word)


def box(spec):
    xs, ys = spec.split(",")
    x0, x1 = (int(v) for v in xs.split("-"))
    y0, y1 = (int(v) for v in ys.split("-"))
    return x0, x1, y0, y1


def paint(fig, spec):
    where, change = spec.rsplit(":", 1)
    src, dst = change.split(">")
    src = colour(src)
    dst = None if dst == "none" else colour(dst)
    x0, x1, y0, y1 = box(where)
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            if fig[y][x][3] and fig[y][x][:3] == src:
                fig[y][x] = (0, 0, 0, 0) if dst is None else dst + (255,)


def clear(fig, spec):
    x0, x1, y0, y1 = box(spec)
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            fig[y][x] = (0, 0, 0, 0)


def cut(width, data, cx, cy, fix=None):
    """One cell of a sheet, cx, cy of its 4x4, as a 64x64 figure.  The cell is
    split into 64x64 blocks.  A block is transparent when less than half of it
    is figure; otherwise it takes the per-channel median of its figure pixels,
    which keeps the purple fringe between magenta and the outline out of the
    colour, and snaps to DB32 less pink."""
    fix = fix or {}
    sat = float(fix.get("green", 1))
    extra = tuple(hexrgb(h) for h in fix["add"].split(",")) \
        if "add" in fix else ()
    accents = []
    for spec in fix.get("accent", []):
        parts = spec.split(":")
        share = float(parts[2]) if len(parts) > 2 else 0.25
        accents.append(box(parts[0]) + (colour(parts[1]), share))
    cell = width // 4
    block = cell / SIZE
    out = []
    for by in range(SIZE):
        row = []
        y0, y1 = int(cy * cell + by * block), int(cy * cell + (by + 1) * block)
        for bx in range(SIZE):
            x0 = int(cx * cell + bx * block)
            x1 = int(cx * cell + (bx + 1) * block)
            rs, gs, bs = [], [], []
            total = 0
            for y in range(y0, y1):
                base = (y * width) * 3
                for x in range(x0, x1):
                    i = base + x * 3
                    r, g, b = data[i], data[i + 1], data[i + 2]
                    total += 1
                    if not magenta(r, g, b):
                        rs.append(r)
                        gs.append(g)
                        bs.append(b)
            if len(rs) < KEEP * total:
                row.append((0, 0, 0, 0))
                continue
            hit = [a for a in accents if a[0] <= bx <= a[1] and a[2] <= by <= a[3]]
            if hit:
                # A small mark (a red eye) that the median would lose: keep
                # it if enough of the block's figure snaps to it.
                tally = {}
                for r, g, b in zip(rs, gs, bs):
                    k = snap((r, g, b), extra)
                    tally[k] = tally.get(k, 0) + 1
                for a in hit:
                    if tally.get(a[4], 0) >= a[5] * len(rs):
                        row.append(a[4] + (255,))
                        break
                else:
                    hit = []
                if hit:
                    continue
            rs.sort(), gs.sort(), bs.sort()
            m = len(rs) // 2
            c = (rs[m], gs[m], bs[m])
            if sat != 1:
                c = saturate(c, sat)
            row.append(snap(c, extra) + (255,))
        out.append(row)
    for spec in fix.get("clear", []):
        clear(out, spec)
    for spec in fix.get("paint", []):
        paint(out, spec)
    return out


def read_accepted(brief):
    """accepted.txt: (race, class, sex) -> (sheet folder, cell)."""
    took, problems = {}, []
    with open(os.path.join(brief, "accepted.txt"), encoding="utf-8") as f:
        for n, line in enumerate(f, 1):
            if line.startswith("#") or not line.strip():
                continue
            race, klass, sex, folder, at = line.split()[:5]
            key = (race, klass, sex)
            if key in took:
                problems.append("accepted.txt %d: a second %s %s %s"
                                % ((n,) + key))
            took[key] = (folder, int(at))
    return took, problems


def cut_sheet(job):
    """Every cell wanted from one sheet: (folder, [cell...]) -> {cell: figure}."""
    folder, wanted = job
    sheet = [n for n in os.listdir(folder) if n.startswith("sheet.")]
    if len(sheet) != 1:
        raise SystemExit("build-portraits: want one sheet.* in %s" % folder)
    width, height, data = load(os.path.join(folder, sheet[0]))
    data, _ = ungrid(width, height, data)
    cells, fixes = read_cells(folder), read_fixes(folder)
    return {n: cut(width, data, (n - 1) % 4, (n - 1) // 4,
                   fixes_for(n, fixes, cells)) for n in wanted}


def cut_all(brief):
    """Cut source.png from the brief, and copy its recipe."""
    races = gamedata_names("p_race.txt")
    classes = gamedata_names("class.txt")
    took, problems = read_accepted(brief)
    for race in races:
        for klass in classes:
            for sex in "MF":
                if (race, klass, sex) not in took:
                    problems.append("accepted.txt has no %s %s %s"
                                    % (race, klass, sex))
    for race, klass, sex in took:
        if race not in races or klass not in classes:
            problems.append("accepted.txt has a %s %s, which the game does "
                            "not" % (race, klass))
    if problems:
        return problems

    jobs = {}
    for folder, n in took.values():
        jobs.setdefault(folder, set()).add(n)
    jobs = sorted((os.path.join(brief, f), sorted(ns)) for f, ns in jobs.items())
    try:
        from multiprocessing import Pool
        with Pool() as pool:
            done = pool.map(cut_sheet, jobs)
    except (ImportError, OSError):
        done = [cut_sheet(job) for job in jobs]
    figures = {}
    for (folder, _), figs in zip(jobs, done):
        for n, fig in figs.items():
            figures[(os.path.relpath(folder, brief), n)] = fig

    source = [[(0, 0, 0, 0)] * (len(races) * SIZE)
              for _ in range(2 * len(classes) * SIZE)]
    for (race, klass, sex), (folder, n) in took.items():
        fig = figures[(os.path.normpath(folder), n)]
        col, row = races.index(race), 2 * classes.index(klass) + (sex == "F")
        for y in range(SIZE):
            source[row * SIZE + y][col * SIZE:(col + 1) * SIZE] = fig[y]
    write_png(os.path.join(HERE, "source.png"), source)
    with open(os.path.join(HERE, "source.txt"), "w", encoding="utf-8",
              newline="\n") as f:
        f.write("# source.txt -- what is where in source.png: a race is a "
                "column and a class\n# two rows, a man's and a woman's, "
                "64x64 each, in this order.\n"
                "# GENERATED by build-portraits.py --cut.  Do not edit.\n")
        f.writelines("race\t%s\n" % r for r in races)
        f.writelines("class\t%s\n" % c for c in classes)
    shutil.copyfile(os.path.join(brief, "accepted.txt"),
                    os.path.join(HERE, "recipe.txt"))
    with open(os.path.join(HERE, "fixes.txt"), "w", encoding="utf-8",
              newline="\n") as f:
        f.write("# fixes.txt -- the clean-ups each sheet's cells were cut "
                "with, copied from\n# the brief's batch folders by "
                "build-portraits.py --cut.  A line is a sheet,\n# a cell and "
                "its fixes; the brief's tools/cut.py says what each does.\n")
        for folder, _ in jobs:
            path = os.path.join(folder, "fixes.txt")
            if not os.path.exists(path):
                continue
            name = os.path.relpath(folder, brief)
            with open(path, encoding="utf-8") as g:
                for line in g:
                    if line.strip() and not line.startswith("#"):
                        f.write("%-13s %s" % (name, line))
    return []


# --- review ------------------------------------------------------------------------

FONT = {
    "A": [".#.", "#.#", "###", "#.#", "#.#"], "B": ["##.", "#.#", "##.", "#.#", "##."],
    "C": [".##", "#..", "#..", "#..", ".##"], "D": ["##.", "#.#", "#.#", "#.#", "##."],
    "E": ["###", "#..", "##.", "#..", "###"], "F": ["###", "#..", "##.", "#..", "#.."],
    "G": [".##", "#..", "#.#", "#.#", ".##"], "H": ["#.#", "#.#", "###", "#.#", "#.#"],
    "I": ["###", ".#.", ".#.", ".#.", "###"], "J": ["..#", "..#", "..#", "#.#", ".#."],
    "K": ["#.#", "#.#", "##.", "#.#", "#.#"], "L": ["#..", "#..", "#..", "#..", "###"],
    "M": ["#.#", "###", "###", "#.#", "#.#"], "N": ["##.", "#.#", "#.#", "#.#", "#.#"],
    "O": [".#.", "#.#", "#.#", "#.#", ".#."], "P": ["##.", "#.#", "##.", "#..", "#.."],
    "Q": [".#.", "#.#", "#.#", "##.", ".##"], "R": ["##.", "#.#", "##.", "#.#", "#.#"],
    "S": [".##", "#..", ".#.", "..#", "##."], "T": ["###", ".#.", ".#.", ".#.", ".#."],
    "U": ["#.#", "#.#", "#.#", "#.#", "###"], "V": ["#.#", "#.#", "#.#", "#.#", ".#."],
    "W": ["#.#", "#.#", "###", "###", "#.#"], "X": ["#.#", "#.#", ".#.", "#.#", "#.#"],
    "Y": ["#.#", "#.#", ".#.", ".#.", ".#."], "Z": ["###", "..#", ".#.", "#..", "###"],
    "-": ["...", "...", "###", "...", "..."], " ": ["...", "...", "...", "...", "..."],
}


def review(outdir, sheet, size, races_a_sheet=7):
    """Sheets to look at, at the size a window shows them: races down,
    classes across with a man and a woman each."""
    races = gamedata_names("p_race.txt")
    classes = gamedata_names("class.txt")
    grounds = read_backdrops()
    label = 2
    gap = 4
    left = 12 * 4 * label
    for half, cls in enumerate((classes[:7], classes[7:])):
        for g in range(0, len(races), races_a_sheet):
            group = races[g:g + races_a_sheet]
            cw = 2 * size + gap * 3
            W = left + len(cls) * cw
            H = 7 * label + 6 + len(group) * (size + gap)
            img = [[(32, 32, 36, 255)] * W for _ in range(H)]

            def text(s, x, y, colour=(232, 232, 232, 255)):
                for k, ch in enumerate(s.upper()):
                    for gy, line in enumerate(FONT.get(ch, FONT[" "])):
                        for gx, bit in enumerate(line):
                            if bit != "#":
                                continue
                            for sy in range(label):
                                for sx in range(label):
                                    px = x + k * 4 * label + gx * label + sx
                                    py = y + gy * label + sy
                                    if 0 <= px < W and 0 <= py < H:
                                        img[py][px] = colour

            for ci, class_name in enumerate(cls):
                text(class_name[:12], left + ci * cw, 2)
            for ri, race_name in enumerate(group):
                y0 = 7 * label + 6 + ri * (size + gap)
                text(race_name, 2, y0 + size // 2 - 5)
                for ci, class_name in enumerate(cls):
                    ground = backdrop(class_name, size, grounds)
                    row = 2 * classes.index(class_name)
                    for si in range(2):
                        p = over(cell(sheet, races.index(race_name), row + si,
                                      size), ground)
                        x0 = left + ci * cw + si * (size + gap)
                        for y in range(size):
                            img[y0 + y][x0:x0 + size] = p[y]
            write_png(os.path.join(outdir, "review-%d-%d-%d.png" % (
                size, g // races_a_sheet + 1, half + 1)), img)


def main(argv):
    if "--cut" in argv:
        at = argv.index("--cut")
        brief = argv[at + 1] if at + 1 < len(argv) else BRIEF
        problems = cut_all(brief)
        if problems:
            for p in problems:
                print("build-portraits: %s" % p, file=sys.stderr)
            return 1

    problems, sheets = build()
    if problems:
        for p in problems:
            print("build-portraits: %s" % p, file=sys.stderr)
        return 1

    if "--check" in argv:
        stale = []
        for name, want in sheets.items():
            path = os.path.join(OUT, name)
            try:
                if name.endswith(".png"):
                    if read_png(path) != want:
                        stale.append(path)
                else:
                    with open(path, encoding="utf-8") as f:
                        if f.read() != want:
                            stale.append(path)
            except (OSError, ValueError) as e:
                stale.append("%s (%s)" % (path, e))
        for path in stale:
            print("build-portraits: %s is not what source.png makes; run "
                  "scripts/portraits/build-portraits.py" % path, file=sys.stderr)
        return 1 if stale else 0

    os.makedirs(OUT, exist_ok=True)
    for name, want in sheets.items():
        path = os.path.join(OUT, name)
        if name.endswith(".png"):
            write_png(path, want)
        else:
            with open(path, "w", encoding="utf-8", newline="\n") as f:
                f.write(want)
    if "--review" in argv:
        where = argv[argv.index("--review") + 1]
        os.makedirs(where, exist_ok=True)
        review(where, sheets["portraits.png"], SIZE)
        review(where, sheets["portraits-32.png"], SMALL)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
