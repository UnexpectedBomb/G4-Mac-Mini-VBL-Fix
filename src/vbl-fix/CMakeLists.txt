cmake_minimum_required(VERSION 3.13)
project(VBLFix C)

# Silent everyday fix: issues cscSetInterrupt(enable) to the boot display at startup
# and quits. Build with the Retro68 PowerPC toolchain:
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=<retro68-build>/toolchain/powerpc-apple-macos/cmake/retroppc.toolchain.cmake
#   cmake --build build
# InterfaceLib only (PBControlSync). PPC application.

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
add_compile_options(-Wno-multichar -Wall)

add_application(VBLFix_v1 autofix_silent.c autofix_silent.r TYPE "APPL" CREATOR "VFix")
if(TARGET retrocrt)
    add_dependencies(VBLFix_v1 retrocrt)
endif()

# Wrap the raw HFS .dsk in an Apple Partition Map so it mounts on OS 9.
set(WRAP_APM ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/wrap-apm.py)
add_custom_command(
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/VBLFix_v1.img
    COMMAND /usr/bin/env python3 ${WRAP_APM}
            ${CMAKE_CURRENT_BINARY_DIR}/VBLFix_v1.dsk
            ${CMAKE_CURRENT_BINARY_DIR}/VBLFix_v1.img VBLFix_v1
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/VBLFix_v1.dsk ${WRAP_APM}
    COMMENT "Wrapping VBLFix_v1.dsk -> .img (mountable on OS 9)" VERBATIM)
add_custom_target(VBLFix_disk ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/VBLFix_v1.img)
add_dependencies(VBLFix_disk VBLFix_v1_APPL)
