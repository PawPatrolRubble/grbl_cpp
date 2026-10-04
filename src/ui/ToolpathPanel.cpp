#include "ToolpathPanel.h"
#include "GcodeListWidget.h"
#include "ToolpathCanvas.h"
#include "plotting/PlotJobTransform.h"
#include "SettingsManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFutureWatcher>
#include <QLabel>
#include <QComboBox>
#include <QSignalBlocker>
#include <QtConcurrent>

ToolpathPanel::ToolpathPanel(QWidget *parent)
    : QWidget(parent)
    , m_listWidget(new GcodeListWidget(this))
    , m_canvas(new ToolpathCanvas(this))
    , m_splitter(new QSplitter(Qt::Horizontal, this))
    , m_previewLegend(new QLabel(this))
    , m_toolFilterCombo(new QComboBox(this))
{
    m_splitter->addWidget(m_listWidget);
    auto* preview = new QWidget(this);
    auto* previewLayout = new QVBoxLayout(preview);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewLayout->setSpacing(2);
    m_previewLegend->setObjectName("toolpathColorLegend");
    m_previewLegend->setWordWrap(true);
    m_previewLegend->setStyleSheet("color: #64748b; font-size: 11px;");
    m_previewLegend->setToolTip("Defined tools use their saved colors. Unassigned or missing tools use blue for straight strokes and green for arcs. White and pale tools have an outline.");
    previewLayout->addWidget(m_previewLegend);
    previewLayout->addWidget(m_canvas, 1);
    m_splitter->addWidget(preview);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 2);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    auto* filterLayout = new QHBoxLayout;
    auto* filterLabel = new QLabel("Show tool:", this);
    filterLabel->setBuddy(m_toolFilterCombo);
    m_toolFilterCombo->setObjectName("toolpathToolFilter");
    m_toolFilterCombo->setAccessibleName("Show tool");
    m_toolFilterCombo->setMinimumContentsLength(22);
    m_toolFilterCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_toolFilterCombo->setToolTip("Filter the list and drawing preview by tool. Original G-code line numbers and execution are preserved.");
    auto* filterHint = new QLabel("View only \u00b7 does not change execution", this);
    filterHint->setStyleSheet("color: #64748b; font-size: 11px;");
    filterLayout->addWidget(filterLabel);
    filterLayout->addWidget(m_toolFilterCombo);
    filterLayout->addStretch();
    filterLayout->addWidget(filterHint);
    layout->addLayout(filterLayout);
    layout->addWidget(m_splitter, 1);

    connect(m_toolFilterCombo, &QComboBox::currentIndexChanged, this, [this] {
        const auto value = m_toolFilterCombo->currentData();
        m_toolFilter = value.isValid() ? std::optional<QUuid>(value.toUuid()) : std::nullopt;
        applyToolFilter();
    });

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

    // Interactive canvas transform (drag handles)
    connect(m_canvas, &ToolpathCanvas::movesTransformed, this, [this](const QList<GcodeMove> &newMoves) {
        const auto result = PlotJobTransform::apply(m_job, newMoves);
        if (!result.valid()) {
            m_canvas->setMovesNoFit(m_currentMoves);
            emit transformRejected(result.error);
            return;
        }
        m_job = result.job;
        ++m_previewGeneration;
        m_currentLines = m_job.lines();
        GcodeParser parser;
        m_currentMoves = parser.parse(m_currentLines);
        m_listWidget->setPlotJob(m_job, SettingsManager::instance().toolLibrary()->tools());
        m_canvas->setMovesNoFit(m_currentMoves);
        updatePreviewColors();
        emit gcodeTransformed(m_currentLines);
    });

    // Clear G-code
    connect(m_listWidget, &GcodeListWidget::clearRequested,
            this, &ToolpathPanel::clearRequested);

    // Forward range changes
    connect(m_listWidget, &GcodeListWidget::rangeChanged,
            this, &ToolpathPanel::rangeChanged);

    auto* tools = SettingsManager::instance().toolLibrary();
    connect(tools, &ToolLibrary::changed, this, [this, tools] {
        m_listWidget->setToolDefinitions(tools->tools());
        updatePreviewColors();
    });
    updatePreviewColors();
}

void ToolpathPanel::updatePreviewColors()
{
    QHash<QUuid, QColor> toolColors;
    for (const auto& tool : SettingsManager::instance().toolLibrary()->tools())
        if (tool.previewColor.isValid()) toolColors.insert(tool.id, tool.previewColor);
    QHash<int, QColor> lineColors;
    for (int row = 0; row < m_job.blocks.size(); ++row) {
        const auto& block = m_job.blocks[row];
        if (block.kind != PlotJobBlock::Kind::Drawing) continue;
        const auto tool = toolColors.constFind(block.toolId);
        if (tool != toolColors.cend()) lineColors.insert(row, *tool);
    }
    m_canvas->setStrokeColors(lineColors);
    const QString strokes = lineColors.isEmpty() ? "Blue straight strokes / green arcs" : "Strokes: assigned tool colors";
    m_previewLegend->setText(strokes + " \u00b7 Orange dashes: travel \u00b7 Yellow: highlight \u00b7 Gray: completed / outside range");
    refreshToolFilter();
}

