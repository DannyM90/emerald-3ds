#!/usr/bin/env python3
"""Preview of the Portuguese title logo, VERSÃO over ESMERALDA, built from the
letters of the player's own English logo:

    python tools/logo_ptbr.py --tree build/upstream --out logo.png

The port does the same at load time (3ds_port/src/3ds_icons_ptbr.c,
DrawLogo); 3ds_port/tests/logo_ptbr_test.py checks that both agree pixel for
pixel. ESMERALDA lies on an arc of a circle, each letter turned with it, and
VERSÃO stands straight above it, as EDICIÓN ESMERALDA does in the Spanish
game. The English letters lie on a parabola: each column is moved up by its
height there to stand them straight, then each letter is set in its place, a
pixel's shade from how much of it the letter covers. Only these measures are
kept here; this reads graphics/title_screen/emerald_version.png from the tree
and writes nothing into the repository.
"""
import argparse
import math
from pathlib import Path

W, H = 128, 32
# Luminance of the English logo's 16 palette entries (its grey ramp); a pixel
# is part of a letter's face from 110 up.
LUM = (0, 156, 90, 222, 24, 74, 238, 41, 172, 106, 189, 139, 205, 123, 65, 255)
FACE_MIN = 110
WHITE, INK = 0xF, 0x4
EDGE_GREYS = (0xF, 0x6, 0x3, 0xC, 0xA, 0x8, 0x1, 0xB, 0xD, 0x9)  # face greys, by luminance below

# The English letters: their box in the logo and the parabola its arc follows
# (k, vertex x): every column is moved up by floor(k*(x-xv)^2 + 0.5) to stand it straight.
BIG_ARC, SMALL_ARC = (0.0026, 55.0), (0.0020, 57.0)
SOURCES = {
    'M': (19, 37, 0, 19, BIG_ARC), 'E': (39, 48, 0, 19, BIG_ARC), 'R': (51, 62, 0, 18, BIG_ARC),
    'A': (63, 78, 0, 19, BIG_ARC), 'L': (80, 89, 0, 19, BIG_ARC), 'D': (90, 104, 0, 24, BIG_ARC),
    'v': (20, 30, 20, 31, SMALL_ARC), 's': (50, 58, 18, 31, SMALL_ARC), 'o': (64, 75, 19, 31, SMALL_ARC),
}
# ESMERALDA on an arc of a circle, as the Spanish game's EDICIÓN ESMERALDA:
# centre, radius of the letters' tops and their depth; each letter between two
# angles (S is the English small s, stretched).
CX, CY, R, DEPTH = 54.9, 154.0, 142.2, 10.05
TOP_IN = -0.25
BIG = (('E', -0.355, -0.2927), ('s', -0.2958, -0.2257), ('M', -0.21, -0.1024), ('E', -0.1009, -0.0367),
       ('R', -0.0333, 0.0361), ('A', 0.0328, 0.1239), ('L', 0.1234, 0.1898), ('D', 0.181, 0.2738),
       ('A', 0.2682, 0.3621))
# VERSÃO, straight, rows 4..11: (source, x, width); Ã's tilde over it.
SMALL_TOP, SMALL_BOTTOM = 4.0, 11.0
SMALL = (('v', 30, 8), ('E', 39, 6), ('R', 46, 7), ('s', 54, 7), ('A', 62, 8), ('o', 71, 8))
TILDE_ROWS = ('.###...#', '#...###.')
TILDE_Y = 1
SUB = 4          # subsamples per pixel side
SUB_MIN = 0.3    # a subsample is ink from this whiteness
COVER_MIN = 8    # of SUB*SUB subsamples, for a face pixel


def rnd(v):
    return int(math.floor(v + 0.5))


def flat_letter(src, key):
    """The English letter straightened: whiteness rows, 0 off the face."""
    x0, x1, y0, y1, (k, xv) = SOURCES[key]
    pts = {}
    for x in range(x0, x1 + 1):
        s = rnd(k * (x - xv) * (x - xv))
        for y in range(y0, y1 + 1):
            v = src[y][x]
            if LUM[v] >= FACE_MIN:
                pts[(x, y - s)] = LUM[v] / 255.0
    mx = min(x for x, y in pts); my = min(y for x, y in pts)
    w = max(x for x, y in pts) - mx + 1; h = max(y for x, y in pts) - my + 1
    a = [[0.0] * w for _ in range(h)]
    for (x, y), v in pts.items():
        a[y - my][x - mx] = v
    return a


def sample(a, u, v):
    h, w = len(a), len(a[0])
    if u < 0 or v < 0 or u >= w or v >= h:
        return 0.0
    u -= 0.5; v -= 0.5
    x0 = min(max(int(math.floor(u)), 0), w - 1); y0 = min(max(int(math.floor(v)), 0), h - 1)
    fx = min(max(u - x0, 0.0), 1.0); fy = min(max(v - y0, 0.0), 1.0)
    x1 = min(x0 + 1, w - 1); y1 = min(y0 + 1, h - 1)
    return (a[y0][x0] * (1 - fx) * (1 - fy) + a[y0][x1] * fx * (1 - fy)
            + a[y1][x0] * (1 - fx) * fy + a[y1][x1] * fx * fy)


