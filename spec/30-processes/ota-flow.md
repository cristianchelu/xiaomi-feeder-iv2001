# OTA update flow

serves:
  - ../20-stories/updates.md

## Trigger

User publishes to `.../cmd/ota`:
```json
{"url": "http://192.168.1.100:8080/firmware.bin", "sha512": "<128 hex chars>"}
```

The `sha512` field is optional; when present it must match the SHA-512 of the
image as written to the inactive bank (see [Verification](#verification)).

Device validates URL (non-empty, http:// or https:// scheme).

## Pre-flight checks

Firmware enforces URL validation and duplicate-OTA rejection. Dispense
gating is specified below but not enforced until the dispense supervisor
exists (`dispense-cycle.md`).

| Check | Action on failure | Enforced |
|-------|-------------------|----------|
| Dispense in progress? | Reject OTA, publish error: `"busy_dispense"` | No — requires `dispense-cycle.md` |
| URL scheme valid? | Reject, publish error: `"invalid_url"` | Yes |
| Already updating? | Reject, publish error: `"already_in_progress"` | Yes |

## Download

### Pre-download memory reclaim

Before spawning the download worker, firmware suspends non-essential
persistent tasks to free RTOS stack RAM. The FreeRTOS heap is `[tune]`
192 KB (`freertos_heap_192k.patch` on `minicli/inc/FreeRTOSConfig.h`); the
download worker needs `[tune]` 12 KB stack. `[design]`

| Task | Action during OTA window |
|------|--------------------------|
| Admin HTTP (`httpd`) | Stop LAN web UI when `WEB_UI_ENABLE=y`; restart after failed OTA when STA ready — see [web-ui.md](web-ui.md) |
| `remote_cli` | End active telnet session, close port 2323 listener, delete task to free stack (when `REMOTE_CLI_ENABLE`; recreated after failed OTA) |
| `app_cli` | Suspend UART0 console task and delete to free stack; recreated after failed OTA |
| `wifi_sta` | Suspend connect worker and delete to free stack; recreated after failed OTA |
| `mqtt_io` | Disarm reconnect, disconnect broker session (task keeps running; broker buffers freed) |

On download-worker spawn failure or any download/verify/apply failure before
reboot: resume suspended tasks in reverse order (admin HTTP when enabled,
MQTT, `wifi_sta`, `app_cli`, `remote_cli` when enabled). On successful apply:
reboot — no resume. See [uart-console.md](uart-console.md) § Remote telnet
console and [web-ui.md](web-ui.md).

### Steps

1. Publish `.../ota/status`: `{"state": "downloading", "pct": 0}`.
2. Suspend idle tasks (above).
3. One HTTP(S) GET to the provided URL, streamed to flash as it arrives.
4. Receive in `[tune]` 4 KB chunks (`OTA_CHUNK_SIZE`).
5. Write each chunk to the inactive application bank at its stream offset.
6. Publish progress every `[tune]` 5 % (e.g. at 5, 10, 15 … 100 %).
7. Enforce maximum image size (partition size minus header). Abort if exceeded.

HTTPS: supported if `mqtt/tls` is enabled and mbedTLS RAM budget permits.

### Streaming download and resume

The image is one HTTP GET whose body is programmed into the inactive bank
as it arrives; the lwIP receive window is capped at `[tune]` 8 KB
([build-integration.md](../40-architecture/build-integration.md) § lwIP
receive window) so the server cannot exhaust the connsys RX buffers. The
image length comes from `Content-Length` of the first response (status
200). `[design]`

When the connection or receive fails, or the server closes before
`Content-Length` bytes arrived, the device waits `[tune]` 1 s, logs
`retry N at <offset>`, and reconnects with `Range: bytes=<offset>-` where
`<offset>` is the number of bytes already programmed. The resume response
must be 206 with a `Content-Range` total equal to the first length; a 200
(server ignoring Range) or a different total ends the attempt as a failure.
Any byte received resets the failure counter; `[tune]` 3 consecutive
attempts without progress abort with `download failed at <offset> after N
attempts` and `"download_failed"`. `[design]`

The device keeps no running hash over the received stream — the only hash
it computes is over the bank contents after the download
([Verification](#verification)) — so a resumed download cannot skew
verification. On completion it logs
`download complete bytes=<n> in <ms> ms flash=<ms> ms`, where `flash` is
the time spent erasing and programming.

### Internal progress phases

The OTA port reports finer-grained `ota_status_t` values than MQTT exposes.
`PREPARING` and `CONNECTING` are internal only — `ota_client` still publishes
`"state": "downloading", "pct": 0` for both. Panel feedback during these
phases: [display-presentation.md](display-presentation.md) § OTA indicator.

| Internal status | When reported |
|-----------------|---------------|
| `PREPARING` | Download worker task starts (MQTT suspend already done in `start`) |
| `CONNECTING` | After pre-download settle, immediately before the HTTP GET |
| `DOWNLOADING` | First HTTP body bytes; `pct` 0–100 during transfer |
| `VERIFYING` | Download complete; SHA-512 / flash verify |
| `APPLYING` | Bank swap pending; reboot follows |
| `ERROR` | Any failure before reboot |

## Verification

After the download loop completes (`download complete bytes=N`):

1. Check the image size against the bank size and probe the vector table in
   the first 64 KB of the inactive bank (the same scan the bootloader uses).
2. Invalidate the CM4 cache lines covering the inactive bank, then compute
   SHA-512 over the `N` bytes written to it. Flash reads are `memcpy` from
   the XIP window and both banks are cacheable
   ([partition-layout.md](../40-architecture/partition-layout.md) § CM4 cache
   regions), so stale lines from before the erase must not feed the hash.
   `[design]`
3. When the manifest carried `sha512`, compare the bank hash to it. On
   mismatch: log `sha512 mismatch`, publish error `"verify_failed"`, do not
   swap banks. Without a manifest hash the bank hash is accepted as-is.
4. Store the bank hash in the A/B control block when the active flag flips
   ([partition-layout.md](../40-architecture/partition-layout.md#ab-control-block)).

Bytes received over HTTP are never hashed directly; the verified object is
the flash contents that will boot. No signature verification in v1 (no PKI
infrastructure). `[design]`

## Flash layout

Total flash: 2 MB. **A/B dual-bank:** two application slots; a custom
bootloader boots one bank at a time. Download targets the inactive bank;
the running image is untouched until verification succeeds. `[design]`

## Apply

1. Publish `.../ota/status`: `{"state": "applying"}`.
2. Mark the verified inactive bank as the boot target.
3. Set boot flag for new image.
4. Reboot (see [Pre-reboot teardown](#pre-reboot-teardown)).

### Pre-reboot teardown

Before WDT reboot into the new bank:

1. Disconnect AP (`wifi_connection_disconnect_ap`) — sends deauth to the
   access point so it can clean up the STA entry immediately.
2. Wait `[tune]` 500 ms for the disconnect to propagate.
3. Disable I-cache; trigger `hal_sys_reboot()`.

The next boot brings the N9 up through the stock SDK `connsys_init()`
sequence; no extra coprocessor reset is applied. Warm reboots (OTA apply,
`bank switch`) associate as fast as cold boots
([wifi-lifecycle.md](wifi-lifecycle.md) § Boot timing across banks).

### Boot after apply

The first boot into the new bank follows the normal boot path. Both banks
execute cached, so `STA ready` arrives ~5 s after `FreeRTOS Running` on
either bank ([wifi-lifecycle.md](wifi-lifecycle.md) § Boot timing across
banks); slot health confirm follows 60 s later ([Slot health](#slot-health)).

### Active bank on MQTT

Every `.../ota/status` JSON includes `"bank": "A"` or `"B"` — the application
partition the bootloader will run. Bench OTA scripts (`tools/ota/`) read the
active bank from retained `ota/status`, not from `.../state`. See
[mqtt-protocol.md](mqtt-protocol.md) § OTA status.

## Slot health

One recovery path covers OTA apply and UART `bank switch`. The bootloader
auto-toggles the active bank after the strike limit when a slot stays
unverified. Slot confirmation means the firmware image boots and stays up —
not that MQTT is reachable.

### Control block

See [partition-layout.md](../40-architecture/partition-layout.md). Every
bank swap sets `unverified = 1` and `boot_attempts = 0`. Only crash-free
confirm clears `unverified` and `boot_attempts`.

### Bootloader: boot-attempt trap

Before bank selection on each boot, while `unverified` is set:

1. Increment `boot_attempts` and persist.
2. If `boot_attempts >=` `[tune]` `BOOT_MAX_ATTEMPTS` (3):
   - Log `boot attempt limit — switching bank`.
   - Toggle `active_flag` to the other bank.
   - Set `boot_attempts = 0`; keep `unverified = 1`.
   - Persist.

Then run existing header validation and cross-bank fallback. Bank validity
uses a vector-table scan over the first `[tune]` 64 KB (4-byte steps) —
the same probe used pre-swap during OTA verify — not the 8-byte header
probe alone.

There is no application-side timeout revert. Resets before confirm are
handled by the bootloader strike counter.

### Application: crash-free confirm

On boot, when `unverified` is clear, return immediately (steady-state boots).

When `unverified == 1`:

1. Increment `system/boot_count` in NVDM (diagnostics).
2. Start `[tune]` 60 s uptime timer.
3. On expiry without intervening reset: clear `unverified`, `boot_attempts`,
   and `boot_count` in the control block / NVDM.

Poll via the app timer tick (`ota_slot_health_poll_ms()`). MQTT connect is
not part of slot confirmation.

When a software watchdog is present (`power-state-machine.md`), a hang
during the confirm window feeds the bootloader strike path via WDT reset;
the confirm timer and WDT are independent mechanisms.

### Recovery layers

| Layer | Trigger | Location |
|-------|---------|----------|
| Boot attempt counter | Reset before 60 s confirm while `unverified` | Bootloader — 3 strikes, auto bank toggle |
| Crash-free confirm | 60 s uptime without reset while `unverified` | Application — clears `unverified` |
| Vector-table scan | Invalid vector table in first 64 KB | Bootloader — reject bank before jump |

### Dev deploy paths

| Workflow | UART required |
|----------|---------------|
| OTA when device is online | No |
| Iteration on a confirmed slot | No |
| Partial CODA flash, bootloader update, both slots bad | Yes |

Guardrails:

- Primary deploy: `mqtt-ota.sh` when MQTT is available.
- CODA / `iot-flash.sh`: flash bootloader + both banks from
  `flash_download.cfg`.
- Do not `bank switch` to a partially written slot; complete the
  inactive-bank write or use OTA.

## Error handling

| Failure | Action |
|---------|--------|
| HTTP connection failed | Abort, publish `"download_failed"` |
| Download interrupted | Abort, publish `"download_failed"`, inactive bank discarded |
| Verification failed | Abort, publish `"verify_failed"`, do not apply — manifest mismatch, unreadable bank, or no vector table for the target bank (UART `vector table not found in bank`, e.g. an image linked for the other bank) |
| Image too large | Abort mid-download or at the post-download size check, publish `"image_too_large"` |
| Post-apply crash loop | Bootloader bank toggle after 3 strikes; UART recovery if both slots fail |

## UART0 recovery (last resort)

- UART0 (GPIO21 TX, GPIO22 RX) always available for serial flash programming.
- Application CLI commands (`bank`, `wifi`) are defined in
  [uart-console.md](uart-console.md).
- MAC is in efuse; Wi-Fi config set at runtime — fully corrupted flash is
  recoverable without losing device identity.
- Not an OTA path; requires physical access to module pads.
