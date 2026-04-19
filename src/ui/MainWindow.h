#pragma once

#include <QMainWindow>
#include <QFutureWatcher>

#include "ui_MainWindow.h"

class ApplicationCore;
class SettingsManager;
struct GcodeMove;
class ConnectionPanel;
class StatusPanel;
class ControlPanel;
class JogPanel;
class ConsolePanel;
class ToolpathPanel;
class SettingsPanel;
class GraphicsDesignPanel;
class DockPanel;
class CustomTitleBar;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void setupUi();
    void setupTitleBar();
    void setupDocks();
    void setupMenuBar();
    void setupStatusBar();
    void setupConnections();
    void saveLayout();
    void restoreLayout();
    void loadAndParseFile(const QString &filePath);
    void updateTitleBarButtons();

    /// Create a dock panel for the given widget.
    DockPanel *createDock(const QString &title, QWidget *widget,
                          Qt::DockWidgetArea area,
                          bool closable = true);

    Ui::MainWindow *ui;
    CustomTitleBar *m_titleBar;

    ApplicationCore *m_core;
    SettingsManager *m_settings;
    QFutureWatcher<QList<GcodeMove>> *m_parseWatcher;

    // Dock panels
    DockPanel *m_dockConnection;
    DockPanel *m_dockStatus;
    DockPanel *m_dockControl;
    DockPanel *m_dockJog;
    DockPanel *m_dockSettings;
    DockPanel *m_dockToolpath;
    DockPanel *m_dockConsole;
    DockPanel *m_dockDesign;

    // Inner widgets (owned by docks)
    ConnectionPanel *m_connectionPanel;
    StatusPanel     *m_statusPanel;
    ControlPanel    *m_controlPanel;
    JogPanel        *m_jogPanel;
    SettingsPanel   *m_settingsPanel;
    ToolpathPanel   *m_toolpathPanel;
    ConsolePanel    *m_consolePanel;
    GraphicsDesignPanel *m_graphicsDesignPanel;

};