#include "MainWindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QKeyEvent>
#include <QApplication>
#include <QHeaderView>
#include <QGroupBox>
#include <QFormLayout>
#include <QMenuBar>
#include <QStatusBar>
#include <QDoubleSpinBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_machineState(new MachineState(this))
    , m_serialManager(new SerialManager(m_machineState, this))
    , m_grblParser(new GrblParser(m_machineState, this))
    , m_gcodeParser(new GcodeParser(this))
    , m_fileManager(new FileManager(this))
    , m_statusTimer(new QTimer(this))
{
    setWindowTitle("GRBL G-Code Sender");
    resize(1200, 800);
    setupUi();
    setupMenuBar();
    setupStatusBar();
    setupConnections();
    updateUiState();
}

MainWindow::~MainWindow()
{
    if (m_serialManager->isConnected())
        m_serialManager->disconnectFromGrbl();
}

void MainWindow::setupUi()
{
    auto *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto *mainLayout = new QHBoxLayout(centralWidget);

    auto *leftPanel = new QVBoxLayout();
    auto *rightPanel = new QVBoxLayout();

    createConnectionPanel(leftPanel);
    createStatusPanel(leftPanel);
    createControlPanel(leftPanel);
    createJogPanel(leftPanel);

    createEditorPanel(rightPanel);
    createConsolePanel(rightPanel);

    auto *leftWidget = new QWidget();
    leftWidget->setLayout(leftPanel);
    leftWidget->setMaximumWidth(360);

    auto *rightWidget = new QWidget();
    rightWidget->setLayout(rightPanel);

    auto *splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(leftWidget);
    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);

    mainLayout->addWidget(splitter);
}

void MainWindow::createConnectionPanel(QVBoxLayout *layout)
{
    auto *group = new QGroupBox("Connection");
    auto *form = new QFormLayout(group);

    m_portCombo = new QComboBox();
    m_portCombo->addItems(SerialManager::availablePorts());
    m_portCombo->setEditable(true);
    form->addRow("Port:", m_portCombo);

    m_baudCombo = new QComboBox();
    m_baudCombo->addItems(SerialManager::commonBaudRates());
    m_baudCombo->setCurrentText("115200");
    form->addRow("Baud:", m_baudCombo);

    auto *btnLayout = new QHBoxLayout();
    m_connectBtn = new QPushButton("Connect");
    m_disconnectBtn = new QPushButton("Disconnect");
    m_disconnectBtn->setEnabled(false);
    btnLayout->addWidget(m_connectBtn);
    btnLayout->addWidget(m_disconnectBtn);
    form->addRow(btnLayout);

    layout->addWidget(group);
}

void MainWindow::createStatusPanel(QVBoxLayout *layout)
{
    auto *group = new QGroupBox("Machine Status");
    auto *form = new QFormLayout(group);

    m_stateLabel = new QLabel("Disconnected");
    m_stateLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    form->addRow("State:", m_stateLabel);

    m_mposLabel = new QLabel("X:0.000 Y:0.000 Z:0.000");
    form->addRow("Machine Pos:", m_mposLabel);

    m_wposLabel = new QLabel("X:0.000 Y:0.000 Z:0.000");
    form->addRow("Work Pos:", m_wposLabel);

    m_feedLabel = new QLabel("0.0 mm/min");
    form->addRow("Feed Rate:", m_feedLabel);

    m_spindleLabel = new QLabel("0.0 RPM");
    form->addRow("Spindle:", m_spindleLabel);

    layout->addWidget(group);
}

void MainWindow::createControlPanel(QVBoxLayout *layout)
{
    auto *group = new QGroupBox("Streaming Control");
    auto *ctrlLayout = new QVBoxLayout(group);

    m_loadBtn = new QPushButton("Load G-Code File");
    ctrlLayout->addWidget(m_loadBtn);

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

    m_progressLabel = new QLabel("No file loaded");
    ctrlLayout->addWidget(m_progressLabel);

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

    connect(homeBtn, &QPushButton::clicked, this, &MainWindow::onHome);
    connect(unlockBtn, &QPushButton::clicked, this, &MainWindow::onUnlock);
    connect(resetBtn, &QPushButton::clicked, this, &MainWindow::onReset);
    connect(statusBtn, &QPushButton::clicked, this, &MainWindow::onStatusReport);

    layout->addWidget(group);
}

