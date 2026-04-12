#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QMenuBar>

class CustomTitleBar : public QWidget
{
    Q_OBJECT

public:
    explicit CustomTitleBar(QWidget *parent = nullptr);

    void setTitle(const QString &title);
    QMenuBar* menuBar() const;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

signals:
    void minimizeRequested();
    void maximizeRequested();
    void closeRequested();

private:
    QLabel *m_titleLabel;
    QMenuBar *m_menuBar;
    QPushButton *m_minimizeBtn;
    QPushButton *m_maximizeBtn;
    QPushButton *m_closeBtn;

    QPoint m_dragPosition;
    bool m_dragging;
};