#include "TransformPanel.h"

#include <QGridLayout>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QPushButton>

TransformPanel::TransformPanel(QWidget *parent)
    : QWidget(parent)
{
    m_group = new QGroupBox("Transform");
    auto *layout = new QGridLayout(m_group);
    layout->setSpacing(4);
    layout->setContentsMargins(6, 6, 6, 6);

    // Row 0: Move
    layout->addWidget(new QLabel("Move By:"), 0, 0);
    m_moveDx = new QDoubleSpinBox();
    m_moveDx->setRange(-100000, 100000);
    m_moveDx->setDecimals(3);
    m_moveDx->setPrefix("dx ");
    layout->addWidget(m_moveDx, 0, 1);

    m_moveDy = new QDoubleSpinBox();
    m_moveDy->setRange(-100000, 100000);
    m_moveDy->setDecimals(3);
    m_moveDy->setPrefix("dy ");
    layout->addWidget(m_moveDy, 0, 2);

    auto *moveBtn = new QPushButton("Apply");
    connect(moveBtn, &QPushButton::clicked, this, [this]() {
        emit moveRequested(m_moveDx->value(), m_moveDy->value());
    });
    layout->addWidget(moveBtn, 0, 3);

    // Row 1: Rotate
    layout->addWidget(new QLabel("Rotate:"), 1, 0);
    m_rotateAngle = new QDoubleSpinBox();
    m_rotateAngle->setRange(-36000, 36000);
    m_rotateAngle->setDecimals(2);
    m_rotateAngle->setSuffix(" °");
    layout->addWidget(m_rotateAngle, 1, 1, 1, 2);

    auto *rotateBtn = new QPushButton("Apply");
    connect(rotateBtn, &QPushButton::clicked, this, [this]() {
        emit rotateRequested(m_rotateAngle->value());
    });
    layout->addWidget(rotateBtn, 1, 3);

    // Row 2: Mirror H/V
    layout->addWidget(new QLabel("Mirror:"), 2, 0);
    auto *flipHBtn = new QPushButton("Flip H");
    flipHBtn->setToolTip("Mirror horizontally (about vertical axis)");
    connect(flipHBtn, &QPushButton::clicked, this, &TransformPanel::flipHorizontalRequested);
    layout->addWidget(flipHBtn, 2, 1);

    auto *flipVBtn = new QPushButton("Flip V");
    flipVBtn->setToolTip("Mirror vertically (about horizontal axis)");
    connect(flipVBtn, &QPushButton::clicked, this, &TransformPanel::flipVerticalRequested);
    layout->addWidget(flipVBtn, 2, 2);

    // Row 3: Rectangular array
    layout->addWidget(new QLabel("Array:"), 3, 0);
    m_arrayRows = new QSpinBox();
    m_arrayRows->setRange(1, 100);
    m_arrayRows->setValue(2);
    m_arrayRows->setToolTip("Number of rows");
    m_arrayRows->setPrefix("R:");
    layout->addWidget(m_arrayRows, 3, 1);

    m_arrayCols = new QSpinBox();
    m_arrayCols->setRange(1, 100);
    m_arrayCols->setValue(2);
    m_arrayCols->setToolTip("Number of columns");
    m_arrayCols->setPrefix("C:");
    layout->addWidget(m_arrayCols, 3, 2);

    auto *arrayBtn = new QPushButton("Apply");
    connect(arrayBtn, &QPushButton::clicked, this, [this]() {
        emit rectArrayRequested(m_arrayRows->value(), m_arrayCols->value(),
                                 m_arraySpacingX->value(), m_arraySpacingY->value());
    });
    layout->addWidget(arrayBtn, 3, 3);

    // Row 4: Array spacing
    layout->addWidget(new QLabel("Spacing:"), 4, 0);
    m_arraySpacingX = new QDoubleSpinBox();
    m_arraySpacingX->setRange(-10000, 10000);
    m_arraySpacingX->setValue(20.0);
    m_arraySpacingX->setSuffix(" X");
    m_arraySpacingX->setDecimals(2);
    m_arraySpacingX->setToolTip("Column spacing (X) in mm");
    layout->addWidget(m_arraySpacingX, 4, 1);

    m_arraySpacingY = new QDoubleSpinBox();
    m_arraySpacingY->setRange(-10000, 10000);
    m_arraySpacingY->setValue(20.0);
    m_arraySpacingY->setSuffix(" Y");
    m_arraySpacingY->setDecimals(2);
    m_arraySpacingY->setToolTip("Row spacing (Y) in mm");
    layout->addWidget(m_arraySpacingY, 4, 2);

    // Row 5: Scale
    layout->addWidget(new QLabel("Scale:"), 5, 0);
    m_scaleFactor = new QDoubleSpinBox();
    m_scaleFactor->setRange(0.001, 1000.0);
    m_scaleFactor->setValue(1.0);
    m_scaleFactor->setDecimals(4);
    m_scaleFactor->setSingleStep(0.1);
    m_scaleFactor->setToolTip("Scale factor (1.0 = no change)");
    layout->addWidget(m_scaleFactor, 5, 1, 1, 2);

    auto *scaleBtn = new QPushButton("Apply");
    connect(scaleBtn, &QPushButton::clicked, this, [this]() {
        emit scaleRequested(m_scaleFactor->value());
    });
    layout->addWidget(scaleBtn, 5, 3);

    // Row 6: Polar array
    layout->addWidget(new QLabel("Polar:"), 6, 0);
    m_polarCount = new QSpinBox();
    m_polarCount->setRange(2, 360);
    m_polarCount->setValue(6);
    m_polarCount->setToolTip("Number of copies (including original)");
    m_polarCount->setPrefix("N:");
    layout->addWidget(m_polarCount, 6, 1);

    m_polarSpan = new QDoubleSpinBox();
    m_polarSpan->setRange(0.1, 360.0);
    m_polarSpan->setValue(360.0);
    m_polarSpan->setSuffix("°");
    m_polarSpan->setDecimals(1);
    m_polarSpan->setToolTip("Total arc span for the array (360 = full circle)");
    layout->addWidget(m_polarSpan, 6, 2);

    auto *polarBtn = new QPushButton("Apply");
    connect(polarBtn, &QPushButton::clicked, this, [this]() {
        emit polarArrayRequested(m_polarCount->value(), m_polarSpan->value(),
                                  m_polarCenterX->value(), m_polarCenterY->value());
    });
    layout->addWidget(polarBtn, 6, 3);

    // Row 7: Polar center
    layout->addWidget(new QLabel("Center:"), 7, 0);
    m_polarCenterX = new QDoubleSpinBox();
    m_polarCenterX->setRange(-100000, 100000);
    m_polarCenterX->setValue(0.0);
    m_polarCenterX->setDecimals(2);
    m_polarCenterX->setPrefix("X:");
    m_polarCenterX->setToolTip("Polar array center X (0 = selection center)");
    layout->addWidget(m_polarCenterX, 7, 1);

    m_polarCenterY = new QDoubleSpinBox();
    m_polarCenterY->setRange(-100000, 100000);
    m_polarCenterY->setValue(0.0);
    m_polarCenterY->setDecimals(2);
    m_polarCenterY->setPrefix("Y:");
    m_polarCenterY->setToolTip("Polar array center Y (0 = selection center)");
    layout->addWidget(m_polarCenterY, 7, 2);

    // Row 8: Mirror by axis angle
    layout->addWidget(new QLabel("Mirror°:"), 8, 0);
    m_mirrorAxisAngle = new QDoubleSpinBox();
    m_mirrorAxisAngle->setRange(0.0, 179.9);
    m_mirrorAxisAngle->setValue(0.0);
    m_mirrorAxisAngle->setDecimals(1);
    m_mirrorAxisAngle->setSuffix("°");
    m_mirrorAxisAngle->setToolTip("Mirror axis angle (0°=horizontal, 90°=vertical)");
    layout->addWidget(m_mirrorAxisAngle, 8, 1, 1, 2);

    auto *mirrorAxisBtn = new QPushButton("Apply");
    connect(mirrorAxisBtn, &QPushButton::clicked, this, [this]() {
        emit mirrorByAngleRequested(m_mirrorAxisAngle->value());
    });
    layout->addWidget(mirrorAxisBtn, 8, 3);

    // Row 9: Align H
    layout->addWidget(new QLabel("Align H:"), 9, 0);
    auto *alignLBtn = new QPushButton("⊣ Left");
    alignLBtn->setToolTip("Align left edges");
    connect(alignLBtn, &QPushButton::clicked, this, &TransformPanel::alignLeftRequested);
    layout->addWidget(alignLBtn, 9, 1);

    auto *alignCHBtn = new QPushButton("⊥ Ctr");
    alignCHBtn->setToolTip("Center horizontally");
    connect(alignCHBtn, &QPushButton::clicked, this, &TransformPanel::alignCenterHRequested);
    layout->addWidget(alignCHBtn, 9, 2);

    auto *alignRBtn = new QPushButton("⊢ Right");
    alignRBtn->setToolTip("Align right edges");
    connect(alignRBtn, &QPushButton::clicked, this, &TransformPanel::alignRightRequested);
    layout->addWidget(alignRBtn, 9, 3);

    // Row 10: Align V
    layout->addWidget(new QLabel("Align V:"), 10, 0);
    auto *alignTBtn = new QPushButton("⊤ Top");
    alignTBtn->setToolTip("Align top edges");
    connect(alignTBtn, &QPushButton::clicked, this, &TransformPanel::alignTopRequested);
    layout->addWidget(alignTBtn, 10, 1);

    auto *alignCVBtn = new QPushButton("⊠ Mid");
    alignCVBtn->setToolTip("Center vertically");
    connect(alignCVBtn, &QPushButton::clicked, this, &TransformPanel::alignCenterVRequested);
    layout->addWidget(alignCVBtn, 10, 2);

    auto *alignBBtn = new QPushButton("⊥ Bot");
    alignBBtn->setToolTip("Align bottom edges");
    connect(alignBBtn, &QPushButton::clicked, this, &TransformPanel::alignBottomRequested);
    layout->addWidget(alignBBtn, 10, 3);

    // Add group to widget
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(m_group);

    m_group->setVisible(false);
}

void TransformPanel::resetInputs()
{
    m_moveDx->setValue(0.0);
    m_moveDy->setValue(0.0);
    m_rotateAngle->setValue(0.0);
}

void TransformPanel::setVisible(bool visible)
{
    m_group->setVisible(visible);
}