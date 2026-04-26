#include "TextEntity.h"

#include "../TextToGcode/VectorFontLoader.h"
#include "../TextToGcode/SingleStrokeConverter.h"

#include <QPainterPath>
#include <QFontMetricsF>
#include <QJsonObject>
#include <cmath>
#include <algorithm>

TextEntity::TextEntity(const QPointF& position, const QString& text, double fontSize)
    : Entity(Kind::Text)
    , m_position(position)
    , m_text(text)
    , m_fontSize(fontSize)
{
}

Entity::Kind TextEntity::kind() const
{
    return Kind::Text;
}

QRectF TextEntity::boundingRect() const
{
    if (!m_boundingRectCacheValid) {
        m_boundingRectCache = computeBoundingRect();
        m_boundingRectCacheValid = true;
    }
    return m_boundingRectCache;
}

QPointF TextEntity::center() const
{
    return boundingRect().center();
}

void TextEntity::draw(QPainter& painter) const
{
    if (m_text.isEmpty()) return;

    if (m_fontMode == FontMode::VectorFont && !m_vectorFontData.isEmpty()) {
        // Draw vector font strokes
        auto strokes = computeVectorStrokes();
        for (const auto& stroke : strokes) {
            if (stroke.size() < 2) continue;
            for (int i = 1; i < stroke.size(); ++i) {
                painter.drawLine(stroke[i - 1], stroke[i]);
            }
        }
    } else if (m_singleStroke) {
        // Draw single-stroke (centerline) rendering of system font
        const auto &strokes = singleStrokes();

        // Build or translate batched line cache
        if (!m_ssLineCacheValid) {
            m_ssLineCache.clear();
            int totalSegs = 0;
            for (const QPolygonF &s : strokes)
                if (s.size() >= 2) totalSegs += s.size() - 1;
            m_ssLineCache.reserve(totalSegs);

            for (const QPolygonF &stroke : strokes) {
                if (stroke.size() < 2) continue;
                for (int i = 1; i < stroke.size(); ++i) {
                    m_ssLineCache.append(QLineF(
                        m_position.x() + stroke[i-1].x(), m_position.y() - stroke[i-1].y(),
                        m_position.x() + stroke[i].x(),   m_position.y() - stroke[i].y()));
                }
            }
            m_ssLineCachePos = m_position;
            m_ssLineCacheValid = true;
        } else if (m_ssLineCachePos != m_position) {
            // Position changed (drag) — translate cached lines instead of rebuilding
            QPointF delta = m_position - m_ssLineCachePos;
            for (QLineF &line : m_ssLineCache) {
                line.translate(delta);
            }
            m_ssLineCachePos = m_position;
        }
        painter.drawLines(m_ssLineCache);
    } else {
        // Draw system font outlines — use pixmap cache for fast blitting
        if (!m_outlinePathCacheValid) {
            QFont font(m_fontFamily);
            font.setPointSizeF(m_fontSize);

            m_outlinePathCache = QPainterPath();
            QStringList lines = autoWrappedText().split('\n');
            double yOffset = 0.0;
            double lineHeight = m_fontSize * m_lineHeight;
            for (const QString& line : lines) {
                if (!line.isEmpty()) {
                    m_outlinePathCache.addText(0, yOffset, font, line);
                }
                yOffset += lineHeight;
            }
            m_outlinePathCacheValid = true;
            m_outlinePixmapCacheValid = false;  // force pixmap rebuild
        }

        if (!m_outlinePixmapCacheValid) {
            // Render path to pixmap at a fixed resolution
            QRectF pathBounds = m_outlinePathCache.boundingRect().adjusted(-2, -2, 2, 2);
            constexpr double pixScale = 4.0;  // render at 4x for quality
            int pw = std::max(1, static_cast<int>(std::ceil(pathBounds.width() * pixScale)));
            int ph = std::max(1, static_cast<int>(std::ceil(pathBounds.height() * pixScale)));
            // Cap size to avoid excessive memory
            if (pw > 4096) { pw = 4096; }
            if (ph > 4096) { ph = 4096; }
            m_outlinePixmapCache = QPixmap(pw, ph);
            m_outlinePixmapCache.fill(Qt::transparent);
            {
                QPainter pmPainter(&m_outlinePixmapCache);
                pmPainter.setRenderHint(QPainter::Antialiasing, true);
                pmPainter.scale(pixScale, pixScale);
                pmPainter.translate(-pathBounds.topLeft());
                pmPainter.setPen(painter.pen());
                pmPainter.setBrush(Qt::NoBrush);
                pmPainter.drawPath(m_outlinePathCache);
            }
            m_outlinePixmapRect = pathBounds;
            m_outlinePixmapCacheValid = true;
        }

        painter.save();
        painter.translate(m_position);
        painter.scale(1, -1);  // Un-flip Y for text
        // Draw the pre-rendered pixmap
        QRectF target = m_outlinePixmapRect;
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.drawPixmap(target, m_outlinePixmapCache, QRectF(m_outlinePixmapCache.rect()));
        painter.restore();
    }
}

