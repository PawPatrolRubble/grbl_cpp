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
    ~SettingsManager() override = default;

    void saveLayout(const QByteArray &dockState, const QByteArray &geometry);
    [[nodiscard]] QByteArray restoreLayout(QByteArray &geometry) const;

    void setJogStepSize(double mm);
    void setJogFeedRate(double mmPerMin);
    [[nodiscard]] double jogStepSize() const;
    [[nodiscard]] double jogFeedRate() const;

    void resetToDefaults();

private:
    QSettings *m_settings;
};
