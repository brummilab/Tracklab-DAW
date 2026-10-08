#!/usr/bin/env python3
"""Creates the MP3 fixture for the platform comparison (CI job `mp3-fixture`).

Synthetic and deterministic: three sines plus noise from a fixed-seed LCG, 3 s, 44.1 kHz stereo, 16 bit,
encoded once with `lame` (128 kbit/s CBR). The MP3 is shared with the other CI jobs as a workflow artifact
(never committed, R13), so the encoder version does not have to match across platforms.

Usage: make_mp3_fixture.py <output-dir>
"""
import math
import struct
import subprocess
import sys
import wave
from pathlib import Path

SAMPLE_RATE = 44100
SECONDS = 3


def lcg_noise(seed):
    """Deterministic noise in [-1, 1): Numerical Recipes LCG."""
    state = seed
    while True:
        state = (1664525 * state + 1013904223) & 0xFFFFFFFF
        yield state / 2147483648.0 - 1.0


def main():
    out_dir = Path(sys.argv[1])
    out_dir.mkdir(parents=True, exist_ok=True)
    wav_path = out_dir / "fixture.wav"
    mp3_path = out_dir / "fixture.mp3"

    noise_left = lcg_noise(1)
    noise_right = lcg_noise(2)
    frames = bytearray()
    for n in range(SAMPLE_RATE * SECONDS):
        t = n / SAMPLE_RATE
        tone = (0.25 * math.sin(2 * math.pi * 220.0 * t)
                + 0.15 * math.sin(2 * math.pi * 997.0 * t)
                + 0.05 * math.sin(2 * math.pi * 5000.0 * t))
        left = tone + 0.05 * next(noise_left)
        right = 0.8 * tone + 0.05 * next(noise_right)
        frames += struct.pack("<hh", int(round(left * 32767)), int(round(right * 32767)))

    with wave.open(str(wav_path), "wb") as wav:
        wav.setnchannels(2)
        wav.setsampwidth(2)
        wav.setframerate(SAMPLE_RATE)
        wav.writeframes(bytes(frames))

    subprocess.run(["lame", "--silent", "-b", "128", "-q", "2", str(wav_path), str(mp3_path)], check=True)
    wav_path.unlink()
    print(f"wrote {mp3_path} ({mp3_path.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
