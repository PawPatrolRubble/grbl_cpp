#include "TransformToolbar.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QFormLayout>

TransformToolbar::TransformToolbar(QWidget *parent)
    : QWidget(parent)
{
    // --- Scale ---
    m_scaleX = new QDoubleSpinBox();
    m_scaleX->setRange(1.0, 10000.0);
    m_scaleX->setValue(100.0);
    m_scaleX->setSuffix(" %");
    m_scaleX->setSingleStep(5.0);

    m_scaleY = new QDoubleSpinBox();
    m_scaleY->setRange(1.0, 10000.0);
    m_scaleY->setValue(100.0);
    m_scaleY->setSuffix(" %");
    m_scaleY->setSingleStep(5.0);

    auto *scaleLayout = new QFormLayout();
    scaleLayout->addRow("X:", m_scaleX);
    scaleLayout->addRow("Y:", m_scaleY);
    auto *scaleGroup = new QGroupBox("Scale");
    scaleGroup->setLayout(scaleLayout);

    // --- Translate ---
    m_offsetX = new QDoubleSpinBox();
    m_offsetX->setRange(-99999.0, 99999.0);
    m_offsetX->setValue(0.0);
    m_offsetX->setSuffix(" mm");
    m_offsetX->setSingleStep(1.0);

    m_offsetY = new QDoubleSpinBox();
    m_offsetY->setRange(-99999.0, 99999.0);
    m_offsetY->setValue(0.0);
    m_offsetY->setSuffix(" mm");
    m_offsetY->setSingleStep(1.0);

    auto *translateLayout = new QFormLayout();
    translateLayout->addRow("X:", m_offsetX);
    translateLayout->addRow("Y:", m_offsetY);
    auto *translateGroup = new QGroupBox("Translate");
    translateGroup->setLayout(translateLayout);

    // --- Rotate ---
    m_rotation = new QDoubleSpinBox();
    m_rotation->setRange(-360.0, 360.0);
    m_rotation->setValue(0.0);
    m_rotation->setSuffix(" °");
    m_rotation->setSingleStep(5.0);

    auto *rotateLayout = new QFormLayout();
    rotateLayout->addRow("Angle:", m_rotation);
    auto *rotateGroup = new QGroupBox("Rotate");
    rotateGroup->setLayout(rotateLayout);

    // --- Buttons ---
    m_resetBtn = new QPushButton("Reset");

    auto *btnLayout = new QVBoxLayout();
    btnLayout->addWidget(m_resetBtn);
    btnLayout->addStretch();

    // --- Assemble ---
    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(4, 2, 4, 2);
    mainLayout->addWidget(scaleGroup);
    mainLayout->addWidget(translateGroup);
    mainLayout->addWidget(rotateGroup);
    mainLayout->addLayout(btnLayout);
    mainLayout->addStretch();

    connect(m_scaleX, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &TransformToolbar::applyRequested);
    connect(m_scaleY, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &TransformToolbar::applyRequested);
    connect(m_offsetX, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &TransformToolbar::applyRequested);
    connect(m_offsetY, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &TransformToolbar::applyRequested);
    connect(m_rotation, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &TransformToolbar::applyRequested);
    connect(m_resetBtn, &QPushButton::clicked, this, [this]() {
        m_scaleX->setValue(100.0);
        m_scaleY->setValue(100.0);
        m_offsetX->setValue(0.0);
        m_offsetY->setValue(0.0);
        m_rotation->setValue(0.0);
        emit resetRequested();
    });
}

double TransformToolbar::scaleX() const { return m_scaleX->value() / 100.0; }
double TransformToolbar::scaleY() const { return m_scaleY->value() / 100.0; }
double TransformToolbar::offsetX() const { return m_offsetX->value(); }
double TransformToolbar::offsetY() const { return m_offsetY->value(); }
double TransformToolbar::rotation() const { return m_rotation->value(); }
