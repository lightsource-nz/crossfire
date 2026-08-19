add_executable(crossfire_main)

# builds crossfire_main as a 2-board SPI-linked test rig instead of the full 4-port hub:
# one local USB-MIDI device + one virtual "device" reached over a pair of one-way SPI
# links to an identically-built peer board, standing in for the custom PCB's other ports
# until that hardware is available to test against. see crossfire_spi_link.c
option(CROSSFIRE_ENABLE_SPI_LINK "Build crossfire_main as a 2-board SPI-linked MIDI transport test rig" OFF)

target_include_directories(
        crossfire_main PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_CURRENT_LIST_DIR}/fonts
)

target_sources(
        crossfire_main PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/src/crossfire.c
        ${CMAKE_CURRENT_LIST_DIR}/src/crossfire_forward.c
        ${CMAKE_CURRENT_LIST_DIR}/fonts/TypeLightSans_ttf_16px_font.c
)

#   THE STATUS DISPLAY IS OPTIONAL, because it is a Pico-OLED-1.3 expansion board -- it plugs
# into a Pico header and exists on no other target. Defaulted from the platform rather than
# forced on: an RP2 build gets the panel it has always had, and a board without one builds
# headless with USB-MIDI forwarding and console output instead of failing to configure.
if(LIGHT_SYSTEM STREQUAL PICO_SDK AND LIGHT_PLATFORM STREQUAL TARGET)
        set(_cf_display_default ON)
else()
        set(_cf_display_default OFF)
endif()
option(CROSSFIRE_ENABLE_DISPLAY "Build crossfire_main with the Pico-OLED-1.3 status display" ${_cf_display_default})

target_link_libraries(
        crossfire_main PUBLIC
        light_usbhost_midi
        light_draw
)

if(CROSSFIRE_ENABLE_DISPLAY)
        target_link_libraries(
                crossfire_main PUBLIC
                light_canvas
                light_display
                light_display_po13
        )
        target_compile_definitions(crossfire_main PUBLIC CF_HAVE_DISPLAY)
endif()

if(CROSSFIRE_ENABLE_SPI_LINK)
        target_sources(
                crossfire_main PUBLIC
                ${CMAKE_CURRENT_LIST_DIR}/src/crossfire_spi_link.c
        )
        target_compile_definitions(crossfire_main PUBLIC CF_HAVE_SPI_LINK)
        #   the link drives SPI through light_ioport, so it needs that target in its own right.
        # It used to arrive transitively through light_display -- which a headless build does not
        # link, so naming it here is what keeps the link usable without a panel
        target_link_libraries(crossfire_main PUBLIC light_ioport)
        if(LIGHT_SYSTEM STREQUAL PICO_SDK AND LIGHT_PLATFORM STREQUAL TARGET)
                target_link_libraries(crossfire_main PUBLIC hardware_spi)
        endif()
endif()

#   the forwarding engine needs a real USB host stack behind it, which is any TARGET build with
# TinyUSB -- both platforms now, not just the Pico. Kept as its own flag rather than reusing
# _HAVE_TINYUSB so a host-side test target can link the engine against a mock backend instead.
#   nested rather than one parenthesised expression: CMake's if() rejected
#   `A STREQUAL B AND (C STREQUAL D OR C STREQUAL E)` with "Unknown arguments specified"
set(_cf_have_backend OFF)
if(LIGHT_PLATFORM STREQUAL TARGET)
        if(LIGHT_SYSTEM STREQUAL PICO_SDK OR LIGHT_SYSTEM STREQUAL CMSIS)
                set(_cf_have_backend ON)
        endif()
endif()
if(_cf_have_backend)
        target_compile_definitions(crossfire_main PUBLIC CF_HAVE_MIDI_BACKEND)
endif()

#   the per-platform packaging step, which has no common spelling: the SDK generates .uf2 and
# friends through its own function, while a bare-CMSIS image gets .bin/.hex and a size breakdown
# from the property hook cmake/cmsis.cmake installs
if(LIGHT_SYSTEM STREQUAL PICO_SDK AND LIGHT_PLATFORM STREQUAL TARGET)
        pico_enable_stdio_uart(crossfire_main 1)
        pico_add_extra_outputs(crossfire_main)
elseif(LIGHT_SYSTEM STREQUAL CMSIS AND LIGHT_PLATFORM STREQUAL TARGET)
        light_target_set_property(crossfire_main application_target 1)
endif()
