#include "AngularDimensionPage.h"
#include "../../../grbl/GraphicsDesign/AngularDimensionEntity.h"
#include "../../../grbl/GraphicsDesign/Entity.h"

#include <QGridLayout>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>

AngularDimensionPage::AngularDimensionPage(QWidget *parent)
    : PropertyPage(parent)
{
    auto *layout = new QGridLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto makeSpinBox = [this](double lo, double hi) {
        return createCoordSpinBox(lo, hi, 3);
    };

    layout->addWidget(new QLabel("Vertex X:"), 0, 0);
    m_vertexX = makeSpinBox(-100000, 100000);
    layout->addWidget(m_vertexX, 0, 1);

    layout->addWidget(new QLabel("Vertex Y:"), 1, 0);
    m_vertexY = makeSpinBox(-100000, 100000);
    layout->addWidget(m_vertexY, 1, 1);

    layout->addWidget(new QLabel("Ray 1 X:"), 2, 0);
    m_p1X = makeSpinBox(-100000, 100000);
    layout->addWidget(m_p1X, 2, 1);

    layout->addWidget(new QLabel("Ray 1 Y:"), 3, 0);
    m_p1Y = makeSpinBox(-100000, 100000);
    layout->addWidget(m_p1Y, 3, 1);

    layout->addWidget(new QLabel("Ray 2 X:"), 4, 0);
    m_p2X = makeSpinBox(-100000, 100000);
    layout->addWidget(m_p2X, 4, 1);

    layout->addWidget(new QLabel("Ray 2 Y:"), 5, 0);
    m_p2Y = makeSpinBox(-100000, 100000);
    layout->addWidget(m_p2Y, 5, 1);

    layout->addWidget(new QLabel("Arc Radius:"), 6, 0);
    m_radius = createCoordSpinBox(1.0, 100000, 3);
    layout->addWidget(m_radius, 6, 1);

    layout->setRowStretch(7, 1);

    // Emit propertyChanged when any widget changes
    connect(m_vertexX, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AngularDimensionPage::propertyChanged);
    connect(m_vertexY, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AngularDimensionPage::propertyChanged);
    connect(m_p1X, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AngularDimensionPage::propertyChanged);
    connect(m_p1Y, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AngularDimensionPage::propertyChanged);
    connect(m_p2X, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AngularDimensionPage::propertyChanged);
    connect(m_p2Y, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AngularDimensionPage::propertyChanged);
    connect(m_radius, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AngularDimensionPage::propertyChanged);
}

void AngularDimensionPage::bind(const Entity *entity)
{
    if (!entity || entity->kind() != Entity::Kind::AngularDimension) return;

    const auto *adim = static_cast<const AngularDimensionEntity*>(entity);

    m_updating = true;
    QSignalBlocker b1(m_vertexX);
    QSignalBlocker b2(m_vertexY);
    QSignalBlocker b3(m_p1X);
    QSignalBlocker b4(m_p1Y);
    QSignalBlocker b5(m_p2X);
    QSignalBlocker b6(m_p2Y);
    QSignalBlocker b7(m_radius);

    m_vertexX->setValue(adim->vertex().x());
    m_vertexY->setValue(adim->vertex().y());
    m_p1X->setValue(adim->p1().x());
    m_p1Y->setValue(adim->p1().y());
    m_p2X->setValue(adim->p2().x());
    m_p2Y->setValue(adim->p2().y());
    m_radius->setValue(adim->radius());

    m_updating = false;
}

std::unique_ptr<Entity> AngularDimensionPage::apply() const
{
    return std::make_unique<AngularDimensionEntity>(
        QPointF(m_vertexX->value(), m_vertexY->value()),
        QPointF(m_p1X->value(), m_p1Y->value()),
        QPointF(m_p2X->value(), m_p2Y->value()),
        m_radius->value());
}

int AngularDimensionPage::entityType() const
{
    return static_cast<int>(Entity::Kind::AngularDimension);
}