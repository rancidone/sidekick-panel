#pragma once

#include "magicpanel/canvas.h"

namespace magicpanel::draw {

void rect(Canvas& canvas, int x0, int y0, int x1, int y1, Color color);
void line(Canvas& canvas, int x0, int y0, int x1, int y1, Color color);
void plus(Canvas& canvas, int x, int y, int radius, Color color);

}  // namespace magicpanel::draw
