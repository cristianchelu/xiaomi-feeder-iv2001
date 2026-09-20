/*
 * Hardware watchdog — spec/30-processes/power-state-machine.md § Watchdog
 */

#ifndef WDT_ADAPTER_H
#define WDT_ADAPTER_H

/* First statement of main(): latch the reset reason and arm the 30 s
 * reset-mode watchdog. No logging is available yet. */
void wdt_adapter_arm_early(void);

/* Log the latched reset reason once app_log is up (app task start). */
void wdt_adapter_log_reset_reason(void);

/* Called once per app event-loop iteration. */
void wdt_adapter_feed(void);

#endif /* WDT_ADAPTER_H */
