#!/usr/bin/env python3
"""Verify the A Band of One USB-Serial/JTAG score transaction on real hardware.

This script intentionally uses only pyserial from the existing ESP-IDF venv.
It does not flash, erase, or reset the board. The board must already be running
the independently built A Band of One image.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from typing import Any, Callable

import serial


def crc32_update(crc: int, byte: int) -> int:
    crc ^= byte
    for _ in range(8):
        crc = (crc >> 1) ^ (0xEDB88320 if crc & 1 else 0)
    return crc & 0xFFFFFFFF


def crc32_bytes(crc: int, data: bytes) -> int:
    for byte in data:
        crc = crc32_update(crc, byte)
    return crc


def crc32_u16(crc: int, value: int) -> int:
    return crc32_bytes(crc, value.to_bytes(2, "little"))


def canonical_score_crc(
    score_id: str,
    title: str,
    bpm: int,
    notes: list[dict[str, int]],
    score_version: int = 1,
) -> int:
    """Match ScoreProtocol::canonical_crc32 byte-for-byte."""

    encoded_id = score_id.encode("ascii")
    encoded_title = title.encode("utf-8")
    crc = 0xFFFFFFFF
    crc = crc32_bytes(crc, b"abo.score")
    crc = crc32_update(crc, 0)
    crc = crc32_update(crc, score_version)
    crc = crc32_update(crc, len(encoded_id))
    crc = crc32_bytes(crc, encoded_id)
    crc = crc32_update(crc, len(encoded_title))
    crc = crc32_bytes(crc, encoded_title)
    crc = crc32_u16(crc, bpm)
    crc = crc32_u16(crc, len(notes))
    for note in notes:
        crc = crc32_update(crc, note["n"])
        crc = crc32_u16(crc, note["b"] * 96)
    return crc ^ 0xFFFFFFFF


def make_message(command: str, tx: str, **fields: Any) -> bytes:
    message = {"t": command, "v": 1, "tx": tx}
    message.update(fields)
    return (json.dumps(message, ensure_ascii=False, separators=(",", ":")) + "\n").encode(
        "utf-8"
    )


def read_json_until(
    port: serial.Serial,
    predicate: Callable[[dict[str, Any]], bool],
    timeout: float,
) -> dict[str, Any]:
    deadline = time.monotonic() + timeout
    observed: list[str] = []
    while time.monotonic() < deadline:
        raw = port.readline()
        if not raw:
            continue
        try:
            message = json.loads(raw.decode("utf-8", errors="replace"))
        except json.JSONDecodeError:
            # Boot logs and ESP-IDF diagnostics are not protocol messages.
            continue
        if not isinstance(message, dict):
            continue
        if predicate(message):
            return message
        observed.append(json.dumps(message, ensure_ascii=False))
    details = "; ".join(observed[-5:])
    raise RuntimeError(f"timeout waiting for protocol response; observed={details}")


def expect_ack(port: serial.Serial, command: str, tx: str, timeout: float) -> dict[str, Any]:
    def matches(message: dict[str, Any]) -> bool:
        if message.get("t") == "error":
            raise RuntimeError(f"board rejected {command}: {message}")
        return (
            message.get("t") == "ack"
            and message.get("cmd") == command
            and message.get("tx") == tx
        )

    return read_json_until(port, matches, timeout)


def expect_error(
    port: serial.Serial, command: str, tx: str, reason: str, timeout: float
) -> dict[str, Any]:
    def matches(message: dict[str, Any]) -> bool:
        return (
            message.get("t") == "error"
            and message.get("cmd") == command
            and message.get("tx") == tx
            and message.get("reason") == reason
        )

    return read_json_until(port, matches, timeout)


def expect_state(port: serial.Serial, score_id: str, timeout: float) -> dict[str, Any]:
    state = read_json_until(port, lambda message: message.get("t") == "state", timeout)
    if state.get("score_loaded") is not True or state.get("score_id") != score_id:
        raise RuntimeError(f"unexpected committed score state: {state}")
    return state


def upload_score(
    port: serial.Serial,
    tx: str,
    score_id: str,
    title: str,
    notes: list[dict[str, int]],
    timeout: float,
    crc_override: int | None = None,
) -> dict[str, Any] | None:
    port.write(
        make_message(
            "score_begin",
            tx,
            schema="abo.score",
            score_version=1,
            id=score_id,
            title=title,
            bpm=90,
            count=len(notes),
        )
    )
    expect_ack(port, "score_begin", tx, timeout)

    for sequence, start in enumerate(range(0, len(notes), 16)):
        chunk = notes[start : start + 16]
        port.write(make_message("score_chunk", tx, seq=sequence, notes=chunk))
        expect_ack(port, "score_chunk", tx, timeout)

    crc = canonical_score_crc(score_id, title, 90, notes)
    if crc_override is not None:
        crc = crc_override
    port.write(
        make_message(
            "score_commit",
            tx,
            chunks=(len(notes) + 15) // 16,
            crc32=f"{crc:08x}",
        )
    )
    if crc_override is not None:
        expect_error(port, "score_commit", tx, "crc_mismatch", timeout)
        return None
    ack = expect_ack(port, "score_commit", tx, timeout)
    if ack.get("score_id") != score_id or ack.get("score_version") != 1:
        raise RuntimeError(f"commit ACK omitted score identity: {ack}")
    return ack


def verify(port_name: str, baud: int, timeout: float) -> None:
    notes = [{"n": note, "b": 1} for note in range(7)]
    with serial.Serial(port_name, baudrate=baud, timeout=0.1) as port:
        port.reset_input_buffer()
        port.write(make_message("ping", "00000000"))
        read_json_until(port, lambda message: message.get("t") == "hello", timeout)
        read_json_until(port, lambda message: message.get("t") == "state", timeout)
        print("PASS ping -> hello/state")

        upload_score(port, "a1b2c3d4", "hil-scale", "HIL Scale", notes, timeout)
        state = expect_state(port, "hil-scale", timeout)
        print(f"PASS good score_commit -> state score_id={state['score_id']}")

        upload_score(
            port,
            "b2c3d4e5",
            "bad-score",
            "Bad Score",
            notes,
            timeout,
            crc_override=0,
        )
        port.write(make_message("ping", "00000000"))
        read_json_until(port, lambda message: message.get("t") == "hello", timeout)
        state = read_json_until(port, lambda message: message.get("t") == "state", timeout)
        if state.get("score_id") != "hil-scale" or state.get("score_loaded") is not True:
            raise RuntimeError(f"bad CRC replaced the committed score: {state}")
        print("PASS bad CRC -> crc_mismatch and previous score preserved")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="USB-Serial/JTAG port, e.g. COM5")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=3.0)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        verify(args.port, args.baud, args.timeout)
    except (OSError, serial.SerialException, RuntimeError) as error:
        print(f"FAIL {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
