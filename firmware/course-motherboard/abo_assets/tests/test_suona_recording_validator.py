import importlib.util
import sys
import tempfile
import wave
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "tools" / "validate_suona_recording.py"


def load_tool():
    spec = importlib.util.spec_from_file_location("validate_suona_recording", TOOL)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def write_wav(path: Path, *, rate=48000, channels=1, seconds=4):
    with wave.open(str(path), "wb") as output:
        output.setnchannels(channels)
        output.setsampwidth(2)
        output.setframerate(rate)
        output.writeframes(b"\x00\x00" * rate * seconds * channels)


def populate_valid_recording(folder: Path):
    for name in ["C3", "Fs3", "C4", "Fs4", "C5", "Fs5", "C6"]:
        write_wav(folder / f"suona_sus_mf_{name}.wav")
    (folder / "LICENSE-RELEASE.txt").write_text("许可：CC0 1.0\n", encoding="utf-8")


def main():
    tool = load_tool()
    with tempfile.TemporaryDirectory() as temporary:
        folder = Path(temporary)
        populate_valid_recording(folder)
        report = tool.validate_directory(folder)
        assert report["ok"], report["errors"]
        assert report["files"]["suona_sus_mf_C4.wav"]["duration_seconds"] == 4.0

        (folder / "suona_sus_mf_Fs5.wav").unlink()
        missing = tool.validate_directory(folder)
        assert not missing["ok"]
        assert any("suona_sus_mf_Fs5.wav" in item for item in missing["errors"])

        write_wav(folder / "suona_sus_mf_Fs5.wav", rate=44100)
        wrong_format = tool.validate_directory(folder)
        assert not wrong_format["ok"]
        assert any("48000Hz" in item for item in wrong_format["errors"])

    print("suona recording validator tests passed")


if __name__ == "__main__":
    main()
