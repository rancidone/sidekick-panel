#pragma once

#include <cstdint>

#include "magicpanel/canvas.h"

namespace magicpanel {

enum class RenderLayer : std::uint8_t {
  Background = 0,
  Atmosphere = 1,
  Actor = 2,
  Effect = 3,
  Foreground = 4,
};

constexpr std::uint8_t layer_mask(RenderLayer layer) {
  return static_cast<std::uint8_t>(1u << static_cast<std::uint8_t>(layer));
}

class LayeredCanvas final : public Canvas {
 public:
  explicit LayeredCanvas(Canvas& target) : target_(target) { clear_layers(RenderLayer::Background); }

  int width() const override { return target_.width(); }
  int height() const override { return target_.height(); }
  void clear(Color color) override;
  void set_pixel(int x, int y, Color color) override;
  Color get_pixel(int x, int y) const override { return target_.get_pixel(x, y); }
  void present() override { target_.present(); }
  bool poll_quit() override { return target_.poll_quit(); }

  void set_layer(RenderLayer layer) { layer_ = layer; }
  RenderLayer layer() const { return layer_; }
  RenderLayer pixel_layer(int x, int y) const;
  bool pixel_in_layers(int x, int y, std::uint8_t mask) const;
  void set_pixel_preserve_layer(int x, int y, Color color);

 private:
  void clear_layers(RenderLayer layer);

  Canvas& target_;
  RenderLayer layer_ = RenderLayer::Background;
  RenderLayer layers_[kPanelHeight][kPanelWidth]{};
};

class ScopedRenderLayer {
 public:
  ScopedRenderLayer(LayeredCanvas& canvas, RenderLayer layer)
      : canvas_(canvas), previous_(canvas.layer()) {
    canvas_.set_layer(layer);
  }

  ~ScopedRenderLayer() { canvas_.set_layer(previous_); }

 private:
  LayeredCanvas& canvas_;
  RenderLayer previous_;
};

}  // namespace magicpanel
