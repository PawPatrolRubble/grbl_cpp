#pragma once

#include <QWidget>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QLabel>

class TransformToolbar : public QWidget
{
    Q_OBJECT

public:
    explicit TransformToolbar(QWidget *parent = nullptr);

    double scaleX() const;
    double scaleY() const;
    double offsetX() const;
    double offsetY() const;
    double rotation() const;

signals:
    void applyRequested();
    void resetRequested();

private:
    QDoubleSpinBox *m_scaleX;
    QDoubleSpinBox *m_scaleY;
    QDoubleSpinBox *m_offsetX;
    QDoubleSpinBox *m_offsetY;
    QDoubleSpinBox *m_rotation;
    QPushButton *m_resetBtn;
};
