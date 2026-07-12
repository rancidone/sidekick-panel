#pragma once

#include <cstdint>

#include "magicpanel/atmosphere.h"
#include "magicpanel/event.h"

namespace magicpanel {

class EnvironmentState {
 public:
  void handle_event(Event const& event);
  bool has_weather() const { return has_weather_; }
  AtmosphereConfig apply(AtmosphereConfig config) const;

 private:
  bool has_weather_ = false;
  SkyMode sky_ = SkyMode::Night;
  MoonMode moon_ = MoonMode::Moon;
  WeatherMode weather_ = WeatherMode::None;
  CloudCover cloud_cover_ = CloudCover::Light;
  FogDensity fog_ = FogDensity::None;
  std::uint8_t daylight_ = 0;
  std::uint8_t wind_ = 80;
};

}  // namespace magicpanel
