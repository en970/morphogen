#!/usr/bin/env python3
"""Convert the greyscale PGM images the figure generator writes into PNG.

Uses nothing outside the standard library, deliberately: the paper has to build
in CI from a checkout and a LaTeX engine, and adding an image library to that
list to do a job that zlib and struct can do in thirty lines would be a poor
trade.

The images are also tinted on the way through — paper and ink rather than white
and black — so that the figures in the paper are the same two colours as the
laboratory itself.
"""
import struct
import sys
import zlib
from pathlib import Path

PAPER = (0xF2, 0xF1, 0xE8)
INK = (0x2B, 0x2B, 0x2B)


def read_pgm(path):
    data = path.read_bytes()
    # P5 <w> <h> <maxval> then binary, with whitespace/comments between tokens
    tokens, i = [], 0
    while len(tokens) < 4:
        while i < len(data) and data[i : i + 1].isspace():
            i += 1
        if data[i : i + 1] == b"#":
            while i < len(data) and data[i] != 0x0A:
                i += 1
            continue
        j = i
        while j < len(data) and not data[j : j + 1].isspace():
            j += 1
        tokens.append(data[i:j])
        i = j
    i += 1  # the single whitespace byte after maxval
    magic, w, h = tokens[0], int(tokens[1]), int(tokens[2])
    if magic != b"P5":
        raise ValueError(f"{path}: not a binary PGM")
    return w, h, data[i : i + w * h]


def chunk(tag, payload):
    return (
        struct.pack(">I", len(payload))
        + tag
        + payload
        + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)
    )


def write_png(path, w, h, grey):
    # grey is 0..255 where 255 is "no ink"; map it onto paper..ink
    rows = bytearray()
    for y in range(h):
        rows.append(0)  # filter: none
        row = grey[y * w : (y + 1) * w]
        for v in row:
            t = v / 255.0
            for c in range(3):
                rows.append(int(round(INK[c] + (PAPER[c] - INK[c]) * t)))

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(rows), 9))
    png += chunk(b"IEND", b"")
    path.write_bytes(png)


def main():
    root = Path(sys.argv[1] if len(sys.argv) > 1 else "paper/fig")
    n = 0
    for pgm in sorted(root.glob("*.pgm")):
        w, h, grey = read_pgm(pgm)
        write_png(pgm.with_suffix(".png"), w, h, grey)
        pgm.unlink()
        n += 1
    print(f"  {n} figures -> png")


if __name__ == "__main__":
    main()
