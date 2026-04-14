#include "TabWidget.h"
#include <QPushButton>
#include <QVBoxLayout>

TabWidget::TabWidget(QWidget *parent)
    : QWidget(parent)
    , m_tabBar(new QTabBar(this))
    , m_stack(new QStackedWidget(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(m_tabBar);
    layout->addWidget(m_stack);

    m_tabBar->setExpanding(false);
    m_tabBar->setTabsClosable(false);
    m_tabBar->setMovable(false);
    m_tabBar->setDocumentMode(true);

    connect(m_tabBar, &QTabBar::currentChanged, this, &TabWidget::onTabClicked);
    connect(m_tabBar, &QTabBar::tabCloseRequested, this, &TabWidget::onTabCloseRequested);
}

int TabWidget::addPage(const QString &title, QWidget *page, bool closable)
{
    int index = m_tabBar->addTab(title);
    m_stack->addWidget(page);

    if (closable) {
        auto *closeBtn = new QPushButton("×");
        closeBtn->setFixedSize(16, 16);
        closeBtn->setFlat(true);
        m_tabBar->setTabButton(index, QTabBar::RightSide, closeBtn);
    }

    emit pageAdded(index);
    return index;
}

void TabWidget::removePage(int index)
{
    QWidget *page = m_stack->widget(index);
    m_stack->removeWidget(page);
    m_tabBar->removeTab(index);
    page->deleteLater();
    emit pageRemoved(index);
}

void TabWidget::setCurrentPage(int index)
{
    m_tabBar->setCurrentIndex(index);
    m_stack->setCurrentIndex(index);
}

int TabWidget::currentPage() const { return m_tabBar->currentIndex(); }
int TabWidget::pageCount() const { return m_tabBar->count(); }
QWidget *TabWidget::pageAt(int index) const { return m_stack->widget(index); }
QString TabWidget::pageTitle(int index) const { return m_tabBar->tabText(index); }

void TabWidget::onTabClicked(int index)
{
    m_stack->setCurrentIndex(index);
    emit currentChanged(index);
}

void TabWidget::onTabCloseRequested(int index) { emit closeRequested(index); }