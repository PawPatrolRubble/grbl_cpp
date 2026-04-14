#include "MainPage.h"
#include "panels/ConnectionPanel.h"
#include "panels/StatusPanel.h"
#include "panels/ControlPanel.h"
#include "panels/JogPanel.h"
#include "panels/ConsolePanel.h"
#include "ToolpathPanel.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>

MainPage::MainPage(QWidget *parent)
    : QWidget(parent)
    , m_connection(new ConnectionPanel(this))
    , m_status(new StatusPanel(this))
    , m_control(new ControlPanel(this))
    , m_jog(new JogPanel(this))
    , m_toolpath(new ToolpathPanel(this))
    , m_console(new ConsolePanel(this))
{
    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // Left sidebar: Connection, Status, Control stacked vertically
    auto *leftSidebar = new QWidget(this);
    auto *leftLayout = new QVBoxLayout(leftSidebar);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(4);
    leftLayout->addWidget(m_connection);
    leftLayout->addWidget(m_status);
    leftLayout->addWidget(m_control);
    leftLayout->addStretch();

    // Center: Toolpath canvas + Console in splitter
    auto *centerSplitter = new QSplitter(Qt::Vertical, this);
    centerSplitter->addWidget(m_toolpath);
    centerSplitter->addWidget(m_console);
    centerSplitter->setStretchFactor(0, 3);
    centerSplitter->setStretchFactor(1, 1);

    // Right sidebar: Jog controls
    auto *rightSidebar = new QWidget(this);
    auto *rightLayout = new QVBoxLayout(rightSidebar);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(4);
    rightLayout->addWidget(m_jog);
    rightLayout->addStretch();

    // Layout with fixed-width sidebars
    leftSidebar->setFixedWidth(240);
    rightSidebar->setFixedWidth(200);

    mainLayout->addWidget(leftSidebar);
    mainLayout->addWidget(centerSplitter, 1);
    mainLayout->addWidget(rightSidebar);
}