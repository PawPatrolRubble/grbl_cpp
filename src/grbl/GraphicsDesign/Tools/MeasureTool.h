#pragma once

#include "AngularDimensionTool.h"

/// Compatibility wrapper for the former Measure tool slot.
/// The single visible angle annotation workflow lives in AngularDimensionTool.
class MeasureTool final : public AngularDimensionTool
{
public:
    using AngularDimensionTool::AngularDimensionTool;
};
