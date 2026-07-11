#pragma once

#include <cstddef>
#include <cstdint>

#include "magicpanel/canvas.h"

namespace magicpanel {

struct Sprite {
  int width;
  int height;
  std::uint16_t const* rgb565;
  std::uint8_t const* alpha_mask;
};

void draw_sprite(Canvas& canvas,
                 Sprite const& sprite,
                 int origin_x,
                 int origin_y,
                 Color tint = Color{255, 255, 255},
                 float tint_strength = 0.0f);

}  // namespace magicpanel
