#include "RectShape.h"
#include <cmath>

RectShape::RectShape(const QPointF &origin, double width, double height)
    : Shape(Type::Rectangle)
    , m_origin(origin)
    , m_width(width)
    , m_height(height)
{
}

void RectShape::setOrigin(const QPointF &p) { m_origin = p; }
void RectShape::setWidth(double w) { m_width = w; }
void RectShape::setHeight(double h) { m_height = h; }

QRectF RectShape::boundingRect() const
{
    return QRectF(m_origin, QSizeF(m_width, m_height));
}

void RectShape::draw(QPainterPath &path) const
{
    path.addRect(boundingRect());
}

QList<QPointF> RectShape::handlePositions() const
{
    return {
        m_origin,
        QPointF(m_origin.x() + m_width, m_origin.y()),
        QPointF(m_origin.x() + m_width, m_origin.y() + m_height),
        QPointF(m_origin.x(), m_origin.y() + m_height)
    };
}

QList<GcodeMove> RectShape::toGcodeMoves(double zSafe, double zCut, double feedRate) const
{
    QList<GcodeMove> moves;

    double x1 = m_origin.x();
    double y1 = m_origin.y();
    double x2 = x1 + m_width;
    double y2 = y1 + m_height;

    // G0 rapid to start corner (pen up)
    GcodeMove rapid1;
    rapid1.type = GcodeMove::Type::Rapid;
    rapid1.start = QVector3D(0, 0, zSafe);
    rapid1.end = QVector3D(x1, y1, zSafe);
    moves.append(rapid1);

    // G1 plunge
    GcodeMove plunge;
    plunge.type = GcodeMove::Type::Linear;
    plunge.start = QVector3D(x1, y1, zSafe);
    plunge.end = QVector3D(x1, y1, zCut);
    plunge.feedRate = feedRate;
    moves.append(plunge);

    // 4 edges
    GcodeMove edge1, edge2, edge3, edge4;
    edge1.type = edge2.type = edge3.type = edge4.type = GcodeMove::Type::Linear;
    edge1.feedRate = edge2.feedRate = edge3.feedRate = edge4.feedRate = feedRate;

    edge1.start = QVector3D(x1, y1, zCut);
    edge1.end = QVector3D(x2, y1, zCut);
    moves.append(edge1);

    edge2.start = QVector3D(x2, y1, zCut);
    edge2.end = QVector3D(x2, y2, zCut);
    moves.append(edge2);

    edge3.start = QVector3D(x2, y2, zCut);
    edge3.end = QVector3D(x1, y2, zCut);
    moves.append(edge3);

    edge4.start = QVector3D(x1, y2, zCut);
    edge4.end = QVector3D(x1, y1, zCut);
    moves.append(edge4);

    // G0 raise
    GcodeMove rapid2;
    rapid2.type = GcodeMove::Type::Rapid;
    rapid2.start = QVector3D(x1, y1, zCut);
    rapid2.end = QVector3D(x1, y1, zSafe);
    moves.append(rapid2);

    return moves;
}

QJsonObject RectShape::toJson() const
{
    QJsonObject json;
    json["type"] = "rect";
    json["x"] = m_origin.x();
    json["y"] = m_origin.y();
    json["width"] = m_width;
    json["height"] = m_height;
    json["rotation"] = m_rotation;
    return json;
}

RectShape* RectShape::fromJson(const QJsonObject &json)
{
    QPointF origin(json["x"].toDouble(), json["y"].toDouble());
    double w = json["width"].toDouble(10);
    double h = json["height"].toDouble(10);
    RectShape *shape = new RectShape(origin, w, h);
    shape->setRotation(json["rotation"].toDouble(0));
    return shape;
}

void RectShape::translate(double dx, double dy)
{
    m_origin += QPointF(dx, dy);
}

void RectShape::scale(double sx, double sy, const QPointF &pivot)
{
    m_origin = QPointF(pivot.x() + (m_origin.x() - pivot.x()) * sx,
                       pivot.y() + (m_origin.y() - pivot.y()) * sy);
    m_width *= sx;
    m_height *= sy;
}