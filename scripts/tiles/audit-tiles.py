#!/usr/bin/env python3
"""
What each shipped tileset does not draw.

borrow-tiles.py and make-services.py each check what they wrote.  Neither
checks *coverage* -- whether anything in the game is still without a tile --
and that is the question that actually matters, because the answer is invisible
in play until you walk into the thing and find a letter where a picture should
be.  Four categories of gap sat unnoticed for exactly that reason.

    scripts/tiles/audit-tiles.py            the table
    scripts/tiles/audit-tiles.py --list     and what is in each gap

Exit status is 0 when the only things left are the ones left on purpose:
uniques, which render as text until they have art of their own, and the two
traps that are not pictures.  Anything else is a gap and fails.

=== Case ===

The engine compares these names with my_stricmp, and the shipped files do not
agree with the data on case: the prf files say "Filthy street urchin" where
monster.txt says "filthy street urchin".  The first version of this compared
exactly and reported 629 missing monsters in a set that draws all of Angband.
Everything here folds case, and anything else that checks coverage must too.
"""

import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
TILES = os.path.join(ROOT, 'lib', 'tiles')
DATA = os.path.join(ROOT, 'lib', 'gamedata')

MONSTERS = ('monster.txt', 'monster.zangband.txt', 'monster.zangbandtk.txt')

# Left out on purpose.  'no trap' is the absence of a trap and 'door lock' is a
# property of a door rather than something drawn on the floor beside it.
TRAP_SKIP = ('no trap', 'door lock')

# Flavoured kinds are covered by flavor: lines rather than object: lines,
# because before it is identified a potion is its flavour and nothing else.
FLAVOURED = ('potion', 'scroll', 'ring', 'amulet', 'rod', 'staff', 'wand',
             'mushroom')


def records(filename, start='name', fields=()):
    """Records from a gamedata file, in file order.

    `start` is the field that begins one, and it is not always `name:`:
    terrain.txt puts `code:` first, and a reader that assumes otherwise hangs
    every feature's code on the feature before it, quietly.
    """
    out, cur = [], None
    path = os.path.join(DATA, filename)
    for line in open(path, encoding='utf-8', errors='replace'):
        line = line.rstrip('\n')
        if line.startswith(start + ':'):
            cur = {start: line[len(start) + 1:].strip()}
            out.append(cur)
        elif cur is not None:
            for f in fields:
                if line.startswith(f + ':'):
                    cur[f] = cur.get(f, '') + line[len(f) + 1:].strip()
    return out


def obj_name(fmt):
    """object.txt's `name:` as lookup_sval() compares it -- see borrow-tiles.py."""
    out, at = [], 0
    while at < len(fmt):
        ch = fmt[at]
        if ch == '&':
            while at < len(fmt) and fmt[at] in ' &':
                at += 1
            continue
        if ch == '~':
            at += 1
            continue
        if ch == '|':
            rest = fmt[at + 1:]
            a = rest.find('|')
            b = rest.find('|', a + 1) if a >= 0 else -1
            if a < 0 or b < 0:
                break
            out.append(rest[:a])
            at = at + 1 + b + 1
            continue
        out.append(ch)
        at += 1
    return ''.join(out)


def tilesets():
    """(name, directory, pref) for each graphics mode in list.txt."""
    out, cur = [], None
    for line in open(os.path.join(TILES, 'list.txt'), encoding='utf-8'):
        line = line.strip()
        if line.startswith('name:'):
            cur = {'name': line.split(':', 2)[2]}
            out.append(cur)
        elif cur is None or not line or line.startswith('#'):
            continue
        elif line.startswith('directory:'):
            cur['dir'] = line.split(':', 1)[1]
        elif line.startswith('pref:'):
            cur['pref'] = line.split(':', 1)[1]
    return [t for t in out if t.get('dir')]


