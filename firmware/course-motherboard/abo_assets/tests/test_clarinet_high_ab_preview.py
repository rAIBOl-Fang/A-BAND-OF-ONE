import importlib.util
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools" / "prepare_clarinet_high_ab_preview.py"

assert SCRIPT.is_file(), "independent clarinet high-note A/B encoder is missing"
spec = importlib.util.spec_from_file_location("clarinet_high_ab_preview", SCRIPT)
module = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(module)

assert module.OUTPUT.name == "clarinet-tonejs-high-ab-preview"
assert len(module.SOURCES) == 9
replacements = {asset_id: source for asset_id, source, _ in module.SOURCES if source.startswith("clarinet-tonejs-high-ab/")}
assert replacements == {
    "clarinet_d5": "clarinet-tonejs-high-ab/clarinet_d5_b1.wav",
    "clarinet_f5": "clarinet-tonejs-high-ab/clarinet_f5_b1.wav",
    "clarinet_as5": "clarinet-tonejs-high-ab/clarinet_as5_b1.wav",
}
assert module.LOOP_END_SAMPLES["clarinet_d5"] == 28656
assert module.LOOP_END_SAMPLES["clarinet_f5"] == 28608
assert module.LOOP_END_SAMPLES["clarinet_as5"] == 27264

print("clarinet high-note A/B preview encoder tests passed")
