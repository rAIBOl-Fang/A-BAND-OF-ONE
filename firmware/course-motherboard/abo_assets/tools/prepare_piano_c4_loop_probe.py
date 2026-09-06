"""Build a non-destructive A/B preview for the piano C4 sustain loop.

Candidate A is an exact copy of the current formal EIAD asset. Candidate B
keeps the original SFZ loop source, joins it to the attack with a short
crossfade, and compensates the quiet sustain for the keyboard speaker. The
formal delivery asset and its manifest are never overwritten.
"""

import importlib.util
import json
import math
import re
import shutil
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RAW = ROOT / "raw"
DELIVERY = ROOT / "delivery"
FORMAL_ASSET = DELIVERY / "piano_c4.eiad"
FORMAL_MANIFEST = DELIVERY / "manifest.json"
SFZ = RAW / "piano-upright-kw" / "UprightPianoKW-small-20190703.sfz"
SOURCE = RAW / "piano-upright-kw" / "samples" / "C4vH.wav"
OUTPUT = DELIVERY / "piano-c4-loop-probe"

BASE_TOOL = Path(__file__).resolve().with_name("prepare_p1_assets.py")
SPEC = importlib.util.spec_from_file_location("prepare_p1_assets", BASE_TOOL)
BASE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BASE)

RATE = BASE.RATE
TARGET_PEAK = BASE.TARGET_PEAK
ATTACK_SAMPLES = round(1.20 * RATE)
TRANSITION_SAMPLES = round(0.12 * RATE)
SUSTAIN_GAIN_DB = 6.0
B2_SUSTAIN_GAIN_DB = 15.0


def digest(path: Path) -> str:
    return BASE.digest(path)


def read_wav(path: Path):
    return BASE.read_wav(path)


def resample(samples, source_rate, count):
    return BASE.resample(samples, source_rate, count)


def normalize(samples):
    return BASE.normalize(samples)


def encode_eiad(samples):
    return BASE.encode_eiad(samples)


def read_sfz_c4_loop(path: Path):
    text = Path(path).read_text(encoding="utf-8")
    match = re.search(
        r"loop_start=(\d+)\s+loop_end=(\d+)\s+sample=samples/C4vH\.wav",
        text,
    )
    assert match, f"C4 SFZ loop is missing: {path}"
    return int(match.group(1)), int(match.group(2))


def clip_to_target(value: float) -> int:
    return max(-TARGET_PEAK, min(TARGET_PEAK, round(value)))


def apply_gain(samples, decibels: float):
    factor = 10 ** (decibels / 20)
    return [clip_to_target(sample * factor) for sample in samples]


def crossfade(left, right):
    assert len(left) == len(right) and len(left) > 0
    last = len(left) - 1
    return [
        clip_to_target(left[index] * (1 - index / last) + right[index] * (index / last))
        for index in range(len(left))
    ]


def bake_loop_crossfade(samples, loop_start, loop_end, fade_samples):
    assert 0 <= loop_start < loop_end <= len(samples)
    assert 0 < fade_samples <= loop_end - loop_start
    output = list(samples)
    for offset in range(fade_samples):
        tail_index = loop_end - fade_samples + offset
        head_index = loop_start + offset
        fade = offset / max(1, fade_samples - 1)
        output[tail_index] = clip_to_target(
            samples[tail_index] * (1 - fade) + samples[head_index] * fade
        )
    return output


def build_corrected_asset(samples, source_rate, native_loop, sustain_gain_db=SUSTAIN_GAIN_DB):
    target_count = round(len(samples) * RATE / source_rate)
    full = normalize(resample(samples, source_rate, target_count))
    native_start, native_end = native_loop
    loop_start = round(native_start * RATE / source_rate)
    loop_end = round(native_end * RATE / source_rate)
    assert 0 <= loop_start < loop_end <= len(full)
    assert ATTACK_SAMPLES > TRANSITION_SAMPLES

    attack = full[:ATTACK_SAMPLES]
    sustain = apply_gain(full[loop_start:loop_end], sustain_gain_db)
    assert len(sustain) > TRANSITION_SAMPLES

    # The transition consumes the first 120 ms of the sustain segment. This
    # keeps attack->sustain continuous, while leaving a clean, later loop
    # region that can itself be crossfaded at the original SFZ boundary.
    transition = crossfade(attack[-TRANSITION_SAMPLES:], sustain[:TRANSITION_SAMPLES])
    combined = attack[:-TRANSITION_SAMPLES] + transition + sustain[TRANSITION_SAMPLES:]
    raw_loop_start = ATTACK_SAMPLES
    raw_loop_end = raw_loop_start + len(sustain) - TRANSITION_SAMPLES
    baked = bake_loop_crossfade(
        combined, raw_loop_start, raw_loop_end, TRANSITION_SAMPLES
    )
    return {
        "samples": baked,
        "sample_rate_hz": RATE,
        "total_samples": len(baked),
        "loop_start_sample": raw_loop_start + TRANSITION_SAMPLES,
        "loop_end_sample": raw_loop_end,
        "source_loop_start_sample": loop_start,
        "source_loop_end_sample": loop_end,
        "transition_samples": TRANSITION_SAMPLES,
        "sustain_gain_db": sustain_gain_db,
    }


