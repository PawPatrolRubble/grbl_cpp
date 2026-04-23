#pragma once

#include "PropertyPage.h"

class LineEntity;

/// Property editor page for LineEntity.
/// Displays: Center X/Y, Length, Angle
class LinePage : public PropertyPage
{
    Q_OBJECT

public:
    explicit LinePage(QWidget *parent = nullptr);

    void bind(const Entity *entity) override;
    std::unique_ptr<Entity> apply() const override;
    int entityType() const override;

private:
    QDoubleSpinBox *m_centerX;
    QDoubleSpinBox *m_centerY;
    QDoubleSpinBox *m_length;
    QDoubleSpinBox *m_angle;
};