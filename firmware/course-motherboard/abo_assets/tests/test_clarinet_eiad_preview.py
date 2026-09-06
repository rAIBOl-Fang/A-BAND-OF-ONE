import importlib.util
import math
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools" / "prepare_clarinet_eiad_preview.py"

assert SCRIPT.is_file(), "clarinet EIAD preview encoder is missing"
spec = importlib.util.spec_from_file_location("clarinet_eiad_preview", SCRIPT)
module = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(module)

assert module.CROSSFADE_SAMPLES == 960, "20ms at 48kHz must be 960 samples"
assert module.LOOP_START_SAMPLE == 19200
assert len(module.SOURCES) == 9

expected_loop_ends = {
    "clarinet_d3": 27984, "clarinet_f3": 36912, "clarinet_as3": 27984,
    "clarinet_d4": 32400, "clarinet_f4": 29472, "clarinet_as4": 31056,
    "clarinet_d5": 29376, "clarinet_f5": 30432, "clarinet_as5": 27552,
}
assert module.LOOP_END_SAMPLES == expected_loop_ends, "each frozen root needs its phase-matched loop end"

for asset_id, filename, _ in module.SOURCES:
    _, raw = module.read_wav(module.RAW / filename)
    loop_end = module.LOOP_END_SAMPLES[asset_id]
    head = raw[module.LOOP_START_SAMPLE:module.LOOP_START_SAMPLE + module.CROSSFADE_SAMPLES]
    tail = raw[loop_end - module.CROSSFADE_SAMPLES:loop_end]
    correlation = sum(a * b for a, b in zip(head, tail)) / math.sqrt(sum(a * a for a in head) * sum(b * b for b in tail))
    assert correlation >= 0.90, f"{asset_id} loop is not phase matched: {correlation:.3f}"

source = list(range(40000))
loop_end = module.LOOP_END_SAMPLES["clarinet_d3"]
result = module.bake_loop_crossfade(source, module.LOOP_START_SAMPLE, loop_end, module.CROSSFADE_SAMPLES)
assert result[loop_end - module.CROSSFADE_SAMPLES] == source[loop_end - module.CROSSFADE_SAMPLES]
assert abs(result[loop_end - 1] - source[module.LOOP_START_SAMPLE + module.CROSSFADE_SAMPLES - 1]) <= 1

print("clarinet EIAD preview encoder unit tests passed")
