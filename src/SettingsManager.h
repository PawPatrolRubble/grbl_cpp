#pragma once

#include <QObject>
#include <QByteArray>
#include <QColor>
#include <QJsonObject>

class QSettings;

/// Global singleton that manages application settings via a local JSON file.
/// Window/dock layout stays in QSettings (binary blobs).
class SettingsManager : public QObject
{
    Q_OBJECT

public:
    static SettingsManager& instance();

    // --- Window layout (QSettings for binary blobs) ---
    void saveLayout(const QByteArray &dockState, const QByteArray &geometry);
    [[nodiscard]] QByteArray restoreLayout(QByteArray &geometry) const;

    // --- Jog defaults ---
    void setJogStepSize(double mm);
    void setJogFeedRate(double mmPerMin);
    [[nodiscard]] double jogStepSize() const;
    [[nodiscard]] double jogFeedRate() const;

    // --- Canvas (Design page ruled-paper) ---
    struct CanvasSettings {
        bool   showRuledLines  = true;
        double lineSpacing     = 8.0;   // mm
        QColor lineColor       = QColor(200, 220, 240, 120);
        bool   showMarginLine  = true;
        double marginOffset    = 10.0;  // mm from left
        QColor marginColor     = QColor(240, 180, 180, 100);
        QColor paperColor      = QColor(255, 255, 252);
    };

    [[nodiscard]] CanvasSettings canvasSettings() const { return m_canvas; }
    void setCanvasSettings(const CanvasSettings &s);

    // --- Persist / reset ---
    void save();
    void resetToDefaults();

signals:
    void canvasSettingsChanged(const CanvasSettings &settings);

private:
    explicit SettingsManager(QObject *parent = nullptr);
    ~SettingsManager() override = default;
    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;

    [[nodiscard]] QString settingsFilePath() const;
    [[nodiscard]] QJsonObject toJson() const;
    void fromJson(const QJsonObject &root);
    void load();

    static QJsonObject colorToJson(const QColor &c);
    static QColor      colorFromJson(const QJsonObject &obj, const QColor &fallback);

    QSettings *m_settings;  // for layout blobs only

    // Cached values (JSON-backed)
    double m_jogStepSize  = 1.0;
    double m_jogFeedRate  = 500.0;
    CanvasSettings m_canvas;
};
