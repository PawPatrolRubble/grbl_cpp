#include "TrimTool.h"

#include "../Document.h"
#include "../Entity.h"
#include "../LineEntity.h"
#include "../ArcEntity.h"
#include "../CircleEntity.h"
#include "../PolylineEntity.h"
#include "../RectangleEntity.h"
#include "../CommandStack.h"
#include "../GeometryUtils.h"
#include "../TrimModification.h"

#include <QDebug>
#include <QPainter>
#include <QLineF>
#include <QRectF>
#include <algorithm>
#include <cmath>

namespace {

constexpr double kClickRadius = 15.0;  // world units for hover detection

// Find nearest entity to point (lines, arcs, circles)
int nearestEntity(const QPointF& worldPos, const Document& doc)
{
    int best = -1;
    double bestDist = kClickRadius;

    for (int i = 0; i < doc.entityCount(); ++i) {
        const Entity* e = doc.entityAt(i);
        if (!e || !doc.isEntityVisible(e)) continue;

        double dist = e->distanceTo(worldPos);
        if (dist < bestDist) {
            bestDist = dist;
            best = i;
        }
    }
    return best;
}

} // namespace

// ---------------------------------------------------------------------------
// TrimTool implementation
// ---------------------------------------------------------------------------

TrimTool::TrimTool(Document& doc, CommandStack& cs)
    : Tool(doc, cs)
{
}

void TrimTool::setPhase(Phase phase)
{
    m_phase = phase;
    m_hoverEntityIdx = -1;
    m_fenceCrossedEntities.clear();
}

QString TrimTool::name() const
{
    return "Trim";
}

QString TrimTool::instructions() const
{
    if (m_phase == Phase::SelectCuttingEdges) {
        return "Select cutting edge entities (Shift+click). Enter or click empty space for quick trim.";
    }
    if (m_fenceMode) {
        return "Drag to draw fence crossing entities to trim. 'F' for fence mode. Esc to exit.";
    }
    return "Click entity to trim at nearest intersection. Shift+click to set cutting edges. Esc to cancel.";
}

void TrimTool::cancel()
{
    if (m_phase == Phase::TrimObjects && m_fenceDrawing) {
        m_fenceDrawing = false;
        m_fenceCrossedEntities.clear();
    } else {
        m_cuttingEdges.clear();
        m_phase = Phase::SelectCuttingEdges;
        m_hoverEntityIdx = -1;
    }
}

bool TrimTool::isActive() const
{
    return m_fenceDrawing || m_phase == Phase::TrimObjects;
}

// ---------------------------------------------------------------------------
// Cutting edges management
// ---------------------------------------------------------------------------

void TrimTool::addCuttingEdge(const QUuid& id)
{
    m_cuttingEdges.insert(id);
}

void TrimTool::removeCuttingEdge(const QUuid& id)
{
    m_cuttingEdges.remove(id);
}

void TrimTool::clearCuttingEdges()
{
    m_cuttingEdges.clear();
}

void TrimTool::finishCuttingEdgeSelection()
{
    // Empty cutting-edge selection means quick trim against all visible entities.
    m_phase = Phase::TrimObjects;
    m_hoverEntityIdx = -1;
}

// ---------------------------------------------------------------------------
// Mouse handling
// ---------------------------------------------------------------------------

void TrimTool::mousePress(const QPointF& pos)
{
    if (m_phase == Phase::SelectCuttingEdges) {
        // Find entity under cursor and add as cutting edge
        int idx = nearestEntity(pos, document());
        if (idx >= 0) {
            const Entity* e = document().entityAt(idx);
            if (e) {
                if (m_cuttingEdges.contains(e->id())) {
                    removeCuttingEdge(e->id());  // Toggle off
                } else {
                    addCuttingEdge(e->id());      // Toggle on
                }
            }
        } else {
            // Click on empty space - finish cutting edge selection (quick trim mode)
            finishCuttingEdgeSelection();
        }
    } else if (m_phase == Phase::TrimObjects) {
        if (m_fenceMode) {
            // Start fence drawing
            m_fenceDrawing = true;
            m_fenceStart = pos;
            m_fenceEnd = pos;
            m_fenceCrossedEntities.clear();
        } else {
            // Quick trim: click entity to trim at nearest intersection
            qDebug() << "TrimTool::mousePress - trying to find entity at" << pos;
            qDebug() << "  document entityCount=" << document().entityCount();
            int idx = nearestEntity(pos, document());
            qDebug() << "  nearestEntity returned idx=" << idx;
            if (idx >= 0) {
                const Entity* e = document().entityAt(idx);
                qDebug() << "  entity kind=" << static_cast<int>(e->kind())
                         << "id=" << e->id().toString();
                if (e && !m_cuttingEdges.contains(e->id())) {
                    TrimModification trimmer(document());
                    applyTrimResult(idx, trimmer.quickTrim(e, idx, pos));
                } else {
                    qDebug() << "  entity is cutting edge or null, skipping";
                }
            } else {
                qDebug() << "  no entity found near click position";
            }
        }
    }
}

