#pragma once

#include "PropertyPage.h"

class AngularDimensionEntity;

/// Property editor page for AngularDimensionEntity.
/// Uses QGridLayout with Apply button.
/// Displays: Vertex X/Y, Ray 1 X/Y, Ray 2 X/Y, Arc Radius
class AngularDimensionPage : public PropertyPage
{
    Q_OBJECT

public:
    explicit AngularDimensionPage(QWidget *parent = nullptr);

    void bind(const Entity *entity) override;
    std::unique_ptr<Entity> apply() const override;
    int entityType() const override;

private:
    QDoubleSpinBox *m_vertexX;
    QDoubleSpinBox *m_vertexY;
    QDoubleSpinBox *m_p1X;
    QDoubleSpinBox *m_p1Y;
    QDoubleSpinBox *m_p2X;
    QDoubleSpinBox *m_p2Y;
    QDoubleSpinBox *m_radius;
};