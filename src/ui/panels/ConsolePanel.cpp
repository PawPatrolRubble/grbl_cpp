#include "ConsolePanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollBar>
#include <QTextCursor>

ConsolePanel::ConsolePanel(QWidget *parent)
    : QWidget(parent)
    , m_flushTimer(this)
{
    setObjectName("consolePanel");
    m_flushTimer.setSingleShot(true);
    m_flushTimer.setInterval(200);
    connect(&m_flushTimer, &QTimer::timeout, this, [this]() {
        if (!m_pendingMessages.isEmpty()) {
            m_consoleEdit->appendPlainText(m_pendingMessages.join("\n"));
            m_pendingMessages.clear();
            trimExcessLines();
        }
        m_flushPending = false;
    });
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

    m_consoleEdit = new QPlainTextEdit();
    m_consoleEdit->setReadOnly(true);
    m_consoleEdit->setFont(QFont("Consolas", 9));
    m_consoleEdit->setObjectName("consoleEdit");
    m_consoleEdit->setMaximumBlockCount(MAX_CONSOLE_LINES);
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
    m_consoleEdit->appendPlainText(msg);
}

void ConsolePanel::queueMessage(const QString &msg)
{
    m_pendingMessages.append(msg);
    if (!m_flushPending) {
        m_flushPending = true;
        m_flushTimer.start(200);
    }
}

void ConsolePanel::trimExcessLines()
{
    // QPlainTextEdit::setMaximumBlockCount handles this automatically,
    // but we keep this as a safety net.
    if (m_consoleEdit->document()->blockCount() > MAX_CONSOLE_LINES) {
        QTextCursor cursor = m_consoleEdit->textCursor();
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor,
                            m_consoleEdit->document()->blockCount() - MAX_CONSOLE_LINES);
        cursor.removeSelectedText();
    }
}