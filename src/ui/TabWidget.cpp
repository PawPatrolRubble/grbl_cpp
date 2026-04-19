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
    m_pages.append(page);

    if (closable) {
        auto *closeBtn = new QPushButton("x");
        closeBtn->setFixedSize(16, 16);
        closeBtn->setFlat(true);
        m_tabBar->setTabButton(index, QTabBar::RightSide, closeBtn);

        // Connect close button - find tab by button at click time (index may have shifted)
        connect(closeBtn, &QPushButton::clicked, this, [this, closeBtn]() {
            for (int i = 0; i < m_tabBar->count(); ++i) {
                if (m_tabBar->tabButton(i, QTabBar::RightSide) == closeBtn) {
                    emit closeRequested(i);
                    break;
                }
            }
        });
    }

    emit pageAdded(index);
    return index;
}

void TabWidget::removePage(int index)
{
    // Boundary validation
    if (index < 0 || index >= m_tabBar->count()) {
        return;
    }

    QWidget *page = m_pages.at(index);
    m_stack->removeWidget(page);
    m_tabBar->removeTab(index);
    m_pages.removeAt(index);
    page->deleteLater();

    emit pageRemoved(index);
}

void TabWidget::setCurrentPage(int index)
{
    // Only set tab bar - onTabClicked slot syncs the stack
    m_tabBar->setCurrentIndex(index);
}

int TabWidget::currentPage() const { return m_tabBar->currentIndex(); }
int TabWidget::pageCount() const { return m_tabBar->count(); }
QWidget *TabWidget::pageAt(int index) const { return m_pages.value(index); }
QString TabWidget::pageTitle(int index) const { return m_tabBar->tabText(index); }

void TabWidget::onTabClicked(int index)
{
    // Use m_pages to get correct widget (prevents index mismatch)
    if (index >= 0 && index < m_pages.size()) {
        m_stack->setCurrentWidget(m_pages.at(index));
    }
    emit currentChanged(index);
}

void TabWidget::onTabCloseRequested(int index) { emit closeRequested(index); }