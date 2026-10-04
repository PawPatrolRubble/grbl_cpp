#pragma once

#include "ToolLibrary.h"
#include <QStringList>

struct ToolChangeRequest
{
    QUuid currentToolId; // Null means a known empty holder.
    QUuid nextToolId;    // Null means return the current pen only.
    int workCoordinateSystem = 54;
    double drawingFeed = 1000;
};

struct ToolChangePhase
{
    QString title;
    QUuid toolId;
    QStringList commands;
    enum class HolderEffect { None, Empty, Loaded };
    HolderEffect holderEffect = HolderEffect::None;
};

struct ToolChangePreview
{
    QList<ToolChangePhase> phases;
    QStringList errors;
    QStringList warnings;
    bool valid() const { return errors.isEmpty(); }
    QStringList commandLines() const;
    QString displayText() const;
};

// Pure compilation for inspection. This module never accesses the serial transport.
class ToolChangeMacroCompiler
{
public:
    // Below stock GRBL's uint16_t dwell timer capacity at a 50 ms time step.
    static constexpr double maxDwellSeconds = 3000;
    static ToolChangePreview compile(const ToolLibrary& library, const ToolChangeRequest& request);
};
