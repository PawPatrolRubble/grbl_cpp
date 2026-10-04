#include "ToolLibrary.h"

#include <QJsonObject>
#include <QSet>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <cmath>

namespace {
bool finiteStation(const PenRackStation& pen)
{
    return std::isfinite(pen.slotX) && std::isfinite(pen.slotY)
        && std::isfinite(pen.returnZ) && std::isfinite(pen.pickupZ);
}
bool finiteRack(const PenRackSettings& rack)
{
    for (double value : {rack.clearanceZ, rack.travelFeed, rack.dockFeed, rack.settleSeconds,
                         rack.localRadius, rack.minX, rack.maxX, rack.minY, rack.maxY, rack.minZ, rack.maxZ})
        if (!std::isfinite(value)) return false;
    return true;
}
bool readNumber(const QJsonObject& json, const char* key, double& target)
{
    if (!json.contains(key)) return true;
    if (!json[key].isDouble() || !std::isfinite(json[key].toDouble())) return false;
    target = json[key].toDouble();
    return true;
}
bool readStation(const QJsonObject& json, PenRackStation& pen)
{
    for (const char* field : {"slot", "returnMacro", "pickupMacro"})
        if (json.contains(field) && !json[field].isString()) return false;
    if (json.contains("configured") && !json["configured"].isBool()) return false;
    if (json.contains("motionCalibrated") && !json["motionCalibrated"].isBool()) return false;
    pen.configured = json["configured"].toBool(false);
    pen.motionCalibrated = json["motionCalibrated"].toBool(false);
    pen.slot = json["slot"].toString().trimmed();
    pen.returnMacro = json["returnMacro"].toString(pen.returnMacro);
    pen.pickupMacro = json["pickupMacro"].toString(pen.pickupMacro);
    return readNumber(json, "x", pen.slotX) && readNumber(json, "y", pen.slotY)
        && readNumber(json, "returnZ", pen.returnZ) && readNumber(json, "pickupZ", pen.pickupZ);
}
} // namespace

bool ToolLibrary::updateRackSettings(const PenRackSettings& rack)
{
    if (!finiteRack(rack)) return false;
    const auto before = rackToJson();
    m_rack = rack;
    auto previousMotion = before;
    auto nextMotion = rackToJson();
    previousMotion.remove("motionCalibrated");
    nextMotion.remove("motionCalibrated");
    if (previousMotion != nextMotion) m_rack.motionCalibrated = false;
    emit changed();
    return true;
}
QJsonObject ToolLibrary::rackToJson() const
{
    return {{"schemaVersion", 1}, {"configured", m_rack.configured}, {"motionCalibrated", m_rack.motionCalibrated}, {"clearanceZ", m_rack.clearanceZ},
            {"travelFeed", m_rack.travelFeed}, {"dockFeed", m_rack.dockFeed}, {"settleSeconds", m_rack.settleSeconds},
            {"localRadius", m_rack.localRadius}, {"minX", m_rack.minX}, {"maxX", m_rack.maxX},
            {"minY", m_rack.minY}, {"maxY", m_rack.maxY}, {"minZ", m_rack.minZ}, {"maxZ", m_rack.maxZ}};
}
bool ToolLibrary::loadRackFromJson(const QJsonObject& json)
{
    if (json.contains("schemaVersion") && json["schemaVersion"].toInt(-1) != 1) return false;
    if (json.contains("configured") && !json["configured"].isBool()) return false;
    if (json.contains("motionCalibrated") && !json["motionCalibrated"].isBool()) return false;
    PenRackSettings rack;
    rack.configured = json["configured"].toBool(false);
    rack.motionCalibrated = json["motionCalibrated"].toBool(false);
    if (!readNumber(json, "clearanceZ", rack.clearanceZ) || !readNumber(json, "travelFeed", rack.travelFeed)
        || !readNumber(json, "dockFeed", rack.dockFeed) || !readNumber(json, "settleSeconds", rack.settleSeconds)
        || !readNumber(json, "localRadius", rack.localRadius) || !readNumber(json, "minX", rack.minX)
        || !readNumber(json, "maxX", rack.maxX) || !readNumber(json, "minY", rack.minY)
        || !readNumber(json, "maxY", rack.maxY) || !readNumber(json, "minZ", rack.minZ)
        || !readNumber(json, "maxZ", rack.maxZ)) return false;
    m_rack = rack; // Loading preserves the persisted calibration for this exact configuration.
    emit changed();
    return true;
}

ToolLibrary::ToolLibrary(QObject* parent) : QObject(parent) {}

const ToolDefinition* ToolLibrary::tool(const QUuid& id) const
{
    for (const auto& profile : m_tools) {
        if (profile.id == id) return &profile;
    }
    return nullptr;
}

