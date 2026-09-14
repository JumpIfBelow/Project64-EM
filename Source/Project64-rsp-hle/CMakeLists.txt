file(GLOB SOURCES CONFIGURE_DEPENDS "*.cpp" "*.h")

add_library(Project64-rsp-hle SHARED ${SOURCES})
target_include_directories(Project64-rsp-hle PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(Project64-rsp-hle PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${PJ64_DIRECTORY}/Plugin"
  OUTPUT_NAME "Project64-rsp-hle"
)
