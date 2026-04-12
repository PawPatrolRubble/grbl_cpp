#pragma once

#include <QString>
#include <QStringList>
#include <QMap>
#include <QList>
#include <QPointF>
#include <QFont>

class TextToGcodeGenerator {
public:
    enum class ToolMode { PenPlotter, Laser };
    enum class AnchorPoint { BottomLeft, Center, TopLeft };

    struct Options {
        double fontSize = 10.0;
        bool maintainAspectRatio = true;
        double characterWidth = 10.0;
        double letterSpacing = 1.2;
        double lineSpacing = 1.5;
        AnchorPoint anchor = AnchorPoint::BottomLeft;
        double startX = 0.0;
        double startY = 0.0;

        ToolMode toolMode = ToolMode::PenPlotter;
        double safeZ = 5.0;
        double cutZ = 0.0;
        double feedRateXY = 500.0;
        double plungeRateZ = 200.0;
    };

    QStringList generate(const QString& text,
                         const QMap<QChar, QList<QList<QPointF>>>& fontData,
                         const Options& options);

    QStringList generateFromSystemFont(const QString& text,
                                         const QFont& font,
                                         const Options& options);
};