#pragma once

#include <QObject>
#include <QByteArray>

class QSettings;

/// Manages application settings and dock layout persistence using QSettings.
class SettingsManager : public QObject
{
    Q_OBJECT

public:
    explicit SettingsManager(QObject *parent = nullptr);
    ~SettingsManager() override;

    /// Save the main window's dock layout and geometry.
    void saveLayout(const QByteArray &dockState, const QByteArray &geometry);

    /// Restore the main window's dock layout. Returns the saved dock state.
    QByteArray restoreLayout(QByteArray &geometry) const;

    // Jog defaults
    void setJogStepSize(double mm);
    void setJogFeedRate(double mmPerMin);
    double jogStepSize() const;
    double jogFeedRate() const;

    /// Reset all settings to defaults.
    void resetToDefaults();

private:
    QSettings *m_settings;
};
