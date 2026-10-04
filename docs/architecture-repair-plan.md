# Architecture repairs

Scope: address the six findings from the architecture review in the current working tree. Preserve existing pen assignment and rack preview behavior. At the time of this repair, the mechanism remained under design and automatic execution stayed disabled. The subsequent passive-rack implementation is documented in [proposal 0001](proposals/0001-automatic-passive-rack-exchanges.md).

1. Correlate every GRBL acknowledgement with an identified submitted command. Give the job controller exclusive ownership of ordinary command submission while a job is active; wait for preceding manual/system commands before starting.
2. Separate completed, failed, cancelled and disconnected outcomes. End normal jobs with an acknowledged positive dwell checkpoint. Reject invalid submissions before counting them. Handle controller resets, alarms and serial failures explicitly.
3. Preserve original program blocks and source indices. Conservatively disable geometric transforms for unsupported frames/modes. Transform supported drawing blocks without deleting setup, dwell, or auxiliary commands.
4. Make imports a single undo action and target entity edits by UUID. Make layer edits undoable and preserve the drawing's layer/assignment state through undo/redo.
5. Introduce PlotJob and PlotJobBuilder. Centralize pen resolution, availability checks, plotting options and Z feed selection. Keep preview compilation independent of transport.
6. Build reusable CAD, plotting, transport, application and UI libraries. Register focused tests with CTest, including a fake GRBL transport and behavior regressions. Remove the unused Qt private-module dependency.

Verification: first reproduce existing failures with regression tests, then build the application and run CTest using the offscreen Qt platform. Simulated transport scenarios cover acknowledgement ownership, buffered failure, delayed synchronization, pause/resume, interruption, reset and disconnection. No physical machine commands are part of verification.

## Implemented and verified — 2026-10-04

All six repairs are implemented. The initial regression executable reproduced nine failures before the fixes. The final architecture/UI suite passes 24 checks; the controller/transport suite passes 51; the plotting/document suite passes 44. Existing parsing, CAD interaction, pen assignment and rack preview suites also pass.

The follow-up checks additionally exposed and corrected disabled grid snapping, modal-only blocks becoming geometric motion, radius-form arc scaling, startup queries issuing a rejected checkpoint in Alarm, stale asynchronous previews, keyboard jog gating/auto-repeat, held jog release handling and sibling-model access during application teardown.

Verified builds and runs:

```powershell
cmake --build build/codex-pen-assignment --config Debug --parallel 8
ctest --test-dir build/codex-pen-assignment -C Debug --output-on-failure
# 7/7 suites passed

cmake -S . -B build/codex-architecture-verification -G "Visual Studio 18 2026" -A x64 -DBUILD_TESTING=ON
cmake --build build/codex-architecture-verification --config Release --parallel 8
ctest --test-dir build/codex-architecture-verification -C Release --output-on-failure
# Clean Release build; 7/7 suites passed
```

`git diff --check` passes. Tests use offscreen widgets and simulated commands without connecting to hardware. Application-window construction, rendering and teardown are exercised. Changes remain uncommitted. The implemented ownership and extension boundaries are described in [the current architecture](architecture.md).
