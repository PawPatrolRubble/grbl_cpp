#include "ArcPage.h"
#include "../../../grbl/GraphicsDesign/ArcEntity.h"
#include "../../../grbl/GraphicsDesign/Entity.h"

#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QSignalBlocker>

ArcPage::ArcPage(QWidget *parent)
    : PropertyPage(parent)
{
    auto *layout = new QFormLayout(this);

    m_centerX = createCoordSpinBox(-100000, 100000);
    layout->addRow("Center X:", m_centerX);

    m_centerY = createCoordSpinBox(-100000, 100000);
    layout->addRow("Center Y:", m_centerY);

    m_radius = createCoordSpinBox(0, 100000);
    layout->addRow("Radius:", m_radius);

    m_startAngle = createAngleSpinBox();
    layout->addRow("Start Angle:", m_startAngle);

    m_sweepAngle = createAngleSpinBox();
    layout->addRow("Sweep Angle:", m_sweepAngle);

    // Emit propertyChanged when any widget changes
    connect(m_centerX, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ArcPage::propertyChanged);
    connect(m_centerY, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ArcPage::propertyChanged);
    connect(m_radius, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ArcPage::propertyChanged);
    connect(m_startAngle, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ArcPage::propertyChanged);
    connect(m_sweepAngle, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ArcPage::propertyChanged);
}

void ArcPage::bind(const Entity *entity)
{
    if (!entity || entity->kind() != Entity::Kind::Arc) return;

    const auto *arc = static_cast<const ArcEntity*>(entity);

    m_updating = true;
    QSignalBlocker b1(m_centerX);
    QSignalBlocker b2(m_centerY);
    QSignalBlocker b3(m_radius);
    QSignalBlocker b4(m_startAngle);
    QSignalBlocker b5(m_sweepAngle);

    m_centerX->setValue(arc->center().x());
    m_centerY->setValue(arc->center().y());
    m_radius->setValue(arc->radius());
    m_startAngle->setValue(arc->startAngleDegrees());
    m_sweepAngle->setValue(arc->sweepAngleDegrees());

    m_updating = false;
}

std::unique_ptr<Entity> ArcPage::apply() const
{
    return std::make_unique<ArcEntity>(
        QPointF(m_centerX->value(), m_centerY->value()),
        m_radius->value(),
        m_startAngle->value(),
        m_sweepAngle->value());
}

int ArcPage::entityType() const
{
    return static_cast<int>(Entity::Kind::Arc);
}