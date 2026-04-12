#pragma once

#include <QListView>
#include <QAbstractListModel>
#include <QStringList>

class GcodeListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit GcodeListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void setLines(const QStringList &lines);
    int lineCount() const;

private:
    QStringList m_lines;
};

class GcodeListWidget : public QListView
{
    Q_OBJECT

public:
    explicit GcodeListWidget(QWidget *parent = nullptr);

    void setLines(const QStringList &lines);
    void highlightLine(int lineNumber);
    void selectLine(int lineNumber);

signals:
    void lineClicked(int lineNumber);

private slots:
    void onItemClicked(const QModelIndex &index);

private:
    GcodeListModel *m_model;
    int m_lastHighlight = -1;
};