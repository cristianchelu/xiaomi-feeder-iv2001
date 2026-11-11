# LAN web UI client

serves:
  - ../20-stories/scheduling.md
  - ../20-stories/connectivity.md
  - ../20-stories/feeding.md

## Purpose

The admin panel is a single-page browser client over the HTTP API in
[web-ui.md](web-ui.md). POST bodies and read parsing match
[mqtt-protocol.md](mqtt-protocol.md). Server-side routing and validation are
tested in `test_web_api.c` and `test_schedule_cmd.c`; this document covers
**client-only** behavior in `tools/web/`.

## Source layout

| File | Role |
|------|------|
| `tools/web/index.html` | Markup, styles, DOM event wiring |
| `tools/web/logic.mjs` | Pure helpers (weekday masks, JSON bodies, formatters) |
| `tools/web/build.sh` | Inlines `logic.mjs`, minifies, gzip, emit `web_ui_gz.c` |
| `tools/web/test_logic.mjs` | Host tests (`node --test`) |

`build.sh` strips `export` keywords so inlined functions are global inside the
page `<script>`. The flash image still receives one gzipped HTML file.

Styles in `index.html` use **native CSS nesting** for smaller gzipped output
(see `tools/web/README.md` § CSS: native nesting). Do not flatten to repeated
flat selectors or switch to a CSS preprocessor without measuring `build.sh`
byte counts.

## Weekday model

| UI | Wire format |
|----|-------------|
| Day toggles labelled M–S (single letter, Mon=0 … Sun=6) | `repeat_days` integer array on `POST /api/schedule/set` |
| Index `0` = Monday, `6` = Sunday | Same as MQTT `cmd/schedule/set` and UART bitmask semantics |

The UI uses a 7-bit mask internally (`1 << day_index`). `maskToIndices(mask)`
produces the sorted `repeat_days` array for the POST body.

## Pure helpers (`logic.mjs`)

| Function | Use |
|----------|-----|
| `DAYS` | Weekday labels for the checkbox row |
| `indicesToMask(indices)` | Bitmask from selected indices |
| `maskToIndices(mask)` | `repeat_days` array from bitmask |
| `readMaskFromCheckedValues(values)` | Bitmask from checkbox `value` strings |
| `validateScheduleMask(mask)` | `{ok, error?}` — requires non-zero mask before save |
| `buildScheduleSetBody({hour, min, mask, g, enabled})` | `POST /api/schedule/set` object |
| `buildScheduleDeleteBody(hour, min)` | `POST /api/schedule/delete` |
| `buildScheduleToggleBody(hour, min)` | `POST /api/schedule/toggle` |
| `buildScheduleSkipBody(hour, min, skip)` | `POST /api/schedule/skip` |
| `buildDispenseBody(g)` | `POST /api/dispense` |
| `buildScheduleEnableBody(enabled)` | `POST /api/schedule/enable` |
| `buildScheduleTodayBody(enabled)` | `POST /api/schedule/today` |
| `buildConfigTzBody(tz_rule)` | `POST /api/config` time slice |
| `buildFeedOverfillBody({enabled, threshold_g})` | `POST /api/feed/overfill` |
| `formatNextFeedParts(next)` | Two-line next feed (`Next feed · in N min` / `HH:MM · Ng`) |
| `formatDeviceTime(st)` | Feeder `local_time` in header (or `—` if unsynced) |
| `formatStatusAlerts(st)` | Problem badges (time sync, bowl error) |
| `statusStateRows(st)` | Current-state label/value rows for the Now tab |
| `formatBowlWeightWire(wire)` | MQTT `bowl_weight` string → display |
| `sortSlotsByTime(slots)` | Client-side card sort by clock time |
| `ALL_DAYS_MASK` | Default day mask when opening **+ Add feed** |
| `formatSlotClock(time)` | Zero-padded `HH:MM` for `<input type="time">` value |
| `formatSlotSummary(time, g)` | `HH:MM · Ng` headline for schedule cards (matches Next feed detail) |
| `weekdayFromLocalTime(local_time)` | Mon=0 … Sun=6 from feeder `local_time` date |
| `slotDayBadges(repeat_days, todayIndex?)` | Per-day `{letter, label, on, today}` for schedule cards |
| `dayDotClass(d)` | CSS classes for a day-dot badge |
| `slotCardMuted(slot)` | `true` when card body should mute (`!enabled` or `to_be_skipped`) |
| `formatSlotState(state)` / `slotStateTone(state)` | Slot state badge |
| `slotSkipControl(slot, local_time)` | Skip-today icon (`null` hides control); requires `enabled`, `today`, not past, not terminal |
| `parseApiResponse(text)` | JSON object or plain string (feed mode GET) |
| `apiContentType(body)` | `application/json` vs `text/plain` for POST |
| `mutationMessage(r, okText, failPrefix?)` | User-visible result from `{ok, error?}` |
| `TIMEZONES` | `{label, rule}` presets for the timezone dropdown (POSIX rules, e.g. Paris `CET-1CEST,M3.5.0,M10.5.0/3`) |
| `timezoneLabel(rule)` | Preset label for a stored rule, or `Custom` |
| `formatProgress(p)` | Live dispense line from `progress` (`Dispensing 2/5 · 17.3 g`, `Weighing…`), `null` when idle |
| `formatRssi(dbm)` | `-55 dBm · good` (≥ −60 good, ≥ −70 fair, else weak), `—` when unknown |
| `withKey(body, password)` | Prefixes `key=<password>\n` when a web password is stored |
| `buildNetworkBody(fields)` | `POST /api/network` object with only the non-empty fields (`mqtt_port` as a number) |
| `buildDisplayBody(fields)` | `POST /api/display` object (`mode`, `brightness`, `battery_off_s`, `mains_off_s`) |