double TextEntity::distanceTo(const QPointF& point) const
{
    QRectF rect = boundingRect();
    if (rect.contains(point)) return 0.0;

    double dx = std::max({rect.left() - point.x(), 0.0, point.x() - rect.right()});
    double dy = std::max({rect.top() - point.y(), 0.0, point.y() - rect.bottom()});
    return std::hypot(dx, dy);
}

void TextEntity::translate(const QPointF& delta)
{
    m_position += delta;
    m_boundingRectCacheValid = false;
}

QString TextEntity::displayName() const
{
    QString preview = m_text.left(20).simplified();
    if (m_text.length() > 20) preview += "...";
    return QString("Text: %1").arg(preview);
}

std::unique_ptr<Entity> TextEntity::clone() const
{
    auto copy = std::make_unique<TextEntity>(m_position, m_text, m_fontSize);
    copyBaseFieldsTo(copy.get());
    copy->m_fontMode = m_fontMode;
    copy->m_fontFamily = m_fontFamily;
    copy->m_vectorFontPath = m_vectorFontPath;
    copy->m_vectorFontData = m_vectorFontData;
    copy->m_letterSpacing = m_letterSpacing;
    copy->m_lineHeight = m_lineHeight;
    copy->m_maxWidth = m_maxWidth;
    copy->m_singleStroke = m_singleStroke;
    copy->m_ssResolution = m_ssResolution;
    copy->m_ssSmoothing = m_ssSmoothing;
    copy->m_ssCleanup = m_ssCleanup;
    copy->m_ssMergeGap = m_ssMergeGap;
    copy->m_ssMinSegLen = m_ssMinSegLen;

    // Copy render caches to avoid expensive recomputation (e.g. during drag)
    copy->m_ssCache = m_ssCache;
    copy->m_ssCacheValid = m_ssCacheValid;
    copy->m_ssLineCache = m_ssLineCache;
    copy->m_ssLineCacheValid = m_ssLineCacheValid;
    copy->m_ssLineCachePos = m_ssLineCachePos;
    copy->m_outlinePathCache = m_outlinePathCache;
    copy->m_outlinePathCacheValid = m_outlinePathCacheValid;
    copy->m_outlinePixmapCache = m_outlinePixmapCache;
    copy->m_outlinePixmapCacheValid = m_outlinePixmapCacheValid;
    copy->m_outlinePixmapRect = m_outlinePixmapRect;
    copy->m_boundingRectCache = m_boundingRectCache;
    copy->m_boundingRectCacheValid = m_boundingRectCacheValid;
    return copy;
}

std::vector<Entity::EditHandle> TextEntity::editHandles() const
{
    std::vector<EditHandle> handles;
    QRectF rect = boundingRect();
    handles.push_back({"position", m_position, HandleRole::Resize, 0.0, false, {}});
    handles.push_back({"corner_tr", QPointF(rect.right(), rect.top()), HandleRole::Resize, 0.0, false, {}});
    handles.push_back({"corner_br", QPointF(rect.right(), rect.bottom()), HandleRole::Resize, 0.0, false, {}});
    return handles;
}

