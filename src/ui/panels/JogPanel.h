#pragma once

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QDoubleSpinBox>

class QKeyEvent;

class JogPanel : public QWidget
{
    Q_OBJECT

public:
    explicit JogPanel(QWidget *parent = nullptr);

    /// Delegate keyboard jog events from MainWindow.
    /// Returns true if the event was handled.
    bool handleKeyPress(QKeyEvent *event);
    bool handleKeyRelease(QKeyEvent *event);

public slots:
    void setMotionEnabled(bool enabled);

signals:
    void jogContinuousRequested(const QString &axis, int direction, double feed);
    void jogStepRequested(const QString &axis, double distance, double feed);
    void jogCancelRequested();

private:
    enum class JogMode { Continuous, Step };

    void doJog(const QString &axis, int direction);

    JogMode m_jogMode = JogMode::Continuous;
    QComboBox *m_jogModeCombo;
    QSlider *m_jogStepSlider;
    QLabel *m_jogStepLabel;
    QDoubleSpinBox *m_jogFeedSpin;
    QList<QPushButton*> m_jogButtons;
};
