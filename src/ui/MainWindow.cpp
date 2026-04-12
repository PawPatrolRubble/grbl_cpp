#include "MainWindow.h"
#include "panels/ConnectionPanel.h"
#include "panels/StatusPanel.h"
#include "panels/ControlPanel.h"
#include "panels/JogPanel.h"
#include "panels/ConsolePanel.h"
#include "ToolpathPanel.h"
#include "TextToGcodeDialog.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QKeyEvent>
#include <QMenuBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QtConcurrent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_core(new ApplicationCore(this))
    , m_parseWatcher(new QFutureWatcher<QList<GcodeMove>>(this))
{
    setWindowTitle("GRBL G-Code Sender");
    resize(1200, 800);
    setupUi();
    setupMenuBar();
    setupStatusBar();
    setupConnections();
}

MainWindow::~MainWindow()
{
    // ApplicationCore will clean up all services automatically (parent-child ownership)
}

// ---------------------------------------------------------------------------
// UI Layout
// ---------------------------------------------------------------------------

void MainWindow::setupUi()
{
    auto *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // Left column
    m_connectionPanel = new ConnectionPanel();
    m_connectionPanel->setAvailablePorts(SerialManager::availablePorts());
    m_connectionPanel->setAvailableBaudRates(SerialManager::commonBaudRates());
    m_connectionPanel->setDefaultBaudRate("115200");

    m_statusPanel = new StatusPanel();
    m_controlPanel = new ControlPanel();
    m_jogPanel = new JogPanel();

    auto *leftLayout = new QVBoxLayout();
    leftLayout->addWidget(m_connectionPanel);
    leftLayout->addWidget(m_statusPanel);
    leftLayout->addWidget(m_controlPanel);
    leftLayout->addWidget(m_jogPanel);

    auto *leftWidget = new QWidget();
    leftWidget->setLayout(leftLayout);
    leftWidget->setMaximumWidth(360);

    // Right column
    m_toolpathPanel = new ToolpathPanel();
    m_consolePanel = new ConsolePanel();

    auto *rightLayout = new QVBoxLayout();
    rightLayout->addWidget(m_toolpathPanel, 3);
    rightLayout->addWidget(m_consolePanel, 1);

    auto *rightWidget = new QWidget();
    rightWidget->setLayout(rightLayout);

    auto *splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(leftWidget);
    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);

    auto *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->addWidget(splitter);
}

