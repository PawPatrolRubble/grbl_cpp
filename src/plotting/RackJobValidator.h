#pragma once
#include "PlotJob.h"
#include <QVector3D>
#include <optional>
class ToolLibrary;

class RackJobValidator {
public:
    static QString validate(const PlotJob& job, const ToolLibrary& tools,
                            const std::optional<QVector3D>& workOffset,
                            const std::optional<QVector3D>& machinePosition);
};
