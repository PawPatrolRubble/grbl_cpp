#pragma once

#include <QListView>
#include <QAbstractListModel>
#include <QStringList>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QMenu>
#include <QIcon>
#include <QSortFilterProxyModel>
#include <optional>
#include "plotting/PlotJob.h"
#include "grbl/ToolLibrary.h"

enum class GcodeType { Rapid, Linear, ArcCW, ArcCCW, Other, Comment };

class GcodeListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role { ToolIdRole = Qt::UserRole + 1, ToolNameRole, PreviewColorRole };

    explicit GcodeListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void setLines(const QStringList &lines);
    void setPlotJob(const PlotJob& job, const QList<ToolDefinition>& tools);
    void setToolDefinitions(const QList<ToolDefinition>& tools);
    QStringList lines() const;
    int lineCount() const;
    int executingLine() const;
    void setExecutingLine(int line);

    int rangeStart() const { return m_rangeStart; }
    int rangeEnd() const { return m_rangeEnd; }
    void setRangeStart(int line);
    void setRangeEnd(int line);
    void clearRange();

private:
    struct ToolAnnotation {
        QString name;
        QColor previewColor;
        QIcon swatch;
    };
    GcodeType typeAt(int row) const;
    QString toolLabel(int row) const;
    void cacheToolDefinitions(const QList<ToolDefinition>& tools);

    QStringList m_lines;
    QList<PlotJobBlock> m_blocks;
    QHash<QUuid, ToolAnnotation> m_tools;
    mutable QHash<int, GcodeType> m_typeCache;  // lazy per-row type cache
    int m_executingLine = -1;
    int m_rangeStart = -1;
    int m_rangeEnd = -1;
};

class GcodeListWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GcodeListWidget(QWidget *parent = nullptr);

    void setLines(const QStringList &lines);
    void setPlotJob(const PlotJob& job, const QList<ToolDefinition>& tools);
    void setToolDefinitions(const QList<ToolDefinition>& tools);
    void setToolFilter(const std::optional<QUuid>& tool);
    QStringList lines() const;
    void highlightLine(int lineNumber);
    void selectLine(int lineNumber);
    void setExecutingLine(int lineNumber);
    int selectedLine() const;
    int rangeStart() const;
    int rangeEnd() const;
    bool returnToOriginEnabled() const;

signals:
    void lineClicked(int lineNumber);
    void clearRequested();
    void rangeChanged(int startLine, int endLine);

private slots:
    void onItemClicked(const QModelIndex &index);
    void onExportClicked();
    void showContextMenu(const QPoint &pos);

private:
    void updateRangeLabel();
    void updateLineControls(const QStringList& lines);
    void updateCountLabel();
    QModelIndex viewIndexForLine(int line) const;

    GcodeListModel *m_model;
    QSortFilterProxyModel *m_filterModel;
    QListView *m_listView;
    QLabel *m_countLabel;
    QLabel *m_rangeLabel;
    QPushButton *m_exportBtn;
    QPushButton *m_clearBtn;
    QPushButton *m_setStartBtn;
    QPushButton *m_setEndBtn;
    QPushButton *m_clearRangeBtn;
    QCheckBox *m_returnToOriginCheck;
    int m_lastHighlight = -1;
    bool m_managedRack = false;
    std::optional<QUuid> m_toolFilter;
};
