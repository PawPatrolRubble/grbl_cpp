#pragma once

#include "PlotJob.h"

class Document;
class ToolLibrary;

struct PlotOptions
{
    double drawingFeed = 2000;
    double plungeFeed = 1500;
    double safeZ = 5;
    double drawingZ = 0;
    int workCoordinateSystem = 54;
    bool automaticRack = false;
    bool initialHolderKnown = false;
    QUuid initialToolId; // Null with initialHolderKnown means empty.
    bool returnFinalPen = true;
};

struct PlotJobBuildResult
{
    PlotJob job;
    QStringList errors;
    QStringList warnings;
    bool valid() const { return errors.isEmpty(); }
};

class PlotJobBuilder
{
public:
    static PlotJobBuildResult build(const Document& document, const PlotOptions& options,
                                   const ToolLibrary* tools = nullptr);
};
