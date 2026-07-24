add_executable(crossfire_main)

target_include_directories(
        crossfire_main PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_sources(
        crossfire_main PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/src/crossfire.c
        ${CMAKE_CURRENT_LIST_DIR}/src/crossfire_forward.c
)

target_link_libraries(
        crossfire_main PUBLIC
        light_usbhost_midi
)

if(LIGHT_SYSTEM STREQUAL PICO_SDK AND LIGHT_PLATFORM STREQUAL TARGET)
        # gates crossfire_forward.c's forwarding engine; kept as its own flag (rather
        # than reusing _HAVE_TINYUSB) so a host-side test target can link the engine
        # against a mock backend instead of the real tinyusb host stack
        target_compile_definitions(crossfire_main PUBLIC CF_HAVE_MIDI_BACKEND)
        pico_enable_stdio_uart(crossfire_main 1)
        pico_add_extra_outputs(crossfire_main)
endif()