void ToolpathPanel::refreshToolFilter()
{
    const QSignalBlocker blocker(m_toolFilterCombo);
    m_toolFilterCombo->clear();
    m_toolFilterCombo->addItem("All tools");
    auto* tools = SettingsManager::instance().toolLibrary();
    QSet<QUuid> used;
    QList<QUuid> toolIds;
    QHash<QUuid, QString> labels;
    QHash<QString, int> labelCounts;
    bool hasUnassigned = false;
    for (const auto& block : m_job.blocks) {
        if (block.toolId.isNull()) { hasUnassigned = true; continue; }
        if (used.contains(block.toolId)) continue;
        used.insert(block.toolId);
        toolIds << block.toolId;
        const auto* tool = tools->tool(block.toolId);
        const QString shortId = block.toolId.toString(QUuid::WithoutBraces).left(8);
        const QString label = tool ? QString("%1 (%2)").arg(tool->name, tool->previewColor.name())
                                  : QString("Missing tool (%1)").arg(shortId);
        labels.insert(block.toolId, label);
        ++labelCounts[label];
    }
    for (const auto& id : toolIds) {
        QString label = labels.value(id);
        if (labelCounts.value(label) > 1)
            label += QString(" [%1]").arg(id.toString(QUuid::WithoutBraces).left(8));
        m_toolFilterCombo->addItem(label, QVariant::fromValue(id));
        m_toolFilterCombo->setItemData(m_toolFilterCombo->count() - 1,
                                     QString("Tool ID: %1").arg(id.toString()), Qt::ToolTipRole);
    }
    if (hasUnassigned)
        m_toolFilterCombo->addItem("Unassigned / setup", QVariant::fromValue(QUuid{}));
    const int selected = m_toolFilter ? m_toolFilterCombo->findData(QVariant::fromValue(*m_toolFilter)) : 0;
    if (selected < 0) m_toolFilter.reset();
    m_toolFilterCombo->setCurrentIndex(selected < 0 ? 0 : selected);
    m_toolFilterCombo->setEnabled(!m_job.blocks.isEmpty());
    applyToolFilter();
}

void ToolpathPanel::applyToolFilter()
{
    m_listWidget->setToolFilter(m_toolFilter);
    QSet<int> visibleLines;
    if (m_toolFilter) {
        for (int row = 0; row < m_job.blocks.size(); ++row)
            if (m_job.blocks[row].toolId == *m_toolFilter) visibleLines.insert(row);
    }
    m_canvas->setLineFilter(m_toolFilter.has_value(), visibleLines);
    const QString reason = PlotJobTransform::unsupportedReason(m_job);
    m_canvas->setTransformEnabled(!m_toolFilter && reason.isEmpty());
    m_canvas->setToolTip(m_toolFilter ? "Choose All tools to transform the complete drawing." : reason);
}

void ToolpathPanel::setWorktableSize(const QSizeF &size)
{
    m_canvas->setWorktableSize(size);
}

void ToolpathPanel::setGcodeLines(const QStringList &lines)
{
    setPlotJob(PlotJob::fromLines(lines));
}

void ToolpathPanel::setPlotJob(const PlotJob& job)
{
    m_toolFilter.reset();
    m_job = job;
    m_originalMoves.clear();
    m_currentMoves.clear();
    m_canvas->setMoves({});
    m_currentLines = job.lines();
    m_listWidget->setPlotJob(job, SettingsManager::instance().toolLibrary()->tools());
    updatePreviewColors();
    const auto generation = ++m_previewGeneration;
    const auto lines = job.drawingPreviewLines();
    auto* watcher = new QFutureWatcher<QList<GcodeMove>>(this);
    connect(watcher, &QFutureWatcher<QList<GcodeMove>>::finished, this, [this, watcher, generation] {
        if (generation == m_previewGeneration) {
            m_currentMoves = watcher->result();
            m_originalMoves = m_currentMoves;
            m_canvas->setMoves(m_currentMoves);
            updatePreviewColors();
            emit previewReady(m_job.id);
        }
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([lines] {
        GcodeParser parser;
        return parser.parse(lines);
    }));
}

void ToolpathPanel::setMoves(const QList<GcodeMove> &moves)
{
    ++m_previewGeneration;
    m_originalMoves = moves;
    m_currentMoves = moves;
    m_canvas->setMoves(moves);
    updatePreviewColors();
}

void ToolpathPanel::setExecutingLine(int lineNumber)
{
    m_listWidget->setExecutingLine(lineNumber);
    m_canvas->setExecutingLine(lineNumber);
}

void ToolpathPanel::setStreamRange(int startLine, int endLine)
{
    m_canvas->setStreamRange(startLine, endLine);
}

void ToolpathPanel::setCutterPosition(const QVector3D &pos)
{
    m_canvas->setCutterPosition(pos);
}

QStringList ToolpathPanel::currentGcodeLines() const
{
    return m_currentLines;
}

int ToolpathPanel::startFromLine() const
{
    return m_listWidget->selectedLine();
}

int ToolpathPanel::rangeStart() const
{
    return m_listWidget->rangeStart();
}

int ToolpathPanel::rangeEnd() const
{
    return m_listWidget->rangeEnd();
}

void ToolpathPanel::clearAll()
{
    m_toolFilter.reset();
    ++m_previewGeneration;
    m_originalMoves.clear();
    m_currentMoves.clear();
    m_currentLines.clear();
    m_job = PlotJob{};
    m_listWidget->setLines({});
    m_canvas->setMoves({});
    updatePreviewColors();
}

bool ToolpathPanel::returnToOriginEnabled() const
{
    return m_listWidget->returnToOriginEnabled();
}
