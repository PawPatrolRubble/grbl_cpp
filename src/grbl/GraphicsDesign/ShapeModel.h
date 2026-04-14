#pragma once

#include <QObject>
#include <QList>
#include <QSet>
#include <QJsonDocument>
#include "Shape.h"

/// Manages a collection of shapes with selection support.
class ShapeModel : public QObject
{
    Q_OBJECT

public:
    explicit ShapeModel(QObject *parent = nullptr);
    ~ShapeModel();

    // Shape management
    void addShape(Shape *shape);
    void removeShape(const QUuid &id);
    void clear();
    QList<Shape*> shapes() const { return m_shapes; }
    Shape* shapeById(const QUuid &id) const;
    int count() const { return m_shapes.size(); }

    // Selection
    void select(const QUuid &id);
    void deselect(const QUuid &id);
    void clearSelection();
    void setSelected(const QUuid &id, bool selected);
    QSet<QUuid> selection() const { return m_selectedIds; }
    QList<Shape*> selectedShapes() const;
    bool isSelected(const QUuid &id) const;
    Shape* selectedShape() const;  // Returns first selected or null

    // G-code generation
    QStringList generateGcode(double feedRateXY, double plungeRateZ,
                              double safeZ, double cutZ) const;

    // Serialization
    QJsonDocument toJson() const;
    bool loadFromJson(const QJsonDocument &doc);

signals:
    void shapeAdded(Shape *shape);
    void shapeRemoved(const QUuid &id);
    void shapeChanged(const QUuid &id);
    void selectionChanged();
    void modelChanged();

private:
    QList<Shape*> m_shapes;
    QSet<QUuid> m_selectedIds;
};