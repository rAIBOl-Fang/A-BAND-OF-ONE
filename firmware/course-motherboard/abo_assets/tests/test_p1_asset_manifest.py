import hashlib
import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MANIFEST_PATH = ROOT / "delivery" / "manifest.json"
EXPECTED_IDS = {
    "piano_b2", "piano_fis3", "piano_c4", "piano_fis4", "piano_c5", "piano_fis5", "piano_c6",
    "violin_g3", "violin_c4", "violin_e4", "violin_g4", "violin_c5", "violin_e5", "violin_a5",
    "clarinet_d3", "clarinet_f3", "clarinet_as3", "clarinet_d4", "clarinet_f4", "clarinet_as4", "clarinet_d5", "clarinet_f5", "clarinet_as5",
}
EXPECTED_TOTAL_PAYLOAD_BYTES = 1309174
EXPECTED_PCM_BYTES_BY_INSTRUMENT = {
    "piano": 1402514,
    "violin": 1008000,
    "clarinet": 691200,
}
MAX_SINGLE_INSTRUMENT_PCM_BYTES = 1402514
PSRAM_BUDGET_BYTES = 2 * 1024 * 1024
ATTRIBUTION_PATH = ROOT / "delivery" / "ATTRIBUTION.md"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    assert MANIFEST_PATH.is_file(), "P1 delivery manifest is missing"
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    assets = manifest["assets"]
    assert {asset["id"] for asset in assets} == EXPECTED_IDS
    assert len(assets) == 23

    total_payload_bytes = 0
    for asset in assets:
        assert asset["sample_rate_hz"] == 48000
        assert asset["channels"] == 1
        assert asset["pcm_bits"] == 16
        assert asset["normalization_dbfs"] == -3
        assert asset["source_sha256"]
        assert asset["loop_start_sample"] < asset["loop_end_sample"] <= asset["total_samples"]
        encoded = ROOT / "delivery" / asset["file"]
        assert encoded.is_file(), f"missing encoded asset: {encoded.name}"
        assert encoded.stat().st_size == asset["encoded_bytes"]
        if asset["id"].startswith("clarinet_"):
            assert asset["format"] == "PCM16LE-v1"
            assert asset["total_samples"] == 38400
            assert asset["raw_loop_start_sample"] == 19200
            assert asset["loop_start_sample"] == 20160
            assert asset["crossfade_samples"] == 960
            assert asset["encoded_bytes"] == 76800
            assert asset["pcm_sha256"] == sha256(encoded)
            assert encoded.suffix == ".pcm16le"
        else:
            assert asset["format"] == "EIAD-v1"
            assert sha256(encoded) == asset["eiad_sha256"]
        if asset["id"] == "piano_c4":
            assert asset["total_samples"] == 125257
            assert asset["loop_start_sample"] == 63360
            assert asset["loop_end_sample"] == 125257
            assert asset["transition_samples"] == 5760
            assert asset["sustain_gain_db"] == 6.0
            assert asset["source_sfz_loop_start_sample"] == 145927
            assert asset["source_sfz_loop_end_sample"] == 213379
        total_payload_bytes += asset["encoded_bytes"]

    assert total_payload_bytes == EXPECTED_TOTAL_PAYLOAD_BYTES

    pcm_bytes_by_instrument = {}
    for asset in assets:
        instrument = asset["id"].split("_", 1)[0]
        pcm_bytes_by_instrument[instrument] = pcm_bytes_by_instrument.get(instrument, 0) + asset["total_samples"] * 2

    assert pcm_bytes_by_instrument == EXPECTED_PCM_BYTES_BY_INSTRUMENT
    max_single_instrument_pcm = max(pcm_bytes_by_instrument.values())
    assert max_single_instrument_pcm == MAX_SINGLE_INSTRUMENT_PCM_BYTES
    assert max_single_instrument_pcm < PSRAM_BUDGET_BYTES
    assert pcm_bytes_by_instrument["piano"] + pcm_bytes_by_instrument["violin"] > PSRAM_BUDGET_BYTES

    assert ATTRIBUTION_PATH.is_file(), "delivery attribution is missing"
    attribution = ATTRIBUTION_PATH.read_text(encoding="utf-8")
    for marker in (
        "FreePats Upright Piano KW",
        "VSCO-2-CE Solo Violin",
        "nbrosowsky/tonejs-instruments",
        "CC0 1.0",
        "CC BY 3.0",
        "原始作者未核实",
        "manifest.json",
    ):
        assert marker in attribution, f"missing attribution marker: {marker}"

    assert manifest["total_payload_bytes"] == EXPECTED_TOTAL_PAYLOAD_BYTES
    assert manifest["storage_contract"] == "14 EIAD-v1 + 9 PCM16LE-v1"
    print(f"P1 asset manifest passed: 23 assets, {total_payload_bytes} bytes")


if __name__ == "__main__":
    main()
