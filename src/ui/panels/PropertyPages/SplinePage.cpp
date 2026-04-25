#include "SplinePage.h"
#include "../../../grbl/GraphicsDesign/SplineEntity.h"
#include "../../../grbl/GraphicsDesign/Entity.h"

#include <QGridLayout>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>

SplinePage::SplinePage(QWidget *parent)
    : PropertyPage(parent)
{
    auto *layout = new QGridLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_pointCountLabel = new QLabel("Points: 0");
    layout->addWidget(m_pointCountLabel, 0, 0, 1, 2);

    layout->addWidget(new QLabel("Steps/segment:"), 1, 0);
    m_stepsSpin = new QSpinBox();
    m_stepsSpin->setRange(4, 100);
    m_stepsSpin->setValue(20);
    layout->addWidget(m_stepsSpin, 1, 1);

    layout->setRowStretch(2, 1);

    // Emit propertyChanged when steps changes
    connect(m_stepsSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SplinePage::propertyChanged);
}

void SplinePage::bind(const Entity *entity)
{
    if (!entity || entity->kind() != Entity::Kind::Spline) return;

    const auto *spline = static_cast<const SplineEntity*>(entity);

    m_original = spline;

    m_updating = true;
    QSignalBlocker b(m_stepsSpin);

    m_pointCountLabel->setText(QString("Points: %1").arg(spline->controlPoints().size()));
    m_stepsSpin->setValue(spline->stepsPerSegment());

    m_updating = false;
}

std::unique_ptr<Entity> SplinePage::apply() const
{
    if (!m_original) return nullptr;
    auto clone = m_original->clone();
    static_cast<SplineEntity*>(clone.get())->setStepsPerSegment(m_stepsSpin->value());
    return clone;
}

int SplinePage::entityType() const
{
    return static_cast<int>(Entity::Kind::Spline);
}