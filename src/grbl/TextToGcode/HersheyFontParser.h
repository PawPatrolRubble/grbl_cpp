#pragma once

#include "VectorFontParser.h"

class HersheyFontParser : public VectorFontParser {
public:
    QMap<QChar, QList<QList<QPointF>>> parse(QIODevice& device) override;
    QString formatName() const override { return "Hershey"; }
};
