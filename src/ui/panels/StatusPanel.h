#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include "grbl/MachineState.h"

namespace Ui { class StatusPanel; }

class StatusPanel : public QWidget
{
    Q_OBJECT

public:
    explicit StatusPanel(QWidget *parent = nullptr);
    ~StatusPanel();

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
    void resetWorkOffsetRequested();
    void emergencyStopRequested();
    void homeMachineRequested();

private:
    Ui::StatusPanel *ui;
};
