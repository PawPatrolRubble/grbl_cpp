#pragma once

#include "PropertyPage.h"

class ArcEntity;

/// Property editor page for ArcEntity.
/// Displays: Center X/Y, Radius, Start Angle, Sweep Angle
class ArcPage : public PropertyPage
{
    Q_OBJECT

public:
    explicit ArcPage(QWidget *parent = nullptr);

    void bind(const Entity *entity) override;
    std::unique_ptr<Entity> apply() const override;
    int entityType() const override;

private:
    QDoubleSpinBox *m_centerX;
    QDoubleSpinBox *m_centerY;
    QDoubleSpinBox *m_radius;
    QDoubleSpinBox *m_startAngle;
    QDoubleSpinBox *m_sweepAngle;
};