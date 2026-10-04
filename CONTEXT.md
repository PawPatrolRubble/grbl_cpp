# Machine toolpaths

This application assigns physical tools to drawing paths and prepares programs for a machine. Pen plotting is the currently supported process.

## Language

**Tool**:
A physical implement assigned to a path, such as a pen. Its identity stays the same when its name or rack location changes.
_Avoid_: Pen when referring to all assignable implements; CAD drawing tool when referring to a physical tool.

**Tool type**:
The category of a physical tool and the process it supports. Pen is the currently supported type.

**Pen**:
A tool that deposits ink along a drawing path. Its preview color represents the chosen ink color.

**Path**:
Design geometry selected for drawing with an assigned tool. Its tool may come from an explicit assignment or the document default.

**Preview color**:
The color used to identify a tool in lists and toolpath previews.

**Pen rack station**:
A location on the current machine where a pen is returned and picked up, including its docking motions and calibration.

**CAD drawing tool**:
An editor interaction for creating or modifying geometry, such as Line or Trim. It is distinct from a physical tool assigned to a path.
