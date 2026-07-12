#!/usr/bin/env python3
import os
import struct
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
ASSETS = ROOT / "assets"
OUT = ROOT / "cpp" / "generated" / "magicpanel" / "sprite_assets.h"

WAND_MASK_REGION = (33, 35, 50, 42)
WAND_MASK_LINE_START = (37.0, 41.0)
WAND_MASK_LINE_END = (49.0, 36.0)
WAND_MASK_MAX_DISTANCE = 2.25
WAND_MASK_COLORS = {
    (16, 22, 74),
    (121, 69, 27),
    (74, 42, 16),
}

SPRITES = [
    ("wizard_idle", "wizard_0001.png", None),
    ("wizard_excite", "wizard_excite_0001.png", None),
    ("wizard_cast", "wizard_cast_0001.png", None),
    ("wizard_cast_no_wand", "wizard_cast_0001.png", (WAND_MASK_REGION, WAND_MASK_COLORS)),
    ("wizard_jump", "wizard_jump_0001.png", None),
    ("wizard_blink_1", "wizard_blink_0001.png", None),
    ("wizard_blink_2", "wizard_blink_0002.png", None),
    ("wizard_star_1", "wizard_star_pulse_0001.png", None),
    ("wizard_star_2", "wizard_star_pulse_0002.png", None),
    ("wizard_star_3", "wizard_star_pulse_0003.png", None),
    ("wizard_star_4", "wizard_star_pulse_0004.png", None),
    ("bug", "bug.png", None),
]


def bmp_pixels(path: Path):
    with tempfile.TemporaryDirectory() as tmp:
        bmp = Path(tmp) / (path.stem + ".bmp")
        subprocess.run(
            ["sips", "-s", "format", "bmp", str(path), "--out", str(bmp)],
            check=True,
            stdout=subprocess.DEVNULL,
        )
        data = bmp.read_bytes()

    if data[:2] != b"BM":
        raise ValueError(f"{path}: not a BMP after conversion")
    offset = struct.unpack_from("<I", data, 10)[0]
    dib_size = struct.unpack_from("<I", data, 14)[0]
    width, height, planes, bpp, compression, image_size = struct.unpack_from(
        "<iiHHII", data, 18
    )
    if dib_size < 40 or bpp != 32:
        raise ValueError(f"{path}: expected 32-bit BMP, got bpp={bpp}")

    top_down = height < 0
    h = abs(height)
    pixels = []
    for out_y in range(h):
        source_y = out_y if top_down else h - 1 - out_y
        row = []
        for x in range(width):
            i = offset + (source_y * width + x) * 4
            b, g, r, a = data[i : i + 4]
            row.append((a << 24) | (r << 16) | (g << 8) | b)
        pixels.extend(row)
    return width, h, pixels


def near_segment(x: int, y: int, start: tuple[float, float], end: tuple[float, float]) -> bool:
    sx, sy = start
    ex, ey = end
    dx = ex - sx
    dy = ey - sy
    length_sq = dx * dx + dy * dy
    if length_sq == 0:
        return False
    t = ((x - sx) * dx + (y - sy) * dy) / length_sq
    if t < -0.1 or t > 1.1:
        return False
    t = max(0.0, min(1.0, t))
    px = sx + dx * t
    py = sy + dy * t
    dist_sq = (x - px) * (x - px) + (y - py) * (y - py)
    return dist_sq <= WAND_MASK_MAX_DISTANCE * WAND_MASK_MAX_DISTANCE


def emit():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "#pragma once",
        "",
        "#include <cstdint>",
        "",
        '#include "magicpanel/sprite.h"',
        "",
        "namespace magicpanel::assets {",
        "",
    ]
    for name, filename, mask in SPRITES:
        width, height, pixels = bmp_pixels(ASSETS / filename)
        if mask is not None:
            (x0, y0, x1, y1), colors = mask
            for y in range(max(0, y0), min(height, y1 + 1)):
                for x in range(max(0, x0), min(width, x1 + 1)):
                    i = y * width + x
                    pixel = pixels[i]
                    rgb = ((pixel >> 16) & 0xFF, (pixel >> 8) & 0xFF, pixel & 0xFF)
                    if rgb in colors and near_segment(x, y, WAND_MASK_LINE_START, WAND_MASK_LINE_END):
                        pixels[i] = 0
        rgb565 = []
        mask = [0] * ((len(pixels) + 7) // 8)
        for i, pixel in enumerate(pixels):
            a = (pixel >> 24) & 0xFF
            r = (pixel >> 16) & 0xFF
            g = (pixel >> 8) & 0xFF
            b = pixel & 0xFF
            value = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            rgb565.append(value if a else 0)
            if a:
                mask[i // 8] |= 1 << (i % 8)

        lines.append(f"inline constexpr std::uint16_t k{name}_rgb565[] = {{")
        for i in range(0, len(rgb565), 12):
            chunk = ", ".join(f"0x{p:04X}u" for p in rgb565[i : i + 12])
            lines.append(f"  {chunk},")
        lines.append("};")
        lines.append(f"inline constexpr std::uint8_t k{name}_mask[] = {{")
        for i in range(0, len(mask), 16):
            chunk = ", ".join(f"0x{p:02X}u" for p in mask[i : i + 16])
            lines.append(f"  {chunk},")
        lines.append("};")
        lines.append(
            f"inline constexpr Sprite k{name}{{{width}, {height}, k{name}_rgb565, k{name}_mask}};"
        )
        lines.append("")
    lines.append("}  // namespace magicpanel::assets")
    OUT.write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    emit()
