#!/usr/bin/env python3
"""The hand marks over the terraces (voxel_terraces.py, "7. hand marks"): the
piece a level alone gives, the runs a face or a band drawn wider than a tile
falls down, a ledge never given a piece, a real map taking its marks; and
the ledges between two levels ("ledges between two levels"): landings half
way, each ledge's top and foot, Lilycove's rings."""

import pathlib
import sys
from unittest.mock import patch

PORT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PORT / "scripts"))

import voxel_terraces as vt  # noqa: E402

L = vt.LEVEL


class FakeCells:
    """What apply_marks reads of a Cells: the kept cells, the bands, the
    faces, the bridges and the water (none)."""

    def __init__(self, faces=(), band=None, keep=()):
        self.faces = set(faces)
        self.band = dict(band or {})
        self.keep = set(keep)
        self.bridges = set()

    def water(self, cell):
        return False


def grid_levels(rows):
    """{cell: px} from rows of levels, north row first."""
    return {(x, y): L * v for y, row in enumerate(rows) for x, v in enumerate(row)}


def test_auto_pieces():
    # a plateau at 1 over ground at 0, its face the row y=2
    level = grid_levels([[0, 0, 0, 0, 0],
                         [0, 1, 1, 1, 0],
                         [0, 1, 1, 1, 0],
                         [0, 0, 0, 0, 0]])
    got = {cell: vt.auto_piece(level, cell)[0] for cell in level if level[cell]}
    assert got == {(1, 1): "W", (2, 1): "F", (3, 1): "E",
                   (1, 2): "cSW", (2, 2): "S", (3, 2): "cSE"}, got
    # a concave corner: only the diagonal falls
    level = grid_levels([[1, 1], [0, 1]])
    assert vt.auto_piece(level, (1, 0)) == ("vSW", L)
    # two levels at once: a band of two, a face of two in its tile
    level = grid_levels([[0, 2, 2]])
    assert vt.auto_piece(level, (1, 0)) == ("W2", 2 * L)
    level = grid_levels([[2], [0]])
    assert vt.auto_piece(level, (0, 0)) == ("S:32", 2 * L)


def test_marks_and_runs():
    # a plateau at 2 whose face is drawn two rows tall (y=1, y=2), all
    # marked at the top's level: the face falls a level a row
    pieces = {cell: ("F", 0) for cell in grid_levels([[0] * 3] * 5)}
    marks = {(x, y): {"level": 2 * L, "piece": None} for x in range(3) for y in range(3)}
    c = FakeCells(faces={(x, y) for x in range(3) for y in (1, 2)})
    flat = {}
    with patch.object(vt, "is_face", lambda cc, cell: cell in cc.faces):
        marked = vt.apply_marks(c, pieces, flat, {}, marks)
    assert marked == set(marks)
    for x in range(3):
        assert pieces[(x, 0)] == ("F", 2 * L), pieces[(x, 0)]
        assert pieces[(x, 1)] == ("S", 2 * L), pieces[(x, 1)]
        assert pieces[(x, 2)] == ("S", L), pieces[(x, 2)]
        assert pieces[(x, 3)] == ("F", 0)
    assert flat[(1, 0)] == 2 * L

    # a band drawn two columns wide over a one-level fall: the fall shared out
    pieces = {(x, 0): ("F", 0) for x in range(4)}
    marks = {(x, 0): {"level": L, "piece": None} for x in (1, 2, 3)}
    c = FakeCells(band={(1, 0): "W", (2, 0): "W"})
    with patch.object(vt, "is_face", lambda cc, cell: False):
        vt.apply_marks(c, pieces, {}, {}, marks)
    assert pieces[(2, 0)] == ("W:8", L), pieces[(2, 0)]
    assert pieces[(1, 0)] == ("W:8", L - 8), pieces[(1, 0)]
    assert pieces[(3, 0)] == ("F", L)

    # a piece given is taken as given; a ledge is never marked
    pieces = {(0, 0): ("F", 0), (1, 0): ("F", 0)}
    marks = {(0, 0): {"level": L, "piece": "S"}, (1, 0): {"level": L, "piece": None}}
    with patch.object(vt, "is_face", lambda cc, cell: False):
        marked = vt.apply_marks(FakeCells(keep={(1, 0)}), pieces, {}, {}, marks)
    assert marked == {(0, 0)}
    assert pieces == {(0, 0): ("S", L), (1, 0): ("F", 0)}, pieces

    # a neighbour left apart from the marks follows the levels; one that
    # still meets them is left as it was
    pieces = {(x, y): ("F", 0) for x in range(3) for y in range(2)}
    pieces[(2, 1)] = ("E", 0)          # meets nothing marked: kept
    marks = {(0, 0): {"level": L, "piece": None}}
    with patch.object(vt, "is_face", lambda cc, cell: False):
        vt.apply_marks(FakeCells(), pieces, {}, {}, marks)
    assert pieces[(0, 0)] == ("cSE", L), pieces[(0, 0)]
    assert pieces[(2, 1)] == ("E", 0)


def test_unknown_piece_refused():
    class Members:
        members = [("LAYOUT_LAVARIDGE_TOWN", 0, 0)]
    fixes = {"layouts": {"LAYOUT_LAVARIDGE_TOWN": {"terraces": {"1,2": {"piece": "Z"}}}}}
    try:
        vt.load_marks(Members(), fixes)
    except ValueError:
        return
    raise AssertionError("an unknown piece was taken")


