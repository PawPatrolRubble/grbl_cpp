#pragma once

#include <QWidget>
#include <QTabBar>
#include <QStackedWidget>

/// Custom tab widget combining tab bar with stacked page container.
/// Main tab (index 0) is non-closable. Other tabs have close buttons.
class TabWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TabWidget(QWidget *parent = nullptr);

    int addPage(const QString &title, QWidget *page, bool closable = true);
    void removePage(int index);
    void setCurrentPage(int index);
    int currentPage() const;
    int pageCount() const;
    QWidget *pageAt(int index) const;
    QString pageTitle(int index) const;

signals:
    void pageAdded(int index);
    void pageRemoved(int index);
    void currentChanged(int index);
    void closeRequested(int index);

private slots:
    void onTabClicked(int index);
    void onTabCloseRequested(int index);

private:
    QTabBar *m_tabBar;
    QStackedWidget *m_stack;
};