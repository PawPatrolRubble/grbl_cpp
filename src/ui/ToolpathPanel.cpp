#include "ToolpathPanel.h"
#include "GcodeListWidget.h"
#include "ToolpathCanvas.h"
#include "TransformToolbar.h"

#include <QVBoxLayout>
#include <QHBoxLayout>

ToolpathPanel::ToolpathPanel(QWidget *parent)
    : QWidget(parent)
    , m_listWidget(new GcodeListWidget(this))
    , m_canvas(new ToolpathCanvas(this))
    , m_toolbar(new TransformToolbar(this))
    , m_splitter(new QSplitter(Qt::Horizontal, this))
{
    m_splitter->addWidget(m_listWidget);
    m_splitter->addWidget(m_canvas);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 2);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addWidget(m_toolbar, 0);
    layout->addWidget(m_splitter, 1);

    // Bidirectional sync
    connect(m_listWidget, &GcodeListWidget::lineClicked,
            m_canvas, &ToolpathCanvas::highlightSegment);
    connect(m_canvas, &ToolpathCanvas::segmentClicked,
            m_listWidget, &GcodeListWidget::selectLine);

    // Forward selection outward
    connect(m_listWidget, &GcodeListWidget::lineClicked,
            this, &ToolpathPanel::lineSelected);
    connect(m_canvas, &ToolpathCanvas::segmentClicked,
            this, &ToolpathPanel::lineSelected);

    // Transform controls (toolbar)
    connect(m_toolbar, &TransformToolbar::applyRequested, this, &ToolpathPanel::applyTransform);
    connect(m_toolbar, &TransformToolbar::resetRequested, this, &ToolpathPanel::resetTransform);

    // Interactive canvas transform (drag handles)
    connect(m_canvas, &ToolpathCanvas::movesTransformed, this, [this](const QList<GcodeMove> &newMoves) {
        m_currentMoves = newMoves;
        m_currentLines = GcodeParser::movesToGcode(m_currentMoves);
        m_listWidget->setLines(m_currentLines);
        emit gcodeTransformed(m_currentLines);
    });
}

void ToolpathPanel::setGcodeLines(const QStringList &lines)
{
    m_currentLines = lines;
    m_listWidget->setLines(lines);
}

void ToolpathPanel::setMoves(const QList<GcodeMove> &moves)
{
    m_originalMoves = moves;
    m_currentMoves = moves;
    m_canvas->setMoves(moves);
}

void ToolpathPanel::setExecutingLine(int lineNumber)
{
    m_listWidget->highlightLine(lineNumber);
    m_canvas->setExecutingLine(lineNumber);
}

void ToolpathPanel::setCutterPosition(const QVector3D &pos)
{
    m_canvas->setCutterPosition(pos);
}

QStringList ToolpathPanel::currentGcodeLines() const
{
    return m_currentLines;
}

void ToolpathPanel::applyTransform()
{
    if (m_originalMoves.isEmpty())
        return;

    // Apply transform to the ORIGINAL moves (not cumulative)
    QList<GcodeMove> transformed = GcodeParser::transformMoves(
        m_originalMoves,
        m_toolbar->scaleX(), m_toolbar->scaleY(),
        m_toolbar->offsetX(), m_toolbar->offsetY(),
        m_toolbar->rotation());

    m_currentMoves = transformed;

    // Update canvas
    m_canvas->setMoves(m_currentMoves);

    // Regenerate G-code text and update line list
    m_currentLines = GcodeParser::movesToGcode(m_currentMoves);
    m_listWidget->setLines(m_currentLines);

    emit gcodeTransformed(m_currentLines);
}

void ToolpathPanel::resetTransform()
{
    if (m_originalMoves.isEmpty())
        return;

    m_currentMoves = m_originalMoves;
    m_canvas->setMoves(m_currentMoves);

    m_currentLines = GcodeParser::movesToGcode(m_currentMoves);
    m_listWidget->setLines(m_currentLines);

    emit gcodeTransformed(m_currentLines);
}
