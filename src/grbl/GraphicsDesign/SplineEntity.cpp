#include "SplineEntity.h"

#include <QPainter>
#include <QPainterPath>
#include <QJsonArray>
#include <QJsonObject>
#include <cmath>
#include <algorithm>
#include <limits>

// ===========================================================================
// Catmull-Rom evaluation
// ===========================================================================

QPointF SplineEntity::catmullRom(const QPointF& p0, const QPointF& p1,
                                  const QPointF& p2, const QPointF& p3, double t)
{
    double t2 = t * t;
    double t3 = t2 * t;
    // Uniform Catmull-Rom (alpha = 0)
    return 0.5 * (2.0 * p1
        + (-p0 + p2) * t
        + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2
        + (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t3);
}

const QVector<QPointF>& SplineEntity::evaluatedCurve() const
{
    if (m_cacheValid)
        return m_cache;

    m_cache.clear();
    const int n = m_points.size();

    if (n < 2) {
        if (n == 1) m_cache.append(m_points[0]);
        m_cacheValid = true;
        return m_cache;
    }

    // Phantom-endpoint clamping: extend beyond endpoints by duplicating
    auto p = [&](int i) -> const QPointF& {
        return m_points[std::clamp(i, 0, n - 1)];
    };

    for (int seg = 0; seg < n - 1; ++seg) {
        for (int step = 0; step <= m_steps; ++step) {
            double t = static_cast<double>(step) / m_steps;
            m_cache.append(catmullRom(p(seg - 1), p(seg), p(seg + 1), p(seg + 2), t));
        }
    }

    m_cacheValid = true;
    return m_cache;
}

// ===========================================================================
// Construction
// ===========================================================================

SplineEntity::SplineEntity(const QList<QPointF>& controlPoints)
    : Entity(Kind::Spline)
    , m_points(controlPoints)
{}

// ===========================================================================
// Entity interface
// ===========================================================================

QRectF SplineEntity::boundingRect() const
{
    const auto& pts = evaluatedCurve();
    if (pts.isEmpty()) return {};

    double minX = pts[0].x(), maxX = pts[0].x();
    double minY = pts[0].y(), maxY = pts[0].y();
    for (const QPointF& pt : pts) {
        minX = std::min(minX, pt.x());  maxX = std::max(maxX, pt.x());
        minY = std::min(minY, pt.y());  maxY = std::max(maxY, pt.y());
    }
    return QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
}

QPointF SplineEntity::center() const
{
    if (m_points.isEmpty()) return {};
    QPointF sum;
    for (const QPointF& p : m_points) sum += p;
    return sum / m_points.size();
}

void SplineEntity::draw(QPainter& painter) const
{
    const auto& pts = evaluatedCurve();
    if (pts.size() < 2) {
        if (!pts.isEmpty()) painter.drawPoint(pts[0]);
        return;
    }
    QPainterPath path;
    path.moveTo(pts[0]);
    for (int i = 1; i < pts.size(); ++i)
        path.lineTo(pts[i]);
    painter.drawPath(path);
}

double SplineEntity::distanceTo(const QPointF& point) const
{
    const auto& pts = evaluatedCurve();
    double minDist = std::numeric_limits<double>::max();

    for (int i = 0; i + 1 < pts.size(); ++i) {
        QPointF d = pts[i + 1] - pts[i];
        double len2 = d.x() * d.x() + d.y() * d.y();
        double t = (len2 > 1e-12)
            ? std::clamp(((point - pts[i]).x() * d.x() + (point - pts[i]).y() * d.y()) / len2, 0.0, 1.0)
            : 0.0;
        QPointF nearest = pts[i] + t * d;
        double dx = point.x() - nearest.x();
        double dy = point.y() - nearest.y();
        minDist = std::min(minDist, std::hypot(dx, dy));
    }
    return minDist;
}

void SplineEntity::translate(const QPointF& delta)
{
    for (QPointF& p : m_points) p += delta;
    invalidateCache();
}

std::unique_ptr<Entity> SplineEntity::clone() const
{
    auto copy = std::make_unique<SplineEntity>(m_points);
    copyBaseFieldsTo(copy.get());
    copy->m_steps = m_steps;
    return copy;
}

std::vector<Entity::EditHandle> SplineEntity::editHandles() const
{
    std::vector<EditHandle> handles;
    handles.reserve(static_cast<size_t>(m_points.size()));
    for (int i = 0; i < m_points.size(); ++i) {
        EditHandle h;
        h.id       = QString("pt%1").arg(i);
        h.worldPos = m_points[i];
        h.role     = HandleRole::Resize;
        handles.push_back(h);
    }
    return handles;
}

std::unique_ptr<Entity> SplineEntity::withDraggedHandle(const QString& handleId,
                                                         const QPointF& worldPos) const
{
    if (!handleId.startsWith("pt")) return clone();
    bool ok = false;
    int idx = handleId.mid(2).toInt(&ok);
    if (!ok || idx < 0 || idx >= m_points.size()) return clone();

    auto copy = std::unique_ptr<SplineEntity>(
        static_cast<SplineEntity*>(clone().release()));
    copy->m_points[idx] = worldPos;
    copy->invalidateCache();
    return copy;
}

std::vector<QPointF> SplineEntity::snapPoints() const
{
    std::vector<QPointF> pts;
    pts.reserve(static_cast<size_t>(m_points.size()));
    for (const QPointF& p : m_points) pts.push_back(p);
    return pts;
}

QList<GcodeMove> SplineEntity::toGcodeMoves(double zSafe, double zCut, double feedRate) const
{
    QList<GcodeMove> moves;
    const auto& pts = evaluatedCurve();
    if (pts.size() < 2) return moves;

    // Rapid to first point at safe height
    GcodeMove rapid;
    rapid.type  = GcodeMove::Type::Rapid;
    rapid.start = QVector3D(0, 0, static_cast<float>(zSafe));
    rapid.end   = QVector3D(static_cast<float>(pts[0].x()),
                             static_cast<float>(pts[0].y()),
                             static_cast<float>(zSafe));
    moves.append(rapid);

    // Plunge
    GcodeMove plunge;
    plunge.type     = GcodeMove::Type::Linear;
    plunge.start    = rapid.end;
    plunge.end      = QVector3D(static_cast<float>(pts[0].x()),
                                 static_cast<float>(pts[0].y()),
                                 static_cast<float>(zCut));
    plunge.feedRate = feedRate;
    moves.append(plunge);

    // Dense G1 segments along the curve
    for (int i = 0; i + 1 < pts.size(); ++i) {
        GcodeMove seg;
        seg.type     = GcodeMove::Type::Linear;
        seg.start    = QVector3D(static_cast<float>(pts[i].x()),
                                  static_cast<float>(pts[i].y()),
                                  static_cast<float>(zCut));
        seg.end      = QVector3D(static_cast<float>(pts[i + 1].x()),
                                  static_cast<float>(pts[i + 1].y()),
                                  static_cast<float>(zCut));
        seg.feedRate = feedRate;
        moves.append(seg);
    }

    // Retract
    GcodeMove retract;
    retract.type  = GcodeMove::Type::Rapid;
    retract.start = moves.last().end;
    retract.end   = QVector3D(retract.start.x(), retract.start.y(),
                               static_cast<float>(zSafe));
    moves.append(retract);

    return moves;
}

// ===========================================================================
// JSON serialization
// ===========================================================================

QJsonObject SplineEntity::toJson() const
{
    QJsonObject obj;
    obj["type"]             = "spline";
    obj["id"]               = m_id.toString();
    obj["layerId"]          = m_layerId.toString();
    obj["stepsPerSegment"]  = m_steps;

    QJsonArray arr;
    for (const QPointF& p : m_points) {
        QJsonObject pt;
        pt["x"] = p.x();
        pt["y"] = p.y();
        arr.append(pt);
    }
    obj["points"] = arr;
    return obj;
}

std::unique_ptr<SplineEntity> SplineEntity::fromJson(const QJsonObject& json)
{
    QList<QPointF> pts;
    const QJsonArray arr = json["points"].toArray();
    for (const auto& v : arr) {
        QJsonObject pt = v.toObject();
        pts.append(QPointF(pt["x"].toDouble(), pt["y"].toDouble()));
    }

    auto e = std::make_unique<SplineEntity>(pts);
    if (json.contains("id"))
        e->setId(QUuid::fromString(json["id"].toString()));
    if (json.contains("layerId"))
        e->setLayerId(QUuid::fromString(json["layerId"].toString()));
    if (json.contains("stepsPerSegment"))
        e->setStepsPerSegment(json["stepsPerSegment"].toInt(20));
    return e;
}
