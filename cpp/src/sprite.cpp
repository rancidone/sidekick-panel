#include "magicpanel/sprite.h"

namespace magicpanel {
namespace {

bool mask_bit(Sprite const& sprite, int x, int y) {
  int index = y * sprite.width + x;
  return (sprite.alpha_mask[index / 8] & (1u << (index % 8))) != 0;
}

Color from_rgb565(std::uint16_t value) {
  std::uint8_t r5 = static_cast<std::uint8_t>((value >> 11) & 0x1F);
  std::uint8_t g6 = static_cast<std::uint8_t>((value >> 5) & 0x3F);
  std::uint8_t b5 = static_cast<std::uint8_t>(value & 0x1F);
  return Color{
      static_cast<std::uint8_t>((r5 << 3) | (r5 >> 2)),
      static_cast<std::uint8_t>((g6 << 2) | (g6 >> 4)),
      static_cast<std::uint8_t>((b5 << 3) | (b5 >> 2)),
  };
}

}  // namespace

void draw_sprite(Canvas& canvas,
                 Sprite const& sprite,
                 int origin_x,
                 int origin_y,
                 Color tint,
                 float tint_strength) {
  for (int y = 0; y < sprite.height; ++y) {
    for (int x = 0; x < sprite.width; ++x) {
      if (!mask_bit(sprite, x, y)) {
        continue;
      }
      Color color = from_rgb565(sprite.rgb565[y * sprite.width + x]);
      if (tint_strength > 0.0f) {
        color = blend(color, tint, tint_strength);
      }
      canvas.set_pixel(origin_x + x, origin_y + y, color);
    }
  }
}

}  // namespace magicpanel
