#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "magicpanel/canvas.h"

namespace magicpanel {

class Rng {
 public:
  explicit Rng(std::uint32_t seed = 0xC0FFEEu) : state_(seed) {}

  float unit() {
    state_ = state_ * 1664525u + 1013904223u;
    return static_cast<float>((state_ >> 8) & 0x00FFFFFFu) / 16777215.0f;
  }

  float range(float lo, float hi) { return lo + (hi - lo) * unit(); }

 private:
  std::uint32_t state_;
};

struct Particle {
  float x = 0.0f;
  float y = 0.0f;
  float vx = 0.0f;
  float vy = 0.0f;
  float ttl = 0.0f;
  float max_ttl = 0.0f;
  float gravity = 0.0f;
  float drag = 0.0f;
  Color color = kBlack;
};

template <std::size_t MaxParticles>
class ParticleSystem {
 public:
  void clear() {
    count_ = 0;
  }

  bool emit(Particle particle) {
    if (count_ >= particles_.size()) {
      return false;
    }
    particles_[count_++] = particle;
    return true;
  }

  void burst(float x,
             float y,
             Color color,
             int count,
             float speed,
             float ttl,
             Rng& rng,
             float gravity = 0.0f,
             float drag = 0.0f) {
    constexpr float tau = 6.28318530718f;
    for (int i = 0; i < count; ++i) {
      float angle = rng.range(0.0f, tau);
      float particle_speed = rng.range(speed * 0.3f, speed);
      emit(Particle{x,
                    y,
                    std::cos(angle) * particle_speed,
                    std::sin(angle) * particle_speed,
                    ttl,
                    ttl,
                    gravity,
                    drag,
                    color});
    }
  }

  void tick(float dt) {
    std::size_t write = 0;
    for (std::size_t read = 0; read < count_; ++read) {
      Particle p = particles_[read];
      if (p.drag > 0.0f) {
        float decay = std::fmax(0.0f, 1.0f - p.drag * dt);
        p.vx *= decay;
        p.vy *= decay;
      }
      p.vy += p.gravity * dt;
      p.x += p.vx * dt;
      p.y += p.vy * dt;
      p.ttl -= dt;
      if (p.ttl > 0.0f) {
        particles_[write++] = p;
      }
    }
    count_ = write;
  }

  void draw(Canvas& canvas, Color background) const {
    for (std::size_t i = 0; i < count_; ++i) {
      auto const& p = particles_[i];
      float fade = p.max_ttl <= 0.0f ? 0.0f : std::fmax(0.0f, std::fmin(1.0f, p.ttl / p.max_ttl));
      canvas.set_pixel(static_cast<int>(p.x), static_cast<int>(p.y), blend(background, p.color, fade));
    }
  }

  std::size_t size() const { return count_; }

 private:
  std::array<Particle, MaxParticles> particles_{};
  std::size_t count_ = 0;
};

struct Light {
  float x = 0.0f;
  float y = 0.0f;
  float radius = 1.0f;
  float strength = 0.0f;
  Color color = Color{255, 255, 255};
};

template <std::size_t MaxLights>
class LightSystem {
 public:
  void clear() { count_ = 0; }

  bool add(Light light) {
    if (count_ >= lights_.size()) {
      return false;
    }
    lights_[count_++] = light;
    return true;
  }

  Color shade(Color base, float x, float y) const {
    Color result = base;
    for (std::size_t i = 0; i < count_; ++i) {
      auto const& light = lights_[i];
      float dist = std::hypot(x - light.x, y - light.y);
      float falloff = std::fmax(0.0f, 1.0f - dist / light.radius);
      result = blend(result, light.color, light.strength * falloff * falloff);
    }
    return result;
  }

  void apply(Canvas& canvas) const {
    for (int y = 0; y < canvas.height(); ++y) {
      for (int x = 0; x < canvas.width(); ++x) {
        canvas.set_pixel(x, y, shade(canvas.get_pixel(x, y), static_cast<float>(x),
                                     static_cast<float>(y)));
      }
    }
  }

 private:
  std::array<Light, MaxLights> lights_{};
  std::size_t count_ = 0;
};

}  // namespace magicpanel
