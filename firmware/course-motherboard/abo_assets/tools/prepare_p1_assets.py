"""Build the 23 P1 EIAD delivery assets from preserved raw WAV files.

Uses only Python's standard library.  It never writes under raw/.
"""
import hashlib
import json
import math
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RAW = ROOT / "raw"
DELIVERY = ROOT / "delivery"
RATE = 48_000
FRAME = 480
TARGET_PEAK = round(32767 * (10 ** (-3 / 20)))
PIANO_C4_SFZ = RAW / "piano-upright-kw" / "UprightPianoKW-small-20190703.sfz"
PIANO_C4_ATTACK_SAMPLES = round(1.20 * RATE)
PIANO_C4_TRANSITION_SAMPLES = round(0.12 * RATE)
PIANO_C4_SUSTAIN_GAIN_DB = 6.0
CLARINET_SECONDS = 0.8
CLARINET_LOOP_START_SAMPLE = round(0.40 * RATE)
CLARINET_CROSSFADE_SAMPLES = round(0.020 * RATE)
CLARINET_LOOP_END_SAMPLES = {
    "clarinet_d3": 27984,
    "clarinet_f3": 36912,
    "clarinet_as3": 27984,
    "clarinet_d4": 32400,
    "clarinet_f4": 29472,
    "clarinet_as4": 31056,
    "clarinet_d5": 29376,
    "clarinet_f5": 30432,
    "clarinet_as5": 27552,
}

SOURCES = [
    ("piano_b2", "piano-upright-kw/samples/B2vH.wav", 2.0, 0.75, 1.80),
    ("piano_fis3", "piano-upright-kw/samples/F#3vH.wav", 2.0, 0.75, 1.80),
    ("piano_c4", "piano-upright-kw/samples/C4vH.wav", 2.0, 0.75, 1.80),
    ("piano_fis4", "piano-upright-kw/samples/F#4vH.wav", 2.0, 0.75, 1.80),
    ("piano_c5", "piano-upright-kw/samples/C5vH.wav", 2.0, 0.75, 1.80),
    ("piano_fis5", "piano-upright-kw/samples/F#5vH.wav", 2.0, 0.75, 1.80),
    ("piano_c6", "piano-upright-kw/samples/C6vH.wav", 2.0, 0.75, 1.80),
    ("violin_g3", "violin-vsco2/LLVln_ArcoVib_G3_f.wav", 1.5, 0.70, 1.30),
    ("violin_c4", "violin-vsco2/LLVln_ArcoVib_C4_f.wav", 1.5, 0.70, 1.30),
    ("violin_e4", "violin-vsco2/LLVln_ArcoVib_E4_f.wav", 1.5, 0.70, 1.30),
    ("violin_g4", "violin-vsco2/LLVln_ArcoVib_G4_f.wav", 1.5, 0.70, 1.30),
    ("violin_c5", "violin-vsco2/LLVln_ArcoVib_C5_f.wav", 1.5, 0.70, 1.30),
    ("violin_e5", "violin-vsco2/LLVln_ArcoVib_E5_f.wav", 1.5, 0.70, 1.30),
    ("violin_a5", "violin-vsco2/LLVln_ArcoVib_A5_f.wav", 1.5, 0.70, 1.30),
    ("clarinet_d3", "clarinet-tonejs/clarinet_d3.wav", CLARINET_SECONDS, 0.42, CLARINET_LOOP_END_SAMPLES["clarinet_d3"] / RATE),
    ("clarinet_f3", "clarinet-tonejs/clarinet_f3.wav", CLARINET_SECONDS, 0.42, CLARINET_LOOP_END_SAMPLES["clarinet_f3"] / RATE),
    ("clarinet_as3", "clarinet-tonejs/clarinet_as3.wav", CLARINET_SECONDS, 0.42, CLARINET_LOOP_END_SAMPLES["clarinet_as3"] / RATE),
    ("clarinet_d4", "clarinet-tonejs/clarinet_d4.wav", CLARINET_SECONDS, 0.42, CLARINET_LOOP_END_SAMPLES["clarinet_d4"] / RATE),
    ("clarinet_f4", "clarinet-tonejs/clarinet_f4.wav", CLARINET_SECONDS, 0.42, CLARINET_LOOP_END_SAMPLES["clarinet_f4"] / RATE),
    ("clarinet_as4", "clarinet-tonejs/clarinet_as4.wav", CLARINET_SECONDS, 0.42, CLARINET_LOOP_END_SAMPLES["clarinet_as4"] / RATE),
    ("clarinet_d5", "clarinet-tonejs/clarinet_d5.wav", CLARINET_SECONDS, 0.42, CLARINET_LOOP_END_SAMPLES["clarinet_d5"] / RATE),
    ("clarinet_f5", "clarinet-tonejs/clarinet_f5.wav", CLARINET_SECONDS, 0.42, CLARINET_LOOP_END_SAMPLES["clarinet_f5"] / RATE),
    ("clarinet_as5", "clarinet-tonejs/clarinet_as5.wav", CLARINET_SECONDS, 0.42, CLARINET_LOOP_END_SAMPLES["clarinet_as5"] / RATE),
]

