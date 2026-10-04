# 0001 — Automatic passive rack exchanges

**Date:** 2026-10-04
**Status:** Accepted

## Problem Statement

Paths have pen assignments and stations have validated motion templates, but generated jobs cannot execute pen exchanges. The operator has confirmed a passive rack: motion releases and picks up pens. A complete job needs ordered drawing and rack motion, explicit machine/work coordinates, and recovery when the expected holder contents become uncertain.

## Proposed Solution

Extend the typed PlotJob with automatic-rack execution metadata, initial holder expectation, pen sequence, configuration fingerprint, motion bounds, phase labels, synchronization barriers and holder effects. PlotJobBuilder preserves drawable leaf order and calls the existing pure macro compiler at pen transitions. It raises in machine coordinates, travels to the drawing entry at rack clearance, then lowers in work coordinates. Adjacent paths using the same pen share a load. The final policy returns the pen or keeps it raised.

GcodeStreamer remains the sole command owner. Drawing may be buffered; each rack/setup/finish command and synchronization barrier drains earlier pending commands and waits for its own correlated acknowledgement. G4 checkpoints synchronize physical motion before contact hooks, retraction effects and subsequent drawing. Pause suspends the exchange deadline. Failure, cancellation, reset, alarm or disconnect discards queued execution and marks the holder unknown; recovery never attempts an automatic return.

ApplicationCore supplies a pure PlotEngine preflight validator. It checks the frozen program/configuration, calibrated rack and station flags, fresh controller coordinates and selected WCS offsets, machine bounds, and pen-up clearance. The backend additionally requires an idle ready controller, explicit session homing confirmation and the expected holder. The UI exposes automatic mode, initial holder and final policy, phase progress and an operator Start checklist covering homing, rack occupancy and shared drawing heights. Motion-setting edits invalidate calibration. Flat export and line-range execution are unavailable for managed jobs.

```mermaid
sequenceDiagram
    participant UI as Design and Start UI
    participant Builder as PlotJobBuilder
    participant Core as ApplicationCore preflight
    participant Job as GcodeStreamer
    participant GRBL as GRBL transport
    UI->>Builder: Paths, pens, initial holder, final policy
    Builder-->>UI: Frozen typed job and sequence
    UI->>Core: Confirm homing, holder and rack occupancy; Start
    Core->>Job: Job with preflight validator
    Job->>Core: Validate program, configuration, coordinates
    Core-->>Job: Accept or actionable error
    Job->>GRBL: Drain checkpoint, rack command, checkpoint
    GRBL-->>Job: Correlated acknowledgement for each command
    Job->>Job: Commit holder effect after retraction checkpoint
    Job->>GRBL: Buffered drawing until next barrier
    Job-->>UI: Phase, progress, outcome and holder state
```

## Alternatives Considered

- Flatten rack commands into ordinary G-code: loses phase barriers, holder state and configuration checks.
- Firmware M6/actuator control: unnecessary for the confirmed passive rack and unsupported by stock GRBL.
- Reorder paths by pen: reduces exchanges but changes the user's ordered drawing.

## Impact Assessment

| Area | Impact | Notes |
|---|---|---|
| Database | Additive settings | Calibration defaults false; older pen libraries still load. |
| API contract | Additive internal C++ model | Existing manual/single-pen jobs retain their behavior. |
| Frontend | Existing panels and Start dialog | Automatic controls and calibration confirmations. |
| Tests | New service integration tests | Fake transport, planner, preflight, pause/failure and UI smoke coverage. |
| External API | Existing GRBL protocol | Explicit G53 motion and G4 checkpoints; no actuator commands. |
| Infrastructure | None | No dependencies or services. |
| Observability | Phase and holder signals | Failure identifies the interrupted phase/source line. |
| Security / Compliance | Physical motion validation | Typed job integrity and bounded machine/work coordinates. |

## Open Questions

None for software execution of the confirmed passive rack. Station-specific docking paths and clear routes must be calibrated on the actual machine. Shared plotting heights must work for all participating pens; per-pen drawing-height offsets and sensors are future extensions.

## Acceptance Criteria

- Black, Black, Red, Red, Black preserves path order and loads only at transitions, starting empty or from a specified loaded pen.
- Default final return ends with an empty holder; keep policy ends raised with the last pen.
- No drawing after pickup is submitted before its retraction checkpoint succeeds; no rack command bypasses pending drawing.
- Error at any exchange block, reset, disconnect, cancellation and timeout invalidate holder/homing confidence and submit no recovery motion.
- Pause resumes without replaying completed exchange commands or timing out while intentionally held.
- Stale configuration, uncalibrated stations/rack, unknown or mismatched holder, unconfirmed homing, unavailable offsets, out-of-bounds drawing and partial-job Start fail before motion.
- Single-pen/manual tests continue to pass; managed export and drawing transforms cannot lose execution metadata.
- Debug and Release CTest suites and offscreen UI construction succeed without connecting to hardware.

## Decision

Accepted under the user's instruction to implement automatic exchanges and confirmation that the rack is passive. See [ADR 0001](../decisions/0001-managed-passive-rack-jobs.md).
