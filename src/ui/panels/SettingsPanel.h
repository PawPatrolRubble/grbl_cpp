#pragma once

#include <QWidget>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QMap>
#include <QTableWidget>

class SettingsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPanel(QWidget *parent = nullptr);

    // Jog defaults
    double jogStepSize() const;
    double jogFeedRate() const;

    void setGrblSettings(const QMap<int, double> &settings);

signals:
    void jogDefaultsChanged(double stepSize, double feedRate);
    void grblSettingWriteRequested(int id, double value);
    void grblSettingsRefreshRequested();

private:
    void populateGrblTable(const QMap<int, double> &settings);
    static QString grblSettingDescription(int id);

    QDoubleSpinBox *m_jogStepSpin;
    QDoubleSpinBox *m_jogFeedSpin;
    QTableWidget *m_grblTable;
};