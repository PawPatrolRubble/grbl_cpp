#include "MainWindow.h"
#include "DockPanel.h"
#include "ApplicationCore.h"
#include "SettingsManager.h"
#include "panels/ConnectionPanel.h"
#include "panels/StatusPanel.h"
#include "panels/ControlPanel.h"
#include "panels/JogPanel.h"
#include "panels/ConsolePanel.h"
#include "panels/SettingsPanel.h"
#include "panels/GraphicsDesignPanel.h"
#include "ToolpathPanel.h"
#include "TextToGcodeDialog.h"

#include "grbl/MachineState.h"
#include "grbl/SerialManager.h"
#include "grbl/GcodeParser.h"
#include "grbl/FileManager.h"
#include "grbl/GrblConstants.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QKeyEvent>
#include <QStatusBar>
#include <QMenuBar>
#include <QVBoxLayout>
#include <QtConcurrent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_core(new ApplicationCore(this))
    , m_settings(new SettingsManager(this))
    , m_parseWatcher(new QFutureWatcher<QList<GcodeMove>>(this))
{
    // Use default system title bar
    ui->setupUi(this);

    setupDocks();
    setupMenuBar();
    setupStatusBar();
    setupConnections();
    restoreLayout();
}

MainWindow::~MainWindow()
{
    saveLayout();
    delete ui;
}

// ---------------------------------------------------------------------------
// Menu Bar
// ---------------------------------------------------------------------------

void MainWindow::setupMenuBar()
{
    // Use QMainWindow's standard menu bar
    auto *menuBar = this->menuBar();

    // --- File menu ---
    auto *fileMenu = menuBar->addMenu("&File");
    fileMenu->addAction("Open...", this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, "Open G-Code File",
                                                     QString(), "G-Code (*.nc *.ngc *.gcode *.txt)");
        if (!path.isEmpty())
            loadAndParseFile(path);
    }, QKeySequence::Open);
    fileMenu->addSeparator();
    fileMenu->addAction("Text to G-code...", this, [this]() {
        TextToGcodeDialog dialog(this);
        connect(&dialog, &TextToGcodeDialog::gcodeGenerated, this, [this](const QStringList &lines) {
            m_core->fileManager()->loadLines("TextToGcode", lines);
            m_toolpathPanel->setGcodeLines(m_core->fileManager()->lines());
            m_controlPanel->setFileLoaded(m_core->fileManager()->fileName(), m_core->fileManager()->lines().size());
            m_consolePanel->appendMessage("Generated Text to G-code loaded.");

            QStringList parseLines = m_core->fileManager()->lines();
            QFuture<QList<GcodeMove>> future = QtConcurrent::run([parseLines]() {
                GcodeParser parser;
                return parser.parse(parseLines);
            });
            m_parseWatcher->setFuture(future);
        });
        dialog.exec();
    }, QKeySequence(Qt::CTRL | Qt::Key_T));
    fileMenu->addAction("Graphics Design", this, [this]() {
        m_dockDesign->raise();
        m_dockDesign->show();
    }, QKeySequence(Qt::CTRL | Qt::Key_G));
    fileMenu->addSeparator();
    fileMenu->addAction("Exit", this, &QWidget::close, QKeySequence::Quit);

    // --- View menu (dock visibility toggles) ---
    auto *viewMenu = menuBar->addMenu("&View");
    viewMenu->addAction("Connection", m_dockConnection, &QDockWidget::toggleViewAction);
    viewMenu->addAction("Machine Status", m_dockStatus, &QDockWidget::toggleViewAction);
    viewMenu->addAction("Streaming Control", m_dockControl, &QDockWidget::toggleViewAction);
    viewMenu->addAction("Jog Control", m_dockJog, &QDockWidget::toggleViewAction);
    viewMenu->addAction("Settings", m_dockSettings, &QDockWidget::toggleViewAction);
    viewMenu->addSeparator();
    viewMenu->addAction("Toolpath", m_dockToolpath, &QDockWidget::toggleViewAction);
    viewMenu->addAction("Console", m_dockConsole, &QDockWidget::toggleViewAction);
    viewMenu->addAction("Design", m_dockDesign, &QDockWidget::toggleViewAction);
    viewMenu->addSeparator();
    viewMenu->addAction("Reset Layout", this, [this]() {
        m_settings->resetToDefaults();
        for (auto *dock : {m_dockConnection, m_dockStatus, m_dockControl, m_dockJog, m_dockSettings,
                           m_dockToolpath, m_dockConsole, m_dockDesign}) {
            removeDockWidget(dock);
        }
        addDockWidget(Qt::LeftDockWidgetArea, m_dockConnection);
        addDockWidget(Qt::LeftDockWidgetArea, m_dockStatus);
        addDockWidget(Qt::LeftDockWidgetArea, m_dockControl);
        addDockWidget(Qt::LeftDockWidgetArea, m_dockJog);
        addDockWidget(Qt::RightDockWidgetArea, m_dockSettings);
        addDockWidget(Qt::RightDockWidgetArea, m_dockToolpath);
        addDockWidget(Qt::RightDockWidgetArea, m_dockDesign);
        addDockWidget(Qt::BottomDockWidgetArea, m_dockConsole);
        tabifyDockWidget(m_dockToolpath, m_dockDesign);
        tabifyDockWidget(m_dockConnection, m_dockStatus);
        tabifyDockWidget(m_dockStatus, m_dockControl);
        tabifyDockWidget(m_dockControl, m_dockJog);
        m_dockSettings->hide();
        m_dockConnection->raise();
        m_dockToolpath->raise();
        for (auto *dock : {m_dockConnection, m_dockStatus, m_dockControl, m_dockJog,
                           m_dockToolpath, m_dockConsole, m_dockDesign}) {
            dock->show();
        }
    });

    // --- Help menu ---
    auto *helpMenu = menuBar->addMenu("&Help");
    helpMenu->addAction("About", this, [this]() {
        QMessageBox::about(this, "About", "GRBL G-Code Sender v1.0\nBuilt with Qt6");
    });
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage("Ready");
}

