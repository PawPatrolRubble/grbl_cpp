#pragma once

#include <QMainWindow>
#include <QFutureWatcher>

#include "ApplicationCore.h"
#include "SettingsManager.h"
#include "grbl/GcodeParser.h"

class DockPanel;
class ConnectionPanel;
class StatusPanel;
class ControlPanel;
class JogPanel;
class ConsolePanel;
class ToolpathPanel;
class SettingsPanel;

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
    void saveLayout();
    void restoreLayout();
    void loadAndParseFile(const QString &filePath);

    /// Create a dock panel for the given widget.
    DockPanel *createDock(const QString &title, QWidget *widget,
                          Qt::DockWidgetArea area,
                          bool closable = true);

    ApplicationCore *m_core;
    SettingsManager *m_settings;
    QFutureWatcher<QList<GcodeMove>> *m_parseWatcher;

    // Dock panels
    DockPanel *m_dockConnection;
    DockPanel *m_dockStatus;
    DockPanel *m_dockControl;
    DockPanel *m_dockJog;
    DockPanel *m_dockConsole;
    DockPanel *m_dockToolpath;
    DockPanel *m_dockSettings;

    // Inner widgets (owned by docks)
    ConnectionPanel *m_connectionPanel;
    StatusPanel     *m_statusPanel;
    ControlPanel    *m_controlPanel;
    JogPanel        *m_jogPanel;
    ConsolePanel    *m_consolePanel;
    ToolpathPanel   *m_toolpathPanel;
    SettingsPanel   *m_settingsPanel;
};
