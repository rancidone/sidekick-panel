#include "magicpanel/status_panel.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <cmath>

#include "magicpanel/color.h"
#include "magicpanel/drawing.h"

namespace magicpanel {
namespace {

constexpr Color kInk{48, 34, 28};
constexpr Color kInkDim{82, 58, 45};
constexpr Color kPaperDark{104, 70, 46};
constexpr Color kPaperMid{176, 126, 76};
constexpr Color kPaperLight{232, 190, 118};
constexpr Color kPaperGlow{255, 222, 142};
constexpr Color kWood{90, 54, 32};
constexpr Color kRunGreen{72, 132, 76};
constexpr Color kRunBlue{82, 166, 210};
constexpr Color kRunGold{242, 184, 74};
constexpr Color kRunFail{210, 64, 52};

struct Glyph {
  char ch;
  std::array<std::uint8_t, 5> rows;
};

constexpr std::array<Glyph, 18> kGlyphs{{
    {'A', {0b111, 0b101, 0b111, 0b101, 0b101}},
    {'B', {0b110, 0b101, 0b110, 0b101, 0b110}},
    {'C', {0b111, 0b100, 0b100, 0b100, 0b111}},
    {'D', {0b110, 0b101, 0b101, 0b101, 0b110}},
    {'E', {0b111, 0b100, 0b110, 0b100, 0b111}},
    {'F', {0b111, 0b100, 0b110, 0b100, 0b100}},
    {'G', {0b111, 0b100, 0b101, 0b101, 0b111}},
    {'I', {0b111, 0b010, 0b010, 0b010, 0b111}},
    {'K', {0b101, 0b101, 0b110, 0b101, 0b101}},
    {'L', {0b100, 0b100, 0b100, 0b100, 0b111}},
    {'O', {0b111, 0b101, 0b101, 0b101, 0b111}},
    {'P', {0b111, 0b101, 0b111, 0b100, 0b100}},
    {'R', {0b110, 0b101, 0b110, 0b101, 0b101}},
    {'S', {0b111, 0b100, 0b111, 0b001, 0b111}},
    {'T', {0b111, 0b010, 0b010, 0b010, 0b010}},
    {'U', {0b101, 0b101, 0b101, 0b101, 0b111}},
    {'Y', {0b101, 0b101, 0b010, 0b010, 0b010}},
    {' ', {0b000, 0b000, 0b000, 0b000, 0b000}},
}};

Color mix(Color a, Color b, float t) {
  return blend(a, b, std::clamp(t, 0.0f, 1.0f));
}

Color dim(Color color, float strength) {
  strength = std::clamp(strength, 0.0f, 1.0f);
  return Color{static_cast<std::uint8_t>(static_cast<float>(color.r) * strength),
               static_cast<std::uint8_t>(static_cast<float>(color.g) * strength),
               static_cast<std::uint8_t>(static_cast<float>(color.b) * strength)};
}

Glyph const* glyph_for(char ch) {
  for (auto const& glyph : kGlyphs) {
    if (glyph.ch == ch) {
      return &glyph;
    }
  }
  return nullptr;
}

void draw_text_3x5(Canvas& canvas, int x, int y, char const* text, Color color) {
  int cursor = x;
  for (std::size_t i = 0; text[i] != '\0'; ++i) {
    Glyph const* glyph = glyph_for(text[i]);
    if (glyph != nullptr) {
      for (int row = 0; row < 5; ++row) {
        std::uint8_t bits = glyph->rows[static_cast<std::size_t>(row)];
        for (int col = 0; col < 3; ++col) {
          if ((bits & (1u << (2 - col))) != 0u) {
            canvas.set_pixel(cursor + col, y + row, color);
          }
        }
      }
    }
    cursor += 4;
  }
}

void draw_status_icon(Canvas& canvas, int x, int y, DeployStatusResult result, float pulse) {
  if (result == DeployStatusResult::Success) {
    draw::line(canvas, x, y + 2, x + 2, y + 4, kRunGreen);
    draw::line(canvas, x + 2, y + 4, x + 5, y, kRunGreen);
  } else if (result == DeployStatusResult::Failed) {
    draw::line(canvas, x, y, x + 5, y + 5, kRunFail);
    draw::line(canvas, x + 5, y, x, y + 5, kRunFail);
  } else {
    Color color = mix(kRunBlue, kRunGold, pulse);
    draw::plus(canvas, x + 3, y + 3, 2, color);
    canvas.set_pixel(x + 1, y + 1, dim(color, 0.65f));
    canvas.set_pixel(x + 5, y + 5, dim(color, 0.65f));
  }
}

void draw_rune_row(Canvas& canvas, int x, int y, int width, Color color, std::uint8_t progress) {
  int filled = (width * static_cast<int>(progress)) / 255;
  for (int i = 0; i < width; i += 4) {
    Color pixel = i <= filled ? color : dim(kInkDim, 0.45f);
    canvas.set_pixel(x + i, y, pixel);
    if ((i & 7) == 0) {
      canvas.set_pixel(x + i + 1, y + 1, dim(pixel, 0.75f));
    }
  }
}

}  // namespace

void draw_deploy_status_scroll(Canvas& canvas,
                               int x,
                               int y,
                               float now_seconds,
                               DeployStatusPanel const& panel) {
  int open = std::clamp(static_cast<int>(panel.open), 0, 255);
  if (open == 0) {
    return;
  }

  constexpr int kMaxWidth = 65;
  constexpr int kHeight = 28;
  int body_width = std::max(8, (kMaxWidth * open) / 255);
  int right = x + body_width - 1;
  int bottom = y + kHeight - 1;
  float pulse = 0.5f + 0.5f * std::sin(now_seconds * 7.0f);

  draw::rect(canvas, x + 2, y + 3, right + 1, bottom + 2, Color{18, 15, 18});
  draw::rect(canvas, x + 1, y + 2, right, bottom, kPaperDark);
  draw::rect(canvas, x + 3, y + 4, right - 2, bottom - 2, kPaperMid);

  for (int yy = y + 5; yy <= bottom - 3; ++yy) {
    float t = static_cast<float>(yy - (y + 5)) / 20.0f;
    Color paper = mix(kPaperLight, kPaperMid, t * 0.45f);
    for (int xx = x + 4; xx <= right - 3; ++xx) {
      if (((xx * 5 + yy * 3) & 31) == 0) {
        paper = mix(paper, kPaperGlow, 0.2f);
      }
      canvas.set_pixel(xx, yy, paper);
    }
  }

  draw::rect(canvas, x, y + 1, x + 3, bottom - 1, kWood);
  draw::rect(canvas, right - 1, y + 1, right + 2, bottom - 1, kWood);
  draw::line(canvas, x + 4, y + 3, right - 3, y + 3, kInkDim);
  draw::line(canvas, x + 4, bottom - 2, right - 3, bottom - 2, kInkDim);

  if (body_width < 24) {
    return;
  }

  char const* title = panel.kind == DeployStatusKind::Build ? "BUILD" : "DEPLOY";
  draw_text_3x5(canvas, x + 8, y + 6, title, kInk);
  draw_status_icon(canvas, right - 11, y + 5, panel.result, pulse);

  int bar_x = x + 8;
  int bar_y = y + 15;
  int bar_w = std::max(6, body_width - 21);
  draw::rect(canvas, bar_x - 1, bar_y - 1, bar_x + bar_w, bar_y + 3, kInkDim);
  draw::rect(canvas, bar_x, bar_y, bar_x + bar_w - 1, bar_y + 2, Color{64, 42, 36});
  int filled = (bar_w * static_cast<int>(panel.progress)) / 255;
  Color fill = panel.result == DeployStatusResult::Failed ? kRunFail
              : panel.result == DeployStatusResult::Success ? kRunGreen
                                                            : mix(kRunBlue, kRunGold, pulse * 0.45f);
  if (filled > 0) {
    draw::rect(canvas, bar_x, bar_y, bar_x + filled - 1, bar_y + 2, fill);
  }

  draw_rune_row(canvas, x + 8, y + 22, std::max(4, body_width - 21), kInk, panel.progress);
  if (panel.result != DeployStatusResult::Running) {
    Color mark = panel.result == DeployStatusResult::Success ? kInk : kRunFail;
    draw_status_icon(canvas, right - 15, bottom - 8, panel.result, pulse);
    canvas.set_pixel(right - 12, bottom - 2, dim(mark, 0.75f));
  }
}

}  // namespace magicpanel
