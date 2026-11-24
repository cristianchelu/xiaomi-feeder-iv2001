/*
 * System port — spec/30-processes/mqtt-protocol.md § Reboot command
 */

#ifndef SYSTEM_PORT_H
#define SYSTEM_PORT_H

/* Restart the whole device. Does not return on target. */
void system_port_reboot(void);

#endif /* SYSTEM_PORT_H */
