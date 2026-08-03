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

target_link_libraries(
        crossfire_main PUBLIC
        light_usbhost_midi
        light_display
        light_display_po13
        rend
)

if(CROSSFIRE_ENABLE_SPI_LINK)
        target_sources(
                crossfire_main PUBLIC
                ${CMAKE_CURRENT_LIST_DIR}/src/crossfire_spi_link.c
        )
        target_compile_definitions(crossfire_main PUBLIC CF_HAVE_SPI_LINK)
        if(LIGHT_SYSTEM STREQUAL PICO_SDK AND LIGHT_PLATFORM STREQUAL TARGET)
                target_link_libraries(crossfire_main PUBLIC hardware_spi)
        endif()
endif()

if(LIGHT_SYSTEM STREQUAL PICO_SDK AND LIGHT_PLATFORM STREQUAL TARGET)
        # gates crossfire_forward.c's forwarding engine; kept as its own flag (rather
        # than reusing _HAVE_TINYUSB) so a host-side test target can link the engine
        # against a mock backend instead of the real tinyusb host stack
        target_compile_definitions(crossfire_main PUBLIC CF_HAVE_MIDI_BACKEND)
        pico_enable_stdio_uart(crossfire_main 1)
        pico_add_extra_outputs(crossfire_main)
endif()
