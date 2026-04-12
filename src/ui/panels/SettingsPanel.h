#pragma once

#include <QWidget>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QCheckBox>

class QGroupBox;

class SettingsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPanel(QWidget *parent = nullptr);

    // Jog defaults
    double jogStepSize() const;
    double jogFeedRate() const;

signals:
    void jogDefaultsChanged(double stepSize, double feedRate);

private:
    QGroupBox *createJogDefaultsGroup();
    QGroupBox *createAppearanceGroup();

    QDoubleSpinBox *m_jogStepSpin;
    QDoubleSpinBox *m_jogFeedSpin;
};
