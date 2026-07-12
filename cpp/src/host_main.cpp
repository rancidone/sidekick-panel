#include <fcntl.h>
#include <SDL.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <string>
#include <thread>
#include <vector>

#include "magicpanel/app.h"
#include "magicpanel/engine.h"

namespace {

constexpr char kDefaultSocketPath[] = "/tmp/magicpanel.sock";

class SdlCanvas final : public magicpanel::FrameBufferCanvas {
 public:
  explicit SdlCanvas(int scale = 8) : scale_(scale) {
    SDL_SetMainReady();
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "metal");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
      std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
      ok_ = false;
      return;
    }
    window_ = SDL_CreateWindow("Magic Panel C++ Emulator",
                               SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED,
                               width() * scale_,
                               height() * scale_,
                               SDL_WINDOW_SHOWN);
    if (window_ == nullptr) {
      std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
      ok_ = false;
      return;
    }
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer_ == nullptr) {
      std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
      ok_ = false;
      return;
    }
    SDL_RenderSetLogicalSize(renderer_, width() * scale_, height() * scale_);
    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                 width(), height());
    if (texture_ == nullptr) {
      std::fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
      ok_ = false;
      return;
    }
  }

  ~SdlCanvas() override {
    if (texture_ != nullptr) {
      SDL_DestroyTexture(texture_);
    }
    if (renderer_ != nullptr) {
      SDL_DestroyRenderer(renderer_);
    }
    if (window_ != nullptr) {
      SDL_DestroyWindow(window_);
    }
    if (SDL_WasInit(SDL_INIT_VIDEO) != 0) {
      SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
  }

  bool ok() const { return ok_; }

  bool poll_quit() override {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
      if (event.type == SDL_QUIT) {
        return true;
      }
      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
        return true;
      }
    }
    return false;
  }

  void present() override {
    if (!ok_) {
      return;
    }
    void* pixels = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(texture_, nullptr, &pixels, &pitch) != 0) {
      std::fprintf(stderr, "SDL_LockTexture failed: %s\n", SDL_GetError());
      return;
    }

    for (int y = 0; y < height(); ++y) {
      auto* row = reinterpret_cast<std::uint32_t*>(static_cast<std::uint8_t*>(pixels) + y * pitch);
      for (int x = 0; x < width(); ++x) {
        magicpanel::Color color = panel_color(pixel(x, y));
        row[x] = 0xFF000000u | (static_cast<std::uint32_t>(color.r) << 16) |
                 (static_cast<std::uint32_t>(color.g) << 8) | color.b;
      }
    }

    SDL_UnlockTexture(texture_);
    SDL_SetRenderDrawColor(renderer_, 6, 6, 10, 255);
    SDL_RenderClear(renderer_);
    SDL_Rect dest{0, 0, width() * scale_, height() * scale_};
    if (SDL_RenderCopy(renderer_, texture_, nullptr, &dest) != 0) {
      std::fprintf(stderr, "SDL_RenderCopy failed: %s\n", SDL_GetError());
      return;
    }
    SDL_RenderPresent(renderer_);
  }

 private:
  static magicpanel::Color panel_color(magicpanel::Color color) {
    auto channel = [](std::uint8_t value) -> std::uint8_t {
      float normalized = static_cast<float>(value) / 255.0f;
      float corrected = std::pow(normalized, 1.15f);
      int out = static_cast<int>(std::round(corrected * 255.0f));
      return static_cast<std::uint8_t>(out < 0 ? 0 : out > 255 ? 255 : out);
    };
    return magicpanel::Color{channel(color.r), channel(color.g), channel(color.b)};
  }

  int scale_;
  bool ok_ = true;
  SDL_Window* window_ = nullptr;
  SDL_Renderer* renderer_ = nullptr;
  SDL_Texture* texture_ = nullptr;
};

class TerminalCanvas final : public magicpanel::FrameBufferCanvas {
 public:
  void present() override {
    ++frames_;
    if (frames_ % 3 != 0) {
      return;
    }
    std::printf("\033[7;1H");
    std::printf("\033[38;2;90;90;100m+");
    for (int x = 0; x < width(); ++x) {
      std::putchar('-');
    }
    std::puts("+\033[0m");
    for (int y = 0; y < height(); y += 2) {
      std::printf("\033[38;2;90;90;100m|\033[0m");
      for (int x = 0; x < width(); ++x) {
        auto top = panel_color(pixel(x, y));
        auto bottom = panel_color(pixel(x, y + 1));
        std::printf("\033[38;2;%u;%u;%um\033[48;2;%u;%u;%um▀",
                    top.r, top.g, top.b, bottom.r, bottom.g, bottom.b);
      }
      std::puts("\033[0m\033[38;2;90;90;100m|\033[0m");
    }
    std::printf("\033[38;2;90;90;100m+");
    for (int x = 0; x < width(); ++x) {
      std::putchar('-');
    }
    std::puts("+\033[0m");
    std::fflush(stdout);
  }

 private:
  static magicpanel::Color panel_color(magicpanel::Color color) {
    // Cheap approximation of LED-panel output: apply a little gamma and keep
    // very dark pixels visible enough for terminals with aggressive contrast.
    auto channel = [](std::uint8_t value) -> std::uint8_t {
      float normalized = static_cast<float>(value) / 255.0f;
      float corrected = std::pow(normalized, 1.25f);
      int out = static_cast<int>(std::round(corrected * 255.0f));
      return static_cast<std::uint8_t>(out < 0 ? 0 : out > 255 ? 255 : out);
    };
    return magicpanel::Color{channel(color.r), channel(color.g), channel(color.b)};
  }

