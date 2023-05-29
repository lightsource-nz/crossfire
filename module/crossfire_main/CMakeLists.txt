
add_executable(crossfire_main)

light_framework_application_module(crossfire_main)

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
        light_framework
        light_usb_common
)

if(LIGHT_SYSTEM STREQUAL PICO_SDK)
        pico_add_extra_outputs(crossfire_main)
endif()
