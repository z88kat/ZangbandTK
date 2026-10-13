#!/usr/bin/env python3
"""
What colour blindness costs the Neon tiles, and the search for colours that
cost less.

    scripts/tiles/neon-colourblind.py                  report both tables
    scripts/tiles/neon-colourblind.py --search N [--seed S] [--from cb]
                                                       search, print cbhue lines

On a map of tiles, two monsters of the same kind share a silhouette and are
told apart only by their colour: a fire hound from a cold hound, a baby green
dragon from a baby red one.  The measure is how many of those pairs of colours
-- 3,312 of them across every kind in lib/gamedata -- a colour-blind player can
no longer tell apart, under the Machado, Oliveira and Fernandes (2009)
simulation of protanopia, deuteranopia and tritanopia at full severity: a pair
is lost when it is at least 10 CIEDE2000 apart for normal vision and under 10
for the simulated one.  The report also gives the closest of the pairs the game
gives a meaning to -- hit points green to yellow to red, good and bad effects,
a monster's health -- under any of the three.

The search is how palette.txt's cbhue lines were found, kept so they can be
found again: hill climbing in OKLCH from Neon's own table (or, with --from cb,
from the cbhue table), minimising the pairs lost, under rules that keep the
result Neon and keep it working --

  * each hue near its own angle, 25 degrees either way; green 45, because
    leaning it towards blue-green is what parts it from red
  * the hues still in spectrum order, red to magenta, at least 10 degrees
    apart, because a shimmering monster walks them as a rainbow
  * each tone's lightness near Neon's (pale 0.10, neon 0.10, dim 0.14,
    shadow 0.08 in OKLab), in order, and with at least 70% of Neon's chroma,
    so nothing drains to grey
  * a colour's neighbouring tones at least 80% as far apart as Neon's, up to
    10, because the lighting steps along them
  * grey grey: no more coloured than Neon's, and as cool
  * every slot over Neon's own contrast floor for what it draws
  * no pair normal vision could tell apart in Neon lost, and the meaning pairs
    at least 12 apart under all three simulations

The result is checked again, without the search, by build-neon.py --check.
"""

import importlib.util
import itertools
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))


def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


neon = _load('build_neon', os.path.join(HERE, 'build-neon.py'))
cp = _load('check_palettes', os.path.join(ROOT, 'scripts', 'palettes',
                                          'check-palettes.py'))

TONES = neon.TONES
HUES_TABLE, ATTRS = neon.read_palette()[:2]
HUES = list(HUES_TABLE)
VISIONS = list(cp.CVD)
LOST = 10.0
MEANING = [('G', 'y'), ('y', 'r'), ('G', 'r'), ('G', 'R'), ('g', 'r'),
           ('g', 'R'), ('y', 'o'), ('o', 'R'), ('o', 'r'), ('R', 'r'),
           ('G', 'o')]


# --- the measure -----------------------------------------------------------------

def _same_kind_pairs():
    """{(code, code): how many kinds have both}, for codes Neon draws apart."""
    by_base = {}
    for filename in neon.MONSTERS:
        for m in neon.read_records(filename, ('base', 'color')):
            if 'base' in m and 'color' in m:
                by_base.setdefault(m['base'], set()).add(m['color'][0])
    weights = {}
    for codes in by_base.values():
        for a, b in itertools.combinations(sorted(codes), 2):
            if ATTRS.get(a) != ATTRS.get(b):
                weights[(a, b)] = weights.get((a, b), 0) + 1
    return weights


WEIGHTS = _same_kind_pairs()


def colour(table, code):
    hue, tone = ATTRS.get(code, ('grey', 'neon'))
    return '#' + table[hue][tone]


def measure(table):
    """({vision: pairs lost}, closest meaning pair (d, vision, a, b))."""
    lost = {v: 0 for v in VISIONS}
    for (a, b), w in WEIGHTS.items():
        ca, cb = colour(table, a), colour(table, b)
        if cp.difference(ca, cb) < LOST:
            continue
        for v in VISIONS:
            if cp.difference(ca, cb, v) < LOST:
                lost[v] += w
    worst = min((cp.difference(colour(table, a), colour(table, b), v),
                 v or 'normal', a, b)
                for v in [None] + VISIONS for a, b in MEANING)
    return lost, worst


# --- OKLCH -------------------------------------------------------------------------

def to_lch(hexs):
    r, g, b = (neon._lin(v) for v in neon.rgb(hexs.lstrip('#')))
    l = 0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b
    m = 0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b
    s = 0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b
    l, m, s = (math.copysign(abs(x) ** (1 / 3), x) for x in (l, m, s))
    L = 0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s
    a = 1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s
    bb = 0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s
    return L, math.hypot(a, bb), math.degrees(math.atan2(bb, a)) % 360


def from_lch(L, C, h):
    """OKLCH to sRGB hex, the chroma reduced until it can be displayed."""
    for _ in range(40):
        a, b = C * math.cos(math.radians(h)), C * math.sin(math.radians(h))
        l = (L + 0.3963377774 * a + 0.2158037573 * b) ** 3
        m = (L - 0.1055613458 * a - 0.0638541728 * b) ** 3
        s = (L - 0.0894841775 * a - 1.2914855480 * b) ** 3
        rgb = (4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
               -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
               -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s)
        if all(-1e-4 <= v <= 1 + 1e-4 for v in rgb):
            break
        C *= 0.93
    out = []
    for v in rgb:
        v = min(1.0, max(0.0, v))
        v = 12.92 * v if v <= 0.0031308 else 1.055 * v ** (1 / 2.4) - 0.055
        out.append(int(round(v * 255)))
    return '%02X%02X%02X' % tuple(out)


