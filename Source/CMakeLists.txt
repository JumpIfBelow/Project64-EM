# Build
add_subdirectory(3rdParty)
add_subdirectory(Common)
add_subdirectory(Settings)
add_subdirectory(Project64-core)

if(WIN32)
  add_subdirectory(Project64)
  add_subdirectory(RSP)
  add_subdirectory(Project64-input)
  add_subdirectory(Project64-audio)
  add_subdirectory(nragev20)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_subdirectory(Project64-rsp-hle)
  add_subdirectory(Project64-input-sdl)
  add_subdirectory(Project64-audio-sdl)
  add_subdirectory(Project64-video)
  add_subdirectory(Project64-linux)
else()
  message(FATAL_ERROR "Project64-EM currently supports Windows and Linux")
endif()

set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
include(InstallRequiredSystemLibraries)
if(WIN32)
  install(DIRECTORY "${PJ64_DIRECTORY}/" DESTINATION .)
else()
  install(TARGETS
    project64-em
    RUNTIME DESTINATION .
  )
  install(TARGETS
    Project64-rsp-hle
    Project64-input-sdl
    Project64-audio-sdl
    Project64-gfx
    LIBRARY DESTINATION Plugin
  )
  install(DIRECTORY
    "${CMAKE_SOURCE_DIR}/Config"
    "${CMAKE_SOURCE_DIR}/Lang"
    "${CMAKE_SOURCE_DIR}/Scripts"
    DESTINATION .
  )
endif()
