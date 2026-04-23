#pragma once

#include "PropertyPage.h"

class LinearDimensionEntity;

/// Property editor page for LinearDimensionEntity.
/// Uses QGridLayout with Apply button (per original GDP pattern).
/// Displays: P1 X/Y, P2 X/Y, Offset distance
class DimensionPage : public PropertyPage
{
    Q_OBJECT

public:
    explicit DimensionPage(QWidget *parent = nullptr);

    void bind(const Entity *entity) override;
    std::unique_ptr<Entity> apply() const override;
    int entityType() const override;

private:
    QDoubleSpinBox *m_p1X;
    QDoubleSpinBox *m_p1Y;
    QDoubleSpinBox *m_p2X;
    QDoubleSpinBox *m_p2Y;
    QDoubleSpinBox *m_offset;
};