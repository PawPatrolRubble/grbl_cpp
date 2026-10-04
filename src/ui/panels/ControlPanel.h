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
    void setExecutionReady(bool ready);
    void setStreamingEnded(const QString& message);
    void setFileLoaded(const QString &fileName, int lineCount);
    void setStreamingProgress(int current, int total);
    void setExecutionPhase(const QString& phase) { m_executionPhase = phase; }
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
    QString m_executionPhase;
    Ui::ControlPanel *ui;
    bool m_connected = false;
    bool m_ready = false;
    bool m_fileLoaded = false;
    bool m_streaming = false;
    void updateControls();
};
