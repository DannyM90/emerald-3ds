"""The checked-in German manifest and the port's German texts, without a ROM."""
import ast
import gzip
import json
import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import localize_german
from emerald3ds_builder import rom

KINDS = {"c", "u8array", "asm", "u32", "u16s", "u16array", "u16rows2", "u8rows9",
         "u8rows22", "u16rows10", "bard"}
ROM_SIZE = 16 * 1024 * 1024


def manifest():
    return json.loads(gzip.decompress((ROOT / "tools/locales/german.json.gz").read_bytes()))


class GermanManifestTests(unittest.TestCase):
    def test_one_cartridge_everywhere(self):
        self.assertEqual(manifest()["rom_sha1"], localize_german.GERMAN_SHA1)
        self.assertEqual(rom.GERMAN_SHA1, localize_german.GERMAN_SHA1)

    def test_edits_are_positions_only(self):
        # offsets, sizes and kinds: never a byte of the game's text
        for name, entry in manifest()["files"].items():
            self.assertRegex(entry["sha256"], r"^[0-9a-f]{64}$", name)
            previous_end = 0
            for edit in sorted(entry["edits"]):
                start, end, offset, size, kind = edit
                with self.subTest(file=name, edit=edit):
                    self.assertTrue(all(isinstance(n, int) for n in (start, end, offset, size)))
                    self.assertIn(kind, KINDS)
                    self.assertTrue(previous_end <= start < end)
                    self.assertTrue(0 <= offset < offset + size <= ROM_SIZE)
                previous_end = end

    def test_graphics_and_tables_are_rom_ranges(self):
        data = manifest()
        for path, offset, size in data["graphics"]:
            self.assertTrue(path.startswith("graphics/") or path.startswith("data/"), path)
            self.assertTrue(0 <= offset < offset + size <= ROM_SIZE, path)
        for name, offset, size in data["floors"]:
            self.assertTrue(0 <= offset < offset + size <= ROM_SIZE, name)
        self.assertEqual(len(data["floors"]), 4)

    def test_no_more_texts_left_in_english(self):
        # each one is listed with its reason; a regenerated manifest that
        # leaves more of them unmatched is a regression, not a detail
        unmapped = manifest()["unmapped_source_texts"]
        self.assertLessEqual(len(unmapped), 25)
        for text in unmapped:
            self.assertTrue(text["reason"], text)


def ctr_texts():
    """Every CTR_TEXT(...) of the port whose six arguments are string literals."""
    literal = r'"(?:[^"\\]|\\.)*"'
    call = re.compile(r"CTR_TEXT\(\s*(" + r"(?:" + literal + r"\s*)+" +
                      r"(?:,\s*(?:" + literal + r"\s*)+){5})\)")
    for folder in ("src", "include"):
        for path in sorted((ROOT / "3ds_port" / folder).glob("*")):
            if path.suffix not in (".c", ".h", ".inc"):
                continue
            for match in call.finditer(path.read_text(encoding="utf-8")):
                args = []
                for arg in re.split(r",\s*(?=\")", match.group(1)):
                    args.append("".join(ast.literal_eval(part)
                                        for part in re.findall(literal, arg)))
                if len(args) == 6:
                    yield path.name, args


def glyphs():
    """What CtrLocale_Glyph draws: the rest becomes '?' on the touch screen."""
    header = (ROOT / "3ds_port/include/3ds_locale_text.h").read_text(encoding="utf-8")
    body = header[header.index("CtrLocale_Glyph"):header.index("default:")]
    chars = set("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789")
    for case in re.findall(r"case\s+(0x[0-9a-fA-F]+|'(?:\\.|[^'\\])'):", body):
        chars.add(chr(int(case, 16)) if case.startswith("0x") else ast.literal_eval(case))
    return chars


class GermanPortTextTests(unittest.TestCase):
    def test_the_port_has_german_texts(self):
        texts = list(ctr_texts())
        self.assertGreater(len(texts), 150)
        for name, args in texts:
            self.assertTrue(args[5], "%s: empty German text for %r" % (name, args[0]))

    def test_german_texts_show_on_the_touch_screen(self):
        # an umlaut is fine, a typographic quote or an accent the font lacks
        # would be drawn as '?'
        drawn = glyphs()
        for ch in "ÄÖÜäöüß":
            self.assertIn(ch, drawn)
        for name, args in ctr_texts():
            missing = sorted(set(args[5]) - drawn - {"\n"})
            self.assertFalse(missing, "%s: %r has %r" % (name, args[5], missing))


if __name__ == "__main__":
    unittest.main()
