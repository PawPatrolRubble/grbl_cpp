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
#include "GraphicsDesignDialog.h"

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
            ui->toolpathPanel->setGcodeLines(m_core->fileManager()->lines());
            m_controlPanel->setFileLoaded(m_core->fileManager()->fileName(), m_core->fileManager()->lines().size());
            ui->consolePanel->appendMessage("Generated Text to G-code loaded.");

            QStringList parseLines = m_core->fileManager()->lines();
            QFuture<QList<GcodeMove>> future = QtConcurrent::run([parseLines]() {
                GcodeParser parser;
                return parser.parse(parseLines);
            });
            m_parseWatcher->setFuture(future);
        });
        dialog.exec();
    }, QKeySequence(Qt::CTRL | Qt::Key_T));
    auto *graphicsAction = new QAction("Graphics Design", this);
    graphicsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    connect(graphicsAction, &QAction::triggered, this, [this]() {
        GraphicsDesignDialog dialog(this);
        connect(&dialog, &GraphicsDesignDialog::gcodeGenerated, this, [this](const QStringList &lines) {
            m_core->fileManager()->loadLines("GraphicsDesign", lines);
            ui->toolpathPanel->setGcodeLines(m_core->fileManager()->lines());
            m_controlPanel->setFileLoaded(m_core->fileManager()->fileName(), m_core->fileManager()->lines().size());
            ui->consolePanel->appendMessage("Generated Graphics Design G-code loaded.");

            QStringList parseLines = m_core->fileManager()->lines();
            QFuture<QList<GcodeMove>> future = QtConcurrent::run([parseLines]() {
                GcodeParser parser;
                return parser.parse(parseLines);
            });
            m_parseWatcher->setFuture(future);
        });
        dialog.exec();
    });
    fileMenu->addAction(graphicsAction);
    fileMenu->addSeparator();
    fileMenu->addAction("Exit", this, &QWidget::close, QKeySequence::Quit);

    // --- View menu (dock visibility toggles) ---
    auto *viewMenu = menuBar->addMenu("&View");
    viewMenu->addAction("Connection", m_dockConnection, &QDockWidget::toggleViewAction);
    viewMenu->addAction("Machine Status", m_dockStatus, &QDockWidget::toggleViewAction);
    viewMenu->addAction("Streaming Control", m_dockControl, &QDockWidget::toggleViewAction);
    viewMenu->addAction("Jog Control", m_dockJog, &QDockWidget::toggleViewAction);
    viewMenu->addAction("Settings", m_dockSettings, &QDockWidget::toggleViewAction);
    viewMenu->addAction("Graphics Design", m_dockGraphicsDesign, &QDockWidget::toggleViewAction);
    viewMenu->addSeparator();
    viewMenu->addAction("Reset Layout", this, [this]() {
        m_settings->resetToDefaults();
        for (auto *dock : {m_dockConnection, m_dockStatus, m_dockControl, m_dockJog, m_dockSettings, m_dockGraphicsDesign}) {
            removeDockWidget(dock);
        }
        addDockWidget(Qt::LeftDockWidgetArea, m_dockConnection);
        addDockWidget(Qt::LeftDockWidgetArea, m_dockStatus);
        addDockWidget(Qt::LeftDockWidgetArea, m_dockControl);
        addDockWidget(Qt::LeftDockWidgetArea, m_dockJog);
        addDockWidget(Qt::RightDockWidgetArea, m_dockSettings);
        addDockWidget(Qt::RightDockWidgetArea, m_dockGraphicsDesign);
        m_dockSettings->hide();
        m_dockGraphicsDesign->hide();
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

    // Graphics Design panel (right, hidden by default)
    m_graphicsDesignPanel = new GraphicsDesignPanel();
    m_dockGraphicsDesign = createDock("Graphics Design", m_graphicsDesignPanel, Qt::RightDockWidgetArea);
    m_dockGraphicsDesign->hide();

    // Tabify left docks
    tabifyDockWidget(m_dockConnection, m_dockStatus);
    tabifyDockWidget(m_dockStatus, m_dockControl);
    tabifyDockWidget(m_dockControl, m_dockJog);
    m_dockConnection->raise();
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
                ui->consolePanel->appendMessage(connected ? "Connected to GRBL" : "Disconnected");
            });

    // --- Core responses → console ---
    connect(m_core, &ApplicationCore::responseReceived,
            this, [this](const QString &response) {
                ui->consolePanel->appendMessage(response);
            });

    // --- Core streaming → UI ---
    connect(m_core, &ApplicationCore::streamingProgress,
            m_controlPanel, &ControlPanel::setStreamingProgress);
    connect(m_core, &ApplicationCore::streamingProgress,
            this, [this](int current, int /*total*/) {
                ui->toolpathPanel->setExecutingLine(current);
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
                ui->consolePanel->appendMessage("Streaming finished");
                statusBar()->showMessage("Streaming complete");
            });

    // --- MachineState → StatusPanel + ToolpathPanel ---
    connect(state, &MachineState::machinePositionChanged,
            m_statusPanel, &StatusPanel::setMachinePosition);
    connect(state, &MachineState::machinePositionChanged,
            ui->toolpathPanel, &ToolpathPanel::setCutterPosition);
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
                ui->consolePanel->appendMessage(QString("Set %1 work position to zero").arg(axis));
            });
    connect(m_statusPanel, &StatusPanel::zeroAllRequested,
            this, [this]() {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->serialManager()->sendCommand("G10 L20 P0 X0 Y0 Z0");
                ui->consolePanel->appendMessage("Set all work positions to zero");
            });
    connect(m_statusPanel, &StatusPanel::goToZeroRequested,
            this, [this]() {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->serialManager()->sendCommand("G90 G0 X0 Y0 Z0");
                ui->consolePanel->appendMessage("Moving to zero position");
            });
    connect(m_statusPanel, &StatusPanel::resetWorkOffsetRequested,
            this, [this]() {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->serialManager()->sendCommand("G10 L2 P1 X0 Y0 Z0");
                ui->consolePanel->appendMessage("Reset work offset — work position now equals machine position");
            });

    // --- ControlPanel → Core ---
    connect(m_controlPanel, &ControlPanel::loadFileRequested,
            this, [this]() {
                QString path = QFileDialog::getOpenFileName(this, "Open G-Code File",
                                                             QString(), "G-Code (*.nc *.ngc *.gcode *.txt)");
                if (!path.isEmpty())
                    loadAndParseFile(path);
            });
    connect(m_controlPanel, &ControlPanel::startRequested,
            this, [this]() {
                m_core->startStreaming(m_core->fileManager()->lines());
            });
    connect(m_controlPanel, &ControlPanel::pauseRequested,
            m_core, &ApplicationCore::pauseStreaming);
    connect(m_controlPanel, &ControlPanel::resumeRequested,
            m_core, &ApplicationCore::resumeStreaming);
    connect(m_controlPanel, &ControlPanel::stopRequested,
            m_core, &ApplicationCore::stopStreaming);

    // --- JogPanel → SerialManager ---
    connect(m_jogPanel, &JogPanel::jogStepRequested,
            this, [this](const QString &axis, double distance, double feed) {
                if (!m_core->serialManager()->isConnected()) return;
                QString cmd = QString("G91 G0 %1%2 F%3").arg(axis).arg(distance).arg(feed);
                m_core->serialManager()->sendCommand(cmd);
                ui->consolePanel->appendMessage(cmd);
            });
    connect(m_jogPanel, &JogPanel::jogContinuousRequested,
            this, [this](const QString &axis, int direction, double feed) {
                if (!m_core->serialManager()->isConnected()) return;
                QString cmd = QString("$J=G91 %1%2 F%3").arg(axis).arg(direction > 0 ? "" : "-").arg(feed);
                m_core->serialManager()->sendCommand(cmd);
                ui->consolePanel->appendMessage(cmd);
            });
    connect(m_jogPanel, &JogPanel::jogCancelRequested,
            this, [this]() {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->serialManager()->sendCommand("\x85");  // Jog cancel
                ui->consolePanel->appendMessage("Jog cancel");
            });

    // --- ConsolePanel → SerialManager ---
    connect(ui->consolePanel, &ConsolePanel::commandEntered,
            this, [this](const QString &cmd) {
                if (m_core->serialManager()->isConnected())
                    m_core->sendCommand(cmd);
            });

    // --- Async parse ---
    connect(m_parseWatcher, &QFutureWatcher<QList<GcodeMove>>::finished,
            this, [this]() {
                ui->toolpathPanel->setMoves(m_parseWatcher->result());
                statusBar()->showMessage("File loaded: " + m_core->fileManager()->fileName());
            });

    // --- GraphicsDesignPanel → G-code generation ---
    connect(m_graphicsDesignPanel, &GraphicsDesignPanel::gcodeGenerated,
            this, [this](const QStringList &lines) {
                m_core->fileManager()->loadLines("GraphicsDesign", lines);
                ui->toolpathPanel->setGcodeLines(m_core->fileManager()->lines());
                m_controlPanel->setFileLoaded(m_core->fileManager()->fileName(), m_core->fileManager()->lines().size());
                ui->consolePanel->appendMessage("Generated Graphics Design G-code loaded.");
                statusBar()->showMessage("Graphics Design G-code loaded");

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
        ui->toolpathPanel->setGcodeLines(fm->lines());
        m_controlPanel->setFileLoaded(fm->fileName(), fm->lines().size());
        ui->consolePanel->appendMessage("File loaded: " + fm->fileName());
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