void MainWindow::setupMenuBar()
{
    auto *fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("Open...", this, [this]() {
        emit m_controlPanel->loadFileRequested();
    }, QKeySequence::Open);
    fileMenu->addSeparator();
    fileMenu->addAction("Text to G-code...", this, [this]() {
        TextToGcodeDialog dialog(this);
        connect(&dialog, &TextToGcodeDialog::gcodeGenerated, this, [this](const QStringList &lines) {
            m_core->fileManager()->loadLines("TextToGcode", lines);
            m_toolpathPanel->setGcodeLines(m_core->fileManager()->lines());
            m_controlPanel->setFileLoaded(m_core->fileManager()->fileName(), m_core->fileManager()->lines().size());
            m_consolePanel->appendMessage("Generated Text to G-code loaded.");

            // Parse in background using stateless GcodeParser
            QStringList parseLines = m_core->fileManager()->lines();
            QFuture<QList<GcodeMove>> future = QtConcurrent::run([parseLines]() {
                GcodeParser parser;
                return parser.parse(parseLines);
            });
            m_parseWatcher->setFuture(future);
        });
        dialog.exec();
    }, QKeySequence(Qt::CTRL | Qt::Key_T));
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

// ---------------------------------------------------------------------------
// Signal / Slot Wiring — UI panels ↔ ApplicationCore
// ---------------------------------------------------------------------------

void MainWindow::setupConnections()
{
    // --- ConnectionPanel → SerialManager (via core) ---
    connect(m_connectionPanel, &ConnectionPanel::connectRequested,
            this, [this](const QString &port, int baud) {
                m_core->serialManager()->connectToGrbl(port, baud);
            });
    connect(m_connectionPanel, &ConnectionPanel::disconnectRequested,
            m_core->serialManager(), &SerialManager::disconnectFromGrbl);

    // --- SerialManager → panels ---
    connect(m_core->serialManager(), &SerialManager::connectedChanged,
            m_connectionPanel, &ConnectionPanel::setConnected);
    connect(m_core->serialManager(), &SerialManager::connectedChanged,
            this, [this](bool connected) {
                m_consolePanel->appendMessage(connected ? "Connected to GRBL" : "Disconnected");
            });

    // --- Core responses → console + GrblParser (already wired in ApplicationCore) ---
    connect(m_core, &ApplicationCore::responseReceived,
            this, [this](const QString &response) {
                m_consolePanel->appendMessage(response);
            });

    // --- Core streaming → UI ---
    connect(m_core, &ApplicationCore::streamingProgress,
            m_controlPanel, &ControlPanel::setStreamingProgress);
    connect(m_core, &ApplicationCore::streamingProgress,
            this, [this](int current, int /*total*/) {
                m_toolpathPanel->setExecutingLine(current);
            });
    connect(m_core, &ApplicationCore::streamingStarted,
            m_controlPanel, &ControlPanel::setStreamingStarted);
    connect(m_core, &ApplicationCore::streamingPaused,
            m_controlPanel, &ControlPanel::setStreamingPaused);
    connect(m_core, &ApplicationCore::streamingResumed,
            m_controlPanel, &ControlPanel::setStreamingResumed);
    connect(m_core, &ApplicationCore::streamingFinished,
            this, [this]() {
                m_controlPanel->setStreamingFinished();
                m_consolePanel->appendMessage("Streaming finished");
                statusBar()->showMessage("Streaming complete");
            });

    // --- MachineState → StatusPanel + ToolpathPanel ---
    auto *state = m_core->machineState();
    connect(state, &MachineState::machinePositionChanged,
            m_statusPanel, &StatusPanel::setMachinePosition);
    connect(state, &MachineState::machinePositionChanged,
            m_toolpathPanel, &ToolpathPanel::setCutterPosition);
    connect(state, &MachineState::workPositionChanged,
            m_statusPanel, &StatusPanel::setWorkPosition);
    connect(state, &MachineState::stateChanged,
            m_statusPanel, &StatusPanel::setState);
    connect(state, &MachineState::stateChanged,
            this, [this](MachineState::State s) {
                static const QHash<MachineState::State, QString> names = {
                    {MachineState::State::Idle, "Idle"}, {MachineState::State::Run, "Running"},
                    {MachineState::State::Hold, "Hold"}, {MachineState::State::Home, "Homing"},
                    {MachineState::State::Alarm, "ALARM"}, {MachineState::State::Jog, "Jog"},
                    {MachineState::State::Door, "Door"}, {MachineState::State::Check, "Check"},
                    {MachineState::State::Sleep, "Sleep"}, {MachineState::State::Unknown, "Unknown"},
                };
                statusBar()->showMessage("State: " + names.value(s, "Unknown"));
            });
    connect(state, &MachineState::feedRateChanged,
            m_statusPanel, &StatusPanel::setFeedRate);
    connect(state, &MachineState::spindleSpeedChanged,
            m_statusPanel, &StatusPanel::setSpindleSpeed);

    // --- StatusPanel → SerialManager ---
    connect(m_statusPanel, &StatusPanel::zeroAxisRequested,
            this, [this](const QString &axis) {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->serialManager()->sendCommand(QString("G10 L20 P0 %1%2").arg(axis).arg(0));
                m_consolePanel->appendMessage(QString("Set %1 work position to zero").arg(axis));
            });
    connect(m_statusPanel, &StatusPanel::zeroAllRequested,
            this, [this]() {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->serialManager()->sendCommand("G10 L20 P0 X0 Y0 Z0");
                m_consolePanel->appendMessage("Set all work positions to zero");
            });
    connect(m_statusPanel, &StatusPanel::goToZeroRequested,
            this, [this]() {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->serialManager()->sendCommand("G90 G0 X0 Y0 Z0");
                m_consolePanel->appendMessage("Moving to zero position");
            });

    // --- ToolpathPanel → transform feedback ---
    connect(m_toolpathPanel, &ToolpathPanel::gcodeTransformed,
            this, [this](const QStringList &newLines) {
                m_controlPanel->setFileLoaded(m_core->fileManager()->fileName(), newLines.size());
                m_consolePanel->appendMessage(QString("Transform applied — %1 lines").arg(newLines.size()));
            });

    // --- Core errors → console + statusbar ---
    connect(m_core, &ApplicationCore::errorOccurred,
            this, [this](const QString &err) {
                m_consolePanel->appendMessage("ERROR: " + err);
                statusBar()->showMessage("Error: " + err);
            });

    // --- ControlPanel → file loading / streaming (via core facade) ---
    connect(m_controlPanel, &ControlPanel::loadFileRequested, this, [this]() {
        QString filePath = QFileDialog::getOpenFileName(this, "Open G-Code File", QString(),
            "G-Code Files (*.nc *.gcode *.ngc *.txt);;All Files (*)");
        if (filePath.isEmpty()) return;
        loadAndParseFile(filePath);
    });
    connect(m_controlPanel, &ControlPanel::startRequested, this, [this]() {
        if (!m_core->serialManager()->isConnected()) {
            QMessageBox::warning(this, "Not Connected", "Please connect to GRBL first.");
            return;
        }
        if (!m_core->fileManager()->isLoaded()) {
            QMessageBox::warning(this, "No File", "Please load a G-code file first.");
            return;
        }
        m_core->startStreaming(m_toolpathPanel->currentGcodeLines());
    });
    connect(m_controlPanel, &ControlPanel::pauseRequested,  this, [this]() { m_core->pauseStreaming(); });
    connect(m_controlPanel, &ControlPanel::resumeRequested, this, [this]() { m_core->resumeStreaming(); });
    connect(m_controlPanel, &ControlPanel::stopRequested,   this, [this]() { m_core->stopStreaming(); });

    // --- ControlPanel → quick commands (via core facade) ---
    connect(m_controlPanel, &ControlPanel::homeRequested,
            this, [this]() { m_core->sendCommand("$H"); });
    connect(m_controlPanel, &ControlPanel::unlockRequested,
            this, [this]() { m_core->sendCommand("$X"); });
    connect(m_controlPanel, &ControlPanel::resetRequested,
            this, [this]() { m_core->sendRealtime(0x18); });
    connect(m_controlPanel, &ControlPanel::statusRequested,
            this, [this]() {
                if (m_core->serialManager()->isConnected())
                    m_core->sendRealtime('?');
            });

    // --- JogPanel → SerialManager ---
    connect(m_jogPanel, &JogPanel::jogContinuousRequested,
            this, [this](const QString &axis, int dir, double feed) {
                if (m_core->serialManager()->isConnected())
                    m_core->serialManager()->jogContinuous(axis, dir, feed);
            });
    connect(m_jogPanel, &JogPanel::jogStepRequested,
            this, [this](const QString &axis, double dist, double feed) {
                if (m_core->serialManager()->isConnected())
                    m_core->serialManager()->jogStep(axis, dist, feed);
            });
    connect(m_jogPanel, &JogPanel::jogCancelRequested,
            this, [this]() {
                if (m_core->serialManager()->isConnected())
                    m_core->serialManager()->jogCancel();
            });

    // --- ConsolePanel → SerialManager (via core facade) ---
    connect(m_consolePanel, &ConsolePanel::commandEntered,
            this, [this](const QString &cmd) {
                if (m_core->serialManager()->isConnected())
                    m_core->sendCommand(cmd);
            });

    // --- Async parse ---
    connect(m_parseWatcher, &QFutureWatcher<QList<GcodeMove>>::finished,
            this, [this]() {
                m_toolpathPanel->setMoves(m_parseWatcher->result());
                statusBar()->showMessage("File loaded: " + m_core->fileManager()->fileName());
            });
}

// ---------------------------------------------------------------------------
// Async G-code parsing
// ---------------------------------------------------------------------------

void MainWindow::loadAndParseFile(const QString &filePath)
{
    if (m_core->fileManager()->loadFile(filePath)) {
        m_toolpathPanel->setGcodeLines(m_core->fileManager()->lines());
        m_controlPanel->setFileLoaded(m_core->fileManager()->fileName(), m_core->fileManager()->lines().size());
        m_consolePanel->appendMessage("File loaded: " + m_core->fileManager()->fileName());
        statusBar()->showMessage("Parsing G-code...");

        QStringList lines = m_core->fileManager()->lines();
        QFuture<QList<GcodeMove>> future = QtConcurrent::run([lines]() {
            GcodeParser parser;
            return parser.parse(lines);
        });
        m_parseWatcher->setFuture(future);
    }
}

// ---------------------------------------------------------------------------
// Keyboard Jog (delegated to JogPanel)
// ---------------------------------------------------------------------------

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (m_core->serialManager()->isConnected() && m_jogPanel->handleKeyPress(event))
        return;
    QMainWindow::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (m_core->serialManager()->isConnected() && m_jogPanel->handleKeyRelease(event))
        return;
    QMainWindow::keyReleaseEvent(event);
}
