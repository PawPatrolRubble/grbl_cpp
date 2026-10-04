#include "SettingsManager.h"
#include "grbl/ToolLibrary.h"

#include <QSettings>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QSaveFile>

SettingsManager& SettingsManager::instance()
{
    static SettingsManager s;
    return s;
}

SettingsManager::SettingsManager(QObject *parent)
    : QObject(parent)
{
    m_settings = new QSettings("GrblSender", "GrblSender", this);
    m_toolLibrary = new ToolLibrary(this);
    load();
    connect(m_toolLibrary, &ToolLibrary::changed, this, &SettingsManager::save);
}

// ---- Layout (binary, stays in QSettings) ----

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

// ---- Jog defaults ----

void SettingsManager::setJogStepSize(double mm)
{
    m_jogStepSize = mm;
    save();
}

void SettingsManager::setJogFeedRate(double mmPerMin)
{
    m_jogFeedRate = mmPerMin;
    save();
}

double SettingsManager::jogStepSize() const  { return m_jogStepSize; }
double SettingsManager::jogFeedRate() const  { return m_jogFeedRate; }

// ---- Canvas ----

void SettingsManager::setCanvasSettings(const CanvasSettings &s)
{
    m_canvas = s;
    save();
    emit canvasSettingsChanged(m_canvas);
}

// ---- Reset ----

void SettingsManager::resetToDefaults()
{
    m_settings->clear();
    m_settings->sync();

    m_jogStepSize = 1.0;
    m_jogFeedRate = 500.0;
    m_canvas = CanvasSettings{};
    save();
    emit canvasSettingsChanged(m_canvas);
}

// ---- JSON persistence ----

QString SettingsManager::settingsFilePath() const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    return dir + "/settings.json";
}

QJsonObject SettingsManager::colorToJson(const QColor &c)
{
    QJsonObject obj;
    obj["r"] = c.red();
    obj["g"] = c.green();
    obj["b"] = c.blue();
    obj["a"] = c.alpha();
    return obj;
}

QColor SettingsManager::colorFromJson(const QJsonObject &obj, const QColor &fallback)
{
    if (obj.isEmpty()) return fallback;
    return QColor(obj["r"].toInt(fallback.red()),
                  obj["g"].toInt(fallback.green()),
                  obj["b"].toInt(fallback.blue()),
                  obj["a"].toInt(fallback.alpha()));
}

QJsonObject SettingsManager::toJson() const
{
    QJsonObject root;
    root["tools"] = m_toolLibrary->toJson();
    root["penRack"] = m_toolLibrary->rackToJson();

    // Jog
    QJsonObject jog;
    jog["stepSize"] = m_jogStepSize;
    jog["feedRate"] = m_jogFeedRate;
    root["jog"] = jog;

    // Canvas
    QJsonObject canvas;
    canvas["showRuledLines"] = m_canvas.showRuledLines;
    canvas["lineSpacing"]    = m_canvas.lineSpacing;
    canvas["lineColor"]      = colorToJson(m_canvas.lineColor);
    canvas["showMarginLine"] = m_canvas.showMarginLine;
    canvas["marginOffset"]   = m_canvas.marginOffset;
    canvas["marginColor"]    = colorToJson(m_canvas.marginColor);
    canvas["paperColor"]     = colorToJson(m_canvas.paperColor);
    root["canvas"] = canvas;

    return root;
}

void SettingsManager::fromJson(const QJsonObject &root)
{
    CanvasSettings defaults;
    // An explicitly empty tools collection must not resurrect legacy pens.
    const auto definitions = root.contains("tools") ? root["tools"] : root["pens"];
    if (definitions.isArray()) m_toolLibrary->loadFromJson(definitions.toArray());
    m_toolLibrary->loadRackFromJson(root["penRack"].toObject());

    // Jog
    QJsonObject jog = root["jog"].toObject();
    m_jogStepSize = jog["stepSize"].toDouble(1.0);
    m_jogFeedRate = jog["feedRate"].toDouble(500.0);

    // Canvas
    QJsonObject canvas = root["canvas"].toObject();
    m_canvas.showRuledLines = canvas["showRuledLines"].toBool(defaults.showRuledLines);
    m_canvas.lineSpacing    = canvas["lineSpacing"].toDouble(defaults.lineSpacing);
    m_canvas.lineColor      = colorFromJson(canvas["lineColor"].toObject(), defaults.lineColor);
    m_canvas.showMarginLine = canvas["showMarginLine"].toBool(defaults.showMarginLine);
    m_canvas.marginOffset   = canvas["marginOffset"].toDouble(defaults.marginOffset);
    m_canvas.marginColor    = colorFromJson(canvas["marginColor"].toObject(), defaults.marginColor);
    m_canvas.paperColor     = colorFromJson(canvas["paperColor"].toObject(), defaults.paperColor);
}

void SettingsManager::save()
{
    QSaveFile file(settingsFilePath());
    if (!file.open(QIODevice::WriteOnly)) {
        emit settingsSaveFailed(file.errorString());
        return;
    }
    file.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
    if (!file.commit()) emit settingsSaveFailed(file.errorString());
}

void SettingsManager::load()
{
    QFile file(settingsFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return;
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error == QJsonParseError::NoError && doc.isObject())
        fromJson(doc.object());
}
