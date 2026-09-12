cmake_minimum_required(VERSION 3.14)

find_package(OpenGL REQUIRED)
find_package(SDL2 CONFIG REQUIRED)

include(FetchContent)
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()
FetchContent_Declare(
  imgui
  URL "https://github.com/ocornut/imgui/archive/refs/tags/v1.92.9.tar.gz"
  URL_HASH SHA256=af97ed649182c39314320514a672b82008ab462b9293fe23d37b30bfa5d05519
)
FetchContent_MakeAvailable(imgui)

install(FILES "${imgui_SOURCE_DIR}/LICENSE.txt"
  DESTINATION licenses
  RENAME DearImGui-LICENSE.txt
)

add_library(Project64-linux-ui STATIC
  "${imgui_SOURCE_DIR}/imgui.cpp"
  "${imgui_SOURCE_DIR}/imgui_draw.cpp"
  "${imgui_SOURCE_DIR}/imgui_tables.cpp"
  "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
  "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
  "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp"
)
target_include_directories(Project64-linux-ui PUBLIC
  "${imgui_SOURCE_DIR}"
  "${imgui_SOURCE_DIR}/backends"
)
target_link_libraries(Project64-linux-ui PRIVATE SDL2::SDL2 OpenGL::GL)

add_executable(project64-em
  main.cpp
  Launcher.cpp
  LinuxConfig.cpp
  ../Project64-input-sdl/InputConfig.cpp
)
target_include_directories(project64-em PRIVATE
  "${CMAKE_SOURCE_DIR}/Source/3rdParty"
  "${CMAKE_SOURCE_DIR}/Source"
)
target_link_libraries(project64-em PRIVATE
  Project64-core
  Common
  Settings
  7zip
  MiniZip::MiniZip
  ZLIB::ZLIB
  SDL2::SDL2
  OpenGL::GL
  Project64-linux-ui
  ${CMAKE_DL_LIBS}
)
set_target_properties(project64-em PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${PJ64_DIRECTORY}"
)

add_dependencies(project64-em
  Project64-rsp-hle
  Project64-input-sdl
  Project64-audio-sdl
  Project64-gfx
)

if(BUILD_TESTING)
  add_executable(Project64-linux-input-config-test
    ../Project64-input-sdl/InputConfigTest.cpp
    ../Project64-input-sdl/InputConfig.cpp
  )
  target_include_directories(Project64-linux-input-config-test PRIVATE "${CMAKE_SOURCE_DIR}/Source")
  target_link_libraries(Project64-linux-input-config-test PRIVATE SDL2::SDL2)
  add_test(NAME Project64-linux-input-config COMMAND Project64-linux-input-config-test)
endif()

add_custom_command(TARGET project64-em POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy_directory
    "${CMAKE_SOURCE_DIR}/Config" "${PJ64_DIRECTORY}/Config"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory
    "${CMAKE_SOURCE_DIR}/Lang" "${PJ64_DIRECTORY}/Lang"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory
    "${CMAKE_SOURCE_DIR}/Scripts" "${PJ64_DIRECTORY}/Scripts"
)
