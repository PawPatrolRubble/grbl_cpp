#pragma once

#include <QPointF>
#include <vector>
#include <optional>

class Entity;
class LineEntity;
class ArcEntity;
class CircleEntity;

/// Centralized geometry utilities for intersection calculations and trim operations.
/// Migrated from TrimTool.cpp, CanvasSnap.cpp, and FilletTool.cpp to avoid duplication.
namespace GeometryUtils {

/// Tolerance for floating-point comparisons
constexpr double kEps = 1e-9;
constexpr double kPi = 3.14159265358979323846;

// === Angle Utilities ===

/// Convert degrees to radians
inline double degToRad(double d) { return d * kPi / 180.0; }

/// Convert radians to degrees
inline double radToDeg(double r) { return r * 180.0 / kPi; }

/// Normalize angle to [0, 360) degrees
double normAngle(double a);

/// Compute angle of point relative to center (returns degrees [0, 360))
double pointAngle(const QPointF& pt, const QPointF& center);

/// Check if angle is within arc sweep range
bool angleInArc(double angleDeg, double startDeg, double sweepDeg);

// === Line-Line Intersections ===

/// Compute intersection parameters for two lines.
/// Returns true if lines intersect (not parallel), with:
/// - t: parameter on line (p1,p2) where intersection occurs (0=at p1, 1=at p2)
/// - u: parameter on line (q1,q2) where intersection occurs
/// Works on infinite lines; caller should check t,u bounds for segment intersection.
bool lineLineParams(const QPointF& p1, const QPointF& p2,
                    const QPointF& q1, const QPointF& q2,
                    double& t, double& u);

/// Compute segment intersection point (only if intersection is within both segments).
std::optional<QPointF> lineLineIntersection(const QPointF& p1, const QPointF& p2,
                                             const QPointF& q1, const QPointF& q2);

// === Line-Circle Intersections ===

/// Compute intersection parameters on infinite line with circle.
/// Returns sorted t values where line P(t) = p1 + t*(p2-p1) intersects circle.
std::vector<double> lineCircleParams(const QPointF& p1, const QPointF& p2,
                                      const QPointF& center, double r);

/// Compute segment intersection points with circle (only t in [0,1]).
std::vector<QPointF> lineCircleIntersections(const QPointF& p1, const QPointF& p2,
                                              const QPointF& center, double r);

// === Circle-Circle Intersections ===

/// Compute intersection points between two circles (up to 2 points).
std::vector<QPointF> circleCircleIntersections(const QPointF& c1, double r1,
                                                const QPointF& c2, double r2);

// === Arc Utilities ===

/// Check if a point is on an arc segment (within arc sweep).
bool isPointOnArc(const QPointF& p, const QPointF& center, double radius,
                  double startDeg, double sweepDeg);

/// Compute point on circle at given angle (degrees).
QPointF pointOnCircle(const QPointF& center, double radius, double angleDeg);

// === Distance Utilities ===

/// Distance from point to line segment, also returns parameter t in [0,1].
double distanceToSegment(const QPointF& pt, const QPointF& p1, const QPointF& p2, double& t);

// === Entity-Level Intersections ===

/// Find all intersection points between two entities.
std::vector<QPointF> entityIntersections(const Entity* e1, const Entity* e2);

/// Find intersection parameters on entity e1 with entity e2.
/// For LineEntity: returns t values in [0,1] for valid segment intersections.
/// For ArcEntity: returns angle parameters (degrees) within arc sweep.
std::vector<double> intersectionParamsOnEntity(const Entity* e1, const Entity* e2, int e1Index);

/// Find all intersections of an entity with all visible entities (excluding self).
std::vector<QPointF> findAllIntersections(const Entity* entity, int excludeIndex,
                                          const std::vector<const Entity*>& allEntities);

} // namespace GeometryUtils