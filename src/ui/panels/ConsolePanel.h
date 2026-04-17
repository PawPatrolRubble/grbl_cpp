#pragma once

#include <QWidget>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QStringList>

class ConsolePanel : public QWidget
{
    Q_OBJECT

public:
    explicit ConsolePanel(QWidget *parent = nullptr);

public slots:
    void appendMessage(const QString &msg);

    /// Queue a message for batched display (non-blocking during streaming).
    void queueMessage(const QString &msg);

signals:
    void commandEntered(const QString &command);

private:
    void trimExcessLines();

    QPlainTextEdit *m_consoleEdit;
    QLineEdit *m_commandEdit;

    // Throttled batch update
    QTimer m_flushTimer;
    QStringList m_pendingMessages;
    bool m_flushPending = false;

    static constexpr int MAX_CONSOLE_LINES = 5000;
};
