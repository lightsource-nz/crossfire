cmake_minimum_required(VERSION 3.12)

if(DEFINED ENV{LIGHT_PATH} AND (NOT LIGHT_PATH))
    set(LIGHT_PATH "$ENV{LIGHT_PATH}" CACHE PATH "path to the root directory of the light-framework project")
endif()
set(LIGHT_PATH "${CMAKE_CURRENT_LIST_DIR}/../light_framework_mk3" CACHE PATH "path to the root directory of the light-framework project")
get_filename_component(LIGHT_PATH "${LIGHT_PATH}" REALPATH BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")

#   rend also declares FONT_CRUSHER_PATH, but defaults it relative to ITSELF -- and rend lives
# at module/rend here, so its guess lands on crossfire/module/font-crusher and misses a checkout
# sitting beside this project. Findcrush.cmake then quietly fetches font-crusher from GitHub, so
# edits to the local crush tooling silently have no effect on this build. Every build tree in
# this repo is currently in exactly that state, with FONT_CRUSHER_PATH pointing into _deps.
#   resolved here first, as a sibling of the project root, matching what screen-test does. Must
# be CACHE: creating a cache entry removes a normal variable of the same name, so an uncached
# value here would be discarded the moment rend is added and rend's guess would win anyway
if(DEFINED ENV{FONT_CRUSHER_PATH} AND (NOT FONT_CRUSHER_PATH))
    set(FONT_CRUSHER_PATH "$ENV{FONT_CRUSHER_PATH}" CACHE PATH "path to the root directory of the font-crusher project")
endif()
if(NOT FONT_CRUSHER_PATH)
    get_filename_component(_font_crusher_default "${CMAKE_CURRENT_LIST_DIR}/../font-crusher" REALPATH)
    if(EXISTS "${_font_crusher_default}/CMakeLists.txt")
        set(FONT_CRUSHER_PATH "${_font_crusher_default}" CACHE PATH "path to the root directory of the font-crusher project")
    endif()
endif()

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
add_subdirectory(module/light_canvas)
add_subdirectory(module/light_ioport)
add_subdirectory(module/light_display_sh1107)
add_subdirectory(module/light_display_po13)
add_subdirectory(module/crossfire_main)

if(LIGHT_PLATFORM STREQUAL HOST)
        enable_testing()
        add_subdirectory(test/crossfire_forward_test)
endif()
