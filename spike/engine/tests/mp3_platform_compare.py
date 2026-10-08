#!/usr/bin/env python3
"""Compares the MP3 decoding of several platforms/compilers (CI job `mp3-compare`).

Every input is the raw interleaved little-endian float32 file written by `spike_cli dump-pcm` for the SAME MP3.
The first file is the reference. Prints a Markdown table (also usable as GitHub step summary).

Exit code 1 if the sample counts differ or a maximum deviation exceeds the tolerance (1e-5, about -100 dB);
bit-identical files are reported as such. A deviation that stays below the tolerance is reported but accepted:
the acceptance criterion of M0-06 is "sample-equal, deviation reported".

Usage: mp3_platform_compare.py <reference.f32> <other.f32>...
"""
import array
import math
import sys
from pathlib import Path

TOLERANCE = 1.0e-5


def load(path):
    data = array.array("f")
    data.frombytes(Path(path).read_bytes())
    if sys.byteorder != "little":
        data.byteswap()
    return data


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    reference = load(argv[1])
    failed = False
    print("| file | samples | max abs deviation | in dB | bit-identical |")
    print("|---|---|---|---|---|")
    print(f"| {Path(argv[1]).parent.name or argv[1]} (reference) | {len(reference)} | 0 | - | yes |")
    for other_path in argv[2:]:
        other = load(other_path)
        name = Path(other_path).parent.name or other_path
        if len(other) != len(reference):
            print(f"| {name} | {len(other)} | length differs from reference ({len(reference)}) | - | no |")
            failed = True
            continue
        worst = max((abs(a - b) for a, b in zip(reference, other)), default=0.0)
        db = "-" if worst == 0.0 else f"{20.0 * math.log10(worst):.1f}"
        identical = "yes" if reference == other else "no"
        print(f"| {name} | {len(other)} | {worst:.3e} | {db} | {identical} |")
        if worst > TOLERANCE:
            failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
