#include "RackJobValidator.h"
#include "grbl/ToolLibrary.h"
#include <QSet>
#include <cmath>

QString RackJobValidator::validate(const PlotJob& job, const ToolLibrary& tools,
                                   const std::optional<QVector3D>& workOffset,
                                   const std::optional<QVector3D>& machinePosition)
{
    if (!job.automaticRack || job.executionSeal.isEmpty() || job.executionSeal != job.executionDigest())
        return "Regenerate the complete automatic program before Start.";
    if (job.configurationFingerprint != tools.configurationFingerprint())
        return "Pen or rack settings changed after generation. Regenerate the design job.";
    const auto& rack = tools.rackSettings();
    if (!rack.configured || !rack.motionCalibrated)
        return "Verify rack bounds and clear straight travel between rack and drawing at clearance, then mark the rack calibrated.";
    QSet<QUuid> required(job.penSequence.begin(), job.penSequence.end());
    if (!job.initialToolId.isNull()) required.insert(job.initialToolId);
    for (const auto& id : required) {
        const auto* pen = tools.tool(id);
        const auto station = tools.penRackStation(id);
        if (!pen || pen->kind != ToolKind::Pen || !station.configured || !station.motionCalibrated)
            return "Verify passive pickup and return for every required pen, then mark its station calibrated.";
    }
    if (!workOffset || !machinePosition)
        return "Current machine coordinates or G54–G59/G92/TLO offsets are unavailable. Refresh with $G and $# and wait for an idle position report.";
    const std::array<double, 3> offset{workOffset->x(), workOffset->y(), workOffset->z()};
    const std::array<double, 3> position{machinePosition->x(), machinePosition->y(), machinePosition->z()};
    const std::array<double, 3> minimum{rack.minX, rack.minY, rack.minZ}, maximum{rack.maxX, rack.maxY, rack.maxZ};
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(offset[axis]) || !std::isfinite(position[axis])
            || !std::isfinite(job.workMinimum[axis]) || !std::isfinite(job.workMaximum[axis]))
            return "Machine/work coordinates contain invalid values.";
        if (position[axis] < minimum[axis] || position[axis] > maximum[axis])
            return "The current machine position is outside the calibrated rack travel bounds.";
        if (job.workMinimum[axis] + offset[axis] < minimum[axis]
            || job.workMaximum[axis] + offset[axis] > maximum[axis])
            return "Drawing motion in the selected work frame is outside calibrated machine bounds.";
    }
    // Raised cross-frame travel holds machine Z fixed, then lowers at the drawing entry.
    if (job.safeZ + offset[2] > std::round(rack.clearanceZ * 1000) / 1000)
        return "Drawing pen-up Z lies above rack machine clearance. Adjust work Z or rack clearance before Start.";
    return {};
}
