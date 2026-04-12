#include "GcodeListWidget.h"
#include <QFont>
#include <QScrollBar>

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

    if (role == Qt::DisplayRole)
        return QString("%1: %2").arg(index.row() + 1, 5).arg(m_lines.at(index.row()));

    return {};
}

void GcodeListModel::setLines(const QStringList &lines)
{
    beginResetModel();
    m_lines = lines;
    endResetModel();
}

int GcodeListModel::lineCount() const
{
    return m_lines.size();
}

GcodeListWidget::GcodeListWidget(QWidget *parent)
    : QListView(parent)
    , m_model(new GcodeListModel(this))
{
    setModel(m_model);
    setFont(QFont("Consolas", 10));
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);

    setStyleSheet(
        "QListView {"
        "  background-color: #1e1e2e;"
        "  color: #cdd6f4;"
        "  border: none;"
        "}"
        "QListView::item:selected {"
        "  background-color: #3366cc;"
        "  color: white;"
        "}"
        "QListView::item:hover {"
        "  background-color: #313244;"
        "}"
    );

    connect(this, &QListView::clicked, this, &GcodeListWidget::onItemClicked);
}

void GcodeListWidget::setLines(const QStringList &lines)
{
    m_lastHighlight = -1;
    m_model->setLines(lines);
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
            scrollTo(curIdx, QAbstractItemView::PositionAtCenter);
    }

    m_lastHighlight = lineNumber;
}

void GcodeListWidget::selectLine(int lineNumber)
{
    if (lineNumber >= 0 && lineNumber < m_model->lineCount()) {
        QModelIndex idx = m_model->index(lineNumber, 0);
        setCurrentIndex(idx);
        scrollTo(idx, QAbstractItemView::PositionAtCenter);
    }
}

void GcodeListWidget::onItemClicked(const QModelIndex &index)
{
    emit lineClicked(index.row());
}