def baseline_record(formal_manifest, formal_asset):
    source = next(asset for asset in formal_manifest["assets"] if asset["id"] == "piano_c4")
    return {
        **source,
        "id": "piano_c4_baseline",
        "label": "A · 当前正式资产",
        "file": formal_asset.name,
        "candidate": "formal-copy",
    }


def corrected_record(asset_id, label, candidate, corrected, encoded_path, source_loop, source_path, sfz_path):
    return {
        "id": asset_id,
        "label": label,
        "file": encoded_path.name,
        "format": "EIAD-v1",
        "sample_rate_hz": RATE,
        "channels": 1,
        "pcm_bits": 16,
        "normalization_dbfs": -3,
        "source": f"raw/piano-upright-kw/{source_path.name}",
        "source_sha256": digest(source_path),
        "source_sfz": f"raw/piano-upright-kw/{sfz_path.name}",
        "source_sfz_sha256": digest(sfz_path),
        "source_sfz_loop_start_sample": source_loop[0],
        "source_sfz_loop_end_sample": source_loop[1],
        "source_loop_start_sample": corrected["source_loop_start_sample"],
        "source_loop_end_sample": corrected["source_loop_end_sample"],
        "attack_samples": ATTACK_SAMPLES,
        "transition_samples": corrected["transition_samples"],
        "sustain_gain_db": corrected["sustain_gain_db"],
        "total_samples": corrected["total_samples"],
        "loop_start_sample": corrected["loop_start_sample"],
        "loop_end_sample": corrected["loop_end_sample"],
        "encoded_bytes": encoded_path.stat().st_size,
        "eiad_sha256": digest(encoded_path),
        "candidate": candidate,
    }


def main() -> None:
    assert FORMAL_ASSET.is_file(), f"missing formal asset: {FORMAL_ASSET}"
    assert FORMAL_MANIFEST.is_file(), f"missing formal manifest: {FORMAL_MANIFEST}"
    assert SOURCE.is_file(), f"missing C4 source: {SOURCE}"
    assert SFZ.is_file(), f"missing piano SFZ: {SFZ}"

    OUTPUT.mkdir(parents=True, exist_ok=True)
    baseline_path = OUTPUT / "piano_c4_baseline.eiad"
    corrected_path = OUTPUT / "piano_c4_sfz_loop.eiad"
    corrected_b2_path = OUTPUT / "piano_c4_sfz_loop_b2.eiad"
    shutil.copyfile(FORMAL_ASSET, baseline_path)

    source_rate, raw = read_wav(SOURCE)
    source_loop = read_sfz_c4_loop(SFZ)
    corrected = build_corrected_asset(raw, source_rate, source_loop)
    corrected_b2 = build_corrected_asset(raw, source_rate, source_loop, B2_SUSTAIN_GAIN_DB)
    corrected_path.write_bytes(encode_eiad(corrected["samples"]))
    corrected_b2_path.write_bytes(encode_eiad(corrected_b2["samples"]))

    formal_manifest = json.loads(FORMAL_MANIFEST.read_text(encoding="utf-8"))
    records = [
        baseline_record(formal_manifest, baseline_path),
        corrected_record(
            "piano_c4_sfz_loop",
            "B · 原 SFZ 循环修正版",
            "sfz-loop-crossfade",
            corrected,
            corrected_path,
            source_loop,
            SOURCE,
            SFZ,
        ),
        corrected_record(
            "piano_c4_sfz_loop_b2",
            "B2 · 原 SFZ 循环 +15dB",
            "sfz-loop-crossfade-louder",
            corrected_b2,
            corrected_b2_path,
            source_loop,
            SOURCE,
            SFZ,
        ),
    ]
    manifest = {
        "bundle": "piano-c4-loop-probe",
        "purpose": "Non-destructive browser A/B for the P1 piano C4 long-hold loop",
        "formal_asset_unchanged": True,
        "assets": records,
        "total_encoded_bytes": sum(record["encoded_bytes"] for record in records),
    }
    (OUTPUT / "manifest.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    print(
        "C4 loop probe built: "
        f"A/B/B2, {manifest['total_encoded_bytes']} bytes; "
        f"B loop={records[1]['loop_start_sample']}..{records[1]['loop_end_sample']}"
    )


if __name__ == "__main__":
    main()
