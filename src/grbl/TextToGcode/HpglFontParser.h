#pragma once

#include "VectorFontParser.h"

class HpglFontParser : public VectorFontParser {
public:
    QMap<QChar, QList<QList<QPointF>>> parse(QIODevice& device) override;
    QString formatName() const override { return "HPGL"; }
};
