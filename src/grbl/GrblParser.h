#pragma once

#include <QObject>
#include <QString>
#include <QVector3D>
#include <QStringList>
#include "MachineState.h"

class GrblParser : public QObject
{
    Q_OBJECT

public:
    explicit GrblParser(MachineState *machineState, QObject *parent = nullptr);

    void parseResponse(const QString &response);
    void requestStatusReport();

    static MachineState::State parseState(const QString &stateStr);
    static QVector3D parsePosition(const QString &posStr);
    static QVector3D parseWco(const QString &wcoStr);

private:
    void parseStatusReport(const QString &report);
    void parseFeedOverrides(const QString &fsStr);

    MachineState *m_machineState;
};