void MainWindow::createJogPanel(QVBoxLayout *layout)
{
    auto *group = new QGroupBox("Jog Control");
    auto *jogLayout = new QVBoxLayout(group);

    auto *modeRow = new QHBoxLayout();
    modeRow->addWidget(new QLabel("Mode:"));
    m_jogModeCombo = new QComboBox();
    m_jogModeCombo->addItems({"Continuous", "Step"});
    m_jogModeCombo->setCurrentIndex(0);
    modeRow->addWidget(m_jogModeCombo);

    modeRow->addWidget(new QLabel("Step:"));
    m_jogStepSlider = new QSlider(Qt::Horizontal);
    m_jogStepSlider->setRange(1, 100);
    m_jogStepSlider->setValue(10);
    m_jogStepLabel = new QLabel("1.0 mm");
    modeRow->addWidget(m_jogStepSlider);
    modeRow->addWidget(m_jogStepLabel);
    jogLayout->addLayout(modeRow);

    connect(m_jogStepSlider, &QSlider::valueChanged, this, [this](int val) {
        double step = val / 10.0;
        m_jogStepLabel->setText(QString("%1 mm").arg(step, 0, 'f', 1));
    });

    auto *feedRow = new QHBoxLayout();
    feedRow->addWidget(new QLabel("Feed:"));
    m_jogFeedSpin = new QDoubleSpinBox();
    m_jogFeedSpin->setRange(1, 30000);
    m_jogFeedSpin->setValue(1000);
    m_jogFeedSpin->setSuffix(" mm/min");
    m_jogFeedSpin->setDecimals(0);
    feedRow->addWidget(m_jogFeedSpin);
    jogLayout->addLayout(feedRow);

    auto *grid = new QGridLayout();
    auto *btnYPlus = new QPushButton("Y+");
    auto *btnYMinus = new QPushButton("Y-");
    auto *btnXMinus = new QPushButton("X-");
    auto *btnXPlus = new QPushButton("X+");
    auto *btnZPlus = new QPushButton("Z+");
    auto *btnZMinus = new QPushButton("Z-");

    grid->addWidget(btnYPlus, 0, 1);
    grid->addWidget(btnXMinus, 1, 0);
    grid->addWidget(btnXPlus, 1, 2);
    grid->addWidget(btnYMinus, 2, 1);
    grid->addWidget(btnZPlus, 0, 3);
    grid->addWidget(btnZMinus, 2, 3);

    jogLayout->addLayout(grid);

    auto setupJogBtn = [this](QPushButton *btn, const QString &axis, int dir) {
        connect(btn, &QPushButton::pressed, this, [this, axis, dir]() {
            if (!m_serialManager->isConnected()) return;
            if (m_jogModeCombo->currentIndex() == 0) {
                m_serialManager->jogContinuous(axis, dir, m_jogFeedSpin->value());
            } else {
                double step = m_jogStepSlider->value() / 10.0;
                double signedStep = (dir >= 0) ? step : -step;
                m_serialManager->jogStep(axis, signedStep, m_jogFeedSpin->value());
            }
        });
        connect(btn, &QPushButton::released, this, [this]() {
            if (m_jogModeCombo->currentIndex() == 0 && m_serialManager->isConnected())
                m_serialManager->jogCancel();
        });
    };

    setupJogBtn(btnXPlus, "X", 1);
    setupJogBtn(btnXMinus, "X", -1);
    setupJogBtn(btnYPlus, "Y", 1);
    setupJogBtn(btnYMinus, "Y", -1);
    setupJogBtn(btnZPlus, "Z", 1);
    setupJogBtn(btnZMinus, "Z", -1);

    layout->addWidget(group);
}

void MainWindow::createEditorPanel(QVBoxLayout *layout)
{
    auto *group = new QGroupBox("G-Code Editor");
    auto *vLayout = new QVBoxLayout(group);

    m_editorEdit = new QTextEdit();
    m_editorEdit->setFont(QFont("Consolas", 10));
    m_editorEdit->setPlaceholderText("Load a G-code file or type commands here...");
    vLayout->addWidget(m_editorEdit);

    layout->addWidget(group, 3);
}

void MainWindow::createConsolePanel(QVBoxLayout *layout)
{
    auto *group = new QGroupBox("Console");
    auto *vLayout = new QVBoxLayout(group);

    m_consoleEdit = new QTextEdit();
    m_consoleEdit->setReadOnly(true);
    m_consoleEdit->setFont(QFont("Consolas", 9));
    m_consoleEdit->setMaximumHeight(200);
    vLayout->addWidget(m_consoleEdit);

    auto *cmdLayout = new QHBoxLayout();
    auto *cmdEdit = new QLineEdit();
    cmdEdit->setPlaceholderText("Send command...");
    auto *sendBtn = new QPushButton("Send");
    cmdLayout->addWidget(cmdEdit);
    cmdLayout->addWidget(sendBtn);
    vLayout->addLayout(cmdLayout);

    connect(sendBtn, &QPushButton::clicked, this, &MainWindow::onSendCommand);
    connect(cmdEdit, &QLineEdit::returnPressed, this, &MainWindow::onSendCommand);

    m_lastCommandEdit = cmdEdit;

    layout->addWidget(group, 1);
}

