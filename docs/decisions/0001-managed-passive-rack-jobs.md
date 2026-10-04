# 0001 — Managed passive rack jobs

**Date:** 2026-10-04
**Status:** Accepted
**Deciders:** User (passive rack and implementation scope), Codex (software design)
**Proposal:** [Automatic passive rack exchanges](../proposals/0001-automatic-passive-rack-exchanges.md)

## Context

The application already assigns pens to paths and compiles station motion previews. The user has confirmed that rack motion alone releases and picks up pens and requested automatic exchanges. Stock GRBL accepts motion before that motion necessarily finishes, so exchanges require physical synchronization and software holder tracking. Machine origin, work offsets and rack calibration must be explicit before execution.

## Options Considered

### Option A — Flattened G-code

- **Summary:** Send all rack and drawing commands through ordinary buffered streaming.
- **Pros:** Small implementation.
- **Cons:** No phase boundary, holder recovery or stale-configuration validation.

### Option B — Managed typed job

- **Summary:** Preserve phase barriers and holder effects in PlotJob, using the existing command owner.
- **Pros:** Testable ordering, synchronized transitions, explicit recovery and no second transport path.
- **Cons:** Managed jobs require whole-job execution and retain metadata in the application.

## Decision

Use managed typed jobs with sequential passive rack commands, synchronization checkpoints, calibrated configuration and operator-confirmed initial conditions.

## Rationale

The existing PlotJob and GcodeStreamer provide the required model and ownership boundaries. Reusing the macro compiler preserves its restricted, bounded G53 templates. Operator confirmations accommodate a passive rack without inventing sensor or actuator support.

## Consequences

- **Positive:** Path order, pen changes and recovery become one coherent execution lifecycle.
- **Negative / trade-offs:** Shared drawing heights and straight travel at calibrated clearance remain machine setup responsibilities; managed jobs cannot be run from a raw line range or transformed after compilation.
- **Risks:** Software cannot detect a failed mechanical pickup. Hardware calibration and inspection remain necessary; interruption invalidates holder and homing confidence before another Start.

## Related Decisions

None.
