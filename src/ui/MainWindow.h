#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QTextEdit>
#include <QTableWidget>
#include <QSlider>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTimer>
#include <QLineEdit>
#include <QDoubleSpinBox>

#include "grbl/MachineState.h"
#include "grbl/SerialManager.h"
#include "grbl/GrblParser.h"
#include "grbl/GcodeParser.h"
#include "grbl/FileManager.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onConnect();
    void onDisconnect();
    void onSendCommand();
    void onLoadFile();
    void onStartStreaming();
    void onPauseStreaming();
    void onResumeStreaming();
    void onStopStreaming();
    void onJogXPlus();
    void onJogXMinus();
    void onJogYPlus();
    void onJogYMinus();
    void onJogZPlus();
    void onJogZMinus();
    void onJogRelease();
    void onHome();
    void onUnlock();
    void onReset();
    void onStatusReport();

    void onResponseReceived(const QString &response);
    void onConnectedChanged(bool connected);
    void onStateUpdated(MachineState::State state);
    void onStreamingProgress(int current, int total);
    void onStreamingFinished();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    void setupUi();
    void setupMenuBar();
    void setupStatusBar();
    void createConnectionPanel(QVBoxLayout *layout);
    void createStatusPanel(QVBoxLayout *layout);
    void createControlPanel(QVBoxLayout *layout);
    void createJogPanel(QVBoxLayout *layout);
    void createEditorPanel(QVBoxLayout *layout);
    void createConsolePanel(QVBoxLayout *layout);

    void updateUiState();
    void logToConsole(const QString &msg);
    void setupConnections();

    MachineState *m_machineState;
    SerialManager *m_serialManager;
    GrblParser *m_grblParser;
    GcodeParser *m_gcodeParser;
    FileManager *m_fileManager;

    QComboBox *m_portCombo;
    QComboBox *m_baudCombo;
    QPushButton *m_connectBtn;
    QPushButton *m_disconnectBtn;
    QLabel *m_stateLabel;
    QLabel *m_mposLabel;
    QLabel *m_wposLabel;
    QLabel *m_feedLabel;
    QLabel *m_spindleLabel;
    QTextEdit *m_consoleEdit;
    QTextEdit *m_editorEdit;
    QPushButton *m_loadBtn;
    QPushButton *m_startBtn;
    QPushButton *m_pauseBtn;
    QPushButton *m_resumeBtn;
    QPushButton *m_stopBtn;
    QSlider *m_feedOverrideSlider;
    QLabel *m_feedOverrideLabel;
    QSlider *m_spindleOverrideSlider;
    QLabel *m_spindleOverrideLabel;
    QLabel *m_progressLabel;
    QSlider *m_jogStepSlider;
    QLabel *m_jogStepLabel;
    QLineEdit *m_lastCommandEdit;

    enum class JogMode { Continuous, Step };
    JogMode m_jogMode = JogMode::Continuous;
    QComboBox *m_jogModeCombo;
    QDoubleSpinBox *m_jogFeedSpin;

    QTimer *m_statusTimer;
};