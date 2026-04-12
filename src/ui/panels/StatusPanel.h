#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include "grbl/MachineState.h"

class StatusPanel : public QWidget
{
    Q_OBJECT

public:
    explicit StatusPanel(QWidget *parent = nullptr);

public slots:
    void setMachinePosition(const QVector3D &pos);
    void setWorkPosition(const QVector3D &pos);
    void setState(MachineState::State state);
    void setFeedRate(double rate);
    void setSpindleSpeed(double speed);

signals:
    void zeroAxisRequested(const QString &axis);
    void zeroAllRequested();
    void goToZeroRequested();
    void resetWorkOffsetRequested();  // Set work = machine (G10 L2 P1)

private:
    QLabel *createAxisLabel(const QString &axis, const QString &objectName);
    QLabel *createSideLabel(const QString &text, const QString &objectName);
    QPushButton *createZeroButton(const QString &axis);

    QLabel *m_stateLabel;
    QLabel *m_mposX, *m_mposY, *m_mposZ;
    QLabel *m_wposX, *m_wposY, *m_wposZ;
    QLabel *m_feedLabel;
    QLabel *m_spindleLabel;
};
