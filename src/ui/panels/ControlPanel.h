#pragma once

#include <QWidget>
#include <QPushButton>
#include <QSlider>
#include <QLabel>

class ControlPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ControlPanel(QWidget *parent = nullptr);

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
    QPushButton *m_loadBtn;
    QPushButton *m_startBtn;
    QPushButton *m_pauseBtn;
    QPushButton *m_resumeBtn;
    QPushButton *m_stopBtn;
    QLabel *m_progressLabel;
    QSlider *m_feedOverrideSlider;
    QLabel *m_feedOverrideLabel;
    QSlider *m_spindleOverrideSlider;
    QLabel *m_spindleOverrideLabel;
};
