#pragma once

#include "Entity.h"

#include <QString>
#include <QFont>
#include <QMap>
#include <QList>
#include <QPolygonF>
#include <QPainterPath>
#include <QPixmap>

/// Text entity supporting both system fonts and vector (single-line) fonts.
/// System fonts are rendered via QPainterPath outlines.
/// Vector fonts are rendered as stroke polylines (optimized for CNC).
class TextEntity final : public Entity
{
public:
    enum class FontMode { SystemFont, VectorFont };

    TextEntity(const QPointF& position, const QString& text, double fontSize = 10.0);

    // Entity interface
    Kind kind() const override;
    QRectF boundingRect() const override;
    QPointF center() const override;
    void draw(QPainter& painter) const override;
    double distanceTo(const QPointF& point) const override;
    void translate(const QPointF& delta) override;
    std::unique_ptr<Entity> clone() const override;
    QString displayName() const override;
    std::vector<EditHandle> editHandles() const override;
    std::unique_ptr<Entity> withDraggedHandle(const QString& handleId, const QPointF& worldPos) const override;
    std::vector<QPointF> snapPoints() const override;
    QList<GcodeMove> toGcodeMoves(double zSafe, double zCut, double feedRate) const override;
    QJsonObject toJson() const override;
    static std::unique_ptr<TextEntity> fromJson(const QJsonObject& json);

    // Trim support - text trimming is not directly supported
    TrimEnd getTrimPoint(const QPointF& clickPos, const QPointF& intersection) const override;
    QPointF pointAtTrimEnd(TrimEnd end) const override;

    // Accessors
    QPointF position() const { return m_position; }
    QString text() const { return m_text; }
    double fontSize() const { return m_fontSize; }
    FontMode fontMode() const { return m_fontMode; }
    QString fontFamily() const { return m_fontFamily; }
    QString vectorFontPath() const { return m_vectorFontPath; }
    double letterSpacing() const { return m_letterSpacing; }
    double lineHeight() const { return m_lineHeight; }
    double maxWidth() const { return m_maxWidth; }
    bool singleStroke() const { return m_singleStroke; }
    int ssResolution() const { return m_ssResolution; }
    double ssSmoothing() const { return m_ssSmoothing; }
    int ssCleanup() const { return m_ssCleanup; }
    double ssMergeGap() const { return m_ssMergeGap; }
    double ssMinSegLen() const { return m_ssMinSegLen; }

    void setPosition(const QPointF& pos) { m_position = pos; }
    void setText(const QString& text) { m_text = text; }
    void setFontSize(double size) { m_fontSize = size; }
    void setFontFamily(const QString& family) { m_fontFamily = family; }
    void setVectorFont(const QString& fontFilePath);
    void setLetterSpacing(double spacing) { m_letterSpacing = spacing; }
    void setLineHeight(double height) { m_lineHeight = height; }
    void setMaxWidth(double width) { m_maxWidth = width; }
    void setSingleStroke(bool enabled) { m_singleStroke = enabled; }
    void setSsResolution(int v) { m_ssResolution = v; }
    void setSsSmoothing(double v) { m_ssSmoothing = v; }
    void setSsCleanup(int v) { m_ssCleanup = v; }
    void setSsMergeGap(double v) { m_ssMergeGap = v; }
    void setSsMinSegLen(double v) { m_ssMinSegLen = v; }
    QString autoWrappedText() const;

private:
    using GlyphData = QMap<QChar, QList<QList<QPointF>>>;

    QRectF computeBoundingRect() const;
    QList<QList<QPointF>> computeVectorStrokes() const;
    const QList<QPolygonF>& singleStrokes() const;

    QPointF m_position;
    QString m_text;
    double m_fontSize;
    double m_letterSpacing = 1.2;
    double m_lineHeight = 1.5;    // multiplier of fontSize
    double m_maxWidth = 0.0;  // 0 = no wrapping
    bool m_singleStroke = false;
    int m_ssResolution = 256;     // SingleStroke glyph render size (px)
    double m_ssSmoothing = 1.5;   // RDP epsilon
    int m_ssCleanup = 8;          // branch prune length (px)
    double m_ssMergeGap = 4.0;     // merge chain endpoints closer than this (px)
    double m_ssMinSegLen = 3.0;    // discard short segments (px)
    FontMode m_fontMode = FontMode::SystemFont;
    QString m_fontFamily = "Sans Serif";
    QString m_vectorFontPath;
    GlyphData m_vectorFontData;

    mutable QList<QPolygonF> m_ssCache;
    mutable bool m_ssCacheValid = false;
    mutable QVector<QLineF> m_ssLineCache;   // batched line segments for draw
    mutable bool m_ssLineCacheValid = false;
    mutable QPointF m_ssLineCachePos;         // position when line cache was built
    mutable QPainterPath m_outlinePathCache;  // cached system font outline path
    mutable bool m_outlinePathCacheValid = false;
    mutable QPixmap m_outlinePixmapCache;    // pre-rendered text pixmap
    mutable bool m_outlinePixmapCacheValid = false;
    mutable QRectF m_outlinePixmapRect;      // world-space rect of the pixmap
    mutable QRectF m_boundingRectCache;
    mutable bool m_boundingRectCacheValid = false;
};
