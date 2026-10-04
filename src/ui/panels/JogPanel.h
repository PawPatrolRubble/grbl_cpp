#pragma once

#include <QWidget>
#include <QPushButton>

class QKeyEvent;

namespace Ui { class JogPanel; }

class JogPanel : public QWidget
{
    Q_OBJECT

public:
    explicit JogPanel(QWidget *parent = nullptr);
    ~JogPanel();

    bool handleKeyPress(QKeyEvent *event);
    bool handleKeyRelease(QKeyEvent *event);

public slots:
    void setMotionEnabled(bool enabled);
    void setJogEnabled(bool enabled);
    void setAbsoluteMotionEnabled(bool enabled);

signals:
    void jogContinuousRequested(const QString &axis, int direction, double feed);
    void jogStepRequested(const QString &axis, double distance, double feed);
    void jogCancelRequested();
    void goToAbsoluteRequested(double x, double y, double z, double feed);
    void goToXRequested(double x, double feed);
    void goToYRequested(double y, double feed);
    void goToZRequested(double z, double feed);

private:
    enum class JogMode { Continuous, Step };

    void doJog(const QString &axis, int direction);

    JogMode m_jogMode = JogMode::Continuous;
    Ui::JogPanel *ui;
};
