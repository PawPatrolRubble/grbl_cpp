#pragma once

#include "PlotJob.h"
#include "grbl/GcodeParser.h"

struct PlotJobTransformResult
{
    PlotJob job;
    QString error;
    bool valid() const { return error.isEmpty(); }
};

class PlotJobTransform
{
public:
    static QString unsupportedReason(const PlotJob& job);
    static PlotJobTransformResult apply(const PlotJob& job, const QList<GcodeMove>& transformed);
};
