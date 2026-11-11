# Health monitoring

serves:
- [monitoring.md](../20-stories/monitoring.md)
- [feeding.md](../20-stories/feeding.md)

What the feeder records and raises so a failed or missed meal is noticed and
explained without a UART cable: the feed history, health counters, the
problem state (HA, web, front panel), the daily limit, the not-eaten alert and
the task watch.

## Feed history

Every completed dispense job (any source except the UART bench) appends one
record. The last `[tune]` 20 records are kept in NVDM group `hist`, keys
`f0`…`f19` (ring), `next` = index of the next slot to write. One key is
written per job.

Record (one CSV line, ≤ 120 chars):

| Field | Meaning |
|-------|---------|
| `t` | UTC epoch seconds at completion (`0` = time not synced) |
| `src` | `s` schedule, `m` MQTT/HA, `w` web, `b` button |
| `want` | Target grams (`0` = portion job) |
| `got` | Delivered grams (measured, or estimated when not measured) |
| `meas` | `1` measured by the scale, `0` estimated |
| `res` | Outcome: `ok`, `underfill`, `stuck`, `empty_hopper`, `aborted`, `limit` |
| `por` | Portions requested (all batches) |
| `pul` | Index holes counted (all batches) |
| `aj` | Anti-jam retries used |
| `why` | Motor stop reason: `-` (normal), `antijam` (retries exhausted; last jam in `jam`), `session`, `io`, `driver`, `led` |
| `jam` | Last jam reason: `-`, `stall`, `index`, `adc`, `session` |
| `ok`/`busy`/`err` | Index-beam polls that succeeded / were refused (bus busy) / failed |
| `ma` | Highest motor current seen (mA) |
| `dur` | Motor run time (s, all batches) |
| `mqtt` | `1` when the MQTT session was up at completion |
| `bowl` | `1` when the bowl was present at the start of the job |

`GET /api/history` returns the records newest first as a JSON array of objects
with the field names above. The web page shows them in **System → Feed
history**. The newest record is also the `last_feed` attribute of the HA
*Problem* sensor.

## Health counters

NVDM group `hist`, key `counters` (CSV), written when a counter changes:

| Counter | Incremented when |
|---------|------------------|
| `feeds_ok` | A job ends `ok` or `underfill` |
| `feeds_failed` | A job ends `stuck`, `empty_hopper`, `aborted` or `limit` |
| `jams` | The motor starts an anti-jam retry |
| `mqtt_lost` | The MQTT session drops while armed (any cause) |
| `boots` | The firmware starts |
| `stalls` | The task watch reports a stalled task |

`/api/status` adds `"health":{feeds_ok, feeds_failed, jams, mqtt_lost, boots,
stalls, uptime_s, reset}` (`reset` = last reset reason string as logged at
boot). Shown in **System → Diagnostics**.

## Problem state

A problem is one of the codes below. **Live** codes follow their condition;
**latched** codes stay until *Clear problem* (`cmd/problem_clear`, HA button,
web) or their own clear rule.

| Code | Meaning | Kind | Raised when | Also cleared when |
|------|---------|------|-------------|-------------------|
| E1 | Feed failed | latched | A job ends `stuck`, `aborted` or `empty_hopper`; or the motor counted ≥ 1 hole, the bowl was present before and after, and the measured delta is below `[tune]` 1 g ("nothing fell") | the next job ends `ok` |
| E2 | Hopper empty | live | Hopper level `empty` | — |
| E3 | Bowl missing at feed | latched | A job starts while the scale reports the bowl missing (the job still runs) | — |
| E4 | Connection lost | latched | MQTT session down for more than `[tune]` 5 min while armed | — |
| E5 | Time not synced | live | Wall clock not set (schedule cannot run) | — |
| E6 | Daily limit reached | latched | A job is refused or shortened by the daily limit | local midnight |
| E7 | Not eaten | latched | See [Not-eaten alert](#not-eaten-alert) | consumption detected |
| E9 | Task stalled | latched | See [Task watch](#task-watch) | — |

**MQTT.** Retained `petfeeder/<id>/problem` (QoS 1), published on change and on
connect:

```json
{"active":true,"codes":["E1","E3"],"text":"Feed failed (stuck); Bowl missing at feed","last_feed":{...}}
```

`last_feed` is the newest history record (object as in `/api/history`,
omitted when the history is empty). E4 is published once the session is back
(the text gives the outage length: `Connection lost (12 min)`).

**Home Assistant.** `binary_sensor` *Problem* (`device_class: problem`,
`value_template` on `active`, `json_attributes_topic` = the same topic) and
`button` *Clear problem* (`cmd/problem_clear`, empty payload).

**Web.** `/api/status` adds `"problem":{active, codes, text}`; the Home tab shows
a red banner with the text and a *Clear* button (`POST /api/problem_clear`).

**Front panel.** While a problem is active and no dispense, OTA or animation is
on screen, the digits cycle `[tune]` 1.5 s `Err` → 1.5 s `E` + code digit
(`E 1`) → 3 s normal content. The gram unit is hidden during the two error
phases; icons keep their normal state. The shown code is the most important
active one in the order E1, E2, E3, E7, E6, E9, E4, E5. While blanked (idle
screen-off) the panel stays off.

## Daily limit

NVDM `feed/daily_max_g` (0 = off, 10–2000, default 0). The feeder sums the
grams delivered today (history `got` of jobs completed since local midnight,
any source except UART). Before a job starts:

- delivered today ≥ limit → the job is refused: history record `res=limit`,
  `got=0`, E6, a schedule slot goes to state `skipped_limit`, MQTT/web callers
  get `busy`-style refusal `limit`.
- otherwise the target is reduced to the remaining allowance (whole portions,
  at least 1); when reduced, E6 is raised and the job runs.

Settable with `cmd/feed_limit {"daily_max_g":N}`; state retained on
`.../feed/limit` `{"daily_max_g":N,"today_g":G}`. HA `number` *Daily limit*
(config, g, 0–2000, step 10). Web: **Settings → Feeding**.

## Not-eaten alert

NVDM `feed/not_eaten_h` (0 = off, 1–24, default 0). After a job delivers at
least `[tune]` 5 g (measured), the feeder records the time and the eaten-today
total. If, `not_eaten_h` hours later, the eaten-today total has not grown by
`[tune]` 5 g (and no newer job restarted the timer), E7 is raised. E7 clears as
soon as eaten-today grows by 5 g after the alert. A local-midnight reset of
eaten-today keeps the comparison (the growth counts across midnight).

Settable with `cmd/feed_limit {"not_eaten_h":N}` (same topic and state
payload, field `not_eaten_h`). HA `number` *Not eaten alert* (config, h, 0–24).
Web: **Settings → Feeding**.

## Task watch

The `app`, `motor` and `mqtt_io` task loops each bump a heartbeat counter every
iteration. The `app` task checks `motor` and `mqtt_io` on each display tick;
the `mqtt_io` task checks `app`. A heartbeat unchanged for `[tune]` 60 s logs
`[watch] task <name> stalled <n> s` once, increments `stalls` and raises E9.
Nothing restarts the device: the user reboots when they choose. A task that
resumes logs `[watch] task <name> resumed`.
