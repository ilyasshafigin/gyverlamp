#!/usr/bin/env python3
"""Fresh fixed public-API native capture example, followed by generic compose.

This is one limited example, not a general effect/audio exporter. No microphone,
ADC, firmware upload or active-audio WASM fidelity is implied.
"""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

from compose import compose, digest, positive_int, validate_audio

ROOT = Path(__file__).resolve().parents[3]
DEFAULT_FIXTURE = ROOT / "sim/fixtures/audio/example-three-signal.json"


def run_example(out, fixture=DEFAULT_FIXTURE, cxx=None, font=None):
    out = Path(out).expanduser().resolve()
    fixture = Path(fixture).expanduser().resolve()
    audio = json.loads(fixture.read_text(encoding="utf-8"))
    if not isinstance(audio, list) or len(audio) < 2:
        raise ValueError("example fixture needs at least two uniform audio frames")
    step = positive_int(audio[1].get("ms") if isinstance(audio[1], dict) else None, "fixture frameMs")
    if step % 10:
        raise ValueError("fixture frameMs must be a multiple of 10 ms")
    validate_audio(audio, len(audio), step)
    if audio[-1]["ms"] > 0xFFFFFFFF:
        raise ValueError("fixture timestamps exceed the native uint32 clock")
    if fixture.parent == out and fixture.name in ("audio.json", "capture.json", "report.json"):
        raise ValueError("fixture would be overwritten; choose a separate --out")
    compiler = [cxx] if cxx else shlex.split(os.environ.get("CXX", ""))
    if not compiler:
        compiler = [shutil.which("clang++") or shutil.which("c++") or "clang++"]
    if not shutil.which(compiler[0]):
        raise ValueError("C++ compiler unavailable; install clang++ or pass --cxx /path/to/compiler (or set CXX)")
    out.mkdir(parents=True, exist_ok=True)
    (out / "audio.json").write_text(json.dumps(audio, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    header = f"static constexpr unsigned kAudioFrameCount={len(audio)};\nstatic constexpr unsigned kAudioFrameMs={step};\nstatic const AudioFrame kAudioFrames[]={{\n"
    header += ",\n".join("{%d,%d,%d,%s,true}" % (r["level"], r["bass"], r["treble"], str(r["beat"]).lower()) for r in audio)
    (out / "audio_fixture.h").write_text(header + "\n};\n", encoding="utf-8")
    sources = [
        "sim/tools/audio_compare/capture_example.cpp", "src/effect/catalog/living_membrane.cpp",
        "src/effect/shared.cpp", "src/effect/math.cpp", "sim/host/src/host_led.cpp", "sim/host/src/sim_stubs.cpp",
        "sim/common/sim_random.cpp", "sim/common/sim_time.cpp",
    ]
    includes = ["sim/host/shims", "sim/host/src", "sim/common", "src"]
    binary = out / "capture-living-membrane"
    command = compiler + ["-std=c++17", "-O2", "-DSIMULATOR_AUDIO_INPUT"]
    command += ["-I" + str(ROOT / path) for path in includes] + ["-I" + str(out), "-iquote", str(ROOT / "src/effect")]
    command += [str(ROOT / path) for path in sources] + ["-o", str(binary)]
    subprocess.run(command, cwd=ROOT, check=True)
    dimensions = json.loads(subprocess.check_output([str(binary), "--info"], cwd=out, text=True))
    for lane in ("off", "on"):
        subprocess.run([str(binary), lane, str(out / (lane + ".rgb"))], cwd=out, check=True)
    hashes = {path: digest(ROOT / path) for path in sources + ["src/effect/catalog/living_membrane.h"]}
    manifest = dict(schemaVersion=1, **dimensions, frameMs=step, frameCount=len(audio), rowOrder="top-to-bottom", audio="audio.json",
                    left=dict(label="Без аудиореакции", rgb="off.rgb"), right=dict(label="Три сигнала", rgb="on.rgb"),
                    provenance="Native production · синт. AudioFrame",
                    fidelityNote="Host shims; post-mic синтетика, без ADC/микрофонной задержки и WASM audio parity",
                    caption="LivingMembrane · amount 255 · три независимых синтетических канала",
                    signalLabels=["Level · свет", "Bass · сдвиг", "Treble · цвет"],
                    settings=dict(effect="LivingMembrane", seed=12345, brightness=255, speed=128, scale=160, amount=255,
                                  band="Bass", bandIgnoredByThisEffect=True, beatUnusedByThisEffect=True,
                                  palette="16 CHSV(i*16,240,255) colors; native FastLED host shim"),
                    sourceHashes=hashes, fixturePath=str(fixture), fixtureSha256=digest(fixture),
                    generatedFixtureSha256=digest(out / "audio_fixture.h"), compilerCommand=command,
                    sourceHashScope="Listed translation units plus effect header, not a complete dependency identity; compilation is fresh.")
    capture = out / "capture.json"
    capture.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    report = compose(capture, out, font)
    return dict(capture=str(capture), off=str(out / "off.rgb"), on=str(out / "on.rgb"), audio=str(out / "audio.json"), **report["outputs"])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--fixture", type=Path, default=DEFAULT_FIXTURE, help="uniform JSON AudioFrame fixture; defaults to tracked 400x50ms diagnostic sequence")
    parser.add_argument("--cxx", help="compiler executable; otherwise CXX or clang++/c++")
    parser.add_argument("--font", help="Cyrillic-capable Arial/DejaVu Sans font path")
    args = parser.parse_args()
    try:
        result = run_example(args.out, args.fixture, args.cxx, args.font)
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        parser.exit(2, f"native audio example: {error}\n")
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
