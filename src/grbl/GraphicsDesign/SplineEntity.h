#pragma once

#include "Entity.h"

#include <QList>
#include <QVector>

/// Catmull-Rom spline entity.
///
/// The user defines a sequence of *through-points* (knots); the resulting
/// curve passes smoothly through every one of them using the centripetal
/// Catmull-Rom algorithm.  Phantom duplicate-endpoint padding is applied so
/// the curve reaches the first and last knots exactly.
///
/// G-code output is dense G1 linear segments — GRBL's look-ahead planner
/// smooths these automatically on hardware.
class SplineEntity final : public Entity
{
public:
    explicit SplineEntity(const QList<QPointF>& controlPoints);

    // --- Entity interface ---
    Kind        kind()          const override { return Kind::Spline; }
    QRectF      boundingRect()  const override;
    QPointF     center()        const override;
    void        draw(QPainter& painter)        const override;
    double      distanceTo(const QPointF& pt)  const override;
    void        translate(const QPointF& delta)      override;
    std::unique_ptr<Entity> clone()            const override;

    std::vector<EditHandle>  editHandles() const override;
    std::unique_ptr<Entity>  withDraggedHandle(const QString& handleId,
                                               const QPointF& worldPos) const override;
    std::vector<QPointF>     snapPoints()   const override;

    QList<GcodeMove> toGcodeMoves(double zSafe, double zCut, double feedRate) const override;
    QJsonObject      toJson()               const override;
    QString          displayName()          const override { return "Spline"; }

    static std::unique_ptr<SplineEntity> fromJson(const QJsonObject& json);

    // --- Spline-specific API ---
    const QList<QPointF>& controlPoints()    const { return m_points; }
    void setControlPoints(const QList<QPointF>& pts) { m_points = pts; invalidateCache(); }
    int  pointCount()                         const { return m_points.size(); }

    /// Number of linear segments per inter-knot span (higher = smoother G-code).
    int  stepsPerSegment()                    const { return m_steps; }
    void setStepsPerSegment(int s)                  { m_steps = std::max(4, s); invalidateCache(); }

private:
    QList<QPointF>       m_points;
    int                  m_steps  = 20;

    mutable QVector<QPointF> m_cache;
    mutable bool             m_cacheValid = false;

    void                      invalidateCache() const { m_cacheValid = false; }
    const QVector<QPointF>&   evaluatedCurve()  const;

    /// Catmull-Rom point at parameter t∈[0,1] for the span p1→p2.
    static QPointF catmullRom(const QPointF& p0, const QPointF& p1,
                              const QPointF& p2, const QPointF& p3, double t);
};