std::unique_ptr<Entity> TextEntity::withDraggedHandle(const QString& handleId, const QPointF& worldPos) const
{
    auto updated = std::unique_ptr<TextEntity>(static_cast<TextEntity*>(clone().release()));

    if (handleId == "position") {
        updated->setPosition(worldPos);
    } else if (handleId == "corner_tr" || handleId == "corner_br") {
        // Scale font size proportionally based on horizontal extent
        QRectF rect = boundingRect();
        double origWidth = rect.width();
        if (origWidth > 0.001) {
            double newWidth = std::abs(worldPos.x() - m_position.x());
            double scale = newWidth / origWidth;
            updated->m_fontSize = std::max(1.0, m_fontSize * scale);
            // Invalidate all render caches since font size changed
            updated->m_ssCacheValid = false;
            updated->m_ssLineCacheValid = false;
            updated->m_outlinePathCacheValid = false;
            updated->m_outlinePixmapCacheValid = false;
            updated->m_boundingRectCacheValid = false;
        }
    }
    return updated;
}

std::vector<QPointF> TextEntity::snapPoints() const
{
    QRectF rect = boundingRect();
    return {m_position, rect.center(), rect.topLeft(), rect.topRight(),
            rect.bottomLeft(), rect.bottomRight()};
}

QList<GcodeMove> TextEntity::toGcodeMoves(double zSafe, double zCut, double feedRate) const
{
    QList<GcodeMove> moves;
    if (m_text.isEmpty()) return moves;

    if (m_fontMode == FontMode::VectorFont && !m_vectorFontData.isEmpty()) {
        // Vector font: strokes are already in world coordinates
        auto strokes = computeVectorStrokes();
        for (const auto& stroke : strokes) {
            if (stroke.isEmpty()) continue;

            QPointF p0 = stroke.first();

            GcodeMove rapid;
            rapid.type = GcodeMove::Type::Rapid;
            rapid.end = QVector3D(p0.x(), p0.y(), zSafe);
            moves.append(rapid);

            GcodeMove plunge;
            plunge.type = GcodeMove::Type::Linear;
            plunge.start = QVector3D(p0.x(), p0.y(), zSafe);
            plunge.end = QVector3D(p0.x(), p0.y(), zCut);
            plunge.feedRate = feedRate;
            moves.append(plunge);

            for (int k = 1; k < stroke.size(); ++k) {
                QPointF prev = stroke[k - 1];
                QPointF cur = stroke[k];
                GcodeMove line;
                line.type = GcodeMove::Type::Linear;
                line.start = QVector3D(prev.x(), prev.y(), zCut);
                line.end = QVector3D(cur.x(), cur.y(), zCut);
                line.feedRate = feedRate;
                moves.append(line);
            }

            QPointF last = stroke.last();
            GcodeMove raise;
            raise.type = GcodeMove::Type::Rapid;
            raise.start = QVector3D(last.x(), last.y(), zCut);
            raise.end = QVector3D(last.x(), last.y(), zSafe);
            moves.append(raise);
        }
    } else {
        // System font
        QFont font(m_fontFamily);
        font.setPointSizeF(m_fontSize);

        QList<QPolygonF> strokes;
        if (m_singleStroke) {
            // Single-stroke (centerline) conversion — cached
            strokes = singleStrokes();
        } else {
            // Outline polygons via QPainterPath
            QPainterPath path;
            QStringList lines = autoWrappedText().split('\n');
            double yOffset = 0.0;
            double lineHeight = m_fontSize * m_lineHeight;
            for (const QString& line : lines) {
                if (!line.isEmpty()) {
                    path.addText(0, yOffset, font, line);
                }
                yOffset += lineHeight;
            }
            strokes = path.toSubpathPolygons();
        }

        for (const QPolygonF& stroke : strokes) {
            if (stroke.isEmpty()) continue;

            QPointF p0 = stroke.first();
            double x0 = m_position.x() + p0.x();
            double y0 = m_position.y() - p0.y();

            GcodeMove rapid;
            rapid.type = GcodeMove::Type::Rapid;
            rapid.end = QVector3D(x0, y0, zSafe);
            moves.append(rapid);

            GcodeMove plunge;
            plunge.type = GcodeMove::Type::Linear;
            plunge.start = QVector3D(x0, y0, zSafe);
            plunge.end = QVector3D(x0, y0, zCut);
            plunge.feedRate = feedRate;
            moves.append(plunge);

            for (int k = 1; k < stroke.size(); ++k) {
                QPointF prev = stroke[k - 1];
                QPointF cur = stroke[k];
                GcodeMove line;
                line.type = GcodeMove::Type::Linear;
                line.start = QVector3D(m_position.x() + prev.x(),
                                        m_position.y() - prev.y(), zCut);
                line.end = QVector3D(m_position.x() + cur.x(),
                                      m_position.y() - cur.y(), zCut);
                line.feedRate = feedRate;
                moves.append(line);
            }

            QPointF last = stroke.last();
            GcodeMove raise;
            raise.type = GcodeMove::Type::Rapid;
            raise.start = QVector3D(m_position.x() + last.x(),
                                     m_position.y() - last.y(), zCut);
            raise.end = QVector3D(m_position.x() + last.x(),
                                   m_position.y() - last.y(), zSafe);
            moves.append(raise);
        }
    }

    return moves;
}