def test_real_map():
    # Lavaridge: a cell of its south lawn raised a level by hand stands so,
    # and the solve without marks is the drawing's own
    lid = "LAYOUT_LAVARIDGE_TOWN"
    auto = vt.solve_auto(lid)
    cell = (10, 17)
    top = auto["pieces"][cell][1]
    sol = vt.finish(auto, {cell: {"level": top + L, "piece": None}})
    assert sol["pieces"][cell] == ("S", top + L), sol["pieces"][cell]
    assert sol["marked"] == {cell}
    assert vt.finish(auto, {})["pieces"] == auto["pieces"]
    # a mark's level counts from the group's main ground, wherever the world
    # placed it
    zero = vt.zero_of(auto)
    fixes = {"layouts": {lid: {"terraces": {"10,17": {"level": (top - zero) // L + 1}}}}}
    assert vt.load_marks(auto["cells"], fixes, zero=zero) == {cell: {"level": top + L, "piece": None}}
    shifted = vt.load_marks(auto["cells"], fixes, zero=zero - L)
    assert shifted == {cell: {"level": top, "piece": None}}, shifted


class LedgeCells:
    """What ledge_landings reads of a Cells, from rows of characters: '.'
    ground (walked), '#' rock, 'v' '>' '<' a ledge jumped south, east, west,
    'L'/'J' corners jumped south and east / south and west."""

    WAYS = {"v": ((0, 1),), ">": ((1, 0),), "<": ((-1, 0),), "L": ((1, 0), (0, 1)), "J": ((-1, 0), (0, 1))}

    def __init__(self, rows):
        self.src, self.kind, self.jumps = {}, {}, {}
        self.keep = set()
        for y, row in enumerate(rows):
            for x, ch in enumerate(row):
                cell = (x, y)
                self.src[cell] = ("LAYOUT_TEST", x, y)
                self.kind[cell] = vt.ROCK if ch == "#" else vt.GROUND
                if ch in self.WAYS:
                    self.jumps[cell] = self.WAYS[ch]
                    self.keep.add(cell)

    def water(self, cell):
        return False

    def walked(self, cell):
        return cell in self.src and cell not in self.jumps and self.kind[cell] == vt.GROUND

    def role(self, cell):
        return "floor" if self.walked(cell) else "wall"

    def house(self, cell):
        return False


def test_landings():
    # Lilycove's upper ring: jumped into from the hill (3 levels) on three
    # sides, out of to the ground south (2 levels): half way
    rows = [".......",
            ".LvvvJ.",
            ".>...<.",
            "#LvvvJ#",
            "......."]
    c = LedgeCells(rows)
    flat = {cell: (3 * L if y < 3 else 2 * L) for cell in c.src for (_, y) in [cell] if cell not in c.jumps}
    got = vt.ledge_landings(c, flat)
    assert set(got) == {(2, 2), (3, 2), (4, 2)}, got
    assert all(v == 2.5 * L for v in got.values()), got
    assert flat[(3, 2)] == 2.5 * L
    # each ledge between them a step of half a level: its sides
    sides = vt.ledge_sides(c, None, flat)
    assert sides[(3, 1)] == {(0, 1): [3 * L, 2.5 * L]}, sides[(3, 1)]
    assert sides[(3, 3)] == {(0, 1): [2.5 * L, 2 * L]}, sides[(3, 3)]
    assert sides[(1, 2)] == {(1, 0): [3 * L, 2.5 * L]}, sides[(1, 2)]
    # a corner takes what it lacks from its arms
    assert sides[(1, 3)] == {(1, 0): [3 * L, 2.5 * L], (0, 1): [2.5 * L, 2 * L]}, sides[(1, 3)]
    # a mark sets a ledge's top and foot
    marked = vt.ledge_sides(c, None, flat, {(3, 1): {"level": 4 * L, "foot": 3 * L}})
    assert marked[(3, 1)] == {(0, 1): [4 * L, 3 * L]}

    # walked out of on one side: no landing, the ground stays as it was
    rows[2] = ".>....."
    c = LedgeCells(rows)
    flat = {cell: (3 * L if y < 3 else 2 * L) for cell in c.src for (_, y) in [cell] if cell not in c.jumps}
    assert vt.ledge_landings(c, flat) == {}


def test_real_ledges():
    # Lilycove: its two rings of ledges between the hill and the town below
    # stand half way, every ledge round them a step; none jumped up
    lid = "LAYOUT_LILYCOVE_CITY"
    sol = vt.finish(vt.solve_auto(lid), {})
    c = sol["cells"]
    at = {src[1:]: cell for cell, src in c.src.items() if src[0] == lid}
    hill, below = sol["pieces"][at[(48, 8)]][1], sol["pieces"][at[(48, 12)]][1]
    assert hill - below == L, (hill, below)
    for x in range(48, 54):
        assert sol["pieces"][at[(x, 10)]] == ("F", (hill + below) / 2), sol["pieces"][at[(x, 10)]]
    ring = [at[(x, y)] for x in range(47, 55) for y in (9, 11)] + [at[(47, 10)], at[(54, 10)]]
    assert set(ring) <= sol["ledges"]["steps"], set(ring) - sol["ledges"]["steps"]
    assert not sol["ledges"]["uphill"], sol["ledges"]["uphill"]


if __name__ == "__main__":
    for name, fn in list(globals().items()):
        if name.startswith("test_"):
            fn()
            print("ok", name)
