"""Contract tests for the non-formal clarinet PCM listening bundle."""

import hashlib
import json
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RAW = ROOT / "raw" / "clarinet-tonejs"
OUTPUT = ROOT / "delivery" / "clarinet-tonejs-pcm-preview"
EXPECTED = {
    "clarinet_d3": ("D3", 27984),
    "clarinet_f3": ("F3", 36912),
    "clarinet_as3": ("A#3", 27984),
    "clarinet_d4": ("D4", 32400),
    "clarinet_f4": ("F4", 29472),
    "clarinet_as4": ("A#4", 31056),
    "clarinet_d5": ("D5", 29376),
    "clarinet_f5": ("F5", 30432),
    "clarinet_as5": ("A#5", 27552),
}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_pcm_wav(path):
    data = path.read_bytes()
    assert data[:4] == b"RIFF" and data[8:12] == b"WAVE", path
    cursor, fmt, pcm = 12, None, None
    while cursor + 8 <= len(data):
        tag = data[cursor:cursor + 4]
        length = struct.unpack_from("<I", data, cursor + 4)[0]
        chunk = data[cursor + 8:cursor + 8 + length]
        cursor += 8 + length + (length & 1)
        if tag == b"fmt ":
            fmt = chunk
        elif tag == b"data":
            pcm = chunk
    assert fmt is not None and pcm is not None, path
    code, channels, rate, _, _, bits = struct.unpack_from("<HHIIHH", fmt)
    assert (code, channels, rate, bits) == (1, 1, 48000, 16), path
    assert len(pcm) == 76800, path
    return list(struct.unpack("<38400h", pcm))


def main():
    manifest_path = OUTPUT / "manifest.json"
    assert manifest_path.is_file(), "PCM preview manifest is missing"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    assert manifest["bundle"] == "clarinet-tonejs-pcm-preview"
    assert manifest["sample_rate_hz"] == 48000
    assert manifest["channels"] == 1
    assert manifest["pcm_bits"] == 16
    assert manifest["total_pcm_bytes"] == 691200
    assert (OUTPUT / "ATTRIBUTION.md").is_file()

    assets = manifest["assets"]
    assert [asset["id"] for asset in assets] == list(EXPECTED)
    total_bytes = 0
    for asset in assets:
        root_note, loop_end = EXPECTED[asset["id"]]
        assert asset["root_note"] == root_note
        assert asset["format"] == "PCM16LE-WAV-v1"
        assert asset["sample_rate_hz"] == 48000
        assert asset["channels"] == 1
        assert asset["pcm_bits"] == 16
        assert asset["normalization_dbfs"] == -3
        assert asset["total_samples"] == 38400
        assert asset["raw_loop_start_sample"] == 19200
        assert asset["raw_loop_end_sample"] == loop_end
        assert asset["loop_start_sample"] == 20160
        assert asset["loop_end_sample"] == loop_end
        assert asset["crossfade_samples"] == 960
        source = RAW / Path(asset["source"]).name
        output = OUTPUT / asset["file"]
        assert source.is_file()
        assert output.is_file()
        assert asset["source_sha256"] == sha256(source)
        samples = read_pcm_wav(output)
        assert asset["data_bytes"] == 76800
        assert asset["encoded_bytes"] == output.stat().st_size
        assert asset["pcm_sha256"] == sha256(output)
        assert max(abs(sample) for sample in samples) == 23197
        total_bytes += asset["data_bytes"]
    assert total_bytes == 691200
    print("clarinet PCM preview asset tests passed")


if __name__ == "__main__":
    main()