def assignments(directory, pref, seen=None):
    """Every assignment a set makes, by directive, keys folded to lower case."""
    seen = seen if seen is not None else set()
    got = {k: set() for k in ('monster', 'monster-base', 'object', 'flavor',
                              'trap', 'feat', 'GF')}
    path = os.path.join(TILES, directory, pref)
    if pref in seen or not os.path.exists(path):
        return got
    seen.add(pref)

    for line in open(path, encoding='utf-8', errors='replace'):
        line = line.strip()
        if line.startswith('%:'):
            more = assignments(directory, line[2:].strip(), seen)
            for kind in got:
                got[kind] |= more[kind]
            continue
        m = re.match(r'^(monster-base|monster|object|flavor|trap|feat|GF):'
                     r'(.*?):(?:0x[0-9A-Fa-f]{2}|\d+):'
                     r'(?:0x[0-9A-Fa-f]{2}|\d+)$', line)
        if not m:
            continue
        kind, key = m.group(1), m.group(2).lower()
        # trap:, feat: and GF: carry a lighting or direction keyword that is
        # not part of the thing's name.
        if kind in ('trap', 'feat', 'GF'):
            key = key.rsplit(':', 1)[0]
        got[kind].add(key)
    return got


def main():
    show = '--list' in sys.argv

    mons = []
    for f in MONSTERS:
        mons += records(f, fields=('base', 'flags'))
    terrain = [r['code'] for r in records('terrain.txt', start='code')]
    objects = [(r['type'], obj_name(r['name']))
               for r in records('object.txt', fields=('type',))
               if r.get('type') and r['type'] not in FLAVOURED]
    flavours = {int(l.split(':')[1]) for l
                in open(os.path.join(DATA, 'flavor.txt'), encoding='utf-8')
                if l.startswith(('flavor:', 'fixed:'))}
    traps = [l.split(':')[2].strip() for l
             in open(os.path.join(DATA, 'trap.txt'), encoding='utf-8')
             if l.startswith('name:') and len(l.split(':')) > 2]
    projections = [r['code'] for r in records('projection.txt', start='code')]

    print('%-24s %8s %8s %8s %8s %8s %8s'
          % ('tileset', 'monster', 'terrain', 'object', 'flavour', 'trap',
             'spell'))

    faults = 0
    for ts in tilesets():
        got = assignments(ts['dir'], ts['pref'])

        miss_mon = [r['name'] for r in mons
                    if r['name'].lower() not in got['monster']
                    and (r.get('base') or '').lower() not in got['monster-base']]
        uniques = [n for n, r in ((r['name'], r) for r in mons)
                   if 'UNIQUE' in r.get('flags', '')]
        left_mon = [n for n in miss_mon if n not in uniques]

        miss_terr = [t for t in terrain if t.lower() not in got['feat']]
        miss_obj = [o for o in objects
                    if ('%s:%s' % o).lower() not in got['object']]
        miss_flav = sorted(flavours - {int(x) for x in got['flavor']
                                       if x.isdigit()})
        miss_trap = [t for t in traps if t.lower() not in got['trap']]
        left_trap = [t for t in miss_trap if t not in TRAP_SKIP]

        gf = set()
        for k in got['GF']:
            gf |= {t.strip() for t in re.split(r'\s*\|\s*', k)}
        miss_gf = [] if '*' in gf else [p for p in projections
                                        if p.lower() not in gf]

        print('%-24s %8d %8d %8d %8d %8d %8d'
              % (ts['name'], len(miss_mon), len(miss_terr), len(miss_obj),
                 len(miss_flav), len(miss_trap), len(miss_gf)))

        gaps = (('monsters that are not unique', left_mon),
                ('terrain', miss_terr),
                ('objects', ['%s:%s' % o for o in miss_obj]),
                ('flavours', [str(f) for f in miss_flav]),
                ('traps that are drawn', left_trap),
                ('spell effects', miss_gf))
        for label, items in gaps:
            if items:
                faults += len(items)
                print('    %d %s: %s%s'
                      % (len(items), label, ', '.join(sorted(items)[:5]),
                         ' ...' if len(items) > 5 else ''))
        if show and miss_mon:
            print('    (%d uniques, left as text on purpose)'
                  % len([n for n in miss_mon if n in uniques]))

    print('\nCounts are things with no tile at all.  Uniques and the two traps'
          '\nthat are not pictures are left out on purpose and do not fail.')
    print('\n%d gaps' % faults)
    return 1 if faults else 0


if __name__ == '__main__':
    sys.exit(main())