// ---------------------------------------------------------------------------
// Dock Widgets
// ---------------------------------------------------------------------------

void MainWindow::setupDocks()
{
    setDockNestingEnabled(true);

    // Connection panel (left)
    m_connectionPanel = new ConnectionPanel();
    m_connectionPanel->setAvailablePorts(SerialManager::availablePorts());
    m_connectionPanel->setAvailableBaudRates(SerialManager::commonBaudRates());
    m_connectionPanel->setDefaultBaudRate("115200");
    m_dockConnection = createDock("Connection", m_connectionPanel, Qt::LeftDockWidgetArea);

    // Status panel (left)
    m_statusPanel = new StatusPanel();
    m_dockStatus = createDock("Machine Status", m_statusPanel, Qt::LeftDockWidgetArea);

    // Control panel (left)
    m_controlPanel = new ControlPanel();
    m_dockControl = createDock("Streaming Control", m_controlPanel, Qt::LeftDockWidgetArea);

    // Jog panel (left)
    m_jogPanel = new JogPanel();
    m_dockJog = createDock("Jog Control", m_jogPanel, Qt::LeftDockWidgetArea);

    // Settings panel (right, hidden by default)
    m_settingsPanel = new SettingsPanel();
    m_dockSettings = createDock("Settings", m_settingsPanel, Qt::RightDockWidgetArea);
    m_dockSettings->hide();

    // Tabify left docks
    tabifyDockWidget(m_dockConnection, m_dockStatus);
    tabifyDockWidget(m_dockStatus, m_dockControl);
    tabifyDockWidget(m_dockControl, m_dockJog);
    m_dockConnection->raise();

    // --- Central panels as dockable widgets ---

    // Toolpath panel
    m_toolpathPanel = new ToolpathPanel();
    m_dockToolpath = createDock("Toolpath", m_toolpathPanel, Qt::RightDockWidgetArea, false);

    // Console panel
    m_consolePanel = new ConsolePanel();
    m_consolePanel->setMinimumHeight(120);
    m_consolePanel->setMaximumHeight(250);
    m_dockConsole = createDock("Console", m_consolePanel, Qt::BottomDockWidgetArea, false);

    // Graphics Design panel
    m_graphicsDesignPanel = new GraphicsDesignPanel();
    m_dockDesign = createDock("Design", m_graphicsDesignPanel, Qt::RightDockWidgetArea, false);

    // Tabify central panels so they appear as tabs
    tabifyDockWidget(m_dockToolpath, m_dockDesign);
    m_dockToolpath->raise();
}