## Page layout

Sticky tab bar (**Home** · **Schedule** · **Settings** · **System** · **Help**); one pane
visible at a time (CSS `:checked` on hidden radios — no framework). Each pane is
a column of titled cards; every card saves its own fields with one button.

### Home tab

| Block | Source |
|-------|--------|
| Next feed | `/api/status` `next` — two-line hero (`formatNextFeedParts`) |
| Problem alerts | `/api/status` — time not synced, `bowl_error` |
| Problem banner | `problem` — red card `E1 E3 — <text>` with **Clear** (`POST /api/problem_clear`), hidden when inactive |
| Live dispense | `progress` (`formatProgress`), shown only while a dispense runs |
| **Current state** grid | `/api/status` telemetry fields — see below |
| Feed now | `POST /api/dispense`; the grams field starts at `dispense_g` (or 10) |

**Current state** rows (`statusStateRows`) expect these `/api/status` fields:

| Row label | Field | Display helper |
|-----------|-------|----------------|
| Food in bowl | `bowl_weight` | `formatBowlWeightWire` |
| Hopper | `hopper` | `formatHopperLevel` |
| Activity | `dispense_busy` | `formatBusy` |
| Bowl | `bowl_error` | `formatBowlHealth` |
| Battery | `battery` (optional) | `formatBatteryWire` |
| Power | `mains` (optional) | `formatMainsWire` |

Plus **Eaten today** (`eaten_today`) with a Reset button.

Poll `/api/status` every 5 s (every 1 s while `progress.state` is not `idle`).

### Schedule tab

| Block | Behavior |
|-------|----------|
| Master toggles | Schedule / Today |
| Feed next meal | `POST /api/schedule/feed_next`; refusal shown in words (no meal left today, daily limit, bowl full, busy) |
| **+ Add feed** | Opens bottom-sheet editor with empty form |
| Slot cards | `HH:MM · Ng` headline · day letters · **`state` from API**; `.slot-main` at 55% opacity when disabled or `to_be_skipped`; row actions stay full contrast |
| Day letters | Off days plain text; scheduled days filled accent dot; **today** when off gets bordered badge |
| Row actions | Enable switch (`.tog`), **skip-today** icon (active when `to_be_skipped`), delete |
| Add/edit sheet | Hidden until **+ Add feed** or pen on a card; Save upserts by time; Cancel closes without save |
| Slot enable/disable | Card enable switch only — not in the editor |

**Add/edit sheet fields** (no enabled toggle; new slots save as enabled, edits preserve existing enabled state):

| Field | Control |
|-------|---------|
| Time | Required `<input type="time">` |
| Days | M–S toggles; hidden `#daysGate` input (`required`) synced from bitmask |
| Amount | Grams (5–150) |

### Settings tab

| Card | Fields |
|------|--------|
| Feeding | Feed mode; HA Dispense-button amount in grams (empty = one portion, placeholder shows the portion size in g) (`POST /api/dispense_amount`); daily limit and not-eaten alert (`POST /api/feed_limit`, `formatHistoryRow`-independent); grams dispensed today; child lock (saved on toggle) |
| Overfill protection | Toggle + 30–500 g threshold (threshold disabled when off) |
| Portion | Portion size and calibration status; Calibrate / Reset to 10 g |
| Screen | Mode, brightness, screen-off delay on battery and on mains (seconds, 0 = never) |
| Scale | Calibration status; Zero (bowl removed) / Span (empty bowl) |
| Battery | Wi-Fi on battery |

### System tab

