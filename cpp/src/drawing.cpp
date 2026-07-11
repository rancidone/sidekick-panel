#include "magicpanel/drawing.h"

#include <algorithm>
#include <cstdlib>

namespace magicpanel::draw {

void rect(Canvas& canvas, int x0, int y0, int x1, int y1, Color color) {
  if (x0 > x1) {
    std::swap(x0, x1);
  }
  if (y0 > y1) {
    std::swap(y0, y1);
  }
  for (int y = y0; y <= y1; ++y) {
    for (int x = x0; x <= x1; ++x) {
      canvas.set_pixel(x, y, color);
    }
  }
}

void line(Canvas& canvas, int x0, int y0, int x1, int y1, Color color) {
  int dx = std::abs(x1 - x0);
  int sx = x0 < x1 ? 1 : -1;
  int dy = -std::abs(y1 - y0);
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  while (true) {
    canvas.set_pixel(x0, y0, color);
    if (x0 == x1 && y0 == y1) {
      break;
    }
    int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void plus(Canvas& canvas, int x, int y, int radius, Color color) {
  for (int d = -radius; d <= radius; ++d) {
    canvas.set_pixel(x + d, y, color);
    canvas.set_pixel(x, y + d, color);
  }
}

}  // namespace magicpanel::draw
