#include "magicpanel/engine.h"

namespace magicpanel {

void run_engine(Canvas& canvas,
                SceneManager& scenes,
                LivenessTracker& liveness,
                Platform& platform,
                EngineConfig config) {
  float frame_seconds = 1.0f / config.fps;
  float last = platform.now_seconds();

  while (!canvas.poll_quit()) {
    float frame_start = platform.now_seconds();
    float dt = frame_start - last;
    last = frame_start;

    Event event;
    while (platform.poll_event(event)) {
      liveness.mark_seen(frame_start);
      scenes.handle_event(event);
    }

    scenes.render(canvas, dt, frame_start);
    canvas.present();

    float elapsed = platform.now_seconds() - frame_start;
    if (elapsed < frame_seconds) {
      platform.sleep_seconds(frame_seconds - elapsed);
    }
  }
}

}  // namespace magicpanel
