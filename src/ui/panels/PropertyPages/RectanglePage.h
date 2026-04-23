#pragma once

#include "PropertyPage.h"

class RectangleEntity;

/// Property editor page for RectangleEntity.
/// Displays: Center X/Y, Width, Height, Angle
class RectanglePage : public PropertyPage
{
    Q_OBJECT

public:
    explicit RectanglePage(QWidget *parent = nullptr);

    void bind(const Entity *entity) override;
    std::unique_ptr<Entity> apply() const override;
    int entityType() const override;

private:
    QDoubleSpinBox *m_centerX;
    QDoubleSpinBox *m_centerY;
    QDoubleSpinBox *m_width;
    QDoubleSpinBox *m_height;
    QDoubleSpinBox *m_angle;
};