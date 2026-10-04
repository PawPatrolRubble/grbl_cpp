#include "GrblParser.h"
#include "MachineState.h"

#include <QStringList>
#include <QRegularExpression>
#include <cmath>
#include <limits>

namespace {
std::optional<QVector3D> validatedPosition(const QString& text)
{
    const auto words = text.split(',');
    if (words.size() != 3) return {};
    double values[3];
    for (int axis = 0; axis < 3; ++axis) {
        bool ok = false;
        values[axis] = words[axis].toDouble(&ok);
        if (!ok || !std::isfinite(values[axis]) || std::abs(values[axis]) > std::numeric_limits<float>::max()) return {};
    }
    return QVector3D(values[0], values[1], values[2]);
}
}

void GrblParser::invalidateOffsets()
{
    m_wcsOffsets.clear();
    m_wcoKnown = false;
    m_positionKnown = false;
}
void GrblParser::invalidateCoordinates()
{
    invalidateOffsets();
    m_positionAge.invalidate();
}
std::optional<QVector3D> GrblParser::workCoordinateOffset(int frame) const
{
    const auto key = QString("G%1").arg(frame);
    if (frame < 54 || frame > 59 || !m_wcsOffsets.contains(key)
        || !m_wcsOffsets.contains("G92") || !m_wcsOffsets.contains("TLO")) return {};
    return m_wcsOffsets[key] + m_wcsOffsets["G92"] + m_wcsOffsets["TLO"];
}
std::optional<QVector3D> GrblParser::currentMachinePosition() const
{
    if (!m_positionKnown || !m_positionAge.isValid() || m_positionAge.elapsed() > 5000) return {};
    return m_machineState->machinePosition();
}

GrblParser::GrblParser(MachineState *machineState, QObject *parent)
    : QObject(parent)
    , m_machineState(machineState)
{
}

void GrblParser::parseResponse(const QString &response)
{
    if (response.startsWith('<') && response.endsWith('>'))
        parseStatusReport(response);
    else if (response.startsWith("[G5") || response.startsWith("[G28:") ||
             response.startsWith("[G30:") || response.startsWith("[G92:") ||
             response.startsWith("[TLO:"))
        parseGCodeParameters(response);
    else if (response.startsWith("[GC:"))
        parseGCodeState(response);
    else if (response.startsWith("[MSG:"))
        m_machineState->setMessage(response.mid(5, response.size() - 6));
    else if (response.startsWith("ALARM:")) {
        m_machineState->setState(MachineState::State::Alarm);
        m_machineState->setMessage(response);
    } else if (response.startsWith("error:"))
        m_machineState->setMessage(response);
    else if (response.startsWith("Grbl")) {
        invalidateCoordinates();
        m_lastWco = {};
        m_wcsOffsets.clear();
        m_activeWcs = "G54";
        m_machineState->reset();
    }
    else if (response.startsWith("$") && response.contains("="))
        parseGrblSetting(response);
}

void GrblParser::requestStatusReport()
{
}

void GrblParser::parseStatusReport(const QString &report)
{
    QString content = report.mid(1, report.size() - 2);
    QStringList fields = content.split('|');

    if (fields.isEmpty())
        return;

    MachineState::State state = parseState(fields[0]);
    m_machineState->setState(state);

    bool hasMPos = false;
    bool hasWPos = false;
    QVector3D parsedMPos;
    QVector3D parsedWPos;

    for (int i = 1; i < fields.size(); ++i) {
        const QString &field = fields[i];

        if (field.startsWith("WCO:")) {
            const auto value = validatedPosition(field.mid(4));
            m_wcoKnown = value.has_value();
            if (value) { m_lastWco = *value; m_machineState->setWco(m_lastWco); }
        } else if (field.startsWith("MPos:")) {
            const auto value = validatedPosition(field.mid(5));
            hasMPos = value.has_value();
            if (value) parsedMPos = *value;
            else m_positionKnown = false;
        } else if (field.startsWith("WPos:")) {
            const auto value = validatedPosition(field.mid(5));
            hasWPos = value.has_value();
            if (value) parsedWPos = *value;
            else m_positionKnown = false;
        } else if (field.startsWith("FS:") || field.startsWith("Fs:")) {
            parseFeedOverrides(field.mid(3));
        } else if (field.startsWith("F:")) {
            bool ok = false;
            double feed = field.mid(2).toDouble(&ok);
            if (ok) m_machineState->setFeedRate(feed);
        } else if (field.startsWith("S:")) {
            bool ok = false;
            double speed = field.mid(2).toDouble(&ok);
            if (ok) m_machineState->setSpindleSpeed(speed);
        }
    }

    if (hasMPos) {
        m_positionKnown = true;
        m_positionAge.start();
        m_machineState->setMachinePosition(parsedMPos);
        m_machineState->setWorkPosition(parsedMPos - m_lastWco);
    } else if (hasWPos && m_wcoKnown) {
        m_positionKnown = true;
        m_positionAge.start();
        m_machineState->setWorkPosition(parsedWPos);
        m_machineState->setMachinePosition(parsedWPos + m_lastWco);
    }
}

