#include "DxfParser.h"
#include "GraphicsDesign/LineEntity.h"
#include "GraphicsDesign/CircleEntity.h"
#include "GraphicsDesign/ArcEntity.h"
#include "GraphicsDesign/PolylineEntity.h"

#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <cmath>

namespace {
constexpr double kPi = 3.14159265358979323846;

// Check if a group pair starts a new entity
bool isEntityStart(int code, const QString& value)
{
    return code == 0 && (value == "LINE" || value == "CIRCLE" || value == "ARC" ||
                         value == "LWPOLYLINE" || value == "POLYLINE" || value == "SPLINE" ||
                         value == "ELLIPSE" || value == "POINT");
}

// Evaluate cubic B-spline at parameter t using De Boor's algorithm
QPointF evalBSpline(const QList<QPointF>& controlPts, const QList<double>& knots, int degree, double t)
{
    int n = controlPts.size();
    if (n == 0) return {};

    // Find knot span
    int span = degree;
    for (int i = degree; i < n; ++i) {
        if (t < knots[i + 1]) { span = i; break; }
    }
    if (t >= knots[n]) span = n - 1;

    // De Boor's algorithm
    QList<QPointF> d;
    for (int j = 0; j <= degree; ++j) {
        int idx = span - degree + j;
        if (idx >= 0 && idx < n)
            d.append(controlPts[idx]);
        else
            d.append(QPointF(0, 0));
    }

    for (int r = 1; r <= degree; ++r) {
        for (int j = degree; j >= r; --j) {
            int ki = span - degree + j;
            double denom = knots[ki + degree - r + 1] - knots[ki];
            double alpha = (denom > 1e-10) ? (t - knots[ki]) / denom : 0.0;
            d[j] = d[j - 1] * (1.0 - alpha) + d[j] * alpha;
        }
    }
    return d[degree];
}

} // namespace

QList<DxfParser::GroupPair> DxfParser::readGroupPairs(const QString& filePath)
{
    QList<GroupPair> pairs;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_error = "Cannot open file: " + filePath;
        return pairs;
    }

    // Read entire file at once for speed
    QByteArray data = file.readAll();
    file.close();

    // Reserve estimated capacity (2 lines per pair, ~20 chars avg)
    pairs.reserve(data.size() / 20);

    int pos = 0;
    int len = data.size();
    const char* d = data.constData();

    auto readLine = [&]() -> QByteArray {
        int start = pos;
        while (pos < len && d[pos] != '\n' && d[pos] != '\r') pos++;
        QByteArray line = QByteArray::fromRawData(d + start, pos - start);
        // Skip line endings
        if (pos < len && d[pos] == '\r') pos++;
        if (pos < len && d[pos] == '\n') pos++;
        return line;
    };

    while (pos < len) {
        QByteArray codeLine = readLine();
        if (pos >= len) break;
        QByteArray valueLine = readLine();

        bool ok;
        int code = codeLine.trimmed().toInt(&ok);
        if (!ok) continue;

        pairs.append({code, QString::fromUtf8(valueLine.trimmed())});
    }
    return pairs;
}

std::vector<std::unique_ptr<Entity>> DxfParser::parse(const QString& filePath)
{
    std::vector<std::unique_ptr<Entity>> entities;
    m_error.clear();

    auto pairs = readGroupPairs(filePath);
    if (pairs.isEmpty()) return entities;

    // Find ENTITIES section
    for (int i = 0; i < pairs.size(); ++i) {
        if (pairs[i].code == 2 && pairs[i].value == "ENTITIES") {
            parseEntitiesSection(pairs, i + 1, entities);
            break;
        }
    }

    if (entities.empty() && m_error.isEmpty())
        m_error = "No supported entities found in DXF file.";

    return entities;
}

void DxfParser::parseEntitiesSection(const QList<GroupPair>& pairs, int startIdx,
                                      std::vector<std::unique_ptr<Entity>>& entities)
{
    int i = startIdx;
    while (i < pairs.size()) {
        if (pairs[i].code == 0 && pairs[i].value == "ENDSEC")
            break;

        if (pairs[i].code == 0) {
            QString entityType = pairs[i].value;
            i++; // Move past entity type marker

            std::unique_ptr<Entity> entity;
            if (entityType == "LINE")
                entity = parseLine(pairs, i);
            else if (entityType == "CIRCLE")
                entity = parseCircle(pairs, i);
            else if (entityType == "ARC")
                entity = parseArc(pairs, i);
            else if (entityType == "LWPOLYLINE")
                entity = parseLwPolyline(pairs, i);
            else if (entityType == "POLYLINE")
                entity = parsePolyline(pairs, i);
            else if (entityType == "SPLINE")
                entity = parseSpline(pairs, i);
            else if (entityType == "ELLIPSE")
                entity = parseEllipse(pairs, i);
            else {
                // Skip unknown entity
                while (i < pairs.size() && !(pairs[i].code == 0))
                    i++;
                continue;
            }

            if (entity)
                entities.push_back(std::move(entity));
        } else {
            i++;
        }
    }
}

