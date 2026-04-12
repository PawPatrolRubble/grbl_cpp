#include "MainWindow.h"
#include "DockPanel.h"
#include "panels/ConnectionPanel.h"
#include "panels/StatusPanel.h"
#include "panels/ControlPanel.h"
#include "panels/JogPanel.h"
#include "panels/ConsolePanel.h"
#include "panels/SettingsPanel.h"
#include "ToolpathPanel.h"
#include "TextToGcodeDialog.h"

#include "grbl/MachineState.h"
#include "grbl/SerialManager.h"
#include "grbl/GcodeParser.h"
#include "grbl/FileManager.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QKeyEvent>
#include <QMenuBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QtConcurrent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_core(new ApplicationCore(this))
    , m_settings(new SettingsManager(this))
    , m_parseWatcher(new QFutureWatcher<QList<GcodeMove>>(this))
{
    setWindowTitle("GRBL G-Code Sender");
    resize(1200, 800);
    setupUi();
    setupMenuBar();
    setupStatusBar();
    setupConnections();
    restoreLayout();
}

MainWindow::~MainWindow()
{
    saveLayout();
}

// ---------------------------------------------------------------------------
// UI Layout — Dock-based
// ---------------------------------------------------------------------------

void MainWindow::setupUi()
{
    // Central widget: ToolpathPanel (always visible, not dockable)
    m_toolpathPanel = new ToolpathPanel();
    setCentralWidget(m_toolpathPanel);

    // --- Left side docks (tabified) ---

    m_connectionPanel = new ConnectionPanel();
    m_connectionPanel->setAvailablePorts(SerialManager::availablePorts());
    m_connectionPanel->setAvailableBaudRates(SerialManager::commonBaudRates());
    m_connectionPanel->setDefaultBaudRate("115200");

    m_dockConnection = createDock("Connection", m_connectionPanel, Qt::LeftDockWidgetArea);
    m_dockConnection->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    m_statusPanel = new StatusPanel();
    m_dockStatus = createDock("Machine Status", m_statusPanel, Qt::LeftDockWidgetArea);

    m_controlPanel = new ControlPanel();
    m_dockControl = createDock("Streaming Control", m_controlPanel, Qt::LeftDockWidgetArea);

    m_jogPanel = new JogPanel();
    m_dockJog = createDock("Jog Control", m_jogPanel, Qt::LeftDockWidgetArea);

    // --- Bottom dock ---

    m_consolePanel = new ConsolePanel();
    m_dockConsole = createDock("Console", m_consolePanel, Qt::BottomDockWidgetArea);
    m_dockConsole->setMinimumHeight(150);

    // --- Right dock (hidden by default) ---

    m_settingsPanel = new SettingsPanel();
    m_dockSettings = createDock("Settings", m_settingsPanel, Qt::RightDockWidgetArea);
    m_dockSettings->hide();  // Hidden by default, toggle via menu
}

DockPanel *MainWindow::createDock(const QString &title, QWidget *widget,
                                   Qt::DockWidgetArea area, bool closable)
{
    auto *dock = new DockPanel(title, widget, area, closable, true, this);
    addDockWidget(area, dock);
    return dock;
}

