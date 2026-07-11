#include "magicpanel/canvas.h"

namespace magicpanel {

FrameBufferCanvas::FrameBufferCanvas() {
  clear(kBlack);
}

void FrameBufferCanvas::clear(Color color) {
  for (auto& row : pixels_) {
    for (auto& pixel : row) {
      pixel = color;
    }
  }
}

void FrameBufferCanvas::set_pixel(int x, int y, Color color) {
  if (x < 0 || x >= width() || y < 0 || y >= height()) {
    return;
  }
  pixels_[y][x] = color;
}

Color FrameBufferCanvas::get_pixel(int x, int y) const {
  if (x < 0 || x >= width() || y < 0 || y >= height()) {
    return kBlack;
  }
  return pixels_[y][x];
}

}  // namespace magicpanel
