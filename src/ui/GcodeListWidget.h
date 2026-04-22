#pragma once

#include <QListView>
#include <QAbstractListModel>
#include <QStringList>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QMenu>

enum class GcodeType { Rapid, Linear, ArcCW, ArcCCW, Other, Comment };

class GcodeListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit GcodeListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void setLines(const QStringList &lines);
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
    GcodeType typeAt(int row) const;

    QStringList m_lines;
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

    GcodeListModel *m_model;
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
};