void TrimTool::mouseMove(const QPointF& pos)
{
    if (m_phase == Phase::TrimObjects) {
        if (m_fenceDrawing) {
            m_fenceEnd = pos;
            m_fenceCrossedEntities = findEntitiesCrossedByFence(m_fenceStart, m_fenceEnd);
        } else {
            // Update hover for preview
            m_hoverEntityIdx = nearestEntity(pos, document());
        }
    }
}

void TrimTool::mouseRelease(const QPointF& pos)
{
    if (m_fenceDrawing) {
        m_fenceEnd = pos;
        m_fenceDrawing = false;
        trimEntitiesByFence();
    }
}

// ---------------------------------------------------------------------------
// Fence mode
// ---------------------------------------------------------------------------

std::vector<int> TrimTool::findEntitiesCrossedByFence(const QPointF& p1, const QPointF& p2) const
{
    std::vector<int> result;

    for (int i = 0; i < document().entityCount(); ++i) {
        const Entity* e = document().entityAt(i);
        if (!e || !document().isEntityVisible(e)) continue;
        if (m_cuttingEdges.contains(e->id())) continue;  // Skip cutting edges

        bool crosses = false;

        if (e->kind() == Entity::Kind::Line) {
            const auto* line = static_cast<const LineEntity*>(e);
            auto intersection = GeometryUtils::lineLineIntersection(p1, p2,
                line->startPoint(), line->endPoint());
            crosses = intersection.has_value();
        } else if (e->kind() == Entity::Kind::Arc) {
            const auto* arc = static_cast<const ArcEntity*>(e);
            auto ts = GeometryUtils::lineCircleParams(p1, p2, arc->center(), arc->radius());
            for (double t : ts) {
                if (t > GeometryUtils::kEps && t < 1.0 - GeometryUtils::kEps) {
                    QPointF hit = GeometryUtils::pointOnCircle(arc->center(), arc->radius(),
                        GeometryUtils::pointAngle(p1 + t * (p2 - p1), arc->center()));
                    if (GeometryUtils::isPointOnArc(hit, arc->center(), arc->radius(),
                            arc->startAngleDegrees(), arc->sweepAngleDegrees())) {
                        crosses = true;
                        break;
                    }
                }
            }
        } else if (e->kind() == Entity::Kind::Circle) {
            const auto* circle = static_cast<const CircleEntity*>(e);
            auto ts = GeometryUtils::lineCircleParams(p1, p2, circle->center(), circle->radius());
            for (double t : ts) {
                if (t > GeometryUtils::kEps && t < 1.0 - GeometryUtils::kEps) {
                    crosses = true;
                    break;
                }
            }
        } else if (e->kind() == Entity::Kind::Polygon) {
            const auto* poly = static_cast<const PolylineEntity*>(e);
            const auto& pts = poly->points();
            for (size_t seg = 0; seg + 1 < pts.size(); ++seg) {
                auto intersection = GeometryUtils::lineLineIntersection(p1, p2, pts[seg], pts[seg + 1]);
                if (intersection.has_value()) {
                    crosses = true;
                    break;
                }
            }
        } else if (e->kind() == Entity::Kind::Rectangle) {
            const auto* rect = static_cast<const RectangleEntity*>(e);
            auto corners = rect->corners();
            for (int i = 0; i < 4; ++i) {
                auto intersection = GeometryUtils::lineLineIntersection(p1, p2,
                    corners[i], corners[(i + 1) % 4]);
                if (intersection.has_value()) {
                    crosses = true;
                    break;
                }
            }
        }

        if (crosses) {
            result.push_back(i);
        }
    }

    return result;
}

