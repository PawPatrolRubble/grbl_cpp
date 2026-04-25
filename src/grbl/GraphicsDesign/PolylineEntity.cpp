#include "PolylineEntity.h"

#include <QJsonArray>
#include <QPainterPath>
#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kArcTolerance = 0.3; // mm — max deviation from fitted arc
constexpr int kMinArcPoints = 5;      // Minimum points for arc fitting

/// Fit a circle through three points. Returns false if collinear.
bool fitCircle(const QPointF& p1, const QPointF& p2, const QPointF& p3,
               QPointF& center, double& radius)
{
    double ax = p1.x(), ay = p1.y();
    double bx = p2.x(), by = p2.y();
    double cx = p3.x(), cy = p3.y();

    double D = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
    if (std::abs(D) < 1e-10) return false;

    double ux = ((ax*ax + ay*ay) * (by - cy) + (bx*bx + by*by) * (cy - ay) + (cx*cx + cy*cy) * (ay - by)) / D;
    double uy = ((ax*ax + ay*ay) * (cx - bx) + (bx*bx + by*by) * (ax - cx) + (cx*cx + cy*cy) * (bx - ax)) / D;

    center = QPointF(ux, uy);
    radius = std::hypot(ax - ux, ay - uy);
    return radius > 0.1 && radius < 1e5; // Reject degenerate/near-linear
}

/// Least-squares circle fit using algebraic method (Kasa).
/// More robust than 3-point fit for noisy data.
bool fitCircleLeastSquares(const QList<QPointF>& points, int start, int end,
                           QPointF& center, double& radius)
{
    int n = end - start;
    if (n < 3) return false;

    double sumX = 0, sumY = 0, sumX2 = 0, sumY2 = 0, sumXY = 0;
    double sumX3 = 0, sumY3 = 0, sumX2Y = 0, sumXY2 = 0;

    for (int i = start; i < end; ++i) {
        double x = points[i].x(), y = points[i].y();
        sumX += x; sumY += y;
        sumX2 += x*x; sumY2 += y*y;
        sumXY += x*y;
        sumX3 += x*x*x; sumY3 += y*y*y;
        sumX2Y += x*x*y; sumXY2 += x*y*y;
    }

    double A = n * sumX2 - sumX * sumX;
    double B = n * sumXY - sumX * sumY;
    double C = n * sumY2 - sumY * sumY;
    double D = 0.5 * (n * sumX3 + n * sumXY2 - sumX * sumX2 - sumX * sumY2);
    double E = 0.5 * (n * sumX2Y + n * sumY3 - sumY * sumX2 - sumY * sumY2);

    double denom = A * C - B * B;
    if (std::abs(denom) < 1e-10) return false;

    double cx = (D * C - B * E) / denom;
    double cy = (A * E - B * D) / denom;

    center = QPointF(cx, cy);

    // Compute radius as average distance
    double sumR = 0;
    for (int i = start; i < end; ++i) {
        sumR += std::hypot(points[i].x() - cx, points[i].y() - cy);
    }
    radius = sumR / n;

    return radius > 0.1 && radius < 1e5;
}

/// Check if all points in range [start, end) lie within tolerance of a circle.
/// Also checks that the arc sweep doesn't exceed 270° to avoid wild arcs.
bool allPointsOnArc(const QList<QPointF>& points, int start, int end,
                    const QPointF& center, double radius, double tolerance)
{
    for (int i = start; i < end; ++i) {
        double dist = std::hypot(points[i].x() - center.x(), points[i].y() - center.y());
        if (std::abs(dist - radius) > tolerance)
            return false;
    }

    // Check sweep angle doesn't exceed 270°
    double startAngle = std::atan2(points[start].y() - center.y(), points[start].x() - center.x());
    double endAngle = std::atan2(points[end - 1].y() - center.y(), points[end - 1].x() - center.x());
    double sweep = endAngle - startAngle;
    while (sweep > kPi) sweep -= 2 * kPi;
    while (sweep < -kPi) sweep += 2 * kPi;
    if (std::abs(sweep) > 1.5 * kPi) return false; // > 270°

    return true;
}

/// Determine CW/CCW by checking cross product along the path direction.
bool isCounterClockwise(const QPointF& center, const QList<QPointF>& points, int start, int end)
{
    // Sum cross products of consecutive vectors from center
    double crossSum = 0;
    for (int i = start; i + 1 < end; ++i) {
        double ax = points[i].x() - center.x(), ay = points[i].y() - center.y();
        double bx = points[i+1].x() - center.x(), by = points[i+1].y() - center.y();
        crossSum += ax * by - ay * bx;
    }
    return crossSum > 0;
}

} // namespace

PolylineEntity::PolylineEntity(const QList<QPointF>& points)
    : Entity(Kind::Polygon)  // Reuse Polygon kind for polylines
    , m_points(points)
{
}

Entity::Kind PolylineEntity::kind() const
{
    return Kind::Polygon;
}

QRectF PolylineEntity::boundingRect() const
{
    if (m_points.isEmpty()) return {};
    double minX = m_points[0].x(), maxX = minX;
    double minY = m_points[0].y(), maxY = minY;
    for (const auto& pt : m_points) {
        minX = std::min(minX, pt.x());
        maxX = std::max(maxX, pt.x());
        minY = std::min(minY, pt.y());
        maxY = std::max(maxY, pt.y());
    }
    return QRectF(minX, minY, maxX - minX, maxY - minY);
}

