# Eaten-today tracking

serves:
  - ../20-stories/monitoring.md
  - ../20-stories/display.md

Cumulative grams the pet ate since local midnight. Derived in the monitoring
layer from **settled presented bowl mass** ([auto-tare.md](auto-tare.md)); the
weigh driver stays stateless ([weighing.md](weighing.md)).

## Model

| Variable | Scope | Role |
|----------|-------|------|
| `total_dg` | RAM + NVDM | Cumulative eaten mass today, tenth-grams |
| `day_key` | RAM + NVDM | Local date `YYYYMMDD` the total belongs to (0 = clock unknown) |
| `ref_dg` | RAM | Last settled presented mass (the bowl level the pet is eating down from) |
| `ref_valid` | RAM | False until a first usable settled sample |

## Idle sample rule

Called on every idle weight sample (500 ms, no dispense/OTA active) with the
presented mass and a `usable` flag. `usable` is true only when the scale is
calibrated, a sample is valid, `bowl_error` is clear, and auto-tare is anchored
(`stable_valid`, not `pending_calibration`).

1. **Midnight rollover:** with a known `day_key` different from the stored one,
   `total_dg = 0` and the new key is stored. With the clock unknown (0) the
   total keeps accumulating; the stored key is adopted once the clock is known
   if it was 0, otherwise a mismatch resets.
2. Not `usable` → `ref_valid = false`; nothing counted.
3. First usable sample → `ref_dg = presented`, `ref_valid = true`.
4. **Settle:** a sample is settled after `[tune]` 6 consecutive samples whose
   presented mass stays within `[tune]` 2 dg (0.2 g) of the previous sample.
5. Settled and `presented > ref_dg` → `ref_dg = presented` (food added or
   drift up; never counted).
6. Settled and `ref_dg − presented ≥ [tune]` 10 dg (1 g) → `total_dg +=
   ref_dg − presented`, `ref_dg = presented`, persist.
7. Smaller drops are not counted until they accumulate past 1 g against
   `ref_dg`.

Dispense completion and bowl removal call `eaten_today_invalidate()` so the
next usable settled sample becomes the new `ref_dg` (food added by the feeder
and bowl removal are never counted as eaten).

## Persistence

NVDM group `eaten`: `day` (uint32 `YYYYMMDD`), `dg` (uint32). Written only when
the total changes or rolls over. On boot the stored total is restored when its
day equals the current local day or the clock is not yet known; otherwise it
starts at 0. Factory reset erases the group.

## MQTT / display

Retained `.../eaten_today` — see [mqtt-protocol.md](mqtt-protocol.md) §
Eaten today. Display mode `eaten_today` shows the rounded whole grams
([display-presentation.md](display-presentation.md)).
