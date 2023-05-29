cmake_minimum_required(VERSION 3.12)

include(${LIGHT_PATH}/external/light_preinit.cmake)

project(crossfire C CXX ASM)

add_subdirectory(${LIGHT_PATH}/ light_framework)

# modules
add_subdirectory(module/light_usb_common)
add_subdirectory(module/light_usb_midi)
add_subdirectory(module/crossfire_main)
