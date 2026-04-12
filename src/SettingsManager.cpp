#include "SettingsManager.h"

#include <QSettings>
#include <QStandardPaths>

static constexpr double kDefaultJogStepSize = 1.0;
static constexpr double kDefaultJogFeedRate = 500.0;

SettingsManager::SettingsManager(QObject *parent)
    : QObject(parent)
{
    m_settings = new QSettings("GrblSender", "GrblSender", this);
}

void SettingsManager::saveLayout(const QByteArray &dockState, const QByteArray &geometry)
{
    m_settings->setValue("MainWindow/dockState", dockState);
    m_settings->setValue("MainWindow/geometry", geometry);
    m_settings->sync();
}

QByteArray SettingsManager::restoreLayout(QByteArray &geometry) const
{
    geometry = m_settings->value("MainWindow/geometry").toByteArray();
    return m_settings->value("MainWindow/dockState").toByteArray();
}

void SettingsManager::setJogStepSize(double mm)
{
    m_settings->setValue("Jog/stepSize", mm);
    m_settings->sync();
}

void SettingsManager::setJogFeedRate(double mmPerMin)
{
    m_settings->setValue("Jog/feedRate", mmPerMin);
    m_settings->sync();
}

double SettingsManager::jogStepSize() const
{
    return m_settings->value("Jog/stepSize", kDefaultJogStepSize).toDouble();
}

double SettingsManager::jogFeedRate() const
{
    return m_settings->value("Jog/feedRate", kDefaultJogFeedRate).toDouble();
}

void SettingsManager::resetToDefaults()
{
    m_settings->clear();
    m_settings->sync();
}
