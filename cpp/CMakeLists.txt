cmake_minimum_required(VERSION 3.20)
project(magicpanel_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

add_library(magicpanel_core
  src/app.cpp
  src/canvas.cpp
  src/color.cpp
  src/drawing.cpp
  src/engine.cpp
  src/event.cpp
  src/liveness.cpp
  src/reactions.cpp
  src/scene.cpp
  src/scenes.cpp
  src/sprite.cpp
  src/state_store.cpp
)
target_include_directories(magicpanel_core PUBLIC include generated)
target_compile_options(magicpanel_core PRIVATE -Wall -Wextra -Wpedantic -fno-exceptions -fno-rtti)

add_executable(magicpanel_host src/host_main.cpp)
target_link_libraries(magicpanel_host PRIVATE magicpanel_core PkgConfig::SDL2)
target_compile_options(magicpanel_host PRIVATE -Wall -Wextra -Wpedantic -fno-exceptions -fno-rtti)

add_executable(magicpanel_cpp_tests tests/core_tests.cpp)
target_link_libraries(magicpanel_cpp_tests PRIVATE magicpanel_core)
target_compile_options(magicpanel_cpp_tests PRIVATE -Wall -Wextra -Wpedantic -fno-exceptions -fno-rtti)
enable_testing()
add_test(NAME magicpanel_cpp_tests COMMAND magicpanel_cpp_tests)
