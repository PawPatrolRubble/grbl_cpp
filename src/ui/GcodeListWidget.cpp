#include "GcodeListWidget.h"
#include <QFont>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRegularExpression>
#include <QFileDialog>
#include <QMessageBox>

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

QVariant GcodeListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_lines.size())
        return {};

    const int row = index.row();
    const GcodeLine &gl = m_lines.at(row);

    if (role == Qt::DisplayRole)
        return QString("%1: %2").arg(row + 1, 5).arg(gl.text);

    if (role == Qt::ForegroundRole) {
        if (row == m_executingLine)
            return QColor("#ffcc00");
        return colorForType(gl.type);
    }

    if (role == Qt::BackgroundRole) {
        if (row == m_executingLine)
            return QColor("#3d3d00");
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
    m_lines.clear();
    m_lines.reserve(lines.size());
    m_executingLine = -1;
    for (const QString &line : lines) {
        GcodeLine gl;
        gl.text = line;
        gl.type = detectGcodeType(line);
        m_lines.append(gl);
    }
    endResetModel();
}

QStringList GcodeListModel::lines() const
{
    QStringList result;
    result.reserve(m_lines.size());
    for (const auto &gl : m_lines)
        result << gl.text;
    return result;
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
    , m_exportBtn(new QPushButton("Export"))
    , m_clearBtn(new QPushButton("Clear"))
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

    // Return to origin checkbox
    m_returnToOriginCheck->setObjectName("returnToOriginCheck");
    m_returnToOriginCheck->setToolTip("Append G90 G0 X0 Y0 Z0 when streaming to move back to work origin");

    // Setup list view
    m_listView->setModel(m_model);
    m_listView->setFont(QFont("Consolas", 10));
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

    // Layout
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addLayout(headerLayout);
    layout->addWidget(m_listView, 1);
    layout->addWidget(m_returnToOriginCheck);

    // Max width constraint
    setMaximumWidth(350);
    setMinimumWidth(200);

    connect(m_listView, &QListView::clicked, this, &GcodeListWidget::onItemClicked);
    connect(m_exportBtn, &QPushButton::clicked, this, &GcodeListWidget::onExportClicked);
    connect(m_clearBtn, &QPushButton::clicked, this, &GcodeListWidget::clearRequested);
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

    // Throttle auto-scroll: only scroll every 20 lines or on the last line
    // to avoid expensive layout recalculations that freeze the UI.
    if (lineNumber >= 0 && lineNumber < m_model->lineCount()) {
        if (lineNumber % 20 == 0 || lineNumber == m_model->lineCount() - 1) {
            QModelIndex idx = m_model->index(lineNumber, 0);
            m_listView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
        }
    }
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