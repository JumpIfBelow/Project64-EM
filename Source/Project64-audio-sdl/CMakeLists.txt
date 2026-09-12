find_package(SDL2 CONFIG REQUIRED)

add_library(Project64-audio-sdl SHARED AudioPlugin.cpp)
target_link_libraries(Project64-audio-sdl PRIVATE SDL2::SDL2)
target_include_directories(Project64-audio-sdl PRIVATE "${CMAKE_SOURCE_DIR}/Source")
set_target_properties(Project64-audio-sdl PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${PJ64_DIRECTORY}/Plugin"
  OUTPUT_NAME "Project64-audio-sdl"
)
