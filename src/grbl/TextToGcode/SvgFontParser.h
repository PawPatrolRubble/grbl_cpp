#pragma once

#include "VectorFontParser.h"
#include <QPainterPath>

class SvgFontParser : public VectorFontParser {
public:
    QMap<QChar, QList<QList<QPointF>>> parse(QIODevice& device) override;
    QString formatName() const override { return "SVG"; }

private:
    QPainterPath parseSvgPath(const QString& pathData);
};
