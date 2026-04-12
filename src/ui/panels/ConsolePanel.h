#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>

class ConsolePanel : public QWidget
{
    Q_OBJECT

public:
    explicit ConsolePanel(QWidget *parent = nullptr);

public slots:
    void appendMessage(const QString &msg);

signals:
    void commandEntered(const QString &command);

private:
    QTextEdit *m_consoleEdit;
    QLineEdit *m_commandEdit;
};
