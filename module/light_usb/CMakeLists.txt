
add_library(light_usb_common INTERFACE)

target_include_directories(
        light_usb_common INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_sources(
        light_usb_common INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/src/module.c
        ${CMAKE_CURRENT_LIST_DIR}/src/usb.c
)

target_link_libraries(
        light_usb_common INTERFACE
        light_core
)

if(LIGHT_SYSTEM STREQUAL PICO_SDK)
        target_link_libraries(
                light_usb_common INTERFACE
                pico_stdlib
        )
endif()
#   NO tinyusb_pico_pio_usb. This project uses the RP2's hardware USB controller only --
# crossfire_main/include/tusb_config.h sets CFG_TUH_RPI_PIO_USB to 0 and BOARD_TUH_RHPORT to
# the native port -- so the PIO-emulated host controller was linked in and never called.
#
#   it was not merely wasted: Pico-PIO-USB is not a submodule of anything. tinyusb GITIGNORES
# hw/mcu/raspberry_pi/ and its own tools/get_deps.py clones the library in from a third-party
# repository, so the sources existed on one developer's disk, tracked by no repository at all.
# Every rp2040 and rp2350 build of this project therefore depended on a directory that no clone
# reproduces, and CI found it immediately: `cannot find -ltinyusb_pico_pio_usb`.
#
#   dropping the link removes the dependency rather than papering over it with a fetch step for
# code nothing here executes. If PIO-USB is ever wanted (a second USB port on spare GPIOs), it
# comes back with CFG_TUH_RPI_PIO_USB=1 and a get_deps.py step, together.
if(LIGHT_SYSTEM STREQUAL PICO_SDK AND LIGHT_PLATFORM STREQUAL TARGET)
        target_link_libraries(
                light_usb_common INTERFACE
                tinyusb_board
        )
endif()
