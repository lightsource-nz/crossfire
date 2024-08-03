
add_library(light_usb_midi INTERFACE)

target_include_directories(
        light_usb_midi INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_sources(
        light_usb_midi INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/src/module.c
        ${CMAKE_CURRENT_LIST_DIR}/src/usb_midi.c
)

target_link_libraries(
        light_usb_midi INTERFACE
        light_usb_common
        light_framework
)

if(LIGHT_SYSTEM STREQUAL PICO_SDK)
        target_link_libraries(
                light_usb_midi INTERFACE
                
        )
endif()
