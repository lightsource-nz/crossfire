#include <stdio.h>
#include <string.h>

#include <light.h>

#include "crossfire_internal.h"
#include "mock_midi.h"

// defines this_app, which light_core's framework.c always references; the test
// never actually runs the application lifecycle (no light_framework_run() call),
// so the event/main handlers here are never invoked
static void test_app_event(const struct light_module *mod, uint8_t event, void *arg) { }
static uint8_t test_app_main(struct light_application *app) { return LF_STATUS_SHUTDOWN; }
Light_Application_Define(crossfire_forward_test, test_app_event, test_app_main, &light_core);

// crossfire_forward.c calls these (declared in crossfire_internal.h, defined in
// crossfire.c) on every mount/unmount and forwarded packet -- crossfire.c itself isn't
// part of this test target (it pulls in the real display/USB host stack), so stub them
// out here rather than exercising either concern
void crossfire_display_update_status(void) { }
void crossfire_display_update_indicators(void) { }

// counted rather than ignored: WHEN this is requested is itself under test. It resets the whole
// USB host controller, so a disconnect that still leaves devices mounted must not ask for one
static int usbhost_reset_requests = 0;
void crossfire_usbhost_request_reset(void) { usbhost_reset_requests++; }

static int failures = 0;

#define CHECK(cond, msg) do { \
        if(!(cond)) { \
                printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
                failures++; \
        } else { \
                printf("PASS: %s\n", msg); \
        } \
} while(0)

struct test_case {
        const char *name;
        void (*run)(void);
};

static void test_broadcast_two_devices(void)
{
        mock_midi_reset();
        mock_midi_connect(0, 1, 1, 1); // device 0: daddr 1, 1 rx cable, 1 tx cable
        mock_midi_connect(1, 2, 1, 1); // device 1: daddr 2, 1 rx cable, 1 tx cable

        // note-on, middle C, velocity 100, cable 0 -- byte 0 is (cable_num << 4) | CIN,
        // CIN 0x9 = note-on
        const uint8_t note_on[4] = { 0x09, 0x90, 0x3C, 0x64 };
        mock_midi_feed(0, note_on);

        cf_forward_service();

        uint8_t out[4];
        bool got = mock_midi_take_written(1, out);
        CHECK(got && memcmp(out, note_on, 4) == 0,
                "note-on from device 0 forwards to device 1's matching cable");

        got = mock_midi_take_written(0, out);
        CHECK(!got, "forwarding never loops data back to its own source device");
}

static void test_broadcast_three_devices(void)
{
        mock_midi_reset();
        mock_midi_connect(0, 1, 1, 1);
        mock_midi_connect(1, 2, 1, 1);
        mock_midi_connect(2, 3, 1, 1);

        // control change, channel volume, max, cable 0 -- CIN 0xB = control change
        const uint8_t cc[4] = { 0x0B, 0xB0, 0x07, 0x7F };
        mock_midi_feed(0, cc);

        cf_forward_service();

        uint8_t out[4];
        bool got1 = mock_midi_take_written(1, out);
        bool got2 = mock_midi_take_written(2, out);
        CHECK(got1 && got2,
                "one source broadcasts to every other mounted device");
}

static void test_cable_count_mismatch(void)
{
        mock_midi_reset();
        mock_midi_connect(0, 1, 2, 2); // device 0 has 2 rx/tx cables
        mock_midi_connect(1, 2, 1, 1); // device 1 only has 1

        // note-on, cable 1 (which device 1 doesn't have): byte 0 = (1 << 4) | 0x9
        const uint8_t data[4] = { 0x19, 0x91, 0x40, 0x50 };
        mock_midi_feed(0, data);

        cf_forward_service();

        uint8_t out[4];
        bool got = mock_midi_take_written(1, out);
        CHECK(!got, "forwarding is skipped for destinations that lack the source's cable number");
}

static void test_disconnect_removes_forwarding_target(void)
{
        mock_midi_reset();
        mock_midi_connect(0, 1, 1, 1);
        mock_midi_connect(1, 2, 1, 1);
        mock_midi_disconnect(1);

        // note-off, cable 0 -- CIN 0x8 = note-off
        const uint8_t data[4] = { 0x08, 0x80, 0x3C, 0x40 };
        mock_midi_feed(0, data);

        cf_forward_service();

        uint8_t out[4];
        bool got = mock_midi_take_written(1, out);
        CHECK(!got, "an unmounted device is dropped from the forwarding table");
}

// -- hub mode: four devices on one hub's downstream ports, reached through a single root port --

#define TEST_HUB_ADDR   5 // the address tinyusb gives the hub itself; any non-zero value works

// mounts 'count' devices on hub ports 1..count, each with one rx and one tx cable. daddr is
// idx+1, matching the order tinyusb would assign addresses as they enumerate
static void connect_hub_devices(uint8_t count)
{
        for(uint8_t idx = 0; idx < count; idx++) {
                mock_midi_set_bus_info(idx + 1, TEST_HUB_ADDR, idx + 1);
                mock_midi_connect(idx, idx + 1, 1, 1);
        }
}

