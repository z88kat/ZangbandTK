import re, sys

def frames_from(path):
    W, H = 260, 200
    grid = [[(' ', 0)] * W for _ in range(H)]
    out = []
    def snap():
        return [row[:] for row in grid]
    for raw in open(path, 'rb'):
        line = raw.decode('utf-8', 'replace').rstrip('\n')
        m = re.match(r'term-text (\d+) (\d+) (\d+) ([0-9a-f]{2}) (.*)$', line)
        if m:
            x, y, n, a, s = (int(m.group(1)), int(m.group(2)), int(m.group(3)),
                             int(m.group(4), 16), m.group(5))
            for i, ch in enumerate(s[:n]):
                if 0 <= y < H and 0 <= x + i < W:
                    grid[y][x + i] = (ch, a)
            continue
        m = re.match(r'term-wipe (\d+) (\d+) (\d+)', line)
        if m:
            x, y, n = map(int, m.groups())
            for i in range(n):
                if 0 <= y < H and 0 <= x + i < W:
                    grid[y][x + i] = (' ', 0)
            continue
        if line.startswith('term-xtra-clear'):
            grid = [[(' ', 0)] * W for _ in range(H)]
            continue
        if line.startswith('term-xtra-event'):
            out.append(snap())
    return out

if __name__ == '__main__':
    fs = frames_from(sys.argv[1])
    print("frames:", len(fs))
    idx = int(sys.argv[2]) if len(sys.argv) > 2 else len(fs) - 1
    for row in fs[idx][:26]:
        print(''.join(c for c, a in row).rstrip()[:100])
