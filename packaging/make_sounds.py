#!/usr/bin/env python3
"""Generates the two notification sounds in src/sounds (original, public domain).

    python packaging/make_sounds.py

Plain sine tones with a soft attack and an exponential decay, 16-bit mono.
"""
import math
import struct
import wave
from pathlib import Path

RATE = 22050


def tone(freq, seconds, volume=0.35):
    n = int(RATE * seconds)
    out = []
    for i in range(n):
        t = i / RATE
        attack = min(1.0, t / 0.008)
        decay = math.exp(-t * 7.0)
        # a quiet octave above gives it a bell-like colour
        s = math.sin(2 * math.pi * freq * t) + 0.25 * math.sin(2 * math.pi * freq * 2 * t)
        out.append(volume * attack * decay * s / 1.25)
    return out


def mix(parts):
    """parts: list of (start_seconds, samples)"""
    total = max(int(RATE * start) + len(s) for start, s in parts)
    buf = [0.0] * total
    for start, samples in parts:
        o = int(RATE * start)
        for i, v in enumerate(samples):
            buf[o + i] += v
    return buf


def save(path, samples):
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1, min(1, v)) * 32767)) for v in samples))


out = Path(__file__).resolve().parent.parent / "src" / "sounds"
out.mkdir(exist_ok=True)
# finished: two rising notes (E5 then A5)
save(out / "finished.wav", mix([(0.0, tone(659.25, 0.45)), (0.16, tone(880.0, 0.6))]))
# failed: two falling, lower notes (E4 then B3)
save(out / "failed.wav", mix([(0.0, tone(329.63, 0.5)), (0.2, tone(246.94, 0.7))]))
