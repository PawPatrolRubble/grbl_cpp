#pragma once

#include "Tool.h"
#include <QPointF>

/// Two-click fillet tool.
/// Click first line, then second line — inserts an arc tangent to both lines
/// at the intersection corner and trims both lines back to the tangent points.
class FilletTool final : public Tool
{
public:
    FilletTool(Document& doc, CommandStack& cs);

    double filletRadius() const { return m_radius; }
    void setFilletRadius(double r);

    // Tool interface
    QString name() const override;
    QString instructions() const override;
    void mousePress(const QPointF& pos) override;
    void mouseMove(const QPointF& pos) override;
    void mouseRelease(const QPointF& pos) override;
    void cancel() override;
    void paintPreview(QPainter& painter) const override;
    bool isActive() const override;

private:
    double m_radius = 5.0;   // default 5 mm

    int    m_firstIdx   = -1;
    double m_firstParam = 0.0;
    int    m_hoverIdx   = -1;
    double m_hoverParam = 0.0;

    // Computed preview data
    bool   m_previewValid = false;
    QPointF m_arcCenter;
    double  m_arcRadius   = 0.0;
    double  m_arcStart    = 0.0;   // degrees
    double  m_arcSweep    = 0.0;   // degrees (positive = CCW)
    QPointF m_tan1;                // tangent point on first line
    QPointF m_tan2;                // tangent point on second line

    bool computeFillet(int firstIdx, int secondIdx);
    int  nearestLine(const QPointF& worldPos, double& paramOut) const;
};
