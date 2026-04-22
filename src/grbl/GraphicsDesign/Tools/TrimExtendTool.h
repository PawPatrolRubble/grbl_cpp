#pragma once

#include "Tool.h"

#include <QPointF>
#include <optional>
#include <vector>

/// Tool for trimming or extending LINE and ARC entities at their intersections
/// with other entities in the document.
///
/// Trim  — click a portion of a line/arc to remove it up to the nearest
///          intersections with other entities.
/// Extend — click near an endpoint of a line/arc to stretch it to the nearest
///          intersection with another entity.
class TrimExtendTool final : public Tool
{
public:
    enum class Mode { Trim, Extend };

    TrimExtendTool(Document& doc, CommandStack& cs, Mode mode);

    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

    // Tool interface
    QString name() const override;
    QString instructions() const override;
    void mousePress(const QPointF& pos) override;
    void mouseMove(const QPointF& pos) override;
    void mouseRelease(const QPointF& pos) override;
    void cancel() override;
    void paintPreview(QPainter& painter) const override;
    bool isActive() const override { return false; }

private:
    Mode m_mode;

    // Hover state
    int    m_hoverIdx  = -1;    // index of entity under cursor
    double m_hoverParam = 0.0;  // parameter [0,1] along entity at cursor
    double m_trimLo = 0.0;      // lower boundary of segment to trim
    double m_trimHi = 1.0;      // upper boundary of segment to trim
    bool   m_hasLo  = false;
    bool   m_hasHi  = false;

    // Extend state
    bool m_extendFromEnd = false;  // true = extend from end (t→1), false = from start (t→0)
    double m_extendTarget = 0.0;   // parameter to extend to (may be >1 or <0)
    bool m_canExtend = false;

    void updateHover(const QPointF& worldPos);

    // Returns sorted intersection parameters ∈ (0,1) on entity at `idx` with all other entities
    std::vector<double> computeCuts(int entityIdx) const;

    // Returns the parameter on a LINE entity if extended to the nearest cut target;
    // sets m_extendFromEnd. Returns false if no target found.
    bool computeExtend(int entityIdx, const QPointF& clickPos);
};
