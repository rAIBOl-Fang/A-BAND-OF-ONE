import importlib.util
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "tools" / "prepare_piano_c4_loop_probe.py"
SFZ = ROOT / "raw" / "piano-upright-kw" / "UprightPianoKW-small-20190703.sfz"
OUTPUT = ROOT / "delivery" / "piano-c4-loop-probe"


def load_tool():
    spec = importlib.util.spec_from_file_location("prepare_piano_c4_loop_probe", TOOL)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> None:
    tool = load_tool()
    assert tool.ATTACK_SAMPLES == 57_600
    assert tool.TRANSITION_SAMPLES == 5_760
    assert tool.SUSTAIN_GAIN_DB == 6.0
    assert tool.B2_SUSTAIN_GAIN_DB == 15.0
    assert tool.read_sfz_c4_loop(SFZ) == (145_927, 213_379)

    source_rate, raw = tool.read_wav(ROOT / "raw" / "piano-upright-kw" / "samples" / "C4vH.wav")
    corrected = tool.build_corrected_asset(raw, source_rate, tool.read_sfz_c4_loop(SFZ))
    corrected_b2 = tool.build_corrected_asset(
        raw, source_rate, tool.read_sfz_c4_loop(SFZ), tool.B2_SUSTAIN_GAIN_DB
    )
    assert corrected["loop_start_sample"] == tool.ATTACK_SAMPLES + tool.TRANSITION_SAMPLES
    assert corrected["transition_samples"] == tool.TRANSITION_SAMPLES
    assert corrected["loop_end_sample"] > corrected["loop_start_sample"]
    assert corrected["loop_end_sample"] <= corrected["total_samples"]
    assert corrected["total_samples"] > tool.ATTACK_SAMPLES

    loop_head = corrected["samples"][corrected["loop_start_sample"]]
    loop_tail = corrected["samples"][corrected["loop_end_sample"] - 1]
    assert abs(loop_tail - loop_head) <= 500, (loop_tail, loop_head)
    b2_loop = corrected_b2["samples"][corrected_b2["loop_start_sample"]:corrected_b2["loop_end_sample"]]
    assert sum(value * value for value in b2_loop) > sum(value * value for value in corrected["samples"][corrected["loop_start_sample"]:corrected["loop_end_sample"]]) * 6
    assert max(abs(value) for value in b2_loop) <= tool.TARGET_PEAK

    manifest_path = OUTPUT / "manifest.json"
    assert manifest_path.is_file(), "run prepare_piano_c4_loop_probe.py first"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    assert {asset["id"] for asset in manifest["assets"]} == {
        "piano_c4_baseline",
        "piano_c4_sfz_loop",
        "piano_c4_sfz_loop_b2",
    }
    assert manifest["assets"][1]["loop_start_sample"] == tool.ATTACK_SAMPLES + tool.TRANSITION_SAMPLES
    assert manifest["assets"][1]["transition_samples"] == tool.TRANSITION_SAMPLES
    assert manifest["assets"][1]["sustain_gain_db"] == tool.SUSTAIN_GAIN_DB
    assert manifest["assets"][2]["sustain_gain_db"] == tool.B2_SUSTAIN_GAIN_DB
    for asset in manifest["assets"]:
        encoded = OUTPUT / asset["file"]
        assert encoded.is_file(), encoded
        assert encoded.stat().st_size == asset["encoded_bytes"]
    print("C4 loop probe contract passed: baseline + B + B2 candidates")


if __name__ == "__main__":
    main()
