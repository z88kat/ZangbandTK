"""Render a captured ZangbandTK terminal frame as an SVG.

The game is drawn with characters and colour attributes, so an SVG of exactly
those is not an approximation of a screenshot -- it is the screen, at any size,
with selectable text and no image library involved.
"""
import sys, html
sys.path.insert(0, sys.argv[0].rsplit('/', 1)[0])
from capture import frames_from
# The palette, read out of the game's own source so it cannot drift.
import re as _re, os as _os
_root = _os.path.join(_os.path.dirname(_os.path.abspath(sys.argv[0])), '..', '..')
_src = open(_os.path.join(_root, 'src', 'z-color.c')).read()
_m = _re.search(r'uint8_t angband_color_table\[MAX_COLORS\]\[4\] =\s*\{(.*?)\n\};',
                _src, _re.S)
PALETTE = ['#%s%s%s' % rgb for rgb in
           _re.findall(r'\{0x[0-9a-f]{2}, 0x([0-9a-f]{2}), 0x([0-9a-f]{2}), 0x([0-9a-f]{2})\}',
                       _m.group(1))]

CW, CH = 8.4, 17.0          # character cell, in px
PAD = 10

def render(frame, out, title):
    rows = [r for r in frame]
    while rows and not ''.join(c for c, a in rows[-1]).strip():
        rows.pop()
    width = 0
    for r in rows:
        line = ''.join(c for c, a in r).rstrip()
        width = max(width, len(line))
    w = width * CW + PAD * 2
    h = len(rows) * CH + PAD * 2

    parts = ['<svg xmlns="http://www.w3.org/2000/svg" width="%.0f" height="%.0f" '
             'viewBox="0 0 %.1f %.1f" role="img" aria-label="%s">'
             % (w, h, w, h, html.escape(title)),
             '<rect width="100%" height="100%" fill="#000"/>',
             '<g font-family="Menlo,DejaVu Sans Mono,Consolas,monospace" '
             'font-size="14" xml:space="preserve">']

    for y, row in enumerate(rows):
        # group consecutive cells sharing a colour into one <text>
        x = 0
        while x < len(row):
            ch, a = row[x]
            if ch == ' ':
                x += 1
                continue
            run = ch
            attr = a
            x2 = x + 1
            while x2 < len(row) and row[x2][1] == attr and row[x2][0] != ' ':
                run += row[x2][0]
                x2 += 1
            colour = PALETTE[attr % len(PALETTE)] if attr < len(PALETTE) else '#ffffff'
            parts.append('<text x="%.1f" y="%.1f" fill="%s">%s</text>'
                         % (PAD + x * CW, PAD + (y + 1) * CH - 4, colour,
                            html.escape(run)))
            x = x2

    parts.append('</g></svg>')
    open(out, 'w').write('\n'.join(parts))
    return width, len(rows)

if __name__ == '__main__':
    log, idx, out, title = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4]
    fs = frames_from(log)
    print(out, render(fs[idx], out, title))
