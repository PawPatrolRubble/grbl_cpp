#pragma once

#include "Tool.h"

#include <QList>

/// Catmull-Rom spline drawing tool.
///
/// Click to place through-points; the preview shows the live smooth curve
/// through all placed points + cursor position.  Finish with Enter or
/// double-click; remove the last point with Backspace; cancel with Esc.
class SplineTool final : public Tool
{
public:
    SplineTool(Document& document, CommandStack& commandStack);

    QString name()         const override { return "Spline"; }
    QString instructions() const override;

    void mousePress(const QPointF& pos)   override;
    void mouseMove(const QPointF& pos)    override;
    void mouseRelease(const QPointF& pos) override;
    void cancel()                         override;
    void paintPreview(QPainter& painter)  const override;
    bool isActive()                       const override { return !m_points.isEmpty(); }

    /// Finalize and add the spline entity. Returns true on success.
    bool finish();

    /// Remove the last placed point. Returns true if a point was removed.
    bool removeLastPoint();

private:
    QList<QPointF> m_points;       ///< Confirmed through-points
    QPointF        m_cursor;       ///< Live cursor position

    /// Evaluate Catmull-Rom for the preview points list (includes m_cursor).
    QVector<QPointF> evaluatePreview(const QList<QPointF>& pts) const;

    static QPointF catmullRom(const QPointF& p0, const QPointF& p1,
                              const QPointF& p2, const QPointF& p3, double t);
};
