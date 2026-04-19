#include "MainWindow.h"
#include "TabWidget.h"
#include "pages/MainPage.h"
#include "pages/GraphicsDesignPage.h"
#include "ApplicationCore.h"
#include "SettingsManager.h"
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

    setupTabs();
    setupMenuBar();
    setupStatusBar();
    setupConnections();
}

MainWindow::~MainWindow()
{
    delete ui;
}

// ---------------------------------------------------------------------------
// Custom Title Bar - uses embedded menu bar
// ---------------------------------------------------------------------------

void MainWindow::setupMenuBar()
{
    // Use QMainWindow's standard menu bar
    auto *menuBar = this->menuBar();

    // --- File menu ---
    auto *fileMenu = menuBar->addMenu("&File");
    fileMenu->addAction("Open...", this, [this]() {
        emit m_mainPage->controlPanel()->loadFileRequested();
    }, QKeySequence::Open);
    fileMenu->addSeparator();
    fileMenu->addAction("Text to G-code...", this, [this]() {
        TextToGcodeDialog dialog(this);
        connect(&dialog, &TextToGcodeDialog::gcodeGenerated, this, [this](const QStringList &lines) {
            m_core->fileManager()->loadLines("TextToGcode", lines);
            m_mainPage->toolpathPanel()->setGcodeLines(m_core->fileManager()->lines());
            m_mainPage->controlPanel()->setFileLoaded(m_core->fileManager()->fileName(), m_core->fileManager()->lines().size());
            m_mainPage->consolePanel()->appendMessage("Generated Text to G-code loaded.");

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
        openGraphicsDesignTab();
    }, QKeySequence(Qt::CTRL | Qt::Key_G));
    fileMenu->addSeparator();
    fileMenu->addAction("Exit", this, &QWidget::close, QKeySequence::Quit);

    // --- View menu (tab navigation) ---
    auto *viewMenu = menuBar->addMenu("&View");
    viewMenu->addAction("Next Tab", this, [this]() {
        int next = (m_tabs->currentPage() + 1) % m_tabs->pageCount();
        m_tabs->setCurrentPage(next);
    }, QKeySequence(Qt::CTRL | Qt::Key_Tab));
    viewMenu->addAction("Previous Tab", this, [this]() {
        int prev = (m_tabs->currentPage() - 1);
        if (prev < 0) prev = m_tabs->pageCount() - 1;
        m_tabs->setCurrentPage(prev);
    }, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab));
    viewMenu->addSeparator();
    viewMenu->addAction("Close Tab", this, [this]() {
        closeTab(m_tabs->currentPage());
    }, QKeySequence(Qt::CTRL | Qt::Key_W));

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
// Tab Navigation
// ---------------------------------------------------------------------------

void MainWindow::setupTabs()
{
    m_tabs = new TabWidget(this);
    setCentralWidget(m_tabs);

    // Add main page (non-closable)
    m_mainPage = new MainPage(this);
    m_tabs->addPage("Main", m_mainPage, false);

    // Set available ports on connection panel
    m_mainPage->connectionPanel()->setAvailablePorts(SerialManager::availablePorts());
    m_mainPage->connectionPanel()->setAvailableBaudRates(SerialManager::commonBaudRates());
    m_mainPage->connectionPanel()->setDefaultBaudRate("115200");

    // Handle tab close requests
    connect(m_tabs, &TabWidget::closeRequested, this, &MainWindow::closeTab);
}

void MainWindow::openGraphicsDesignTab()
{
    auto *designPage = new GraphicsDesignPage(this);
    int index = m_tabs->addPage("Graphics Design", designPage, true);
    m_tabs->setCurrentPage(index);
    m_designPages.append(designPage);

    // Wire gcode generation to main page
    connect(designPage, &GraphicsDesignPage::gcodeGenerated,
            this, [this](const QStringList &lines) {
                m_core->fileManager()->loadLines("GraphicsDesign", lines);
                m_mainPage->toolpathPanel()->setGcodeLines(m_core->fileManager()->lines());
                m_mainPage->controlPanel()->setFileLoaded(m_core->fileManager()->fileName(),
                                                          m_core->fileManager()->lines().size());
                m_mainPage->consolePanel()->appendMessage("Generated Graphics Design G-code loaded.");

                QStringList parseLines = m_core->fileManager()->lines();
                QFuture<QList<GcodeMove>> future = QtConcurrent::run([parseLines]() {
                    GcodeParser parser;
                    return parser.parse(parseLines);
                });
                m_parseWatcher->setFuture(future);
            });
}

void MainWindow::closeTab(int index)
{
    if (index == 0) return;  // Main tab cannot be closed

    QWidget *page = m_tabs->pageAt(index);
    if (auto *designPage = qobject_cast<GraphicsDesignPage*>(page)) {
        m_designPages.removeOne(designPage);
    }
    m_tabs->removePage(index);
}

// ---------------------------------------------------------------------------
// Signal / Slot Wiring
// ---------------------------------------------------------------------------

void MainWindow::setupConnections()
{
    auto *serial = m_core->serialManager();
    auto *state = m_core->machineState();

    // --- ConnectionPanel -> SerialManager ---
    connect(m_mainPage->connectionPanel(), &ConnectionPanel::connectRequested,
            this, [this](const QString &port, int baud) {
                m_core->serialManager()->connectToGrbl(port, baud);
            });
    connect(m_mainPage->connectionPanel(), &ConnectionPanel::disconnectRequested,
            serial, &SerialManager::disconnectFromGrbl);

    // --- SerialManager -> panels ---
    connect(serial, &SerialManager::connectedChanged,
            m_mainPage->connectionPanel(), &ConnectionPanel::setConnected);
    connect(serial, &SerialManager::connectedChanged,
            this, [this](bool connected) {
                m_mainPage->consolePanel()->appendMessage(connected ? "Connected to GRBL" : "Disconnected");
            });

    // --- Core responses -> console ---
    connect(m_core, &ApplicationCore::responseReceived,
            this, [this](const QString &response) {
                m_mainPage->consolePanel()->appendMessage(response);
            });

    // --- Core streaming -> UI ---
    connect(m_core, &ApplicationCore::streamingProgress,
            m_mainPage->controlPanel(), &ControlPanel::setStreamingProgress);
    connect(m_core, &ApplicationCore::streamingProgress,
            this, [this](int current, int /*total*/) {
                m_mainPage->toolpathPanel()->setExecutingLine(current);
            });
    connect(m_core, &ApplicationCore::streamingStarted,
            m_mainPage->controlPanel(), &ControlPanel::setStreamingStarted);
    connect(m_core, &ApplicationCore::streamingPaused,
            m_mainPage->controlPanel(), &ControlPanel::setStreamingPaused);
    connect(m_core, &ApplicationCore::streamingResumed,
            m_mainPage->controlPanel(), &ControlPanel::setStreamingResumed);
    connect(m_core, &ApplicationCore::streamingFinished,
            this, [this]() {
                m_mainPage->controlPanel()->setStreamingFinished();
                m_mainPage->consolePanel()->appendMessage("Streaming finished");
                statusBar()->showMessage("Streaming complete");
            });

    // --- MachineState -> StatusPanel + ToolpathPanel ---
    connect(state, &MachineState::machinePositionChanged,
            m_mainPage->statusPanel(), &StatusPanel::setMachinePosition);
    connect(state, &MachineState::machinePositionChanged,
            m_mainPage->toolpathPanel(), &ToolpathPanel::setCutterPosition);
    connect(state, &MachineState::workPositionChanged,
            m_mainPage->statusPanel(), &StatusPanel::setWorkPosition);
    connect(state, &MachineState::stateChanged,
            m_mainPage->statusPanel(), &StatusPanel::setState);
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
            m_mainPage->statusPanel(), &StatusPanel::setFeedRate);
    connect(state, &MachineState::spindleSpeedChanged,
            m_mainPage->statusPanel(), &StatusPanel::setSpindleSpeed);

    // --- StatusPanel -> SerialManager ---
    connect(m_mainPage->statusPanel(), &StatusPanel::zeroAxisRequested,
            this, [this](const QString &axis) {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->serialManager()->sendCommand(QString("G10 L20 P0 %1%2").arg(axis).arg(0));
                m_mainPage->consolePanel()->appendMessage(QString("Set %1 work position to zero").arg(axis));
            });
    connect(m_mainPage->statusPanel(), &StatusPanel::zeroAllRequested,
            this, [this]() {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->serialManager()->sendCommand("G10 L20 P0 X0 Y0 Z0");
                m_mainPage->consolePanel()->appendMessage("Set all work positions to zero");
            });
    connect(m_mainPage->statusPanel(), &StatusPanel::goToZeroRequested,
            this, [this]() {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->serialManager()->sendCommand("G90 G0 X0 Y0 Z0");
                m_mainPage->consolePanel()->appendMessage("Moving to zero position");
            });
    connect(m_mainPage->statusPanel(), &StatusPanel::resetWorkOffsetRequested,
            this, [this]() {
                if (!m_core->serialManager()->isConnected()) return;
                m_core->serialManager()->sendCommand("G10 L2 P1 X0 Y0 Z0");
                m_mainPage->consolePanel()->appendMessage("Reset work offset - work position now equals machine position");
            });

    // --- ToolpathPanel -> transform feedback ---
    connect(m_mainPage->toolpathPanel(), &ToolpathPanel::gcodeTransformed,
            this, [this](const QStringList &newLines) {
                m_mainPage->controlPanel()->setFileLoaded(m_core->fileManager()->fileName(), newLines.size());
                m_mainPage->consolePanel()->appendMessage(QString("Transform applied - %1 lines").arg(newLines.size()));
            });

    // --- Core errors -> console + statusbar ---
    connect(m_core, &ApplicationCore::errorOccurred,
            this, [this](const QString &err) {
                m_mainPage->consolePanel()->appendMessage("ERROR: " + err);
                statusBar()->showMessage("Error: " + err);
            });

    // --- ControlPanel -> file loading / streaming ---
    connect(m_mainPage->controlPanel(), &ControlPanel::loadFileRequested, this, [this]() {
        QString filePath = QFileDialog::getOpenFileName(this, "Open G-Code File", QString(),
            "G-Code Files (*.nc *.gcode *.ngc *.txt);;All Files (*)");
        if (filePath.isEmpty()) return;
        loadAndParseFile(filePath);
    });
    connect(m_mainPage->controlPanel(), &ControlPanel::startRequested, this, [this]() {
        if (!m_core->serialManager()->isConnected()) {
            QMessageBox::warning(this, "Not Connected", "Please connect to GRBL first.");
            return;
        }
        if (!m_core->fileManager()->isLoaded()) {
            QMessageBox::warning(this, "No File", "Please load a G-code file first.");
            return;
        }
        m_core->startStreaming(m_mainPage->toolpathPanel()->currentGcodeLines());
    });
    connect(m_mainPage->controlPanel(), &ControlPanel::pauseRequested,  this, [this]() { m_core->pauseStreaming(); });
    connect(m_mainPage->controlPanel(), &ControlPanel::resumeRequested, this, [this]() { m_core->resumeStreaming(); });
    connect(m_mainPage->controlPanel(), &ControlPanel::stopRequested,   this, [this]() { m_core->stopStreaming(); });

    // --- ControlPanel -> quick commands ---
    connect(m_mainPage->controlPanel(), &ControlPanel::homeRequested,
            this, [this]() { m_core->sendCommand("$H"); });
    connect(m_mainPage->controlPanel(), &ControlPanel::unlockRequested,
            this, [this]() { m_core->sendCommand("$X"); });
    connect(m_mainPage->controlPanel(), &ControlPanel::resetRequested,
            this, [this]() { m_core->sendRealtime(GrblRealtime::Reset); });
    connect(m_mainPage->controlPanel(), &ControlPanel::statusRequested,
            this, [this]() {
                if (m_core->serialManager()->isConnected())
                    m_core->sendRealtime(GrblRealtime::StatusReport);
            });

    // --- JogPanel -> SerialManager ---
    connect(m_mainPage->jogPanel(), &JogPanel::jogContinuousRequested,
            this, [this](const QString &axis, int dir, double feed) {
                if (m_core->serialManager()->isConnected())
                    m_core->serialManager()->jogContinuous(axis, dir, feed);
            });
    connect(m_mainPage->jogPanel(), &JogPanel::jogStepRequested,
            this, [this](const QString &axis, double dist, double feed) {
                if (m_core->serialManager()->isConnected())
                    m_core->serialManager()->jogStep(axis, dist, feed);
            });
    connect(m_mainPage->jogPanel(), &JogPanel::jogCancelRequested,
            this, [this]() {
                if (m_core->serialManager()->isConnected())
                    m_core->serialManager()->jogCancel();
            });

    // --- ConsolePanel -> SerialManager ---
    connect(m_mainPage->consolePanel(), &ConsolePanel::commandEntered,
            this, [this](const QString &cmd) {
                if (m_core->serialManager()->isConnected())
                    m_core->sendCommand(cmd);
            });

    // --- Async parse ---
    connect(m_parseWatcher, &QFutureWatcher<QList<GcodeMove>>::finished,
            this, [this]() {
                m_mainPage->toolpathPanel()->setMoves(m_parseWatcher->result());
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
        m_mainPage->toolpathPanel()->setGcodeLines(fm->lines());
        m_mainPage->controlPanel()->setFileLoaded(fm->fileName(), fm->lines().size());
        m_mainPage->consolePanel()->appendMessage("File loaded: " + fm->fileName());
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
    if (m_core->serialManager()->isConnected() && m_mainPage->jogPanel()->handleKeyPress(event))
        return;
    QMainWindow::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (m_core->serialManager()->isConnected() && m_mainPage->jogPanel()->handleKeyRelease(event))
        return;
    QMainWindow::keyReleaseEvent(event);
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
}