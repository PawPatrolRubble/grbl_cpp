#include "ConsolePanel.h"
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>

ConsolePanel::ConsolePanel(QWidget *parent)
    : QWidget(parent)
{
    auto *group = new QGroupBox("Console");
    auto *vLayout = new QVBoxLayout(group);

    m_consoleEdit = new QTextEdit();
    m_consoleEdit->setReadOnly(true);
    m_consoleEdit->setFont(QFont("Consolas", 9));
    m_consoleEdit->setMaximumHeight(200);
    vLayout->addWidget(m_consoleEdit);

    auto *cmdLayout = new QHBoxLayout();
    m_commandEdit = new QLineEdit();
    m_commandEdit->setPlaceholderText("Send command...");
    auto *sendBtn = new QPushButton("Send");
    cmdLayout->addWidget(m_commandEdit);
    cmdLayout->addWidget(sendBtn);
    vLayout->addLayout(cmdLayout);

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(group);

    auto sendCommand = [this]() {
        QString cmd = m_commandEdit->text().trimmed();
        if (!cmd.isEmpty()) {
            emit commandEntered(cmd);
            m_commandEdit->clear();
        }
    };

    connect(sendBtn, &QPushButton::clicked, this, sendCommand);
    connect(m_commandEdit, &QLineEdit::returnPressed, this, sendCommand);
}

void ConsolePanel::appendMessage(const QString &msg)
{
    m_consoleEdit->append(msg);
}
