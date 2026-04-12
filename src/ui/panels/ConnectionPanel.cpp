#include "ConnectionPanel.h"
#include <QGroupBox>
#include <QFormLayout>
#include <QHBoxLayout>

ConnectionPanel::ConnectionPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *group = new QGroupBox("Connection");
    auto *form = new QFormLayout(group);

    m_portCombo = new QComboBox();
    m_portCombo->setEditable(true);
    form->addRow("Port:", m_portCombo);

    m_baudCombo = new QComboBox();
    form->addRow("Baud:", m_baudCombo);

    auto *btnLayout = new QHBoxLayout();
    m_connectBtn = new QPushButton("Connect");
    m_disconnectBtn = new QPushButton("Disconnect");
    m_disconnectBtn->setEnabled(false);
    btnLayout->addWidget(m_connectBtn);
    btnLayout->addWidget(m_disconnectBtn);
    form->addRow(btnLayout);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(group);

    connect(m_connectBtn, &QPushButton::clicked, this, [this]() {
        emit connectRequested(extractPortName(m_portCombo->currentText()),
                              m_baudCombo->currentText().toInt());
    });
    connect(m_disconnectBtn, &QPushButton::clicked, this, [this]() {
        emit disconnectRequested();
    });
}

void ConnectionPanel::setAvailablePorts(const QStringList &ports)
{
    m_portCombo->clear();
    m_portCombo->addItems(ports);
}

void ConnectionPanel::setAvailableBaudRates(const QStringList &rates)
{
    m_baudCombo->clear();
    m_baudCombo->addItems(rates);
}

void ConnectionPanel::setDefaultBaudRate(const QString &rate)
{
    m_baudCombo->setCurrentText(rate);
}

void ConnectionPanel::setConnected(bool connected)
{
    m_connectBtn->setEnabled(!connected);
    m_disconnectBtn->setEnabled(connected);
    m_portCombo->setEnabled(!connected);
    m_baudCombo->setEnabled(!connected);
}

QString ConnectionPanel::extractPortName(const QString &portString)
{
    // "COM3 - USB2.0-Serial" -> "COM3"
    int dashIdx = portString.indexOf(QLatin1String(" - "));
    if (dashIdx > 0) {
        return portString.left(dashIdx).trimmed();
    }
    return portString.trimmed();
}
