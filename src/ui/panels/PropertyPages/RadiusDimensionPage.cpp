#include "RadiusDimensionPage.h"
#include "../../../grbl/GraphicsDesign/RadiusDimensionEntity.h"
#include "../../../grbl/GraphicsDesign/Entity.h"

#include <QGridLayout>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>

RadiusDimensionPage::RadiusDimensionPage(QWidget *parent)
    : PropertyPage(parent)
{
    auto *layout = new QGridLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto makeSpinBox = [this](double lo, double hi) {
        return createCoordSpinBox(lo, hi, 3);
    };

    layout->addWidget(new QLabel("Center X:"), 0, 0);
    m_centerX = makeSpinBox(-100000, 100000);
    layout->addWidget(m_centerX, 0, 1);

    layout->addWidget(new QLabel("Center Y:"), 1, 0);
    m_centerY = makeSpinBox(-100000, 100000);
    layout->addWidget(m_centerY, 1, 1);

    layout->addWidget(new QLabel("Radius:"), 2, 0);
    m_radius = createCoordSpinBox(0.001, 100000, 3);
    layout->addWidget(m_radius, 2, 1);

    layout->addWidget(new QLabel("Angle (°):"), 3, 0);
    m_angle = createAngleSpinBox();
    layout->addWidget(m_angle, 3, 1);

    m_diameter = new QCheckBox("Diameter mode");
    layout->addWidget(m_diameter, 4, 0, 1, 2);

    layout->setRowStretch(5, 1);

    // Emit propertyChanged when any widget changes
    connect(m_centerX, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &RadiusDimensionPage::propertyChanged);
    connect(m_centerY, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &RadiusDimensionPage::propertyChanged);
    connect(m_radius, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &RadiusDimensionPage::propertyChanged);
    connect(m_angle, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &RadiusDimensionPage::propertyChanged);
    connect(m_diameter, &QCheckBox::toggled,
            this, &RadiusDimensionPage::propertyChanged);
}

void RadiusDimensionPage::bind(const Entity *entity)
{
    if (!entity || entity->kind() != Entity::Kind::RadiusDimension) return;

    const auto *rdim = static_cast<const RadiusDimensionEntity*>(entity);

    m_updating = true;
    QSignalBlocker b1(m_centerX);
    QSignalBlocker b2(m_centerY);
    QSignalBlocker b3(m_radius);
    QSignalBlocker b4(m_angle);
    QSignalBlocker b5(m_diameter);

    m_centerX->setValue(rdim->circleCenter().x());
    m_centerY->setValue(rdim->circleCenter().y());
    m_radius->setValue(rdim->circleRadius());
    m_angle->setValue(rdim->angleDeg());
    m_diameter->setChecked(rdim->isDiameter());

    m_updating = false;
}

std::unique_ptr<Entity> RadiusDimensionPage::apply() const
{
    return std::make_unique<RadiusDimensionEntity>(
        QPointF(m_centerX->value(), m_centerY->value()),
        m_radius->value(),
        m_angle->value(),
        m_diameter->isChecked());
}

int RadiusDimensionPage::entityType() const
{
    return static_cast<int>(Entity::Kind::RadiusDimension);
}