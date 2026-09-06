"""Validate incoming self-recorded suona WAV files without modifying them."""

import argparse
import hashlib
import json
import wave
from pathlib import Path


EXPECTED = [
    "suona_sus_mf_C3.wav",
    "suona_sus_mf_Fs3.wav",
    "suona_sus_mf_C4.wav",
    "suona_sus_mf_Fs4.wav",
    "suona_sus_mf_C5.wav",
    "suona_sus_mf_Fs5.wav",
    "suona_sus_mf_C6.wav",
]
LICENSE = "LICENSE-RELEASE.txt"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inspect_wav(path: Path) -> dict:
    with wave.open(str(path), "rb") as source:
        channels = source.getnchannels()
        sample_rate_hz = source.getframerate()
        pcm_bits = source.getsampwidth() * 8
        frames = source.getnframes()
    return {
        "bytes": path.stat().st_size,
        "sha256": sha256(path),
        "channels": channels,
        "sample_rate_hz": sample_rate_hz,
        "pcm_bits": pcm_bits,
        "duration_seconds": frames / sample_rate_hz,
    }


def validate_directory(folder: Path) -> dict:
    folder = Path(folder)
    errors, files = [], {}
    for name in EXPECTED:
        path = folder / name
        if not path.is_file():
            errors.append(f"missing required file: {name}")
            continue
        try:
            details = inspect_wav(path)
            files[name] = details
        except (wave.Error, EOFError) as error:
            errors.append(f"invalid WAV {name}: {error}")
            continue
        if details["sample_rate_hz"] != 48000:
            errors.append(f"{name}: expected 48000Hz, got {details['sample_rate_hz']}Hz")
        if details["channels"] != 1:
            errors.append(f"{name}: expected mono, got {details['channels']} channels")
        if details["pcm_bits"] not in (16, 24):
            errors.append(f"{name}: expected 16-bit or 24-bit PCM, got {details['pcm_bits']}-bit")
        if not 4 <= details["duration_seconds"] <= 7:
            errors.append(f"{name}: expected 4-7 seconds, got {details['duration_seconds']:.3f}s")

    release = folder / LICENSE
    if not release.is_file():
        errors.append(f"missing required file: {LICENSE}")
    elif "CC0 1.0" not in release.read_text(encoding="utf-8"):
        errors.append(f"{LICENSE}: must explicitly contain CC0 1.0")

    return {"ok": not errors, "folder": str(folder), "files": files, "errors": errors}


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", nargs="?", type=Path,
                        default=root / "incoming" / "suona-self-recording")
    arguments = parser.parse_args()
    report = validate_directory(arguments.folder)
    print(json.dumps(report, ensure_ascii=False, indent=2))
    raise SystemExit(0 if report["ok"] else 1)


if __name__ == "__main__":
    main()
