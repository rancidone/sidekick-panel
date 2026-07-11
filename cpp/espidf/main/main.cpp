#include <cstdio>
#include <memory>
#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

#include "magicpanel/app.h"
#include "magicpanel/engine.h"

namespace {

class EspIdfCanvas final : public magicpanel::FrameBufferCanvas {
 public:
  void present() override {
    // TODO: Wire this framebuffer to a concrete HUB75/I2S-DMA display driver.
    // The core already renders into pixels_; the adapter boundary is here.
  }
};

class EspPlatform final : public magicpanel::Platform {
 public:
  bool poll_event(magicpanel::Event& event) override {
    (void)event;
    // TODO: Replace with BLE/UART/Wi-Fi JSON-line transport for device input.
    return false;
  }

  float now_seconds() override {
    return static_cast<float>(esp_timer_get_time()) / 1000000.0f;
  }

  void sleep_seconds(float seconds) override {
    int ticks = static_cast<int>((seconds * 1000.0f) / portTICK_PERIOD_MS);
    vTaskDelay(ticks > 0 ? ticks : 1);
  }
};

}  // namespace

extern "C" void app_main(void) {
  std::printf("Magic Panel ESP-IDF runtime starting\n");

  magicpanel::MagicPanelApp app;
  EspIdfCanvas canvas;
  EspPlatform platform;

  magicpanel::run_engine(canvas, app.scenes(), app.liveness(), platform);
}
