find_package(SDL2 CONFIG REQUIRED)

add_library(Project64-input-sdl SHARED InputPlugin.cpp)
target_link_libraries(Project64-input-sdl PRIVATE SDL2::SDL2)
target_include_directories(Project64-input-sdl PRIVATE "${CMAKE_SOURCE_DIR}/Source")
set_target_properties(Project64-input-sdl PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${PJ64_DIRECTORY}/Plugin"
  OUTPUT_NAME "Project64-input-sdl"
)
