cmake_minimum_required(VERSION 3.22)
project(SimpleFontRendering
  DESCRIPTION   "A simple bitmap font rendering program"
  HOMEPAGE_URL  "https://github.com/cedmundo/SimpleFontRendering"
  VERSION       "1.0.0"
  LANGUAGES     C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

find_package(glfw3 REQUIRED)
add_subdirectory(vendor)

add_executable(SimpleFontRendering)
target_sources(SimpleFontRendering PRIVATE main.c)
target_link_libraries(SimpleFontRendering PRIVATE glfw glad)