# --- the rules ---------------------------------------------------------------------

ORIG = {h: {t: to_lch(HUES_TABLE[h][t]) for t in TONES} for h in HUES}
ANGLE_ROOM = dict({h: 25.0 for h in HUES}, green=45.0)
BAND = {'pale': 0.10, 'neon': 0.10, 'dim': 0.14, 'shadow': 0.08}
TONE_GAP = {h: [cp.difference('#' + HUES_TABLE[h][TONES[i]],
                              '#' + HUES_TABLE[h][TONES[i + 1]])
                for i in range(3)] for h in HUES}
SPECTRUM = [h for h in HUES if h != 'grey']


def _floors():
    """Neon's contrast floor for each slot, from what the slot draws."""
    _, _, used, _, _ = neon.build()
    slot_of = {HUES_TABLE[h][t].upper(): (h, t) for h in HUES for t in TONES}
    floors = {}
    for hexs, kinds in used.items():
        slot = slot_of.get(hexs.upper())
        if not slot:
            continue
        if kinds & {'monster', 'object', 'flavour', 'trap', 'effect'}:
            floors[slot] = max(floors.get(slot, 0), 3.0)
        elif 'terrain' in kinds:
            floors[slot] = max(floors.get(slot, 0), 2.5)
    return floors


def table_of(params):
    return {h: {t: from_lch(params[h][1][i], params[h][2][i], params[h][0])
                for i, t in enumerate(TONES)} for h in HUES}


def params_of(table):
    p = {}
    for h in HUES:
        drawn = [to_lch(table[h][t]) for t in TONES]
        p[h] = [drawn[1][2], [d[0] for d in drawn], [d[1] for d in drawn]]
    return p


def legal(params):
    table = table_of(params)
    for h in HUES:
        drawn = [to_lch(table[h][t]) for t in TONES]
        for i, t in enumerate(TONES):
            L, C, ang = drawn[i]
            L0, C0, ang0 = ORIG[h][t]
            turn = abs((ang - ang0 + 180) % 360 - 180)
            if abs(L - L0) > BAND[t]:
                return False
            if h == 'grey':
                if C > C0 + 0.012 or (C > 0.015 and turn > 20):
                    return False
            elif C < 0.70 * C0 or turn > ANGLE_ROOM[h]:
                return False
        if not all(drawn[i][0] > drawn[i + 1][0] + 0.01 for i in range(3)):
            return False
        for i in range(3):
            gap = cp.difference('#' + table[h][TONES[i]],
                                '#' + table[h][TONES[i + 1]])
            if gap < min(0.8 * TONE_GAP[h][i], 10.0):
                return False
    base = params['red'][0]
    rel = [(params[h][0] - base) % 360 for h in SPECTRUM]
    return (all(rel[i + 1] - rel[i] >= 10 for i in range(len(rel) - 1))
            and rel[-1] <= 350)


def cost(table, floors):
    lost, _ = measure(table)
    total = sum(lost.values())
    for (a, b), w in WEIGHTS.items():
        if (cp.difference(colour(HUES_TABLE, a), colour(HUES_TABLE, b)) >= LOST
                and cp.difference(colour(table, a), colour(table, b)) < LOST):
            total += 50 * w
    for a, b in MEANING:
        for v in [None] + VISIONS:
            d = cp.difference(colour(table, a), colour(table, b), v)
            if d < 12:
                total += 40 * (12 - d)
    for (h, t), floor in floors.items():
        c = neon.contrast_on_black(table[h][t])
        if c < floor:
            total += 200 * (floor - c)
    return total


def search(iterations, seed, first):
    rng = random.Random(seed)
    floors = _floors()
    p = params_of(first)
    best = cost(table_of(p), floors) if legal(p) else float('inf')
    for _ in range(iterations):
        q = {h: [v[0], list(v[1]), list(v[2])] for h, v in p.items()}
        h = rng.choice(HUES)
        pick = rng.random()
        if pick < 0.25:
            q[h][0] = (q[h][0] + rng.gauss(0, 6)) % 360
        elif pick < 0.75:
            q[h][1][rng.randrange(4)] += rng.gauss(0, 0.03)
        else:
            i = rng.randrange(4)
            q[h][2][i] = max(0.0, q[h][2][i] + rng.gauss(0, 0.02))
        if not legal(q):
            continue
        c = cost(table_of(q), floors)
        if c <= best:
            p, best = q, c
    return table_of(p)


def main(argv):
    cb = neon.read_cb_hues()
    if '--search' in argv:
        n = int(argv[argv.index('--search') + 1])
        seed = int(argv[argv.index('--seed') + 1]) if '--seed' in argv else 1
        first = cb if ('--from' in argv and cb) else HUES_TABLE
        table = search(n, seed, first)
        lost, worst = measure(table)
        print('# lost %s, closest meaning pair %.1f' % (lost, worst[0]))
        for h in HUES:
            print('cbhue:%s:%s' % (h, ':'.join(table[h][t] for t in TONES)))
        return 0
    print('%d same-kind colour pairs' % sum(WEIGHTS.values()))
    for label, table in (('Neon', HUES_TABLE), ('colour-blind', cb)):
        if not table:
            continue
        lost, worst = measure(table)
        print('%-13s lost: %s; closest meaning pair %.1f (%s, %s/%s)'
              % (label, ', '.join('%s %d' % (v, n) for v, n in lost.items()),
                 worst[0], worst[1], worst[2], worst[3]))
    if cb:
        print('rules hold for the colour-blind table: %s'
              % legal(params_of(cb)))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
