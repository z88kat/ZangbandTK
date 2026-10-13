#!/usr/bin/env python3
"""
Build the character portraits: every race, every class, a man and a woman.

    scripts/portraits/build-portraits.py            write the sheet and index
    scripts/portraits/build-portraits.py --check    fail if they are out of date
    scripts/portraits/build-portraits.py --review DIR
                                                    also write sheets to look at

The Character window, the tombstone and the Character Record show a portrait
of the character, where they used to show its map tile.  Twenty-eight races,
fourteen classes and two sexes make 784, and none of them is drawn: a portrait
is a 32x32 bust composed from small text grids in art.py -- a body for the sex
and the race's build, the race's head and what it adds to one, the class's
clothes, headgear and what it carries -- outlined, and with the figure left on
a transparent ground.  The class's backdrop is a cell of its own, so a window
can set the figure on it or, like the tombstone, on nothing.

The output is lib/tcl/portraits/portraits.png and portraits.txt, the index the
front end reads them by: the races in column order, the classes in row order,
a man's row then a woman's for each class, and the backdrops on a last row.
Both are committed, so a build of the game needs no Python; --check, which the
Tk workflow runs, fails when they are not what this would make.

The sheet is a build artifact: change art.py and run this, never the PNG.  It
is deterministic, with no randomness anywhere.  The game's races and classes
are read from lib/gamedata, and a race or class without art, or art for one the
game does not have, is an error rather than a gap.

The look was agreed from a sample of ten on 13 October 2026; see the diary.
"""

import os
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)

import art  # noqa: E402

SIZE = 32
OUT = os.path.join(ROOT, "lib", "tcl", "portraits")
OUTLINE = "#120e14"


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
        for i in range(stride):
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


def rgb(spec):
    """A colour from '#rrggbb', or ('#rrggbb', alpha)."""
    alpha = 255
    if isinstance(spec, tuple):
        spec, alpha = spec
    h = spec.lstrip("#")
    return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16), alpha)


# --- what the game has -------------------------------------------------------------

def gamedata_names(filename):
    names = []
    with open(os.path.join(ROOT, "lib", "gamedata", filename),
              encoding="utf-8") as f:
        for line in f:
            if line.startswith("name:"):
                names.append(line[5:].strip())
    return names


def check_art():
    """Every grid rectangular, and art for exactly the game's races and classes."""
    problems = []

    def grids(where, value):
        if (isinstance(value, tuple) and len(value) == 3
                and isinstance(value[2], list)):
            widths = {len(row) for row in value[2]}
            if len(widths) != 1:
                problems.append("%s: rows of widths %s" % (where, sorted(widths)))
        elif isinstance(value, dict):
            for k, v in value.items():
                grids("%s.%s" % (where, k), v)

    for name in dir(art):
        if not name.startswith("_"):
            grids(name, getattr(art, name))

    for kind, have, want in (("race", art.RACES, gamedata_names("p_race.txt")),
                             ("class", art.CLASSES, gamedata_names("class.txt"))):
        for name in want:
            if name not in have:
                problems.append("no art for the %s %s" % (kind, name))
        for name in have:
            if name not in want:
                problems.append("art for a %s the game does not have: %s"
                                % (kind, name))
    return problems


# --- one portrait ----------------------------------------------------------------

# Shoulder half-widths a row, from the top of the torso down.
TORSO = {
    ("M", "M"): [5, 9, 11, 12],
    ("M", "F"): [4, 7, 9, 10],
    ("S", "M"): [4, 7, 9, 10],
    ("S", "F"): [3, 6, 8, 9],
    ("L", "M"): [6, 11, 13, 14],
    ("L", "F"): [5, 9, 11, 12],
}

# Where the head's top is, by height.
HEAD_Y = {"S": 7, "M": 5, "L": 4}

# Ears that stand out of the hair rather than lie under it.
PROUD_EARS = {"half", "point", "long", "droop", "dog"}


class Figure:
    def __init__(self):
        self.px = [[None] * SIZE for _ in range(SIZE)]

    def put(self, x, y, colour):
        if 0 <= x < SIZE and 0 <= y < SIZE:
            self.px[y][x] = colour

    def draw(self, layer, pal, at=(0, 0)):
        dx, dy, rows = layer
        for j, row in enumerate(rows):
            for i, ch in enumerate(row):
                if ch in ". ":
                    continue
                if ch not in pal:
                    raise KeyError("no colour for %r" % ch)
                self.put(at[0] + dx + i, at[1] + dy + j, pal[ch])


