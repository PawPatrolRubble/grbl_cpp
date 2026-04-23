#include "LinePage.h"
#include "../../../grbl/GraphicsDesign/LineEntity.h"
#include "../../../grbl/GraphicsDesign/Entity.h"

#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QSignalBlocker>

LinePage::LinePage(QWidget *parent)
    : PropertyPage(parent)
{
    auto *layout = new QFormLayout(this);

    m_centerX = createCoordSpinBox(-100000, 100000);
    layout->addRow("Center X:", m_centerX);

    m_centerY = createCoordSpinBox(-100000, 100000);
    layout->addRow("Center Y:", m_centerY);

    m_length = createCoordSpinBox(0, 100000);
    layout->addRow("Length:", m_length);

    m_angle = createAngleSpinBox();
    layout->addRow("Angle:", m_angle);

    // Emit propertyChanged when any widget changes
    connect(m_centerX, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LinePage::propertyChanged);
    connect(m_centerY, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LinePage::propertyChanged);
    connect(m_length, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LinePage::propertyChanged);
    connect(m_angle, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LinePage::propertyChanged);
}

void LinePage::bind(const Entity *entity)
{
    if (!entity || entity->kind() != Entity::Kind::Line) return;

    const auto *line = static_cast<const LineEntity*>(entity);

    m_updating = true;
    QSignalBlocker b1(m_centerX);
    QSignalBlocker b2(m_centerY);
    QSignalBlocker b3(m_length);
    QSignalBlocker b4(m_angle);

    m_centerX->setValue(line->center().x());
    m_centerY->setValue(line->center().y());
    m_length->setValue(line->length());
    m_angle->setValue(line->angleDegrees());

    m_updating = false;
}

std::unique_ptr<Entity> LinePage::apply() const
{
    auto line = std::make_unique<LineEntity>(QPointF(0, 0), QPointF(1, 0));
    line->setCenterLengthAngle(
        QPointF(m_centerX->value(), m_centerY->value()),
        m_length->value(),
        m_angle->value());
    return line;
}

int LinePage::entityType() const
{
    return static_cast<int>(Entity::Kind::Line);
}