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

SPRITES = {
    "wizard_idle": "wizard_0001.png",
    "wizard_excite": "wizard_excite_0001.png",
    "wizard_cast": "wizard_cast_0001.png",
    "wizard_jump": "wizard_jump_0001.png",
    "wizard_blink_1": "wizard_blink_0001.png",
    "wizard_blink_2": "wizard_blink_0002.png",
    "wizard_star_1": "wizard_star_pulse_0001.png",
    "wizard_star_2": "wizard_star_pulse_0002.png",
    "wizard_star_3": "wizard_star_pulse_0003.png",
    "wizard_star_4": "wizard_star_pulse_0004.png",
    "bug": "bug.png",
}


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
    for name, filename in SPRITES.items():
        width, height, pixels = bmp_pixels(ASSETS / filename)
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
