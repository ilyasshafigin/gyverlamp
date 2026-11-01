import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from PIL import Image

TOOLS = Path(__file__).resolve().parent
ROOT = TOOLS.parents[2]
spec = importlib.util.spec_from_file_location("audio_compare_compose", TOOLS / "compose.py")
assert spec is not None and spec.loader is not None
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def gif_palettes(path):
    """Read actual encoded global/local tables, not source-code intent."""
    data = path.read_bytes()
    assert data[:6] in (b"GIF87a", b"GIF89a")
    position = 13
    palettes = []
    if data[10] & 128:
        size = 3 * 2 ** ((data[10] & 7) + 1)
        palettes.append(data[position:position + size])
        position += size
    while data[position] != 0x3B:
        tag = data[position]
        position += 1
        if tag == 0x21:
            position += 1
        elif tag == 0x2C:
            packed = data[position + 8]
            position += 9
            if packed & 128:
                size = 3 * 2 ** ((packed & 7) + 1)
                palettes.append(data[position:position + size])
                position += size
            position += 1  # LZW minimum code size
        else:
            raise AssertionError(f"unexpected GIF block {tag}")
        while data[position]:
            position += data[position] + 1
        position += 1
    return palettes


class ComposeTests(unittest.TestCase):
    def setUp(self):
        base = ROOT / ".artifacts/tests/audio-compare/compositor"
        base.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="test space ", dir=base)
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.capture = self.root / "inputs with spaces"
        self.capture.mkdir()
        self.audio = [dict(frame=i, ms=i * 50, level=i * 60, bass=255 - i * 60, treble=i * 40, beat=i == 2) for i in range(4)]
        (self.capture / "audio.json").write_text(json.dumps(self.audio))
        for side, channel in (("left", 0), ("right", 2)):
            data = bytearray()
            for i in range(4):
                pixel = [0, 0, 0]
                pixel[channel] = 100 + i * 40
                data.extend(bytes(pixel) * 6)
            (self.capture / (side + ".rgb")).write_bytes(data)
        self.manifest = dict(schemaVersion=1, width=3, height=2, frameMs=50, frameCount=4, rowOrder="top-to-bottom", audio="audio.json",
                             left=dict(label="Без звука", rgb="left.rgb"), right=dict(label="Другой источник", rgb="right.rgb"),
                             provenance="Offline prototype · synthetic", fidelityNote="Только представление; не микрофон и не firmware", settings=dict(test=True))
        self.path = self.capture / "capture.json"
        self.save()

    def save(self):
        self.path.write_text(json.dumps(self.manifest, ensure_ascii=False), encoding="utf-8")

    def test_real_gif_timing_geometry_palette_and_reproducibility(self):
        first = self.root / "first"
        second = self.root / "second"
        report = module.compose(self.path, first)
        module.compose(self.path, second)
        self.assertEqual((first / "comparison.gif").read_bytes(), (second / "comparison.gif").read_bytes())
        with Image.open(first / "comparison.gif") as gif:
            self.assertEqual(getattr(gif, "n_frames", 1), 4)
            self.assertEqual(gif.size, (640, 480))
            for i in range(4):
                gif.seek(i)
                self.assertEqual(gif.info["duration"], 50)
                image = gif.convert("RGB")
                self.assertEqual(image.getpixel((160, 240)), (100 + i * 40, 0, 0))
                self.assertEqual(image.getpixel((480, 240)), (0, 0, 100 + i * 40))
                # Non-square source is letterboxed, not stretched to 272x272.
                self.assertEqual(image.getpixel((160, 110)), module.BACKGROUND)
        tables = gif_palettes(first / "comparison.gif")
        self.assertTrue(tables)
        self.assertTrue(all(table == tables[0] for table in tables))
        self.assertFalse(report["dither"])
        self.assertEqual(report["durationMs"], 200)
        self.assertEqual(report["provenance"], self.manifest["provenance"])
        self.assertEqual(report["captureMetadata"]["settings"], {"test": True})
        self.assertEqual(report["inputHashes"]["left"], module.digest(self.capture / "left.rgb"))

    def test_cli_resolves_manifest_paths_from_another_cwd_with_spaces(self):
        elsewhere = self.root / "unrelated cwd"
        elsewhere.mkdir()
        result = subprocess.run([sys.executable, str(TOOLS / "compose.py"), "--manifest", str(self.path), "--out", str(self.root / "output with spaces")], cwd=elsewhere, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        outputs = json.loads(result.stdout)
        self.assertTrue(Path(outputs["gif"]).is_file())
        report = json.loads(Path(outputs["report"]).read_text())
        self.assertEqual(report["inputPaths"]["audio"], str(self.capture / "audio.json"))

    def test_duplicate_backward_and_missing_timestamps_fail_before_publish(self):
        for bad in (0, 25, -50, 150):
            with self.subTest(timestamp=bad):
                audio = [dict(row) for row in self.audio]
                audio[1]["ms"] = bad
                (self.capture / "audio.json").write_text(json.dumps(audio))
                with self.assertRaisesRegex(ValueError, "timestamps"):
                    module.compose(self.path, self.root / "invalid")
                self.assertFalse((self.root / "invalid/comparison.gif").exists())

    def test_truncated_rgb_and_invalid_essential_metadata(self):
        original = (self.capture / "left.rgb").read_bytes()
        (self.capture / "left.rgb").write_bytes(original[:-1])
        with self.assertRaisesRegex(ValueError, "RGB24 bytes"):
            module.compose(self.path, self.root / "invalid")
        (self.capture / "left.rgb").write_bytes(original)
        for field, value in (("width", 0), ("height", True), ("frameCount", 3), ("frameMs", 33), ("rowOrder", "bottom-up"), ("schemaVersion", 2), ("provenance", "")):
            with self.subTest(field=field):
                previous = self.manifest[field]
                self.manifest[field] = value
                self.save()
                with self.assertRaises(ValueError):
                    module.compose(self.path, self.root / "invalid")
                self.manifest[field] = previous
        self.assertFalse((self.root / "invalid/comparison.gif").exists())

    def test_invalid_signal_bool_and_frame_order(self):
        for key, value in (("level", 256), ("bass", True), ("treble", -1), ("beat", 1), ("frame", 0)):
            audio = [dict(row) for row in self.audio]
            audio[1][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                module.validate_audio(audio, 4, 50)

    def test_missing_font_fails_actionably_and_preserves_existing_outputs(self):
        out = self.root / "out"
        out.mkdir()
        (out / "comparison.gif").write_bytes(b"existing")
        with self.assertRaisesRegex(ValueError, "--font"):
            module.compose(self.path, out, str(self.root / "missing.ttf"))
        self.assertEqual((out / "comparison.gif").read_bytes(), b"existing")
        self.assertFalse((out / "report.json").exists())

    def test_tracked_default_fixture_is_complete_and_independent(self):
        fixture = ROOT / "sim/fixtures/audio/example-three-signal.json"
        audio = json.loads(fixture.read_text())
        module.validate_audio(audio, 400, 50)
        self.assertEqual((audio[0]["ms"], audio[-1]["ms"]), (0, 19950))
        self.assertEqual(audio[80]["bass"], 255)
        self.assertEqual(audio[140]["level"], 255)
        self.assertEqual(audio[200]["treble"], 255)
        self.assertEqual(audio[80]["level"], 0)
        self.assertEqual(audio[140]["bass"], 0)


if __name__ == "__main__":
    unittest.main()
