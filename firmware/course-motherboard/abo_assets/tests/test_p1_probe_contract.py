from pathlib import Path

root = Path(__file__).resolve().parents[2]
cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
app = (root / "main" / "app_main.cpp").read_text(encoding="utf-8")

assert "ABO_P1_PIANO_PROBE" in cmake
assert "EASY_INPUT_ABO_P1_PIANO_PROBE" in app
assert "abo_p1_piano_c4_sound" in app
assert "request_embedded_asset" in app
print("P1 piano probe contract passed")
