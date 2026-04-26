#include "GeometryUtils.h"

#include "Entity.h"
#include "LineEntity.h"
#include "ArcEntity.h"
#include "CircleEntity.h"

#include <algorithm>
#include <cmath>

namespace GeometryUtils {

// === Angle Utilities ===

double normAngle(double a)
{
    a = std::fmod(a, 360.0);
    return a < 0.0 ? a + 360.0 : a;
}

double pointAngle(const QPointF& pt, const QPointF& center)
{
    return normAngle(std::atan2(pt.y() - center.y(), pt.x() - center.x()) * 180.0 / kPi);
}

bool angleInArc(double angleDeg, double startDeg, double sweepDeg)
{
    // Normalize relative angle
    double a = normAngle(angleDeg - startDeg);
    double s = std::abs(sweepDeg);

    // Handle zero sweep
    if (s < kEps) {
        return std::abs(a) < 0.5 || std::abs(a - 360.0) < 0.5;
    }

    // Check if angle is within sweep range
    if (sweepDeg >= 0) {
        return a <= s + 0.5 || a >= 360.0 - 0.5;
    } else {
        // For negative sweep (CW direction)
        return a >= 360.0 - s - 0.5 || a <= 0.5;
    }
}

// === Line-Line Intersections ===

bool lineLineParams(const QPointF& p1, const QPointF& p2,
                    const QPointF& q1, const QPointF& q2,
                    double& t, double& u)
{
    double dx = p2.x() - p1.x();
    double dy = p2.y() - p1.y();
    double ex = q2.x() - q1.x();
    double ey = q2.y() - q1.y();

    // Cross product of direction vectors
    double denom = dx * ey - dy * ex;
    if (std::abs(denom) < kEps) {
        return false;  // Lines are parallel
    }

    double fx = q1.x() - p1.x();
    double fy = q1.y() - p1.y();

    t = (fx * ey - fy * ex) / denom;
    u = (fx * dy - fy * dx) / denom;

    return true;
}

std::optional<QPointF> lineLineIntersection(const QPointF& p1, const QPointF& p2,
                                             const QPointF& q1, const QPointF& q2)
{
    double t, u;
    if (!lineLineParams(p1, p2, q1, q2, t, u)) {
        return std::nullopt;
    }

    // Check if intersection is within both segments
    if (t >= 0.0 && t <= 1.0 && u >= 0.0 && u <= 1.0) {
        return p1 + t * (p2 - p1);
    }
    return std::nullopt;
}

// === Line-Circle Intersections ===

std::vector<double> lineCircleParams(const QPointF& p1, const QPointF& p2,
                                      const QPointF& center, double r)
{
    std::vector<double> results;

    double dx = p2.x() - p1.x();
    double dy = p2.y() - p1.y();
    double fx = p1.x() - center.x();
    double fy = p1.y() - center.y();

    double a = dx * dx + dy * dy;
    if (a < kEps * kEps) {
        return results;  // Degenerate line (p1 == p2)
    }

    double b = 2.0 * (fx * dx + fy * dy);
    double c = fx * fx + fy * fy - r * r;

    double disc = b * b - 4.0 * a * c;
    if (disc < 0.0) {
        return results;  // No intersection
    }

    double sq = std::sqrt(disc);

    // Two intersection parameters
    double t1 = (-b - sq) / (2.0 * a);
    double t2 = (-b + sq) / (2.0 * a);

    // Sort and return
    if (t1 < t2) {
        results.push_back(t1);
        results.push_back(t2);
    } else {
        results.push_back(t2);
        results.push_back(t1);
    }

    return results;
}

std::vector<QPointF> lineCircleIntersections(const QPointF& p1, const QPointF& p2,
                                              const QPointF& center, double r)
{
    std::vector<QPointF> results;

    auto params = lineCircleParams(p1, p2, center, r);
    for (double t : params) {
        if (t >= 0.0 && t <= 1.0) {
            results.push_back(p1 + t * (p2 - p1));
        }
    }

    return results;
}

// === Circle-Circle Intersections ===

std::vector<QPointF> circleCircleIntersections(const QPointF& c1, double r1,
                                                const QPointF& c2, double r2)
{
    std::vector<QPointF> results;

    double d = std::hypot(c2.x() - c1.x(), c2.y() - c1.y());

    // No intersection cases
    if (d > r1 + r2 + kEps) {
        return results;  // Circles are separate
    }
    if (d < std::abs(r1 - r2) - kEps) {
        return results;  // One circle inside another
    }
    if (d < kEps) {
        return results;  // Coincident circles (infinite intersections)
    }

    // Calculate intersection points using geometry
    double a = (r1 * r1 - r2 * r2 + d * d) / (2.0 * d);
    double hSq = r1 * r1 - a * a;
    if (hSq < 0.0) {
        hSq = 0.0;  // Numerical safety
    }
    double h = std::sqrt(hSq);

    // Point on line between centers at distance a from c1
    QPointF p2 = c1 + a * (c2 - c1) / d;

    // Perpendicular offset
    QPointF offset = h * QPointF(-(c2.y() - c1.y()) / d, (c2.x() - c1.x()) / d);

    results.push_back(p2 + offset);
    if (h > kEps) {
        results.push_back(p2 - offset);
    }

    return results;
}

// === Arc Utilities ===

bool isPointOnArc(const QPointF& p, const QPointF& center, double radius,
                  double startDeg, double sweepDeg)
{
    // Check radius
    double dist = std::hypot(p.x() - center.x(), p.y() - center.y());
    if (std::abs(dist - radius) > 0.1) {
        return false;
    }

    // Check angle
    double angle = pointAngle(p, center);
    return angleInArc(angle, startDeg, sweepDeg);
}

QPointF pointOnCircle(const QPointF& center, double radius, double angleDeg)
{
    double rad = degToRad(angleDeg);
    return QPointF(center.x() + radius * std::cos(rad),
                   center.y() + radius * std::sin(rad));
}

// === Distance Utilities ===

double distanceToSegment(const QPointF& pt, const QPointF& p1, const QPointF& p2, double& t)
{
    QPointF d = p2 - p1;
    double lenSq = d.x() * d.x() + d.y() * d.y();

    if (lenSq < kEps * kEps) {
        t = 0.0;
        return std::hypot(pt.x() - p1.x(), pt.y() - p1.y());
    }

    // Project point onto line
    QPointF f = pt - p1;
    t = (f.x() * d.x() + f.y() * d.y()) / lenSq;

    // Clamp to segment
    t = std::clamp(t, 0.0, 1.0);

    QPointF proj = p1 + t * d;
    return std::hypot(pt.x() - proj.x(), pt.y() - proj.y());
}

// === Entity-Level Intersections ===

std::vector<QPointF> entityIntersections(const Entity* e1, const Entity* e2)
{
    std::vector<QPointF> results;

    if (!e1 || !e2) {
        return results;
    }

    auto k1 = e1->kind();
    auto k2 = e2->kind();

    // Line-Line
    if (k1 == Entity::Kind::Line && k2 == Entity::Kind::Line) {
        const auto* l1 = static_cast<const LineEntity*>(e1);
        const auto* l2 = static_cast<const LineEntity*>(e2);
        auto pt = lineLineIntersection(l1->startPoint(), l1->endPoint(),
                                       l2->startPoint(), l2->endPoint());
        if (pt) {
            results.push_back(*pt);
        }
    }
    // Line-Circle
    else if (k1 == Entity::Kind::Line && k2 == Entity::Kind::Circle) {
        const auto* line = static_cast<const LineEntity*>(e1);
        const auto* circle = static_cast<const CircleEntity*>(e2);
        results = lineCircleIntersections(line->startPoint(), line->endPoint(),
                                          circle->center(), circle->radius());
    }
    // Circle-Line (swap)
    else if (k1 == Entity::Kind::Circle && k2 == Entity::Kind::Line) {
        return entityIntersections(e2, e1);
    }
    // Line-Arc
    else if (k1 == Entity::Kind::Line && k2 == Entity::Kind::Arc) {
        const auto* line = static_cast<const LineEntity*>(e1);
        const auto* arc = static_cast<const ArcEntity*>(e2);
        auto pts = lineCircleIntersections(line->startPoint(), line->endPoint(),
                                           arc->center(), arc->radius());
        for (const auto& pt : pts) {
            if (isPointOnArc(pt, arc->center(), arc->radius(),
                            arc->startAngleDegrees(), arc->sweepAngleDegrees())) {
                results.push_back(pt);
            }
        }
    }
    // Arc-Line (swap)
    else if (k1 == Entity::Kind::Arc && k2 == Entity::Kind::Line) {
        return entityIntersections(e2, e1);
    }
    // Circle-Circle
    else if (k1 == Entity::Kind::Circle && k2 == Entity::Kind::Circle) {
        const auto* c1 = static_cast<const CircleEntity*>(e1);
        const auto* c2 = static_cast<const CircleEntity*>(e2);
        results = circleCircleIntersections(c1->center(), c1->radius(),
                                            c2->center(), c2->radius());
    }
    // Arc-Circle and Circle-Arc
    else if ((k1 == Entity::Kind::Arc && k2 == Entity::Kind::Circle) ||
             (k1 == Entity::Kind::Circle && k2 == Entity::Kind::Arc)) {
        const auto* arc = static_cast<const ArcEntity*>(k1 == Entity::Kind::Arc ? e1 : e2);
        const auto* circle = static_cast<const CircleEntity*>(k1 == Entity::Kind::Circle ? e1 : e2);
        auto pts = circleCircleIntersections(arc->center(), arc->radius(),
                                             circle->center(), circle->radius());
        for (const auto& pt : pts) {
            if (isPointOnArc(pt, arc->center(), arc->radius(),
                            arc->startAngleDegrees(), arc->sweepAngleDegrees())) {
                results.push_back(pt);
            }
        }
    }
    // Arc-Arc (lines first approach - basic implementation)
    else if (k1 == Entity::Kind::Arc && k2 == Entity::Kind::Arc) {
        const auto* a1 = static_cast<const ArcEntity*>(e1);
        const auto* a2 = static_cast<const ArcEntity*>(e2);
        auto pts = circleCircleIntersections(a1->center(), a1->radius(),
                                             a2->center(), a2->radius());
        for (const auto& pt : pts) {
            if (isPointOnArc(pt, a1->center(), a1->radius(),
                            a1->startAngleDegrees(), a1->sweepAngleDegrees()) &&
                isPointOnArc(pt, a2->center(), a2->radius(),
                            a2->startAngleDegrees(), a2->sweepAngleDegrees())) {
                results.push_back(pt);
            }
        }
    }

    return results;
}

std::vector<double> intersectionParamsOnEntity(const Entity* e1, const Entity* e2, int e1Index)
{
    std::vector<double> params;

    if (!e1 || !e2 || e1 == e2) {
        return params;
    }

    auto k1 = e1->kind();
    auto k2 = e2->kind();

    // Line with other entities
    if (k1 == Entity::Kind::Line) {
        const auto* line = static_cast<const LineEntity*>(e1);
        QPointF p1 = line->startPoint();
        QPointF p2 = line->endPoint();

        if (k2 == Entity::Kind::Line) {
            const auto* other = static_cast<const LineEntity*>(e2);
            double t, u;
            if (lineLineParams(p1, p2, other->startPoint(), other->endPoint(), t, u)) {
                if (t > kEps && t < 1.0 - kEps && u > kEps && u < 1.0 - kEps) {
                    params.push_back(t);
                }
            }
        }
        else if (k2 == Entity::Kind::Circle) {
            const auto* circle = static_cast<const CircleEntity*>(e2);
            for (double t : lineCircleParams(p1, p2, circle->center(), circle->radius())) {
                if (t > kEps && t < 1.0 - kEps) {
                    params.push_back(t);
                }
            }
        }
        else if (k2 == Entity::Kind::Arc) {
            const auto* arc = static_cast<const ArcEntity*>(e2);
            for (double t : lineCircleParams(p1, p2, arc->center(), arc->radius())) {
                if (t > kEps && t < 1.0 - kEps) {
                    QPointF hit = p1 + t * (p2 - p1);
                    if (isPointOnArc(hit, arc->center(), arc->radius(),
                                    arc->startAngleDegrees(), arc->sweepAngleDegrees())) {
                        params.push_back(t);
                    }
                }
            }
        }
    }
    // Arc with other entities (returns angle parameters)
    else if (k1 == Entity::Kind::Arc) {
        const auto* arc = static_cast<const ArcEntity*>(e1);

        auto intersections = entityIntersections(e1, e2);
        for (const auto& pt : intersections) {
            double angle = pointAngle(pt, arc->center());
            double start = arc->startAngleDegrees();
            double sweep = arc->sweepAngleDegrees();

            // Convert angle to parameter along arc (0 to 1)
            double relAngle = normAngle(angle - start);
            if (sweep < 0) {
                // CW arc - adjust calculation
                relAngle = 360.0 - relAngle;
            }
            double param = relAngle / std::abs(sweep);
            if (param > kEps && param < 1.0 - kEps) {
                params.push_back(param);
            }
        }
    }

    std::sort(params.begin(), params.end());
    params.erase(std::unique(params.begin(), params.end(),
                             [](double a, double b) { return std::abs(a - b) < kEps; }),
                 params.end());

    return params;
}

std::vector<QPointF> findAllIntersections(const Entity* entity, int excludeIndex,
                                          const std::vector<const Entity*>& allEntities)
{
    std::vector<QPointF> results;

    for (size_t i = 0; i < allEntities.size(); ++i) {
        if (static_cast<int>(i) == excludeIndex) {
            continue;
        }
        const Entity* other = allEntities[i];
        if (!other) {
            continue;
        }

        auto pts = entityIntersections(entity, other);
        results.insert(results.end(), pts.begin(), pts.end());
    }

    // Remove duplicates
    std::sort(results.begin(), results.end(),
              [](const QPointF& a, const QPointF& b) {
                  return a.x() < b.x() || (a.x() == b.x() && a.y() < b.y());
              });
    results.erase(std::unique(results.begin(), results.end(),
                              [](const QPointF& a, const QPointF& b) {
                                  return std::hypot(a.x() - b.x(), a.y() - b.y()) < 0.1;
                              }),
                 results.end());

    return results;
}

} // namespace GeometryUtils