static void test_hub_four_devices_broadcast(void)
{
        mock_midi_reset();
        connect_hub_devices(4);

        // note-on, middle C, cable 0, from the instrument on hub port 1
        const uint8_t note_on[4] = { 0x09, 0x90, 0x3C, 0x64 };
        mock_midi_feed(0, note_on);

        cf_forward_service();

        uint8_t out[4];
        bool all = true;
        for(uint8_t idx = 1; idx < 4; idx++)
                all = all && mock_midi_take_written(idx, out) && memcmp(out, note_on, 4) == 0;
        CHECK(all, "a device on one hub port broadcasts to all three other hub ports");
        CHECK(!mock_midi_take_written(0, out),
                "hub forwarding never loops data back out the port it arrived on");
}

static void test_hub_port_mapping(void)
{
        mock_midi_reset();
        connect_hub_devices(4);

        CHECK(cf_hub_addr() == TEST_HUB_ADDR, "the hub's address is learned from the devices behind it");

        bool mapped = true;
        for(uint8_t idx = 0; idx < 4; idx++)
                mapped = mapped && cf_hub_port_of_device(idx) == (uint8_t)(idx + 1);
        CHECK(mapped, "each mounted device reports the hub port it is plugged into");

        bool occupied = true;
        for(uint8_t port = 1; port <= 4; port++)
                occupied = occupied && cf_hub_port_occupied(port);
        CHECK(occupied, "every hub port with a device on it reads as occupied");
        CHECK(!cf_hub_port_occupied(CF_HUB_PORT_NONE),
                "port 0 is never occupied -- it is what a root-attached device reports");
}

static void test_hub_root_attached_device_has_no_port(void)
{
        mock_midi_reset();
        // no mock_midi_set_bus_info(): every address defaults to hub_addr 0, the root port
        mock_midi_connect(0, 1, 1, 1);

        CHECK(cf_hub_port_of_device(0) == CF_HUB_PORT_NONE,
                "a device plugged straight into the root port reports no hub port");
        CHECK(cf_hub_addr() == 0,
                "no hub is claimed when nothing has mounted behind one");
}

static void test_hub_unplug_keeps_siblings(void)
{
        mock_midi_reset();
        connect_hub_devices(4);

        mock_midi_disconnect(1); // the instrument on hub port 2

        CHECK(!cf_hub_port_occupied(2), "unplugging a device frees its hub port");
        CHECK(cf_hub_port_occupied(1) && cf_hub_port_occupied(3) && cf_hub_port_occupied(4),
                "the other hub ports are untouched by one device leaving");
        CHECK(cf_hub_addr() == TEST_HUB_ADDR,
                "the hub is still known while devices remain behind it");
        //   THE POINT OF THE WHOLE CHANGE: this reset tears down the host controller, taking
        // every other device on the bus with it. Behind a hub that would turn unplugging one
        // instrument into losing all four
        CHECK(usbhost_reset_requests == 0,
                "no host controller reset is requested while other devices are still mounted");

        const uint8_t cc[4] = { 0x0B, 0xB0, 0x07, 0x7F };
        mock_midi_feed(0, cc);
        cf_forward_service();

        uint8_t out[4];
        CHECK(mock_midi_take_written(2, out) && mock_midi_take_written(3, out),
                "the surviving devices keep receiving forwarded traffic");
        CHECK(!mock_midi_take_written(1, out),
                "the unplugged device is dropped from the forwarding table");
}

static void test_hub_reset_requested_once_port_is_empty(void)
{
        mock_midi_reset();
        connect_hub_devices(4);

        for(uint8_t idx = 0; idx < 3; idx++)
                mock_midi_disconnect(idx);
        CHECK(usbhost_reset_requests == 0,
                "unplugging all but the last device requests no host controller reset");

        mock_midi_disconnect(3);
        CHECK(usbhost_reset_requests == 1,
                "the disconnect that empties the bus requests the host controller reset");
        CHECK(cf_hub_addr() == 0,
                "the hub is forgotten once nothing is mounted behind it");
}

static const struct test_case test_cases[] = {
        { "broadcast_two_devices", test_broadcast_two_devices },
        { "broadcast_three_devices", test_broadcast_three_devices },
        { "cable_count_mismatch", test_cable_count_mismatch },
        { "disconnect_removes_forwarding_target", test_disconnect_removes_forwarding_target },
        { "hub_four_devices_broadcast", test_hub_four_devices_broadcast },
        { "hub_port_mapping", test_hub_port_mapping },
        { "hub_root_attached_device_has_no_port", test_hub_root_attached_device_has_no_port },
        { "hub_unplug_keeps_siblings", test_hub_unplug_keeps_siblings },
        { "hub_reset_requested_once_port_is_empty", test_hub_reset_requested_once_port_is_empty },
};
#define TEST_CASE_COUNT (sizeof(test_cases) / sizeof(test_cases[0]))

// each test case is run as its own process invocation (`crossfire_forward_test
// <name>`), so CTest can register and report on them individually
int main(int argc, char *argv[])
{
        if(argc != 2) {
                printf("usage: %s <test-case>\navailable test cases:\n", argv[0]);
                for(size_t i = 0; i < TEST_CASE_COUNT; i++)
                        printf("  %s\n", test_cases[i].name);
                return 1;
        }

        light_framework_init();

        for(size_t i = 0; i < TEST_CASE_COUNT; i++) {
                if(strcmp(argv[1], test_cases[i].name) != 0)
                        continue;
                test_cases[i].run();
                return failures ? 1 : 0;
        }

        printf("unknown test case '%s'\n", argv[1]);
        return 1;
}
