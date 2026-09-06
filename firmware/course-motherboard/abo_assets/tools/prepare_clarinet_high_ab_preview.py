"""Build an isolated high-note B1 clarinet A/B EIAD preview bundle."""

import json
import shutil
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from prepare_p1_assets import RATE, digest, encode_eiad, normalize, read_wav
from prepare_clarinet_eiad_preview import bake_loop_crossfade


ROOT = Path(__file__).resolve().parents[1]
RAW = ROOT / "raw"
OUTPUT = ROOT / "delivery" / "clarinet-tonejs-high-ab-preview"
ATTRIBUTION = ROOT / "incoming" / "tonejs-clarinet-probe" / "ATTRIBUTION.md"
SECONDS = 0.8
LOOP_START_SAMPLE = round(0.40 * RATE)
CROSSFADE_SAMPLES = round(0.020 * RATE)
SOURCES = [
    ("clarinet_d3", "clarinet-tonejs/clarinet_d3.wav", "D3"),
    ("clarinet_f3", "clarinet-tonejs/clarinet_f3.wav", "F3"),
    ("clarinet_as3", "clarinet-tonejs/clarinet_as3.wav", "A#3"),
    ("clarinet_d4", "clarinet-tonejs/clarinet_d4.wav", "D4"),
    ("clarinet_f4", "clarinet-tonejs/clarinet_f4.wav", "F4"),
    ("clarinet_as4", "clarinet-tonejs/clarinet_as4.wav", "A#4"),
    ("clarinet_d5", "clarinet-tonejs-high-ab/clarinet_d5_b1.wav", "D5"),
    ("clarinet_f5", "clarinet-tonejs-high-ab/clarinet_f5_b1.wav", "F5"),
    ("clarinet_as5", "clarinet-tonejs-high-ab/clarinet_as5_b1.wav", "A#5"),
]
LOOP_END_SAMPLES = {
    "clarinet_d3": 27984, "clarinet_f3": 36912, "clarinet_as3": 27984,
    "clarinet_d4": 32400, "clarinet_f4": 29472, "clarinet_as4": 31056,
    "clarinet_d5": 28656, "clarinet_f5": 28608, "clarinet_as5": 27264,
}


def main():
    assert ATTRIBUTION.is_file(), f"missing attribution: {ATTRIBUTION}"
    OUTPUT.mkdir(parents=True, exist_ok=True)
    records, total_bytes = [], 0
    expected_samples = round(SECONDS * RATE)
    for asset_id, relative, root_note in SOURCES:
        source = RAW / relative
        rate, raw = read_wav(source)
        assert rate == RATE, f"{relative}: expected {RATE}Hz, got {rate}Hz"
        assert len(raw) == expected_samples, f"{relative}: expected {expected_samples} samples, got {len(raw)}"
        loop_end = LOOP_END_SAMPLES[asset_id]
        baked = normalize(bake_loop_crossfade(raw, LOOP_START_SAMPLE, loop_end, CROSSFADE_SAMPLES))
        encoded = encode_eiad(baked)
        target = OUTPUT / f"{asset_id}.eiad"
        target.write_bytes(encoded)
        records.append({
            "id": asset_id, "root_note": root_note, "file": target.name, "format": "EIAD-v1",
            "sample_rate_hz": RATE, "channels": 1, "pcm_bits": 16, "normalization_dbfs": -3,
            "source": f"raw/{relative}", "source_sha256": digest(source), "total_samples": len(baked),
            "raw_loop_start_sample": LOOP_START_SAMPLE, "raw_loop_end_sample": loop_end,
            "loop_start_sample": LOOP_START_SAMPLE + CROSSFADE_SAMPLES, "loop_end_sample": loop_end,
            "crossfade_samples": CROSSFADE_SAMPLES, "encoded_bytes": len(encoded), "eiad_sha256": digest(target),
        })
        total_bytes += len(encoded)
    shutil.copy2(ATTRIBUTION, OUTPUT / "ATTRIBUTION.md")
    (OUTPUT / "manifest.json").write_text(json.dumps({
        "bundle": "clarinet-tonejs-high-ab-preview",
        "comparison": "B1 replaces D5/F5/A#5 only; baseline remains untouched",
        "license": "CC-BY-3.0 (project-level accepted; original author unverified)",
        "assets": records, "total_encoded_bytes": total_bytes,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"clarinet high-note A/B preview built: {len(records)} assets, {total_bytes} bytes")


if __name__ == "__main__":
    main()
