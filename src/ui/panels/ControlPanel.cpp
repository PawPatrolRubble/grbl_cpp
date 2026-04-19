#include "ControlPanel.h"
#include "ui_ControlPanel.h"

ControlPanel::ControlPanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ControlPanel)
{
    ui->setupUi(this);
    setObjectName("controlPanel");
    setAttribute(Qt::WA_StyledBackground, true);

    // Set slider ranges and initial values
    ui->feedOverrideSlider->setRange(0, 200);
    ui->feedOverrideSlider->setValue(100);
    ui->spindleOverrideSlider->setRange(0, 200);
    ui->spindleOverrideSlider->setValue(100);

    // Connections
    connect(ui->loadBtn, &QPushButton::clicked, this, &ControlPanel::loadFileRequested);
    connect(ui->startBtn, &QPushButton::clicked, this, &ControlPanel::startRequested);
    connect(ui->pauseBtn, &QPushButton::clicked, this, &ControlPanel::pauseRequested);
    connect(ui->resumeBtn, &QPushButton::clicked, this, &ControlPanel::resumeRequested);
    connect(ui->stopBtn, &QPushButton::clicked, this, &ControlPanel::stopRequested);
    connect(ui->homeBtn, &QPushButton::clicked, this, &ControlPanel::homeRequested);
    connect(ui->unlockBtn, &QPushButton::clicked, this, &ControlPanel::unlockRequested);
    connect(ui->resetBtn, &QPushButton::clicked, this, &ControlPanel::resetRequested);
    connect(ui->statusBtn, &QPushButton::clicked, this, &ControlPanel::statusRequested);

    // Slider connections
    connect(ui->feedOverrideSlider, &QSlider::valueChanged, this, [this](int val) {
        ui->feedOverrideLabel->setText(QString("%1%").arg(val));
    });
    connect(ui->spindleOverrideSlider, &QSlider::valueChanged, this, [this](int val) {
        ui->spindleOverrideLabel->setText(QString("%1%").arg(val));
    });
}

ControlPanel::~ControlPanel()
{
    delete ui;
}

void ControlPanel::setConnected(bool connected)
{
    Q_UNUSED(connected)
}

void ControlPanel::setFileLoaded(const QString &fileName, int lineCount)
{
    if (fileName.isEmpty() || lineCount == 0) {
        ui->progressLabel->setText("No file loaded");
        ui->startBtn->setEnabled(false);
    } else {
        ui->progressLabel->setText(QString("Loaded: %1 (%2 lines)").arg(fileName).arg(lineCount));
        ui->startBtn->setEnabled(true);
    }
}

void ControlPanel::setStreamingProgress(int current, int total)
{
    ui->progressLabel->setText(QString("Streaming: %1 / %2").arg(current).arg(total));
}

void ControlPanel::setStreamingFinished()
{
    ui->startBtn->setEnabled(true);
    ui->pauseBtn->setEnabled(false);
    ui->resumeBtn->setEnabled(false);
    ui->stopBtn->setEnabled(false);
    ui->progressLabel->setText("Streaming complete");
}

void ControlPanel::setStreamingStarted()
{
    ui->startBtn->setEnabled(false);
    ui->pauseBtn->setEnabled(true);
    ui->resumeBtn->setEnabled(false);
    ui->stopBtn->setEnabled(true);
    ui->progressLabel->setText("Streaming started...");
}

void ControlPanel::setStreamingPaused()
{
    ui->pauseBtn->setEnabled(false);
    ui->resumeBtn->setEnabled(true);
    ui->progressLabel->setText("Streaming paused");
}

void ControlPanel::setStreamingResumed()
{
    ui->pauseBtn->setEnabled(true);
    ui->resumeBtn->setEnabled(false);
    ui->progressLabel->setText("Streaming resumed");
}