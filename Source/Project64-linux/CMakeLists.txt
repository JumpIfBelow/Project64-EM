cmake_minimum_required(VERSION 3.14)

find_package(OpenGL REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets)

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
  Qt6::Widgets
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
