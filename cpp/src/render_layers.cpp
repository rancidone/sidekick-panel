#include "magicpanel/render_layers.h"

namespace magicpanel {

void LayeredCanvas::clear(Color color) {
  target_.clear(color);
  clear_layers(RenderLayer::Background);
}

void LayeredCanvas::set_pixel(int x, int y, Color color) {
  if (x < 0 || x >= width() || y < 0 || y >= height()) {
    return;
  }
  target_.set_pixel(x, y, color);
  layers_[y][x] = layer_;
}

RenderLayer LayeredCanvas::pixel_layer(int x, int y) const {
  if (x < 0 || x >= width() || y < 0 || y >= height()) {
    return RenderLayer::Background;
  }
  return layers_[y][x];
}

bool LayeredCanvas::pixel_in_layers(int x, int y, std::uint8_t mask) const {
  return (layer_mask(pixel_layer(x, y)) & mask) != 0;
}

void LayeredCanvas::set_pixel_preserve_layer(int x, int y, Color color) {
  target_.set_pixel(x, y, color);
}

void LayeredCanvas::clear_layers(RenderLayer layer) {
  for (auto& row : layers_) {
    for (auto& pixel_layer : row) {
      pixel_layer = layer;
    }
  }
}

}  // namespace magicpanel
