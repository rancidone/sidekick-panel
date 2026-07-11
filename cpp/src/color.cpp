#include "magicpanel/color.h"

#include <algorithm>
#include <cmath>

namespace magicpanel {

Color blend(Color from, Color to, float strength) {
  strength = std::clamp(strength, 0.0f, 1.0f);
  auto channel = [strength](std::uint8_t a, std::uint8_t b) {
    return static_cast<std::uint8_t>(
        std::lround(static_cast<float>(a) * (1.0f - strength) +
                    static_cast<float>(b) * strength));
  };
  return Color{channel(from.r, to.r), channel(from.g, to.g), channel(from.b, to.b)};
}

}  // namespace magicpanel
