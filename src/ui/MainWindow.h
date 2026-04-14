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
class TabWidget;
class MainPage;

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
    void setupTabs();
    void setupMenuBar();
    void setupStatusBar();
    void setupConnections();
    void loadAndParseFile(const QString &filePath);
    void openGraphicsDesignTab();
    void closeTab(int index);

    Ui::MainWindow *ui;

    ApplicationCore *m_core;
    SettingsManager *m_settings;
    QFutureWatcher<QList<GcodeMove>> *m_parseWatcher;

    // Tab navigation
    TabWidget *m_tabs;
    MainPage *m_mainPage;
};