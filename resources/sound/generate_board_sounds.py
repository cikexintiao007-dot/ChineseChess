# SPDX-License-Identifier: CC0-1.0
"""Reproduce the original board/UI cues using Python's standard library."""
import array
import math
import random
import wave
from pathlib import Path

RATE = 44100


def knock(duration, seed):
    random_source = random.Random(seed)
    samples = []
    for index in range(round(duration * RATE)):
        time = index / RATE
        body = sum(
            amplitude * math.sin(2 * math.pi * frequency * time) * math.exp(-time / decay)
            for frequency, amplitude, decay in (
                (210, 0.45, 0.028),
                (570, 0.32, 0.018),
                (1340, 0.20, 0.012),
                (2900, 0.12, 0.008),
            )
        )
        attack = random_source.uniform(-1, 1) * 0.25 * math.exp(-time / 0.004)
        fade = min(1, index / 44, (duration * RATE - index) / 220)
        samples.append((body + attack) * max(0, fade))
    return samples


def write(name, samples, peak):
    gain = peak * 32767 / max(abs(value) for value in samples)
    pcm = array.array("h", (round(value * gain) for value in samples))
    with wave.open(str(Path(__file__).parent / name), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(RATE)
        output.writeframes(pcm.tobytes())


def main():
    write("selectChess.wav", knock(0.075, 11), 0.20)
    write("moveChess.wav", knock(0.14, 23), 0.40)
    undo = knock(0.14, 37) + [0.0] * round(RATE * 0.09)
    second = knock(0.14, 41)
    offset = round(RATE * 0.09)
    for index, sample in enumerate(second):
        undo[offset + index] += sample * 0.75
    write("backChess.wav", undo, 0.32)
    draw = [
        math.sin(2 * math.pi * 523.25 * index / RATE)
        * math.exp(-index / RATE / 0.09)
        * min(1, index / 88)
        for index in range(round(RATE * 0.32))
    ]
    write("drawSound.wav", draw, 0.25)


if __name__ == "__main__":
    main()
