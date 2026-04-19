#pragma once

#include "Tool.h"

#include <optional>
#include <QString>

/// Text placement tool.
/// Click to set position, then configure text, font, and size in a dialog.
class TextTool final : public Tool
{
public:
    TextTool(Document& document, CommandStack& commandStack);

    QString name() const override;
    QString instructions() const override;

    void mousePress(const QPointF& pos) override;
    void mouseMove(const QPointF& pos) override;
    void mouseRelease(const QPointF& pos) override;
    void cancel() override;
    void paintPreview(QPainter& painter) const override;
    bool isActive() const override;

    // Font settings (persisted across placements)
    void setFontFamily(const QString& family) { m_fontFamily = family; }
    void setVectorFontPath(const QString& path) { m_vectorFontPath = path; }
    void setFontSize(double size) { m_fontSize = size; }
    void setUseVectorFont(bool use) { m_useVectorFont = use; }

private:
    QPointF m_currentPoint;
    bool m_placing = false;

    // Persistent font settings
    QString m_fontFamily = "Sans Serif";
    QString m_vectorFontPath = ":/fonts/fonts/futural.jhf";
    double m_fontSize = 10.0;
    bool m_useVectorFont = true;
};