def shade(count):
    c = count / (SUB * SUB)
    l = 100 + 155 * min(1.0, (c - 0.5) / 0.45)
    best = None
    for i in EDGE_GREYS:
        d = abs(LUM[i] - l)
        if best is None or d < best[0]:
            best = (d, i)
    return best[1]


def put(img, face, x, y, count):
    if count < COVER_MIN or not (0 <= x < W and 0 <= y < H):
        return
    c = shade(count)
    if face[y][x] and LUM[img[y][x]] >= LUM[c]:
        return
    img[y][x] = c
    face[y][x] = 1


def draw_big(img, face, a, t0, t1):
    h, w = len(a), len(a[0])
    xs = [CX + (R - v) * math.sin(t) for t in (t0, t1) for v in (TOP_IN, DEPTH)]
    ys = [CY - (R - v) * math.cos(t) for t in (t0, t1, (t0 + t1) / 2) for v in (TOP_IN, DEPTH)]
    for x in range(int(math.floor(min(xs))) - 1, int(math.floor(max(xs))) + 2):
        for y in range(int(math.floor(min(ys))) - 1, int(math.floor(max(ys))) + 2):
            n = 0
            for i in range(SUB):
                for j in range(SUB):
                    X = x + (i + 0.5) / SUB; Y = y + (j + 0.5) / SUB
                    t = math.atan2(X - CX, CY - Y); r = math.hypot(X - CX, Y - CY)
                    u = (t - t0) / (t1 - t0) * w; v = ((R - r) - TOP_IN) / (DEPTH - TOP_IN) * h
                    if sample(a, u, v) >= SUB_MIN:
                        n += 1
            put(img, face, x, y, n)


def draw_small(img, face, a, x0, wd):
    h, w = len(a), len(a[0])
    for x in range(x0, x0 + wd):
        for y in range(int(SMALL_TOP) - 1, int(SMALL_BOTTOM) + 2):
            n = 0
            for i in range(SUB):
                for j in range(SUB):
                    u = (x - x0 + (i + 0.5) / SUB) / wd * w
                    v = (y + (j + 0.5) / SUB - SMALL_TOP) / (SMALL_BOTTOM - SMALL_TOP) * h
                    if sample(a, u, v) >= SUB_MIN:
                        n += 1
            put(img, face, x, y, n)


def outline(img, face):
    for y in range(H):
        for x in range(W):
            if face[y][x] and all(0 <= x + dx < W and 0 <= y + dy < H and face[y + dy][x + dx]
                                  for dx in (-1, 0, 1) for dy in (-1, 0, 1)):
                img[y][x] = WHITE
    for _ in range(2):  # two rings of ink round the faces
        ring = [(x, y) for y in range(H) for x in range(W) if not face[y][x] and img[y][x] != INK
                and any(0 <= x + dx < W and 0 <= y + dy < H and (face[y + dy][x + dx] or img[y + dy][x + dx] == INK)
                        for dx in (-1, 0, 1) for dy in (-1, 0, 1))]
        if _ == 0:
            ring = [(x, y) for (x, y) in ring if any(0 <= x + dx < W and 0 <= y + dy < H and face[y + dy][x + dx]
                                                     for dx in (-1, 0, 1) for dy in (-1, 0, 1))]
        for x, y in ring:
            img[y][x] = INK
    # pockets closed in by the letters: ink too
    out = [[0] * W for _ in range(H)]
    stack = [(x, y) for x in range(W) for y in (0, H - 1)] + [(x, y) for y in range(H) for x in (0, W - 1)]
    stack = [(x, y) for x, y in stack if img[y][x] == 0]
    for x, y in stack:
        out[y][x] = 1
    while stack:
        x, y = stack.pop()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            X, Y = x + dx, y + dy
            if 0 <= X < W and 0 <= Y < H and not out[Y][X] and img[Y][X] == 0:
                out[Y][X] = 1; stack.append((X, Y))
    for y in range(H):
        for x in range(W):
            if img[y][x] == 0 and not out[y][x]:
                img[y][x] = INK


def draw(src):
    img = [[0] * W for _ in range(H)]
    face = [[0] * W for _ in range(H)]
    cache = {}
    def letter(k):
        if k not in cache:
            cache[k] = flat_letter(src, k)
        return cache[k]
    for k, t0, t1 in BIG:
        draw_big(img, face, letter(k), t0, t1)
    for k, x, wd in SMALL:
        draw_small(img, face, letter(k), x, wd)
    ax = [x for k, x, wd in SMALL if k == 'A'][0]; aw = [wd for k, x, wd in SMALL if k == 'A'][0]
    tx = ax + (aw - len(TILDE_ROWS[0])) // 2
    for r, row in enumerate(TILDE_ROWS):
        for i, ch in enumerate(row):
            if ch == '#':
                img[TILDE_Y + r][tx + i] = WHITE; face[TILDE_Y + r][tx + i] = 1
    outline(img, face)
    return img


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--tree', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    from PIL import Image

    logo = Image.open(args.tree / 'graphics/title_screen/emerald_version.png')
    src = [[logo.getpixel((x, y)) for x in range(W)] for y in range(H)]
    img = draw(src)
    out = Image.new('P', (W, H))
    out.putpalette(logo.getpalette())
    for y in range(H):
        for x in range(W):
            out.putpixel((x, y), img[y][x])
    out.resize((W * 8, H * 8), Image.NEAREST).save(args.out)
    print('preview:', args.out)


if __name__ == '__main__':
    main()
