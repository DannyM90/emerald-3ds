#!/usr/bin/env python3
"""
The Frontier Pass's backdrop on the top screen: the bottom screen's menu
backdrop (gen_menu_art.py) drawn 400 wide, its emerald in the middle, read
by 3ds_bottom_pass.inc from romfs/bottom/pass_top.bin. Same format as
menu.bin.

    python scripts/gen_pass_top_art.py --output romfs/bottom/pass_top.bin
"""
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_menu_art as M  # noqa: E402

M.W = 400
M.GEM_CENTRE = (200, 112)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[1])
    ap.add_argument("--output", required=True)
    ap.add_argument("--png", help="also write the backdrop as a PNG, to look at")
    args = ap.parse_args()
    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    Path(args.output).write_bytes(M.build())
    if args.png:
        M.backdrop().save(args.png)
    print("pass art: %s" % args.output)


if __name__ == "__main__":
    main()
