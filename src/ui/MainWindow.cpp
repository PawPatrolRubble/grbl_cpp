#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "DockPanel.h"
#include "ApplicationCore.h"
#include "SettingsManager.h"
#include "DesignCanvas.h"
#include "panels/ConnectionPanel.h"
#include "panels/StatusPanel.h"
#include "panels/ControlPanel.h"
#include "panels/JogPanel.h"
#include "panels/ConsolePanel.h"
#include "panels/SettingsPanel.h"
#include "panels/GraphicsDesignPanel.h"
#include "ToolpathPanel.h"
#include "TextToGcodeDialog.h"
#include "RackJobStartDialog.h"
#include "plotting/RackJobValidator.h"
#include "grbl/GrblParser.h"

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
#include <QTabBar>
#include <QTimer>
#include <QMouseEvent>
#include <QWindow>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_core(new ApplicationCore(this))
    , m_settings(&SettingsManager::instance())
{
    // Use default system title bar
    ui->setupUi(this);

    setupDocks();
    setupMenuBar();
    setupStatusBar();
    setupConnections();
    updateMotionControls();
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
    fileMenu->setObjectName("fileMenu");
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
    viewMenu->addAction("Toolpath", this, [this]() { m_dockToolpath->show(); m_dockToolpath->raise(); });
    viewMenu->addAction("Console", this, [this]() { m_dockConsole->show(); m_dockConsole->raise(); });
    viewMenu->addAction("Design", this, [this]() { m_dockDesign->show(); m_dockDesign->raise(); });
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
        addDockWidget(Qt::RightDockWidgetArea, m_dockConsole);
        tabifyDockWidget(m_dockToolpath, m_dockDesign);
        tabifyDockWidget(m_dockToolpath, m_dockSettings);
        splitDockWidget(m_dockToolpath, m_dockConsole, Qt::Vertical);
        splitDockWidget(m_dockConnection, m_dockStatus, Qt::Vertical);
        splitDockWidget(m_dockStatus, m_dockControl, Qt::Vertical);
        splitDockWidget(m_dockControl, m_dockJog, Qt::Vertical);
        m_dockToolpath->raise();
        for (auto *dock : {m_dockConnection, m_dockStatus, m_dockControl, m_dockJog,
                           m_dockToolpath, m_dockConsole, m_dockDesign, m_dockSettings}) {
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
    setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::North);
    setTabPosition(Qt::RightDockWidgetArea, QTabWidget::North);

    // Use a minimal central widget — all content lives in docks
    auto *placeholder = new QWidget();
    placeholder->setMaximumSize(0, 0);
    setCentralWidget(placeholder);

    int dockWidth = 450;

    // --- Left side docks ---
    m_connectionPanel = new ConnectionPanel();
    m_connectionPanel->setAvailablePorts(SerialManager::availablePorts());
    m_connectionPanel->setAvailableBaudRates(SerialManager::commonBaudRates());
    m_connectionPanel->setDefaultBaudRate("115200");
    m_dockConnection = createDock("Connection", m_connectionPanel, Qt::LeftDockWidgetArea, false);
    m_dockConnection->setMaximumWidth(dockWidth);

    m_statusPanel = new StatusPanel();
    m_dockStatus = createDock("Machine Status", m_statusPanel, Qt::LeftDockWidgetArea, false);
    m_dockStatus->setMaximumWidth(dockWidth);

    m_controlPanel = new ControlPanel();
    m_dockControl = createDock("Streaming Control", m_controlPanel, Qt::LeftDockWidgetArea, false);
    m_dockControl->setMaximumWidth(dockWidth);

    m_jogPanel = new JogPanel();
    m_dockJog = createDock("Jog Control", m_jogPanel, Qt::LeftDockWidgetArea, false);
    m_dockJog->setMaximumWidth(dockWidth);

    // Left docks stacked vertically (no tabify)
    splitDockWidget(m_dockConnection, m_dockStatus, Qt::Vertical);
    splitDockWidget(m_dockStatus, m_dockControl, Qt::Vertical);
    splitDockWidget(m_dockControl, m_dockJog, Qt::Vertical);

    // --- Right side: Toolpath + Design as tabs, Console below ---
    m_toolpathPanel = new ToolpathPanel();
    m_dockToolpath = createDock("Toolpath", m_toolpathPanel, Qt::RightDockWidgetArea);

    m_graphicsDesignPanel = new GraphicsDesignPanel();
    m_dockDesign = createDock("Design", m_graphicsDesignPanel, Qt::RightDockWidgetArea);

    // Tabify Toolpath and Design in the same area
    tabifyDockWidget(m_dockToolpath, m_dockDesign);
    m_dockToolpath->raise();

    // Console below the tabbed Toolpath/Design area
    m_consolePanel = new ConsolePanel();
    m_consolePanel->setMinimumHeight(120);
    m_dockConsole = createDock("Console", m_consolePanel, Qt::RightDockWidgetArea);
    // Split Console below the Toolpath dock
    splitDockWidget(m_dockToolpath, m_dockConsole, Qt::Vertical);

    // Settings (tabified with Toolpath/Design)
    m_settingsPanel = new SettingsPanel();
    m_dockSettings = createDock("Settings", m_settingsPanel, Qt::RightDockWidgetArea);
    tabifyDockWidget(m_dockToolpath, m_dockSettings);
    m_dockToolpath->raise();

    // Set left panel initial width to dockWidth
    resizeDocks({m_dockConnection}, {dockWidth}, Qt::Horizontal);
}

