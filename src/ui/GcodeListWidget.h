#pragma once

#include <QListView>
#include <QAbstractListModel>
#include <QStringList>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>

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

private:
    GcodeType typeAt(int row) const;

    QStringList m_lines;
    mutable QHash<int, GcodeType> m_typeCache;  // lazy per-row type cache
    int m_executingLine = -1;
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
    bool returnToOriginEnabled() const;

signals:
    void lineClicked(int lineNumber);
    void clearRequested();

private slots:
    void onItemClicked(const QModelIndex &index);
    void onExportClicked();

private:
    GcodeListModel *m_model;
    QListView *m_listView;
    QLabel *m_countLabel;
    QPushButton *m_exportBtn;
    QPushButton *m_clearBtn;
    QCheckBox *m_returnToOriginCheck;
    int m_lastHighlight = -1;
};