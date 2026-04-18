#pragma once

#include "Entity.h"

#include <QString>
#include <QFont>
#include <QMap>
#include <QList>

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
    std::vector<EditHandle> editHandles() const override;
    std::unique_ptr<Entity> withDraggedHandle(const QString& handleId, const QPointF& worldPos) const override;
    std::vector<QPointF> snapPoints() const override;
    QList<GcodeMove> toGcodeMoves(double zSafe, double zCut, double feedRate) const override;
    QJsonObject toJson() const override;
    static std::unique_ptr<TextEntity> fromJson(const QJsonObject& json);

    // Accessors
    QPointF position() const { return m_position; }
    QString text() const { return m_text; }
    double fontSize() const { return m_fontSize; }
    FontMode fontMode() const { return m_fontMode; }
    QString fontFamily() const { return m_fontFamily; }
    QString vectorFontPath() const { return m_vectorFontPath; }
    double letterSpacing() const { return m_letterSpacing; }
    double maxWidth() const { return m_maxWidth; }

    void setPosition(const QPointF& pos) { m_position = pos; }
    void setText(const QString& text) { m_text = text; }
    void setFontSize(double size) { m_fontSize = size; }
    void setFontFamily(const QString& family) { m_fontFamily = family; }
    void setVectorFont(const QString& fontFilePath);
    void setLetterSpacing(double spacing) { m_letterSpacing = spacing; }
    void setMaxWidth(double width) { m_maxWidth = width; }
    QString autoWrappedText() const;

private:
    using GlyphData = QMap<QChar, QList<QList<QPointF>>>;

    QRectF computeBoundingRect() const;
    QList<QList<QPointF>> computeVectorStrokes() const;

    QPointF m_position;
    QString m_text;
    double m_fontSize;
    double m_letterSpacing = 1.2;
    double m_maxWidth = 0.0;  // 0 = no wrapping
    FontMode m_fontMode = FontMode::SystemFont;
    QString m_fontFamily = "Sans Serif";
    QString m_vectorFontPath;
    GlyphData m_vectorFontData;
};
