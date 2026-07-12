#include "magicpanel/atmosphere.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "magicpanel/color.h"
#include "magicpanel/drawing.h"

namespace magicpanel {
namespace {

constexpr Color kStarDim{44, 50, 70};
constexpr Color kStarBright{145, 142, 118};
constexpr Color kDaySkyTop{54, 92, 118};
constexpr Color kDaySkyLow{104, 132, 130};
constexpr Color kStormSkyTop{28, 36, 48};
constexpr Color kStormSkyLow{56, 62, 62};
constexpr Color kCloudCool{118, 132, 132};
constexpr Color kCloudLight{176, 184, 166};
constexpr Color kCloudShadow{70, 82, 86};
constexpr Color kCanopyBack{8, 18, 25};
constexpr Color kCanopyCool{16, 35, 42};
constexpr Color kCanopyDark{6, 25, 22};
constexpr Color kCanopyMid{12, 45, 35};
// Daylit counterparts for the distant horizon silhouette, so the skyline
// lightens with the sky instead of staying night-dark at noon.
constexpr Color kCanopyBackDay{58, 78, 66};
constexpr Color kCanopyCoolDay{74, 100, 82};
constexpr Color kCanopyDarkDay{46, 78, 52};
constexpr Color kBarkWarm{72, 45, 29};
constexpr Color kGroundShadow{5, 18, 13};
constexpr Color kSoil{38, 29, 23};
constexpr Color kSoilWarm{96, 62, 35};
constexpr Color kGrassDark{7, 31, 19};
constexpr Color kGrassMid{16, 52, 30};
constexpr Color kGrassTip{38, 82, 45};
constexpr Color kGrassLit{72, 118, 52};
constexpr Color kGrassDry{68, 65, 34};
constexpr Color kGrassDead{39, 34, 22};
constexpr Color kFlowerWhite{210, 220, 198};
constexpr Color kFlowerGold{224, 130, 34};
constexpr Color kFlowerBlue{105, 152, 185};
constexpr Color kFlowerRose{188, 82, 96};
constexpr Color kFireHot{238, 146, 36};
constexpr Color kFireCore{255, 218, 92};
constexpr Color kCharcoal{20, 18, 17};
constexpr Color kMoonBase{172, 180, 192};
constexpr Color kMoonLight{226, 220, 190};
constexpr Color kBloodMoonBase{150, 38, 28};
constexpr Color kBloodMoonLight{235, 93, 44};
constexpr Color kSunBase{255, 168, 66};
constexpr Color kSunLight{255, 228, 126};

Color mix(Color a, Color b, float t) {
  return blend(a, b, std::clamp(t, 0.0f, 1.0f));
}

float daylight_amount(AtmosphereConfig const& config) {
  return static_cast<float>(config.daylight) / 255.0f;
}

Color scale(Color color, float strength) {
  strength = std::clamp(strength, 0.0f, 1.0f);
  return Color{static_cast<std::uint8_t>(static_cast<float>(color.r) * strength),
               static_cast<std::uint8_t>(static_cast<float>(color.g) * strength),
               static_cast<std::uint8_t>(static_cast<float>(color.b) * strength)};
}

std::uint32_t noise_hash(std::uint32_t value) {
  value ^= value >> 16;
  value *= 0x7feb352du;
  value ^= value >> 15;
  value *= 0x846ca68bu;
  value ^= value >> 16;
  return value;
}

float noise_unit(std::uint32_t value) {
  return static_cast<float>(noise_hash(value) & 0x00FFFFFFu) / 16777215.0f;
}

float wave(float now_seconds, int x, std::uint32_t salt, float speed) {
  float phase = noise_unit(static_cast<std::uint32_t>(x) * 97u + salt) * 6.2831853f;
  return 0.5f + 0.5f * std::sin(now_seconds * speed + phase);
}

float cycle01(float now_seconds, float seconds, std::uint32_t salt) {
  float offset = noise_unit(salt) * seconds;
  return std::fmod(now_seconds + offset, seconds) / seconds;
}

Color seasonal_grass(Color alive, float dry, float day) {
  // Grass used to hold the same brightness at midnight as at noon, which
  // read as too bright/saturated whenever the rest of the scene (sky,
  // horizon) dimmed for evening or night.
  alive = mix(scale(alive, 0.4f), alive, day);
  return mix(alive, dry > 0.65f ? kGrassDead : kGrassDry, dry);
}

void draw_grass_tuft(Canvas& canvas, int x, int base_y, int height, int sway, float seed, float dry, float day) {
  int h = std::max(4, height);
  Color shadow = kGroundShadow;
  Color dark = seasonal_grass(seed > 0.7f ? kGrassMid : kGrassDark, dry, day);
  Color mid = seasonal_grass(seed > 0.76f ? kGrassTip : kGrassMid, dry, day);
  Color tip = seasonal_grass(seed > 0.91f ? kGrassLit : kGrassTip, dry, day);

  draw::line(canvas, x - 3, base_y, x + 4, base_y, shadow);
  draw::line(canvas, x - 2, base_y - 1, x + 2, base_y - 1, dark);
  canvas.set_pixel(x - 1, base_y - 2, mid);
  canvas.set_pixel(x, base_y - 2, mid);
  canvas.set_pixel(x + 1, base_y - 2, mid);

  draw::line(canvas, x, base_y - 2, x + sway, base_y - h, tip);
  draw::line(canvas, x - 1, base_y - 2, x - 4 + sway, base_y - 2 - h / 2, mid);
  draw::line(canvas, x + 1, base_y - 2, x + 4 + sway, base_y - 2 - (h * 2) / 3, mid);

  if (seed > 0.55f) {
    draw::line(canvas, x - 2, base_y - 1, x - 6 + sway, base_y - 3, dark);
  }
  if (seed > 0.68f) {
    draw::line(canvas, x + 2, base_y - 1, x + 6 + sway, base_y - 4, dark);
  }
  if (seed > 0.82f) {
    draw::line(canvas, x + 2, base_y - 2, x + 2 + sway, base_y - h + 1, scale(tip, 0.75f));
  }
}

void draw_meadow_stalk(Canvas& canvas, int x, int base_y, int height, int bend, Color color) {
  int mid_x = x + bend / 2;
  int mid_y = base_y - height / 2;
  int tip_x = x + bend;
  int tip_y = base_y - height;
  draw::line(canvas, x, base_y, mid_x, mid_y, color);
  draw::line(canvas, mid_x, mid_y, tip_x, tip_y, color);
}

void draw_flower(Canvas& canvas, int x, int y, Color petal, Color center) {
  canvas.set_pixel(x, y, center);
  canvas.set_pixel(x - 1, y, petal);
  canvas.set_pixel(x + 1, y, petal);
  canvas.set_pixel(x, y - 1, petal);
  canvas.set_pixel(x, y + 1, scale(petal, 0.7f));
}

void draw_sunflower(Canvas& canvas, int x, int y) {
  Color petal = Color{232, 172, 34};
  Color center = Color{92, 58, 26};
  draw_flower(canvas, x, y, petal, center);
  canvas.set_pixel(x - 2, y, scale(petal, 0.85f));
  canvas.set_pixel(x + 2, y, scale(petal, 0.85f));
}

void draw_dandelion(Canvas& canvas, int x, int y, bool seed_head) {
  Color puff = seed_head ? Color{204, 212, 188} : Color{230, 210, 78};
  canvas.set_pixel(x, y, puff);
  canvas.set_pixel(x - 1, y, scale(puff, 0.82f));
  canvas.set_pixel(x + 1, y, scale(puff, 0.82f));
  canvas.set_pixel(x, y - 1, scale(puff, 0.88f));
}

void draw_tree_canopy(Canvas& canvas, int cx, int cy, int radius, float fill, Color color) {
  int r = std::max(1, radius);
  for (int y = -r; y <= r; ++y) {
    for (int x = -r - 1; x <= r + 1; ++x) {
      float dx = static_cast<float>(x) / static_cast<float>(r + 1);
      float dy = static_cast<float>(y) / static_cast<float>(r);
      float dist = dx * dx + dy * dy;
      float noise = noise_unit(static_cast<std::uint32_t>((cx + x) * 47 + (cy + y) * 89));
      if (dist < fill + noise * 0.2f) {
        canvas.set_pixel(cx + x, cy + y, noise > 0.74f ? kCanopyMid : color);
      }
    }
  }
}

struct CloudStratum {
  int count;
  int min_y;
  int y_range;
  int min_width;
  int width_range;
  int height;
  float speed;
  float shade;
  std::uint32_t salt;
};

struct CloudPalette {
  Color top;
  Color middle;
  Color bottom;
  Color edge;
};

struct CloudPuff {
  float x;
  float y;
  float rx;
  float ry;
};

CloudPalette cloud_palette(AtmosphereConfig const& config, float depth) {
  float day = daylight_amount(config);
  if (config.sky == SkyMode::Storm) {
    return CloudPalette{mix(Color{54, 62, 70}, kCloudShadow, depth),
                        mix(Color{36, 44, 52}, Color{48, 58, 64}, depth),
                        mix(Color{18, 24, 32}, Color{32, 38, 46}, depth),
                        Color{16, 20, 28}};
  }

  float twilight = std::clamp(1.0f - std::abs(day - 0.35f) / 0.26f, 0.0f, 1.0f);
  Color night_top = Color{28, 34, 45};
  Color night_mid = Color{20, 28, 38};
  Color night_bottom = Color{10, 15, 24};
  Color top = mix(night_top, kCloudLight, day);
  Color middle = mix(night_mid, kCloudCool, day);
  Color bottom = mix(night_bottom, kCloudShadow, day);
  bottom = mix(bottom, Color{156, 96, 62}, twilight * 0.45f);
  return CloudPalette{mix(top, middle, depth * 0.28f),
                      mix(middle, bottom, depth * 0.2f),
                      mix(bottom, Color{36, 42, 48}, depth * 0.25f),
                      mix(bottom, night_bottom, 0.35f)};
}

void draw_cloud_stratum(Canvas& canvas,
                        float now_seconds,
                        AtmosphereConfig const& config,
                        CloudStratum const& stratum,
                        float depth) {
  float palette_depth = std::clamp(depth + stratum.shade * 0.18f, 0.0f, 1.0f);
  CloudPalette palette = cloud_palette(config, palette_depth);
  for (int i = 0; i < stratum.count; ++i) {
    std::uint32_t cloud_id = stratum.salt + static_cast<std::uint32_t>(i) * 101u;
    float seed = noise_unit(cloud_id + 33u);
    // Cloud speed tracks wind, not a fixed rate — calm air barely drifts,
    // storms push clouds along briskly.
    float wind_factor = std::clamp(static_cast<float>(config.wind) / 400.0f, 0.05f, 0.6f);
    float drift = now_seconds * stratum.speed * wind_factor * (0.4f + seed);
    int base_x = static_cast<int>(std::fmod(seed * 128.0f + drift, 150.0f)) - 22;
    int base_y = stratum.min_y + static_cast<int>(noise_unit(cloud_id + 71u) *
                                                  static_cast<float>(stratum.y_range));
    int width = stratum.min_width + static_cast<int>(noise_unit(cloud_id + 37u) *
                                                     static_cast<float>(stratum.width_range));
    int height = stratum.height;
    int puff_count = 4 + static_cast<int>(noise_unit(cloud_id + 5u) * 3.0f);
    std::array<CloudPuff, 7> puffs{};
    float center_x = static_cast<float>(base_x) + static_cast<float>(width) * 0.5f;
    float center_y = static_cast<float>(base_y);
    float morph = std::sin(now_seconds * (0.16f + seed * 0.08f) + seed * 9.0f);

    for (int p = 0; p < puff_count; ++p) {
      float t = puff_count == 1 ? 0.5f : static_cast<float>(p) / static_cast<float>(puff_count - 1);
      float jitter_x = (noise_unit(cloud_id + static_cast<std::uint32_t>(p) * 41u + 13u) - 0.5f) *
                       static_cast<float>(width) * 0.18f;
      float jitter_y = (noise_unit(cloud_id + static_cast<std::uint32_t>(p) * 37u + 29u) - 0.5f) *
                       static_cast<float>(height) * 0.85f;
      float crown = std::sin(t * 3.1415926f);
      float radius_seed = noise_unit(cloud_id + static_cast<std::uint32_t>(p) * 61u + 7u);
      float breathe = 1.0f + 0.10f * std::sin(now_seconds * (0.21f + radius_seed * 0.12f) +
                                              radius_seed * 6.2831853f);
      puffs[static_cast<std::size_t>(p)] =
          CloudPuff{center_x - static_cast<float>(width) * 0.38f + t * static_cast<float>(width) * 0.76f +
                        jitter_x + morph * (radius_seed - 0.5f) * 1.2f,
                    center_y - crown * static_cast<float>(height) * 0.42f + jitter_y,
                    (static_cast<float>(width) * (0.16f + radius_seed * 0.08f)) * breathe,
                    (static_cast<float>(height) * (0.52f + radius_seed * 0.22f)) * breathe};
    }

    int min_x = base_x - 2;
    int max_x = base_x + width + 2;
    int min_y = base_y - height - 2;
    int max_y = base_y + height + 3;
    for (int y = min_y; y <= max_y; ++y) {
      for (int x = min_x; x <= max_x; ++x) {
        float best = 10.0f;
        float weighted_y = 0.0f;
        float weight = 0.0f;
        for (int p = 0; p < puff_count; ++p) {
          CloudPuff const& puff = puffs[static_cast<std::size_t>(p)];
          float dx = (static_cast<float>(x) - puff.x) / std::max(1.0f, puff.rx);
          float dy = (static_cast<float>(y) - puff.y) / std::max(1.0f, puff.ry);
          float d = dx * dx + dy * dy;
          if (d < best) {
            best = d;
          }
          if (d < 1.25f) {
            float w = 1.25f - d;
            weighted_y += puff.y * w;
            weight += w;
          }
        }
        float edge_noise = noise_unit(static_cast<std::uint32_t>(x * 53 + y * 97 + cloud_id * 7u));
        if (best > 1.0f + (edge_noise - 0.5f) * 0.2f) {
          continue;
        }

        float local_center_y = weight > 0.0f ? weighted_y / weight : center_y;
        float vertical = std::clamp((static_cast<float>(y) - (local_center_y - static_cast<float>(height))) /
                                        static_cast<float>(height * 2),
                                    0.0f,
                                    1.0f);
        Color color = vertical < 0.34f ? palette.top : vertical < 0.66f ? palette.middle : palette.bottom;
        if (best > 0.76f) {
          color = mix(color, palette.edge, 0.45f);
        }
        if (edge_noise > 0.92f && vertical > 0.45f) {
          color = mix(color, palette.bottom, 0.35f);
        }
        canvas.set_pixel(base_x + x, base_y + y, color);
      }
    }
    if (config.sky == SkyMode::Storm) {
      draw::line(canvas, base_x + width / 4, base_y + height / 2,
                 base_x + width - 3, base_y + height / 2, palette.bottom);
    }
  }
}

}  // namespace

void AtmosphereSystem::render(Canvas& canvas, float now_seconds, AtmosphereConfig const& config) const {
  draw_sky(canvas, config);
  draw_horizon(canvas, now_seconds, config);
  if (config.stars) {
    draw_stars(canvas, now_seconds, config);
  }
  draw_moon(canvas, now_seconds, config);
  if (config.clouds) {
    draw_clouds(canvas, now_seconds, config);
  }
  if (config.motes) {
    draw_motes(canvas, now_seconds, config);
  }
  draw_weather(canvas, now_seconds, config);
  if (config.fog != FogDensity::None) {
    draw_fog(canvas, now_seconds, config);
  }
  if (config.grass) {
    draw_grass(canvas, now_seconds, config);
  }
}

std::uint32_t AtmosphereSystem::hash(std::uint32_t value) {
  value ^= value >> 16;
  value *= 0x7feb352du;
  value ^= value >> 15;
  value *= 0x846ca68bu;
  value ^= value >> 16;
  return value;
}

float AtmosphereSystem::unit(std::uint32_t value) {
  return static_cast<float>(hash(value) & 0x00FFFFFFu) / 16777215.0f;
}

Color AtmosphereSystem::dim(Color color, float strength) {
  strength = std::clamp(strength, 0.0f, 1.0f);
  return Color{static_cast<std::uint8_t>(static_cast<float>(color.r) * strength),
               static_cast<std::uint8_t>(static_cast<float>(color.g) * strength),
               static_cast<std::uint8_t>(static_cast<float>(color.b) * strength)};
}

void AtmosphereSystem::draw_sky(Canvas& canvas, AtmosphereConfig const& config) const {
  float day = daylight_amount(config);
  if (config.sky == SkyMode::Night && day <= 0.0f) {
    return;
  }
  Color target_top = config.sky == SkyMode::Storm ? kStormSkyTop : kDaySkyTop;
  Color target_low = config.sky == SkyMode::Storm ? kStormSkyLow : kDaySkyLow;
  float sky_strength = config.sky == SkyMode::Storm ? std::max(0.45f, day) : day;
  Color top = mix(config.background, target_top, sky_strength);
  Color low = mix(config.background, target_low, sky_strength);
  for (int y = 0; y < canvas.height(); ++y) {
    float t = std::clamp(static_cast<float>(y) / 48.0f, 0.0f, 1.0f);
    Color color = mix(top, low, t);
    for (int x = 0; x < canvas.width(); ++x) {
      canvas.set_pixel(x, y, color);
    }
  }
}

void AtmosphereSystem::draw_horizon(Canvas& canvas, float now_seconds, AtmosphereConfig const& config) const {
  if (!config.horizon_dither) {
    return;
  }
  // A static (non-animated) rolling profile: a few low-frequency sine waves
  // summed together, which reads as gentle rolling hills rather than the
  // jagged, block-stepped silhouette that bucketed per-x noise produced.
  // The phase seed ties to the same wall-clock epoch as the trees, so the
  // skyline itself reshuffles across sessions instead of being the exact
  // same fixed shape forever.
  float terrain_epoch = std::floor(now_seconds / 5400.0f);
  float terrain_seed = noise_unit(static_cast<std::uint32_t>(terrain_epoch) * 4523u) * 6.2831853f;
  auto horizon_edge = [terrain_seed](int x, int layer) {
    float fx = static_cast<float>(x);
    float phase = static_cast<float>(layer) * 2.7f + terrain_seed;
    float roll = std::sin(fx * 0.045f + phase) * 5.0f +
                std::sin(fx * 0.021f + phase * 1.7f + 1.3f) * 3.5f +
                std::sin(fx * 0.012f + phase * 0.6f) * 2.0f;
    // Occasional sharper peaks on the back (most distant) layer only, so the
    // skyline isn't uniformly rolling hills everywhere — a slow region
    // selector picks where peaks are allowed, and a steep power curve gives
    // them a pointed (not rolling) shape where they do appear.
    float peaks = 0.0f;
    if (layer == 0) {
      float region = std::max(0.0f, std::sin(fx * 0.007f + 4.0f + terrain_seed));
      float shape = std::pow(std::max(0.0f, std::sin(fx * 0.05f + 1.1f + terrain_seed)), 4.0f);
      peaks = region * shape * 10.0f;
    }
    int base = 31 + layer * 5;
    return base + static_cast<int>(roll) - static_cast<int>(peaks);
  };

  // Smooth (bilinear-interpolated) coarse value noise: a hint of distant
  // texture that fades gradually between sample points instead of the
  // hard-edged blocks a raw grid lookup produces.
  auto value_noise = [](float x, float y, std::uint32_t salt) {
    int x0 = static_cast<int>(std::floor(x));
    int y0 = static_cast<int>(std::floor(y));
    float tx = x - static_cast<float>(x0);
    float ty = y - static_cast<float>(y0);
    auto sample = [salt](int xi, int yi) {
      return noise_unit(static_cast<std::uint32_t>(xi * 131 + yi * 17) + salt);
    };
    float top = sample(x0, y0) + (sample(x0 + 1, y0) - sample(x0, y0)) * tx;
    float bottom = sample(x0, y0 + 1) + (sample(x0 + 1, y0 + 1) - sample(x0, y0 + 1)) * tx;
    return top + (bottom - top) * ty;
  };

  float day = daylight_amount(config);
  for (int layer = 0; layer < 3; ++layer) {
    Color night_color = layer == 0 ? kCanopyBack : layer == 1 ? kCanopyCool : kCanopyDark;
    Color day_color = layer == 0 ? kCanopyBackDay : layer == 1 ? kCanopyCoolDay : kCanopyDarkDay;
    Color color = mix(night_color, day_color, day);
    int shade_top = 45 + layer * 3;
    // Draw well past the shaded band, all the way to the ground — the
    // silhouette's *top* edge rolls (horizon_edge), but a fixed cutoff
    // underneath it is dead flat across every column and only sometimes
    // gets hidden by the meadow's own rolling edge above it, showing
    // through as a pale seam. Extending it removes that seam instead of
    // relying on the grass layer to fully mask it.
    int floor = canvas.height();
    for (int x = 0; x < canvas.width(); ++x) {
      int edge = horizon_edge(x, layer);
      edge = std::clamp(edge, 23, shade_top - 2);
      // A distant silhouette reads as one solid, soft mass — shade it with a
      // smooth top-to-bottom gradient rather than punching random holes
      // through it, which read as stray sky-colored speckle. A coarse,
      // low-contrast blotch (sampled on a multi-pixel grid, not per-pixel)
      // adds a hint of distant texture without reading as confetti.
      for (int y = edge; y <= floor; ++y) {
        float depth = std::clamp(static_cast<float>(y - edge) / static_cast<float>(std::max(1, shade_top - edge)),
                                 0.0f, 1.0f);
        Color pixel = mix(scale(color, 0.72f), color, 1.0f - depth);
        float blotch = value_noise(static_cast<float>(x) / 9.0f, static_cast<float>(y) / 6.0f,
                                   static_cast<std::uint32_t>(layer) * 7919u);
        pixel = mix(scale(pixel, 0.92f), pixel, blotch);
        canvas.set_pixel(x, y, pixel);
      }
    }
  }

  // Scaled back to a single tree while its look gets sorted out — the
  // clustering machinery stays in place underneath (kClusterCount) for when
  // more get added back.
  constexpr bool kBackgroundTreesEnabled = true;
  if (kBackgroundTreesEnabled) {
  float epoch = std::floor(now_seconds / 5400.0f);
  std::uint32_t epoch_seed = static_cast<std::uint32_t>(epoch) * 6151u;
  constexpr int kClusterCount = 1;
  for (int cluster = 0; cluster < kClusterCount; ++cluster) {
    std::uint32_t cluster_seed = epoch_seed + static_cast<std::uint32_t>(cluster) * 911u;
    float cluster_cx = noise_unit(cluster_seed + 5u) * static_cast<float>(canvas.width());
    float cluster_spread = 8.0f + noise_unit(cluster_seed + 41u) * 16.0f;
    int trees_here = 1;
    for (int slot = 0; slot < trees_here; ++slot) {
    std::uint32_t tree_id = cluster_seed * 131u + static_cast<std::uint32_t>(slot) * 977u + 3u;
    // A grand, mature oak — always fully grown, no sapling/aging/falling
    // cycle. It can still scorch if a fireball lands on it, for continuity
    // with the bug-kill spell, but it doesn't wither or topple on its own.
    float seed = noise_unit(tree_id + 3u);
    float offset = (noise_unit(tree_id + 29u) - 0.5f) * 2.0f * cluster_spread;
    int tree_x = std::clamp(static_cast<int>(cluster_cx + offset), 0, canvas.width() - 1);
    int trunk_bottom = 47 + static_cast<int>(noise_unit(tree_id + 11u) * 3.0f);
    int height = 21 + static_cast<int>(seed * 6.0f);
    int trunk_top = trunk_bottom - height;
    Color trunk = mix(scale(kBarkWarm, 0.3f), kBarkWarm, day);

    float fire_proximity =
        std::clamp(1.0f - std::abs(static_cast<float>(tree_x) - config.fire_hazard_x) / 20.0f, 0.0f, 1.0f);
    float burn = config.fire_hazard_intensity * fire_proximity;
    trunk = mix(trunk, kCharcoal, burn * 0.8f);

    // Thick trunk: three adjacent strokes instead of one thin line.
    draw::line(canvas, tree_x - 1, trunk_bottom, tree_x - 1, trunk_top, scale(trunk, 0.7f));
    draw::line(canvas, tree_x, trunk_bottom, tree_x, trunk_top, trunk);
    draw::line(canvas, tree_x + 1, trunk_bottom, tree_x + 1, trunk_top, scale(trunk, 0.85f));

    // A pair of sturdy limbs the leaf clusters hang off of.
    int limb_y = trunk_top + static_cast<int>(static_cast<float>(height) * 0.3f);
    int left_tip_x = tree_x - 7 - static_cast<int>(seed * 3.0f);
    int left_tip_y = limb_y - 3;
    int right_tip_x = tree_x + 6 + static_cast<int>(noise_unit(tree_id + 61u) * 3.0f);
    int right_tip_y = limb_y - 1;
    draw::line(canvas, tree_x, limb_y + 2, left_tip_x, left_tip_y, scale(trunk, 0.68f));
    draw::line(canvas, tree_x, limb_y, right_tip_x, right_tip_y, scale(trunk, 0.68f));

    Color canopy = mix(mix(scale(kCanopyMid, 0.3f), kCanopyMid, day), kCharcoal, burn);
    // Several overlapping leaf clusters along the crown and both limbs,
    // rather than one round blob on a stick.
    draw_tree_canopy(canvas, tree_x, trunk_top - 3, 6, 0.55f, canopy);
    draw_tree_canopy(canvas, tree_x - 4, trunk_top + 1, 5, 0.5f, canopy);
    draw_tree_canopy(canvas, tree_x + 4, trunk_top + 2, 5, 0.5f, canopy);
    draw_tree_canopy(canvas, left_tip_x, left_tip_y, 4, 0.5f, canopy);
    draw_tree_canopy(canvas, right_tip_x, right_tip_y, 4, 0.5f, canopy);

    // A couple of small grass tufts at the base break up the trunk's
    // otherwise dead-straight line into the ground.
    draw_grass_tuft(canvas, tree_x - 3, trunk_bottom + 1, 4, 0, seed, 0.0f, day);
    draw_grass_tuft(canvas, tree_x + 3, trunk_bottom + 1, 5, 0, noise_unit(tree_id + 89u), 0.0f, day);

    if (burn > 0.0f) {
      int spark_count = 2 + static_cast<int>(burn * 5.0f);
      for (int spark = 0; spark < spark_count; ++spark) {
        float spark_seed = noise_unit(tree_id + static_cast<std::uint32_t>(spark) * 73u + 503u);
        int sx = tree_x + static_cast<int>((spark_seed - 0.5f) * 16.0f);
        int sy = trunk_top - static_cast<int>(spark_seed * 8.0f) - static_cast<int>(burn * 4.0f);
        canvas.set_pixel(sx, sy, spark_seed > 0.55f ? kFireCore : kFireHot);
      }
    }
    }
  }
  }
}

void AtmosphereSystem::draw_stars(Canvas& canvas, float now_seconds, AtmosphereConfig const& config) const {
  float night = 1.0f - daylight_amount(config);
  if (night < 0.12f) {
    return;
  }
  for (std::uint32_t i = 0; i < 16; ++i) {
    int x = 2 + static_cast<int>(unit(i * 19u + 3u) * static_cast<float>(canvas.width() - 4));
    int y = 3 + static_cast<int>(unit(i * 23u + 11u) * 25.0f);
    if ((x > canvas.width() - 30 && y < 22) || (x < 48 && y > 10)) {
      continue;
    }
    float phase = unit(i * 31u + 7u) * 6.2831853f;
    float twinkle = 0.2f + 0.45f * (0.5f + 0.5f * std::sin(now_seconds * (0.55f + unit(i) * 0.9f) + phase));
    Color color = scale(mix(kStarDim, kStarBright, twinkle), std::clamp(night * 1.15f, 0.0f, 1.0f));
    canvas.set_pixel(x, y, color);
  }
}

void AtmosphereSystem::draw_clouds(Canvas& canvas, float now_seconds, AtmosphereConfig const& config) const {
  if (config.cloud_cover == CloudCover::Clear) {
    return;
  }
  bool dense = config.cloud_cover == CloudCover::Dense;
  // Keep clouds up in the sky, clear of the wizard's hat and the treeline.
  if (config.sky == SkyMode::Storm) {
    draw_cloud_stratum(canvas, now_seconds, config,
                       CloudStratum{dense ? 5 : 3, 2, 5, 18, 14, 6, 2.2f, 0.25f, 0xC10Du}, 0.25f);
    draw_cloud_stratum(canvas, now_seconds, config,
                       CloudStratum{dense ? 5 : 3, 7, 6, 22, 18, 7, 4.1f, 0.9f, 0x5A11u}, 0.85f);
    return;
  }
  draw_cloud_stratum(canvas, now_seconds, config,
                     CloudStratum{dense ? 4 : 2, 1, 4, 15, 12, 4, 0.75f, 0.15f, 0xC10Du}, 0.15f);
  draw_cloud_stratum(canvas, now_seconds, config,
                     CloudStratum{dense ? 5 : 2, 5, 6, 20, 16, 5, 1.65f, 0.7f, 0x5A11u}, 0.7f);
}

void AtmosphereSystem::draw_moon(Canvas& canvas, float now_seconds, AtmosphereConfig const& config) const {
  if (config.moon == MoonMode::None) {
    return;
  }
  int cx = canvas.width() - 16;
  int cy = 10;
  int radius = config.moon == MoonMode::Sun ? 5 : 6;
  Color base = kMoonBase;
  Color light = kMoonLight;
  if (config.moon == MoonMode::BloodMoon) {
    base = kBloodMoonBase;
    light = kBloodMoonLight;
  } else if (config.moon == MoonMode::Sun) {
    base = kSunBase;
    light = kSunLight;
  }

  for (int y = cy - radius - 1; y <= cy + radius + 1; ++y) {
    for (int x = cx - radius - 1; x <= cx + radius + 1; ++x) {
      float dx = static_cast<float>(x - cx);
      float dy = static_cast<float>(y - cy);
      float dist = std::sqrt(dx * dx + dy * dy);
      if (dist <= static_cast<float>(radius)) {
        float shade = 1.0f - dist / static_cast<float>(radius);
        Color color = mix(base, light, 0.25f + 0.55f * shade);
        if (((x * 5 + y * 9) & 23) == 0 && config.moon != MoonMode::Sun) {
          color = dim(color, 0.55f);
        }
        canvas.set_pixel(x, y, color);
      } else if (dist < static_cast<float>(radius) + 1.5f) {
        float pulse = 0.55f + 0.15f * std::sin(now_seconds * 0.7f);
        canvas.set_pixel(x, y, dim(light, config.moon == MoonMode::BloodMoon ? pulse : pulse * 0.45f));
      }
    }
  }
}

void AtmosphereSystem::draw_grass(Canvas& canvas, float now_seconds, AtmosphereConfig const& config) const {
  int base_y = canvas.height() - 1;
  float season = 0.5f + 0.5f * std::sin(now_seconds * 0.0175f);
  float overgrown = 0.5f + 0.5f * std::sin(now_seconds * 0.0135f + 1.7f);
  float dry = std::clamp((0.34f - season) / 0.34f, 0.0f, 1.0f);
  float meadow_breath = 0.9f + 0.1f * std::sin(now_seconds * 0.1f);
  // Blend the meadow's top edge toward the horizon's own tone so the ground
  // fades into the treeline instead of butting against it as a hard seam.
  float day = daylight_amount(config);
  Color horizon_tone = mix(mix(kCanopyDark, kCanopyDarkDay, day), kGroundShadow, 0.4f);
  // Loop bound gives enough headroom for the tallest possible uneven_top
  // (meadow_height up to 18, plus the wave/roll terms) — too tight a bound
  // here clips the rolling top edge into a flat line.
  for (int y = base_y - 24; y <= base_y; ++y) {
    for (int x = 0; x < canvas.width(); ++x) {
      float noise = unit(static_cast<std::uint32_t>(x * 31 + y * 97));
      float patch = wave(now_seconds, x / 8, 0xA51u, 0.07f);
      int meadow_height = 11 + static_cast<int>(overgrown * 7.0f) - static_cast<int>(dry * 3.0f);
      // Same static rolling-hill treatment as the canopy's horizon_edge: a
      // few summed low-frequency sines instead of noisy per-column jag, so
      // the meadow's top edge reads as a gentle slope, not scattered jitter.
      float fx = static_cast<float>(x);
      float roll = std::sin(fx * 0.05f + 0.6f) * 2.0f + std::sin(fx * 0.023f + 2.1f) * 1.4f;
      int uneven_top = base_y - meadow_height + static_cast<int>(wave(now_seconds, x / 6, 0xDADu, 0.04f) * 2.0f) +
                       static_cast<int>(roll);
      // Strictly masked to the meadow's own rolling edge — nothing from this
      // layer draws above uneven_top, full stop, so no ground speckle can
      // float up into the horizon above it.
      if (y < uneven_top) {
        continue;
      }
      float depth = static_cast<float>(y - uneven_top) / static_cast<float>(base_y - uneven_top + 1);
      depth = std::clamp(depth, 0.0f, 1.0f);
      Color color = seasonal_grass(mix(kGroundShadow, kGrassDark, depth * 0.55f + patch * 0.12f), dry, day);
      float edge_fade = std::clamp(static_cast<float>(y - uneven_top) / 3.0f, 0.0f, 1.0f);
      color = mix(horizon_tone, color, edge_fade);
      if (noise > 0.83f) {
        color = mix(color, noise > 0.92f ? kSoilWarm : kSoil, noise > 0.92f ? 0.32f : 0.5f);
      } else if (noise > 0.7f - patch * 0.06f) {
        color = mix(color, seasonal_grass(kGrassMid, dry, day), 0.25f + patch * 0.16f);
      }
      canvas.set_pixel(x, y, color);
    }
  }

  for (int x = 2; x < canvas.width(); x += 7) {
    float seed = unit(static_cast<std::uint32_t>(x * 41 + 17));
    float growth = std::clamp(0.18f + wave(now_seconds, x, 0xBEEFu, 0.08f) * (0.75f + overgrown * 0.55f) -
                                  dry * 0.35f,
                              0.0f,
                              1.0f);
    if (seed < 0.5f - growth * 0.2f) {
      continue;
    }
    int y = base_y - 10 + static_cast<int>(unit(static_cast<std::uint32_t>(x * 13 + 9)) * 9.0f);
    int height = 3 + static_cast<int>((seed * 7.0f + 3.0f) * growth * meadow_breath);
    int bend = static_cast<int>(std::round(std::sin(now_seconds * 0.4f + static_cast<float>(x) * 0.14f) *
                                           (0.4f + static_cast<float>(config.wind) / 320.0f)));
    Color color = seasonal_grass(seed > 0.86f ? kGrassTip : seed > 0.56f ? kGrassMid : kGrassDark, dry, day);
    draw_meadow_stalk(canvas, x, y, height, bend + static_cast<int>((x % 3) - 1), color);
  }

  for (int x = 4; x < canvas.width(); x += 15) {
    float seed = unit(static_cast<std::uint32_t>(x * 13 + 5));
    float growth = std::clamp(0.12f + wave(now_seconds, x, 0xCAFEu, 0.05f) * (0.9f + overgrown * 0.35f) -
                                  dry * 0.45f,
                              0.0f,
                              1.0f);
    if (seed < 0.44f - growth * 0.2f) {
      continue;
    }
    int jitter = static_cast<int>(unit(static_cast<std::uint32_t>(x * 29 + 3)) * 5.0f) - 2;
    int tuft_x = x + jitter;
    if (tuft_x > 22 && tuft_x < 62) {
      continue;
    }
    int tuft_base = base_y - static_cast<int>(unit(static_cast<std::uint32_t>(x * 7 + 11)) * 5.0f);
    int height = 4 + static_cast<int>((seed * 7.0f + 4.0f) * growth);
    int sway = static_cast<int>(std::round(std::sin(now_seconds * 0.5f + static_cast<float>(x) * 0.19f) *
                                           static_cast<float>(config.wind) / 520.0f));
    draw_grass_tuft(canvas, tuft_x, tuft_base, height, sway, seed, dry, day);
  }

  int flower_epoch = static_cast<int>(std::floor(now_seconds / 1800.0f));
  for (int slot = 0; slot < 8; ++slot) {
    std::uint32_t flower_id = static_cast<std::uint32_t>(slot * 181 + flower_epoch * 911);
    float seed = unit(flower_id + 23u);
    float phase = cycle01(now_seconds, 900.0f + seed * 900.0f, flower_id + 31u);
    float bloom = phase < 0.22f ? phase / 0.22f
                : phase < 0.72f ? 1.0f
                : phase < 0.9f ? 1.0f - (phase - 0.72f) / 0.18f
                                : 0.0f;
    if (seed < 0.34f || (bloom < 0.16f && phase < 0.9f) || dry > 0.78f) {
      continue;
    }
    int x = 7 + static_cast<int>(unit(flower_id + 61u) * 114.0f);
    if (x > 22 && x < 62) {
      x += 42;
    }
    int stem_base = base_y - 7 + static_cast<int>(unit(flower_id + 17u) * 6.0f);
    // Grows from a short sprout up to full height as bloom ramps in, rather
    // than jumping straight to a near-full-height bare stem the instant a
    // flower spawns (which read as a stem with no flower on it yet).
    int stem_h = 2 + static_cast<int>((seed * 6.0f + 3.0f) * (0.15f + bloom * 0.85f));
    int bend = seed > 0.78f ? 1 : -1;
    draw_meadow_stalk(canvas, x, stem_base, stem_h, bend, seasonal_grass(kGrassMid, dry, day));
    int flower_x = x + bend;
    int flower_y = stem_base - stem_h + (phase > 0.72f ? static_cast<int>((phase - 0.72f) * 8.0f) : 0);
    if (phase >= 0.9f) {
      canvas.set_pixel(flower_x, stem_base - 1, seed > 0.82f ? kFlowerGold : kFlowerWhite);
    } else if (bloom > 0.22f) {
      if (seed > 0.86f) {
        draw_sunflower(canvas, flower_x, flower_y);
      } else if (seed > 0.68f) {
        draw_dandelion(canvas, flower_x, flower_y, phase > 0.68f);
      } else {
        Color petal = seed > 0.52f ? kFlowerRose : seed > 0.42f ? kFlowerBlue : kFlowerWhite;
        draw_flower(canvas, flower_x, flower_y, petal, Color{198, 166, 48});
      }
    }
  }
}

void AtmosphereSystem::draw_motes(Canvas& canvas, float now_seconds, AtmosphereConfig const&) const {
  for (std::uint32_t i = 0; i < 9; ++i) {
    float lane = unit(i * 17u + 101u);
    float speed = 2.5f + unit(i * 43u) * 5.0f;
    float x = std::fmod(unit(i * 29u + 1u) * 128.0f + now_seconds * speed, 128.0f);
    float y = 19.0f + lane * 31.0f + std::sin(now_seconds * 1.4f + lane * 7.0f) * 2.0f;
    float pulse = 0.45f + 0.55f * (0.5f + 0.5f * std::sin(now_seconds * 3.0f + lane * 13.0f));
    Color color = mix(Color{58, 94, 120}, Color{142, 210, 190}, pulse);
    canvas.set_pixel(static_cast<int>(x), static_cast<int>(y), color);
    if (pulse > 0.8f) {
      canvas.set_pixel(static_cast<int>(x) - 1, static_cast<int>(y), dim(color, 0.45f));
    }
  }
}

void AtmosphereSystem::draw_fog(Canvas& canvas, float now_seconds, AtmosphereConfig const& config) const {
  int wisps = config.fog == FogDensity::Dense ? 7 : 4;
  Color fog = config.fog == FogDensity::Dense ? Color{84, 96, 100} : Color{54, 66, 72};
  // Fog drifts in and out rather than sitting at a constant wall of haze.
  float breathe = 0.6f + 0.4f * std::sin(now_seconds * 0.05f);
  float strength = (config.fog == FogDensity::Dense ? 0.17f : 0.1f) * breathe;
  for (int wisp = 0; wisp < wisps; ++wisp) {
    std::uint32_t id = static_cast<std::uint32_t>(wisp * 127 + 503);
    float seed = unit(id);
    int y = 22 + static_cast<int>(unit(id + 17u) * 35.0f) +
            static_cast<int>(std::sin(now_seconds * (0.08f + seed * 0.04f) + seed * 9.0f) * 3.0f);
    int thickness = 1;
    int len = 9 + static_cast<int>(unit(id + 41u) * (config.fog == FogDensity::Dense ? 23.0f : 16.0f));
    int base_x = static_cast<int>(std::fmod(seed * 164.0f + now_seconds * (0.8f + seed * 1.5f), 176.0f)) - 32;
    for (int dx = 0; dx < len; ++dx) {
      float t = static_cast<float>(dx) / static_cast<float>(std::max(1, len - 1));
      float taper = std::sin(t * 3.1415926f);
      if (taper <= 0.05f) {
        continue;
      }
      int y_offset = static_cast<int>(std::sin((t + seed) * 6.2831853f) * 1.4f);
      for (int yy = 0; yy < thickness; ++yy) {
        int x = base_x + dx;
        int row = y + y_offset + yy;
        float texture = unit(static_cast<std::uint32_t>((x + wisp * 67) * 17 + row * 71));
        if (texture < 0.18f) {
          continue;
        }
        float alpha = strength * taper * (0.45f + texture * 0.38f);
        canvas.set_pixel(x, row, mix(canvas.get_pixel(x, row), fog, alpha));
      }
    }
  }
}

void AtmosphereSystem::draw_weather(Canvas& canvas, float now_seconds, AtmosphereConfig const& config) const {
  if (config.weather == WeatherMode::None || config.weather == WeatherMode::PreRain) {
    if (config.weather == WeatherMode::PreRain) {
      for (int i = 0; i < 10; ++i) {
        int x = static_cast<int>(unit(static_cast<std::uint32_t>(i * 53 + 17)) * 128.0f);
        int y = 32 + static_cast<int>(unit(static_cast<std::uint32_t>(i * 19 + 11)) * 24.0f);
        canvas.set_pixel(x, y, Color{42, 58, 68});
      }
    }
    return;
  }
  bool rain = config.weather == WeatherMode::Rain || config.weather == WeatherMode::Storm ||
              config.weather == WeatherMode::Thunderstorm;
  int count = config.weather == WeatherMode::Thunderstorm ? 64
            : config.weather == WeatherMode::Storm ? 52
            : config.weather == WeatherMode::Rain ? 34
                                                  : 24;
  for (int i = 0; i < count; ++i) {
    float seed = unit(static_cast<std::uint32_t>(i * 37 + 9));
    float fall = now_seconds * (rain ? (36.0f + static_cast<float>(config.wind) * 0.18f) : 10.0f + seed * 9.0f);
    int x = static_cast<int>(unit(static_cast<std::uint32_t>(i * 53 + 17)) * 128.0f);
    int y = static_cast<int>(std::fmod(seed * 64.0f + fall, 64.0f));
    if (rain) {
      int slant = config.weather == WeatherMode::Rain ? 1 : 2;
      Color drop = config.weather == WeatherMode::Rain ? Color{74, 102, 132} : Color{92, 118, 142};
      canvas.set_pixel(x, y, drop);
      canvas.set_pixel(x - slant, y - 1, dim(drop, 0.65f));
      if (config.weather == WeatherMode::Thunderstorm && (i % 5) == 0) {
        canvas.set_pixel(x - slant * 2, y - 2, dim(drop, 0.45f));
      }
    } else {
      float drift = std::sin(now_seconds * 0.9f + seed * 9.0f) * 4.0f;
      Color flake = config.weather == WeatherMode::Snow ? Color{165, 178, 186} : Color{96, 90, 86};
      canvas.set_pixel(static_cast<int>(static_cast<float>(x) + drift), y, flake);
    }
  }
  if (config.weather == WeatherMode::Thunderstorm) {
    float flash = std::sin(now_seconds * 5.7f);
    if (flash > 0.92f) {
      Color bolt{198, 214, 225};
      int x = 88 + static_cast<int>(unit(static_cast<std::uint32_t>(now_seconds * 17.0f)) * 26.0f);
      draw::line(canvas, x, 5, x - 4, 16, bolt);
      draw::line(canvas, x - 4, 16, x + 1, 25, dim(bolt, 0.82f));
      draw::line(canvas, x + 1, 25, x - 3, 34, dim(bolt, 0.65f));
    }
  }
}

}  // namespace magicpanel