QUuid ToolLibrary::addTool(const QString& name, const QColor& previewColor, ToolKind kind)
{
    if (name.trimmed().isEmpty() || !previewColor.isValid() || kind != ToolKind::Pen) return {};
    ToolDefinition profile{QUuid::createUuid(), name.trimmed(), previewColor, true, kind};
    const QUuid id = profile.id;
    m_tools.append(profile);
    emit changed();
    return id;
}

bool ToolLibrary::updateTool(const ToolDefinition& profile)
{
    if (profile.id.isNull() || profile.name.trimmed().isEmpty() || !profile.previewColor.isValid()
        || profile.kind != ToolKind::Pen)
        return false;
    for (auto& existing : m_tools) {
        if (existing.id != profile.id) continue;
        existing = profile;
        existing.name = existing.name.trimmed();
        emit changed();
        return true;
    }
    return false;
}

bool ToolLibrary::removeTool(const QUuid& id)
{
    for (qsizetype index = 0; index < m_tools.size(); ++index) {
        if (m_tools[index].id != id) continue;
        m_tools.removeAt(index);
        m_penStations.remove(id);
        emit changed();
        return true;
    }
    return false;
}

QJsonArray ToolLibrary::toJson() const
{
    QJsonArray array;
    for (const auto& profile : m_tools) {
        const auto station = penRackStation(profile.id);
        array.append(QJsonObject{{"id", profile.id.toString()}, {"name", profile.name},
                                 {"kind", "pen"}, {"previewColor", profile.previewColor.name(QColor::HexArgb)},
                                 {"enabled", profile.enabled},
                                 // Retain the station envelope for legacy settings round trips.
                                 {"rack", QJsonObject{{"configured", station.configured}, {"motionCalibrated", station.motionCalibrated}, {"slot", station.slot},
                                     {"x", station.slotX}, {"y", station.slotY}, {"returnZ", station.returnZ},
                                     {"pickupZ", station.pickupZ}, {"returnMacro", station.returnMacro},
                                     {"pickupMacro", station.pickupMacro}}}});
    }
    return array;
}

bool ToolLibrary::loadFromJson(const QJsonArray& array)
{
    QList<ToolDefinition> profiles;
    QHash<QUuid, PenRackStation> stations;
    QSet<QUuid> ids;
    for (const auto& value : array) {
        if (!value.isObject()) return false;
        const QJsonObject json = value.toObject();
        if (json.contains("kind") && (!json["kind"].isString() || json["kind"].toString() != "pen")) return false;
        const auto color = json.contains("previewColor") ? json["previewColor"] : json["color"];
        if (!color.isString()) return false;
        ToolDefinition profile{QUuid::fromString(json["id"].toString()),
                                   json["name"].toString().trimmed(),
                                   QColor(color.toString()),
                                   json["enabled"].toBool(true)};
        if (profile.id.isNull() || ids.contains(profile.id) || profile.name.isEmpty()
            || !profile.previewColor.isValid()) return false;
        PenRackStation station;
        if (json.contains("rack") && (!json["rack"].isObject() || !readStation(json["rack"].toObject(), station)))
            return false;
        ids.insert(profile.id);
        profiles.append(profile);
        stations.insert(profile.id, station);
    }
    m_tools = std::move(profiles);
    m_penStations = std::move(stations);
    emit changed();
    return true;
}

QByteArray ToolLibrary::configurationFingerprint() const
{
    const QJsonObject configuration{{"tools", toJson()}, {"rack", rackToJson()}};
    return QCryptographicHash::hash(QJsonDocument(configuration).toJson(QJsonDocument::Compact),
                                    QCryptographicHash::Sha256);
}

PenRackStation ToolLibrary::penRackStation(const QUuid& toolId) const
{
    return m_penStations.value(toolId);
}

bool ToolLibrary::updatePenRackStation(const QUuid& toolId, const PenRackStation& station)
{
    const auto* definition = tool(toolId);
    if (!definition || definition->kind != ToolKind::Pen || !finiteStation(station)) return false;
    const auto existing = penRackStation(toolId);
    const bool motionChanged = existing.configured != station.configured
        || existing.slot != station.slot.trimmed() || existing.slotX != station.slotX
        || existing.slotY != station.slotY || existing.returnZ != station.returnZ
        || existing.pickupZ != station.pickupZ || existing.returnMacro != station.returnMacro
        || existing.pickupMacro != station.pickupMacro;
    auto saved = station;
    saved.slot = saved.slot.trimmed();
    if (motionChanged) saved.motionCalibrated = false;
    m_penStations.insert(toolId, saved);
    emit changed();
    return true;
}
