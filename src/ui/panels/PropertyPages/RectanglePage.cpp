#include "RectanglePage.h"
#include "../../../grbl/GraphicsDesign/RectangleEntity.h"
#include "../../../grbl/GraphicsDesign/Entity.h"

#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QSignalBlocker>

RectanglePage::RectanglePage(QWidget *parent)
    : PropertyPage(parent)
{
    auto *layout = new QFormLayout(this);

    m_centerX = createCoordSpinBox(-100000, 100000);
    layout->addRow("Center X:", m_centerX);

    m_centerY = createCoordSpinBox(-100000, 100000);
    layout->addRow("Center Y:", m_centerY);

    m_width = createCoordSpinBox(0, 100000);
    layout->addRow("Width:", m_width);

    m_height = createCoordSpinBox(0, 100000);
    layout->addRow("Height:", m_height);

    m_angle = createAngleSpinBox();
    layout->addRow("Angle:", m_angle);

    // Emit propertyChanged when any widget changes
    connect(m_centerX, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &RectanglePage::propertyChanged);
    connect(m_centerY, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &RectanglePage::propertyChanged);
    connect(m_width, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &RectanglePage::propertyChanged);
    connect(m_height, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &RectanglePage::propertyChanged);
    connect(m_angle, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &RectanglePage::propertyChanged);
}

void RectanglePage::bind(const Entity *entity)
{
    if (!entity || entity->kind() != Entity::Kind::Rectangle) return;

    const auto *rect = static_cast<const RectangleEntity*>(entity);

    m_updating = true;
    QSignalBlocker b1(m_centerX);
    QSignalBlocker b2(m_centerY);
    QSignalBlocker b3(m_width);
    QSignalBlocker b4(m_height);
    QSignalBlocker b5(m_angle);

    m_centerX->setValue(rect->center().x());
    m_centerY->setValue(rect->center().y());
    m_width->setValue(rect->width());
    m_height->setValue(rect->height());
    m_angle->setValue(rect->angleDegrees());

    m_updating = false;
}

std::unique_ptr<Entity> RectanglePage::apply() const
{
    return std::make_unique<RectangleEntity>(
        QPointF(m_centerX->value(), m_centerY->value()),
        m_width->value(),
        m_height->value(),
        m_angle->value());
}

int RectanglePage::entityType() const
{
    return static_cast<int>(Entity::Kind::Rectangle);
}