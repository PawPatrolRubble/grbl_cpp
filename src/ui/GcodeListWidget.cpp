#include "GcodeListWidget.h"
#include <QFont>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRegularExpression>
#include <QFileDialog>
#include <QMessageBox>
#include <QMenu>
#include <QPainter>

// G-code type detection for coloring — fast string-based, no regex
static GcodeType detectGcodeType(const QString &line)
{
    QString stripped = line.trimmed();
    if (stripped.isEmpty() || stripped.startsWith(';') || stripped.startsWith('('))
        return GcodeType::Comment;

    // Scan for G<digits> pattern
    for (int i = 0; i < stripped.size(); ++i) {
        QChar ch = stripped[i];
        if (ch == 'G' || ch == 'g') {
            // Read the integer that follows
            int j = i + 1;
            while (j < stripped.size() && stripped[j].isDigit())
                ++j;
            if (j > i + 1) {
                int code = QStringView(stripped).mid(i + 1, j - i - 1).toInt();
                if (code == 0) return GcodeType::Rapid;
                if (code == 1) return GcodeType::Linear;
                if (code == 2) return GcodeType::ArcCW;
                if (code == 3) return GcodeType::ArcCCW;
            }
        }
    }
    return GcodeType::Other;
}

static QColor colorForType(GcodeType type)
{
    switch (type) {
    case GcodeType::Rapid:    return QColor("#b91c1c");
    case GcodeType::Linear:   return QColor("#15803d");
    case GcodeType::ArcCW:    return QColor("#1d4ed8");
    case GcodeType::ArcCCW:   return QColor("#7e22ce");
    case GcodeType::Comment:  return QColor("#6b7280");
    default:                  return QColor("#334155");
    }
}

GcodeListModel::GcodeListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int GcodeListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_lines.size();
}

GcodeType GcodeListModel::typeAt(int row) const
{
    auto it = m_typeCache.find(row);
    if (it != m_typeCache.end()) return it.value();
    GcodeType t = detectGcodeType(m_lines.at(row));
    m_typeCache.insert(row, t);
    return t;
}

QString GcodeListModel::toolLabel(int row) const
{
    if (m_blocks.isEmpty()) return QStringLiteral("Unknown tool");
    const auto& block = m_blocks.at(row);
    const auto tool = m_tools.constFind(block.toolId);
    QString label;
    if (!block.toolId.isNull())
        label = tool == m_tools.cend() ? QStringLiteral("Missing tool") : tool->name;
    switch (block.kind) {
    case PlotJobBlock::Kind::Drawing:
        return label.isEmpty() ? QStringLiteral("Current tool") : label;
    case PlotJobBlock::Kind::Rack:
        return label.isEmpty() ? QStringLiteral("Rack") : QStringLiteral("Rack: %1").arg(label);
    case PlotJobBlock::Kind::Synchronization:
        return label.isEmpty() ? QStringLiteral("Wait") : QStringLiteral("Wait: %1").arg(label);
    case PlotJobBlock::Kind::Setup:
        return QStringLiteral("Setup");
    case PlotJobBlock::Kind::Finish:
        return label.isEmpty() ? QStringLiteral("Finish") : QStringLiteral("Finish: %1").arg(label);
    case PlotJobBlock::Kind::Raw:
        return label.isEmpty() ? QStringLiteral("Unknown tool") : label;
    }
    return {};
}

