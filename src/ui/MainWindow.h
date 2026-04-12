#pragma once

#include <QMainWindow>
#include "ToolpathPanel.h"
#include <QSplitter>
#include <QFutureWatcher>

#include "grbl/MachineState.h"
#include "grbl/SerialManager.h"
#include "grbl/GcodeStreamer.h"
#include "grbl/GrblParser.h"
#include "grbl/GcodeParser.h"
#include "grbl/FileManager.h"

class ConnectionPanel;
class StatusPanel;
class ControlPanel;
class JogPanel;
class ConsolePanel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    void setupUi();
    void setupMenuBar();
    void setupStatusBar();
    void setupConnections();
    void loadAndParseFile(const QString &filePath);

private slots:
    void onParseFinished();

private:

    // Backend services
    MachineState *m_machineState;
    SerialManager *m_serialManager;
    GcodeStreamer *m_streamer;
    GrblParser *m_grblParser;
    GcodeParser *m_gcodeParser;
    FileManager *m_fileManager;
    QFutureWatcher<QList<GcodeMove>> *m_parseWatcher;

    // UI panels
    ConnectionPanel *m_connectionPanel;
    StatusPanel *m_statusPanel;
    ControlPanel *m_controlPanel;
    JogPanel *m_jogPanel;
    ConsolePanel *m_consolePanel;
    ToolpathPanel *m_toolpathPanel;
};