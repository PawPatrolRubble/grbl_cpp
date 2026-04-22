#pragma once

#include "Tool.h"
#include <QPointF>

/// Two-click chamfer tool.
/// Click first line, then second line — inserts a chamfer (angled) line at their
/// intersection corner and trims both lines back by the chamfer distance.
class ChamferTool final : public Tool
{
public:
    ChamferTool(Document& doc, CommandStack& cs);

    // Distance from the corner to each chamfer point (machine units, e.g. mm)
    double chamferDistance() const { return m_chamferDist; }
    void setChamferDistance(double d);

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
    double m_chamferDist = 5.0;   // default 5 mm

    // First click state
    int     m_firstIdx  = -1;      // entity index of first line picked
    double  m_firstParam = 0.0;    // parameter on first line near click

    // Hover state (second line candidate while first is locked)
    int     m_hoverIdx  = -1;
    double  m_hoverParam = 0.0;

    // Preview result (filled in updatePreview when both lines known)
    bool    m_previewValid = false;
    QPointF m_chamferP1;           // chamfer start on first line
    QPointF m_chamferP2;           // chamfer end on second line
    QPointF m_corner;              // intersection corner (informational)

    void updatePreview(int firstIdx, int secondIdx);
    int nearestLine(const QPointF& worldPos, double& paramOut) const;
};
