#pragma once

#include "PropertyPage.h"

class CircleEntity;

/// Property editor page for CircleEntity.
/// Displays: Center X/Y, Radius
class CirclePage : public PropertyPage
{
    Q_OBJECT

public:
    explicit CirclePage(QWidget *parent = nullptr);

    void bind(const Entity *entity) override;
    std::unique_ptr<Entity> apply() const override;
    int entityType() const override;

private:
    QDoubleSpinBox *m_centerX;
    QDoubleSpinBox *m_centerY;
    QDoubleSpinBox *m_radius;
};