void TrimTool::trimEntitiesByFence()
{
    // For each entity crossed by fence, trim at fence intersection point
    for (int entityIdx : m_fenceCrossedEntities) {
        const Entity* e = document().entityAt(entityIdx);
        if (!e) continue;

        // Find intersection of fence line with this entity
        std::vector<QPointF> intersections;
        for (int edgeIdx = 0; edgeIdx < document().entityCount(); ++edgeIdx) {
            if (edgeIdx == entityIdx) continue;
            const Entity* edge = document().entityAt(edgeIdx);
            if (!edge || !document().isEntityVisible(edge)) continue;
            if (!m_cuttingEdges.isEmpty() && !m_cuttingEdges.contains(edge->id())) continue;

            auto pts = GeometryUtils::entityIntersections(e, edge);
            for (const auto& pt : pts) {
                intersections.push_back(pt);
            }
        }

        if (intersections.empty()) continue;

        // Use TrimModification to trim at the intersection
        TrimModification trimmer(document());

        // Find intersection nearest to fence midpoint
        QPointF fenceMid = (m_fenceStart + m_fenceEnd) / 2.0;
        QPointF nearestIntersection;
        double nearestDist = std::numeric_limits<double>::max();
        for (const auto& pt : intersections) {
            double dist = QLineF(pt, fenceMid).length();
            if (dist < nearestDist) {
                nearestDist = dist;
                nearestIntersection = pt;
            }
        }

        applyTrimResult(entityIdx, trimmer.trimAtIntersection(e, nearestIntersection, fenceMid));
    }

    m_fenceCrossedEntities.clear();
}

// ---------------------------------------------------------------------------
// Apply trim result via command stack
// ---------------------------------------------------------------------------

void TrimTool::applyTrimResult(int entityIdx, TrimModification::TrimResult&& result)
{
    if (!result.success) return;

    const Entity* original = document().entityAt(entityIdx);

    if (result.replacements.empty()) {
        // Entity completely removed
        commandStack().execute(std::make_unique<DeleteEntityCommand>(entityIdx));
    } else if (result.replacements.size() == 1) {
        // Single replacement
        commandStack().execute(std::make_unique<UpdateEntityCommand>(
            entityIdx, original->clone(), std::move(result.replacements[0])));
    } else {
        // Multiple replacements (e.g., trim in middle creates two segments)
        commandStack().execute(std::make_unique<UpdateEntityCommand>(
            entityIdx, original->clone(), std::move(result.replacements[0])));
        for (size_t i = 1; i < result.replacements.size(); ++i) {
            commandStack().execute(std::make_unique<AddEntityCommand>(std::move(result.replacements[i])));
        }
    }

    m_hoverEntityIdx = -1;
}

// ---------------------------------------------------------------------------
// Preview painting
// ---------------------------------------------------------------------------

void TrimTool::paintPreview(QPainter& painter) const
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Phase 1: Highlight cutting edges
    if (m_phase == Phase::SelectCuttingEdges) {
        QPen cuttingEdgePen(QColor(255, 165, 0), 3);  // Orange for cutting edges
        cuttingEdgePen.setCosmetic(true);
        painter.setPen(cuttingEdgePen);
        for (const QUuid& id : m_cuttingEdges) {
            int idx = document().indexOf(id);
            if (idx < 0) continue;
            const Entity* e = document().entityAt(idx);
            if (e) e->draw(painter);
        }
    }

    // Phase 2: Trim preview
    if (m_phase == Phase::TrimObjects) {
        // Show cutting edges in different color
        QPen cuttingEdgePen(QColor(255, 165, 0, 150), 2);
        cuttingEdgePen.setCosmetic(true);
        painter.setPen(cuttingEdgePen);
        for (const QUuid& id : m_cuttingEdges) {
            int idx = document().indexOf(id);
            if (idx < 0) continue;
            const Entity* e = document().entityAt(idx);
            if (e) e->draw(painter);
        }

        // Highlight hovered entity
        if (!m_fenceDrawing && m_hoverEntityIdx >= 0) {
            const Entity* e = document().entityAt(m_hoverEntityIdx);
            if (e && !m_cuttingEdges.contains(e->id())) {
                // Show entity that will be trimmed in red highlight
                QPen hoverPen(QColor(220, 50, 50, 150), 2);
                hoverPen.setCosmetic(true);
                painter.setPen(hoverPen);
                e->draw(painter);
            }
        }

        // Fence mode preview
        if (m_fenceDrawing) {
            // Draw fence line
            QPen fencePen(QColor(255, 100, 100), 2);
            fencePen.setStyle(Qt::DashLine);
            fencePen.setCosmetic(true);
            painter.setPen(fencePen);
            painter.drawLine(m_fenceStart, m_fenceEnd);

            // Highlight entities crossed by fence
            QPen crossedPen(QColor(220, 50, 50), 2);
            crossedPen.setCosmetic(true);
            painter.setPen(crossedPen);
            for (int idx : m_fenceCrossedEntities) {
                const Entity* e = document().entityAt(idx);
                if (e) e->draw(painter);
            }
        }
    }

    painter.restore();
}