
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

# one CTest test per scenario in test_crossfire_forward.c, each run as its own
# invocation of the executable (`crossfire_forward_test <case-name>`) so they're
# reported and can be selected individually
set(CF_FORWARD_TEST_CASES
        broadcast_two_devices
        broadcast_three_devices
        cable_count_mismatch
        disconnect_removes_forwarding_target
)
foreach(test_case ${CF_FORWARD_TEST_CASES})
        add_test(
                NAME crossfire_forward.${test_case}
                COMMAND crossfire_forward_test ${test_case}
        )
endforeach()