void MainWindow::setupDockTabBars()
{
    for (auto *tabBar : findChildren<QTabBar *>()) {
        if (tabBar->property("_dockTabSetup").toBool())
            continue;
        tabBar->setProperty("_dockTabSetup", true);
        tabBar->setTabsClosable(true);

        connect(tabBar, &QTabBar::tabCloseRequested, this, [tabBar, this](int index) {
            QString tabText = tabBar->tabText(index);
            for (auto *dock : findChildren<DockPanel *>()) {
                if (dock->windowTitle() == tabText) {
                    dock->close();
                    break;
                }
            }
        });

        // Double-click tab to toggle float
        tabBar->installEventFilter(this);
    }
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    QTimer::singleShot(100, this, &MainWindow::setupDockTabBars);

    // Re-setup tab bars whenever any dock's floating/location changes
    for (auto *dock : findChildren<DockPanel *>()) {
        if (dock->property("_dockSignalsConnected").toBool())
            continue;
        dock->setProperty("_dockSignalsConnected", true);
        connect(dock, &QDockWidget::topLevelChanged, this, [this]() {
            QTimer::singleShot(100, this, &MainWindow::setupDockTabBars);
        });
        connect(dock, &QDockWidget::dockLocationChanged, this, [this]() {
            QTimer::singleShot(100, this, &MainWindow::setupDockTabBars);
        });
    }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    auto *tabBar = qobject_cast<QTabBar *>(obj);
    if (!tabBar)
        return QMainWindow::eventFilter(obj, event);

    if (event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton) {
            int tabIndex = tabBar->tabAt(me->pos());
            if (tabIndex >= 0) {
                tabBar->setProperty("_dragTabIndex", tabIndex);
                tabBar->setProperty("_dragStartPos", me->globalPosition().toPoint());
            }
        }
    } else if (event->type() == QEvent::MouseMove) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->buttons() & Qt::LeftButton) {
            QPoint startPos = tabBar->property("_dragStartPos").toPoint();
            int tabIndex = tabBar->property("_dragTabIndex").toInt();
            if (!startPos.isNull() && tabIndex >= 0) {
                QPoint delta = me->globalPosition().toPoint() - startPos;
                if (delta.manhattanLength() > 20) {
                    // Find the dock for this tab and float it
                    QString tabText = tabBar->tabText(tabIndex);
                    for (auto *dock : findChildren<DockPanel *>()) {
                        if (dock->windowTitle() == tabText) {
                            dock->setFloating(true);
                            dock->move(me->globalPosition().toPoint());
                            dock->raise();
                            // Start native window drag
                            if (dock->windowHandle())
                                dock->windowHandle()->startSystemMove();
                            tabBar->setProperty("_dragStartPos", QPoint());
                            tabBar->setProperty("_dragTabIndex", -1);
                            break;
                        }
                    }
                }
            }
        }
    } else if (event->type() == QEvent::MouseButtonRelease) {
        tabBar->setProperty("_dragStartPos", QPoint());
        tabBar->setProperty("_dragTabIndex", -1);
    }

    return QMainWindow::eventFilter(obj, event);
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
    QByteArray dockState = saveState(5);  // version 5 = settings tab
    m_settings->saveLayout(dockState, geometry);
}

