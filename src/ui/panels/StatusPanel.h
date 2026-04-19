#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
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
    void setMessage(const QString &msg);
    void setMotionEnabled(bool enabled);

signals:
    void zeroAxisRequested(const QString &axis);
    void setAxisPositionRequested(const QString &axis, double value);
    void zeroAllRequested();
    void goToZeroRequested();
    void resetWorkOffsetRequested();  // Set work = machine (G10 L2 P1)
    void emergencyStopRequested();
    void homeMachineRequested();

private:
    QLabel *createAxisLabel(const QString &axis, const QString &objectName);
    QLabel *createSideLabel(const QString &text, const QString &objectName);
    QPushButton *createZeroButton(const QString &axis);

    QLabel *m_stateLabel;
    QLabel *m_messageLabel;
    QLabel *m_mposX, *m_mposY, *m_mposZ;
    QLabel *m_wposX, *m_wposY, *m_wposZ;
    QLabel *m_feedLabel;
    QLabel *m_spindleLabel;
    QPushButton *m_zeroXBtn, *m_zeroYBtn, *m_zeroZBtn;
    QLineEdit *m_setXEdit, *m_setYEdit, *m_setZEdit;
    QPushButton *m_setXBtn, *m_setYBtn, *m_setZBtn;
    QPushButton *m_zeroAllBtn;
    QPushButton *m_goToZeroBtn;
    QPushButton *m_resetOffsetBtn;
    QPushButton *m_eStopBtn;
    QPushButton *m_homeBtn;
};
