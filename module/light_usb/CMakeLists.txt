
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
        light_framework
)

if(LIGHT_SYSTEM STREQUAL PICO_SDK)
        target_link_libraries(
                light_usb_common INTERFACE
                tinyusb_board
                tinyusb_pico_pio_usb
                pico_stdlib
        )
endif()