std::unique_ptr<Entity> DxfParser::parseLine(const QList<GroupPair>& pairs, int& idx)
{
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0;

    while (idx < pairs.size() && pairs[idx].code != 0) {
        switch (pairs[idx].code) {
        case 10: x1 = pairs[idx].value.toDouble(); break;
        case 20: y1 = pairs[idx].value.toDouble(); break;
        case 11: x2 = pairs[idx].value.toDouble(); break;
        case 21: y2 = pairs[idx].value.toDouble(); break;
        }
        idx++;
    }
    return std::make_unique<LineEntity>(QPointF(x1, y1), QPointF(x2, y2));
}

std::unique_ptr<Entity> DxfParser::parseCircle(const QList<GroupPair>& pairs, int& idx)
{
    double cx = 0, cy = 0, radius = 0;

    while (idx < pairs.size() && pairs[idx].code != 0) {
        switch (pairs[idx].code) {
        case 10: cx = pairs[idx].value.toDouble(); break;
        case 20: cy = pairs[idx].value.toDouble(); break;
        case 40: radius = pairs[idx].value.toDouble(); break;
        }
        idx++;
    }
    return std::make_unique<CircleEntity>(QPointF(cx, cy), radius);
}

std::unique_ptr<Entity> DxfParser::parseArc(const QList<GroupPair>& pairs, int& idx)
{
    double cx = 0, cy = 0, radius = 0;
    double startAngle = 0, endAngle = 360;

    while (idx < pairs.size() && pairs[idx].code != 0) {
        switch (pairs[idx].code) {
        case 10: cx = pairs[idx].value.toDouble(); break;
        case 20: cy = pairs[idx].value.toDouble(); break;
        case 40: radius = pairs[idx].value.toDouble(); break;
        case 50: startAngle = pairs[idx].value.toDouble(); break;
        case 51: endAngle = pairs[idx].value.toDouble(); break;
        }
        idx++;
    }

    double sweep = endAngle - startAngle;
    if (sweep <= 0) sweep += 360.0;

    return std::make_unique<ArcEntity>(QPointF(cx, cy), radius, startAngle, sweep);
}

std::unique_ptr<Entity> DxfParser::parseLwPolyline(const QList<GroupPair>& pairs, int& idx)
{
    QList<QPointF> points;
    double curX = 0, curY = 0;
    bool hasX = false;
    int flags = 0;

    while (idx < pairs.size() && pairs[idx].code != 0) {
        switch (pairs[idx].code) {
        case 10:
            if (hasX) {
                // Save previous point before starting new one
                points.append(QPointF(curX, curY));
            }
            curX = pairs[idx].value.toDouble();
            hasX = true;
            break;
        case 20:
            curY = pairs[idx].value.toDouble();
            break;
        case 70:
            flags = pairs[idx].value.toInt();
            break;
        }
        idx++;
    }
    // Don't forget the last point
    if (hasX)
        points.append(QPointF(curX, curY));

    // Close if flag bit 0 is set
    if ((flags & 1) && points.size() >= 2 && points.first() != points.last())
        points.append(points.first());

    if (points.size() >= 2)
        return std::make_unique<PolylineEntity>(points);
    return nullptr;
}

std::unique_ptr<Entity> DxfParser::parsePolyline(const QList<GroupPair>& pairs, int& idx)
{
    // Old-style POLYLINE: vertices follow as VERTEX entities until SEQEND
    QList<QPointF> points;
    int flags = 0;

    // Read POLYLINE properties
    while (idx < pairs.size() && pairs[idx].code != 0) {
        if (pairs[idx].code == 70)
            flags = pairs[idx].value.toInt();
        idx++;
    }

    // Read VERTEX entities
    while (idx < pairs.size()) {
        if (pairs[idx].code == 0 && pairs[idx].value == "SEQEND") {
            idx++;
            // Skip SEQEND properties
            while (idx < pairs.size() && pairs[idx].code != 0)
                idx++;
            break;
        }
        if (pairs[idx].code == 0 && pairs[idx].value == "VERTEX") {
            idx++;
            double x = 0, y = 0;
            while (idx < pairs.size() && pairs[idx].code != 0) {
                if (pairs[idx].code == 10) x = pairs[idx].value.toDouble();
                if (pairs[idx].code == 20) y = pairs[idx].value.toDouble();
                idx++;
            }
            points.append(QPointF(x, y));
        } else {
            break; // Unexpected entity
        }
    }

    // Close if flag bit 0 is set
    if ((flags & 1) && points.size() >= 2 && points.first() != points.last())
        points.append(points.first());

    if (points.size() >= 2)
        return std::make_unique<PolylineEntity>(points);
    return nullptr;
}

