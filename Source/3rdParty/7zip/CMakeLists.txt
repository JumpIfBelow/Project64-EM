if(WIN32)
  file(GLOB_RECURSE SOURCES "*.c" "*.h")
else()
  set(SOURCES
    7zAlloc.c
    7zBuf.c
    7zBuf2.c
    7zCrc.c
    7zCrcOpt.c
    7zDec.c
    7zFile.c
    7zIn.c
    7zStream.c
    Alloc.c
    Bcj2.c
    Bra.c
    Bra86.c
    BraIA64.c
    CpuArch.c
    Delta.c
    Lzma2Dec.c
    LzmaDec.c
  )
endif()
include_directories("${CMAKE_SOURCE_DIR}/Source/3rdParty")
add_library(7zip STATIC ${SOURCES})
