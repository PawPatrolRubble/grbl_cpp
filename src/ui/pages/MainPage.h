#pragma once

#include <QWidget>

class ConnectionPanel;
class StatusPanel;
class ControlPanel;
class JogPanel;
class ToolpathPanel;
class ConsolePanel;

/// Main controller page - container for all dock panels in a fixed layout.
/// Embedded as the default (non-closable) tab in TabWidget.
class MainPage : public QWidget
{
    Q_OBJECT

public:
    explicit MainPage(QWidget *parent = nullptr);

    // Panel accessors (for MainWindow to wire signals)
    ConnectionPanel *connectionPanel() const { return m_connection; }
    StatusPanel *statusPanel() const { return m_status; }
    ControlPanel *controlPanel() const { return m_control; }
    JogPanel *jogPanel() const { return m_jog; }
    ToolpathPanel *toolpathPanel() const { return m_toolpath; }
    ConsolePanel *consolePanel() const { return m_console; }

private:
    ConnectionPanel *m_connection;
    StatusPanel *m_status;
    ControlPanel *m_control;
    JogPanel *m_jog;
    ToolpathPanel *m_toolpath;
    ConsolePanel *m_console;
};