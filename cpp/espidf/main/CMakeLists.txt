set(MAGICPANEL_CORE
  ../../src/app.cpp
  ../../src/canvas.cpp
  ../../src/color.cpp
  ../../src/drawing.cpp
  ../../src/engine.cpp
  ../../src/event.cpp
  ../../src/liveness.cpp
  ../../src/reactions.cpp
  ../../src/scene.cpp
  ../../src/scenes.cpp
  ../../src/sprite.cpp
  ../../src/state_store.cpp
)

idf_component_register(
  SRCS "main.cpp" ${MAGICPANEL_CORE}
  INCLUDE_DIRS "../../include" "../../generated"
)

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)
target_compile_options(${COMPONENT_LIB} PRIVATE -fno-exceptions -fno-rtti)