QJsonObject TextEntity::toJson() const
{
    QJsonObject json;
    json["type"] = "text";
    json["x"] = m_position.x();
    json["y"] = m_position.y();
    json["text"] = m_text;
    json["fontSize"] = m_fontSize;
    json["letterSpacing"] = m_letterSpacing;
    json["lineHeight"] = m_lineHeight;
    json["maxWidth"] = m_maxWidth;
    json["fontMode"] = (m_fontMode == FontMode::VectorFont) ? "vector" : "system";
    json["fontFamily"] = m_fontFamily;
    if (!m_vectorFontPath.isEmpty()) {
        json["vectorFontPath"] = m_vectorFontPath;
    }
    json["rotation"] = rotation();
    if (m_singleStroke) {
        json["singleStroke"] = true;
        json["ssResolution"] = m_ssResolution;
        json["ssSmoothing"] = m_ssSmoothing;
        json["ssCleanup"] = m_ssCleanup;
        json["ssMergeGap"] = m_ssMergeGap;
        json["ssMinSegLen"] = m_ssMinSegLen;
    }
    return json;
}

std::unique_ptr<TextEntity> TextEntity::fromJson(const QJsonObject& json)
{
    QPointF pos(json["x"].toDouble(), json["y"].toDouble());
    QString text = json["text"].toString();
    double fontSize = json["fontSize"].toDouble(10.0);
    auto entity = std::make_unique<TextEntity>(pos, text, fontSize);
    entity->setRotation(json["rotation"].toDouble(0));
    entity->m_letterSpacing = json["letterSpacing"].toDouble(1.2);
    entity->m_lineHeight = json["lineHeight"].toDouble(1.5);
    entity->m_maxWidth = json["maxWidth"].toDouble(0.0);
    entity->m_fontFamily = json["fontFamily"].toString("Sans Serif");
    entity->m_singleStroke = json["singleStroke"].toBool(false);
    entity->m_ssResolution = json["ssResolution"].toInt(256);
    entity->m_ssSmoothing = json["ssSmoothing"].toDouble(1.5);
    entity->m_ssCleanup = json["ssCleanup"].toInt(8);
    entity->m_ssMergeGap = json["ssMergeGap"].toDouble(4.0);
    entity->m_ssMinSegLen = json["ssMinSegLen"].toDouble(3.0);

    QString fontMode = json["fontMode"].toString("system");
    if (fontMode == "vector") {
        QString fontPath = json["vectorFontPath"].toString();
        if (!fontPath.isEmpty()) {
            entity->setVectorFont(fontPath);
        }
    }
    return entity;
}

