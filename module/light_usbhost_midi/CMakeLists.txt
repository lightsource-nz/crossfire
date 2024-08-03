
add_library(light_usbhost_midi INTERFACE)

target_include_directories(
        light_usbhost_midi INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_sources(
        light_usbhost_midi INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/src/module.c
        ${CMAKE_CURRENT_LIST_DIR}/src/usbhost_midi.c
)

target_link_libraries(
        light_usbhost_midi INTERFACE
        light_usb_midi
        light_usbhost
        light_framework
)
