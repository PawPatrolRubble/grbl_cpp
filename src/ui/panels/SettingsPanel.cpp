#include "SettingsPanel.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QLabel>

SettingsPanel::SettingsPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("settingsPanel");
    setAttribute(Qt::WA_StyledBackground, true);
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    // Jog Defaults section
    auto *jogLabel = new QLabel("Jog Defaults");
    jogLabel->setObjectName("sectionTitle");
    mainLayout->addWidget(jogLabel);

    auto *form = new QFormLayout();

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

    mainLayout->addLayout(form);
    mainLayout->addStretch();

    connect(m_jogStepSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this]() {
                emit jogDefaultsChanged(m_jogStepSpin->value(), m_jogFeedSpin->value());
            });
    connect(m_jogFeedSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this]() {
                emit jogDefaultsChanged(m_jogStepSpin->value(), m_jogFeedSpin->value());
            });
}

double SettingsPanel::jogStepSize() const
{
    return m_jogStepSpin->value();
}

double SettingsPanel::jogFeedRate() const
{
    return m_jogFeedSpin->value();
}