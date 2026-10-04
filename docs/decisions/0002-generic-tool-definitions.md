# 0002 — Generic tool definitions with pen plotting

**Date:** 2026-10-04
**Status:** Accepted
**Deciders:** User and Codex
**Proposal:** [0002](../proposals/0002-generic-tool-definitions.md)

## Context

The user wants physical tool assignment to remain useful if milling is introduced later. Paths and jobs already reference stable tool IDs, while the library profile and shared UI are pen-specific. The current passive pen rack has calibrated motion and execution checks that must remain explicit. Only plotting is supported today.

## Options Considered

### Keep shared pen names
- **Pros:** No migration.
- **Cons:** Common concepts remain tied to a single process.

### Generic definitions with separate pen stations
- **Pros:** Clear Tool/Pen relationship, stable assignments and a reusable common model.
- **Cons:** Persistence migration and updates across consumers.

### Implement a full machining-operation and machine-profile model
- **Pros:** Broader future process model.
- **Cons:** Adds unused abstractions and unsupported machining behavior now.

## Decision

Use ToolDefinition and ToolLibrary for the shared model, with Pen as the only supported type and passive pen rack stations stored separately by tool ID.

## Rationale

This follows the user's accepted terminology while preserving the current workflow. Separating station configuration prevents common tool identity from depending on pen docking. Legacy readers and stable UUIDs protect existing assignments. Milling operations will be introduced with actual milling support.

## Consequences

- **Positive:** Shared assignment and preview interfaces use Tool, and the Pen process stays explicit.
- **Trade-offs:** The existing settings facade still owns this machine's pen-rack configuration and retains the nested station persistence envelope for compatibility.
- **Risks:** Migration must not drop stations or calibration; unsupported kinds must be rejected rather than run through plotting.

## Related Decisions

- [0001 — Managed passive rack jobs](0001-managed-passive-rack-jobs.md)
