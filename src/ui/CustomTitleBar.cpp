#include "CustomTitleBar.h"
#include <QHBoxLayout>
#include <QMouseEvent>

CustomTitleBar::CustomTitleBar(QWidget *parent)
    : QWidget(parent)
    , m_dragging(false)
{
    setObjectName("customTitleBar");
    setFixedHeight(32);
    setAttribute(Qt::WA_StyledBackground, true);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 0, 4, 0);
    layout->setSpacing(8);

    // App title
    m_titleLabel = new QLabel("GRBL G-Code Sender");
    m_titleLabel->setObjectName("titleBarLabel");
    layout->addWidget(m_titleLabel);

    // Menu bar embedded in title bar
    m_menuBar = new QMenuBar();
    m_menuBar->setObjectName("titleBarMenu");
    layout->addWidget(m_menuBar);

    layout->addStretch();

    // Window control buttons
    m_minimizeBtn = new QPushButton("─");
    m_minimizeBtn->setObjectName("titleBarMinBtn");
    m_minimizeBtn->setFixedSize(32, 24);
    m_minimizeBtn->setToolTip("Minimize");
    layout->addWidget(m_minimizeBtn);

    m_maximizeBtn = new QPushButton("□");
    m_maximizeBtn->setObjectName("titleBarMaxBtn");
    m_maximizeBtn->setFixedSize(32, 24);
    m_maximizeBtn->setToolTip("Maximize");
    layout->addWidget(m_maximizeBtn);

    m_closeBtn = new QPushButton("✕");
    m_closeBtn->setObjectName("titleBarCloseBtn");
    m_closeBtn->setFixedSize(40, 24);
    m_closeBtn->setToolTip("Close");
    layout->addWidget(m_closeBtn);

    connect(m_minimizeBtn, &QPushButton::clicked, this, &CustomTitleBar::minimizeRequested);
    connect(m_maximizeBtn, &QPushButton::clicked, this, &CustomTitleBar::maximizeRequested);
    connect(m_closeBtn, &QPushButton::clicked, this, &CustomTitleBar::closeRequested);
}

void CustomTitleBar::setTitle(const QString &title)
{
    m_titleLabel->setText(title);
}

QMenuBar* CustomTitleBar::menuBar() const
{
    return m_menuBar;
}

void CustomTitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragPosition = event->globalPosition().toPoint() - window()->frameGeometry().topLeft();
        event->accept();
    }
}

void CustomTitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && event->buttons() & Qt::LeftButton) {
        window()->move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    }
}

void CustomTitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit maximizeRequested();
        event->accept();
    }
}