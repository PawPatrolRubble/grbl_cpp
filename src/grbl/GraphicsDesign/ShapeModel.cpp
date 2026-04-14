#include "ShapeModel.h"
#include "LineShape.h"
#include "RectShape.h"
#include "CircleShape.h"
#include "ArcShape.h"
#include "../GcodeParser.h"
#include <QJsonArray>

ShapeModel::ShapeModel(QObject *parent)
    : QObject(parent)
{
}

ShapeModel::~ShapeModel()
{
    clear();
}

void ShapeModel::addShape(Shape *shape)
{
    m_shapes.append(shape);
    emit shapeAdded(shape);
    emit modelChanged();
}

void ShapeModel::removeShape(const QUuid &id)
{
    for (int i = 0; i < m_shapes.size(); ++i) {
        if (m_shapes[i]->id() == id) {
            Shape *shape = m_shapes[i];
            m_shapes.removeAt(i);
            m_selectedIds.remove(id);
            delete shape;
            emit shapeRemoved(id);
            emit modelChanged();
            return;
        }
    }
}

void ShapeModel::clear()
{
    for (Shape *shape : m_shapes) {
        delete shape;
    }
    m_shapes.clear();
    m_selectedIds.clear();
    emit modelChanged();
}

Shape* ShapeModel::shapeById(const QUuid &id) const
{
    for (Shape *shape : m_shapes) {
        if (shape->id() == id)
            return shape;
    }
    return nullptr;
}

void ShapeModel::select(const QUuid &id)
{
    if (shapeById(id)) {
        m_selectedIds.insert(id);
        emit selectionChanged();
    }
}

void ShapeModel::deselect(const QUuid &id)
{
    if (m_selectedIds.remove(id)) {
        emit selectionChanged();
    }
}

void ShapeModel::clearSelection()
{
    if (!m_selectedIds.isEmpty()) {
        m_selectedIds.clear();
        emit selectionChanged();
    }
}

void ShapeModel::setSelected(const QUuid &id, bool selected)
{
    if (selected)
        select(id);
    else
        deselect(id);
}

QList<Shape*> ShapeModel::selectedShapes() const
{
    QList<Shape*> result;
    for (Shape *shape : m_shapes) {
        if (m_selectedIds.contains(shape->id()))
            result.append(shape);
    }
    return result;
}

bool ShapeModel::isSelected(const QUuid &id) const
{
    return m_selectedIds.contains(id);
}

Shape* ShapeModel::selectedShape() const
{
    for (Shape *shape : m_shapes) {
        if (m_selectedIds.contains(shape->id()))
            return shape;
    }
    return nullptr;
}

QStringList ShapeModel::generateGcode(double feedRateXY, double plungeRateZ,
                                        double safeZ, double cutZ) const
{
    QStringList gcode;
    gcode << "G21 G90        ; metric, absolute";
    gcode << QString("G0 Z%1        ; raise pen").arg(safeZ, 0, 'f', 2);

    for (Shape *shape : m_shapes) {
        QList<GcodeMove> moves = shape->toGcodeMoves(safeZ, cutZ, feedRateXY);
        QStringList shapeGcode = GcodeParser::movesToGcode(moves);
        gcode << shapeGcode;
    }

    gcode << QString("G0 Z%1        ; final raise").arg(safeZ, 0, 'f', 2);
    gcode << "; End of design G-code";
    return gcode;
}

QJsonDocument ShapeModel::toJson() const
{
    QJsonObject root;
    QJsonArray shapesArray;

    for (Shape *shape : m_shapes) {
        shapesArray.append(shape->toJson());
    }

    root["shapes"] = shapesArray;
    return QJsonDocument(root);
}

bool ShapeModel::loadFromJson(const QJsonDocument &doc)
{
    clear();

    QJsonObject root = doc.object();
    QJsonArray shapesArray = root["shapes"].toArray();

    for (const QJsonValue &val : shapesArray) {
        Shape *shape = Shape::fromJson(val.toObject());
        if (shape) {
            m_shapes.append(shape);
        }
    }

    emit modelChanged();
    return true;
}