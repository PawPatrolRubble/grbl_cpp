#pragma once

#include <QString>
#include <QUuid>
#include <QColor>

// Default layer color palette
inline QColor defaultLayerColor(int index)
{
    static const QColor palette[] = {
        QColor("#d4d4d4"),  // Layer 0: light grey (default on dark bg)
        QColor("#f87171"),  // Layer 1: red
        QColor("#60a5fa"),  // Layer 2: blue
        QColor("#4ade80"),  // Layer 3: green
        QColor("#fbbf24"),  // Layer 4: amber
        QColor("#a78bfa"),  // Layer 5: violet
        QColor("#22d3ee"),  // Layer 6: cyan
        QColor("#f472b6"),  // Layer 7: pink
    };
    return palette[index % 8];
}

/// A drawing layer that can contain entities.
/// Entities on hidden layers are not rendered or selectable.
/// Entities on locked layers cannot be moved or edited.
struct Layer
{
    QUuid   id;
    QString name;
    bool    visible = true;
    bool    locked  = false;
    QColor  color   = QColor("#d4d4d4");

    static Layer makeDefault(int index = 0)
    {
        Layer l;
        l.id    = QUuid::createUuid();
        l.name  = QString("Layer %1").arg(index);
        l.color = defaultLayerColor(index);
        return l;
    }
};