def palettes(r, c, sex):
    """The race's colours over the class's, and the class's over the race's."""
    rp = dict(r["pal"])
    if sex == "F":
        rp.update(r.get("palF", {}))
    # A head with no skin -- a Klackon's -- lends its carapace to whatever
    # asks for skin: a hood's shadow, a Monk's fist.
    for skin, other in (("s", "a"), ("S", "A"), ("k", "K")):
        if skin not in rp and other in rp:
            rp[skin] = rp[other]
    rp.setdefault("b", rp.get("H", rp.get("S")))
    cp = c["pal"]
    race_pal = {k: rgb(v) for k, v in dict(cp, **rp).items()}
    class_pal = {k: rgb(v) for k, v in dict(rp, **cp).items()}
    return race_pal, class_pal


def compose(race_name, class_name, sex):
    """A portrait's figure: RGBA rows, transparent where there is nothing."""
    r = art.RACES[race_name]
    c = art.CLASSES[class_name]
    rpal, cpal = palettes(r, c, sex)
    fig = Figure()

    height, width = r["build"]
    hx, hy = 10, HEAD_Y[height]
    body = "M" if r.get("same_body") else sex
    halfwidths = TORSO[(width, body)]
    top = hy + 12 + (1 if height != "L" else 0)

    # Behind everything: an aura, wings, a collar, what is carried on the back.
    if c.get("aura"):
        glow = rgb((c["aura"], 150))
        cx, cy = 15.5, hy + 6.5
        for y in range(SIZE):
            for x in range(SIZE):
                d = ((x - cx) ** 2 + (y - cy) ** 2) ** 0.5
                # Dotted, so it shimmers rather than encloses.
                if 8.6 <= d < 9.8 and y < top + 2 and (x + y) % 2 == 0:
                    fig.put(x, y, glow)
    if r.get("back") == "wings":
        fig.draw(art.WINGS, rpal, (1, hy))
    elif r.get("back") == "dragon":
        fig.draw(art.DRAGON_WINGS, rpal, (1, hy))
    if c.get("collar"):
        fig.draw(art.COLLAR, cpal, (0, top - 6))
    for layer in c.get("back", []):
        fig.draw(layer, cpal)

    # The torso, row by row, then the class's clothes on it.
    for n, y in enumerate(range(top, SIZE)):
        hw = halfwidths[min(n, len(halfwidths) - 1)]
        x0, x1 = 16 - hw, 15 + hw
        for x in range(x0, x1 + 1):
            fig.put(x, y, cpal["C" if x == x0 or x >= x1 - 1 else "c"])
    outfit(fig, c, cpal, top, halfwidths[-1])
    if r.get("jewel"):
        # An Amberite's jewel, on a chain at the throat.
        for x, y in ((13, top), (14, top + 1), (18, top), (17, top + 1)):
            fig.put(x, y, rpal["Z"])
        for x in (15, 16):
            for y in (top + 1, top + 2):
                fig.put(x, y, rpal["J"])

    # The neck, then the head.
    for y in range(hy + 11, top + 1):
        for x in range(14, 18):
            fig.put(x, y, rpal["s" if x in (15, 16) else "S"])
    if c["outfit"] in ("robe", "leather", "tabard", "gi", "halfplate"):
        # A collar, so the neck goes into the clothes rather than through them.
        for x in range(12, 20):
            fig.put(x, top, cpal["d"])
    head = r["head"]
    if isinstance(head, dict):
        head = head[sex]
    elif sex == "F" and head in ("round", "square"):
        head = "roundf"
    fig.draw(art.HEADS[head], rpal, (hx, hy))

    def feature(layer):
        fig.draw(layer, rpal, (hx, hy))

    ears = r.get("ears")
    if ears and ears not in PROUD_EARS:
        feature(art.EARS[ears])
    if r.get("nose"):
        feature(art.NOSE)
    if r.get("tusks"):
        feature(art.TUSKS)
    if r.get("fangs"):
        feature(art.FANGS)
    if r.get("teeth"):
        feature(art.TEETH)
    if r.get("stitches"):
        feature(art.STITCHES)
    if r.get("paint"):
        feature(art.PAINT)
    if r.get("tentacles"):
        feature(art.TENTACLES)
    if sex in r.get("beard", ()):
        feature(art.BEARD)
    if sex in r.get("stubble", ()):
        feature(art.STUBBLE)
    if sex in r.get("goatee", ()):
        feature(art.GOATEE)

    style = r["hair"].get(sex)
    if style and not c.get("hides_hair"):
        feature(art.HAIR[style])
    if ears in PROUD_EARS:
        feature(art.EARS[ears])
    if r.get("antennae"):
        feature(art.ANTENNAE)

    if c.get("head"):
        fig.draw(c["head"], cpal, (hx, hy))
    if style and c.get("hides_hair") and style in art.LONG:
        if c.get("spill"):
            feature(c["spill"])
        else:
            # Below a helm, what falls below it.
            dx, dy, rows = art.HAIR[style]
            feature((dx, dy + 7, rows[7:]))

    horned = r.get("horns") or r.get("ram_horns") or r.get("crest")
    if r.get("horns"):
        feature(art.HORNS)
    if r.get("ram_horns"):
        feature(art.RAM_HORNS)
    if r.get("crest"):
        feature(art.CREST)
    if c.get("horns") and not horned:
        fig.draw(c["horns"], cpal, (hx, hy))

    if c.get("item"):
        fig.draw(c["item"], cpal)

    return finish(fig, r)


