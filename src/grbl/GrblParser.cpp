#include "GrblParser.h"
#include "MachineState.h"

#include <QStringList>
#include <QRegularExpression>

GrblParser::GrblParser(MachineState *machineState, QObject *parent)
    : QObject(parent)
    , m_machineState(machineState)
{
}

void GrblParser::parseResponse(const QString &response)
{
    if (response.startsWith('<') && response.endsWith('>'))
        parseStatusReport(response);
    else if (response.startsWith("error"))
        emit m_machineState->stateChanged(MachineState::State::Alarm);
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
            m_lastWco = parseWco(field.mid(4));
        } else if (field.startsWith("MPos:")) {
            parsedMPos = parsePosition(field.mid(5));
            hasMPos = true;
        } else if (field.startsWith("WPos:")) {
            parsedWPos = parsePosition(field.mid(5));
            hasWPos = true;
        } else if (field.startsWith("Fs:")) {
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
        m_machineState->setMachinePosition(parsedMPos);
        m_machineState->setWorkPosition(parsedMPos - m_lastWco);
    } else if (hasWPos) {
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