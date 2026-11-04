#!/usr/bin/env python3
"""Fetch the device framebuffer (/screen.raw, RGB565) and save a PNG. Usage: mirror.py [host] [out.png] [--scale N]"""
import sys, urllib.request, struct, zlib
host = next((a for a in sys.argv[1:] if not a.endswith('.png') and not a.startswith('--') and not a.isdigit()), 'flux-orb.local')
out = next((a for a in sys.argv[1:] if a.endswith('.png')), str(__import__('pathlib').Path(__file__).resolve().parent / 'screen.png'))
scale = int(sys.argv[sys.argv.index('--scale') + 1]) if '--scale' in sys.argv else 2
W = H = 240
raw = urllib.request.urlopen(f'http://{host}/screen.raw', timeout=10).read()
assert len(raw) == W * H * 2, f'got {len(raw)} bytes'
rows = []
for y in range(H):
    row = bytearray([0])
    for x in range(W):
        i = (y * W + x) * 2
        v = (raw[i] << 8) | raw[i + 1]          # LovyanGFX sprite stores swapped (big-endian) RGB565
        r = (v >> 11) & 31; g = (v >> 5) & 63; b = v & 31
        px = bytes((r * 255 // 31, g * 255 // 63, b * 255 // 31))
        row += px * scale
    rows.extend([bytes(row)] * scale)
def chunk(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', W * scale, H * scale, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(b''.join(rows), 6)) + chunk(b'IEND', b'')
open(out, 'wb').write(png); print(out)
