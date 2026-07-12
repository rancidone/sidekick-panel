#pragma once

#include <cstdint>

#include "magicpanel/canvas.h"

namespace magicpanel {

enum class MoonMode {
  None,
  Moon,
  BloodMoon,
  Sun,
};

enum class WeatherMode {
  None,
  PreRain,
  Rain,
  Storm,
  Thunderstorm,
  Snow,
  Ash,
};

enum class CloudCover {
  Clear,
  Light,
  Dense,
};

enum class FogDensity {
  None,
  Light,
  Dense,
};

enum class SkyMode {
  Night,
  Day,
  Storm,
};

struct AtmosphereConfig {
  Color background;
  SkyMode sky = SkyMode::Night;
  std::uint8_t daylight = 0;
  MoonMode moon = MoonMode::Moon;
  WeatherMode weather = WeatherMode::None;
  CloudCover cloud_cover = CloudCover::Light;
  FogDensity fog = FogDensity::None;
  bool clouds = false;
  bool stars = true;
  bool grass = true;
  bool motes = true;
  bool horizon_dither = true;
  // Localized fire from a player-triggered event (e.g. a fireball impact), not
  // ambient weather. 0 means no fire; decays to 0 as the effect fades.
  float fire_hazard_intensity = 0.0f;
  float fire_hazard_x = -1000.0f;
  std::uint8_t wind = 80;
};

class AtmosphereSystem {
 public:
  void render(Canvas& canvas, float now_seconds, AtmosphereConfig const& config) const;

 private:
  static std::uint32_t hash(std::uint32_t value);
  static float unit(std::uint32_t value);
  static Color dim(Color color, float strength);

  void draw_sky(Canvas& canvas, AtmosphereConfig const& config) const;
  void draw_horizon(Canvas& canvas, float now_seconds, AtmosphereConfig const& config) const;
  void draw_stars(Canvas& canvas, float now_seconds, AtmosphereConfig const& config) const;
  void draw_moon(Canvas& canvas, float now_seconds, AtmosphereConfig const& config) const;
  void draw_clouds(Canvas& canvas, float now_seconds, AtmosphereConfig const& config) const;
  void draw_fog(Canvas& canvas, float now_seconds, AtmosphereConfig const& config) const;
  void draw_grass(Canvas& canvas, float now_seconds, AtmosphereConfig const& config) const;
  void draw_motes(Canvas& canvas, float now_seconds, AtmosphereConfig const& config) const;
  void draw_weather(Canvas& canvas, float now_seconds, AtmosphereConfig const& config) const;
};

}  // namespace magicpanel