STEPS = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37,
         41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173,
         190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658,
         724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
         2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484,
         7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818,
         18500, 20350, 22385, 24623, 27086, 29794, 32767]
INDEX = [-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_wav(path):
    data = path.read_bytes()
    assert data[:4] == b"RIFF" and data[8:12] == b"WAVE", path
    cursor, fmt, pcm = 12, None, None
    while cursor + 8 <= len(data):
        tag, length = data[cursor:cursor + 4], struct.unpack_from("<I", data, cursor + 4)[0]
        chunk = data[cursor + 8:cursor + 8 + length]
        cursor += 8 + length + (length & 1)
        if tag == b"fmt ": fmt = chunk
        if tag == b"data": pcm = chunk
    code, channels, rate, _, _, bits = struct.unpack_from("<HHIIHH", fmt)
    assert code == 1 and channels in (1, 2) and bits in (16, 24), path
    width = bits // 8
    values = []
    for offset in range(0, len(pcm), width * channels):
        frame = []
        for channel in range(channels):
            raw = pcm[offset + channel * width: offset + (channel + 1) * width]
            value = int.from_bytes(raw, "little", signed=True)
            frame.append(value >> (bits - 16))
        values.append(round(sum(frame) / channels))
    return rate, values


def resample(samples, source_rate, count):
    if source_rate == RATE:
        return samples[:count]
    result = []
    for target in range(count):
        source = target * source_rate / RATE
        left = min(int(source), len(samples) - 1)
        right = min(left + 1, len(samples) - 1)
        fraction = source - left
        result.append(round(samples[left] + (samples[right] - samples[left]) * fraction))
    return result


def normalize(samples):
    peak = max(1, max(abs(value) for value in samples))
    return [max(-32768, min(32767, round(value * TARGET_PEAK / peak))) for value in samples]


def read_sfz_c4_loop(path):
    text = Path(path).read_text(encoding="utf-8")
    match = re.search(
        r"loop_start=(\d+)\s+loop_end=(\d+)\s+sample=samples/C4vH\.wav",
        text,
    )
    assert match, f"C4 SFZ loop is missing: {path}"
    return int(match.group(1)), int(match.group(2))


def clip_to_target(value):
    return max(-TARGET_PEAK, min(TARGET_PEAK, round(value)))


def apply_gain(samples, decibels):
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


def prepare_piano_c4(source_rate, samples):
    native_loop = read_sfz_c4_loop(PIANO_C4_SFZ)
    target_count = round(len(samples) * RATE / source_rate)
    full = normalize(resample(samples, source_rate, target_count))
    native_start, native_end = native_loop
    loop_start = round(native_start * RATE / source_rate)
    loop_end = round(native_end * RATE / source_rate)
    assert loop_end <= len(full)
    attack = full[:PIANO_C4_ATTACK_SAMPLES]
    sustain = apply_gain(full[loop_start:loop_end], PIANO_C4_SUSTAIN_GAIN_DB)
    transition = crossfade(
        attack[-PIANO_C4_TRANSITION_SAMPLES:],
        sustain[:PIANO_C4_TRANSITION_SAMPLES],
    )
    combined = (
        attack[:-PIANO_C4_TRANSITION_SAMPLES]
        + transition
        + sustain[PIANO_C4_TRANSITION_SAMPLES:]
    )
    raw_loop_start = PIANO_C4_ATTACK_SAMPLES
    raw_loop_end = raw_loop_start + len(sustain) - PIANO_C4_TRANSITION_SAMPLES
    pcm = bake_loop_crossfade(
        combined,
        raw_loop_start,
        raw_loop_end,
        PIANO_C4_TRANSITION_SAMPLES,
    )
    return pcm, {
        "source_sfz": str(PIANO_C4_SFZ.relative_to(RAW)).replace("\\", "/"),
        "source_sfz_sha256": digest(PIANO_C4_SFZ),
        "source_sfz_loop_start_sample": native_start,
        "source_sfz_loop_end_sample": native_end,
        "source_loop_start_sample": loop_start,
        "source_loop_end_sample": loop_end,
        "attack_samples": PIANO_C4_ATTACK_SAMPLES,
        "transition_samples": PIANO_C4_TRANSITION_SAMPLES,
        "sustain_gain_db": PIANO_C4_SUSTAIN_GAIN_DB,
        "loop_start_sample": raw_loop_start + PIANO_C4_TRANSITION_SAMPLES,
        "loop_end_sample": raw_loop_end,
    }


def encode_nibble(sample, predictor, index):
    step = STEPS[index]
    difference, code = sample - predictor, 0
    if difference < 0: code, difference = 8, -difference
    delta = step >> 3
    if difference >= step:
        code |= 4; difference -= step; delta += step
    if difference >= step >> 1:
        code |= 2; difference -= step >> 1; delta += step >> 1
    if difference >= step >> 2:
        code |= 1; delta += step >> 2
    predictor += -delta if code & 8 else delta
    predictor = max(-32768, min(32767, predictor))
    index = max(0, min(88, index + INDEX[code]))
    return code, predictor, index


def encode_eiad(samples):
    frames = []
    for start in range(0, len(samples), FRAME):
        block = samples[start:start + FRAME]
        predictor, index, packed = block[0], 0, bytearray()
        for position in range(1, len(block), 2):
            low, predictor, index = encode_nibble(block[position], predictor, index)
            high = 0
            if position + 1 < len(block):
                high, predictor, index = encode_nibble(block[position + 1], predictor, index)
            packed.append(low | (high << 4))
        frames.append(struct.pack("<HhBB", len(block), block[0], 0, 0) + packed)
    header = b"EIAD" + bytes([1, 1]) + struct.pack("<IHHIH", RATE, FRAME, len(frames), len(samples), 20)
    return header + b"".join(frames)


def main():
    DELIVERY.mkdir(exist_ok=True)
    records = []
    total_payload_bytes = 0
    for asset_id, relative, seconds, loop_from, loop_to in SOURCES:
        source = RAW / relative
        source_rate, raw = read_wav(source)
        extra = {}
        if asset_id == "piano_c4":
            pcm, extra = prepare_piano_c4(source_rate, raw)
        elif asset_id in CLARINET_LOOP_END_SAMPLES:
            total = round(CLARINET_SECONDS * RATE)
            pcm = normalize(bake_loop_crossfade(
                resample(raw, source_rate, total),
                CLARINET_LOOP_START_SAMPLE,
                CLARINET_LOOP_END_SAMPLES[asset_id],
                CLARINET_CROSSFADE_SAMPLES,
            ))
        else:
            total = round(seconds * RATE)
            pcm = normalize(resample(raw, source_rate, total))
        if asset_id in CLARINET_LOOP_END_SAMPLES:
            encoded = struct.pack(f"<{len(pcm)}h", *pcm)
            target = DELIVERY / f"{asset_id}.pcm16le"
            target.write_bytes(encoded)
            record = {
                "id": asset_id,
                "file": target.name,
                "format": "PCM16LE-v1",
                "sample_rate_hz": RATE,
                "channels": 1,
                "pcm_bits": 16,
                "normalization_dbfs": -3,
                "source": relative,
                "source_sha256": digest(source),
                "total_samples": len(pcm),
                "raw_loop_start_sample": CLARINET_LOOP_START_SAMPLE,
                "raw_loop_end_sample": CLARINET_LOOP_END_SAMPLES[asset_id],
                "loop_start_sample": CLARINET_LOOP_START_SAMPLE + CLARINET_CROSSFADE_SAMPLES,
                "loop_end_sample": CLARINET_LOOP_END_SAMPLES[asset_id],
                "crossfade_samples": CLARINET_CROSSFADE_SAMPLES,
                "encoded_bytes": len(encoded),
                "pcm_sha256": digest(target),
            }
        else:
            encoded = encode_eiad(pcm)
            target = DELIVERY / f"{asset_id}.eiad"
            target.write_bytes(encoded)
            record = {
                "id": asset_id,
                "file": target.name,
                "format": "EIAD-v1",
                "sample_rate_hz": RATE,
                "channels": 1,
                "pcm_bits": 16,
                "normalization_dbfs": -3,
                "source": relative,
                "source_sha256": digest(source),
                "total_samples": len(pcm),
                "loop_start_sample": round(loop_from * RATE),
                "loop_end_sample": round(loop_to * RATE),
                "encoded_bytes": len(encoded),
                "eiad_sha256": digest(target),
                **extra,
            }
        records.append(record)
        total_payload_bytes += len(encoded)
    (DELIVERY / "manifest.json").write_text(json.dumps({
        "storage_contract": "14 EIAD-v1 + 9 PCM16LE-v1",
        "total_payload_bytes": total_payload_bytes,
        "assets": records,
    }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