QVariant GcodeListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.model() != this || index.row() < 0 || index.row() >= m_lines.size())
        return {};

    const int row = index.row();
    const PlotJobBlock* block = m_blocks.isEmpty() ? nullptr : &m_blocks.at(row);
    const QUuid toolId = block ? block->toolId : QUuid{};
    const auto tool = m_tools.constFind(toolId);
    if (role == ToolIdRole) return toolId;
    if (role == ToolNameRole) return tool == m_tools.cend() ? QVariant{} : QVariant(tool->name);
    if (role == PreviewColorRole) return tool == m_tools.cend() ? QVariant{} : QVariant(tool->previewColor);
    if (role == Qt::DecorationRole) return tool == m_tools.cend() || tool->swatch.isNull() ? QVariant{} : QVariant(tool->swatch);
    if (role == Qt::ToolTipRole || role == Qt::AccessibleTextRole) {
        QStringList details;
        details << QStringLiteral("Line %1: %2").arg(row + 1).arg(toolLabel(row));
        if (tool != m_tools.cend())
            details << QStringLiteral("Tool color: %1 (%2)").arg(tool->name, tool->previewColor.name());
        else if (block && block->kind == PlotJobBlock::Kind::Drawing)
            details << QStringLiteral("Stroke color is unavailable. Assign a defined tool in Design.");
        else if (!block || block->kind == PlotJobBlock::Kind::Raw)
            details << QStringLiteral("Stroke color is unknown: this G-code has no tool assignment metadata.");
        if (block && !block->phase.isEmpty()) details << QStringLiteral("Operation: %1").arg(block->phase);
        if (block && !block->pathId.isNull()) details << QStringLiteral("Path: %1").arg(block->pathId.toString(QUuid::WithoutBraces));
        details << m_lines.at(row);
        return details.join('\n');
    }

    if (role == Qt::DisplayRole) {
        QString prefix;
        if (row == m_rangeStart)
            prefix = QStringLiteral("\u25B6 ");  // ▶
        else if (row == m_rangeEnd)
            prefix = QStringLiteral("\u25A0 ");  // ■
        return QString("%1%2: [%3] %4").arg(prefix).arg(row + 1, 5).arg(toolLabel(row), m_lines.at(row));
    }

    if (role == Qt::ForegroundRole) {
        if (row == m_executingLine)
            return QColor("#ffcc00");
        if (row == m_rangeStart)
            return QColor("#22c55e");  // green
        if (row == m_rangeEnd)
            return QColor("#ef4444");  // red
        return colorForType(typeAt(row));
    }

    if (role == Qt::BackgroundRole) {
        if (row == m_executingLine)
            return QColor("#3d3d00");
        if (row == m_rangeStart)
            return QColor("#052e16");  // dark green
        if (row == m_rangeEnd)
            return QColor("#450a0a");  // dark red
    }

    if (role == Qt::FontRole) {
        QFont font("Consolas", 10);
        if (row == m_executingLine)
            font.setBold(true);
        return font;
    }

    return {};
}

void GcodeListModel::setLines(const QStringList &lines)
{
    beginResetModel();
    m_lines = lines;
    m_blocks.clear();
    m_tools.clear();
    m_typeCache.clear();
    m_executingLine = -1;
    endResetModel();
}

void GcodeListModel::setPlotJob(const PlotJob& job, const QList<ToolDefinition>& tools)
{
    beginResetModel();
    m_lines = job.lines();
    m_blocks = job.blocks;
    cacheToolDefinitions(tools);
    m_typeCache.clear();
    m_executingLine = -1;
    endResetModel();
}

void GcodeListModel::cacheToolDefinitions(const QList<ToolDefinition>& tools)
{
    m_tools.clear();
    for (const auto& tool : tools) {
        QIcon swatch;
        if (tool.previewColor.isValid()) {
            QPixmap pixels(16, 16);
            pixels.fill(Qt::transparent);
            QPainter painter(&pixels);
            // Both borders keep black and white tools readable on any selection background.
            painter.fillRect(1, 1, 14, 14, QColor("#64748b"));
            painter.fillRect(2, 2, 12, 12, Qt::white);
            painter.fillRect(3, 3, 10, 10, tool.previewColor);
            painter.end();
            for (auto mode : {QIcon::Normal, QIcon::Disabled, QIcon::Active, QIcon::Selected})
                swatch.addPixmap(pixels, mode);
        }
        m_tools.insert(tool.id, {tool.name, tool.previewColor, swatch});
    }
}

