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

    for (int i = 1; i < fields.size(); ++i) {
        const QString &field = fields[i];

        if (field.startsWith("MPos:")) {
            QVector3D pos = parsePosition(field.mid(5));
            m_machineState->setMachinePosition(pos);
            if (i < fields.size()) {
                for (int j = i + 1; j < fields.size(); ++j) {
                    if (fields[j].startsWith("WPos:")) {
                        QVector3D wpos = parsePosition(fields[j].mid(5));
                        m_machineState->setWorkPosition(wpos);
                        break;
                    }
                }
            }
        } else if (field.startsWith("WPos:")) {
            bool hasMPos = false;
            for (const QString &f : fields) {
                if (f.startsWith("MPos:")) { hasMPos = true; break; }
            }
            if (!hasMPos) {
                QVector3D pos = parsePosition(field.mid(5));
                m_machineState->setWorkPosition(pos);
            }
        } else if (field.startsWith("Fs:")) {
            parseFeedOverrides(field.mid(3));
        } else if (field.startsWith("F:")) {
            bool ok = false;
            double feed = field.mid(2).toDouble(&ok);
            if (ok)
                m_machineState->setFeedRate(feed);
        } else if (field.startsWith("S:")) {
            bool ok = false;
            double speed = field.mid(2).toDouble(&ok);
            if (ok)
                m_machineState->setSpindleSpeed(speed);
        }
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