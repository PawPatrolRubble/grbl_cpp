#pragma once

#include <QMainWindow>
#include <QSplitter>
#include <QFutureWatcher>

#include "ApplicationCore.h"
#include "grbl/GcodeParser.h"

class ConnectionPanel;
class StatusPanel;
class ControlPanel;
class JogPanel;
class ConsolePanel;
class ToolpathPanel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    void setupUi();
    void setupMenuBar();
    void setupStatusBar();
    void setupConnections();
    void loadAndParseFile(const QString &filePath);

    ApplicationCore *m_core;
    QFutureWatcher<QList<GcodeMove>> *m_parseWatcher;

    // UI panels only — no backend services
    ConnectionPanel *m_connectionPanel;
    StatusPanel     *m_statusPanel;
    ControlPanel    *m_controlPanel;
    JogPanel        *m_jogPanel;
    ConsolePanel    *m_consolePanel;
    ToolpathPanel   *m_toolpathPanel;
};
