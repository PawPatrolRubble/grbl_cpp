#include "CirclePage.h"
#include "../../../grbl/GraphicsDesign/CircleEntity.h"
#include "../../../grbl/GraphicsDesign/Entity.h"

#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QSignalBlocker>

CirclePage::CirclePage(QWidget *parent)
    : PropertyPage(parent)
{
    auto *layout = new QFormLayout(this);

    m_centerX = createCoordSpinBox(-100000, 100000);
    layout->addRow("Center X:", m_centerX);

    m_centerY = createCoordSpinBox(-100000, 100000);
    layout->addRow("Center Y:", m_centerY);

    m_radius = createCoordSpinBox(0, 100000);
    layout->addRow("Radius:", m_radius);

    // Emit propertyChanged when any widget changes
    connect(m_centerX, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &CirclePage::propertyChanged);
    connect(m_centerY, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &CirclePage::propertyChanged);
    connect(m_radius, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &CirclePage::propertyChanged);
}

void CirclePage::bind(const Entity *entity)
{
    if (!entity || entity->kind() != Entity::Kind::Circle) return;

    const auto *circle = static_cast<const CircleEntity*>(entity);

    m_updating = true;
    QSignalBlocker b1(m_centerX);
    QSignalBlocker b2(m_centerY);
    QSignalBlocker b3(m_radius);

    m_centerX->setValue(circle->center().x());
    m_centerY->setValue(circle->center().y());
    m_radius->setValue(circle->radius());

    m_updating = false;
}

std::unique_ptr<Entity> CirclePage::apply() const
{
    return std::make_unique<CircleEntity>(
        QPointF(m_centerX->value(), m_centerY->value()),
        m_radius->value());
}

int CirclePage::entityType() const
{
    return static_cast<int>(Entity::Kind::Circle);
}