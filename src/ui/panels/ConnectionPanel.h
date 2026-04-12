#pragma once

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>

class ConnectionPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ConnectionPanel(QWidget *parent = nullptr);

    void setAvailablePorts(const QStringList &ports);
    void setAvailableBaudRates(const QStringList &rates);
    void setDefaultBaudRate(const QString &rate);

public slots:
    void setConnected(bool connected);

signals:
    void connectRequested(const QString &portName, int baudRate);
    void disconnectRequested();

private:
    /// Extract port name from "COM3 - USB2.0-Serial" format.
    static QString extractPortName(const QString &portString);

    QComboBox *m_portCombo;
    QComboBox *m_baudCombo;
    QPushButton *m_connectBtn;
    QPushButton *m_disconnectBtn;
};
