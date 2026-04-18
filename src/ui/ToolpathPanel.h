#pragma once

#include <QWidget>
#include <QSplitter>
#include <QVector3D>

#include "grbl/GcodeParser.h"

class GcodeListWidget;
class ToolpathCanvas;

class ToolpathPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ToolpathPanel(QWidget *parent = nullptr);

    void setGcodeLines(const QStringList &lines);
    void setMoves(const QList<GcodeMove> &moves);
    void setExecutingLine(int lineNumber);
    void setCutterPosition(const QVector3D &pos);
    void clearAll();
    bool returnToOriginEnabled() const;

    QStringList currentGcodeLines() const;

signals:
    void lineSelected(int lineNumber);
    void gcodeTransformed(const QStringList &newLines);
    void clearRequested();

private:
    GcodeListWidget *m_listWidget;
    ToolpathCanvas *m_canvas;
    QSplitter *m_splitter;

    // Original data preserved for cumulative transforms
    QList<GcodeMove> m_originalMoves;
    QList<GcodeMove> m_currentMoves;
    QStringList m_currentLines;
};
