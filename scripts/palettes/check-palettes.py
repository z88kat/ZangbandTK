#!/usr/bin/env python3
"""
Check the Display page's palettes: lib/tcl/palettes.txt.

    scripts/palettes/check-palettes.py            check, and fail on a problem
    scripts/palettes/check-palettes.py --report   every number, for tuning one

A palette is the game's twenty-eight named colours with new values.  The game
leans on a few of them for meaning -- the character's hit points are green,
then yellow, then red; a monster's health bar runs green, yellow, orange, red;
the status line marks good effects green and bad ones red -- and a palette
that loses one of those differences loses information, not just looks.  So a
palette here has to pass, and the measurements are the ones accessibility work
uses rather than taste:

  contrast   WCAG 2 contrast against the black the panes are drawn on: 4.5:1
             for the high-contrast palette, 3:1 for the others.  Dark is
             exempt: it is black by meaning.
  meaning    the pairs that carry meaning, kept at least a CIEDE2000
             difference of 12 apart -- for normal vision in every palette,
             and in the colour-blind palette under simulated protanopia,
             deuteranopia and tritanopia as well, using the Machado, Oliveira
             and Fernandes (2009) model at full severity.
  distinct   no two colours the game keeps 12 or more apart brought closer
             than 8, so a palette does not merge what the game separates.

The game's own palette is measured too, and reported rather than failed: it is
the game's, and the colour-blind palette exists because the game's does not
pass for a colour-blind player.  --report shows by how much.
"""

import itertools
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
PALETTES = os.path.join(ROOT, "lib", "tcl", "palettes.txt")

# The game's palette, from src/z-color.c, to measure against.
GAME = {
    "Dark": "#000000", "White": "#ffffff", "Slate": "#808080",
    "Orange": "#ff8000", "Red": "#c00000", "Green": "#008040",
    "Blue": "#0040ff", "Umber": "#804000", "Light Dark": "#606060",
    "Light Slate": "#c0c0c0", "Light Purple": "#ff00ff", "Yellow": "#ffff00",
    "Light Red": "#ff4040", "Light Green": "#00ff00", "Light Blue": "#00ffff",
    "Light Umber": "#c08040", "Purple": "#900090", "Violet": "#9020ff",
    "Teal": "#00a0a0", "Mud": "#6c6c30", "Light Yellow": "#ffff90",
    "Magenta-Pink": "#ff00a0", "Light Teal": "#20ffdc",
    "Light Violet": "#b8a8ff", "Light Pink": "#ff8080",
    "Mustard": "#b4b400", "Blue Slate": "#a0c0d0",
    "Deep Light Blue": "#00b0ff",
}

EXEMPT = {"Dark"}

# The pairs the game gives a meaning to the difference between.
MEANING = [
    ("Light Green", "Yellow"),      # hit points: whole, then hurt
    ("Yellow", "Red"),              # hurt, then in danger
    ("Light Green", "Red"),
    ("Light Green", "Light Red"),   # good and bad effects, messages
    ("Green", "Red"),
    ("Green", "Light Red"),
    ("Yellow", "Orange"),           # a monster's health bar
    ("Orange", "Light Red"),
    ("Orange", "Red"),
    ("Light Red", "Red"),
    ("Light Green", "Orange"),
]

THRESHOLD = 12.0
DISTINCT = 8.0

# Machado, Oliveira & Fernandes (2009), severity 1.0, on linear RGB.
CVD = {
    "protanopia": [[0.152286, 1.052583, -0.204868],
                   [0.114503, 0.786281, 0.099216],
                   [-0.003882, -0.048116, 1.051998]],
    "deuteranopia": [[0.367322, 0.860646, -0.227968],
                     [0.280085, 0.672501, 0.047413],
                     [-0.011820, 0.042940, 0.968881]],
    "tritanopia": [[1.255528, -0.076749, -0.178779],
                   [-0.078411, 0.930809, 0.147602],
                   [0.004733, 0.691367, 0.303900]],
}


# --- colour science ------------------------------------------------------------

def hex_rgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4))


