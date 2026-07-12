#include "magicpanel/sprite.h"

#include <algorithm>

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

void draw_sprite_projected_shadow(Canvas& canvas,
                                  Sprite const& sprite,
                                  int origin_x,
                                  int origin_y,
                                  int ground_y,
                                  int cast_x,
                                  Color shadow,
                                  float strength) {
  // Contact shadow: a soft pool anchored right at the feet, under whichever
  // row of the sprite is lowest. The per-row projection below only traces
  // the sprite's own silhouette, which can read as a thin sliver rather than
  // solid ground contact — the pool anchors it.
  int feet_row = -1;
  int feet_min_x = sprite.width;
  int feet_max_x = 0;
  for (int y = sprite.height - 1; y >= 0; --y) {
    bool any = false;
    for (int x = 0; x < sprite.width; ++x) {
      if (mask_bit(sprite, x, y)) {
        any = true;
        feet_min_x = std::min(feet_min_x, x);
        feet_max_x = std::max(feet_max_x, x);
      }
    }
    if (any) {
      feet_row = y;
      break;
    }
  }
  if (feet_row >= 0) {
    int pool_cx = origin_x + (feet_min_x + feet_max_x) / 2;
    int pool_y = ground_y;
    int rx = std::max(4, static_cast<int>((feet_max_x - feet_min_x) * 0.32f));
    int ry = 2;
    for (int dy = -ry; dy <= ry; ++dy) {
      for (int dx = -rx; dx <= rx; ++dx) {
        float norm = (static_cast<float>(dx) / static_cast<float>(rx)) * (static_cast<float>(dx) / static_cast<float>(rx)) +
                     (static_cast<float>(dy) / static_cast<float>(ry)) * (static_cast<float>(dy) / static_cast<float>(ry));
        if (norm > 1.0f) {
          continue;
        }
        float pool_strength = strength * 0.9f * (1.0f - norm);
        Color base = canvas.get_pixel(pool_cx + dx, pool_y + dy);
        canvas.set_pixel(pool_cx + dx, pool_y + dy, blend(base, shadow, pool_strength));
      }
    }
  }

  for (int y = 0; y < sprite.height; y += 2) {
    int world_y = origin_y + y;
    int height_above_ground = ground_y - world_y;
    if (height_above_ground < 0) {
      continue;
    }
    int projected_y = ground_y - height_above_ground / 7;
    // Zero shift at the feet (height_above_ground == 0); leans away by cast_x
    // per unit of height as a row rises off the ground.
    int projected_shift = (cast_x * height_above_ground) / 24;
    float fade = 1.0f - static_cast<float>(height_above_ground) / 74.0f;
    float row_strength = strength * (fade < 0.25f ? 0.25f : fade);
    for (int x = 0; x < sprite.width; x += 2) {
      if (!mask_bit(sprite, x, y)) {
        continue;
      }
      int projected_x = origin_x + x + projected_shift;
      Color base = canvas.get_pixel(projected_x, projected_y);
      canvas.set_pixel(projected_x, projected_y, blend(base, shadow, row_strength));
      Color soft_base = canvas.get_pixel(projected_x + 1, projected_y);
      canvas.set_pixel(projected_x + 1, projected_y, blend(soft_base, shadow, row_strength * 0.55f));
    }
  }
}

}  // namespace magicpanel
