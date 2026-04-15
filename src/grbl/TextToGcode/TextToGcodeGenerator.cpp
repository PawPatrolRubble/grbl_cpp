#include "TextToGcodeGenerator.h"
#include <QRectF>
#include <QPainterPath>
#include <QFontMetrics>
#include <algorithm>

struct GlyphMetrics {
    double width = 0.0;
    double height = 0.0;
    double scale = 1.0;
    double offsetX = 0.0;
    double offsetY = 0.0;
};

static QMap<QChar, GlyphMetrics> computeGlyphMetrics(
    const QMap<QChar, QList<QList<QPointF>>>& fontData, double fontSize)
{
    QMap<QChar, GlyphMetrics> metrics;

    for (auto it = fontData.constBegin(); it != fontData.constEnd(); ++it) {
        double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
        bool hasPoints = false;

        for (const auto& stroke : it.value()) {
            for (const QPointF& p : stroke) {
                minX = std::min(minX, p.x());
                maxX = std::max(maxX, p.x());
                minY = std::min(minY, p.y());
                maxY = std::max(maxY, p.y());
                hasPoints = true;
            }
        }

        GlyphMetrics m;
        if (hasPoints) {
            m.width = maxX - minX;
            m.height = maxY - minY;
            m.offsetX = -minX;
            m.offsetY = -minY;
            m.scale = (m.height > 0) ? fontSize / m.height : 1.0;
        } else {
            m.scale = 1.0;
        }
        metrics[it.key()] = m;
    }

    return metrics;
}

QStringList TextToGcodeGenerator::generate(const QString& text,
                                           const QMap<QChar, QList<QList<QPointF>>>& fontData,
                                           const Options& options)
{
    QStringList gcode;
    gcode << "G21 G90        ; metric, absolute";

    if (options.toolMode == ToolMode::PenPlotter) {
        gcode << QString("G1 Z%1 F%2        ; raise pen to safe Z")
                 .arg(options.safeZ, 0, 'f', 2).arg(options.plungeRateZ, 0, 'f', 0);
    } else {
        gcode << "M5             ; laser off";
    }

    auto metrics = computeGlyphMetrics(fontData, options.fontSize);

    double cursorX = options.startX;
    double cursorY = options.startY;
    double lineHeight = options.fontSize * options.lineSpacing;

    for (int i = 0; i < text.length(); ++i) {
        QChar c = text.at(i);

        if (c == '\n') {
            cursorX = options.startX;
            cursorY -= lineHeight;
            continue;
        }

        if (c == ' ') {
            cursorX += options.fontSize * 0.5;
            continue;
        }

        if (!fontData.contains(c) || !metrics.contains(c)) {
            cursorX += options.fontSize * 0.5;
            continue;
        }

        const auto& strokes = fontData.value(c);
        const GlyphMetrics& m = metrics[c];

        for (const auto& stroke : strokes) {
            if (stroke.isEmpty()) continue;

            QPointF p0 = stroke.first();
            double x0 = cursorX + (p0.x() + m.offsetX) * m.scale;
            double y0 = cursorY + (p0.y() + m.offsetY) * m.scale;

            gcode << QString("G0 X%1 Y%2")
                     .arg(x0, 0, 'f', 3)
                     .arg(y0, 0, 'f', 3);

            if (options.toolMode == ToolMode::PenPlotter) {
                gcode << QString("G1 Z%1 F%2").arg(options.cutZ, 0, 'f', 2).arg(options.plungeRateZ, 0, 'f', 0);
            } else {
                gcode << "M4             ; laser on";
            }

            for (int k = 1; k < stroke.size(); ++k) {
                QPointF p = stroke[k];
                double x = cursorX + (p.x() + m.offsetX) * m.scale;
                double y = cursorY + (p.y() + m.offsetY) * m.scale;
                gcode << QString("G1 X%1 Y%2 F%3")
                         .arg(x, 0, 'f', 3)
                         .arg(y, 0, 'f', 3)
                         .arg(options.feedRateXY, 0, 'f', 0);
            }

            if (options.toolMode == ToolMode::PenPlotter) {
                gcode << QString("G1 Z%1 F%2        ; raise pen")
                         .arg(options.safeZ, 0, 'f', 2).arg(options.plungeRateZ, 0, 'f', 0);
            } else {
                gcode << "M5             ; laser off";
            }
        }

        double advanceWidth = m.width * m.scale;
        if (advanceWidth < options.fontSize * 0.3) {
            advanceWidth = options.fontSize * 0.3;
        }
        cursorX += advanceWidth * options.letterSpacing;
    }

    gcode << "; End of Text to G-Code";
    return gcode;
}

QStringList TextToGcodeGenerator::generateFromSystemFont(const QString& text, const QFont& font, const Options& options)
{
    QStringList gcode;
    gcode << "G21 G90        ; metric, absolute";

    if (options.toolMode == ToolMode::PenPlotter) {
        gcode << QString("G1 Z%1 F%2        ; raise pen to safe Z")
                 .arg(options.safeZ, 0, 'f', 2).arg(options.plungeRateZ, 0, 'f', 0);
    } else {
        gcode << "M5             ; laser off";
    }

    QPainterPath path;
    QStringList lines = text.split('\n');
    double yOffset = 0.0;
    for (const QString& line : lines) {
        path.addText(0, yOffset, font, line);
        yOffset += options.fontSize * options.lineSpacing;
    }

    double cursorX = options.startX;
    double cursorY = options.startY;

    for (const QPolygonF& stroke : path.toSubpathPolygons()) {
        if (stroke.isEmpty()) continue;

        QPointF p0 = stroke.first();
        gcode << QString("G0 X%1 Y%2")
                 .arg(cursorX + p0.x(), 0, 'f', 2)
                 .arg(cursorY - p0.y(), 0, 'f', 2);

        if (options.toolMode == ToolMode::PenPlotter) {
            gcode << QString("G1 Z%1 F%2").arg(options.cutZ, 0, 'f', 2).arg(options.plungeRateZ, 0, 'f', 0);
        } else {
            gcode << "M4             ; laser on";
        }

        for (int k = 1; k < stroke.size(); ++k) {
            QPointF p = stroke[k];
            gcode << QString("G1 X%1 Y%2 F%3")
                     .arg(cursorX + p.x(), 0, 'f', 2)
                     .arg(cursorY - p.y(), 0, 'f', 2)
                     .arg(options.feedRateXY, 0, 'f', 0);
        }

        if (options.toolMode == ToolMode::PenPlotter) {
            gcode << QString("G1 Z%1 F%2        ; raise pen").arg(options.safeZ, 0, 'f', 2).arg(options.plungeRateZ, 0, 'f', 0);
        } else {
            gcode << "M5             ; laser off";
        }
    }

    gcode << "; End of System Font to G-Code";
    return gcode;
}