TrimEnd TextEntity::getTrimPoint(const QPointF& clickPos, const QPointF& intersection) const
{
    // Text is not directly trimmable - return None
    return TrimEnd::None;
}

QPointF TextEntity::pointAtTrimEnd(TrimEnd end) const
{
    return m_position;
}

void TextEntity::setVectorFont(const QString& fontFilePath)
{
    auto result = VectorFontLoader::loadFont(fontFilePath);
    if (result.success) {
        m_fontMode = FontMode::VectorFont;
        m_vectorFontPath = fontFilePath;
        m_vectorFontData = result.glyphs;
    }
}

QString TextEntity::autoWrappedText() const
{
    if (m_maxWidth <= 0.0 || m_text.isEmpty())
        return m_text;

    if (m_fontMode == FontMode::VectorFont && !m_vectorFontData.isEmpty()) {
        // Estimate glyph widths from vector font data
        double maxGlyphHeight = 0.0;
        for (auto it = m_vectorFontData.constBegin(); it != m_vectorFontData.constEnd(); ++it) {
            for (const auto& stroke : it.value()) {
                for (const QPointF& p : stroke)
                    maxGlyphHeight = std::max(maxGlyphHeight, p.y());
            }
        }
        double scale = (maxGlyphHeight > 0.0) ? m_fontSize / maxGlyphHeight : 1.0;

        QString result;
        double cursorX = 0.0;
        double availableWidth = m_maxWidth - m_position.x();

        for (int i = 0; i < m_text.length(); ++i) {
            QChar c = m_text.at(i);
            if (c == '\n') {
                result += c;
                cursorX = 0.0;
                continue;
            }
            double charWidth;
            if (c == ' ') {
                charWidth = m_fontSize * 0.5 * m_letterSpacing;
            } else {
                QChar lookupChar = m_vectorFontData.contains(c) ? c : c.toUpper();
                if (m_vectorFontData.contains(lookupChar)) {
                    double glyphMinX = 1e9, glyphMaxX = -1e9;
                    for (const auto& stroke : m_vectorFontData.value(lookupChar)) {
                        for (const QPointF& p : stroke) {
                            glyphMinX = std::min(glyphMinX, p.x());
                            glyphMaxX = std::max(glyphMaxX, p.x());
                        }
                    }
                    double glyphW = (glyphMaxX > glyphMinX) ? (glyphMaxX - glyphMinX) : m_fontSize * 0.3;
                    charWidth = glyphW * scale * m_letterSpacing;
                } else {
                    charWidth = m_fontSize * 0.5 * m_letterSpacing;
                }
            }
            if (cursorX + charWidth > availableWidth && cursorX > 0.0) {
                result += '\n';
                cursorX = 0.0;
            }
            result += c;
            cursorX += charWidth;
        }
        return result;
    }

    // System font wrapping
    QFont font(m_fontFamily);
    font.setPointSizeF(m_fontSize);
    QFontMetricsF fm(font);
    double availableWidth = m_maxWidth - m_position.x();

    QString result;
    double cursorX = 0.0;
    for (int i = 0; i < m_text.length(); ++i) {
        QChar c = m_text.at(i);
        if (c == '\n') {
            result += c;
            cursorX = 0.0;
            continue;
        }
        double charWidth = fm.horizontalAdvance(c);
        if (cursorX + charWidth > availableWidth && cursorX > 0.0) {
            result += '\n';
            cursorX = 0.0;
        }
        result += c;
        cursorX += charWidth;
    }
    return result;
}

const QList<QPolygonF>& TextEntity::singleStrokes() const
{
    if (!m_ssCacheValid) {
        QFont font(m_fontFamily);
        font.setPointSizeF(m_fontSize);
        m_ssCache = SingleStrokeConverter::convert(
            m_text, font, 1.5,
            {m_ssResolution, m_ssSmoothing, m_ssCleanup, 0, m_ssMergeGap, m_ssMinSegLen});
        m_ssCacheValid = true;
    }
    return m_ssCache;
}

