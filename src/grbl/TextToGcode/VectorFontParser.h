#pragma once

#include <QMap>
#include <QChar>
#include <QList>
#include <QPointF>
#include <QIODevice>
#include <QString>

class VectorFontParser {
public:
    virtual ~VectorFontParser() = default;

    // Parses the provided device and returns a map of glyphs.
    // Each glyph is a list of strokes, and each stroke is a list of points.
    virtual QMap<QChar, QList<QList<QPointF>>> parse(QIODevice& device) = 0;

    // Returns a short name identifying the format (e.g. "Hershey", "SVG", "JSON", "HPGL")
    virtual QString formatName() const = 0;
};
