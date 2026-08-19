cmake_minimum_required(VERSION 3.12)

if(DEFINED ENV{LIGHT_PATH} AND (NOT LIGHT_PATH))
    set(LIGHT_PATH "$ENV{LIGHT_PATH}" CACHE PATH "path to the root directory of the light-framework project")
endif()
set(LIGHT_PATH "${CMAKE_CURRENT_LIST_DIR}/../light_framework_mk3" CACHE PATH "path to the root directory of the light-framework project")
get_filename_component(LIGHT_PATH "${LIGHT_PATH}" REALPATH BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")

#   light_draw (vendored inside the light_display group, resolved below as module/light_draw)
# also declares FONT_CRUSHER_PATH, but defaults it relative to ITSELF -- two directories deeper
# than light_display's own root, so its guess only lands correctly when light_display resolves
# as a proper sibling checkout. Findcrush.cmake otherwise quietly fetches font-crusher from
# GitHub instead, so edits to the local crush tooling silently have no effect on this build.
#   resolved here first, as a sibling of the project root, matching what screen-test does. Must
# be CACHE: creating a cache entry removes a normal variable of the same name, so an uncached
# value here would be discarded the moment light_draw is added and its own guess would win anyway
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

#   the USB group -- device core, host core and both MIDI class drivers. These were four
# directories in this repository, which meant the only way to use USB in a second project was to
# copy them; nothing in them is specific to crossfire. Resolved by path now, defaulting to a
# sibling checkout at ../light_usb.
#
#   this project links light_usbhost_midi, which pulls the rest of the chain. The others cost
# nothing regardless -- all four are INTERFACE libraries that compile nothing until linked.
#
#   MARKER for the same reason the display group needs one: the group and its core module share
# the name light_usb, and the resolver prefers an in-project module/<name> over the sibling.
# module/light_usb is exactly what a checkout of the MODULE would be called, and this repository
# had one until the extraction. (The CMake targets do not collide -- the core module defines
# light_usb_common -- so this is only about finding the right directory.)
light_resolve_project(LIGHT_USB light_usb MARKER module/light_usb/CMakeLists.txt)
add_subdirectory(${LIGHT_USB_PATH} light_usb_group)

#   the display group -- core, panel drivers and backlight, previously four separate submodules
# here (light_display, light_ioport, light_display_sh1107, light_display_po13) pinned at four
# independent commits. Resolved by path now, defaulting to a sibling checkout at
# ../light_display, so nothing here names a location.
#
#   this project links only the SH1107/po13 pair; the rest of the group's drivers are INTERFACE
# libraries that compile nothing unless something links them, so their presence is free.
#
#   light_ioport is not listed: it moved into the framework and arrives with
# add_subdirectory(${LIGHT_PATH}) above.
#
#   MARKER, because the group and one of its own modules share the name light_display. The
# resolver looks for an in-project module/<name> before the sibling, and module/light_display is
# exactly what a checkout of the MODULE would be called -- so without a marker that only the
# group has, a stray module checkout would silently shadow the group and the build would fail
# hunting for drivers that were never added.
light_resolve_project(LIGHT_DISPLAY light_display MARKER module/light_display/CMakeLists.txt)
add_subdirectory(${LIGHT_DISPLAY_PATH} light_display_group)

add_subdirectory(module/light_canvas)
add_subdirectory(module/crossfire_main)

if(LIGHT_PLATFORM STREQUAL HOST)
        enable_testing()
        add_subdirectory(test/crossfire_forward_test)
endif()