void GcodeListModel::setToolDefinitions(const QList<ToolDefinition>& tools)
{
    cacheToolDefinitions(tools);
    if (!m_lines.isEmpty())
        emit dataChanged(index(0), index(int(m_lines.size()) - 1),
                         {Qt::DisplayRole, Qt::DecorationRole, Qt::ToolTipRole, Qt::AccessibleTextRole,
                          ToolNameRole, PreviewColorRole});
}

QStringList GcodeListModel::lines() const
{
    return m_lines;
}

int GcodeListModel::lineCount() const
{
    return m_lines.size();
}

int GcodeListModel::executingLine() const
{
    return m_executingLine;
}

void GcodeListModel::setExecutingLine(int line)
{
    int oldLine = m_executingLine;
    m_executingLine = line;

    // Emit dataChanged for old and new line to update colors
    if (oldLine >= 0 && oldLine < m_lines.size()) {
        QModelIndex oldIdx = index(oldLine, 0);
        emit dataChanged(oldIdx, oldIdx, {Qt::ForegroundRole, Qt::BackgroundRole, Qt::FontRole});
    }
    if (line >= 0 && line < m_lines.size()) {
        QModelIndex newIdx = index(line, 0);
        emit dataChanged(newIdx, newIdx, {Qt::ForegroundRole, Qt::BackgroundRole, Qt::FontRole});
    }
}

void GcodeListModel::setRangeStart(int line)
{
    int old = m_rangeStart;
    m_rangeStart = line;
    auto notify = [&](int row) {
        if (row >= 0 && row < m_lines.size()) {
            QModelIndex idx = index(row, 0);
            emit dataChanged(idx, idx, {Qt::DisplayRole, Qt::ForegroundRole, Qt::BackgroundRole});
        }
    };
    notify(old);
    notify(line);
}

void GcodeListModel::setRangeEnd(int line)
{
    int old = m_rangeEnd;
    m_rangeEnd = line;
    auto notify = [&](int row) {
        if (row >= 0 && row < m_lines.size()) {
            QModelIndex idx = index(row, 0);
            emit dataChanged(idx, idx, {Qt::DisplayRole, Qt::ForegroundRole, Qt::BackgroundRole});
        }
    };
    notify(old);
    notify(line);
}

void GcodeListModel::clearRange()
{
    int oldStart = m_rangeStart;
    int oldEnd = m_rangeEnd;
    m_rangeStart = -1;
    m_rangeEnd = -1;
    auto notify = [&](int row) {
        if (row >= 0 && row < m_lines.size()) {
            QModelIndex idx = index(row, 0);
            emit dataChanged(idx, idx, {Qt::DisplayRole, Qt::ForegroundRole, Qt::BackgroundRole});
        }
    };
    notify(oldStart);
    notify(oldEnd);
}

