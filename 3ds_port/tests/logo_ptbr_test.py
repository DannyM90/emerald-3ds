"""The Portuguese title logo: DrawLogo (src/3ds_icons_ptbr.c), built on the host,
draws from the tree's English logo exactly what tools/logo_ptbr.py previews,
and a logo that is not the expected one keeps the drawn fallback."""
import argparse
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]

HARNESS = r'''
#include <stdio.h>
void Port_TransformAsset(const char *path, unsigned char *data, unsigned size);
const unsigned char *Port_LatinGlyph(unsigned font, unsigned char code, int *width)
{
    (void)font;
    (void)code;
    *width = 0;
    return NULL;
}
int main(void)
{
    static unsigned char d[4096];
    if (fread(d, 1, sizeof(d), stdin) != sizeof(d))
        return 1;
    Port_TransformAsset("graphics/title_screen/emerald_version.8bpp", d, sizeof(d));
    fwrite(d, 1, sizeof(d), stdout);
    return 0;
}
'''


def tile_offset(x, y):
    """Two 64x32 sprites of 8bpp tiles (gbagfx -mwidth 8 -mheight 4)."""
    return ((x // 64) * 32 + (y // 8) * 8 + (x % 64) // 8) * 64 + (y % 8) * 8 + x % 8


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--cc', default='cc')
    parser.add_argument('--tree', type=Path, default=ROOT.parent,
                        help='bootstrapped tree holding graphics/title_screen/emerald_version.png')
    args = parser.parse_args()
    png = args.tree / 'graphics/title_screen/emerald_version.png'
    tool = args.tree / 'tools/logo_ptbr.py'
    if not png.is_file() or not tool.is_file():
        parser.error('run from a bootstrapped tree, or pass --tree PATH')
    from PIL import Image
    spec = importlib.util.spec_from_file_location('logo_ptbr', tool)
    logo_ptbr = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(logo_ptbr)

    logo = Image.open(png)
    src = [[logo.getpixel((x, y)) for x in range(128)] for y in range(32)]
    data = bytearray(4096)
    for y in range(32):
        for x in range(128):
            data[tile_offset(x, y)] = src[y][x]

    with tempfile.TemporaryDirectory() as tmp:
        harness = Path(tmp) / 'harness.c'
        harness.write_text(HARNESS)
        exe = Path(tmp) / 'logo_test'
        subprocess.run([args.cc, '-std=gnu99', '-O2', '-Wall', '-Wextra', '-Werror', '-DCTR_LANG_PT_BR=1',
                        '-I' + str(ROOT / 'include'), str(harness), str(ROOT / 'src/3ds_icons_ptbr.c'),
                        '-lm', '-o', str(exe)], check=True)
        out = subprocess.run([str(exe)], input=bytes(data), capture_output=True, check=True).stdout
        other = bytearray(data)
        other[tile_offset(0, 0)] ^= 1
        fallback = subprocess.run([str(exe)], input=bytes(other), capture_output=True, check=True).stdout

    want = logo_ptbr.draw(src)
    wrong = [(x, y) for y in range(32) for x in range(128) if out[tile_offset(x, y)] != want[y][x]]
    if wrong:
        sys.exit('logo_ptbr_test: DrawLogo and tools/logo_ptbr.py differ at %d pixels, e.g. %s'
                 % (len(wrong), wrong[:5]))
    if fallback == out:
        sys.exit('logo_ptbr_test: an unexpected source logo did not fall back to the drawn one')
    print('PASS logo pt_br: DrawLogo equals the preview pixel for pixel, unexpected source falls back')


if __name__ == '__main__':
    main()
