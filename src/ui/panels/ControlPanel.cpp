#include "ControlPanel.h"
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>

ControlPanel::ControlPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *group = new QGroupBox("Streaming Control");
    auto *ctrlLayout = new QVBoxLayout(group);

    // Load button
    m_loadBtn = new QPushButton("Load G-Code File");
    ctrlLayout->addWidget(m_loadBtn);

    // Streaming buttons
    m_startBtn = new QPushButton("Start");
    m_startBtn->setEnabled(false);
    m_pauseBtn = new QPushButton("Pause");
    m_pauseBtn->setEnabled(false);
    m_resumeBtn = new QPushButton("Resume");
    m_resumeBtn->setEnabled(false);
    m_stopBtn = new QPushButton("Stop");
    m_stopBtn->setEnabled(false);

    auto *streamLayout = new QHBoxLayout();
    streamLayout->addWidget(m_startBtn);
    streamLayout->addWidget(m_pauseBtn);
    streamLayout->addWidget(m_resumeBtn);
    streamLayout->addWidget(m_stopBtn);
    ctrlLayout->addLayout(streamLayout);

    // Progress
    m_progressLabel = new QLabel("No file loaded");
    ctrlLayout->addWidget(m_progressLabel);

    // Overrides
    auto *overrideGroup = new QGroupBox("Overrides");
    auto *overrideLayout = new QFormLayout(overrideGroup);

    m_feedOverrideSlider = new QSlider(Qt::Horizontal);
    m_feedOverrideSlider->setRange(0, 200);
    m_feedOverrideSlider->setValue(100);
    m_feedOverrideLabel = new QLabel("100%");
    auto *feedLayout = new QHBoxLayout();
    feedLayout->addWidget(m_feedOverrideSlider);
    feedLayout->addWidget(m_feedOverrideLabel);
    overrideLayout->addRow("Feed:", feedLayout);

    m_spindleOverrideSlider = new QSlider(Qt::Horizontal);
    m_spindleOverrideSlider->setRange(0, 200);
    m_spindleOverrideSlider->setValue(100);
    m_spindleOverrideLabel = new QLabel("100%");
    auto *spindleLayout = new QHBoxLayout();
    spindleLayout->addWidget(m_spindleOverrideSlider);
    spindleLayout->addWidget(m_spindleOverrideLabel);
    overrideLayout->addRow("Spindle:", spindleLayout);

    ctrlLayout->addWidget(overrideGroup);

    // Quick commands
    auto *cmdGroup = new QGroupBox("Quick Commands");
    auto *cmdLayout = new QHBoxLayout(cmdGroup);
    auto *homeBtn = new QPushButton("Home ($H)");
    auto *unlockBtn = new QPushButton("Unlock ($X)");
    auto *resetBtn = new QPushButton("Reset");
    auto *statusBtn = new QPushButton("Status (?)");
    cmdLayout->addWidget(homeBtn);
    cmdLayout->addWidget(unlockBtn);
    cmdLayout->addWidget(resetBtn);
    cmdLayout->addWidget(statusBtn);
    ctrlLayout->addWidget(cmdGroup);

    // Outer layout
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(group);

    // Connections
    connect(m_loadBtn, &QPushButton::clicked, this, &ControlPanel::loadFileRequested);
    connect(m_startBtn, &QPushButton::clicked, this, &ControlPanel::startRequested);
    connect(m_pauseBtn, &QPushButton::clicked, this, &ControlPanel::pauseRequested);
    connect(m_resumeBtn, &QPushButton::clicked, this, &ControlPanel::resumeRequested);
    connect(m_stopBtn, &QPushButton::clicked, this, &ControlPanel::stopRequested);
    connect(homeBtn, &QPushButton::clicked, this, &ControlPanel::homeRequested);
    connect(unlockBtn, &QPushButton::clicked, this, &ControlPanel::unlockRequested);
    connect(resetBtn, &QPushButton::clicked, this, &ControlPanel::resetRequested);
    connect(statusBtn, &QPushButton::clicked, this, &ControlPanel::statusRequested);
}

void ControlPanel::setConnected(bool connected)
{
    Q_UNUSED(connected)
    // Could enable/disable quick command buttons here if needed
}

void ControlPanel::setFileLoaded(const QString &fileName, int lineCount)
{
    m_progressLabel->setText(QString("Loaded: %1 (%2 lines)").arg(fileName).arg(lineCount));
    m_startBtn->setEnabled(true);
}

void ControlPanel::setStreamingProgress(int current, int total)
{
    m_progressLabel->setText(QString("Streaming: %1 / %2").arg(current).arg(total));
}

void ControlPanel::setStreamingFinished()
{
    m_startBtn->setEnabled(true);
    m_pauseBtn->setEnabled(false);
    m_resumeBtn->setEnabled(false);
    m_stopBtn->setEnabled(false);
    m_progressLabel->setText("Streaming complete");
}

void ControlPanel::setStreamingStarted()
{
    m_startBtn->setEnabled(false);
    m_pauseBtn->setEnabled(true);
    m_resumeBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_progressLabel->setText("Streaming started...");
}

void ControlPanel::setStreamingPaused()
{
    m_pauseBtn->setEnabled(false);
    m_resumeBtn->setEnabled(true);
    m_progressLabel->setText("Streaming paused");
}

void ControlPanel::setStreamingResumed()
{
    m_pauseBtn->setEnabled(true);
    m_resumeBtn->setEnabled(false);
    m_progressLabel->setText("Streaming resumed");
}

