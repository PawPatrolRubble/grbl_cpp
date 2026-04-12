#include "ConsolePanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

ConsolePanel::ConsolePanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("consolePanel");
    setAttribute(Qt::WA_StyledBackground, true);
    auto *vLayout = new QVBoxLayout(this);
    vLayout->setContentsMargins(0, 0, 0, 0);
    vLayout->setSpacing(0);

    // Title bar matching dock style
    auto *titleLabel = new QLabel("Console");
    titleLabel->setObjectName("consoleTitle");
    vLayout->addWidget(titleLabel);

    // Console content area
    auto *contentLayout = new QVBoxLayout();
    contentLayout->setContentsMargins(8, 8, 8, 8);
    contentLayout->setSpacing(4);

    m_consoleEdit = new QTextEdit();
    m_consoleEdit->setReadOnly(true);
    m_consoleEdit->setFont(QFont("Consolas", 9));
    m_consoleEdit->setObjectName("consoleEdit");
    contentLayout->addWidget(m_consoleEdit);

    auto *cmdLayout = new QHBoxLayout();
    m_commandEdit = new QLineEdit();
    m_commandEdit->setPlaceholderText("Send command...");
    auto *sendBtn = new QPushButton("Send");
    cmdLayout->addWidget(m_commandEdit);
    cmdLayout->addWidget(sendBtn);
    contentLayout->addLayout(cmdLayout);

    vLayout->addLayout(contentLayout);

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