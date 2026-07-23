
add_library(light_usbhost INTERFACE)

target_include_directories(
        light_usbhost INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_sources(
        light_usbhost INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/src/module.c
        ${CMAKE_CURRENT_LIST_DIR}/src/usbhost.c
)

target_link_libraries(
        light_usbhost INTERFACE
        light_usb_common
)

if(LIGHT_SYSTEM STREQUAL PICO_SDK AND LIGHT_PLATFORM STREQUAL TARGET)
        target_link_libraries(
                light_usbhost INTERFACE
                tinyusb_host
        )
endif()
