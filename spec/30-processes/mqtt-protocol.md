# MQTT protocol

serves:
  - ../20-stories/connectivity.md

## Topic namespace

Base: `petfeeder/<device_id>/` where `<device_id>` is user-configurable
(default: last 6 hex chars of MAC, e.g. `petfeeder/a4cf12`).

## State topics (published by device, retained)

| Topic | Payload | QoS |
|-------|---------|-----|
| `.../connection` | `online` or `offline` (plain text) | 1 |
| `.../state` | `{"bowl_error": false}` — device condition (faults / health); see [Device condition](#device-condition) | 1 |
| `.../bowl_weight` | `42.3` — plain string, one decimal (0.1 g); empty string when unknown; see [Bowl weight](#bowl-weight) | 1 |
| `.../hopper` | `normal` \| `low` \| `empty` (plain text) | 1 |
| `.../hopper_ir` | `blocked` \| `clear` (plain text) — raw IR beam; see [Hopper](#hopper) | 1 |
| `.../battery` | `75` — plain integer 0–100; `unknown` when absent; see [Battery](#battery) | 1 |
| `.../battery_voltage` | `5200` — plain integer pack mV; see [Battery pack voltage](#battery-pack-voltage) | 1 |
| `.../mains` | `ON` or `OFF` — barrel connected; see [Mains](#mains) | 1 |
| `.../schedule/state` | Schedule document JSON — see [Schedule](#schedule) | 1 |
| `.../schedule/next` | `{"hour":8,"min":0,"g":30,"in_min":120}` | 1 |
| `.../config` | Config snapshot JSON — see [Config snapshot](#config-snapshot) | 1 |
| `.../feed/mode` | `open_loop` or `compensated` (plain text) — see [Feed mode](#feed-mode) | 1 |
| `.../feed/overfill` | `{"enabled":false,"threshold_g":50}` — see [Overfill protection](#overfill-protection) | 1 |
| `.../child_lock` | `ON` or `OFF` (plain text) — see [Child lock](#child-lock) | 1 |
| `.../timezone` | Device timezone plain text — see [Device timezone](#device-timezone) | 1 |
| `.../display` | `{"mode": "weight", "brightness": 4, "battery_off_s": 60, "mains_off_s": 0}` | 1 |
| `.../feed/dispense_amount` | `{"g":25}` — Dispense-button amount, `0` = one portion; see [Dispense amount](#dispense-amount) | 1 |
| `.../dispense/progress` | `{"state":"running","portions":2,"of":5,"g":"17.3"}` — see [Dispense progress](#dispense-progress) | 0 |
| `.../wifi_rssi` | `-58` — plain integer dBm; checked every `[tune]` 60 s, published every `[tune]` 10 min or on a change ≥ `[tune]` 5 dB | 0 |
| `.../problem` | Problem state — see [health-monitoring.md](health-monitoring.md) § Problem state | 1 |
| `.../feed/limit` | `{"daily_max_g":0,"today_g":45,"not_eaten_h":0}` — daily limit and not-eaten alert | 1 |
| `.../eaten_today` | `85.3` — plain string, one decimal (0.1 g); see [Eaten today](#eaten-today) | 1 |
| `.../ota/status` | `{"state": "idle", "pct": 0, "error": "", "bank": "A"}` — see [OTA status](#ota-status) | 1 |

## Event topics (published by device, not retained)

| Topic | Payload | QoS | Retain |
|-------|---------|-----|--------|
| `.../dispense/event` | Dispense completion JSON — see [Dispense event](#dispense-event) | 1 | false |

Completion (grams, outcome) is reported only via the fire-and-forget dispense
event; the retained `.../dispense/progress` topic is a best-effort live view.

## Command topics (subscribed by device, not retained)

| Topic | Payload | QoS |
|-------|---------|-----|
| `.../cmd/dispense` | `{"g": 30}`; an empty payload dispenses the [Dispense amount](#dispense-amount) (one portion when unset) | 1 |
| `.../cmd/dispense/cancel` | `{}` | 1 |
| `.../cmd/schedule/set` | `{"hour":8,"min":0,"repeat_days":[0,1,2,3,4,5,6],"g":30,"enabled":true}` | 1 |
| `.../cmd/schedule/delete` | `{"hour":8,"min":0}` | 1 |
| `.../cmd/schedule/toggle` | `{"hour":8,"min":0}` | 1 |
| `.../cmd/schedule/skip` | `{"hour":8,"min":0,"skip":true}` | 1 |
| `.../cmd/schedule/feed_next` | (empty) — serve the next meal now ([scheduler-engine.md](scheduler-engine.md) § Feed next meal) | 1 |
| `.../cmd/schedule/enable` | `{"enabled":true}` | 1 |
| `.../cmd/schedule/today` | `{"enabled":true}` | 1 |
| `.../cmd/calibrate` | `{"action": "zero"}` or `{"action": "span"}` — see [Calibrate command](#calibrate-command) | 1 |
| `.../cmd/display` | `{"mode": "weight", "brightness": 4}` | 1 |
| `.../cmd/feed/mode` | `open_loop` or `compensated` (plain text) — see [Feed mode](#feed-mode) | 1 |
| `.../cmd/feed/overfill` | `{"enabled":true}` and/or `{"threshold_g":40}` — see [Overfill protection](#overfill-protection) | 1 |
| `.../cmd/child_lock` | `ON` or `OFF` (plain text) — see [Child lock](#child-lock) | 1 |
| `.../cmd/config` | `{"tz_rule":"...", "tz_label":"..."}` — **legacy** time slice only; see [Config snapshot](#config-snapshot) | 1 |
| `.../cmd/reboot` | `{}` — restart the device ([Reboot command](#reboot-command)) | 1 |
| `.../cmd/ota` | `{"url": "http://...", "sha512": "<128 hex chars>"}` — `sha512` optional; or `{"install":true}` ([One-button install](ota-flow.md#one-button-install)) | 1 |
| `.../cmd/ota_base` | Plain-text `http(s)://host[:port]/dir` firmware folder; stored in NVDM `system/ota_base`, retained on `.../ota/base` | 1 |
| `.../cmd/eaten_reset` | `{}` — zero today's eaten total | 1 |
| `.../cmd/portion_cal` | `{}` — turn 3 portions open-loop, weigh once, set grams per portion; `{"action":"reset"}` → 10 g. Result retained on `.../portion` `{"g":"8.5","status":"ok"\|"failed"\|"default"}` | 1 |
| `.../cmd/power` | `{"battery_wifi":"on"\|"off"\|"scheduled_only"}`; retained on `.../power` | 1 |
| `.../cmd/problem_clear` | (empty) — clears latched problems | 1 |
| `.../cmd/feed_limit` | `{"daily_max_g":N}` and/or `{"not_eaten_h":N}` | 1 |
| `.../cmd/dispense_amount` | `{"g": 25}` (5–150; `0` = one portion) — see [Dispense amount](#dispense-amount) | 1 |

### Reboot command

`cmd/reboot` restarts the whole device. Payload is ignored (`{}`).

| Condition | Result |
|-----------|--------|
| Dispense active | Rejected (`PORT_ERR_BUSY`), no reboot |
| OTA active | Rejected (`PORT_ERR_BUSY`), no reboot |
| Otherwise | Accepted; reboot happens `[tune]` 500 ms after the next display tick (lets the outbox flush and logs drain) |

A second `cmd/reboot` while one is pending is accepted and does not extend
the deadline. Configuration is untouched (this is not a factory reset).
Not exposed on the web API (MQTT only, see [web-ui.md](web-ui.md)).

## Last Will and Testament (LWT)

| Parameter | Value |
|-----------|-------|
| Will topic | `.../connection` |
| Will payload | `offline` |
| Will retain | true |
| Will QoS | 1 |

Set at CONNECT time. Broker publishes will on unclean disconnect. On
graceful disconnect the firmware publishes `offline` on `.../connection`
before `DISCONNECT`. `.../state` is not used for presence.

## Home Assistant discovery

On connect (and every `[tune]` 300 s when the full entity table ships),
publish discovery configs to:
`homeassistant/<component>/petfeeder_<device_id>/<object_id>/config`
(retained, QoS 1). Post-connect discovery publishes go through
[mqtt_outbox](#publish-path) — only `mqtt_io` drains the queue.

### Discovery pacing

The entity table can hold more configs than the outbox has slots, and the
connect snapshots share the same ring. Discovery is therefore enqueued in
the background, not as one burst:

- On `EVT_MQTT_CONNECTED` the `app` task arms discovery with the device id;
  nothing is enqueued yet.
- On every `EVT_DISPLAY_TICK` while armed and the outbox is accepting, `app`
  enqueues the next entity of the table (table order) if the outbox holds
  fewer than `[tune]` 12 pending items. At most one entity is enqueued per
  tick.
- After the last entity the module disarms. A new `EVT_MQTT_CONNECTED`
  re-arms from the first entity.
- When the outbox stops accepting (disconnect, OTA suspend) discovery
  disarms; the next connect re-arms it.
- A failed enqueue (payload formatting or full ring) retries the same entity
  on the next tick.

State and command-reply publishes enqueued by `app` keep at least
`[tune]` 12 free slots (ring depth 24 minus the pacing threshold) while
discovery is running.

Each discovery message includes a `device` block with `identifiers`,
`name`, `manufacturer`, `model`, `sw_version` (firmware `git describe`) and,
once the station has an IP address, `configuration_url` `http://<ip>` (the
feeder web page, shown as "Visit" on the HA device page) for unified HA device
grouping.
`manufacturer` and `model` identify the **physical hardware** (retail
product), not the firmware author — see [validation slice](#home-assistant-validation-slice).

### Entity categories

Discovery payloads set Home Assistant `entity_category` where applicable.
Primary entities omit the field; they appear on the main device card and may
be auto-added to dashboards.

| Category | Entities |
|----------|----------|
| (primary controls) | `dispense`, `problem_clear`, `dispense_amount`, `feeding_schedule`, `child_lock` |
| (primary sensors) | `problem`, `bowl_weight`, `eaten_today`, `bowl_error`, `battery`, `mains`, `hopper_level`, `portion_size`, `dispense_progress`, `dispense_completed` |
| `config` | feeding, overfill, screen, scale, portion, battery Wi-Fi and firmware settings |
| `diagnostic` | `battery_voltage`, `device_timezone`, `wifi_rssi`, `hopper_ir`, `ota_state`, `firmware_bank` |

`dispense_custom` is retired: the device publishes an empty retained config
for it so Home Assistant removes the entity.

Read-only sensors use `diagnostic`, not `config` — Home Assistant rejects
passive sensors with `entity_category: config`. Writable components (switch,
number, select) use `config` for tuning entities.

When `entity_category` changes on a broker that Home Assistant already
discovered, republishing discovery alone does not move existing entities —
clear the config topic (empty retained payload) and republish, or use
`tools/mqtt/mqtt-bench.sh ha rediscover` / `ha rediscover-categories`.

### Full entity table

| Component | object_id | HA device_class | entity_category | Notes |
|-----------|-----------|-----------------|-----------------|-------|
| sensor | bowl_weight | `weight` | — | Unit: g |
| sensor | eaten_today | `weight` | — | Unit: g |
| sensor | battery | `battery` | — | Unit: % |
| sensor | hopper_level | `enum` | — | Options: normal, low, empty |
| binary_sensor | mains | `power` | — | Mains connected — `ON`/`OFF` on `.../mains` |
| button | dispense | — | — | Triggers default portion |
| event | dispense_completed | — | — | Fires on each dispense job completion |
| number | dispense_amount | — | — | "Dispense amount", 5–150 g, `box` mode; stored amount used by the Dispense button (`.../feed/dispense_amount`) |
| sensor | dispense_progress | — | — | "Dispense status": `2/5 (17.3 g)` while running, else `idle` / `weighing` |
| switch | feeding_schedule | — | — | On = global schedule enabled |
| switch | weight_compensation | — | `config` | On = compensated dispense mode |
| switch | overfill_protection | — | `config` | On = skip scheduled feeds when bowl ≥ threshold |
| number | overfill_threshold_g | — | `config` | Range: 30–500 g |
| switch | child_lock | — | — | On/off |
| select | display_mode | — | `config` | Options: weight, eaten_today |
| number | display_brightness | — | `config` | Range: 0–4 (0 = panel off) |
| number | display_battery_off | — | `config` | "Screen off delay (battery)", seconds 0–600 (0 = never) |
| number | display_mains_off | — | `config` | "Screen off delay (mains)", seconds 0–3600 (0 = never) |
| button | reboot | `restart` | `config` | `cmd/reboot` |
| button | calibrate_zero | — | `config` | `cmd/calibrate {"action":"zero"}` — bowl removed |
| button | calibrate_span | — | `config` | `cmd/calibrate {"action":"span"}` — empty provided 350 g bowl installed |
| button | eaten_reset | — | `config` | `cmd/eaten_reset` |
| sensor | portion_size | — | — | Grams per portion (`.../portion`, `value_json.g`) |
| button | portion_cal | — | `config` | `cmd/portion_cal {}` |
| select | battery_wifi | — | `config` | Options: on, off, scheduled_only (`power/battery_wifi`) |
| text | ota_base | — | `config` | Firmware folder URL (≤ 120 chars) |
| button | ota_install | `update` | `config` | `cmd/ota {"install":true}` |
| sensor | hopper_ir | `enum` | `diagnostic` | Raw IR beam: `blocked` \| `clear`; `enabled_by_default`: false |
| sensor | wifi_rssi | `signal_strength` | `diagnostic` | "Wi-Fi signal", unit dBm |
| binary_sensor | problem | `problem` | — | "Problem"; attributes from `.../problem` |
| button | problem_clear | — | — | "Clear problem" → `cmd/problem_clear` |
| button | feed_next | — | — | "Feed next meal" → `cmd/schedule/feed_next` |
| number | daily_limit | — | `config` | "Daily limit", g, 0–2000 step 10 (0 = off) |
| number | not_eaten_h | — | `config` | "Not eaten alert", h, 0–24 (0 = off) |
| sensor | ota_state | — | `diagnostic` | `value_json.state` of `.../ota/status` (idle, downloading, …, error) |
| sensor | firmware_bank | — | `diagnostic` | `value_json.bank` of `.../ota/status` (`A` or `B`) |
| sensor | battery_voltage | `voltage` | `diagnostic` | Unit: mV; `enabled_by_default`: false |
| sensor | device_timezone | — | `diagnostic` | POSIX or label string |

### Home Assistant validation slice

The firmware publishes HA entities incrementally before the full table lands.

**Dispense button** (shipped):

| Field | Value |
|-------|-------|
| Discovery topic | `homeassistant/button/petfeeder_<device_id>/dispense/config` |
| `command_topic` | `petfeeder/<device_id>/cmd/dispense` |
| `payload_press` | `{}` |
| `availability_topic` | `petfeeder/<device_id>/connection` |
| `payload_available` | `online` |
| `payload_not_available` | `offline` |
| `device.identifiers` | `["petfeeder_<device_id>"]` |
| `device.manufacturer` | `Xiaomi` |
| `device.model` | `Smart Pet Food Feeder 2` |

**Bowl error** binary sensor (validation slice):

| Field | Value |
|-------|-------|
| Discovery topic | `homeassistant/binary_sensor/petfeeder_<device_id>/bowl_error/config` |
| `state_topic` | `petfeeder/<device_id>/state` |
| `value_template` | `{{ value_json.bowl_error }}` |
| `payload_on` / `payload_off` | JSON booleans `true` / `false` (not default `ON`/`OFF`) |
| `device_class` | `problem` |
| `availability_topic` | `petfeeder/<device_id>/connection` |
| `payload_available` | `online` |
| `payload_not_available` | `offline` |

Bench payloads: `tools/mqtt/payloads/ha-bowl_error.json`.

**Bowl weight** sensor (validation slice):

| Field | Value |
|-------|-------|
| Discovery topic | `homeassistant/sensor/petfeeder_<device_id>/bowl_weight/config` |
| `state_topic` | `petfeeder/<device_id>/bowl_weight` |
| `unit_of_measurement` | `g` |
| `device_class` | `weight` |
| `state_class` | `measurement` |
| `availability` | Dual — `.../connection` (`online` / `offline`) **and** `.../state` (`value_template`: `{{ value_json.bowl_error == false }}`, `payload_available`: `true`, `payload_not_available`: `false`) |

No `value_template` on the sensor — the topic payload is the gram reading directly.

Bench payloads: `tools/mqtt/payloads/ha-bowl_weight.json`, `bowl_weight-42`, `bowl_weight-empty`.

**Dispense completed** event (validation slice):

| Field | Value |
|-------|-------|
| Discovery topic | `homeassistant/event/petfeeder_<device_id>/dispense_completed/config` |
| `name` | `Dispense completed` |
| `state_topic` | `petfeeder/<device_id>/dispense/event` |
| `event_types` | `["success", "underfill", "stuck", "empty_hopper", "aborted"]` |
| `availability_topic` | `petfeeder/<device_id>/connection` |
| `payload_available` | `online` |
| `payload_not_available` | `offline` |

Event payloads use HA MQTT event JSON on `state_topic` (non-retained). See
[Dispense event](#dispense-event). No post-connect snapshot — events are
ephemeral.

**Battery** sensor (validation slice):

| Field | Value |
|-------|-------|
| Discovery topic | `homeassistant/sensor/petfeeder_<device_id>/battery/config` |
| `name` | `Battery` |
| `state_topic` | `petfeeder/<device_id>/battery` |
| `unit_of_measurement` | `%` |
| `device_class` | `battery` |
| `state_class` | `measurement` |
| `availability` | Dual — `.../connection` (`online` / `offline`) **and** `.../battery` (`value_template`: `{{ 'true' if value != 'unknown' else 'false' }}`, `payload_available`: `true`, `payload_not_available`: `false`) |

No `value_template` on the sensor — payload is plain integer percentage, or `unknown` when unavailable.

**Battery pack voltage** sensor (validation slice, diagnostic):

| Field | Value |
|-------|-------|
| Discovery topic | `homeassistant/sensor/petfeeder_<device_id>/battery_voltage/config` |
| `name` | `Battery pack voltage` |
| `state_topic` | `petfeeder/<device_id>/battery_voltage` |
| `unit_of_measurement` | `mV` |
| `device_class` | `voltage` |
| `state_class` | `measurement` |
| `entity_category` | `diagnostic` |
| `enabled_by_default` | `false` |
| `availability_topic` | `petfeeder/<device_id>/connection` |
| `payload_available` | `online` |
| `payload_not_available` | `offline` |

Bench payload: `tools/mqtt/payloads/ha-battery_voltage.json`.

**Mains connected** binary sensor (validation slice):

| Field | Value |
|-------|-------|
| Discovery topic | `homeassistant/binary_sensor/petfeeder_<device_id>/mains/config` |
| `name` | `Mains connected` |
| `state_topic` | `petfeeder/<device_id>/mains` |
| `payload_on` | `ON` |
| `payload_off` | `OFF` |
| `device_class` | `power` |
| `availability_topic` | `petfeeder/<device_id>/connection` |
| `payload_available` | `online` |
| `payload_not_available` | `offline` |

**Hopper level** sensor (validation slice):

| Field | Value |
|-------|-------|
| Discovery topic | `homeassistant/sensor/petfeeder_<device_id>/hopper_level/config` |
| `name` | `Hopper level` |
| `state_topic` | `petfeeder/<device_id>/hopper` |
| `device_class` | `enum` |
| `options` | `normal`, `low`, `empty` |
| `availability_topic` | `petfeeder/<device_id>/connection` |
| `payload_available` | `online` |
| `payload_not_available` | `offline` |

No `value_template` on the sensor — payload is the level string directly.

**Device timezone** sensor (validation slice):

| Field | Value |
|-------|-------|
| Discovery topic | `homeassistant/sensor/petfeeder_<device_id>/device_timezone/config` |
| `name` | `Device timezone` |
| `state_topic` | `petfeeder/<device_id>/timezone` |
| `availability_topic` | `petfeeder/<device_id>/connection` |
| `payload_available` | `online` |
| `payload_not_available` | `offline` |
| `entity_category` | `diagnostic` |

No `device_class` — payload is a free-form string (`tz_label` when set, else
POSIX `tz_rule`; always non-empty due to UTC0 fallback).

Bench payload: `tools/mqtt/payloads/ha-device_timezone.json`.

**Feeding schedule** switch (validation slice):

| Field | Value |
|-------|-------|
| Discovery topic | `homeassistant/switch/petfeeder_<device_id>/feeding_schedule/config` |
| `name` | `Feeding schedule` |
| `state_topic` | `petfeeder/<device_id>/schedule/state` |
| `value_template` | `{{ 'ON' if value_json.enabled else 'OFF' }}` |
| `state_on` / `state_off` | `ON` / `OFF` |
| `command_topic` | `petfeeder/<device_id>/cmd/schedule/enable` |
| `payload_on` / `payload_off` | `{"enabled": true}` / `{"enabled": false}` |
| `json_attributes_topic` | same as `state_topic` |
| `json_attributes_template` | `{{ value_json \| tojson }}` |
| `force_update` | `true` |
| `optimistic` | `false` |
| `entity_category` | `config` |
| `availability_topic` | `petfeeder/<device_id>/connection` |
| `payload_available` | `online` |
| `payload_not_available` | `offline` |
| `unique_id` | `petfeeder_<device_id>_feeding_schedule` |

Entity `on`/`off` = global schedule enabled. Attributes expose the full JSON
document including `schedule[]` and `today_enabled`. Toggle publishes
`cmd/schedule/enable`.

Bench payloads: `tools/mqtt/payloads/ha-feeding_schedule.json`.

**Weight compensation** switch (validation slice):

| Field | Value |
|-------|-------|
| Discovery topic | `homeassistant/switch/petfeeder_<device_id>/weight_compensation/config` |
| `name` | `Weight compensation` |
| `state_topic` | `petfeeder/<device_id>/feed/mode` |
| `value_template` | `{{ 'ON' if value == 'compensated' else 'OFF' }}` |
| `state_on` | `ON` |
| `state_off` | `OFF` |
| `command_topic` | `petfeeder/<device_id>/cmd/feed/mode` |
| `payload_on` | `compensated` |
| `payload_off` | `open_loop` |
| `availability_topic` | `petfeeder/<device_id>/connection` |
| `payload_available` | `online` |
| `payload_not_available` | `offline` |
| `unique_id` | `petfeeder_<device_id>_weight_compensation` |
| `entity_category` | `config` |

Bench payload: `tools/mqtt/payloads/ha-weight_compensation.json`.

**Overfill protection** switch (validation slice):

| Field | Value |
|-------|-------|
| Discovery topic | `homeassistant/switch/petfeeder_<device_id>/overfill_protection/config` |
| `name` | `Overfill protection` |
| `state_topic` | `petfeeder/<device_id>/feed/overfill` |
| `value_template` | `{{ 'ON' if value_json.enabled else 'OFF' }}` |
| `command_topic` | `petfeeder/<device_id>/cmd/feed/overfill` |
| `payload_on` | `{"enabled": true}` |
| `payload_off` | `{"enabled": false}` |
| `entity_category` | `config` |

Bench payload: `tools/mqtt/payloads/ha-overfill_protection.json`.

**Overfill threshold** number (validation slice):

| Field | Value |
|-------|-------|
| Discovery topic | `homeassistant/number/petfeeder_<device_id>/overfill_threshold_g/config` |
| `name` | `Overfill threshold` |
| `state_topic` | `petfeeder/<device_id>/feed/overfill` |
| `value_template` | `{{ value_json.threshold_g }}` |
| `command_topic` | `petfeeder/<device_id>/cmd/feed/overfill` |
| `command_template` | `{"threshold_g": {{ value }}}` |
| `min` / `max` / `step` | 30 / 100 / 1 |
| `unit_of_measurement` | `g` |
| `entity_category` | `config` |

Bench payload: `tools/mqtt/payloads/ha-overfill_threshold_g.json`.

**Child lock** switch (validation slice):

| Field | Value |
|-------|-------|
| Discovery topic | `homeassistant/switch/petfeeder_<device_id>/child_lock/config` |
| `name` | `Child lock` |
| `state_topic` | `petfeeder/<device_id>/child_lock` |
| `state_on` | `ON` |
| `state_off` | `OFF` |
| `command_topic` | `petfeeder/<device_id>/cmd/child_lock` |
| `payload_on` | `ON` |
| `payload_off` | `OFF` |
| `availability_topic` | `petfeeder/<device_id>/connection` |
| `payload_available` | `online` |
| `payload_not_available` | `offline` |
| `unique_id` | `petfeeder_<device_id>_child_lock` |
| `entity_category` | `config` |

Bench payload: `tools/mqtt/payloads/ha-child_lock.json`.

`cmd/dispense` accepts `{}` (one default portion) or `{"g": <5–150>}` for
gram-targeted dispense. Invalid JSON or out-of-range `g` is rejected at info
with no dispense. UART logs use tag `dispense` — see
[app-logging.md](app-logging.md) § Dispense diagnostics. Entity availability
follows retained `.../connection`
(`online` / `offline`).

Discovery refresh every 300 s is deferred; the outbox path supports it when
the full table ships. Stale discovery topics from prior firmware builds are
not auto-deleted on `device_id` change.

## Connection

| Parameter | Source |
|-----------|--------|
| Broker host | NVDM `mqtt/host` (from provisioning) |
| Broker port | NVDM `mqtt/port` (default 1883) |
| Username | NVDM `mqtt/user` (empty = anonymous) |
| Password | NVDM `mqtt/pass` |
| TLS | NVDM `mqtt/tls` (default false); `true` is rejected at load until a TLS adapter exists |
| Client ID | `petfeeder_<device_id>` |
| Device ID | NVDM `mqtt/device_id` when set; otherwise last 6 hex chars of STA MAC |

### Session lifecycle

Bench and runtime behavior (UART details in
[uart-console.md](uart-console.md#mqtt-commands)):

| Phase | Behavior |
|-------|----------|
| Boot | `mqtt_io` task starts; if `mqtt/host` is stored, connect when Wi-Fi STA has DHCP (no `mqtt connect` needed). Otherwise remain idle until armed |
| `mqtt connect` (or provisioning success) | Arm client; connect when Wi-Fi STA has DHCP |
| `mqtt disconnect` | Disarm for this boot; no reconnect until `mqtt connect` or reboot |
| Successful CONNECT | Subscribe `cmd/#`; publish retained `online` on `.../connection`; LWT installed; `EVT_MQTT_CONNECTED` enqueues retained `.../state`, `.../ota/status` and the other snapshots, and arms paced HA discovery ([Discovery pacing](#discovery-pacing); drained by `mqtt_io`) |
| Connected idle | `mqtt_io` drains [mqtt_outbox](#publish-path) then `MQTTYield`; inbound commands are heap-copied to `app_event_q` and routed on `app` (see [Inbound commands](#inbound-commands)) |
| Session loss while armed | Exponential backoff reconnect (see below) |

### Session display

`mqtt_client_request_connect()` posts `EVT_MQTT_SESSION` (connecting) immediately.
The TCP/MQTT handshake runs on a short-lived **`mqtt_cn` worker** at priority
below `app`; `mqtt_io` (ABOVE_NORMAL) only starts the worker and polls
completion every `[tune]` 50 ms — it does not call `mqtt->connect()` inline.
Phase changes also sync at the end of each `mqtt_io` step. The `app` task maps
phase → `display_mqtt_indicator_*` — see
[app-event-loop.md](app-event-loop.md) and [display-presentation.md](display-presentation.md)
§ MQTT indicator.

While connecting, bowl-gram digits on the TM1637 continue to update at
`[tune]` 500 ms (same `app` display tick path as idle). Orange lightbar
blinks may skip occasional frames when WFCI `try_acquire` fails; weight
readings keep the last good sample on `PORT_ERR_BUSY`.

### Connect execution

| Step | Task | Behavior |
|------|------|----------|
| Arm + session sync | `mqtt_io` or `app` producer | `mqtt_client_request_connect()` sets pending flags and posts connecting phase |
| Handshake | `mqtt_cn` worker | `ConnectNetwork`, `MQTTConnect`, subscribe `cmd/#`, publish retained `connection`=`online`; `mqtt_adapter_yield` before worker exit |
| Post-connect enqueue | `app` on `EVT_MQTT_CONNECTED` | Enqueue retained `.../state`, `.../ota/status` and the other snapshots, arm paced HA discovery — no `mqtt_port->publish` from `app` |
| Poll | `mqtt_io` | `[tune]` 50 ms step delay while worker runs; on completion apply backoff or post `EVT_MQTT_CONNECTED` |
| Disarm | `mqtt_client_stop()` | Clears worker state; disconnect if session was up |

**Yield guard:** `mqtt_io` must not call `MQTTYield` while the `mqtt_cn`
worker holds the socket.  The step function gates yield behind
`!s_connect_worker_running && mqtt->is_connected()`.  Without this guard,
select/recv on the shared socket races with the worker's subscribe call and
causes subscribe timeouts or duplicate CONNACK processing. `[design]`

Task priorities and stack sizes: [task-model.md](../40-architecture/task-model.md).

Summary:

| Session phase | Lightbar |
|---------------|----------|
| Connecting (armed, connect in flight) | Orange inverted blink |
| Connected | Green steady on |
| Backoff after failed connect (armed) | Orange error pattern |
| Disarmed, suspended, or Wi-Fi not ready | Both off |

Subscription is a single wildcard `.../cmd/#` covering the nine command topics
below — not `#`, not other devices' namespaces.

**Implemented now:** connect/LWT/`connection` presence, subscribe, reconnect backoff,
command topic classification, OTA download via `cmd/ota` (HTTP + SHA-512 verify,
A/B bank swap, slot-health confirm), [mqtt_outbox](#publish-path)
(post-connect publish queue), device condition (`bowl_error` on `.../state`),
`bank` on every `ota/status`, HA validation-slice **Dispense** button, **Bowl
error** binary_sensor, **Bowl weight** sensor, **Battery** sensor, **Mains
connected** binary_sensor, and **Dispense completed** event discovery,
`cmd/dispense` → one portion, `.../dispense/event` on job completion,
`.../battery`, `.../mains`, `.../hopper`, and `.../timezone` telemetry publishers,
HA validation-slice **Hopper level** and **Device timezone** sensors.

**Partially implemented:** `.../bowl_weight` telemetry publisher (validation slice);
`.../config` time/TZ slice (`tz_rule`, `tz_label`, `time_synced`, `utc_epoch`);
`cmd/config` for `tz_rule` and `tz_label`.

**Implemented (validation slice):** schedule MQTT topics (`.../schedule/state`,
`.../schedule/next`, `.../cmd/schedule/*`), dedicated coalesced publish path,
and HA **Feeding schedule** switch. Idempotent `cmd/schedule/enable` and
`cmd/schedule/today` succeed without republishing when the value is unchanged.

**Not implemented yet:** remaining telemetry topics (`eaten_today`,
display), additional HA entities from the full table,
300 s discovery refresh, other non-dispense command handlers,
per-topic last-value-wins coalescing on the outbox.

### Publish path

Post-connect MQTT publishes use a ring-buffered **mqtt_outbox** owned by
`mqtt_io`:

| Phase | Who may call `mqtt_port->publish()` |
|-------|-------------------------------------|
| `mqtt_cn` handshake | Connect worker only (retained `connection`=`online`; subscribe; LWT set at CONNECT) |
| Connected session | **`mqtt_io` only** — `mqtt_adapter_yield` before each outbox drain; one item per step |
| `app` task | **Enqueue only** — copy topic and payload into the outbox |

| Parameter | Value |
|-----------|-------|
| Ring depth | `[tune]` 24 slots |
| Max payload per slot | `[tune]` 1024 B (HA discovery; feeding schedule switch at max `device_id` length) |
| Min interval between successful drains | `[tune]` 100 ms |

When the ring is full, new enqueues are dropped and a debug log is emitted.
While MQTT is suspended for OTA (`mqtt_client_suspend_for_ota`), the outbox
stops accepting enqueues — callers get a silent drop with pending count 0.
`mqtt_outbox` is cleared on disconnect. OTA status, HA discovery, and most
state publishers share this path.

### Schedule state publish path

`.../schedule/state` payloads can reach `[tune]` 4096 B (32 slots). This
exceeds the outbox slot limit and uses a dedicated coalesced buffer:

| Parameter | Value |
|-----------|-------|
| Buffer size | `[tune]` 4096 B (single static allocation) |
| Coalescing | Last-value-wins; rapid mutations overwrite pending JSON |
| Drainer | `mqtt_io` calls `mqtt_schedule_drain()` each step when dirty |
| Producer | `app` task only — format JSON into buffer and set dirty flag |

### Inbound commands

Broker → device commands do not use a separate inbox module. `mqtt_io`
receives in the MQTT yield callback, heap-copies topic and payload, posts
`EVT_MQTT_MESSAGE`, and returns. `app` runs `mqtt_route_classify` and
dispatches handlers — never inside the MQTT callback or `mqtt_io` step.

### Reconnect strategy

Exponential backoff on disconnect:

- Initial delay: `[tune]` 1 s.
- Multiply by 2 on each failure.
- Cap at `[tune]` 60 s.
- Reset to initial delay on successful connect.

**Broker wait.** A QoS 1 publish waits at most `[tune]` 4 s for the broker's
acknowledgement.

**Dead session.** The TCP link can die while the client library still reports
the session as connected (every publish then fails, nothing reconnects).
`[tune]` 3 consecutive failed publishes (outbox, schedule, any post-connect
TX) declare the session lost: log `[mqtt] link lost (<n> publishes failed):
reconnecting` once, disconnect, and reconnect with the backoff above. Any
successful publish resets the count. The adapter logs a failed publish at most
once per `[tune]` 60 s (`[mqtt] publish failed topic=<t> (<n> since last
report)`) so failures cannot flood the log ring.

## Rate limiting

| Context | Max publish rate | Source |
|---------|-----------------|--------|
| Outbox drain (all post-connect TX) | 1 publish per `[tune]` 100 ms | `[design]` |
| State topics (general, when implemented) | 1 per topic per `[tune]` 2 s | `[design]` |
| Dispense event | 1 per completed job; drop when MQTT offline | `[design]` |

The outbox drain interval spaces connect-time bursts (e.g. ten HA entities
≈ 1 s). Per-topic last-value-wins coalescing for high-rate state publishers
is deferred until those topics ship.

## Config snapshot

Topic `.../config` (retained, QoS 1). **Time/TZ fields only** in this slice.

| Field | Type | Semantics |
|-------|------|-----------|
| `tz_rule` | string | POSIX TZ from [scheduler-engine.md](scheduler-engine.md); default `"UTC0"` |
| `tz_label` | string | Display-only IANA name; `""` when unset; max 47 UTF-8 bytes |
| `time_synced` | bool | `true` after first successful NTP this boot |
| `utc_epoch` | int | Unix seconds from RTC when synced; `0` when unknown |

Example:

```json
{"tz_rule":"EET-2EEST,M3.5.0/3,M10.5.0/4","tz_label":"Europe/Bucharest","time_synced":true,"utc_epoch":1718841600}
```

Publish on boot (after NVDM load), first NTP success, periodic NTP re-sync,
UART `time set`, and MQTT connect snapshot.

### `cmd/config` (legacy time slice)

**Deprecated pattern.** `cmd/config` exists only for the early time/TZ
provisioning slice (`tz_rule`, `tz_label`). New settings use **one retained
state topic and one command topic per entity** (see `cmd/feed/mode`,
`cmd/schedule/*`, `cmd/display`). Do not add new keys to `cmd/config`.

Writable keys: `tz_rule`, `tz_label`. Unknown keys are rejected.
See [config-store.md](config-store.md).

| Key | Empty string |
|-----|--------------|
| `tz_label` | Clears the label (erases NVDM key; display falls back to `tz_rule`) |
| `tz_rule` | Resets to default UTC0 (erases NVDM key; scheduler uses UTC) |

Publishes updated retained `.../config` and `.../timezone` on success.

## Feed mode

Topic `.../feed/mode` (retained, QoS 1) reports the dispense compensation
mode from NVDM `feed/mode` (see [config-store.md](config-store.md) and
[dispense-cycle.md](dispense-cycle.md)).

| Payload | Meaning |
|---------|---------|
| `open_loop` | Motor runs planned bursts without weight-based correction |
| `compensated` | Closed-loop compensation after each batch |

Command topic `.../cmd/feed/mode` accepts the same plain-text payloads.
Invalid payloads are rejected at info with no NVDM change.

Publish state on boot (MQTT connect snapshot), `cmd/feed/mode` change, and UART
`feed mode` set when MQTT is configured.

## Overfill protection

Topic `.../feed/overfill` (retained, QoS 1) reports overfill settings from
NVDM `feed/overfill_enabled` and `feed/overfill_threshold_g` (see
[config-store.md](config-store.md) and [scheduler-engine.md](scheduler-engine.md)).

```json
{"enabled": false, "threshold_g": 50}
```

| Field | Type | Range |
|-------|------|-------|
| `enabled` | bool | — |
| `threshold_g` | uint16 | 30–500 (out of range rejected, never truncated) |

Command topic `.../cmd/feed/overfill` accepts a JSON object with either or
both fields (partial update). Invalid `threshold_g` is rejected with no NVDM
change. When `enabled` is true and a scheduled slot fires with known bowl
mass `>= threshold_g`, the slot becomes `skipped_full` without a dispense job.

Publish state on boot, command success, and UART `feed overfill` set when MQTT
is configured.

Home Assistant discovery publishes a **switch** (`overfill_protection`) bound
to `enabled` and a **number** (`overfill_threshold_g`) bound to `threshold_g`.
Both use `entity_category: config` — see [validation slice](#home-assistant-validation-slice).

## Child lock

Topic `.../child_lock` (retained, QoS 1) reports the child-lock state from
NVDM `feed/child_lock` (see [config-store.md](config-store.md) and
[button-handling.md](button-handling.md) § Child lock).

| Payload | Meaning |
|---------|---------|
| `ON` | Child lock active: every physical button gesture except the reset+dispense unlock combo is ignored |
| `OFF` | Child lock inactive |

Command topic `.../cmd/child_lock` accepts the same plain-text payloads;
leading and trailing whitespace is ignored and matching is case-sensitive.
Any other payload is rejected at info with no NVDM change. A command equal to
the current state succeeds without an NVDM write or a publish. A successful
command sets or clears the panel lock icon exactly as the physical combo does.

The lock gates physical buttons only. Dispense commands from MQTT, the LAN web
UI, UART, and the scheduler are not affected.

Publish state on boot (MQTT connect snapshot) and after every change,
whether it came from the physical combo or from `cmd/child_lock`.

Home Assistant discovery publishes a **switch** (`child_lock`) with
`entity_category: config` — see [validation slice](#home-assistant-validation-slice).

## Eaten today

Topic `.../eaten_today` (retained, QoS 1): grams eaten since local midnight
([eaten-today.md](eaten-today.md)), plain string with one decimal (`0.0`,
`85.3`). Published on MQTT connect and whenever the value changes (also on the
midnight reset to `0.0`).

## Calibrate command

`cmd/calibrate` runs the same capture as UART `weigh cal zero|span`
([weighing.md](weighing.md) § Calibration) and persists the result in NVDM.

| Action | Precondition | Effect |
|--------|--------------|--------|
| `zero` | Bowl **removed** | Captures `calib/zero` |
| `span` | Zero captured, provided 350 g bowl installed (empty) | Captures `calib/span_g` + `calib/span_raw` |

Rules:

- Payload is JSON with a string `action`; anything else (missing, unknown
  action, non-JSON) is rejected at info with no capture.
- Rejected with `PORT_ERR_BUSY` while a dispense or an OTA is active.
- `span` before `zero` is rejected by the driver (`PORT_ERR_INVALID_ARG`).
- Each capture is the **median of 5** consecutive CS1270 queries; a spread above
  25 raw counts (hand on the scale, bowl moving) is rejected with `PORT_ERR_BUSY`.
- `span` is rejected (`PORT_ERR_INVALID_ARG`) when it differs from `zero` by fewer
  than 50 raw counts (bowl not installed, or zero was taken with the bowl on).
- `zero` is rejected (`PORT_ERR_INVALID_ARG`) when a complete calibration exists
  and the new reading is above half the bowl step (bowl still installed).
- After any successful capture the app drops the cached bowl sample, restarts
  the auto-tare anchor, invalidates the eaten-today baseline and resamples
  immediately, so the panel shows `0g` and no calibration step counts as eaten.
- A capture error from the scale (rail off, no stable sample) is logged at
  info (`calibrate <action> failed`) and leaves NVDM unchanged.
- Success logs `calibrate zero ok` / `calibrate span ok`. The calibration
  state surfaces through the existing `bowl_error` and `bowl_weight` topics
  on the next weigh tick; no dedicated state topic.
- Not exposed on the web API (MQTT and UART only).

## Display

Topic `.../display` (retained, QoS 1) reports the persisted display settings
from NVDM `display/mode` and `display/brightness` (see
[config-store.md](config-store.md) and
[display-presentation.md](display-presentation.md) § Display modes):

```json
{"mode":"weight","brightness":4,"battery_off_s":60,"mains_off_s":0}
```

Command topic `.../cmd/display` takes a JSON object with either or both keys:

| Key | Values | Effect |
|-----|--------|--------|
| `mode` | `weight` \| `eaten_today` | What the panel shows. `off` is not a mode: set `brightness` 0 |
| `brightness` | integer 0–4 | TM1637 brightness; **0 = panel off** (rail powered down), 1–4 lit |
| `battery_off_s` | integer 0–600 | Seconds of inactivity before the panel blanks **on battery**; `0` disables ([display-presentation.md](display-presentation.md) § Idle blank on battery) |
| `mains_off_s` | integer 0–3600 | Same, **on mains**; default `0` (always on) |

Rules:

- Keys are validated **before** anything is applied: one invalid key rejects
  the whole command (info log, no NVDM write, no publish).
- A command that changes nothing succeeds without NVDM write or publish.
- Unknown keys are ignored. An empty or non-JSON payload is rejected.
- While the mode is `off` the panel stays dark for everything (child-lock
  icon, Wi-Fi, dispensing, MQTT indicators) **except** the OTA indicator,
  which always shows so a running update is visible; the panel blanks again
  when the OTA indicator stops.
- Publish the state on MQTT connect (snapshot) and after every change.
- Also settable from the web page (`POST /api/display`, [web-ui.md](web-ui.md)).

## Dispense amount

NVDM `feed/button_g` holds the amount the Dispense button (HA button, empty
`cmd/dispense`) delivers: 5–150 g, or absent / `0` for one portion. Set with
`cmd/dispense_amount {"g":N}` (out-of-range rejected); state retained on
`.../feed/dispense_amount` `{"g":N}`, published on connect and after a change.

## Dispense progress

`.../dispense/progress` (retained, QoS 0) carries
`{"state","portions","of","g"}`: `running` while the motor turns (`portions`
done of `of`, `g` = portions × portion size, one decimal), `weighing` while
the bowl settles, `idle` otherwise. Published only when the payload changes
and only while the outbox holds fewer than `[tune]` 8 items (progress never
competes with state or event messages); polled every display tick. Republished
on connect.

## Device timezone

Topic `.../timezone` (retained, QoS 1) reports the effective timezone string
for display and Home Assistant.

| Payload | When |
|---------|------|
| `tz_label` value | NVDM `time/tz_label` is set and non-empty |
| POSIX `tz_rule` | Label unset; e.g. `UTC0`, `EET-2EEST,M3.5.0/3,M10.5.0/4` |

The payload is always non-empty: missing or cleared `tz_rule` loads as
`UTC0` (see [config-store.md](config-store.md)).

Publish on boot (MQTT connect snapshot), `cmd/config` change, and UART
`time set` when MQTT is configured.

## Device condition

Topic `.../state` (retained, QoS 1) reports operational health — not presence
(`.../connection`) and not OTA slot metadata (`ota/status.bank`).

| Field | Type | When `true` |
|-------|------|-------------|
| `bowl_error` | bool | Weigh cal incomplete, span pending, or calibrated bowl missing (see [weighing.md](weighing.md) § Bowl presence) |

Publish on edge change and as a snapshot after MQTT connect. Not on every
weight sample tick.

Example payloads:

```json
{"bowl_error": false}
{"bowl_error": true}
```

Bench templates: `tools/mqtt/payloads/state-ok.json`,
`state-bowl-error.json`.

## Bowl weight

Topic `.../bowl_weight` (retained, QoS 1) reports **presented** food mass in
the bowl now (see [auto-tare.md](auto-tare.md) and [weighing.md](weighing.md) §
Presented bowl weight). Plain string with **one decimal place** (0.1 g
precision) — not JSON. Separate from `.../state` (health) and from future
`.../eaten_today` (cumulative consumption counter).

| Payload | Meaning |
|---------|---------|
| `42.3` | Calibrated, bowl present, valid sample — 42.3 g presented food |
| `0.0` | Empty bowl or small negative drift clamped to zero |
| `""` (empty) | Unknown — uncalibrated, bowl missing, no valid sample, or implausible reading |

Presentation rules match panel **weight** mode ([display-presentation.md](display-presentation.md)
§ Display modes) except MQTT publishes the **full** presented range (no 999 g
display cap) at 0.1 g precision. Raw `read_dg` is not published on this topic —
use UART `weigh read` for driver-level mass.

Publish triggers (not on every `[tune]` 500 ms weight sample):

| Trigger | Publish |
|---------|---------|
| MQTT connect | Retained snapshot (`N` or `""`) |
| Post-dispense resample | Force snapshot |
| Known grams change by ≥ `[tune]` 2 g | Yes |
| Known ↔ unknown transition | Yes |
| Stable within 2 g deadband | No |

Rate cap: at most **1 publish per `[tune]` 2 s** on this topic; coalesce rapid
changes to the latest value within the window. Post-dispense force bypasses the
coalesce delay.

Home Assistant marks the entity unavailable when `bowl_error` is `true` on
`.../state` (dual `availability` in discovery) so a stale retained gram value is
not shown as a live reading while the bowl is missing or cal is incomplete.

## Battery

Topic `.../battery` (retained, QoS 1) reports pack state of charge as a plain
integer percentage 0–100. Separate from `.../mains` (power source) and
`.../battery_voltage` (diagnostic raw mV).

| Payload | Meaning |
|---------|---------|
| `75` | 75 % remaining (from ADC + discharge curve) |
| `0` | Depleted but present pack (~≤ 4.0 V) |
| `unknown` | Unknown — pack ADC reading exactly 0 mV (no cells / open circuit) |

Use the literal `unknown` (not an empty payload): Mosquitto and most brokers
delete retained messages on zero-length publish, so HA would keep showing the
last numeric state.

Home Assistant marks the entity unavailable when the payload is `unknown`
(dual `availability` in discovery). The battery-topic availability entry must
use a template that renders the strings `true` or `false` — for example
`{{ 'true' if value != 'unknown' else 'false' }}` — because HA compares the
template result to `payload_available` / `payload_not_available` as strings.
A bare boolean expression such as `{{ value != 'unknown' }}` renders `True` or
`False` and does not match `"false"`, leaving the entity available with a
stale percentage.

Samples before MQTT topics are configured (timer tick before first connect) are
ignored so a pre-connect reading cannot be snapshotted as the connect retain.
On connect, ADC is force-sampled before the battery connect snapshot.

Publish triggers:

| Trigger | Publish |
|---------|---------|
| MQTT connect | Retained snapshot of last known state |
| Mains/battery transition | Force resample + publish |
| Known ↔ unknown transition | Yes |
| Percentage change ≥ `[tune]` 1 pt (when known) | Yes |
| Periodic ADC sample (no change) | No |

Sample interval from debounced power source ([battery-monitoring.md](battery-monitoring.md)):
`[tune]` 60 s on battery, `[tune]` 300 s on mains. ADC read skipped on
`PORT_ERR_BUSY` (motor running / WFCI contended).

When `read_battery_mv()` returns exactly **0 mV**, publish `unknown` on this
topic (not `"0"`). Values `> 0 mV` map through the discharge curve including 0 %.

Chemistry enum and knot tables: [battery-monitoring.md](battery-monitoring.md)
§ Chemistry and discharge curves.

## Battery pack voltage

Topic `.../battery_voltage` (retained, QoS 1) reports raw scaled pack voltage
in millivolts. Diagnostic / integrator topic — always publishes the ADC result
(including `0`).

| Payload | Meaning |
|---------|---------|
| `5200` | 5.2 V pack |

Publish triggers (same sample tick as [Battery](#battery)):

| Trigger | Publish |
|---------|---------|
| MQTT connect | Retained snapshot |
| Mains/battery transition | Force resample + publish |
| Change ≥ `[tune]` 10 mV | Yes |
| Periodic ADC sample (no change) | No |

Home Assistant discovers **Battery pack voltage** with `enabled_by_default`:
`false` — hidden until enabled in the UI.

## Hopper

### Raw IR beam (`hopper_ir`)

Topic `.../hopper_ir` (retained, QoS 1) mirrors the **last raw IR reading**
before debounce and the empty latch ([hopper-sensing.md](hopper-sensing.md)):
`blocked` (food in the beam path) or `clear` (beam unobstructed). Nothing is
published until the first successful reading. Published on connect (once a
reading exists) and whenever the raw reading changes — raw readings arrive on
the sensing schedule (post-dispense, debounce streaks, mains idle every 60 s),
so this is a diagnostic aid, not a faster level.

Topic `.../hopper` (retained, QoS 1) reports the published three-state hopper
level (see [hopper-sensing.md](hopper-sensing.md) `hopper_level`).

| Payload | Meaning |
|---------|---------|
| `normal` | IR beam blocked or hopper not latched empty |
| `low` | IR almost empty; hopper not latched empty |
| `empty` | Confirmed out-of-food or compensated underfill |

Publish on published-level edge change and as a connect snapshot after MQTT
connect (`mqtt_hopper_connect_snapshot`). Not tied to the IR background poll
interval.

## Mains

Topic `.../mains` (retained, QoS 1) reports whether the barrel adapter is
present (debounced P1.1 sense).

| Payload | Meaning |
|---------|---------|
| `ON` | Running on mains (barrel connected) |
| `OFF` | Running on battery |

Publish on debounced edge change and as a snapshot after MQTT connect. Not
tied to battery sample interval.

## Dispense event

Topic `.../dispense/event` (QoS 1, **not retained**). Published once per
terminal dispense job when MQTT is connected at completion. If MQTT is offline
when the job finishes, the event is **dropped** — no replay buffer.

Home Assistant discovers an **event** entity (`object_id`: `dispense_completed`,
`name`: `Dispense completed`). The UI shows the outcome above the fold as the
event type (e.g. **Dispense completed** → `stuck`). Automations trigger on
`event_type` (`success`, `stuck`, etc.) and read properties such as `grams`.

### Payload fields

| Field | Type | When present |
|-------|------|--------------|
| `event_type` | string | Always — terminal outcome: `success`, `underfill`, `stuck`, `empty_hopper`, `aborted` |
| `grams` | int | Always — clamped ≥ 0 in payload |
| `grams_estimated` | bool | Always |
| `target_g` | int | Always — portion mode: `portions × 10`; gram mode: request target |
| `source` | string | Always — `mqtt`, `uart`, `button`, `schedule` |
| `mode` | string | Always — `open_loop` or `compensated` |
| `batch_count` | int | Always — motor batches in job (1 in open-loop portion v1) |
| `slot_hour` | uint8 | When `source=schedule` — triggering slot hour |
| `slot_min` | uint8 | When `source=schedule` — triggering slot minute |

When `grams_estimated` is `true`, `grams` is a motor-based fallback
(`portions × 10`) because the scale read failed or `bowl_error` was active.
When `false`, `grams` is the measured bowl delta (`post_settle − baseline`).

Example (measured success):

```json
{
  "event_type": "success",
  "grams": 28,
  "grams_estimated": false,
  "target_g": 30,
  "source": "mqtt",
  "mode": "open_loop",
  "batch_count": 1
}
```

Example (estimated — bowl error):

```json
{
  "event_type": "success",
  "grams": 10,
  "grams_estimated": true,
  "target_g": 10,
  "source": "button",
  "mode": "open_loop",
  "batch_count": 1
}
```

Example (stuck):

```json
{
  "event_type": "stuck",
  "grams": 5,
  "grams_estimated": false,
  "target_g": 10,
  "source": "uart",
  "mode": "open_loop",
  "batch_count": 1
}
```

Negative raw bowl deltas are clamped to **0** in `grams`; the signed raw delta
feeds internal hopper-empty detection (see [dispense-cycle.md](dispense-cycle.md)).

Example (compensated underfill):

```json
{
  "event_type": "underfill",
  "grams": 18,
  "grams_estimated": false,
  "target_g": 30,
  "source": "mqtt",
  "mode": "compensated",
  "batch_count": 3
}
```

Example (scheduled feed):

```json
{
  "event_type": "success",
  "grams": 28,
  "grams_estimated": false,
  "target_g": 30,
  "source": "schedule",
  "mode": "compensated",
  "batch_count": 3,
  "slot_hour": 8,
  "slot_min": 0
}
```

## Schedule

Topic `.../schedule/state` (retained, QoS 1). Published on CRUD, enable/today/
skip/toggle commands, dispense lifecycle, midnight reset, minute rollover status
changes, and MQTT connect snapshot. Uses the [coalesced publish path](#schedule-state-publish-path).

### `schedule/state` payload

```json
{
  "enabled": true,
  "today_enabled": true,
  "schedule": [
    {
      "time": "08:00",
      "repeat_days": [0, 1, 2, 3, 4, 5, 6],
      "g": 30,
      "g_actual": 28,
      "enabled": true,
      "today": true,
      "state": "dispensed"
    }
  ]
}
```

| Field | Rule |
|-------|------|
| `enabled` | Mirrors NVDM `schedule/enabled` |
| `today_enabled` | RAM today override |
| `schedule[]` | Sorted by `time` ascending |
| `time` | `HH:MM` zero-padded, local civil; slot key with `hour`/`min` on commands |
| `repeat_days` | Weekday list (Mon=0 … Sun=6); `[]` = never repeat |
| `g` | Target grams |
| `g_actual` | Omitted when unknown |
| `today` | `true` when weekday matches and entry is visible in today view |
| `state` | Native status string (RAM-derived) |

Runtime status fields are RAM-only — see [scheduler-engine.md](scheduler-engine.md).

### Schedule commands

Rejected commands log at info and do not mutate state (same policy as
`cmd/config`). Successful mutations coalesce a `schedule/state` publish.
NVDM is written only when persisted fields change. `cmd/schedule/enable` and
`cmd/schedule/today` succeed without republishing when the requested value
already matches RAM/NVDM state.

| Topic | Payload |
|-------|---------|
| `cmd/schedule/set` | `{"hour":8,"min":0,"repeat_days":[0,1,2,3,4,5,6],"g":30,"enabled":true}` |
| `cmd/schedule/delete` | `{"hour":8,"min":0}` |
| `cmd/schedule/toggle` | `{"hour":8,"min":0}` |
| `cmd/schedule/skip` | `{"hour":8,"min":0,"skip":true}` |
| `cmd/schedule/enable` | `{"enabled":true}` |
| `cmd/schedule/today` | `{"enabled":true}` |

Topic `.../schedule/next` (retained): `{"hour":H,"min":M,"g":G,"in_min":N}`.

## OTA status

Topic `.../ota/status` (retained, QoS 1). The device keeps the last OTA
status and publishes it whenever it changes while the broker session is up,
and again on every MQTT connect. A fresh boot publishes `idle` with the
active bank; after a failed OTA the `error` status (with its reason) is
what every reconnect re-publishes, until the next `cmd/ota` replaces it.
During the download window the broker session is closed
([ota-flow.md](ota-flow.md) § Pre-download memory reclaim), so no progress
is published then: the topic shows `downloading` / `pct` 0 at accept, then
either `idle` from the new bank after the reboot or `error` once MQTT
resumes. Status updates are enqueued on `app` and drained by `mqtt_io` —
not published directly from the OTA handler.

| Field | Values |
|-------|--------|
| `state` | `idle`, `downloading`, `applying`, `error` |
| `pct` | 0–100 download progress |
| `error` | Empty string on success paths; when `state` is `error`, one of: |
| `bank` | Active application partition: `"A"` or `"B"` — included on every publish |

| `error` value | When |
|---------------|------|
| `invalid_url` | Malformed or oversized `cmd/ota` payload, bad URL scheme, invalid `sha512` hex, or `start` rejected |
| `already_in_progress` | OTA already running (`PORT_ERR_BUSY`) |
| `download_failed` | HTTP download or flash write failure |
| `verify_failed` | Bank hash differs from the manifest `sha512`, bank unreadable, or the written image has no vector table for the target bank (image linked for the other bank) |
| `image_too_large` | Image exceeds inactive bank capacity |
| `rolled_back` | The bootloader hit the boot-attempt limit on the new slot and switched back to the previous bank; reported by the surviving firmware on its next connect ([ota-flow.md](ota-flow.md) § Slot health) |

### `cmd/ota` payload

| Field | Required | Description |
|-------|----------|-------------|
| `url` | yes | HTTP or HTTPS firmware URL (max 255 chars) |
| `sha512` | no | Expected image SHA-512 as 128 lowercase/uppercase hex digits; when omitted, the bank hash is stored without comparison ([ota-flow.md](ota-flow.md) § Verification) |