GcodeListWidget::GcodeListWidget(QWidget *parent)
    : QWidget(parent)
    , m_model(new GcodeListModel(this))
    , m_filterModel(new QSortFilterProxyModel(this))
    , m_listView(new QListView(this))
    , m_countLabel(new QLabel("0 lines", this))
    , m_rangeLabel(new QLabel(this))
    , m_exportBtn(new QPushButton("Export"))
    , m_clearBtn(new QPushButton("Clear"))
    , m_setStartBtn(new QPushButton("Set Start"))
    , m_setEndBtn(new QPushButton("Set End"))
    , m_clearRangeBtn(new QPushButton("Clear Range"))
    , m_returnToOriginCheck(new QCheckBox("Return to origin after streaming"))
{
    // Setup count label
    m_countLabel->setObjectName("gcodeCountLabel");
    m_countLabel->setAlignment(Qt::AlignCenter);
    m_countLabel->setWordWrap(true);
    QFont countFont = m_countLabel->font();
    countFont.setBold(true);
    m_countLabel->setFont(countFont);

    // Setup export button
    m_exportBtn->setObjectName("exportBtn");

    // Setup clear button
    m_clearBtn->setObjectName("clearGcodeBtn");
    m_clearBtn->setToolTip("Clear all loaded G-code lines and toolpath");

    // Range buttons
    m_setStartBtn->setToolTip("Set selected line as stream start");
    m_setEndBtn->setToolTip("Set selected line as stream end");
    m_clearRangeBtn->setToolTip("Clear start/end range");
    m_clearRangeBtn->setVisible(false);

    // Range label
    m_rangeLabel->setAlignment(Qt::AlignCenter);
    m_rangeLabel->setStyleSheet("color: #888; font-size: 11px;");
    m_rangeLabel->setVisible(false);

    // Return to origin checkbox
    m_returnToOriginCheck->setObjectName("returnToOriginCheck");
    m_returnToOriginCheck->setToolTip("Append G90 G0 X0 Y0 Z0 when streaming to move back to work origin");

    // Setup list view
    m_filterModel->setSourceModel(m_model);
    m_filterModel->setFilterRole(GcodeListModel::ToolIdRole);
    m_filterModel->setDynamicSortFilter(false);
    m_listView->setModel(m_filterModel);
    m_listView->setFont(QFont("Consolas", 10));
    m_listView->setIconSize(QSize(16, 16));
    m_listView->setTextElideMode(Qt::ElideNone);
    m_listView->setUniformItemSizes(true);   // critical for 100k+ lines perf
    m_listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_listView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listView->setStyleSheet(
        "QListView {"
        "  background-color: #ffffff;"
        "  border: none;"
        "  selection-background-color: #4338ca;"
        "  selection-color: #ffffff;"
        "}"
        "QListView::item:selected {"
        "  background-color: #4338ca;"
        "  color: #ffffff;"
        "}"
        "QListView::item:hover {"
        "  background-color: #e5e7eb;"
        "}"
        "QListView::item:selected:!active, QListView::item:selected:hover {"
        "  background-color: #4338ca;"
        "  color: #ffffff;"
        "}"
    );

    // Header layout (count + buttons)
    auto *headerLayout = new QHBoxLayout();
    headerLayout->addWidget(m_countLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(m_clearBtn);
    headerLayout->addWidget(m_exportBtn);

    // Range buttons row
    auto *rangeLayout = new QHBoxLayout();
    rangeLayout->addWidget(m_setStartBtn);
    rangeLayout->addWidget(m_setEndBtn);
    rangeLayout->addWidget(m_clearRangeBtn);
    rangeLayout->addStretch();

    auto* colorLegend = new QLabel("Swatch: tool color \u00b7 Text: motion type", this);
    colorLegend->setObjectName("gcodeColorLegend");
    colorLegend->setStyleSheet("color: #64748b; font-size: 11px;");
    colorLegend->setToolTip("Tool swatches come from path assignments in Design. Hover a line for the exact color and operation.");

    // Layout
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addLayout(headerLayout);
    layout->addLayout(rangeLayout);
    layout->addWidget(m_rangeLabel);
    layout->addWidget(colorLegend);
    layout->addWidget(m_listView, 1);
    layout->addWidget(m_returnToOriginCheck);

    // Max width constraint
    setMaximumWidth(450);
    setMinimumWidth(200);

    connect(m_listView, &QListView::clicked, this, &GcodeListWidget::onItemClicked);
    connect(m_exportBtn, &QPushButton::clicked, this, &GcodeListWidget::onExportClicked);
    connect(m_clearBtn, &QPushButton::clicked, this, &GcodeListWidget::clearRequested);

    // Range buttons
    connect(m_setStartBtn, &QPushButton::clicked, this, [this]() {
        int sel = selectedLine();
        if (sel >= 0) {
            m_model->setRangeStart(sel);
            updateRangeLabel();
            emit rangeChanged(m_model->rangeStart(), m_model->rangeEnd());
        }
    });
    connect(m_setEndBtn, &QPushButton::clicked, this, [this]() {
        int sel = selectedLine();
        if (sel >= 0) {
            m_model->setRangeEnd(sel);
            updateRangeLabel();
            emit rangeChanged(m_model->rangeStart(), m_model->rangeEnd());
        }
    });
    connect(m_clearRangeBtn, &QPushButton::clicked, this, [this]() {
        m_model->clearRange();
        updateRangeLabel();
        emit rangeChanged(-1, -1);
    });

    // Right-click context menu for setting start/end range
    m_listView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_listView, &QListView::customContextMenuRequested, this, &GcodeListWidget::showContextMenu);
}

