/* Tests: spec/30-processes/power-state-machine.md § Implementation status (app wiring) */

#include <string.h>

#include "unity.h"

#include "app.h"
#include "app_event.h"
#include "app_event_port.h"
#include "config_keys.h"
#include "display_presentation.h"
#include "fake_config_port.h"
#include "fake_display_port.h"
#include "fake_power_source_port.h"
#include "fake_hopper_ir_port.h"
#include "feeder_runtime.h"
#include "mqtt_client.h"
#include "power_source_input.h"
#include "wifi_sta.h"
#include "wifi_sta_test.h"

extern void fake_app_event_q_reset(void);
extern void host_stub_set_provision_active(bool active);

static void tick(uint32_t now_ms)
{
    app_event_t ev;

    memset(&ev, 0, sizeof(ev));
    ev.type = EVT_DISPLAY_TICK;
    ev.u.display_tick.now_ms = now_ms;
    TEST_ASSERT_TRUE(app_event_post(&ev));
    (void)app_step();
}

static void setup(bool mains, const char *battery_wifi)
{
    fake_config_port_reset();
    fake_display_port_reset();
    fake_app_event_q_reset();
    fake_power_source_port_reset();
    fake_power_source_port_set_mains_present(mains);
    host_stub_set_provision_active(false);
    app_test_reset();
    display_presentation_reset();
    power_source_input_init(fake_power_source_port_get());
    wifi_sta_test_bootstrap();
    TEST_ASSERT_EQUAL(PORT_OK, fake_config_port_get()->write(CONFIG_GROUP_POWER,
                                                             CONFIG_KEY_POWER_BATTERY_WIFI,
                                                             battery_wifi));
    mqtt_client_notify_wifi_ready();
    tick(0u);
    tick(100u);
}

static void run_until(uint32_t from_ms, uint32_t to_ms)
{
    uint32_t t;

    for (t = from_ms; t <= to_ms; t += 500u) {
        tick(t);
    }
}

void test_app_policy_keeps_wifi_during_boot_grace(void)
{
    setup(false, "off");
    run_until(200u, 50000u);
    TEST_ASSERT_EQUAL(WIFI_STA_IDLE, wifi_sta_busy());
}

void test_app_policy_battery_off_disconnects_and_stops_mqtt(void)
{
    setup(false, "off");
    run_until(200u, 62000u);
    TEST_ASSERT_EQUAL(WIFI_STA_BUSY_DISCONNECT, wifi_sta_busy());
    TEST_ASSERT_FALSE(mqtt_client_wifi_is_ready());
}

void test_app_policy_never_touches_radio_while_provisioning(void)
{
    setup(false, "off");
    host_stub_set_provision_active(true);
    run_until(200u, 62000u);
    TEST_ASSERT_EQUAL(WIFI_STA_IDLE, wifi_sta_busy());
    host_stub_set_provision_active(false);
}

void test_app_policy_mains_keeps_wifi_even_when_off_on_battery(void)
{
    setup(true, "off");
    run_until(200u, 62000u);
    TEST_ASSERT_EQUAL(WIFI_STA_IDLE, wifi_sta_busy());
}

void test_app_hopper_not_sampled_while_dispensing(void)
{
    uint32_t before;

    setup(true, "on");
    fake_hopper_ir_port_reset();
    feeder_runtime_set_dispense_active(true);
    before = fake_hopper_ir_port_sense_count();
    run_until(200u, 30000u);   /* three background periods on mains */
    TEST_ASSERT_EQUAL_UINT32(before, fake_hopper_ir_port_sense_count());
    feeder_runtime_set_dispense_active(false);
    run_until(30500u, 45000u);
    TEST_ASSERT_GREATER_THAN_UINT32(before, fake_hopper_ir_port_sense_count());
}
