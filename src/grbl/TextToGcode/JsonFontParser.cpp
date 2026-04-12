#include "JsonFontParser.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>

QMap<QChar, QList<QList<QPointF>>> JsonFontParser::parse(QIODevice& device)
{
    QMap<QChar, QList<QList<QPointF>>> glyphs;
    
    QByteArray data = device.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return glyphs;
    
    QJsonObject root = doc.object();
    QJsonObject glyphsObj = root.value("glyphs").toObject();
    
    for (auto it = glyphsObj.begin(); it != glyphsObj.end(); ++it) {
        if (it.key().isEmpty()) continue;
        QChar c = it.key().at(0);
        QJsonArray strokesArray = it.value().toArray();
        
        QList<QList<QPointF>> strokes;
        for (int i = 0; i < strokesArray.size(); ++i) {
            QJsonArray strokeArray = strokesArray[i].toArray();
            QList<QPointF> path;
            for (int j = 0; j < strokeArray.size(); ++j) {
                QJsonArray point = strokeArray[j].toArray();
                if (point.size() >= 2) {
                    path.append(QPointF(point[0].toDouble(), point[1].toDouble()));
                }
            }
            if (!path.isEmpty()) {
                strokes.append(path);
            }
        }
        if (!strokes.isEmpty()) {
            glyphs.insert(c, strokes);
        }
    }
    
    return glyphs;
}