def outfit(fig, c, pal, top, hw):
    """The class's clothes, on a torso already drawn in its colours."""
    kind = c["outfit"]
    if kind in ("plate", "spiked", "chaos", "tabard", "halfplate"):
        # Pauldrons: a rounded plate on each shoulder.
        for side in (-1, 1):
            cx = 16 + side * (hw - 2) - (1 if side > 0 else 0)
            for dy, span in enumerate([2, 3, 3, 2]):
                for dx in range(-span, span + 1):
                    shade = "q" if dy == 0 and dx < 0 else ("P" if dx > 0 else "p")
                    fig.put(cx + dx, top + 1 + dy, pal[shade])
    if kind in ("plate", "spiked", "chaos", "tabard"):
        # The breastplate's ridge, and its lower edge.
        for y in range(top + 3, SIZE):
            fig.put(15, y, pal["q"])
            fig.put(16, y, pal["P"])
        for x in range(16 - hw + 3, 16 + hw - 3):
            fig.put(x, SIZE - 3, pal["d"])
    if kind == "spiked":
        # One spike to a shoulder, leaning out.
        for side in (-1, 1):
            cx = 16 + side * (hw - 2) - (1 if side > 0 else 0)
            fig.put(cx + side * 2, top - 3, pal["q"])
            fig.put(cx + side * 1, top - 2, pal["q"])
            fig.put(cx + side * 2, top - 2, pal["p"])
            for dx in (-1, 0, 1, 2):
                fig.put(cx + side * dx, top - 1, pal["p" if dx < 2 else "P"])
    if kind == "tabard":
        for y in range(top + 2, SIZE):
            for x in range(12, 20):
                fig.put(x, y, pal["i"])
        for y in range(top + 4, top + 11):
            fig.put(15, y, pal["r"])
            fig.put(16, y, pal["r"])
        for x in range(13, 19):
            fig.put(x, top + 6, pal["r"])
    if kind == "chaos":
        # The Star of Chaos.
        cx, cy = 15, top + 7
        for i in range(-3, 4):
            fig.put(cx + i, cy, pal["d"])
            fig.put(cx, cy + i, pal["d"])
            if abs(i) <= 2:
                fig.put(cx + i, cy + i, pal["d"])
                fig.put(cx + i, cy - i, pal["d"])
        fig.put(cx, cy, pal["q"])
    if kind in ("robe", "halfplate"):
        # A V of trim from the collar, and the robe's front edge below it.
        for n in range(6):
            fig.put(12 + n // 2 + (1 if n % 2 else 0), top + 1 + n, pal["d"])
            fig.put(19 - n // 2 - (1 if n % 2 else 0), top + 1 + n, pal["d"])
        for y in range(top + 6, SIZE):
            fig.put(15, y, pal["d"])
            fig.put(16, y, pal["D"])
    if kind == "leather":
        # A strap across the chest, and a belt.
        for n in range(SIZE - top - 2):
            if 9 + n <= 23:
                fig.put(9 + n, top + 1 + n, pal["d"])
        for x in range(16 - hw + 1, 16 + hw - 1):
            fig.put(x, SIZE - 3, pal["D"])
    if kind == "gi":
        # The wrap, left over right, the undershirt in the V, and the belt.
        for n in range(SIZE - top - 3):
            x = 12 + n // 2
            fig.put(x, top + 1 + n, pal["C"])
            if n < 6:
                for v in range(13, 13 + max(0, 5 - n)):
                    if v > x:
                        fig.put(v, top + 1 + n, pal["i"])
        for x in range(16 - hw + 1, 16 + hw - 1):
            fig.put(x, SIZE - 3, pal["d"])
            fig.put(x, SIZE - 2, pal["D"])
    if c.get("stars"):
        for x, dy in ((7, 6), (22, 4), (11, 10), (20, 11), (9, 14), (24, 9),
                      (13, 5)):
            fig.put(x, top + dy, pal["q"])
    if c.get("cloak"):
        for y in range(top + 1, SIZE):
            fig.put(16 - hw, y, pal["C"])
            fig.put(16 - hw + 1, y, pal["C"])
    if c.get("symbol") == "cross":
        for y in range(top + 6, top + 12):
            fig.put(15, y, pal["i"])
            fig.put(16, y, pal["i"])
        for x in range(13, 19):
            fig.put(x, top + 8, pal["i"])


def finish(fig, r):
    """Outline the figure, and fade a ghost: RGBA, on nothing."""
    line = rgb(r.get("outline", OUTLINE))
    out = []
    for y in range(SIZE):
        row = []
        for x in range(SIZE):
            p = fig.px[y][x]
            if p is None:
                for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                    if (0 <= nx < SIZE and 0 <= ny < SIZE
                            and fig.px[ny][nx] is not None):
                        p = line
                        break
            if p is None:
                row.append((0, 0, 0, 0))
                continue
            alpha = p[3]
            if r.get("ghost"):
                alpha = int(alpha * max(0.15, 0.85 - max(0, y - 20) * 0.07))
            row.append((p[0], p[1], p[2], alpha))
        out.append(row)
    return out


def backdrop(class_name):
    top, bottom = (rgb(v) for v in art.CLASSES[class_name]["backdrop"])
    rows = []
    for y in range(SIZE):
        t = y / (SIZE - 1)
        colour = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
        rows.append([colour + (255,)] * SIZE)
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


# --- the sheet ---------------------------------------------------------------------

def build():
    races = gamedata_names("p_race.txt")
    classes = gamedata_names("class.txt")
    width = len(races) * SIZE
    height = (len(classes) * 2 + 1) * SIZE
    sheet = [[(0, 0, 0, 0)] * width for _ in range(height)]

    def place(cell, col, row):
        for y in range(SIZE):
            sheet[row * SIZE + y][col * SIZE:(col + 1) * SIZE] = cell[y]

    for ci, class_name in enumerate(classes):
        for ri, race_name in enumerate(races):
            for si, sex in enumerate("MF"):
                place(compose(race_name, class_name, sex), ri, ci * 2 + si)
        place(backdrop(class_name), ci, len(classes) * 2)

    index = [
        "# The character portraits: made by scripts/portraits/build-portraits.py",
        "# from scripts/portraits/art.py.  Do not edit; change those and run it.",
        "#",
        "# A race is a column and a class two rows, a man's and a woman's, in the",
        "# order listed here.  The class backdrops are the last row, by class.",
        "size\t%d" % SIZE,
    ]
    index += ["race\t%s" % name for name in races]
    index += ["class\t%s" % name for name in classes]
    return sheet, "\n".join(index) + "\n"


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


def review(outdir, scale=3, races_a_sheet=7):
    """Sheets to look at: races down, classes across with a man and a woman each."""
    races = gamedata_names("p_race.txt")
    classes = gamedata_names("class.txt")
    label = 2
    gap = 4
    left = 12 * 4 * label
    for half, cls in enumerate((classes[:7], classes[7:])):
        for g in range(0, len(races), races_a_sheet):
            group = races[g:g + races_a_sheet]
            cw = 2 * SIZE * scale + gap * 3
            W = left + len(cls) * cw
            H = 7 * label + 6 + len(group) * (SIZE * scale + gap)
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
                y0 = 7 * label + 6 + ri * (SIZE * scale + gap)
                text(race_name, 2, y0 + SIZE * scale // 2 - 5)
                for ci, class_name in enumerate(cls):
                    ground = backdrop(class_name)
                    for si, sex in enumerate("MF"):
                        p = over(compose(race_name, class_name, sex), ground)
                        x0 = left + ci * cw + si * (SIZE * scale + gap)
                        for y in range(SIZE):
                            for x in range(SIZE):
                                for sy in range(scale):
                                    row = img[y0 + y * scale + sy]
                                    for sx in range(scale):
                                        row[x0 + x * scale + sx] = p[y][x]
            write_png(os.path.join(outdir, "review-%d-%d.png" % (
                g // races_a_sheet + 1, half + 1)), img)


def main(argv):
    problems = check_art()
    if problems:
        for p in problems:
            print("build-portraits: %s" % p, file=sys.stderr)
        return 1
    sheet, index = build()
    png = os.path.join(OUT, "portraits.png")
    txt = os.path.join(OUT, "portraits.txt")

    if "--check" in argv:
        stale = []
        try:
            if read_png(png) != sheet:
                stale.append(png)
        except (OSError, ValueError) as e:
            stale.append("%s (%s)" % (png, e))
        try:
            with open(txt, encoding="utf-8") as f:
                if f.read() != index:
                    stale.append(txt)
        except OSError:
            stale.append(txt)
        for path in stale:
            print("build-portraits: %s is not what art.py makes; run "
                  "scripts/portraits/build-portraits.py" % path, file=sys.stderr)
        return 1 if stale else 0

    os.makedirs(OUT, exist_ok=True)
    write_png(png, sheet)
    with open(txt, "w", encoding="utf-8", newline="\n") as f:
        f.write(index)
    if "--review" in argv:
        where = argv[argv.index("--review") + 1]
        os.makedirs(where, exist_ok=True)
        review(where)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
