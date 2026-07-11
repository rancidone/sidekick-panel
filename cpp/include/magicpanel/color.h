#pragma once

#include <cstdint>

namespace magicpanel {

struct Color {
  std::uint8_t r;
  std::uint8_t g;
  std::uint8_t b;
};

constexpr Color kBlack{0, 0, 0};
constexpr int kPanelWidth = 128;
constexpr int kPanelHeight = 64;

Color blend(Color from, Color to, float strength);

}  // namespace magicpanel