| Card | Fields |
|------|--------|
| Time | Feeder time; timezone dropdown (`TIMEZONES` + Custom POSIX rule); NTP server |
| Network | Wi-Fi SSID / password, MQTT host / port / user / password — current values prefilled except passwords (blank keeps the stored one); Wi-Fi signal (`formatRssi`). Saving any field other than NTP reboots; a failing new Wi-Fi falls back to the previous one |
| Security | Web password set / change / remove |
| Backup | Download settings (`GET /api/export`), restore from a file (`POST /api/import`) |
| Firmware | Version (`fw`), active bank, update state/progress (`ota`), firmware folder URL (`POST /api/ota_base`), **Install firmware update** (`POST /api/ota_install`) |
| Device | Reboot; Wi-Fi setup again (`/api/reprovision`); Factory reset (`/api/factory_reset`, typed confirmation `RESET`) |
| Feed history | `GET /api/history` when the tab opens or on Refresh; one line per meal (`formatHistoryRow`: when · source · wanted → got, result badge, holes/time/current and failure notes) |
| Diagnostics | Health counters (`health`, `formatUptime`); recent log (`GET /api/log`) on demand |

### Edited fields

A field the user has changed is *dirty*: status refreshes never overwrite it
until its card is saved (then it takes the device value again) or the page is
reloaded. Untouched fields follow the device.

### Language

All labels exist in English and French. The language is `status.lang`; when
unset, French if the browser language starts with `fr`, else English. A
selector in **System → Language** saves it (`POST /api/lang`) for every
browser.

### Help tab

Static text in the page language: how a meal works (schedule, modes, portion
learning, overfill, daily limit, not-eaten alert, calibrations), the slot
states, and every problem code (E1–E9) with what to do.

### Password

Reads use GET while no password is stored and POST with the `key=` line once
one is. When `/api/status` answers `{"locked":true}` or `unauthorized`, the
page shows a lock screen over every tab; a password accepted by
`POST /api/unlock` is stored and the page reloads its data.

When a POST returns `unauthorized`, the page asks for the web password,
stores it in `localStorage` (wrapped in `try/catch`; memory only when storage
fails) and retries once. Every POST goes through `withKey`.

### Chrome

Header: title · feeder `local_time` (`formatDeviceTime`) · refresh control.

## Client validation

| Rule | When | Message |
|------|------|---------|
| Time + grams | Save slot | Native `required` / `min` / `max`; invalid fields use `:user-invalid` styling |
| At least one weekday | Save slot | Hidden `#daysGate` cleared when day mask is zero |

Server validation and NVDM writes are unchanged; failed POSTs show `error` from
the JSON response in the toast (`ok` / `err` classes).

## Test scope

| Layer | Command | Covers |
|-------|---------|--------|
| HTTP API / schedule commands | `make test-host` | Routes, firmware JSON handlers, `schedule.c` |
| Client logic | `make test-web` | Masks, POST body shapes, formatters, `parseApiResponse` |
| Browser / device | `make preview-web` or manual | Layout, `fetch` against mock or live device |

Host client tests use Node built-in `node:test` only (no npm). Local UI preview:
`make preview-web` serves the bundled page with an in-memory mock API at
`http://127.0.0.1:8765/` (`tools/web/dev_server.mjs`, `mock_api.mjs`). The mock
API derives slot `state` in `mock_api.mjs` only (not `logic.mjs` / not flashed) to
mirror [scheduler-engine.md](scheduler-engine.md); the device UI renders whatever
`/api/schedule/state` returns. See
[build-integration.md](../40-architecture/build-integration.md).

## Build

`logic.mjs` is not served separately. `build.sh` substitutes the
`<!-- INJECT_LOGIC -->` marker in `index.html` with stripped `logic.mjs`, runs
`html-minifier-next` on the full page (JS identifier mangling via terser, CSS
via nesting-aware `lightningcss`, HTML whitespace), then `gzip -9`. Not
`html-minifier-terser`: dormant since 2023, its clean-css pass breaks native
CSS nesting. Sources in git stay readable; minification applies
only to the ephemeral firmware bundle. Set `WEB_UI_SKIP_MINIFY=1` to skip the
minifier step. Output: gitignored `firmware/src/web_ui_gz.c`.

The LinkIt SDK invokes bare `make` in `firmware/GCC/` (no explicit target).
When `WEB_UI_ENABLE=y`, the `web_ui_gz.c` generation rule must not become the
default goal or the app never links and stale UI bytes remain in flash.
`firmware/GCC/Makefile` sets `.DEFAULT_GOAL := all`, passes `all` to the
bank-B sub-make, and drops `web_ui_gz.o` when HTML sources change.

`make preview-web` and `dev_server.mjs` serve the **unminified** inline bundle
for browser debugging.
