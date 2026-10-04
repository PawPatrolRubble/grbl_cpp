# Multi-pen plotting and automatic pen changes

Status: pen definitions and assignment implemented on 2026-10-03; rack positions, local macro templates and exchange preview implemented on 2026-10-04. Automatic rack execution remains proposed.

Delivered scope: name/color/availability pen editor, Ctrl/Shift path selection, bulk assignment and default pen with shared Undo/Redo, group-child metadata persistence, legacy project loading, missing-pen labels, and metadata preservation through edits/copies/arrays/offsets/trim. The rack editor now saves station X/Y, asymmetric return/pickup Z, shared clearance/feeds/bounds/local radius, and separate per-pen local templates. ToolChangeMacroCompiler validates and expands return/load phases without machine side effects; the preview supports empty holders, known loaded pens, final returns and unchanged pens. Multi-pen or unavailable-pen designs cannot generate/export through the current single-pen pipeline. Usage and test commands are in [README.md](../../../README.md#rack-positions-and-pen-change-macro-preview).

Remaining scope: drawing height/feed calibration, rack corridor routing, actuator allowlists, physical calibration/state verification, full PlotJobBuilder and drawing-order integration, rack geometry preview, and execution/recovery. This slice implements the motion-only exchange compiler and command/phase inspection from stage 3; it does not complete that stage or the execution stages.

Verification: `TestPenAssignment` passes 23 behavioral scenarios, plus optional rendered screenshots. `TestToolChangeMacro` passes 39 checks for compilation, validation and persistence. The Debug application builds successfully, and `TestGcodeParser` passes. The existing `TestCanvasSnap` suite has a previously verified excluded-endpoint failure, reproduced in an isolated build of committed `HEAD` during the first slice.

Audience: engineers implementing pen assignment, job generation, and execution in GrblSender.

Goal: let a user assign a pen to selected paths in the design list, then plot those paths in order while automatically returning the previous pen and loading the next pen from a calibrated rack.

The pickup/release mechanism is still being designed. Motion phases and actuator hooks must therefore be configurable. The application can implement assignment and simulation before the mechanism is ready; physical rack operation requires a calibrated machine profile.

## 1. Scope and defaults

- Treat each drawable entity in the current Entities list as an assignable path. A text entity may contain many strokes, all using its assigned pen. Individual glyph/stroke assignment is a later feature.
- Support selecting several rows and assigning one pen in a single undoable action.
- Preserve document/list order. Change pens only when the next drawable path requires a different pen. Do not reorder by pen automatically: overlapping colors can depend on drawing order.
- Keep the existing single-pen workflow available without a rack. Automatic rack changes are an explicit job mode; multiple distinct pens require that mode in the first release.
- An unassigned path uses the job's default pen. A reference to a deleted/missing pen is an error, rather than an instruction to use the default.
- Support both an initially empty holder and an explicitly identified loaded pen. Never infer the physical holder contents from the previous project or application session.
- Provide two job-end policies: return the last pen to its rack slot, or keep it loaded and raised. Proposed default for automatic mode: return it to the rack. This preference is still open.
- Initially defer pen grouping optimization, pen-presence sensors, per-stroke assignments, and automatic recovery after an interrupted exchange.

## 2. Current implementation and consequences

| Existing component | Current behavior | Required extension |
| --- | --- | --- |
| GraphicsDesignPanel | A single-selection entity list with enable checkboxes; Generate/Export use cached G-code | Multiple selection, pen display and bulk assignment, structured generated-job output |
| Document and Entity | Stable in-memory UUIDs, layers, entity JSON, and command-based undo/redo | Pen references, serialization, metadata preservation, deterministic drawable-operation traversal |
| Document::generateGcode | Emits enabled, visible, non-dimension entities in document order using global heights/feed | Build a job that resolves each path's pen and inserts exchange operations |
| GroupEntity | Concatenates child moves, losing operation boundaries | Traverse drawable children before generating moves so mixed-pen groups remain usable |
| SettingsManager and SettingsPanel | JSON application settings and a settings dialog | Machine tool library, rack calibration, and editable exchange templates |
| ToolpathPanel and ToolpathCanvas | Transform parsed moves, then regenerate text through movesToGcode | Transform drawing operations while preserving rack motions, actuator commands, and operation identity |
| GcodeParser / GcodeMove | Geometry-oriented preview model without rack coordinate-frame or pen metadata | Source-line/operation mapping and separate treatment of drawing and machine-coordinate motions |
| GcodeStreamer | Buffers lines, continues after command errors, and reports completion once responses arrive | Exclusive command ownership, exchange checkpoints, failure outcomes, and physical-completion synchronization |
| ApplicationCore and SerialManager | Wire services and manage serial buffering; ordinary command entry remains available | Own a plot-job controller and prevent unrelated command injection during a job |

The preview's move-to-text round trip is especially important: simply adding macro text to the current generation method would allow a later transform to drop non-motion commands and transform rack positions.

## 3. User workflow

1. Open **Settings > Pens and Tool Rack**. Create pens such as Black fine, Red marker, and Blue fine. Give each pen a name, display color, drawing calibration, and physical rack slot.
2. Configure the rack clearance, approach/escape route, docking heights, feed rates, and pickup/release hooks. Until calibrated, allow simulation and prevent physical rack execution.
3. In the design's **Paths** list, use Ctrl/Shift to select rows. The existing entity list becomes this list; keep its enable checkboxes.
4. Choose **Assign pen** from a selector or context menu. Show the pen name and a color swatch on each row. Show **Mixed** when selected paths have different assignments, and **Default** for unassigned paths.
5. Generate the job. Preview pen colors, drawing order, exchange markers, and the number of exchanges. Show the pen sequence, for example `Black -> Red -> Black`.
6. Before Start, establish the holder contents as **Empty**, **Loaded: <pen>**, or **Unknown**. Unknown blocks rack execution until the operator identifies the holder contents. Show the required rack slots and selected end policy.
7. While plotting, display the active path, current pen, and exchange phase, such as `Returning Black` or `Loading Red`.
8. Complete the selected end action, then report completion only after the final motion has finished.

Synchronize canvas and list selection in both directions using entity UUIDs. Rebuilding the list must preserve all selected rows. Changing a checkbox must not accidentally assign a pen or clear selection.

## 4. Data and module design

### Pen references in the design

Add an optional `toolId` UUID to drawable Entity metadata. A null ID means use the document/job default. Keep this separate from layer color and from the application's CAD drawing tools.

Persist the assignment in project JSON, including nested group children. Save the default pen reference and lightweight labels for referenced pens so a project opened on another machine can explain unresolved references. Physical rack coordinates stay in machine settings; loading a project must not overwrite rack calibration.

Use a versioned schema. Older projects without these fields retain their existing single-pen behavior. Missing tool UUIDs remain visible as unresolved references and must be mapped before automatic generation/execution.

Preserve assignments through clone, property edits, transforms, trim/split, clipboard operations, duplication, and undo/redo. Centralize base metadata copying; several existing edit/transform paths construct replacement entities. Replacements keep the original identity and pen, while new copies receive new entity UUIDs and retain the pen. Do not blindly preserve an old assignment when the operation intentionally changes it to Default.

Group assignment applies to drawable descendant leaves in one CompositeCommand. The group's displayed pen is derived from those leaves: a shared pen, Default, or Mixed. Grouping and ungrouping preserve child assignments; the group has no competing pen override. Skip dimensions and disabled/excluded children during generation.

### Machine tool library

Introduce a PlotterToolLibrary with immutable tool UUIDs and JSON persistence through SettingsManager. A profile contains:

| Field | Purpose |
| --- | --- |
| ID, name, display color, enabled | Stable identity, list/preview display, availability |
| Drawing Z correction and lift distance | Account for pen length relative to the job's paper surface |
| Drawing feed and Z feed, or inherited defaults | Per-pen plotting behavior |
| Rack slot and station coordinates | Physical storage location in machine coordinates |
| Return/pickup contact heights and local waypoints | Allow asymmetric docking and escape motions |
| Return/pickup templates and actuator settle times | Mechanical release/grip sequence |
| Calibration status/revision | Prevent execution with incomplete or changed calibration |

Keep global machine clearance, travel limits, rack corridor waypoints, macro version, and firmware capabilities in a rack profile. The clearance must work with the longest configured pen and the empty holder.

Validate one unique pen per physical slot. At job setup, the operator confirms that each required pen is in its slot, except the identified loaded pen whose return slot must be empty. Track expected slot occupancy throughout the job; it is an operator-confirmed expectation until sensors are available.

Renaming a pen keeps its UUID. Removing/disabling a referenced pen exposes an unresolved/unavailable assignment. Configuration edits invalidate generated previews and require recompilation. Freeze the configuration snapshot for an active job.

### Structured plot job

Introduce PlotJobBuilder, ToolChangeMacroCompiler, PlotJob, and PlotJobController. Keep Entity::toGcodeMoves focused on drawing geometry. The builder traverses drawable leaves in order, resolves pens, filters empty move lists, and constructs:

```text
PlotJob
  expected initial holder state
  machine/tool configuration revision
  plotting frame and modal setup
  ordered operations:
    LoadTool / ChangeTool / DrawPath / ReturnTool
  compiled command blocks and synchronization checkpoints
  line -> operation -> entity/tool mapping
  preview geometry and end policy
```

ApplicationCore owns the controller. The controller coordinates the existing streamer/serial transport; it must not become a second independent serial sender. Compilation has no machine side effects.

The first-pen prefix depends on the actual initial holder state. If Start differs from the preview's assumption, rebuild that prefix from the same geometry/configuration snapshot and present the resulting job state before motion. Emit explicit metric/absolute setup before any rack motion, regardless of the controller's previous modal state. Derive each pen's drawing and lift heights from the job's paper-surface height and pen calibration, and honor the configured Z feed rather than inheriting the last XY feed.

## 5. Exchange sequence

The sequence is the user's proposed return-and-load operation with explicit clearance and completion points:

| Phase | Action | State after successful completion |
| --- | --- | --- |
| Finish current path | Raise the pen, then wait for queued drawing motion to finish | Loaded(current) |
| Approach current slot | Raise to rack travel clearance; follow the configured machine-coordinate route | Loaded(current) |
| Return current pen | Lower at controlled feed, perform release/docking steps, allow settling, then escape/retract to clearance | Empty |
| Approach next slot | Travel to the next slot through the calibrated route while clear of the rack | Empty |
| Pick up next pen | Lower at controlled feed, perform pickup/grip steps, allow settling, then escape/retract to clearance | Loaded(next), after checkpoint |
| Resume plotting | Restore plotting modes/frame/feed, travel to the next path with the pen raised, then lower to its drawing height | Drawing with next pen |

Do not perform the return phase when the holder is empty. Do not exchange when the current and required pens are the same. Returning the final pen is the same return operation with no subsequent pickup.

Ordinary travel between the paper and rack, and between rack stations, occurs at verified clearance. Some mechanisms require short lateral movement while docking; allow that only inside a calibrated station's local sequence. An unrestricted XY move at contact height is not a valid travel shortcut.

Use a positive dwell such as `G4 P0.01` as a firmware synchronization command and wait for its matched acknowledgement before releasing the next host-controlled phase. Place checkpoints after reaching the contact position, before invoking release/grip hooks, and after completing escape/retraction. Ordinary `ok` responses can precede physical motion completion. GRBL documents this synchronization technique for tool changes. [GRBL interface reference](https://github.com/gnea/grbl/wiki/Grbl-v1.1-Interface#synchronization).

Rack motions use explicit `G53 G0` or `G53 G1` on every relevant block. Drawing motions use the selected work coordinate system. G53 bypasses coordinate offsets and requires G0/G1 in GRBL's implementation. Keep these coordinate frames distinct in compilation and preview. [GRBL G-code implementation](https://github.com/gnea/grbl/blob/master/grbl/gcode.c).

The host expands exchanges to supported commands. Stock GRBL's M-code parser does not implement M6; emitting `Tn M6` cannot implement this feature. [GRBL M-code implementation](https://github.com/gnea/grbl/blob/master/grbl/gcode.c).

## 6. Configurable macro contract

Provide separate **return** and **pickup** templates. The outer exchange sequence owns travel clearance, checkpoints, and plotting-mode restoration. Templates describe calibrated local approach, contact, release/grip, settle, and escape phases.

Offer named values such as `slotX`, `slotY`, `returnZ`, `pickupZ`, `clearanceZ`, `dockFeed`, and `settleSeconds`. Expand them on the host to numeric G-code; unresolved values are compilation errors. Include a rendered command preview and phase-by-phase simulation.

Start with a Z-driven docking preset. Actuator hooks remain unset until the hardware is defined. Add only commands supported by the chosen firmware and wired actuator; do not assume spindle PWM is a servo controller.

Validate numeric values, positive feeds, bounds, coordinate frames, line lengths, and allowed commands. For the first release, local motion is explicit absolute machine-coordinate G0/G1, dwell is G4, and actuator commands use an explicitly configured firmware allowlist. Exclude reset, homing, settings/EEPROM writes, origin changes, program termination, and unhandled modal changes from templates. Longer approaches use explicit waypoints, not arbitrary scripts.

Restore generated plotting modes at the phase boundary, including units, absolute distance, XY plane, feed mode, arc-center mode, selected work coordinate system, and drawing/plunge feeds. Handle or reject unexpected temporary coordinate/tool-length offsets during preflight. Use software pen-height correction consistently rather than silently mixing it with controller tool-length offsets.

## 7. Generation, preview, transforms, and export

- For assignments `Black, Black, Red, Red, Black`, an empty-holder job loads Black once and performs two exchanges. A final return is counted separately. Disabled/hidden/dimension/empty paths do not trigger exchanges.
- Reuse the current visibility/enabled rules and apply group exclusions consistently to descendant operations.
- Preview drawing strokes in pen colors. Rack travel and exchange phases use a separate overlay. Convert frames using an explicit coordinate snapshot for display; never guess rack motion coordinates using the geometry-only parser.
- Transform drawing operations, then recompile the job. Rack positions, local docking trajectories, dwell, and actuator commands retain their machine meaning. Preserve raw commands for display/export and retain exact source-line mapping even when comments are present.
- Disable arbitrary line-range execution for managed multi-pen jobs in the first release. Starting halfway through a macro or drawing path can skip pen selection and modal setup. Future partial execution should select complete paths and build a fresh initial-tool prefix.
- The managed job owns its final lift/park/return policy. Bypass the existing generic appended return-to-origin command for these jobs; do not combine XY travel with a Z move toward the paper.
- Export a fully expanded, machine-specific `.nc` program with exchange comments and synchronization dwell commands. Include the expected initial holder state and configuration identity in its header. It requires the matching machine setup.
- Raw G-code re-import does not recreate managed operation metadata or physical-tool knowledge. Mark generated multi-pen exports in their header and require regeneration from the design for managed execution in version one. Do not run them through the current geometry-only transform round trip.

## 8. Execution and recovery

Track holder state as `Unknown`, `Empty`, or `Loaded(toolId)`, and track exchange phase separately. Update the loaded tool only after pickup, retraction, and the synchronization checkpoint complete. This is an expected mechanical result until presence sensors exist; display that distinction.

Before motion, require a connected, responsive, idle controller; a known holder state; a valid homing/calibration reference; resolved tools; valid slot availability; and compiled, validated templates. Rack positions must be calibrated in the controller's machine coordinate convention. A new connection or unlock is not evidence that the machine has been homed. Track successful homing in the session and invalidate that reference after reset, disconnect, or a position-losing alarm.

| Event | Required behavior |
| --- | --- |
| Pause | Stop submitting commands, issue feed hold, and retain the current phase; resume the same pending transaction without restarting release/grip |
| Command error or alarm | Prevent queue draining immediately, stop further submission, hold/abort as appropriate, discard queued commands, and report failure |
| Stop/reset/disconnect/controller restart | Abort the job and mark holder/position confidence unknown; do not automatically return a pen |
| Motion/response timeout | Fail the pending phase and require recovery; motion deadlines account for distance/feed, while intentional pauses suspend motion deadlines |
| Exchange interrupted | Show the last pen/slot/phase and ask the operator to inspect holder/rack contents before another automatic job |
| Normal end | Complete final lift and chosen return policy, synchronize, and report success |

Handle transport failure synchronously before SerialManager can drain its pending queue again. Distinguish queued commands from commands already transmitted; buffered controller commands may require reset to discard. GRBL's interface recommends halting on command errors and notes the limitations of buffered streaming. [GRBL error-handling reference](https://github.com/gnea/grbl/wiki/Grbl-v1.1-Interface#g-code-error-handling).

Assign outgoing ordinary commands an owner/transaction identity and match responses through the serial ledger. Use buffered streaming for drawing. During exchanges, send one ordinary command at a time and wait for its response; use the movement checkpoints to establish physical completion before mechanical hooks. No following phase enters the transport until the preceding checkpoint succeeds. During active jobs disable ordinary jog/home/origin/manual-command entry; retain realtime status, pause, stop, and emergency controls. Do not let unrelated query acknowledgements advance an exchange.

Expose separate Completed, Aborted, and Failed outcomes. The existing streamingFinished signal currently also covers stop/disconnect; UI messaging must not present those outcomes as successful plotting.

## 9. Implementation stages

Implement these in order. Physical automatic exchanges remain disabled until the execution and calibration stages pass.

| Stage | Work and affected modules | Completion evidence |
| --- | --- | --- |
| 1. Tool data and metadata | PlotterToolLibrary/profile, SettingsManager, Entity, Document, GroupEntity, CommandStack, replacement/clipboard/trim helpers | Versioned persistence, legacy loading, stable tool references, assignment survives edits and one-step undo/redo |
| 2. Assignment and settings UI | GraphicsDesignPanel, SettingsPanel, new tool/rack editor | Select three paths, assign Red once, see labels/colors, undo once, and save/load correctly |
| 3. Job and macro compiler | PlotJobBuilder, ToolChangeMacroCompiler, drawable traversal, existing generation adapters | Deterministic pen sequences, complete return/load phases, numeric command preview, validation errors identify the pen/slot/phase |
| 4. Preview and command preservation | ToolpathPanel, ToolpathCanvas, GcodeParser/source mapping, GraphicsDesignPage, MainWindow generated-job wiring | Scaling/translating a drawing leaves rack coordinates and actuator commands intact; selection/highlights map correctly |
| 5. Execution controller | PlotJobController, ApplicationCore, GcodeStreamer, SerialManager, MachineState, control/status UI | Fake transport proves phase gating, command ownership, pause/resume, abort/error handling, and true final completion |
| 6. Calibration and machine acceptance | Tool/rack editor and controller integration | Calibrated motion-only trial, single pickup/return, two-pen sample, and recovery checks pass on the actual mechanism |

Register new modules and focused console test targets in CMake using the project's existing Qt test style. Keep the normal single-pen job and existing parser/canvas tests passing.

## 10. Verification scenarios

### Automated behavior tests

1. Assign one pen to three selected UUIDs; one undo restores all three and redo reapplies them.
2. Load old project JSON; it keeps the existing single-pen behavior. Round-trip new projects and nested groups with assignments intact.
3. Clone, copy/paste, duplicate, property-edit, scale, mirror, and trim assigned paths; copied paths have distinct entity IDs and unchanged tool IDs.
4. Compile the `Black, Black, Red, Red, Black` example from Empty and from Loaded(Black). Check initial pickup count, two exchanges, and configurable final return.
5. Disable/hide a path, include dimensions and an empty text path, and compile mixed-pen group children. Excluded operations produce no exchanges.
6. Reject a missing pen, invalid dock height, duplicate slot assignment, inconsistent expected slot occupancy, unresolved placeholder, unsupported actuator command, uncalibrated profile, and unreachable station route before motion.
7. Transform drawing geometry; machine rack coordinates and raw actuator/dwell commands are preserved. Comments do not shift line-to-operation mapping.
8. Simulate rapid acknowledgements while motion still runs. The controller must wait for the synchronization response before committing tool state or completing the job.
9. Pause during release and pickup, then resume. Hooks execute once; queued commands do not leak across phase gates.
10. Inject an error/alarm at each exchange phase, a disconnect, controller restart, and timeout. No next path is submitted, outcome is Failed/Aborted, and recovery requires holder inspection.
11. Attempt manual ordinary commands during a job; they cannot interfere with the exchange's acknowledgement accounting. Realtime pause/stop/status still work.
12. Reject a selected raw line range for a managed job and reject managed execution of a flattened export without its original job metadata.

### Hardware acceptance after the mechanism exists

1. Establish homing and calibrate each station with a known holder state.
2. Run the complete approach/escape routes with an empty holder, then with the longest pen, using conservative travel feeds.
3. Validate each pen's pickup and return independently, including settle time and any required lateral docking motion.
4. Plot two simple paths with different pens, then the Black/Red/Black ordering example. Check registration and per-pen drawing height.
5. Verify a changed paper origin does not move the rack targets. Verify both end policies.
6. Pause during an exchange and resume; then separately test stop/disconnect recovery. Inspect the holder and slots before restarting.

Software simulation verifies sequencing and failure handling. Successful physical pickup and release require the actual mechanism and calibration; they cannot be inferred from acknowledgements alone.

## 11. Hardware decisions still open

- Does docking work through Z motion alone, or require a lateral latch, magnet, servo, or gripper? The user has confirmed this is still being designed.
- What are the station entry/exit routes, contact heights, and required pickup/release delays?
- Which controller firmware and outputs will support any actuator commands?
- Will the holder or rack have pen-presence feedback? Version one assumes operator-confirmed setup plus successful motion sequences.
- Should the default job-end action return the last pen or keep it loaded? This is a configurable policy; the proposed automatic-mode default is return.

These decisions affect templates and machine acceptance. They do not block the tool model, path-assignment UI, compiler, or simulated execution work.
