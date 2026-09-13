"""Generate native 12px BDF glyphs without resampling. Stdlib only.

Original BDF source and license must accompany distributed output.
Missing or oversized glyphs remain blank; the reader displays a placeholder.
"""
import argparse
from pathlib import Path

def main():
    p = argparse.ArgumentParser()
    p.add_argument("bdf", type=Path)
    p.add_argument("output", type=Path)
    args = p.parse_args()
    data = bytearray(8 + 65536 * 24)
    data[:8] = b"NRF12V1\0"
    code, bbox, rows, in_bitmap = -1, None, [], False
    replaced = clipped = 0
    for line in args.bdf.read_text().splitlines():
        parts = line.split()
        if not parts:
            continue
        if parts[0] == "STARTCHAR":
            code, bbox, rows, in_bitmap = -1, None, [], False
        elif parts[0] == "ENCODING":
            code = int(parts[1])
        elif parts[0] == "BBX":
            bbox = tuple(map(int, parts[1:]))
        elif parts[0] == "BITMAP":
            in_bitmap = True
        elif parts[0] == "ENDCHAR":
            in_bitmap = False
            if not (128 <= code <= 65535) or bbox is None:
                continue
            width, height, dx, dy = bbox
            assert len(rows) == height
            bitmap = bytearray(24)
            fits = True
            # Keep the native pixels. Shift small ascenders/quotes into the
            # cell when the BDF baseline puts them just outside it.
            x_origin = max(0, min(dx, 12 - width)) if width <= 12 else dx
            top = 10 - dy - height
            y_origin = max(0, min(top, 12 - height)) if height <= 12 else top
            for row, bits in enumerate(rows):
                for col in range(width):
                    if not (bits & (1 << (((width + 7)//8)*8 - col - 1))):
                        continue
                    x, y = x_origin + col, y_origin + row
                    if not (0 <= x < 12 and 0 <= y < 12):
                        fits = False
                    else:
                        bitmap[y*2 + x//8] |= 128 >> (x%8)
            if fits and any(bitmap):
                data[8 + code*24:8 + (code+1)*24] = bitmap
                replaced += 1
            elif not fits:
                clipped += 1
        elif in_bitmap:
            rows.append(int(parts[0], 16))
    args.output.write_bytes(data)
    print(f"Native glyphs: {replaced}; oversized omitted: {clipped}; bytes: {len(data)}")

if __name__ == "__main__":
    main()