void GcodeListWidget::setLines(const QStringList &lines)
{
    updateLineControls(lines);
    m_model->setLines(lines);
    updateCountLabel();
}

void GcodeListWidget::setPlotJob(const PlotJob& job, const QList<ToolDefinition>& tools)
{
    updateLineControls(job.lines());
    m_model->setPlotJob(job, tools);
    updateCountLabel();
}

void GcodeListWidget::setToolDefinitions(const QList<ToolDefinition>& tools)
{
    m_model->setToolDefinitions(tools);
}

void GcodeListWidget::setToolFilter(const std::optional<QUuid>& tool)
{
    if (m_toolFilter == tool) return;
    const int selected = selectedLine();
    m_toolFilter = tool;
    m_filterModel->setFilterFixedString(tool ? tool->toString() : QString{});
    m_listView->selectionModel()->clearSelection();
    m_listView->setCurrentIndex(viewIndexForLine(selected));
    updateCountLabel();
}

QModelIndex GcodeListWidget::viewIndexForLine(int line) const
{
    return m_filterModel->mapFromSource(m_model->index(line, 0));
}

void GcodeListWidget::updateCountLabel()
{
    const int total = m_model->lineCount();
    const QString count = m_toolFilter ? QString("%1 / %2 lines visible").arg(m_filterModel->rowCount()).arg(total)
                                     : QString("%1 lines").arg(total);
    const int executing = m_model->executingLine();
    if (executing >= 0 && executing < total) {
        QString progress = QString("Line %1 / %2").arg(executing + 1).arg(total);
        if (m_toolFilter) {
            progress += '\n' + count;
            if (!viewIndexForLine(executing).isValid()) progress += "\nHidden by tool filter";
        }
        m_countLabel->setText(progress);
    } else m_countLabel->setText(count);
}

void GcodeListWidget::updateLineControls(const QStringList& lines)
{
    m_managedRack = false;
    for (const auto& line : lines) m_managedRack = m_managedRack || line.contains("GRBL_CPP_MANAGED_RACK");
    for (auto* button : {m_exportBtn, m_setStartBtn, m_setEndBtn}) button->setEnabled(!m_managedRack);
    m_returnToOriginCheck->setEnabled(!m_managedRack);
    const QString restriction = m_managedRack ? "Automatic rack jobs execute as a complete managed program. Regenerate in Design to change geometry or finish policy." : QString{};
    for (auto* button : {m_exportBtn, m_setStartBtn, m_setEndBtn}) button->setToolTip(restriction);
    m_returnToOriginCheck->setToolTip(restriction);
    m_lastHighlight = -1;
}

QStringList GcodeListWidget::lines() const
{
    return m_model->lines();
}

void GcodeListWidget::highlightLine(int lineNumber)
{
    QModelIndex curIdx = viewIndexForLine(lineNumber);

    if (m_lastHighlight >= 0 && m_lastHighlight < m_model->lineCount()) {
        QModelIndex prevIdx = m_model->index(m_lastHighlight, 0);
        m_model->dataChanged(prevIdx, prevIdx);
    }

    if (curIdx.isValid()) {
        if (lineNumber % 10 == 0 || lineNumber == m_model->lineCount() - 1)
            m_listView->scrollTo(curIdx, QAbstractItemView::PositionAtCenter);
    }

    m_lastHighlight = lineNumber;
}

