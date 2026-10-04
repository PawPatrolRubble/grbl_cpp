#pragma once

#include <QWidget>
#include <QSplitter>
#include <QVector3D>
#include <optional>

#include "grbl/GcodeParser.h"
#include "plotting/PlotJob.h"

class GcodeListWidget;
class ToolpathCanvas;
class QLabel;
class QComboBox;

class ToolpathPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ToolpathPanel(QWidget *parent = nullptr);

    void setWorktableSize(const QSizeF &size);
    void setGcodeLines(const QStringList &lines);
    void setPlotJob(const PlotJob& job);
    PlotJob currentPlotJob() const { return m_job; }
    QList<GcodeMove> previewMoves() const { return m_currentMoves; }
    void setMoves(const QList<GcodeMove> &moves);
    void setExecutingLine(int lineNumber);
    void setStreamRange(int startLine, int endLine);
    void setCutterPosition(const QVector3D &pos);
    void clearAll();
    bool returnToOriginEnabled() const;

    QStringList currentGcodeLines() const;
    int startFromLine() const;
    int rangeStart() const;
    int rangeEnd() const;

signals:
    void lineSelected(int lineNumber);
    void gcodeTransformed(const QStringList &newLines);
    void clearRequested();
    void rangeChanged(int startLine, int endLine);
    void transformRejected(const QString& reason);
    void previewReady(const QUuid& jobId);

private:
    void updatePreviewColors();
    void refreshToolFilter();
    void applyToolFilter();
    GcodeListWidget *m_listWidget;
    ToolpathCanvas *m_canvas;
    QSplitter *m_splitter;
    QLabel *m_previewLegend;
    QComboBox *m_toolFilterCombo;
    std::optional<QUuid> m_toolFilter;

    // Original data preserved for cumulative transforms
    QList<GcodeMove> m_originalMoves;
    QList<GcodeMove> m_currentMoves;
    QStringList m_currentLines;
    PlotJob m_job;
    quint64 m_previewGeneration = 0;
};
