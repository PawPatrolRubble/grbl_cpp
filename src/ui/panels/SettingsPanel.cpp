#include "SettingsPanel.h"

#include <QGroupBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QLabel>

SettingsPanel::SettingsPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    mainLayout->addWidget(createJogDefaultsGroup());
    mainLayout->addStretch();
}

QGroupBox *SettingsPanel::createJogDefaultsGroup()
{
    auto *group = new QGroupBox("Jog Defaults");
    auto *form = new QFormLayout(group);

    m_jogStepSpin = new QDoubleSpinBox();
    m_jogStepSpin->setRange(0.001, 999.0);
    m_jogStepSpin->setDecimals(3);
    m_jogStepSpin->setSuffix(" mm");
    m_jogStepSpin->setValue(1.0);
    form->addRow("Step Size:", m_jogStepSpin);

    m_jogFeedSpin = new QDoubleSpinBox();
    m_jogFeedSpin->setRange(1, 10000);
    m_jogFeedSpin->setDecimals(0);
    m_jogFeedSpin->setSuffix(" mm/min");
    m_jogFeedSpin->setValue(500);
    form->addRow("Feed Rate:", m_jogFeedSpin);

    connect(m_jogStepSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this]() {
                emit jogDefaultsChanged(m_jogStepSpin->value(), m_jogFeedSpin->value());
            });
    connect(m_jogFeedSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this]() {
                emit jogDefaultsChanged(m_jogStepSpin->value(), m_jogFeedSpin->value());
            });

    return group;
}

QGroupBox *SettingsPanel::createAppearanceGroup()
{
    auto *group = new QGroupBox("Appearance");
    auto *layout = new QVBoxLayout(group);

    auto *label = new QLabel("Theme settings coming soon.");
    label->setWordWrap(true);
    layout->addWidget(label);

    return group;
}

double SettingsPanel::jogStepSize() const
{
    return m_jogStepSpin->value();
}

double SettingsPanel::jogFeedRate() const
{
    return m_jogFeedSpin->value();
}