void GrblParser::parseFeedOverrides(const QString &fsStr)
{
    QStringList parts = fsStr.split(',');
    if (parts.size() >= 1) {
        bool ok = false;
        double feed = parts[0].toDouble(&ok);
        if (ok)
            m_machineState->setFeedRate(feed);
    }
    if (parts.size() >= 2) {
        bool ok = false;
        double spindle = parts[1].toDouble(&ok);
        if (ok)
            m_machineState->setSpindleSpeed(spindle);
    }
}

MachineState::State GrblParser::parseState(const QString &stateStr)
{
    if (stateStr.startsWith("Idle"))
        return MachineState::State::Idle;
    else if (stateStr.startsWith("Run"))
        return MachineState::State::Run;
    else if (stateStr.startsWith("Hold"))
        return MachineState::State::Hold;
    else if (stateStr.startsWith("Home"))
        return MachineState::State::Home;
    else if (stateStr.startsWith("Alarm"))
        return MachineState::State::Alarm;
    else if (stateStr.startsWith("Jog"))
        return MachineState::State::Jog;
    else if (stateStr.startsWith("Door"))
        return MachineState::State::Door;
    else if (stateStr.startsWith("Check"))
        return MachineState::State::Check;
    else if (stateStr.startsWith("Sleep"))
        return MachineState::State::Sleep;
    return MachineState::State::Unknown;
}

QVector3D GrblParser::parsePosition(const QString &posStr)
{
    QStringList coords = posStr.split(',');
    double x = 0, y = 0, z = 0;
    if (coords.size() >= 1) x = coords[0].toDouble();
    if (coords.size() >= 2) y = coords[1].toDouble();
    if (coords.size() >= 3) z = coords[2].toDouble();
    return QVector3D(x, y, z);
}

QVector3D GrblParser::parseWco(const QString &wcoStr)
{
    return parsePosition(wcoStr);
}

void GrblParser::parseGCodeParameters(const QString &line)
{
    // Format: [G54:0.000,0.000,0.000] or [TLO:0.000] or [PRB:...]
    QString content = line.mid(1, line.size() - 2);
    int colonPos = content.indexOf(':');
    if (colonPos < 0) return;

    QString key = content.left(colonPos);
    QString value = content.mid(colonPos + 1);

    if (key == QLatin1String("TLO")) {
        bool ok;
        double tlo = value.toDouble(&ok);
        if (ok && std::isfinite(tlo) && std::abs(tlo) <= std::numeric_limits<float>::max())
            m_wcsOffsets[key] = QVector3D(0, 0, static_cast<float>(tlo));
        else m_wcsOffsets.remove(key);
    } else if (key == QLatin1String("PRB")) {
        return;  // Skip probe data
    } else {
        const auto coordinates = validatedPosition(value);
        if (coordinates) m_wcsOffsets[key] = *coordinates;
        else m_wcsOffsets.remove(key);
    }

    if (key == m_activeWcs || key == QLatin1String("G92") || key == QLatin1String("TLO"))
        updateWcoFromOffsets();
}

void GrblParser::parseGCodeState(const QString &line)
{
    // Format: [GC:G0 G54 G17 G21 G90 G94 M5 M9 T0 F0 S0]
    QString content = line.mid(4, line.size() - 5);
    QStringList tokens = content.split(' ');

    for (const QString &token : tokens) {
        if (token.size() == 3 && token >= "G54" && token <= "G59") {
            m_activeWcs = token;
            updateWcoFromOffsets();
            return;
        }
    }
}

void GrblParser::updateWcoFromOffsets()
{
    m_wcoKnown = workCoordinateOffset(m_activeWcs.mid(1).toInt()).has_value();
    QVector3D wco = m_wcsOffsets.value(m_activeWcs, QVector3D());
    wco += m_wcsOffsets.value(QStringLiteral("G92"), QVector3D());
    wco += m_wcsOffsets.value(QStringLiteral("TLO"), QVector3D());

    m_lastWco = wco;
    m_machineState->setWco(wco);
    // Immediately recalculate work position so the UI updates
    // without waiting for the next status report poll.
    m_machineState->setWorkPosition(m_machineState->machinePosition() - wco);
}

void GrblParser::parseGrblSetting(const QString &line)
{
    // Format: $N=value (e.g. "$110=1000.000")
    int eqPos = line.indexOf('=');
    if (eqPos < 2) return;

    bool idOk = false;
    int id = line.mid(1, eqPos - 1).toInt(&idOk);
    if (!idOk) return;

    bool valOk = false;
    double value = line.mid(eqPos + 1).toDouble(&valOk);
    if (!valOk) return;

    m_machineState->setGrblSetting(id, value);
}
