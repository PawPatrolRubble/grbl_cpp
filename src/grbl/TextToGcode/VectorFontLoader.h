#pragma once

#include "VectorFontParser.h"
#include <QString>
#include <QMap>
#include <QList>
#include <QPointF>
#include <QChar>
#include <memory>

struct FontLoadResult {
    bool success = false;
    QString errorMessage;
    QMap<QChar, QList<QList<QPointF>>> glyphs;

    explicit operator bool() const { return success; }
};

class VectorFontLoader {
public:
    static std::shared_ptr<VectorFontParser> getParserForFile(const QString& filePath);
    static FontLoadResult loadFont(const QString& filePath);
};