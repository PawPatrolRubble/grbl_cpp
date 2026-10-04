#pragma once

#include <QObject>
#include <QString>
#include <QVector3D>
#include <QStringList>
#include <QHash>
#include <QElapsedTimer>
#include <optional>
#include "MachineState.h"

class GrblParser : public QObject
{
    Q_OBJECT

public:
    explicit GrblParser(MachineState *machineState, QObject *parent = nullptr);
    ~GrblParser() override = default;

    void parseResponse(const QString &response);
    void requestStatusReport();
    std::optional<QVector3D> workCoordinateOffset(int frame) const;
    std::optional<QVector3D> currentMachinePosition() const;
    void invalidateCoordinates();
    void invalidateOffsets();
    void invalidateMachinePosition() { m_positionKnown = false; }

    [[nodiscard]] static MachineState::State parseState(const QString &stateStr);
    [[nodiscard]] static QVector3D parsePosition(const QString &posStr);
    [[nodiscard]] static QVector3D parseWco(const QString &wcoStr);

private:
    void parseStatusReport(const QString &report);
    void parseFeedOverrides(const QString &fsStr);
    void parseGCodeParameters(const QString &line);
    void parseGCodeState(const QString &line);
    void parseGrblSetting(const QString &line);
    void updateWcoFromOffsets();

    MachineState *m_machineState;
    QVector3D m_lastWco;
    QHash<QString, QVector3D> m_wcsOffsets;
    QString m_activeWcs = QStringLiteral("G54");
    bool m_positionKnown = false;
    bool m_wcoKnown = false;
    QElapsedTimer m_positionAge;
};
