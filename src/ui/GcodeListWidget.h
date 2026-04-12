#pragma once

#include <QListView>
#include <QAbstractListModel>
#include <QStringList>
#include <QLabel>

class GcodeListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit GcodeListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void setLines(const QStringList &lines);
    int lineCount() const;
    int executingLine() const;
    void setExecutingLine(int line);

private:
    QStringList m_lines;
    int m_executingLine = -1;
};

class GcodeListWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GcodeListWidget(QWidget *parent = nullptr);

    void setLines(const QStringList &lines);
    void highlightLine(int lineNumber);
    void selectLine(int lineNumber);
    void setExecutingLine(int lineNumber);

signals:
    void lineClicked(int lineNumber);

private slots:
    void onItemClicked(const QModelIndex &index);

private:
    GcodeListModel *m_model;
    QListView *m_listView;
    QLabel *m_countLabel;
    int m_lastHighlight = -1;
};