void MainWindow::setupMenuBar()
{
    // --- File menu ---
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

    // --- View menu (dock visibility toggles) ---
    auto *viewMenu = menuBar()->addMenu("&View");

    auto *actConnection = viewMenu->addAction("Connection", this, [this]() {
        m_dockConnection->setVisible(!m_dockConnection->isVisible());
    });
    actConnection->setCheckable(true);
    connect(m_dockConnection, &QDockWidget::visibilityChanged,
            actConnection, &QAction::setChecked);
    actConnection->setChecked(true);

    auto *actStatus = viewMenu->addAction("Machine Status", this, [this]() {
        m_dockStatus->setVisible(!m_dockStatus->isVisible());
    });
    actStatus->setCheckable(true);
    connect(m_dockStatus, &QDockWidget::visibilityChanged,
            actStatus, &QAction::setChecked);
    actStatus->setChecked(true);

    auto *actControl = viewMenu->addAction("Streaming Control", this, [this]() {
        m_dockControl->setVisible(!m_dockControl->isVisible());
    });
    actControl->setCheckable(true);
    connect(m_dockControl, &QDockWidget::visibilityChanged,
            actControl, &QAction::setChecked);
    actControl->setChecked(true);

    auto *actJog = viewMenu->addAction("Jog Control", this, [this]() {
        m_dockJog->setVisible(!m_dockJog->isVisible());
    });
    actJog->setCheckable(true);
    connect(m_dockJog, &QDockWidget::visibilityChanged,
            actJog, &QAction::setChecked);
    actJog->setChecked(true);

    auto *actConsole = viewMenu->addAction("Console", this, [this]() {
        m_dockConsole->setVisible(!m_dockConsole->isVisible());
    });
    actConsole->setCheckable(true);
    connect(m_dockConsole, &QDockWidget::visibilityChanged,
            actConsole, &QAction::setChecked);
    actConsole->setChecked(true);

    auto *actSettings = viewMenu->addAction("Settings", this, [this]() {
        m_dockSettings->setVisible(!m_dockSettings->isVisible());
    });
    actSettings->setCheckable(true);
    connect(m_dockSettings, &QDockWidget::visibilityChanged,
            actSettings, &QAction::setChecked);

    viewMenu->addSeparator();
    viewMenu->addAction("Reset Layout", this, [this]() {
        // Restore default: remove all saved dock state
        m_settings->resetToDefaults();
        // Re-add docks in default positions
        for (auto *dock : {m_dockConnection, m_dockStatus, m_dockControl, m_dockJog, m_dockConsole, m_dockSettings}) {
            removeDockWidget(dock);
        }
        addDockWidget(Qt::LeftDockWidgetArea, m_dockConnection);
        addDockWidget(Qt::LeftDockWidgetArea, m_dockStatus);
        addDockWidget(Qt::LeftDockWidgetArea, m_dockControl);
        addDockWidget(Qt::LeftDockWidgetArea, m_dockJog);
        addDockWidget(Qt::BottomDockWidgetArea, m_dockConsole);
        addDockWidget(Qt::RightDockWidgetArea, m_dockSettings);
        m_dockSettings->hide();
    });

    // --- Help menu ---
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
// Layout persistence
// ---------------------------------------------------------------------------

void MainWindow::saveLayout()
{
    m_settings->saveLayout(saveState(), saveGeometry());
}

void MainWindow::restoreLayout()
{
    QByteArray geom;
    QByteArray dockState = m_settings->restoreLayout(geom);
    if (!dockState.isEmpty()) {
        restoreState(dockState);
    }
    if (!geom.isEmpty()) {
        restoreGeometry(geom);
    }
}

// ---------------------------------------------------------------------------
// Signal / Slot Wiring
// ---------------------------------------------------------------------------

void MainWindow::setupConnections()
{
    auto *serial = m_core->serialManager();
    auto *state = m_core->machineState();

    // --- ConnectionPanel → SerialManager ---
    connect(m_connectionPanel, &ConnectionPanel::connectRequested,
            this, [this](const QString &port, int baud) {
                m_core->serialManager()->connectToGrbl(port, baud);
            });
    connect(m_connectionPanel, &ConnectionPanel::disconnectRequested,
            serial, &SerialManager::disconnectFromGrbl);

    // --- SerialManager → panels ---
    connect(serial, &SerialManager::connectedChanged,
            m_connectionPanel, &ConnectionPanel::setConnected);
    connect(serial, &SerialManager::connectedChanged,
            this, [this](bool connected) {
                m_consolePanel->appendMessage(connected ? "Connected to GRBL" : "Disconnected");
            });

    // --- Core responses → console ---
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

    // --- ControlPanel → file loading / streaming ---
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

    // --- ControlPanel → quick commands ---
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

    // --- ConsolePanel → SerialManager ---
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
