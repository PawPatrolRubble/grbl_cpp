#pragma once

#include <QWidget>

namespace Ui { class ConnectionPanel; }

class ConnectionPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ConnectionPanel(QWidget *parent = nullptr);
    ~ConnectionPanel();

    void setAvailablePorts(const QStringList &ports);
    void setAvailableBaudRates(const QStringList &rates);
    void setDefaultBaudRate(const QString &rate);

public slots:
    void setConnected(bool connected);

signals:
    void connectRequested(const QString &portName, int baudRate);
    void disconnectRequested();

private:
    static QString extractPortName(const QString &portString);

    Ui::ConnectionPanel *ui;
};