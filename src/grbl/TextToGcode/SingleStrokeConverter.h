#pragma once

#include <QFont>
#include <QList>
#include <QPolygonF>
#include <QString>

/// Converts system-font text into single-stroke (centerline) polylines
/// suitable for CNC/pen-plotter output.
///
/// Pipeline:
///   1. Render glyph as filled bitmap via QPainterPath
///   2. Zhang-Suen thinning → 1 px skeleton
///   3. Trace skeleton into polyline chains
///   4. Simplify with Ramer-Douglas-Peucker
class SingleStrokeConverter
{
public:
    struct Options {
        int glyphSize   = 256;   // render resolution per glyph (px)
        double epsilon  = 1.5;   // RDP simplification tolerance (px)
        int pruneLength = 8;     // prune skeleton branches shorter than this (px)
        int erodePixels = 0;     // morphological erosion before thinning (0 = auto)
    };

    /// Convert multi-line text to centerline polylines in the same
    /// coordinate space as QPainterPath::addText() output.
    static QList<QPolygonF> convert(const QString &text,
                                    const QFont &font,
                                    double lineSpacing = 1.5,
                                    const Options &opts = {});

private:
    SingleStrokeConverter() = default;
};