void GcodeListWidget::selectLine(int lineNumber)
{
    const QModelIndex idx = viewIndexForLine(lineNumber);
    m_listView->setCurrentIndex(idx);
    if (idx.isValid()) {
        m_listView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
    }
}

void GcodeListWidget::setExecutingLine(int lineNumber)
{
    m_model->setExecutingLine(lineNumber);
    updateCountLabel();
    const QModelIndex idx = viewIndexForLine(lineNumber);
    if (idx.isValid()) {
        m_listView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
    }
}

int GcodeListWidget::selectedLine() const
{
    QModelIndex idx = m_filterModel->mapToSource(m_listView->currentIndex());
    return idx.isValid() ? idx.row() : -1;
}

void GcodeListWidget::onItemClicked(const QModelIndex &index)
{
    const auto source = m_filterModel->mapToSource(index);
    if (source.isValid()) emit lineClicked(source.row());
}

void GcodeListWidget::onExportClicked()
{
    if (m_managedRack) return;
    QString file = QFileDialog::getSaveFileName(this, "Export G-code",
        QString(), "G-code Files (*.nc *.ngc *.gcode *.txt)");
    if (!file.isEmpty()) {
        QFile f(file);
        if (f.open(QIODevice::WriteOnly)) {
            for (const QString &line : m_model->lines()) {
                f.write(line.toUtf8());
                f.write("\r\n");
            }
            f.close();
            QMessageBox::information(this, "Export Complete",
                QString("Exported %1 lines to %2").arg(m_model->lineCount()).arg(file));
        } else {
            QMessageBox::warning(this, "Export Failed", "Could not write file.");
        }
    }
}

bool GcodeListWidget::returnToOriginEnabled() const
{
    return !m_managedRack && m_returnToOriginCheck->isChecked();
}

int GcodeListWidget::rangeStart() const
{
    return m_model->rangeStart();
}

int GcodeListWidget::rangeEnd() const
{
    return m_model->rangeEnd();
}

void GcodeListWidget::showContextMenu(const QPoint &pos)
{
    QModelIndex idx = m_listView->indexAt(pos);
    if (!idx.isValid()) return;

    int row = m_filterModel->mapToSource(idx).row();
    QMenu menu(this);

    auto *setStart = menu.addAction(QString("Set Start (line %1)").arg(row + 1));
    auto *setEnd = menu.addAction(QString("Set End (line %1)").arg(row + 1));
    setStart->setEnabled(!m_managedRack);
    setEnd->setEnabled(!m_managedRack);
    menu.addSeparator();
    auto *clearRange = menu.addAction("Clear Range");

    QAction *chosen = menu.exec(m_listView->viewport()->mapToGlobal(pos));
    if (chosen == setStart) {
        m_model->setRangeStart(row);
        updateRangeLabel();
        emit rangeChanged(m_model->rangeStart(), m_model->rangeEnd());
    } else if (chosen == setEnd) {
        m_model->setRangeEnd(row);
        updateRangeLabel();
        emit rangeChanged(m_model->rangeStart(), m_model->rangeEnd());
    } else if (chosen == clearRange) {
        m_model->clearRange();
        updateRangeLabel();
        emit rangeChanged(-1, -1);
    }
}

void GcodeListWidget::updateRangeLabel()
{
    int start = m_model->rangeStart();
    int end = m_model->rangeEnd();
    bool hasRange = (start >= 0 || end >= 0);

    m_rangeLabel->setVisible(hasRange);
    m_clearRangeBtn->setVisible(hasRange);

    if (hasRange) {
        QString startStr = start >= 0 ? QString::number(start + 1) : "1";
        QString endStr = end >= 0 ? QString::number(end + 1) : QString::number(m_model->lineCount());
        m_rangeLabel->setText(QString("Range: line %1 → %2").arg(startStr, endStr));
    }
}
