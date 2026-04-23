#pragma once

#include <QWidget>
#include <QGroupBox>

class QDoubleSpinBox;
class QSpinBox;
class QPushButton;

/// Transform panel for entity manipulation.
/// Provides controls for: Move, Rotate, Mirror (H/V), Rectangular Array,
/// Scale, Polar Array, Mirror by axis angle, and Align (H/V).
///
/// Emits signals for each operation. The parent panel handles the actual
/// transformation logic by applying to selected entities.
class TransformPanel : public QWidget
{
    Q_OBJECT

public:
    explicit TransformPanel(QWidget *parent = nullptr);

    /// Reset input values to defaults (called when selection changes)
    void resetInputs();

    /// Show/hide the panel (controlled by selection state)
    void setVisible(bool visible);

signals:
    // Move
    void moveRequested(double dx, double dy);

    // Rotate
    void rotateRequested(double angleDegrees);

    // Mirror
    void flipHorizontalRequested();
    void flipVerticalRequested();
    void mirrorByAngleRequested(double axisAngle);

    // Rectangular array
    void rectArrayRequested(int rows, int cols, double spacingX, double spacingY);

    // Scale
    void scaleRequested(double factor);

    // Polar array
    void polarArrayRequested(int count, double spanDegrees, double centerX, double centerY);

    // Align
    void alignLeftRequested();
    void alignCenterHRequested();
    void alignRightRequested();
    void alignTopRequested();
    void alignCenterVRequested();
    void alignBottomRequested();

private:
    QGroupBox *m_group;

    // Move
    QDoubleSpinBox *m_moveDx;
    QDoubleSpinBox *m_moveDy;

    // Rotate
    QDoubleSpinBox *m_rotateAngle;

    // Rectangular array
    QSpinBox *m_arrayRows;
    QSpinBox *m_arrayCols;
    QDoubleSpinBox *m_arraySpacingX;
    QDoubleSpinBox *m_arraySpacingY;

    // Scale
    QDoubleSpinBox *m_scaleFactor;

    // Polar array
    QSpinBox *m_polarCount;
    QDoubleSpinBox *m_polarSpan;
    QDoubleSpinBox *m_polarCenterX;
    QDoubleSpinBox *m_polarCenterY;

    // Mirror by axis
    QDoubleSpinBox *m_mirrorAxisAngle;
};