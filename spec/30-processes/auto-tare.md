# Auto-tare (drift compensation)

serves:
  - ../20-stories/monitoring.md
  - ../20-stories/feeding.md

## Purpose

Slow temperature drift shifts raw `read_dg` even when bowl contents are
unchanged. Auto-tare is an internal RAM drift compensator in the
monitoring/presentation layer — the weigh driver stays stateless (see
[weighing.md](weighing.md) **Weigh driver boundary**).

This is **not** a user-facing tare command. There is no `weigh tare`. Bench
operators inspect auto-tare state via `tare show` ([uart-console.md](uart-console.md)).

## RAM model

All mass values in this module are **tenth-grams** (`weight_dg_t`: 1 unit =
0.1 g).

| Variable | Scope | Role |
|----------|-------|------|
| `stable_dg` | RAM | Presented bowl food belief (panel + `bowl_weight` MQTT) |
| `drift_offset_dg` | RAM | Added to raw `read_dg` so `presented = raw + drift_offset_dg` tracks `stable_dg` during drift |
| `stable_valid` | RAM | False until first anchor |
| `pending_calibration` | RAM | True after bowl removal / `bowl_error` until natural anchor or first-dispense anchor |

**Bowl removal / `bowl_error`:** `stable_valid = false`, `pending_calibration = true`,
`drift_offset_dg = 0`. Edge detection runs on each idle weight sample in `app`
(not from display or MQTT sync).

**Calibration complete** (`pending_calibration = false`) when:

- `INITIAL_STABLE_STREAK` quiet samples after boot or bowl re-insert
- Post-dispense read (see [dispense-cycle.md](dispense-cycle.md))
- First dispense pre-motor anchor while `pending_calibration` is set

While `pending_calibration`: drift nudging is **disabled**. Presentation shows
raw calibrated dg until anchored.

```text
presented_dg = raw_dg + drift_offset_dg   (when stable_valid and not pending)
presented_dg = raw_dg                     (when pending_calibration or not stable_valid)
```

## Idle drift

On each idle weight sample when **quiet**, `!pending_calibration`, `stable_valid`,
and the runtime dispense-active flag is false (see
[task-model.md](../40-architecture/task-model.md) § Runtime snapshot):

- **Quiet:** per-sample `|Δraw| ≤ DRIFT_RATE_MAX_DG` (0.1 g)
- Accumulate the creep: `drift_offset_dg −= Δraw`, so slow drift is cancelled
  sample by sample
- If `|Δraw| > DRIFT_RATE_MAX_DG` the change is **real mass** (pet eating, food
  added): `drift_offset_dg` is unchanged and the step passes through to
  `presented_dg`
- After every valid sample `stable_dg = raw_dg + drift_offset_dg` (the presented
  belief)

Earlier revisions set `drift_offset_dg = stable_dg − raw_dg` on every quiet
sample. That pinned `presented_dg` to the last anchor, so food eaten between two
dispenses was erased as soon as the scale went quiet (bowl weight stuck, eaten
today never counted). With the CS1270's 1 g resolution a single-count change is
10 dg > `DRIFT_RATE_MAX_DG`, so in practice every visible change passes through.

## Initial anchor

After boot or when `bowl_error` clears, require `INITIAL_STABLE_STREAK`
consecutive quiet samples, then anchor at the current raw reading:

- `stable_dg = raw_dg`
- `drift_offset_dg = 0`
- `stable_valid = true`
- `pending_calibration = false`

## Re-anchor triggers

| Trigger | Owner |
|---------|--------|
| Post-dispense settle read | `dispense.c` → `auto_tare_anchor(post)` |
| Initial quiet streak | `auto_tare.c` idle sample |
| First dispense after re-insert | `dispense.c` when `pending_calibration` — see [dispense-cycle.md](dispense-cycle.md) § Pre-dispense baseline |

## Suppression

- **During dispense:** no drift nudging (read runtime dispense-active flag); post-dispense anchor
- **`pending_calibration`:** no drift nudging
- **`bowl_error` active:** invalidate stable state; set `pending_calibration`

## `[tune]` parameters

| Parameter | Start | Notes |
|-----------|-------|-------|
| `DRIFT_RATE_MAX_DG` | 1 (0.1 g per 500 ms sample) | Above this → not drift |
| `INITIAL_STABLE_STREAK` | 4 | ~2 s quiet before first anchor |

## UART CLI

See [uart-console.md](uart-console.md) § `tare` commands.
