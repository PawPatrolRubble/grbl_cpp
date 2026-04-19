#include "ConnectionPanel.h"
#include "ui_ConnectionPanel.h"

ConnectionPanel::ConnectionPanel(QWidget *parent)
    : QWidget(parent), ui(new Ui::ConnectionPanel)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_StyledBackground, true);

    connect(ui->connectBtn, &QPushButton::clicked, this, [this]() {
        emit connectRequested(extractPortName(ui->portCombo->currentText()),
                              ui->baudCombo->currentText().toInt());
    });
    connect(ui->disconnectBtn, &QPushButton::clicked, this, &ConnectionPanel::disconnectRequested);
}

ConnectionPanel::~ConnectionPanel()
{
    delete ui;
}

void ConnectionPanel::setAvailablePorts(const QStringList &ports)
{
    ui->portCombo->clear();
    ui->portCombo->addItems(ports);
}

void ConnectionPanel::setAvailableBaudRates(const QStringList &rates)
{
    ui->baudCombo->clear();
    ui->baudCombo->addItems(rates);
}

void ConnectionPanel::setDefaultBaudRate(const QString &rate)
{
    ui->baudCombo->setCurrentText(rate);
}

void ConnectionPanel::setConnected(bool connected)
{
    ui->connectBtn->setEnabled(!connected);
    ui->disconnectBtn->setEnabled(connected);
    ui->portCombo->setEnabled(!connected);
    ui->baudCombo->setEnabled(!connected);

    if (connected) {
        ui->connectBtn->setStyleSheet(
            "QPushButton#connectBtn { background-color: #9ca3af; border-color: #9ca3af; color: #ffffff; }"
            "QPushButton#connectBtn:hover { background-color: #9ca3af; border-color: #9ca3af; }"
            "QPushButton#connectBtn:pressed { background-color: #9ca3af; border-color: #9ca3af; }"
            "QPushButton#connectBtn:disabled { background-color: #9ca3af; border-color: #9ca3af; }");
        ui->disconnectBtn->setStyleSheet(
            "QPushButton#disconnectBtn { background-color: #22c55e; border-color: #22c55e; color: #ffffff; }"
            "QPushButton#disconnectBtn:hover { background-color: #16a34a; border-color: #16a34a; }"
            "QPushButton#disconnectBtn:pressed { background-color: #15803d; border-color: #15803d; }"
            "QPushButton#disconnectBtn:disabled { background-color: #22c55e; border-color: #22c55e; }");
    } else {
        ui->connectBtn->setStyleSheet(QString());
        ui->disconnectBtn->setStyleSheet(QString());
    }
}

QString ConnectionPanel::extractPortName(const QString &portString)
{
    int dashIdx = portString.indexOf(QLatin1String(" - "));
    if (dashIdx > 0) {
        return portString.left(dashIdx).trimmed();
    }
    return portString.trimmed();
}