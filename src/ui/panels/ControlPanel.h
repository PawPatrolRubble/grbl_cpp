#pragma once

#include <QWidget>

namespace Ui { class ControlPanel; }

class ControlPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ControlPanel(QWidget *parent = nullptr);
    ~ControlPanel();

public slots:
    void setConnected(bool connected);
    void setFileLoaded(const QString &fileName, int lineCount);
    void setStreamingProgress(int current, int total);
    void setStreamingFinished();
    void setStreamingStarted();
    void setStreamingPaused();
    void setStreamingResumed();

signals:
    void loadFileRequested();
    void startRequested();
    void pauseRequested();
    void resumeRequested();
    void stopRequested();
    void homeRequested();
    void unlockRequested();
    void resetRequested();
    void statusRequested();

private:
    Ui::ControlPanel *ui;
};
