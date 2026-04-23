#pragma once

#include "PropertyPage.h"

class RadiusDimensionEntity;
class QCheckBox;

/// Property editor page for RadiusDimensionEntity.
/// Uses QGridLayout with Apply button.
/// Displays: Center X/Y, Radius, Angle, Diameter mode checkbox
class RadiusDimensionPage : public PropertyPage
{
    Q_OBJECT

public:
    explicit RadiusDimensionPage(QWidget *parent = nullptr);

    void bind(const Entity *entity) override;
    std::unique_ptr<Entity> apply() const override;
    int entityType() const override;

private:
    QDoubleSpinBox *m_centerX;
    QDoubleSpinBox *m_centerY;
    QDoubleSpinBox *m_radius;
    QDoubleSpinBox *m_angle;
    QCheckBox *m_diameter;
};