void MainWindow::setupMenuBar()
{
    auto *fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("Open...", this, &MainWindow::onLoadFile, QKeySequence::Open);
    fileMenu->addSeparator();
    fileMenu->addAction("Exit", this, &QWidget::close, QKeySequence::Quit);

    auto *helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("About", this, [this]() {
        QMessageBox::about(this, "About", "GRBL G-Code Sender v1.0\nBuilt with Qt6");
    });
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage("Ready");
}

void MainWindow::setupConnections()
{
    connect(m_connectBtn, &QPushButton::clicked, this, &MainWindow::onConnect);
    connect(m_disconnectBtn, &QPushButton::clicked, this, &MainWindow::onDisconnect);
    connect(m_loadBtn, &QPushButton::clicked, this, &MainWindow::onLoadFile);
    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::onStartStreaming);
    connect(m_pauseBtn, &QPushButton::clicked, this, &MainWindow::onPauseStreaming);
    connect(m_resumeBtn, &QPushButton::clicked, this, &MainWindow::onResumeStreaming);
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::onStopStreaming);

    connect(m_serialManager, &SerialManager::responseReceived, this, &MainWindow::onResponseReceived);
    connect(m_serialManager, &SerialManager::connectedChanged, this, &MainWindow::onConnectedChanged);
    connect(m_serialManager, &SerialManager::streamingProgress, this, &MainWindow::onStreamingProgress);
    connect(m_serialManager, &SerialManager::streamingFinished, this, &MainWindow::onStreamingFinished);
    connect(m_serialManager, &SerialManager::errorOccurred, this, [this](const QString &err) {
        logToConsole("ERROR: " + err);
        statusBar()->showMessage("Error: " + err);
    });

    connect(m_machineState, &MachineState::stateChanged, this, &MainWindow::onStateUpdated);
    connect(m_machineState, &MachineState::machinePositionChanged, this, [this](const QVector3D &pos) {
        m_mposLabel->setText(QString("X:%1 Y:%2 Z:%3").arg(pos.x(), 0, 'f', 3).arg(pos.y(), 0, 'f', 3).arg(pos.z(), 0, 'f', 3));
    });
    connect(m_machineState, &MachineState::workPositionChanged, this, [this](const QVector3D &pos) {
        m_wposLabel->setText(QString("X:%1 Y:%2 Z:%3").arg(pos.x(), 0, 'f', 3).arg(pos.y(), 0, 'f', 3).arg(pos.z(), 0, 'f', 3));
    });
    connect(m_machineState, &MachineState::feedRateChanged, this, [this](double rate) {
        m_feedLabel->setText(QString("%1 mm/min").arg(rate, 0, 'f', 1));
    });
    connect(m_machineState, &MachineState::spindleSpeedChanged, this, [this](double speed) {
        m_spindleLabel->setText(QString("%1 RPM").arg(speed, 0, 'f', 1));
    });

    connect(m_statusTimer, &QTimer::timeout, this, [this]() {
        if (m_serialManager->isConnected())
            m_serialManager->sendRealtimeCommand('?');
    });

    connect(m_jogModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        m_jogMode = static_cast<JogMode>(idx);
    });

    m_statusTimer->start(250);
}

void MainWindow::onConnect()
{
    QString port = m_portCombo->currentText();
    int baud = m_baudCombo->currentText().toInt();
    m_serialManager->connectToGrbl(port, baud);
}

void MainWindow::onDisconnect()
{
    m_serialManager->disconnectFromGrbl();
}

void MainWindow::onSendCommand()
{
    if (!m_serialManager->isConnected())
        return;
    QString cmd = m_lastCommandEdit->text().trimmed();
    if (!cmd.isEmpty()) {
        m_serialManager->sendCommand(cmd);
        m_lastCommandEdit->clear();
    }
}