void MainWindow::restoreLayout()
{
    QByteArray geometry;
    QByteArray state = m_settings->restoreLayout(geometry);
    if (!state.isEmpty())
        restoreState(state, 5);  // only restore version 5
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
            m_controlPanel, &ControlPanel::setConnected);
    connect(m_core, &ApplicationCore::executionAvailabilityChanged,
            m_controlPanel, &ControlPanel::setExecutionReady);
    connect(m_core, &ApplicationCore::executionAvailabilityChanged, this, [this](bool) {
        updateMotionControls();
    });
    connect(m_core, &ApplicationCore::errorOccurred, this, [this](const QString& error) {
        m_consolePanel->appendMessage(error);
        statusBar()->showMessage(error);
    });
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
    connect(m_core, &ApplicationCore::executionPhaseChanged, this,
            [this](const QString& phase, const QUuid&, const QUuid&) {
                m_controlPanel->setExecutionPhase(phase);
                statusBar()->showMessage(phase);
                m_consolePanel->appendMessage("Job phase: " + phase);
            });
    connect(m_core, &ApplicationCore::streamingProgress,
            this, [this](int current, int /*total*/) {
                m_toolpathPanel->setExecutingLine(current);
            });
    connect(m_core, &ApplicationCore::streamingStarted,
            m_controlPanel, &ControlPanel::setStreamingStarted);
    connect(m_core, &ApplicationCore::streamingStarted,
            this, [this]() {
                m_core->serialManager()->setStatusPollInterval(500);
                m_graphicsDesignPanel->setEnabled(false);
                m_settingsPanel->setEnabled(false);
                m_toolpathPanel->setEnabled(false);
                if (auto* menu = findChild<QMenu*>("fileMenu")) menu->setEnabled(false);
            });
    connect(m_core, &ApplicationCore::streamingPaused,
            m_controlPanel, &ControlPanel::setStreamingPaused);
    connect(m_core, &ApplicationCore::streamingResumed,
            m_controlPanel, &ControlPanel::setStreamingResumed);
    connect(m_core, &ApplicationCore::streamingEnded,
            this, [this](GcodeStreamer::Outcome outcome, const QString& message, int sourceLine) {
                QString detail = message;
                if (sourceLine >= 0) detail = QString("Line %1: %2").arg(sourceLine + 1).arg(message);
                m_controlPanel->setStreamingEnded(detail);
                m_toolpathPanel->setExecutingLine(-1);
                m_core->serialManager()->setStatusPollInterval(250);
                m_graphicsDesignPanel->setEnabled(true);
                m_settingsPanel->setEnabled(true);
                m_toolpathPanel->setEnabled(true);
                if (auto* menu = findChild<QMenu*>("fileMenu")) menu->setEnabled(true);
                m_consolePanel->appendMessage(detail);
                statusBar()->showMessage(detail);
                Q_UNUSED(outcome)
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
                updateMotionControls();
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

    // --- MachineState → SettingsPanel (GRBL settings) ---
    connect(state, &MachineState::grblSettingsChanged,
            m_settingsPanel, &SettingsPanel::setGrblSettings);
    connect(state, &MachineState::grblSettingsChanged,
            this, [this](const QMap<int, double> &settings) {
                double w = settings.value(130, 300.0);
                double h = settings.value(131, 200.0);
                QSizeF tableSize(w, h);
                m_toolpathPanel->setWorktableSize(tableSize);
                m_graphicsDesignPanel->setWorktableSize(tableSize);
            });
    connect(m_settingsPanel, &SettingsPanel::grblSettingsRefreshRequested,
            this, [this]() {
                if (m_core->serialManager()->isConnected())
                    m_core->sendCommand("$$");
            });
    connect(m_settingsPanel, &SettingsPanel::grblSettingWriteRequested,
            this, [this](int id, double value) {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->sendCommand(QString("$%1=%2").arg(id).arg(value, 0, 'f', 3));
                m_consolePanel->appendMessage(QString("Set $%1=%2").arg(id).arg(value, 0, 'f', 3));
            });

    // --- SettingsPanel → SettingsManager (canvas appearance) ---
    m_settingsPanel->applyCanvasSettings(m_settings->canvasSettings());
    connect(m_settingsPanel, &SettingsPanel::canvasSettingsChanged,
            this, [this](const SettingsManager::CanvasSettings &s) {
                m_settings->setCanvasSettings(s);
            });

    // --- StatusPanel 鈫?SerialManager ---
    connect(m_statusPanel, &StatusPanel::zeroAxisRequested,
            this, [this](const QString &axis) {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->sendCommands({QString("G10 L20 P0 %1%2").arg(axis).arg(0), "$#"});
                m_consolePanel->appendMessage(QString("Set %1 work position to zero").arg(axis));
            });
    connect(m_statusPanel, &StatusPanel::setAxisPositionRequested,
            this, [this](const QString &axis, double value) {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->sendCommands({QString("G10 L20 P0 %1%2").arg(axis).arg(value, 0, 'f', 3), "$#"});
                m_consolePanel->appendMessage(
                    QString("Set %1 work position to %2").arg(axis).arg(value, 0, 'f', 3));
            });
    connect(m_statusPanel, &StatusPanel::zeroAllRequested,
            this, [this]() {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->sendCommands({"G10 L20 P0 X0 Y0 Z0", "$#"});
                m_consolePanel->appendMessage("Set all work positions to zero");
            });
    connect(m_statusPanel, &StatusPanel::goToZeroRequested,
            this, [this]() {
                if (!m_core->serialManager()->isConnected()) return;
                // Safety: raise Z first, then move XY, then lower Z
                m_core->sendCommands({"G90 G0 Z5", "G90 G0 X0 Y0", "G90 G0 Z0"});
                m_consolePanel->appendMessage("Moving to zero position (Z up first)");
            });
    connect(m_statusPanel, &StatusPanel::resetWorkOffsetRequested,
            this, [this]() {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->sendCommands({"G10 L2 P1 X0 Y0 Z0", "$#"});
                m_consolePanel->appendMessage("Reset work offset work position now equals machine position");
            });
    connect(m_statusPanel, &StatusPanel::emergencyStopRequested,
            this, [this]() {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->sendRealtime(GrblRealtime::Reset);
                m_consolePanel->appendMessage("EMERGENCY STOP 鈥?soft reset sent");
            });
    connect(m_statusPanel, &StatusPanel::homeMachineRequested,
            this, [this]() {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->sendCommand("$H");
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
                PlotJob job = m_toolpathPanel->currentPlotJob();
                QStringList lines = job.lines();
                if (lines.isEmpty()) {
                    job = PlotJob::fromLines(m_core->fileManager()->lines());
                    lines = job.lines();
                }
                if (lines.isEmpty()) return;
                if (job.automaticRack) {
                    if (m_toolpathPanel->rangeStart() >= 0 || m_toolpathPanel->rangeEnd() >= 0) {
                        QMessageBox::warning(this, "Complete job required", "Clear the selected line range before starting an automatic rack job.");
                        return;
                    }
                    const QString issue = RackJobValidator::validate(job, *m_settings->toolLibrary(),
                        m_core->grblParser()->workCoordinateOffset(job.workCoordinateSystem),
                        m_core->grblParser()->currentMachinePosition());
                    if (!issue.isEmpty()) { QMessageBox::warning(this, "Automatic job setup", issue); return; }
                    RackJobStartDialog setup(job, *m_settings->toolLibrary(), this);
                    if (setup.exec() != QDialog::Accepted) return;
                    const auto tool = setup.holderToolId();
                    if (!m_core->confirmRackSetup(tool)) {
                        QMessageBox::warning(this, "Controller busy", "Wait for an idle ready controller before confirming the holder."); return;
                    }
                    m_core->startJob(job);
                    return;
                }
                if (m_toolpathPanel->returnToOriginEnabled())
                    job.blocks.append({"G90 G0 X0 Y0 Z0", int(job.blocks.size()), {}, {},
                                       PlotJobBlock::Kind::Finish, PlotJobBlock::Frame::Work});

                int rangeStart = m_toolpathPanel->rangeStart();
                int rangeEnd = m_toolpathPanel->rangeEnd();
                int startLine = 0;
                int endLine = -1;  // -1 means end of file

                if (rangeStart >= 0 || rangeEnd >= 0) {
                    if (rangeStart >= 0) startLine = rangeStart;
                    if (rangeEnd >= 0) endLine = rangeEnd;
                    QString rangeDesc = QString("lines %1 - %2 of %3")
                        .arg(startLine + 1)
                        .arg(endLine >= 0 ? endLine + 1 : lines.size())
                        .arg(lines.size());
                    auto result = QMessageBox::question(this, "Start Streaming",
                        QString("Stream %1?").arg(rangeDesc),
                        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
                    if (result == QMessageBox::Cancel) return;
                    if (result == QMessageBox::No) {
                        startLine = 0;
                        endLine = -1;
                    }
                }
                m_core->startJob(job, startLine, endLine);
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
                m_core->sendCommand("$H");
                m_consolePanel->appendMessage("Homing...");
            });
    connect(m_controlPanel, &ControlPanel::unlockRequested,
            this, [this]() {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->sendCommands({"$X", "$G", "$#"});
                m_consolePanel->appendMessage("Alarm unlocked");
            });
    connect(m_controlPanel, &ControlPanel::resetRequested,
            this, [this]() {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->sendRealtime(0x18);
                m_consolePanel->appendMessage("Soft reset sent");
            });
    connect(m_controlPanel, &ControlPanel::statusRequested,
            this, [this]() {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->sendRealtime('?');
            });

    // --- JogPanel 鈫?SerialManager ---
    connect(m_jogPanel, &JogPanel::jogContinuousRequested,
            this, [this](const QString &axis, int direction, double feed) {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->jogContinuous(axis, direction, feed);
            });
    connect(m_jogPanel, &JogPanel::jogStepRequested,
            this, [this](const QString &axis, double distance, double feed) {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->jogStep(axis, distance, feed);
            });
    connect(m_jogPanel, &JogPanel::jogCancelRequested,
            this, [this]() {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->jogCancel();
            });
    connect(m_jogPanel, &JogPanel::goToAbsoluteRequested,
            this, [this](double x, double y, double z, double feed) {
                if (!m_core->serialManager()->isConnected()) return;
                // Safety: raise Z first, then move XY, then lower Z
                QString feedStr = QString("F%1").arg(feed, 0, 'f', 0);
                m_core->sendCommands({QString("G90 G0 Z%1 %2").arg(qMax(z, 5.0), 0, 'f', 3).arg(feedStr),
                    QString("G90 G0 X%1 Y%2 %3").arg(x, 0, 'f', 3).arg(y, 0, 'f', 3).arg(feedStr),
                    QString("G90 G0 Z%1 %2").arg(z, 0, 'f', 3).arg(feedStr)});
                m_consolePanel->appendMessage(QString("Go to absolute: X%1 Y%2 Z%3 F%4 (Z up first)")
                    .arg(x, 0, 'f', 3).arg(y, 0, 'f', 3).arg(z, 0, 'f', 3).arg(feed, 0, 'f', 0));
            });
    connect(m_jogPanel, &JogPanel::goToXRequested,
            this, [this](double x, double feed) {
                if (!m_core->serialManager()->isConnected()) return;
                QString feedStr = QString("F%1").arg(feed, 0, 'f', 0);
                m_core->sendCommand(QString("G90 G0 X%1 %2").arg(x, 0, 'f', 3).arg(feedStr));
                m_consolePanel->appendMessage(QString("Go to X%1 F%2").arg(x, 0, 'f', 3).arg(feed, 0, 'f', 0));
            });
    connect(m_jogPanel, &JogPanel::goToYRequested,
            this, [this](double y, double feed) {
                if (!m_core->serialManager()->isConnected()) return;
                QString feedStr = QString("F%1").arg(feed, 0, 'f', 0);
                m_core->sendCommand(QString("G90 G0 Y%1 %2").arg(y, 0, 'f', 3).arg(feedStr));
                m_consolePanel->appendMessage(QString("Go to Y%1 F%2").arg(y, 0, 'f', 3).arg(feed, 0, 'f', 0));
            });
    connect(m_jogPanel, &JogPanel::goToZRequested,
            this, [this](double z, double feed) {
                if (!m_core->serialManager()->isConnected()) return;
                QString feedStr = QString("F%1").arg(feed, 0, 'f', 0);
                m_core->sendCommand(QString("G90 G0 Z%1 %2").arg(z, 0, 'f', 3).arg(feedStr));
                m_consolePanel->appendMessage(QString("Go to Z%1 F%2").arg(z, 0, 'f', 3).arg(feed, 0, 'f', 0));
            });

    // --- ConsolePanel 鈫?SerialManager ---
    connect(m_consolePanel, &ConsolePanel::commandEntered,
            this, [this](const QString &cmd) {
                if (m_core->serialManager()->isConnected())
                    m_core->sendCommand(cmd);
            });

    // --- Async parse ---
    connect(m_toolpathPanel, &ToolpathPanel::previewReady,
            this, [this](const QUuid&) {
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

    // --- ToolpathPanel transform → keep FileManager in sync ---
    connect(m_toolpathPanel, &ToolpathPanel::gcodeTransformed,
            this, [this](const QStringList &lines) {
                m_core->fileManager()->loadLines(m_core->fileManager()->fileName(), lines);
                m_controlPanel->setFileLoaded(m_core->fileManager()->fileName(), lines.size());
            });
    connect(m_toolpathPanel, &ToolpathPanel::transformRejected, this, [this](const QString& reason) {
        m_consolePanel->appendMessage(reason);
        statusBar()->showMessage(reason);
    });

    // --- ToolpathPanel range → update canvas visualization ---
    connect(m_toolpathPanel, &ToolpathPanel::rangeChanged,
            m_toolpathPanel, &ToolpathPanel::setStreamRange);

    // --- GraphicsDesignPanel (tab) 鈫?G-code generation ---
    connect(m_graphicsDesignPanel, &GraphicsDesignPanel::plotJobGenerated,
            this, [this](const PlotJob& job) {
                const auto lines = job.lines();
                m_core->fileManager()->loadLines("GraphicsDesign", lines);
                m_toolpathPanel->setPlotJob(job);
                m_controlPanel->setFileLoaded(m_core->fileManager()->fileName(), m_core->fileManager()->lines().size());
                m_consolePanel->appendMessage("Generated Graphics Design G-code loaded.");
                statusBar()->showMessage("Graphics Design G-code loaded");

                // Switch to toolpath tab to show result
                m_dockToolpath->raise();
                m_dockToolpath->show();

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

    } else {
        QMessageBox::warning(this, "Error", "Could not load file: " + filePath);
    }
}

void MainWindow::updateMotionControls()
{
    const bool ready = m_core->streamer()->readyToStart();
    m_statusPanel->setMotionEnabled(ready);
    m_jogPanel->setAbsoluteMotionEnabled(ready);
    const auto state = m_core->machineState()->state();
    // Keep held jog buttons enabled until release can send jog-cancel. Ordinary
    // absolute moves and coordinate edits still wait for the manual checkpoint.
    m_jogPanel->setJogEnabled(m_core->serialManager()->isConnected()
        && m_core->serialManager()->isReady() && !m_core->streamer()->isStreaming()
        && (state == MachineState::State::Idle || state == MachineState::State::Jog));
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