QList<QList<QPointF>> TextEntity::computeVectorStrokes() const
{
    QList<QList<QPointF>> result;
    if (m_vectorFontData.isEmpty() || m_text.isEmpty()) return result;

    QString wrappedText = autoWrappedText();

    // Compute per-glyph metrics for scaling
    double maxGlyphHeight = 0.0;
    for (auto it = m_vectorFontData.constBegin(); it != m_vectorFontData.constEnd(); ++it) {
        for (const auto& stroke : it.value()) {
            for (const QPointF& p : stroke) {
                maxGlyphHeight = std::max(maxGlyphHeight, p.y());
            }
        }
    }
    double scale = (maxGlyphHeight > 0.0) ? m_fontSize / maxGlyphHeight : 1.0;

    double cursorX = m_position.x();
    double cursorY = m_position.y();
    double lineHeight = m_fontSize * m_lineHeight;

    for (int i = 0; i < wrappedText.length(); ++i) {
        QChar c = wrappedText.at(i);

        if (c == '\n') {
            cursorX = m_position.x();
            cursorY -= lineHeight;  // Y-up: next line is below
            continue;
        }

        if (c == ' ') {
            cursorX += m_fontSize * 0.5 * m_letterSpacing;
            continue;
        }

        if (!m_vectorFontData.contains(c) && !m_vectorFontData.contains(c.toUpper())) {
            cursorX += m_fontSize * 0.5 * m_letterSpacing;
            continue;
        }

        QChar lookupChar = m_vectorFontData.contains(c) ? c : c.toUpper();
        const auto& glyphStrokes = m_vectorFontData.value(lookupChar);

        double glyphMinX = 1e9, glyphMaxX = -1e9;
        for (const auto& stroke : glyphStrokes) {
            QList<QPointF> worldStroke;
            for (const QPointF& p : stroke) {
                double x = cursorX + p.x() * scale;
                double y = cursorY + p.y() * scale;
                worldStroke.append(QPointF(x, y));
                glyphMinX = std::min(glyphMinX, p.x());
                glyphMaxX = std::max(glyphMaxX, p.x());
            }
            if (worldStroke.size() >= 2) {
                result.append(worldStroke);
            }
        }

        double glyphWidth = (glyphMaxX > glyphMinX) ? (glyphMaxX - glyphMinX) : m_fontSize * 0.3;
        cursorX += glyphWidth * scale * m_letterSpacing;
    }

    return result;
}

QRectF TextEntity::computeBoundingRect() const
{
    if (m_text.isEmpty()) {
        return QRectF(m_position, QSizeF(0, 0));
    }

    if (m_fontMode == FontMode::VectorFont && !m_vectorFontData.isEmpty()) {
        auto strokes = computeVectorStrokes();
        if (strokes.isEmpty()) {
            return QRectF(m_position, QSizeF(m_fontSize, m_fontSize));
        }
        double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
        for (const auto& stroke : strokes) {
            for (const QPointF& p : stroke) {
                minX = std::min(minX, p.x());
                maxX = std::max(maxX, p.x());
                minY = std::min(minY, p.y());
                maxY = std::max(maxY, p.y());
            }
        }
        return QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
    }

    QFont font(m_fontFamily);
    font.setPointSizeF(m_fontSize);
    QFontMetricsF fm(font);
    QStringList lines = autoWrappedText().split('\n');
    double maxWidth = 0.0;
    for (const QString& line : lines) {
        maxWidth = std::max(maxWidth, fm.horizontalAdvance(line));
    }
    double totalHeight = lines.size() * m_fontSize * m_lineHeight;
    QRectF textRect(m_position.x(), m_position.y() - totalHeight,
                    maxWidth, totalHeight);
    return textRect;
}
