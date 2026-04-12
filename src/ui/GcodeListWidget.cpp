#include "GcodeListWidget.h"
#include <QFont>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QRegularExpression>

// G-code type detection for coloring
enum class GcodeType { Rapid, Linear, ArcCW, ArcCCW, Other, Comment };

static GcodeType detectGcodeType(const QString &line)
{
    QString stripped = line.trimmed();
    if (stripped.isEmpty() || stripped.startsWith(';') || stripped.startsWith('('))
        return GcodeType::Comment;

    static const QRegularExpression gRegex("[Gg](\\d+)");
    QRegularExpressionMatch match = gRegex.match(stripped);

    if (match.hasMatch()) {
        int code = match.captured(1).toInt();
        if (code == 0) return GcodeType::Rapid;
        if (code == 1) return GcodeType::Linear;
        if (code == 2) return GcodeType::ArcCW;
        if (code == 3) return GcodeType::ArcCCW;
    }
    return GcodeType::Other;
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

QVariant GcodeListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_lines.size())
        return {};

    const int row = index.row();
    const QString &line = m_lines.at(row);

    if (role == Qt::DisplayRole)
        return QString("%1: %2").arg(row + 1, 5).arg(line);

    if (role == Qt::ForegroundRole) {
        // Executing line - bright yellow highlight
        if (row == m_executingLine)
            return QColor("#ffcc00");  // Bright yellow

        GcodeType type = detectGcodeType(line);
        switch (type) {
        case GcodeType::Rapid:    return QColor("#f87171");  // Red - G0 rapid moves
        case GcodeType::Linear:   return QColor("#4ade80");  // Green - G1 linear moves
        case GcodeType::ArcCW:    return QColor("#60a5fa");  // Blue - G2 clockwise arcs
        case GcodeType::ArcCCW:   return QColor("#c084fc");  // Purple - G3 counter-clockwise arcs
        case GcodeType::Comment:  return QColor("#6b7280");  // Gray - comments
        default:                  return QColor("#cdd6f4");  // Default text color
        }
    }

    if (role == Qt::BackgroundRole) {
        // Executing line - highlighted background
        if (row == m_executingLine)
            return QColor("#3d3d00");  // Dark yellow background
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
    m_executingLine = -1;
    endResetModel();
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

GcodeListWidget::GcodeListWidget(QWidget *parent)
    : QWidget(parent)
    , m_model(new GcodeListModel(this))
    , m_listView(new QListView(this))
    , m_countLabel(new QLabel("0 lines", this))
{
    // Setup count label
    m_countLabel->setObjectName("gcodeCountLabel");
    m_countLabel->setAlignment(Qt::AlignCenter);
    QFont countFont = m_countLabel->font();
    countFont.setBold(true);
    m_countLabel->setFont(countFont);

    // Setup list view
    m_listView->setModel(m_model);
    m_listView->setFont(QFont("Consolas", 10));
    m_listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_listView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listView->setStyleSheet(
        "QListView {"
        "  background-color: #1e1e2e;"
        "  border: none;"
        "}"
        "QListView::item:selected {"
        "  background-color: #3366cc;"
        "}"
        "QListView::item:hover {"
        "  background-color: #313244;"
        "}"
    );

    // Layout
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addWidget(m_countLabel);
    layout->addWidget(m_listView, 1);

    // Max width constraint
    setMaximumWidth(350);
    setMinimumWidth(200);

    connect(m_listView, &QListView::clicked, this, &GcodeListWidget::onItemClicked);
}

void GcodeListWidget::setLines(const QStringList &lines)
{
    m_lastHighlight = -1;
    m_model->setLines(lines);
    m_countLabel->setText(QString("%1 lines").arg(lines.size()));
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

    // Auto-scroll to keep executing line visible
    if (lineNumber >= 0 && lineNumber < m_model->lineCount()) {
        QModelIndex idx = m_model->index(lineNumber, 0);
        m_listView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
    }
}

void GcodeListWidget::onItemClicked(const QModelIndex &index)
{
    emit lineClicked(index.row());
}