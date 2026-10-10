#!/usr/bin/env python3
"""Extract photos from a PixelPoach ``.sav`` file as PNG (or BMP) images.

PixelPoach stores photos in a Game Boy Camera-format save: each photo is a
128x112 2 bpp image (224 tiles of 16 bytes) plus a thumbnail and metadata,
laid out in 4 KB slots. This script reads that save, finds every occupied slot
via the vector state, decodes the 2 bpp tiles, and writes one image per photo.

Usage::

    python tools/extract_sav.py PixelPoach.sav
    python tools/extract_sav.py PixelPoach.sav -o photos --green --scale 6

PNG output requires Pillow (``pip install pillow``). If Pillow is not installed
the script falls back to writing 24-bit BMP files, which any image viewer opens.
"""

import argparse
import os
import struct
import sys

IMG_W = 128
IMG_H = 112
IMG_BYTES = 3584             # 224 tiles * 16 bytes, 2 bpp
SLOT_SIZE = 0x1000
SLOT_BASE = 0x2000
SLOT_COUNT = 30
VECTOR_OFF = 0x11B2          # 30 bytes: slot -> image number (0..29), 0xFF = empty

GRAY = [(255, 255, 255), (170, 170, 170), (85, 85, 85), (0, 0, 0)]
GREEN = [(224, 248, 208), (136, 192, 112), (52, 104, 86), (8, 24, 32)]

try:
    from PIL import Image
    HAVE_PIL = True
except ImportError:
    HAVE_PIL = False


def decode_image(buf):
    """Decode a 128x112 2 bpp camera image into a list of shade values 0..3."""
    shades = []
    for y in range(IMG_H):
        ty, yy = y >> 3, y & 7
        for x in range(IMG_W):
            tx, xx = x >> 3, x & 7
            o = (ty * 16 + tx) * 16 + yy * 2
            if o + 1 >= len(buf):
                shades.append(0)
                continue
            b0 = (buf[o] >> (7 - xx)) & 1
            b1 = (buf[o + 1] >> (7 - xx)) & 1
            shades.append(b0 | (b1 << 1))
    return shades


def write_png(path, shades, palette, scale):
    img = Image.new("RGB", (IMG_W, IMG_H))
    img.putdata([palette[s] for s in shades])
    if scale != 1:
        img = img.resize((IMG_W * scale, IMG_H * scale), Image.NEAREST)
    img.save(path)


def write_bmp(path, shades, palette, scale):
    w, h = IMG_W * scale, IMG_H * scale
    row_size = (w * 3 + 3) & ~3
    data_size = row_size * h
    with open(path, "wb") as f:
        f.write(b"BM")
        f.write(struct.pack("<I", 54 + data_size))
        f.write(struct.pack("<HH", 0, 0))
        f.write(struct.pack("<I", 54))
        f.write(struct.pack("<I", 40))
        f.write(struct.pack("<ii", w, h))
        f.write(struct.pack("<HH", 1, 24))
        f.write(struct.pack("<I", 0))
        f.write(struct.pack("<I", data_size))
        f.write(struct.pack("<ii", 2835, 2835))
        f.write(struct.pack("<II", 0, 0))
        # pixel data, bottom-up
        for y in range(h - 1, -1, -1):
            row = bytearray()
            src_y = y // scale
            for x in range(w):
                src_x = x // scale
                r, g, b = palette[shades[src_y * IMG_W + src_x]]
                row += bytes((b, g, r))
            row += b"\x00" * (row_size - w * 3)
            f.write(bytes(row))


def main():
    ap = argparse.ArgumentParser(
        description="Extract photos from a PixelPoach .sav file as images.")
    ap.add_argument("sav", help="path to the PixelPoach.sav file")
    ap.add_argument("-o", "--outdir", default="photos",
                    help="output directory (default: photos)")
    ap.add_argument("-s", "--scale", type=int, default=4,
                    help="integer upscale factor (default: 4)")
    ap.add_argument("--green", action="store_true",
                    help="use the classic Game Boy green palette instead of grayscale")
    args = ap.parse_args()

    with open(args.sav, "rb") as f:
        data = f.read()

    n_slots = (len(data) - SLOT_BASE) // SLOT_SIZE
    if n_slots <= 0:
        sys.exit("error: file is too small to be a camera save")
    n_slots = min(n_slots, SLOT_COUNT)

    vector = data[VECTOR_OFF:VECTOR_OFF + SLOT_COUNT]

    # collect occupied slots, keyed by their image number (what the camera shows)
    entries = []
    for slot in range(n_slots):
        num = vector[slot]
        if num != 0xFF:
            entries.append((num + 1, slot))
    entries.sort()

    if not entries:
        print("no photos found in", args.sav)
        return

    os.makedirs(args.outdir, exist_ok=True)
    palette = GREEN if args.green else GRAY

    for photo_no, slot in entries:
        base = SLOT_BASE + slot * SLOT_SIZE
        buf = data[base:base + IMG_BYTES]
        if len(buf) < IMG_BYTES:
            continue
        shades = decode_image(buf)
        ext = ".png" if HAVE_PIL else ".bmp"
        out = os.path.join(args.outdir, "photo_%02d%s" % (photo_no, ext))
        if HAVE_PIL:
            write_png(out, shades, palette, args.scale)
        else:
            write_bmp(out, shades, palette, args.scale)
        print("wrote", out)


if __name__ == "__main__":
    main()
