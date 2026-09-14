find_package(OpenGL REQUIRED)
find_package(SDL2 CONFIG REQUIRED)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Version.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/Version.h")

file(GLOB_RECURSE VIDEO_SOURCES CONFIGURE_DEPENDS "*.c" "*.cpp" "*.h")
list(FILTER VIDEO_SOURCES EXCLUDE REGEX "/Renderer/OGLES")
list(FILTER VIDEO_SOURCES EXCLUDE REGEX "/Renderer/OGLESwrappers")
list(FILTER VIDEO_SOURCES EXCLUDE REGEX "/Android\\.cpp$")

file(GLOB PNG_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/Source/3rdParty/png/png*.c")
list(FILTER PNG_SOURCES EXCLUDE REGEX "pngtest\\.c$")

add_library(Project64-gfx SHARED ${VIDEO_SOURCES} ${PNG_SOURCES})
target_compile_definitions(Project64-gfx PRIVATE PNG_CONFIGURE_LIBPNG PJ64_SDL_VIDEO=1)
target_compile_options(Project64-gfx PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-include${CMAKE_CURRENT_SOURCE_DIR}/LinuxPch.h>")
target_include_directories(Project64-gfx PRIVATE
  "${CMAKE_SOURCE_DIR}/Source/3rdParty"
  "${CMAKE_SOURCE_DIR}/Source"
  "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(Project64-gfx PRIVATE Common Settings ZLIB::ZLIB OpenGL::GL SDL2::SDL2 ${CMAKE_DL_LIBS} m)
set_target_properties(Project64-gfx PROPERTIES
  C_VISIBILITY_PRESET hidden
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
  LIBRARY_OUTPUT_DIRECTORY "${PJ64_DIRECTORY}/Plugin"
  OUTPUT_NAME "Project64-gfx"
)
