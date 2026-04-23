#include "DimensionPage.h"
#include "../../../grbl/GraphicsDesign/LinearDimensionEntity.h"
#include "../../../grbl/GraphicsDesign/Entity.h"

#include <QGridLayout>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>

DimensionPage::DimensionPage(QWidget *parent)
    : PropertyPage(parent)
{
    auto *layout = new QGridLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto makeSpinBox = [this](double lo, double hi) {
        auto *sb = createCoordSpinBox(lo, hi, 3);
        return sb;
    };

    layout->addWidget(new QLabel("P1 X:"), 0, 0);
    m_p1X = makeSpinBox(-100000, 100000);
    layout->addWidget(m_p1X, 0, 1);

    layout->addWidget(new QLabel("P1 Y:"), 1, 0);
    m_p1Y = makeSpinBox(-100000, 100000);
    layout->addWidget(m_p1Y, 1, 1);

    layout->addWidget(new QLabel("P2 X:"), 2, 0);
    m_p2X = makeSpinBox(-100000, 100000);
    layout->addWidget(m_p2X, 2, 1);

    layout->addWidget(new QLabel("P2 Y:"), 3, 0);
    m_p2Y = makeSpinBox(-100000, 100000);
    layout->addWidget(m_p2Y, 3, 1);

    layout->addWidget(new QLabel("Offset:"), 4, 0);
    m_offset = makeSpinBox(-100000, 100000);
    layout->addWidget(m_offset, 4, 1);

    layout->setRowStretch(5, 1);

    // Emit propertyChanged when any widget changes
    connect(m_p1X, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &DimensionPage::propertyChanged);
    connect(m_p1Y, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &DimensionPage::propertyChanged);
    connect(m_p2X, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &DimensionPage::propertyChanged);
    connect(m_p2Y, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &DimensionPage::propertyChanged);
    connect(m_offset, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &DimensionPage::propertyChanged);
}

void DimensionPage::bind(const Entity *entity)
{
    if (!entity || entity->kind() != Entity::Kind::LinearDimension) return;

    const auto *dim = static_cast<const LinearDimensionEntity*>(entity);

    m_updating = true;
    QSignalBlocker b1(m_p1X);
    QSignalBlocker b2(m_p1Y);
    QSignalBlocker b3(m_p2X);
    QSignalBlocker b4(m_p2Y);
    QSignalBlocker b5(m_offset);

    m_p1X->setValue(dim->p1().x());
    m_p1Y->setValue(dim->p1().y());
    m_p2X->setValue(dim->p2().x());
    m_p2Y->setValue(dim->p2().y());
    m_offset->setValue(dim->offsetDistance());

    m_updating = false;
}

std::unique_ptr<Entity> DimensionPage::apply() const
{
    return std::make_unique<LinearDimensionEntity>(
        QPointF(m_p1X->value(), m_p1Y->value()),
        QPointF(m_p2X->value(), m_p2Y->value()),
        m_offset->value());
}

int DimensionPage::entityType() const
{
    return static_cast<int>(Entity::Kind::LinearDimension);
}