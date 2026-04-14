#pragma once

#include <QPlainTextEdit>
#include <QWidget>

class LineNumberPlainTextEdit : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit LineNumberPlainTextEdit(QWidget *parent = nullptr);

    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect &, int);

private:
    QWidget *m_lineNumberArea;
};