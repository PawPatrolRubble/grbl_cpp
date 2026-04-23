#pragma once

#include "PropertyPage.h"

class SplineEntity;

/// Property editor page for SplineEntity.
/// Displays: Control point count (read-only), Steps per segment (editable)
class SplinePage : public PropertyPage
{
    Q_OBJECT

public:
    explicit SplinePage(QWidget *parent = nullptr);

    void bind(const Entity *entity) override;
    std::unique_ptr<Entity> apply() const override;
    int entityType() const override;

private:
    QLabel *m_pointCountLabel;
    QSpinBox *m_stepsSpin;
};