  int frames_ = 0;
};

class HostPlatform final : public magicpanel::Platform {
 public:
  explicit HostPlatform(std::string path) : path_(std::move(path)) {
    ::unlink(path_.c_str());
    fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd_ < 0) {
      std::perror("socket");
      return;
    }

    sockaddr_un addr {};
    addr.sun_family = AF_UNIX;
    std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path_.c_str());
    if (::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
      std::perror("bind");
      ::close(fd_);
      fd_ = -1;
      return;
    }
    if (::listen(fd_, 4) < 0) {
      std::perror("listen");
      ::close(fd_);
      fd_ = -1;
      return;
    }
    ::fcntl(fd_, F_SETFL, ::fcntl(fd_, F_GETFL, 0) | O_NONBLOCK);
    std::printf("Listening for events on %s\n", path_.c_str());
  }

  bool ok() const { return fd_ >= 0; }

  ~HostPlatform() override {
    if (fd_ >= 0) {
      ::close(fd_);
    }
    for (int client : clients_) {
      ::close(client);
    }
    ::unlink(path_.c_str());
  }

  bool poll_event(magicpanel::Event& event) override {
    accept_pending();
    char buf[256];
    for (auto it = clients_.begin(); it != clients_.end();) {
      ssize_t n = ::read(*it, buf, sizeof(buf) - 1);
      if (n == 0) {
        ::close(*it);
        it = clients_.erase(it);
        continue;
      }
      if (n < 0) {
        ++it;
        continue;
      }
      buf[n] = '\0';
      pending_ += buf;
      ++it;
    }
    auto newline = pending_.find('\n');
    if (newline == std::string::npos) {
      return false;
    }
    std::string line = pending_.substr(0, newline);
    pending_.erase(0, newline + 1);
    auto decoded = magicpanel::decode_event_line(line);
    if (!decoded) {
      return false;
    }
    event = *decoded;
    return true;
  }

  float now_seconds() override {
    // Wall-clock based (not process-start-relative): every scene animation —
    // tree growth, cloud drift, etc. — is a function of this value, so a
    // steady_clock-since-launch origin meant every hot-reload restart reset
    // all of it back to the exact same starting phase. Wrapped to keep the
    // magnitude small enough for float32 to still resolve sub-frame deltas.
    using clock = std::chrono::system_clock;
    double epoch_seconds = std::chrono::duration<double>(clock::now().time_since_epoch()).count();
    constexpr double kWrapSeconds = 43200.0;  // 12h; float32 stays sub-ms precise at this scale
    return static_cast<float>(std::fmod(epoch_seconds, kWrapSeconds));
  }

  void sleep_seconds(float seconds) override {
    std::this_thread::sleep_for(std::chrono::duration<float>(seconds));
  }

 private:
  void accept_pending() {
    if (fd_ < 0) {
      return;
    }
    while (true) {
      int client = ::accept(fd_, nullptr, nullptr);
      if (client < 0) {
        return;
      }
      ::fcntl(client, F_SETFL, ::fcntl(client, F_GETFL, 0) | O_NONBLOCK);
      clients_.push_back(client);
    }
  }

  std::string path_;
  int fd_ = -1;
  std::vector<int> clients_;
  std::string pending_;
};

}  // namespace

int main(int argc, char** argv) {
  bool terminal = false;
  std::string socket_path = kDefaultSocketPath;
  int scale = 8;
  for (int i = 1; i < argc; ++i) {
    if (std::string_view(argv[i]) == "--terminal") {
      terminal = true;
    } else if (std::string_view(argv[i]) == "--socket" && i + 1 < argc) {
      socket_path = argv[++i];
    } else if (std::string_view(argv[i]) == "--scale" && i + 1 < argc) {
      scale = std::max(1, std::atoi(argv[++i]));
    }
  }

  setvbuf(stdout, nullptr, _IONBF, 0);
  std::puts("Magic Panel C++ host");
  std::puts(terminal ? "Terminal truecolor emulator." : "SDL2 pixel emulator.");
  std::printf("Event socket: %s\n", socket_path.c_str());
  std::puts("Python CLI examples:");
  std::puts("  magicpanel send tests_passed");
  std::puts("  magicpanel send bug_squashed");
  std::puts("  magicpanel scene arcane_tree");
  std::puts("Host options: --scale N, --terminal, --socket PATH");
  std::puts(terminal ? "Press Ctrl-C to stop." : "Close the window, press Escape, or press Ctrl-C to stop.");

  magicpanel::MagicPanelApp app("/tmp/magicpanel-cpp-state.json");
  HostPlatform platform(socket_path);
  if (!platform.ok()) {
    return 1;
  }

  if (terminal) {
    std::printf("\033[2J\033[H");
    TerminalCanvas canvas;
    magicpanel::run_engine(canvas, app.scenes(), app.liveness(), platform);
  } else {
    SdlCanvas canvas(scale);
    if (!canvas.ok()) {
      return 1;
    }
    magicpanel::run_engine(canvas, app.scenes(), app.liveness(), platform);
  }
}
