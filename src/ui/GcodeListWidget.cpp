#include "GcodeListWidget.h"
#include <QFont>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRegularExpression>
#include <QFileDialog>
#include <QMessageBox>
#include <QMenu>

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
    case GcodeType::Rapid:    return QColor("#f87171");
    case GcodeType::Linear:   return QColor("#4ade80");
    case GcodeType::ArcCW:    return QColor("#60a5fa");
    case GcodeType::ArcCCW:   return QColor("#c084fc");
    case GcodeType::Comment:  return QColor("#6b7280");
    default:                  return QColor("#cdd6f4");
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

QVariant GcodeListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_lines.size())
        return {};

    const int row = index.row();

    if (role == Qt::DisplayRole) {
        QString prefix;
        if (row == m_rangeStart)
            prefix = QStringLiteral("\u25B6 ");  // ▶
        else if (row == m_rangeEnd)
            prefix = QStringLiteral("\u25A0 ");  // ■
        return QString("%1%2: %3").arg(prefix).arg(row + 1, 5).arg(m_lines.at(row));
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
    m_typeCache.clear();
    m_executingLine = -1;
    endResetModel();
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
    m_listView->setModel(m_model);
    m_listView->setFont(QFont("Consolas", 10));
    m_listView->setUniformItemSizes(true);   // critical for 100k+ lines perf
    m_listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_listView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listView->setStyleSheet(
        "QListView {"
        "  background-color: #ffffff;"
        "  border: none;"
        "}"
        "QListView::item:selected {"
        "  background-color: #6366f1;"
        "  color: #ffffff;"
        "}"
        "QListView::item:hover {"
        "  background-color: #e5e7eb;"
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

    // Layout
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addLayout(headerLayout);
    layout->addLayout(rangeLayout);
    layout->addWidget(m_rangeLabel);
    layout->addWidget(m_listView, 1);
    layout->addWidget(m_returnToOriginCheck);

    // Max width constraint
    setMaximumWidth(350);
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
    m_lastHighlight = -1;
    m_model->setLines(lines);
    m_countLabel->setText(QString("%1 lines").arg(lines.size()));
}

QStringList GcodeListWidget::lines() const
{
    return m_model->lines();
}

void GcodeListWidget::highlightLine(int lineNumber)
{
    QModelIndex curIdx = m_model->index(lineNumber, 0);

    if (m_lastHighlight >= 0 && m_lastHighlight < m_model->lineCount()) {
        QModelIndex prevIdx = m_model->index(m_lastHighlight, 0);
        m_model->dataChanged(prevIdx, prevIdx);
    }

    if (lineNumber >= 0 && lineNumber < m_model->lineCount()) {
        if (lineNumber % 10 == 0 || lineNumber == m_model->lineCount() - 1)
            m_listView->scrollTo(curIdx, QAbstractItemView::PositionAtCenter);
    }

    m_lastHighlight = lineNumber;
}

void GcodeListWidget::selectLine(int lineNumber)
{
    if (lineNumber >= 0 && lineNumber < m_model->lineCount()) {
        QModelIndex idx = m_model->index(lineNumber, 0);
        m_listView->setCurrentIndex(idx);
        m_listView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
    }
}

void GcodeListWidget::setExecutingLine(int lineNumber)
{
    m_model->setExecutingLine(lineNumber);

    if (lineNumber >= 0 && lineNumber < m_model->lineCount()) {
        m_countLabel->setText(QString("Line %1 / %2").arg(lineNumber + 1).arg(m_model->lineCount()));
        QModelIndex idx = m_model->index(lineNumber, 0);
        m_listView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
    } else {
        m_countLabel->setText(QString("%1 lines").arg(m_model->lineCount()));
    }
}

int GcodeListWidget::selectedLine() const
{
    QModelIndex idx = m_listView->currentIndex();
    return idx.isValid() ? idx.row() : -1;
}

void GcodeListWidget::onItemClicked(const QModelIndex &index)
{
    emit lineClicked(index.row());
}

void GcodeListWidget::onExportClicked()
{
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
    return m_returnToOriginCheck->isChecked();
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

    int row = idx.row();
    QMenu menu(this);

    auto *setStart = menu.addAction(QString("Set Start (line %1)").arg(row + 1));
    auto *setEnd = menu.addAction(QString("Set End (line %1)").arg(row + 1));
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