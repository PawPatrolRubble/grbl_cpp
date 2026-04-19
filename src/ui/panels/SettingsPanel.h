#pragma once

#include <QWidget>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QMap>
#include <QTableWidget>

#include "SettingsManager.h"

class QPushButton;

class SettingsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPanel(QWidget *parent = nullptr);

    // Jog defaults
    double jogStepSize() const;
    double jogFeedRate() const;

    void setGrblSettings(const QMap<int, double> &settings);

    // Canvas settings – load from SettingsManager
    void applyCanvasSettings(const SettingsManager::CanvasSettings &s);
    SettingsManager::CanvasSettings currentCanvasSettings() const;

signals:
    void jogDefaultsChanged(double stepSize, double feedRate);
    void grblSettingWriteRequested(int id, double value);
    void grblSettingsRefreshRequested();
    void canvasSettingsChanged(const SettingsManager::CanvasSettings &settings);

private:
    void populateGrblTable(const QMap<int, double> &settings);
    static QString grblSettingDescription(int id);
    void emitCanvasChanged();

    QDoubleSpinBox *m_jogStepSpin;
    QDoubleSpinBox *m_jogFeedSpin;
    QTableWidget *m_grblTable;

    // Canvas settings widgets
    QCheckBox      *m_showRuledLines;
    QDoubleSpinBox *m_lineSpacing;
    QPushButton    *m_lineColorBtn;
    QCheckBox      *m_showMarginLine;
    QDoubleSpinBox *m_marginOffset;
    QPushButton    *m_marginColorBtn;
    QPushButton    *m_paperColorBtn;
};