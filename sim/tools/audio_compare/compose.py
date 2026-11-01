#!/usr/bin/env python3
"""Compose synchronized RGB24 captures, without knowing their rendering model.

Manifest schemaVersion=1: width, height, frameMs (GIF: multiple of 10),
frameCount, rowOrder='top-to-bottom', audio (JSON path), left/right objects
{label, rgb}. Paths are relative to the manifest. Audio is a list of
{frame, ms, level, bass, treble, beat}; timestamps start at zero, uniformly.
Required provenance and fidelityNote describe the actual input, not a guessed
production identity. Optional signalLabels, caption and settings are presentation
metadata. Other metadata (e.g. sourceHashes/compilerCommand) is retained in report.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import tempfile
from typing import Any

from PIL import Image, ImageDraw, ImageFont

SIGNALS = ("level", "bass", "treble")
BACKGROUND = (15, 18, 24)
TEXT = (245, 245, 245)
MUTED = (190, 198, 210)
COLORS = ((80, 200, 151), (243, 152, 83), (128, 163, 245))
UI_COLORS = (BACKGROUND, TEXT, MUTED, *COLORS, (68, 73, 85), (39, 44, 53), (170, 179, 194))


def positive_int(value, name):
    if type(value) is not int or value <= 0:
        raise ValueError(f"{name} must be a positive integer")
    return value


def validate_audio(audio, count, frame_ms):
    if not isinstance(audio, list) or len(audio) != count:
        raise ValueError(f"audio must contain exactly {count} frames")
    for i, row in enumerate(audio):
        if not isinstance(row, dict) or type(row.get("frame")) is not int or row["frame"] != i:
            raise ValueError(f"audio[{i}].frame must equal {i}")
        if type(row.get("ms")) is not int or row["ms"] != i * frame_ms:
            raise ValueError(f"audio[{i}].ms must equal {i * frame_ms}; timestamps must be uniform and increasing")
        for key in SIGNALS:
            if type(row.get(key)) is not int or not 0 <= row[key] <= 255:
                raise ValueError(f"audio[{i}].{key} must be an integer in 0..255")
        if type(row.get("beat")) is not bool:
            raise ValueError(f"audio[{i}].beat must be boolean")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_capture(path):
    path = Path(path).expanduser().resolve()
    manifest = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(manifest, dict) or type(manifest.get("schemaVersion")) is not int or manifest["schemaVersion"] != 1:
        raise ValueError("manifest schemaVersion must be 1")
    for name in ("width", "height", "frameMs", "frameCount"):
        positive_int(manifest.get(name), name)
    if manifest["frameMs"] % 10:
        raise ValueError("frameMs must be a multiple of 10 ms for exact GIF timing")
    if manifest.get("rowOrder") != "top-to-bottom":
        raise ValueError("rowOrder must be 'top-to-bottom'")
    for name in ("provenance", "fidelityNote"):
        if not isinstance(manifest.get(name), str) or not manifest[name].strip():
            raise ValueError(f"{name} must describe the capture as a nonempty string")
    paths = {"manifest": path}
    def resolve(value, name):
        if not isinstance(value, str) or not value:
            raise ValueError(f"{name} must be a file path")
        return (path.parent / value).resolve()
    paths["audio"] = resolve(manifest.get("audio"), "audio")
    audio = json.loads(paths["audio"].read_text(encoding="utf-8"))
    validate_audio(audio, manifest["frameCount"], manifest["frameMs"])
    data = {}
    expected = manifest["width"] * manifest["height"] * 3 * manifest["frameCount"]
    for side in ("left", "right"):
        pane = manifest.get(side)
        if not isinstance(pane, dict) or not isinstance(pane.get("label"), str) or not pane["label"].strip():
            raise ValueError(f"{side}.label must be a nonempty string")
        paths[side] = resolve(pane.get("rgb"), f"{side}.rgb")
        if paths[side].stat().st_size != expected:
            raise ValueError(f"{side}.rgb must contain {expected} RGB24 bytes, got {paths[side].stat().st_size}")
        data[side] = paths[side].read_bytes()
    labels = manifest.get("signalLabels", ["Level", "Bass", "Treble"])
    if not isinstance(labels, list) or len(labels) != 3 or any(not isinstance(s, str) or not s for s in labels):
        raise ValueError("signalLabels must contain three nonempty strings")
    if "caption" in manifest and not isinstance(manifest["caption"], str):
        raise ValueError("caption must be a string")
    return manifest, audio, data, paths


def resolve_font(requested=None, text=""):
    candidates = [str(Path(requested).expanduser())] if requested else [
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "DejaVuSans.ttf",
    ]
    needed = set(text + "Без звука Три сигнала")
    for candidate in candidates:
        try:
            font = ImageFont.truetype(candidate, 19)
            def glyph(char):
                box = font.getbbox(char)
                size = (max(1, int(box[2] - box[0])), max(1, int(box[3] - box[1])))
                image = Image.new("L", size)
                ImageDraw.Draw(image).text((-box[0], -box[1]), char, font=font, fill=255)
                return size, image.tobytes()
            missing = glyph(chr(0x10FFFF))
            if any(glyph(c) == missing for c in needed if not c.isspace()):
                continue
            return str(font.path)
        except (OSError, ValueError):
            continue
    raise ValueError("No font with required Cyrillic glyphs found. Install DejaVu Sans or pass --font /path/to/Arial.ttf")


def compose(manifest_path, out, font=None) -> dict[str, Any]:
    manifest, audio, data, paths = load_capture(manifest_path)
    out = Path(out).expanduser().resolve()
    destinations = (out / "comparison.gif", out / "report.json")
    if any(p in paths.values() for p in destinations):
        raise ValueError("output would overwrite a capture input; choose a different --out")
    labels = manifest.get("signalLabels", ["Level", "Bass", "Treble"])
    font_path = resolve_font(font, " ".join([manifest["left"]["label"], manifest["right"]["label"], manifest["provenance"], manifest["fidelityNote"], manifest.get("caption", ""), *labels]))
    normal = ImageFont.truetype(font_path, 19)
    small = ImageFont.truetype(font_path, 14)
    title = ImageFont.truetype(font_path, 22)
    width, height, count, step = (manifest[k] for k in ("width", "height", "frameCount", "frameMs"))
    stride = width * height * 3
    def raw(side, i):
        return Image.frombytes("RGB", (width, height), data[side][i * stride:(i + 1) * stride])
    # The atlas includes both lanes at every timestamp. Every rendered GIF frame
    # uses this ONE learned palette with dithering disabled (including UI colors).
    columns = min(40, count * 2)
    atlas = Image.new("RGB", (columns * width, ((count * 2 + columns - 1) // columns) * height))
    for i in range(count):
        for lane, side in enumerate(("left", "right")):
            n = i * 2 + lane
            atlas.paste(raw(side, i), ((n % columns) * width, (n // columns) * height))
    palette = atlas.quantize(colors=240, dither=Image.Dither.NONE)
    learned = palette.getpalette()
    if learned is None:
        raise ValueError("Pillow did not produce an indexed capture palette")
    colors = learned[:720]
    for color in UI_COLORS:
        colors.extend(color)
    colors.extend([0] * (768 - len(colors)))
    palette.putpalette(colors)
    def fit(draw, xy, text, limit, use_font=small, fill=MUTED):
        while text and draw.textlength(text, font=use_font) > limit:
            text = text[:-1]
        draw.text(xy, text, font=use_font, fill=fill)
    timeline = Image.new("RGB", (640, 480), BACKGROUND)
    draw = ImageDraw.Draw(timeline)
    for k, (key, color) in enumerate(zip(SIGNALS, COLORS)):
        top = 40 + k * 8
        for row in audio:
            x = 24 + row["frame"] * 592 / count
            draw.line((x, top + 6, x, top + 6 - row[key] * 6 / 255), fill=color)
    frames = []
    for i, row in enumerate(audio):
        image = timeline.copy()
        draw = ImageDraw.Draw(image)
        draw.text((24, 8), f'{row["ms"] / 1000:05.2f} / {count * step / 1000:05.2f} s', font=title, fill=TEXT)
        fit(draw, (325, 12), manifest["provenance"], 290)
        marker = 24 + i * 592 / count
        draw.line((marker, 38, marker, 65), fill=TEXT, width=2)
        if row["beat"]:
            draw.text((570, 26), "beat", font=small, fill=COLORS[1])
        for column, side in enumerate(("left", "right")):
            x = 24 + 320 * column
            fit(draw, (x, 73), manifest[side]["label"], 280, normal, TEXT)
            scale = min(272 / width, 272 / height)
            size = (max(1, round(width * scale)), max(1, round(height * scale)))
            origin = (x + (272 - size[0]) // 2, 104 + (272 - size[1]) // 2)
            image.paste(raw(side, i).resize(size, Image.Resampling.NEAREST), origin)
            draw.rectangle((x - 1, 103, x + 272, 376), outline=(68, 73, 85))
        for k, (key, label, color) in enumerate(zip(SIGNALS, labels, COLORS)):
            x = 24 + k * 205
            fit(draw, (x, 385), f'{label} {row[key]}', 190, fill=TEXT)
            draw.rectangle((x, 409, x + 180, 422), fill=(39, 44, 53))
            if row[key]:
                draw.rectangle((x, 409, x + 180 * row[key] / 255, 422), fill=color)
        fit(draw, (24, 437), manifest.get("caption", ""), 592)
        fit(draw, (24, 457), manifest["fidelityNote"], 592, fill=(170, 179, 194))
        frames.append(image.quantize(palette=palette, dither=Image.Dither.NONE))
    out.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".compose-", dir=out) as temporary:
        gif = Path(temporary) / "comparison.gif"
        frames[0].save(gif, save_all=True, append_images=frames[1:], duration=step, loop=0, optimize=False, disposal=1)
        with Image.open(gif) as check:
            duration = 0
            encoded_count = getattr(check, "n_frames", 1)
            for i in range(encoded_count):
                check.seek(i)
                duration += check.info.get("duration", 0)
            if encoded_count != count or duration != count * step or check.size != (640, 480):
                raise ValueError("GIF encoder changed frame count/timing; no output published")
        report = dict(schemaVersion=1, width=width, height=height, rowOrder="top-to-bottom", frameCount=count, frameMs=step,
                      durationMs=duration, gifSize=[640, 480], gifBytes=gif.stat().st_size, font=font_path,
                      inputHashes={k: digest(p) for k, p in paths.items()}, inputPaths={k: str(p) for k, p in paths.items()},
                      paletteSha256=hashlib.sha256(bytes(colors)).hexdigest(), dither=False,
                      provenance=manifest["provenance"], fidelityNote=manifest["fidelityNote"], captureMetadata=manifest,
                      outputs={"gif": str(destinations[0]), "report": str(destinations[1])},
                      limitations=["GIF uses a shared 256-color palette; RGB captures are authoritative.", "Presentation does not certify hardware, microphone or WASM fidelity."])
        report_path = Path(temporary) / "report.json"
        report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        gif.replace(destinations[0])
        report_path.replace(destinations[1])
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--font", help="Arial/DejaVu Sans font file with Cyrillic support")
    args = parser.parse_args()
    try:
        report = compose(args.manifest, args.out, args.font)
    except (OSError, ValueError, KeyError) as error:
        parser.exit(2, f"audio comparison: {error}\n")
    print(json.dumps(report["outputs"], ensure_ascii=False))


if __name__ == "__main__":
    main()
