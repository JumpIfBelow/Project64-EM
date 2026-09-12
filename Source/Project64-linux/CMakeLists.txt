find_package(OpenGL REQUIRED)
find_package(SDL2 CONFIG REQUIRED)

add_executable(project64-em main.cpp)
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

add_custom_command(TARGET project64-em POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy_directory
    "${CMAKE_SOURCE_DIR}/Config" "${PJ64_DIRECTORY}/Config"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory
    "${CMAKE_SOURCE_DIR}/Lang" "${PJ64_DIRECTORY}/Lang"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory
    "${CMAKE_SOURCE_DIR}/Scripts" "${PJ64_DIRECTORY}/Scripts"
)
