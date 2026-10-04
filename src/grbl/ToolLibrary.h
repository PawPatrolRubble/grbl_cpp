#pragma once

#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QUuid>
#include <QHash>

struct PenRackSettings
{
    bool configured = false;
    double clearanceZ = 0;
    double travelFeed = 1000;
    double dockFeed = 300;
    double settleSeconds = 0.3;
    double localRadius = 5;
    double minX = -300, maxX = 0;
    double minY = -300, maxY = 0;
    double minZ = -100, maxZ = 0;
    bool motionCalibrated = false; // Straight travel at clearance has been verified.
};

enum class ToolKind { Pen };

// Identity and display metadata are independent of this machine's pen rack.
struct ToolDefinition
{
    QUuid id;
    QString name;
    QColor previewColor = Qt::black;
    bool enabled = true;
    ToolKind kind = ToolKind::Pen;
};

struct PenRackStation
{
    bool configured = false;
    QString slot;
    double slotX = 0, slotY = 0;
    double returnZ = -10, pickupZ = -10;
    QString returnMacro = "; Define release / local docking steps for this mechanism.";
    QString pickupMacro = "; Define grip / local docking steps for this mechanism.";
    bool motionCalibrated = false; // Passive pickup and return have been verified.
};

class ToolLibrary : public QObject
{
    Q_OBJECT

public:
    explicit ToolLibrary(QObject* parent = nullptr);
    const QList<ToolDefinition>& tools() const { return m_tools; }
    const ToolDefinition* tool(const QUuid& id) const;
    QUuid addTool(const QString& name, const QColor& previewColor, ToolKind kind = ToolKind::Pen);
    bool updateTool(const ToolDefinition& profile);
    bool removeTool(const QUuid& id);
    QJsonArray toJson() const;
    bool loadFromJson(const QJsonArray& array);
    PenRackStation penRackStation(const QUuid& toolId) const;
    bool updatePenRackStation(const QUuid& toolId, const PenRackStation& station);
    const PenRackSettings& rackSettings() const { return m_rack; }
    bool updateRackSettings(const PenRackSettings& settings);
    QJsonObject rackToJson() const;
    bool loadRackFromJson(const QJsonObject& json);
    QByteArray configurationFingerprint() const;

signals:
    void changed();

private:
    QList<ToolDefinition> m_tools;
    QHash<QUuid, PenRackStation> m_penStations;
    PenRackSettings m_rack;
};
