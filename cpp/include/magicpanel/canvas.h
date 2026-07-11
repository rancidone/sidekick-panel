#pragma once

#include "magicpanel/color.h"

namespace magicpanel {

class Canvas {
 public:
  virtual ~Canvas() = default;

  virtual int width() const { return kPanelWidth; }
  virtual int height() const { return kPanelHeight; }
  virtual void clear(Color color) = 0;
  virtual void set_pixel(int x, int y, Color color) = 0;
  virtual Color get_pixel(int x, int y) const = 0;
  virtual void present() = 0;
  virtual bool poll_quit() { return false; }
};

class FrameBufferCanvas : public Canvas {
 public:
  FrameBufferCanvas();

  void clear(Color color) override;
  void set_pixel(int x, int y, Color color) override;
  Color get_pixel(int x, int y) const override;
  Color pixel(int x, int y) const { return get_pixel(x, y); }
  void present() override {}

 protected:
  Color pixels_[kPanelHeight][kPanelWidth];
};

}  // namespace magicpanel