QPointF PolylineEntity::center() const
{
    return boundingRect().center();
}

void PolylineEntity::draw(QPainter& painter) const
{
    if (m_points.size() < 2) return;
    for (int i = 0; i + 1 < m_points.size(); ++i) {
        painter.drawLine(m_points[i], m_points[i + 1]);
    }
}

double PolylineEntity::distanceTo(const QPointF& point) const
{
    double minDist = std::numeric_limits<double>::max();
    for (int i = 0; i + 1 < m_points.size(); ++i) {
        // Distance from point to line segment
        QPointF a = m_points[i], b = m_points[i + 1];
        QPointF ab = b - a;
        double len2 = ab.x() * ab.x() + ab.y() * ab.y();
        double t = 0;
        if (len2 > 1e-10) {
            QPointF ap = point - a;
            t = std::clamp((ap.x() * ab.x() + ap.y() * ab.y()) / len2, 0.0, 1.0);
        }
        QPointF proj = a + t * ab;
        QPointF diff = point - proj;
        double dist = std::sqrt(diff.x() * diff.x() + diff.y() * diff.y());
        minDist = std::min(minDist, dist);
    }
    return minDist;
}

void PolylineEntity::translate(const QPointF& delta)
{
    for (auto& pt : m_points)
        pt += delta;
}

std::unique_ptr<Entity> PolylineEntity::clone() const
{
    auto c = std::make_unique<PolylineEntity>(m_points);
    copyBaseFieldsTo(c.get());
    return c;
}

std::vector<Entity::EditHandle> PolylineEntity::editHandles() const
{
    std::vector<EditHandle> handles;
    for (int i = 0; i < m_points.size(); ++i) {
        EditHandle h;
        h.id = QString("pt_%1").arg(i);
        h.worldPos = m_points[i];
        h.role = HandleRole::Resize;
        handles.push_back(h);
    }
    return handles;
}

std::unique_ptr<Entity> PolylineEntity::withDraggedHandle(const QString& handleId, const QPointF& worldPos) const
{
    auto copy = std::make_unique<PolylineEntity>(m_points);
    copy->setId(id());
    copy->setRotation(rotation());

    if (handleId.startsWith("pt_")) {
        int idx = handleId.mid(3).toInt();
        if (idx >= 0 && idx < copy->m_points.size())
            copy->m_points[idx] = worldPos;
    }
    return copy;
}

std::vector<QPointF> PolylineEntity::snapPoints() const
{
    std::vector<QPointF> snaps;
    for (const auto& pt : m_points)
        snaps.push_back(pt);
    return snaps;
}

QList<GcodeMove> PolylineEntity::toGcodeMoves(double zSafe, double zCut, double feedRate) const
{
    QList<GcodeMove> moves;
    if (m_points.size() < 2) return moves;

    // Rapid to start point
    GcodeMove rapid;
    rapid.type = GcodeMove::Type::Rapid;
    rapid.start = QVector3D(0, 0, zSafe);
    rapid.end = QVector3D(m_points[0].x(), m_points[0].y(), zSafe);
    moves.append(rapid);

    // Plunge
    GcodeMove plunge;
    plunge.type = GcodeMove::Type::Linear;
    plunge.start = QVector3D(m_points[0].x(), m_points[0].y(), zSafe);
    plunge.end = QVector3D(m_points[0].x(), m_points[0].y(), zCut);
    plunge.feedRate = feedRate;
    moves.append(plunge);

    // High-resolution G1 segments — GRBL look-ahead planner smooths these
    for (int i = 0; i + 1 < m_points.size(); ++i) {
        GcodeMove seg;
        seg.type = GcodeMove::Type::Linear;
        seg.start = QVector3D(m_points[i].x(), m_points[i].y(), zCut);
        seg.end = QVector3D(m_points[i + 1].x(), m_points[i + 1].y(), zCut);
        seg.feedRate = feedRate;
        moves.append(seg);
    }

    // Retract
    GcodeMove retract;
    retract.type = GcodeMove::Type::Rapid;
    retract.start = QVector3D(m_points.last().x(), m_points.last().y(), zCut);
    retract.end = QVector3D(m_points.last().x(), m_points.last().y(), zSafe);
    moves.append(retract);

    return moves;
}

QJsonObject PolylineEntity::toJson() const
{
    QJsonObject json;
    json["type"] = "polyline";
    QJsonArray pts;
    for (const auto& pt : m_points) {
        QJsonObject p;
        p["x"] = pt.x();
        p["y"] = pt.y();
        pts.append(p);
    }
    json["points"] = pts;
    json["rotation"] = rotation();
    return json;
}

QString PolylineEntity::displayName() const
{
    return QString("Polyline (%1 pts)").arg(m_points.size());
}

std::unique_ptr<PolylineEntity> PolylineEntity::fromJson(const QJsonObject& json)
{
    QList<QPointF> points;
    QJsonArray pts = json["points"].toArray();
    for (const auto& val : pts) {
        QJsonObject p = val.toObject();
        points.append(QPointF(p["x"].toDouble(), p["y"].toDouble()));
    }
    auto entity = std::make_unique<PolylineEntity>(points);
    entity->setRotation(json["rotation"].toDouble());
    return entity;
}