void MainWindow::onLoadFile()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Open G-Code File", QString(),
        "G-Code Files (*.nc *.gcode *.ngc *.txt);;All Files (*)");
    if (filePath.isEmpty())
        return;

    if (m_fileManager->loadFile(filePath)) {
        m_editorEdit->setPlainText(m_fileManager->lines().join("\n"));
        m_progressLabel->setText(QString("Loaded: %1 (%2 lines)").arg(m_fileManager->fileName()).arg(m_fileManager->lines().size()));
        m_startBtn->setEnabled(true);
        logToConsole("File loaded: " + m_fileManager->fileName());
        statusBar()->showMessage("File loaded: " + m_fileManager->fileName());
    }
}

void MainWindow::onStartStreaming()
{
    if (!m_serialManager->isConnected()) {
        QMessageBox::warning(this, "Not Connected", "Please connect to GRBL first.");
        return;
    }
    if (!m_fileManager->isLoaded()) {
        QMessageBox::warning(this, "No File", "Please load a G-code file first.");
        return;
    }

    m_startBtn->setEnabled(false);
    m_pauseBtn->setEnabled(true);
    m_stopBtn->setEnabled(true);
    m_serialManager->startStreaming(m_fileManager->lines());
}

void MainWindow::onPauseStreaming()
{
    m_serialManager->pauseStreaming();
    m_pauseBtn->setEnabled(false);
    m_resumeBtn->setEnabled(true);
}

void MainWindow::onResumeStreaming()
{
    m_serialManager->resumeStreaming();
    m_resumeBtn->setEnabled(false);
    m_pauseBtn->setEnabled(true);
}

void MainWindow::onStopStreaming()
{
    m_serialManager->stopStreaming();
    m_startBtn->setEnabled(true);
    m_pauseBtn->setEnabled(false);
    m_resumeBtn->setEnabled(false);
    m_stopBtn->setEnabled(false);
}

void MainWindow::onJogXPlus()
{
    if (!m_serialManager->isConnected()) return;
    double feed = m_jogFeedSpin->value();
    if (m_jogMode == JogMode::Continuous) {
        m_serialManager->jogContinuous("X", 1, feed);
    } else {
        double step = m_jogStepSlider->value() / 10.0;
        m_serialManager->jogStep("X", step, feed);
    }
}

void MainWindow::onJogXMinus()
{
    if (!m_serialManager->isConnected()) return;
    double feed = m_jogFeedSpin->value();
    if (m_jogMode == JogMode::Continuous) {
        m_serialManager->jogContinuous("X", -1, feed);
    } else {
        double step = m_jogStepSlider->value() / 10.0;
        m_serialManager->jogStep("X", -step, feed);
    }
}

void MainWindow::onJogYPlus()
{
    if (!m_serialManager->isConnected()) return;
    double feed = m_jogFeedSpin->value();
    if (m_jogMode == JogMode::Continuous) {
        m_serialManager->jogContinuous("Y", 1, feed);
    } else {
        double step = m_jogStepSlider->value() / 10.0;
        m_serialManager->jogStep("Y", step, feed);
    }
}

void MainWindow::onJogYMinus()
{
    if (!m_serialManager->isConnected()) return;
    double feed = m_jogFeedSpin->value();
    if (m_jogMode == JogMode::Continuous) {
        m_serialManager->jogContinuous("Y", -1, feed);
    } else {
        double step = m_jogStepSlider->value() / 10.0;
        m_serialManager->jogStep("Y", -step, feed);
    }
}

void MainWindow::onJogZPlus()
{
    if (!m_serialManager->isConnected()) return;
    double feed = m_jogFeedSpin->value();
    if (m_jogMode == JogMode::Continuous) {
        m_serialManager->jogContinuous("Z", 1, feed);
    } else {
        double step = m_jogStepSlider->value() / 10.0;
        m_serialManager->jogStep("Z", step, feed);
    }
}

void MainWindow::onJogZMinus()
{
    if (!m_serialManager->isConnected()) return;
    double feed = m_jogFeedSpin->value();
    if (m_jogMode == JogMode::Continuous) {
        m_serialManager->jogContinuous("Z", -1, feed);
    } else {
        double step = m_jogStepSlider->value() / 10.0;
        m_serialManager->jogStep("Z", -step, feed);
    }
}

void MainWindow::onJogRelease()
{
    if (m_serialManager->isConnected())
        m_serialManager->jogCancel();
}

void MainWindow::onHome()
{
    m_serialManager->sendCommand("$H");
}

void MainWindow::onUnlock()
{
    m_serialManager->sendCommand("$X");
}

void MainWindow::onReset()
{
    m_serialManager->sendRealtimeCommand(0x18);
}

