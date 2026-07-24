
add_executable(crossfire_forward_test)

target_include_directories(
        crossfire_forward_test PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../../module/crossfire_main/src
        ${CMAKE_CURRENT_LIST_DIR}/mock
)

target_sources(
        crossfire_forward_test PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../../module/crossfire_main/src/crossfire_forward.c
        ${CMAKE_CURRENT_LIST_DIR}/mock/mock_midi.c
        ${CMAKE_CURRENT_LIST_DIR}/src/test_crossfire_forward.c
)

target_compile_definitions(
        crossfire_forward_test PRIVATE
        CF_HAVE_MIDI_BACKEND
)

target_link_libraries(
        crossfire_forward_test PRIVATE
        light_core
)
