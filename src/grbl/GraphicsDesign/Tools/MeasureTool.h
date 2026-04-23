#pragma once

#include "Tool.h"

/// Interactive two-point measurement tool.
/// Click first point, then move/click second point to measure distance, delta
/// X/Y and angle.  No entities are created; results are read by the panel via
/// the public accessors below.
class MeasureTool final : public Tool
{
public:
    using Tool::Tool;

    // --- Tool interface ---
    QString name()         const override { return "Measure"; }
    QString instructions() const override;

    void mousePress(const QPointF &pos)   override;
    void mouseMove(const QPointF &pos)    override;
    void mouseRelease(const QPointF &pos) override;
    void cancel()                         override;
    void paintPreview(QPainter &painter)  const override;
    bool isActive()                       const override { return m_hasFirst; }

    // --- Result accessors (for the side panel) ---
    bool   hasMeasurement() const { return m_hasFirst; }
    double distance()       const { return m_distance; }
    double deltaX()         const { return m_deltaX; }
    double deltaY()         const { return m_deltaY; }
    /// Angle in degrees, measured CCW from positive X axis.
    double angle()          const { return m_angle; }

private:
    bool    m_hasFirst = false;
    QPointF m_p1;
    QPointF m_p2;           ///< Tracks live cursor / final second point.
    double  m_distance = 0.0;
    double  m_deltaX   = 0.0;
    double  m_deltaY   = 0.0;
    double  m_angle    = 0.0;

    void updateFrom(const QPointF &p2);
};
