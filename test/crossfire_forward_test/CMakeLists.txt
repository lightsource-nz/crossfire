
add_executable(crossfire_forward_test)

target_include_directories(
        crossfire_forward_test PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../../module/crossfire_main/src
        ${CMAKE_CURRENT_LIST_DIR}/mock
)

target_sources(
        crossfire_forward_test PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../../module/crossfire_main/src/crossfire_forward.c
        ${CMAKE_CURRENT_LIST_DIR}/../../module/crossfire_main/src/crossfire_hub.c
        ${CMAKE_CURRENT_LIST_DIR}/mock/mock_midi.c
        ${CMAKE_CURRENT_LIST_DIR}/src/test_crossfire_forward.c
)

#   CF_HAVE_USB_HUB is set here unconditionally, unlike in the firmware where it follows
# CROSSFIRE_ENABLE_USB_HUB. This target exists to exercise the forwarding engine against a mock
# backend, and hub mode is the configuration with the most of it in play -- four devices rather
# than one, and a disconnect that must NOT take the survivors down with it. Gating it on the
# firmware option instead would mean the default CI run compiled crossfire_hub.c nowhere
target_compile_definitions(
        crossfire_forward_test PRIVATE
        CF_HAVE_MIDI_BACKEND
        CF_HAVE_USB_HUB
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
        hub_four_devices_broadcast
        hub_port_mapping
        hub_root_attached_device_has_no_port
        hub_unplug_keeps_siblings
        hub_reset_requested_once_port_is_empty
)
foreach(test_case ${CF_FORWARD_TEST_CASES})
        add_test(
                NAME crossfire_forward.${test_case}
                COMMAND crossfire_forward_test ${test_case}
        )
endforeach()