def to_linear(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def to_srgb(c):
    c = min(1.0, max(0.0, c))
    return 12.92 * c if c <= 0.0031308 else 1.055 * c ** (1 / 2.4) - 0.055


def luminance(rgb):
    r, g, b = (to_linear(c) for c in rgb)
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def contrast_on_black(rgb):
    return (luminance(rgb) + 0.05) / 0.05


def simulate(rgb, matrix):
    lin = [to_linear(c) for c in rgb]
    out = [sum(matrix[i][j] * lin[j] for j in range(3)) for i in range(3)]
    return tuple(to_srgb(c) for c in out)


def lab(rgb):
    r, g, b = (to_linear(c) for c in rgb)
    x = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.95047
    y = (0.2126 * r + 0.7152 * g + 0.0722 * b) / 1.00000
    z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.08883

    def f(t):
        return t ** (1 / 3) if t > 216 / 24389 else (24389 / 27 * t + 16) / 116

    fx, fy, fz = f(x), f(y), f(z)
    return 116 * fy - 16, 500 * (fx - fy), 200 * (fy - fz)


def ciede2000(lab1, lab2):
    from math import atan2, cos, degrees, exp, radians, sin, sqrt
    L1, a1, b1 = lab1
    L2, a2, b2 = lab2
    C1, C2 = sqrt(a1 * a1 + b1 * b1), sqrt(a2 * a2 + b2 * b2)
    Cm = (C1 + C2) / 2
    G = 0.5 * (1 - sqrt(Cm ** 7 / (Cm ** 7 + 25 ** 7)))
    a1p, a2p = (1 + G) * a1, (1 + G) * a2
    C1p, C2p = sqrt(a1p * a1p + b1 * b1), sqrt(a2p * a2p + b2 * b2)
    h1p = degrees(atan2(b1, a1p)) % 360 if C1p else 0.0
    h2p = degrees(atan2(b2, a2p)) % 360 if C2p else 0.0
    dLp = L2 - L1
    dCp = C2p - C1p
    if C1p * C2p == 0:
        dhp = 0.0
    elif abs(h2p - h1p) <= 180:
        dhp = h2p - h1p
    elif h2p - h1p > 180:
        dhp = h2p - h1p - 360
    else:
        dhp = h2p - h1p + 360
    dHp = 2 * sqrt(C1p * C2p) * sin(radians(dhp / 2))
    Lpm = (L1 + L2) / 2
    Cpm = (C1p + C2p) / 2
    if C1p * C2p == 0:
        hpm = h1p + h2p
    elif abs(h1p - h2p) <= 180:
        hpm = (h1p + h2p) / 2
    elif h1p + h2p < 360:
        hpm = (h1p + h2p + 360) / 2
    else:
        hpm = (h1p + h2p - 360) / 2
    T = (1 - 0.17 * cos(radians(hpm - 30)) + 0.24 * cos(radians(2 * hpm))
         + 0.32 * cos(radians(3 * hpm + 6)) - 0.20 * cos(radians(4 * hpm - 63)))
    dtheta = 30 * exp(-(((hpm - 275) / 25) ** 2))
    Rc = 2 * sqrt(Cpm ** 7 / (Cpm ** 7 + 25 ** 7))
    Sl = 1 + (0.015 * (Lpm - 50) ** 2) / sqrt(20 + (Lpm - 50) ** 2)
    Sc = 1 + 0.045 * Cpm
    Sh = 1 + 0.015 * Cpm * T
    Rt = -sin(radians(2 * dtheta)) * Rc
    return sqrt((dLp / Sl) ** 2 + (dCp / Sc) ** 2 + (dHp / Sh) ** 2
                + Rt * (dCp / Sc) * (dHp / Sh))


def difference(c1, c2, vision=None):
    rgb1, rgb2 = hex_rgb(c1), hex_rgb(c2)
    if vision:
        rgb1, rgb2 = simulate(rgb1, CVD[vision]), simulate(rgb2, CVD[vision])
    return ciede2000(lab(rgb1), lab(rgb2))


# --- the palettes ---------------------------------------------------------------

def read_palettes(path=PALETTES):
    """[(key, label, {colour: hex}, {role: hex})] in the file's order."""
    palettes = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            fields = line.split("\t")
            if fields[0] == "palette":
                palettes.append((fields[1], fields[2], {}, {}))
            elif fields[0] == "colour":
                palettes[-1][2][fields[1]] = fields[2]
            elif fields[0] == "role":
                palettes[-1][3][fields[1]] = fields[2]
    return palettes


def check(key, label, colours, strict, report):
    problems = []
    full = dict(GAME, **colours)
    unknown = set(colours) - set(GAME)
    for name in sorted(unknown):
        problems.append("%s: no game colour called %s" % (key, name))

    floor = 4.5 if key == "contrast" else 3.0
    for name, value in full.items():
        if name in EXEMPT:
            continue
        ratio = contrast_on_black(hex_rgb(value))
        if report:
            print("  %-16s %s  contrast %5.2f" % (name, value, ratio))
        if ratio < floor:
            problems.append("%s: %s %s has contrast %.2f on black, under %.1f"
                            % (key, name, value, ratio, floor))

    for a, b in itertools.combinations(sorted(full), 2):
        if difference(GAME[a], GAME[b]) < THRESHOLD:
            continue
        d = difference(full[a], full[b])
        if d < DISTINCT:
            problems.append("%s: %s and %s are %.1f apart, which the game "
                            "keeps apart" % (key, a, b, d))

    visions = [None] + (list(CVD) if key == "colourblind" else [])
    for vision in visions:
        for a, b in MEANING:
            d = difference(full[a], full[b], vision)
            if report:
                print("  %-12s %-12s / %-12s  dE00 %5.1f"
                      % (vision or "normal", a, b, d))
            if d < THRESHOLD:
                problems.append("%s: %s and %s are %.1f apart%s, under %.0f"
                                % (key, a, b, d,
                                   " for " + vision if vision else "",
                                   THRESHOLD))
    return problems if strict else []


def main(argv):
    report = "--report" in argv
    problems = []
    if report:
        print("game (measured, not checked)")
        check("game", "The game's", {}, False, True)
        for vision in CVD:
            worst = min(difference(GAME[a], GAME[b], vision) for a, b in MEANING)
            print("  worst meaning pair for %s: %.1f" % (vision, worst))
    for key, label, colours, roles in read_palettes():
        if report:
            print("%s (%s)" % (key, label))
        problems += check(key, label, colours, True, report)
        for role in ("good", "fair", "poor", "mana", "food", "exp"):
            if role not in roles:
                problems.append("%s: no colour for the %s role" % (key, role))
        if key == "colourblind":
            for vision in [None] + list(CVD):
                for a, b in (("good", "fair"), ("fair", "poor"), ("good", "poor")):
                    d = difference(roles[a], roles[b], vision)
                    if report:
                        print("  %-12s role %s / %s  dE00 %5.1f"
                              % (vision or "normal", a, b, d))
                    if d < THRESHOLD:
                        problems.append("%s: the %s and %s roles are %.1f apart"
                                        "%s" % (key, a, b, d,
                                                " for " + vision if vision else ""))
    for p in problems:
        print("check-palettes: %s" % p, file=sys.stderr)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