std::unique_ptr<Entity> DxfParser::parseSpline(const QList<GroupPair>& pairs, int& idx)
{
    QList<QPointF> controlPts;
    QList<double> knots;
    int degree = 3;
    int flags = 0;
    double curX = 0, curY = 0;
    bool hasCtrlX = false;
    QList<QPointF> fitPoints;
    double fitX = 0, fitY = 0;
    bool hasFitX = false;

    while (idx < pairs.size() && pairs[idx].code != 0) {
        switch (pairs[idx].code) {
        case 70: flags = pairs[idx].value.toInt(); break;
        case 71: degree = pairs[idx].value.toInt(); break;
        case 40: knots.append(pairs[idx].value.toDouble()); break;
        case 10:
            if (hasCtrlX)
                controlPts.append(QPointF(curX, curY));
            curX = pairs[idx].value.toDouble();
            hasCtrlX = true;
            break;
        case 20:
            curY = pairs[idx].value.toDouble();
            break;
        case 11:
            if (hasFitX)
                fitPoints.append(QPointF(fitX, fitY));
            fitX = pairs[idx].value.toDouble();
            hasFitX = true;
            break;
        case 21:
            fitY = pairs[idx].value.toDouble();
            break;
        }
        idx++;
    }
    if (hasCtrlX)
        controlPts.append(QPointF(curX, curY));
    if (hasFitX)
        fitPoints.append(QPointF(fitX, fitY));

    // If we have fit points but no control points, use fit points directly
    if (controlPts.isEmpty() && !fitPoints.isEmpty()) {
        if (fitPoints.size() >= 2)
            return std::make_unique<PolylineEntity>(fitPoints);
        return nullptr;
    }

    if (controlPts.size() < 2)
        return nullptr;

    // Generate knot vector if not provided
    if (knots.isEmpty()) {
        int n = controlPts.size();
        int knotCount = n + degree + 1;
        for (int i = 0; i < knotCount; ++i) {
            if (i <= degree) knots.append(0.0);
            else if (i >= knotCount - degree - 1) knots.append(1.0);
            else knots.append(static_cast<double>(i - degree) / (knotCount - 2 * degree - 1));
        }
    }

    // Evaluate spline at regular intervals
    QList<QPointF> points;
    int segments = std::max(static_cast<int>(controlPts.size()) * 4, 20);
    double tMin = knots[degree];
    double tMax = knots[controlPts.size()];

    for (int i = 0; i <= segments; ++i) {
        double t = tMin + (tMax - tMin) * i / segments;
        points.append(evalBSpline(controlPts, knots, degree, t));
    }

    // Close if closed flag
    if ((flags & 1) && points.size() >= 2 && points.first() != points.last())
        points.append(points.first());

    if (points.size() >= 2)
        return std::make_unique<PolylineEntity>(points);
    return nullptr;
}

std::unique_ptr<Entity> DxfParser::parseEllipse(const QList<GroupPair>& pairs, int& idx)
{
    double cx = 0, cy = 0;
    double mx = 0, my = 0;
    double ratio = 1.0;
    double startParam = 0.0;
    double endParam = 2.0 * M_PI;

    while (idx < pairs.size() && pairs[idx].code != 0) {
        int code = pairs[idx].code;
        const QString& val = pairs[idx].value;
        switch (code) {
        case 10: cx = val.toDouble(); break;
        case 20: cy = val.toDouble(); break;
        case 11: mx = val.toDouble(); break;
        case 21: my = val.toDouble(); break;
        case 40: ratio = val.toDouble(); break;
        case 41: startParam = val.toDouble(); break;
        case 42: endParam = val.toDouble(); break;
        }
        idx++;
    }

    double majorLen = std::sqrt(mx * mx + my * my);
    double majorAngle = std::atan2(my, mx);
    double minorLen = majorLen * ratio;

    QList<QPointF> points;
    int segments = 36;
    double span = endParam - startParam;
    if (span <= 0) span += 2.0 * M_PI;

    for (int i = 0; i <= segments; ++i) {
        double t = startParam + span * i / segments;
        double ex = majorLen * std::cos(t);
        double ey = minorLen * std::sin(t);
        double rx = ex * std::cos(majorAngle) - ey * std::sin(majorAngle);
        double ry = ex * std::sin(majorAngle) + ey * std::cos(majorAngle);
        points.append(QPointF(cx + rx, cy + ry));
    }

    if (points.size() >= 2)
        return std::make_unique<PolylineEntity>(points);
    return nullptr;
}
