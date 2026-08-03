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
void crossfire_usbhost_request_reset(void) { }

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

static const struct test_case test_cases[] = {
        { "broadcast_two_devices", test_broadcast_two_devices },
        { "broadcast_three_devices", test_broadcast_three_devices },
        { "cable_count_mismatch", test_cable_count_mismatch },
        { "disconnect_removes_forwarding_target", test_disconnect_removes_forwarding_target },
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
