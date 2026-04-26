#pragma once

#include "Tool.h"
#include "../TrimModification.h"

#include <QPointF>
#include <optional>
#include <vector>
#include <QSet>
#include <QUuid>

/// AutoCAD-style Trim tool with two-phase workflow and fence mode.
///
/// Quick-trim mode (default): Click entity to trim at nearest intersection.
/// Shift+click: Select cutting edges explicitly.
/// Fence mode: Drag across entities to trim multiple at once.
///
class TrimTool final : public Tool
{
public:
    enum class Phase {
        SelectCuttingEdges,  // Select boundary entities (Shift+click)
        TrimObjects          // Trim objects at intersections
    };

    TrimTool(Document& doc, CommandStack& cs);

    void setPhase(Phase phase);
    Phase phase() const { return m_phase; }

    // Tool interface
    QString name() const override;
    QString instructions() const override;
    void mousePress(const QPointF& pos) override;
    void mouseMove(const QPointF& pos) override;
    void mouseRelease(const QPointF& pos) override;
    void cancel() override;
    void paintPreview(QPainter& painter) const override;
    bool isActive() const override;

    // Cutting edges management
    void addCuttingEdge(const QUuid& id);
    void removeCuttingEdge(const QUuid& id);
    const QSet<QUuid>& cuttingEdges() const { return m_cuttingEdges; }
    void clearCuttingEdges();
    void finishCuttingEdgeSelection();  // Proceed to trim phase

    // Fence mode
    void enableFenceMode(bool enable) { m_fenceMode = enable; }
    bool fenceMode() const { return m_fenceMode; }

private:
    Phase m_phase = Phase::TrimObjects;  // Start in trim mode for quick-trim workflow
    QSet<QUuid> m_cuttingEdges;  // Selected cutting edge entities

    // Trim phase state
    int m_hoverEntityIdx = -1;   // Entity under cursor

    // Fence mode state
    bool m_fenceMode = false;
    bool m_fenceDrawing = false;
    QPointF m_fenceStart;
    QPointF m_fenceEnd;
    std::vector<int> m_fenceCrossedEntities;  // Entities crossed by fence line

    // Helper methods
    std::vector<int> findEntitiesCrossedByFence(const QPointF& p1, const QPointF& p2) const;
    void trimEntitiesByFence();
    void applyTrimResult(int entityIdx, TrimModification::TrimResult&& result);
};