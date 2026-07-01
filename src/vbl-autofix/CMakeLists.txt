cmake_minimum_required(VERSION 3.13)
project(VBLAutofix C)

# Tester build: pre-measures VBL, issues cscSetInterrupt(enable), post-measures, and
# reports via beep + a System-Folder log (so testers can report what it did). Build
# with the Retro68 PowerPC toolchain:
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=<retro68-build>/toolchain/powerpc-apple-macos/cmake/retroppc.toolchain.cmake
#   cmake --build build
# InterfaceLib only (PBControlSync/PBStatusSync, SlotVInstall, Timer). PPC application.

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
add_compile_options(-Wno-multichar -Wall)

add_application(VBLAutofix_v1 autofix.c autofix.r TYPE "APPL" CREATOR "VAfx")
if(TARGET retrocrt)
    add_dependencies(VBLAutofix_v1 retrocrt)
endif()

# Wrap the raw HFS .dsk in an Apple Partition Map so it mounts on OS 9.
set(WRAP_APM ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/wrap-apm.py)
add_custom_command(
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/VBLAutofix_v1.img
    COMMAND /usr/bin/env python3 ${WRAP_APM}
            ${CMAKE_CURRENT_BINARY_DIR}/VBLAutofix_v1.dsk
            ${CMAKE_CURRENT_BINARY_DIR}/VBLAutofix_v1.img VBLAutofix_v1
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/VBLAutofix_v1.dsk ${WRAP_APM}
    COMMENT "Wrapping VBLAutofix_v1.dsk -> .img (mountable on OS 9)" VERBATIM)
add_custom_target(VBLAutofix_disk ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/VBLAutofix_v1.img)
add_dependencies(VBLAutofix_disk VBLAutofix_v1_APPL)
