#include "magicpanel/environment.h"

#include <algorithm>
#include <cstdlib>
#include <string>

namespace magicpanel {
namespace {

std::uint8_t byte_field(Event const& event, char const* key, std::uint8_t fallback) {
  std::string value = event.field_or(key);
  if (value.empty()) {
    return fallback;
  }
  int parsed = std::atoi(value.c_str());
  return static_cast<std::uint8_t>(std::clamp(parsed, 0, 255));
}

SkyMode sky_from(std::string const& value, SkyMode fallback) {
  if (value == "day") {
    return SkyMode::Day;
  }
  if (value == "storm") {
    return SkyMode::Storm;
  }
  if (value == "night") {
    return SkyMode::Night;
  }
  return fallback;
}

MoonMode moon_from(std::string const& value, MoonMode fallback) {
  if (value == "sun") {
    return MoonMode::Sun;
  }
  if (value == "moon") {
    return MoonMode::Moon;
  }
  if (value == "blood_moon") {
    return MoonMode::BloodMoon;
  }
  if (value == "none") {
    return MoonMode::None;
  }
  return fallback;
}

CloudCover clouds_from(std::string const& value, CloudCover fallback) {
  if (value == "clear" || value == "none") {
    return CloudCover::Clear;
  }
  if (value == "light") {
    return CloudCover::Light;
  }
  if (value == "dense" || value == "heavy") {
    return CloudCover::Dense;
  }
  return fallback;
}

FogDensity fog_from(std::string const& value, FogDensity fallback) {
  if (value == "none" || value == "clear") {
    return FogDensity::None;
  }
  if (value == "light") {
    return FogDensity::Light;
  }
  if (value == "dense" || value == "heavy") {
    return FogDensity::Dense;
  }
  return fallback;
}

WeatherMode weather_from(std::string const& value, WeatherMode fallback) {
  if (value == "none" || value == "clear") {
    return WeatherMode::None;
  }
  if (value == "pre_rain" || value == "prerain") {
    return WeatherMode::PreRain;
  }
  if (value == "rain" || value == "raining") {
    return WeatherMode::Rain;
  }
  if (value == "storm" || value == "storming") {
    return WeatherMode::Storm;
  }
  if (value == "thunderstorm" || value == "thunder") {
    return WeatherMode::Thunderstorm;
  }
  if (value == "snow") {
    return WeatherMode::Snow;
  }
  if (value == "ash") {
    return WeatherMode::Ash;
  }
  return fallback;
}

}  // namespace

void EnvironmentState::handle_event(Event const& event) {
  if (event.name != "weather" && event.name != "time_of_day") {
    return;
  }
  has_weather_ = true;
  daylight_ = byte_field(event, "daylight", daylight_);
  wind_ = byte_field(event, "wind", wind_);
  sky_ = sky_from(event.field_or("sky"), sky_);
  moon_ = moon_from(event.field_or("moon"), moon_);
  cloud_cover_ = clouds_from(event.field_or("clouds"), cloud_cover_);
  fog_ = fog_from(event.field_or("fog"), fog_);
  weather_ = weather_from(event.field_or("precip"), weather_);
  weather_ = weather_from(event.field_or("weather"), weather_);

  if (weather_ == WeatherMode::Storm || weather_ == WeatherMode::Thunderstorm) {
    sky_ = SkyMode::Storm;
    cloud_cover_ = CloudCover::Dense;
  }
}

AtmosphereConfig EnvironmentState::apply(AtmosphereConfig config) const {
  if (!has_weather_) {
    return config;
  }
  config.sky = sky_;
  config.daylight = daylight_;
  config.moon = moon_;
  config.weather = weather_;
  config.cloud_cover = cloud_cover_;
  config.fog = fog_;
  config.clouds = cloud_cover_ != CloudCover::Clear;
  config.stars = daylight_ < 150 && sky_ != SkyMode::Storm;
  config.motes = daylight_ < 120 && fog_ == FogDensity::None;
  config.wind = wind_;
  return config;
}

}  // namespace magicpanel
