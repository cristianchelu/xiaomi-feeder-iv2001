#include "fake_system_port.h"
#include "system_port.h"

static unsigned s_reboot_calls;

void fake_system_port_reset(void) { s_reboot_calls = 0u; }
unsigned fake_system_port_reboot_calls(void) { return s_reboot_calls; }
void system_port_reboot(void) { s_reboot_calls++; }
