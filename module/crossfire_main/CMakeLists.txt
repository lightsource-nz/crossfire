add_executable(crossfire_main)

target_include_directories(
        crossfire_main PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_sources(
        crossfire_main PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/src/crossfire.c
)

target_link_libraries(
        crossfire_main PUBLIC
        light_usbhost_midi
)

if(LIGHT_SYSTEM STREQUAL PICO_SDK AND LIGHT_PLATFORM STREQUAL TARGET)
        pico_enable_stdio_uart(crossfire_main 1)
        pico_add_extra_outputs(crossfire_main)
endif()