DockPanel *MainWindow::createDock(const QString &title, QWidget *widget,
                                   Qt::DockWidgetArea area, bool closable)
{
    auto *dock = new DockPanel(title, widget, area, closable, true, this);
    addDockWidget(area, dock);
    return dock;
}

// ---------------------------------------------------------------------------
// Layout persistence
// ---------------------------------------------------------------------------

void MainWindow::saveLayout()
{
    QByteArray geometry = saveGeometry();
    QByteArray dockState = saveState();
    m_settings->saveLayout(dockState, geometry);
}

void MainWindow::restoreLayout()
{
    QByteArray geometry;
    QByteArray state = m_settings->restoreLayout(geometry);
    if (!state.isEmpty())
        restoreState(state);
    if (!geometry.isEmpty())
        restoreGeometry(geometry);
}

// ---------------------------------------------------------------------------
// Signal connections
// ---------------------------------------------------------------------------

void MainWindow::setupConnections()
{
    auto *serial = m_core->serialManager();
    auto *state = m_core->machineState();

    // --- ConnectionPanel 鈫?SerialManager ---
    connect(m_connectionPanel, &ConnectionPanel::connectRequested,
            this, [this](const QString &port, int baud) {
                m_core->serialManager()->connectToGrbl(port, baud);
            });
    connect(m_connectionPanel, &ConnectionPanel::disconnectRequested,
            serial, &SerialManager::disconnectFromGrbl);

    // --- SerialManager 鈫?panels ---
    connect(serial, &SerialManager::connectedChanged,
            m_connectionPanel, &ConnectionPanel::setConnected);
    connect(serial, &SerialManager::connectedChanged,
            this, [this](bool connected) {
                m_consolePanel->appendMessage(connected ? "Connected to GRBL" : "Disconnected");
            });

    // --- Core responses 鈫?console (filter out status reports)
    connect(m_core, &ApplicationCore::responseReceived,
            this, [this](const QString &response) {
                // Skip GRBL status reports: <Idle|WPos:...>
                if (response.startsWith('<') && response.endsWith('>'))
                    return;
                // During streaming, batch responses to avoid UI lag
                m_consolePanel->queueMessage(response);
            });

    // --- Core streaming 鈫?UI ---
    connect(m_core, &ApplicationCore::streamingProgress,
            m_controlPanel, &ControlPanel::setStreamingProgress);
    connect(m_core, &ApplicationCore::streamingProgress,
            this, [this](int current, int /*total*/) {
                m_toolpathPanel->setExecutingLine(current);
            });
    connect(m_core, &ApplicationCore::streamingStarted,
            m_controlPanel, &ControlPanel::setStreamingStarted);
    connect(m_core, &ApplicationCore::streamingStarted,
            this, [this]() {
                m_core->serialManager()->setStatusPollInterval(500);
            });
    connect(m_core, &ApplicationCore::streamingPaused,
            m_controlPanel, &ControlPanel::setStreamingPaused);
    connect(m_core, &ApplicationCore::streamingResumed,
            m_controlPanel, &ControlPanel::setStreamingResumed);
    connect(m_core, &ApplicationCore::streamingFinished,
            this, [this]() {
                m_controlPanel->setStreamingFinished();
                m_core->serialManager()->setStatusPollInterval(250);
                m_consolePanel->appendMessage("Streaming finished");
                statusBar()->showMessage("Streaming complete");
            });

    // --- MachineState 鈫?StatusPanel + ToolpathPanel ---
    connect(state, &MachineState::machinePositionChanged,
            m_statusPanel, &StatusPanel::setMachinePosition);
    connect(state, &MachineState::workPositionChanged,
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
    connect(state, &MachineState::stateChanged,
            this, [this](MachineState::State s) {
                bool canMove = (s == MachineState::State::Idle || s == MachineState::State::Jog);
                m_statusPanel->setMotionEnabled(canMove);
                m_jogPanel->setMotionEnabled(canMove);
                // Clear alarm message when state leaves Alarm
                if (s != MachineState::State::Alarm)
                    m_statusPanel->setMessage("");
            });
    connect(state, &MachineState::messageReceived,
            m_statusPanel, &StatusPanel::setMessage);
    connect(state, &MachineState::feedRateChanged,
            m_statusPanel, &StatusPanel::setFeedRate);
    connect(state, &MachineState::spindleSpeedChanged,
            m_statusPanel, &StatusPanel::setSpindleSpeed);

    // --- StatusPanel 鈫?SerialManager ---
    connect(m_statusPanel, &StatusPanel::zeroAxisRequested,
            this, [this](const QString &axis) {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->serialManager()->sendCommand(QString("G10 L20 P0 %1%2").arg(axis).arg(0));
                m_core->serialManager()->sendCommand("$#");
                m_consolePanel->appendMessage(QString("Set %1 work position to zero").arg(axis));
            });
    connect(m_statusPanel, &StatusPanel::setAxisPositionRequested,
            this, [this](const QString &axis, double value) {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->serialManager()->sendCommand(
                    QString("G10 L20 P0 %1%2").arg(axis).arg(value, 0, 'f', 3));
                m_core->serialManager()->sendCommand("$#");
                m_consolePanel->appendMessage(
                    QString("Set %1 work position to %2").arg(axis).arg(value, 0, 'f', 3));
            });
    connect(m_statusPanel, &StatusPanel::zeroAllRequested,
            this, [this]() {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->serialManager()->sendCommand("G10 L20 P0 X0 Y0 Z0");
                m_core->serialManager()->sendCommand("$#");
                m_consolePanel->appendMessage("Set all work positions to zero");
            });
    connect(m_statusPanel, &StatusPanel::goToZeroRequested,
            this, [this]() {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->serialManager()->sendCommand("G90 G0 X0 Y0 Z0");
                m_consolePanel->appendMessage("Moving to zero position");
            });
    connect(m_statusPanel, &StatusPanel::resetWorkOffsetRequested,
            this, [this]() {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->serialManager()->sendCommand("G10 L2 P1 X0 Y0 Z0");
                m_core->serialManager()->sendCommand("$#");
                m_consolePanel->appendMessage("Reset work offset 鈥?work position now equals machine position");
            });
    connect(m_statusPanel, &StatusPanel::emergencyStopRequested,
            this, [this]() {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->stopStreaming();
                m_core->serialManager()->clearQueue();
                m_core->sendRealtime(GrblRealtime::Reset);
                m_consolePanel->appendMessage("EMERGENCY STOP 鈥?soft reset sent");
            });
    connect(m_statusPanel, &StatusPanel::homeMachineRequested,
            this, [this]() {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->serialManager()->sendCommand("$H");
                m_consolePanel->appendMessage("Homing cycle started");
            });

    // --- ControlPanel 鈫?Core ---
    connect(m_controlPanel, &ControlPanel::loadFileRequested,
            this, [this]() {
                QString path = QFileDialog::getOpenFileName(this, "Open G-Code File",
                                                             QString(), "G-Code (*.nc *.ngc *.gcode *.txt)");
                if (!path.isEmpty())
                    loadAndParseFile(path);
            });
    connect(m_controlPanel, &ControlPanel::startRequested,
            this, [this]() {
                QStringList lines = m_toolpathPanel->currentGcodeLines();
                if (lines.isEmpty())
                    lines = m_core->fileManager()->lines();
                if (m_toolpathPanel->returnToOriginEnabled())
                    lines.append("G90 G0 X0 Y0 Z0");
                m_core->startStreaming(lines);
            });
    connect(m_controlPanel, &ControlPanel::pauseRequested,
            m_core, &ApplicationCore::pauseStreaming);
    connect(m_controlPanel, &ControlPanel::resumeRequested,
            m_core, &ApplicationCore::resumeStreaming);
    connect(m_controlPanel, &ControlPanel::stopRequested,
            m_core, &ApplicationCore::stopStreaming);
    connect(m_controlPanel, &ControlPanel::homeRequested,
            this, [this]() {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->serialManager()->sendCommand("$H");
                m_consolePanel->appendMessage("Homing...");
            });
    connect(m_controlPanel, &ControlPanel::unlockRequested,
            this, [this]() {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->serialManager()->sendCommand("$X");
                m_core->serialManager()->sendCommand("$G");
                m_core->serialManager()->sendCommand("$#");
                m_consolePanel->appendMessage("Alarm unlocked");
            });
    connect(m_controlPanel, &ControlPanel::resetRequested,
            this, [this]() {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->serialManager()->sendRealtimeCommand(0x18);
                m_consolePanel->appendMessage("Soft reset sent");
            });
    connect(m_controlPanel, &ControlPanel::statusRequested,
            this, [this]() {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->serialManager()->sendRealtimeCommand('?');
            });

    // --- JogPanel 鈫?SerialManager ---
    connect(m_jogPanel, &JogPanel::jogContinuousRequested,
            this, [this](const QString &axis, int direction, double feed) {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->serialManager()->jogContinuous(axis, direction, feed);
            });
    connect(m_jogPanel, &JogPanel::jogStepRequested,
            this, [this](const QString &axis, double distance, double feed) {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->serialManager()->jogStep(axis, distance, feed);
            });
    connect(m_jogPanel, &JogPanel::jogCancelRequested,
            this, [this]() {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->serialManager()->jogCancel();
            });
    connect(m_jogPanel, &JogPanel::goToAbsoluteRequested,
            this, [this](double x, double y, double z, double feed) {
                if (!m_core->serialManager()->isConnected()) return;
                QString cmd = QString("G90 G0 X%1 Y%2 Z%3 F%4")
                    .arg(x, 0, 'f', 3)
                    .arg(y, 0, 'f', 3)
                    .arg(z, 0, 'f', 3)
                    .arg(feed, 0, 'f', 0);
                m_core->serialManager()->sendCommand(cmd);
                m_consolePanel->appendMessage(QString("Go to absolute: X%1 Y%2 Z%3 F%4")
                    .arg(x, 0, 'f', 3).arg(y, 0, 'f', 3).arg(z, 0, 'f', 3).arg(feed, 0, 'f', 0));
            });

    // --- ConsolePanel 鈫?SerialManager ---
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

    // --- ToolpathPanel clear ---
    connect(m_toolpathPanel, &ToolpathPanel::clearRequested,
            this, [this]() {
                m_toolpathPanel->clearAll();
                m_core->fileManager()->clear();
                m_controlPanel->setFileLoaded("", 0);
                m_consolePanel->appendMessage("G-code cleared");
                statusBar()->showMessage("G-code cleared");
            });

    // --- ToolpathPanel transform 鈫?keep FileManager in sync ---
    connect(m_toolpathPanel, &ToolpathPanel::gcodeTransformed,
            this, [this](const QStringList &lines) {
                m_core->fileManager()->loadLines(m_core->fileManager()->fileName(), lines);
                m_controlPanel->setFileLoaded(m_core->fileManager()->fileName(), lines.size());
            });

    // --- GraphicsDesignPanel (tab) 鈫?G-code generation ---
    connect(m_graphicsDesignPanel, &GraphicsDesignPanel::gcodeGenerated,
            this, [this](const QStringList &lines) {
                m_core->fileManager()->loadLines("GraphicsDesign", lines);
                m_toolpathPanel->setGcodeLines(m_core->fileManager()->lines());
                m_controlPanel->setFileLoaded(m_core->fileManager()->fileName(), m_core->fileManager()->lines().size());
                m_consolePanel->appendMessage("Generated Graphics Design G-code loaded.");
                statusBar()->showMessage("Graphics Design G-code loaded");

                // Switch to toolpath dock to show result
                m_dockToolpath->raise();
                m_dockToolpath->show();

                QStringList parseLines = m_core->fileManager()->lines();
                QFuture<QList<GcodeMove>> future = QtConcurrent::run([parseLines]() {
                    GcodeParser parser;
                    return parser.parse(parseLines);
                });
                m_parseWatcher->setFuture(future);
            });
}

// ---------------------------------------------------------------------------
// Async G-code parsing
// ---------------------------------------------------------------------------

void MainWindow::loadAndParseFile(const QString &filePath)
{
    auto *fm = m_core->fileManager();
    if (fm->loadFile(filePath)) {
        m_toolpathPanel->setGcodeLines(fm->lines());
        m_controlPanel->setFileLoaded(fm->fileName(), fm->lines().size());
        m_consolePanel->appendMessage("File loaded: " + fm->fileName());
        statusBar()->showMessage("Parsing G-code...");

        QStringList lines = fm->lines();
        QFuture<QList<GcodeMove>> future = QtConcurrent::run([lines]() {
            GcodeParser parser;
            return parser.parse(lines);
        });
        m_parseWatcher->setFuture(future);
    } else {
        QMessageBox::warning(this, "Error", "Could not load file: " + filePath);
    }
}

// ---------------------------------------------------------------------------
// Keyboard handling for jog
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

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        ui->retranslateUi(this);
    QMainWindow::changeEvent(event);
}