void MainWindow::onStatusReport()
{
    if (m_serialManager->isConnected())
        m_serialManager->sendRealtimeCommand('?');
}

void MainWindow::onResponseReceived(const QString &response)
{
    logToConsole(response);
    m_grblParser->parseResponse(response);
}

void MainWindow::onConnectedChanged(bool connected)
{
    updateUiState();
    if (connected)
        logToConsole("Connected to GRBL");
    else
        logToConsole("Disconnected");
}

void MainWindow::onStateUpdated(MachineState::State state)
{
    static const QHash<MachineState::State, QString> stateNames = {
        {MachineState::State::Idle, "Idle"},
        {MachineState::State::Run, "Running"},
        {MachineState::State::Hold, "Hold"},
        {MachineState::State::Home, "Homing"},
        {MachineState::State::Alarm, "ALARM"},
        {MachineState::State::Jog, "Jog"},
        {MachineState::State::Door, "Door"},
        {MachineState::State::Check, "Check"},
        {MachineState::State::Sleep, "Sleep"},
        {MachineState::State::Unknown, "Unknown"},
    };

    QString name = stateNames.value(state, "Unknown");
    m_stateLabel->setText(name);

    if (state == MachineState::State::Alarm)
        m_stateLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: red;");
    else if (state == MachineState::State::Idle)
        m_stateLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: green;");
    else if (state == MachineState::State::Run)
        m_stateLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: blue;");
    else
        m_stateLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: orange;");

    statusBar()->showMessage("State: " + name);
}

void MainWindow::onStreamingProgress(int current, int total)
{
    m_progressLabel->setText(QString("Streaming: %1 / %2").arg(current).arg(total));
}

void MainWindow::onStreamingFinished()
{
    m_startBtn->setEnabled(true);
    m_pauseBtn->setEnabled(false);
    m_resumeBtn->setEnabled(false);
    m_stopBtn->setEnabled(false);
    m_progressLabel->setText("Streaming complete");
    logToConsole("Streaming finished");
    statusBar()->showMessage("Streaming complete");
}

void MainWindow::updateUiState()
{
    bool connected = m_serialManager->isConnected();
    m_connectBtn->setEnabled(!connected);
    m_disconnectBtn->setEnabled(connected);
    m_portCombo->setEnabled(!connected);
    m_baudCombo->setEnabled(!connected);
}

void MainWindow::logToConsole(const QString &msg)
{
    m_consoleEdit->append(msg);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (!m_serialManager->isConnected()) {
        QMainWindow::keyPressEvent(event);
        return;
    }

    // In continuous mode, only act on the initial key-down — not auto-repeat.
    // Auto-repeat would flood GRBL's 128-byte RX buffer with $J commands faster
    // than the jog-cancel (0x85) can purge them, causing the machine to keep
    // moving after key release.
    if (event->isAutoRepeat()) {
        event->accept();
        return;
    }

    double feed = m_jogFeedSpin->value();
    double step = m_jogStepSlider->value() / 10.0;
    bool continuous = (m_jogMode == JogMode::Continuous);

    switch (event->key()) {
    case Qt::Key_Right:
        if (continuous) m_serialManager->jogContinuous("X", 1, feed);
        else m_serialManager->jogStep("X", step, feed);
        break;
    case Qt::Key_Left:
        if (continuous) m_serialManager->jogContinuous("X", -1, feed);
        else m_serialManager->jogStep("X", -step, feed);
        break;
    case Qt::Key_Up:
        if (continuous) m_serialManager->jogContinuous("Y", 1, feed);
        else m_serialManager->jogStep("Y", step, feed);
        break;
    case Qt::Key_Down:
        if (continuous) m_serialManager->jogContinuous("Y", -1, feed);
        else m_serialManager->jogStep("Y", -step, feed);
        break;
    case Qt::Key_PageUp:
        if (continuous) m_serialManager->jogContinuous("Z", 1, feed);
        else m_serialManager->jogStep("Z", step, feed);
        break;
    case Qt::Key_PageDown:
        if (continuous) m_serialManager->jogContinuous("Z", -1, feed);
        else m_serialManager->jogStep("Z", -step, feed);
        break;
    default:
        QMainWindow::keyPressEvent(event);
        break;
    }
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (m_jogMode == JogMode::Continuous && m_serialManager->isConnected()) {
        switch (event->key()) {
        case Qt::Key_Right:
        case Qt::Key_Left:
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
            m_serialManager->jogCancel();
            break;
        default:
            break;
        }
    }
    QMainWindow::keyReleaseEvent(event);
}