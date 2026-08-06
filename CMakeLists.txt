cmake_minimum_required(VERSION 3.12)

set(LIGHT_PATH "${CMAKE_CURRENT_LIST_DIR}/../light_framework_mk3" CACHE PATH "path to the root directory of the light-framework project")
get_filename_component(LIGHT_PATH "${LIGHT_PATH}" REALPATH BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")
include(${LIGHT_PATH}/external/light_preinit.cmake)

project(crossfire C CXX ASM)

add_subdirectory(${LIGHT_PATH}/ light_framework)

# modules
add_subdirectory(module/light_usb)
add_subdirectory(module/light_usb_midi)
add_subdirectory(module/light_usbhost)
add_subdirectory(module/light_usbhost_midi)
add_subdirectory(module/rend)
add_subdirectory(module/light_display)
add_subdirectory(module/light_ioport)
add_subdirectory(module/light_display_sh1107)
add_subdirectory(module/light_display_po13)
add_subdirectory(module/crossfire_main)

if(LIGHT_PLATFORM STREQUAL HOST)
        enable_testing()
        add_subdirectory(test/crossfire_forward_test)
endif()
