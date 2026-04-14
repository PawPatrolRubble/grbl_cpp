#include "GraphicsDesignPage.h"
#include "panels/GraphicsDesignPanel.h"

#include <QVBoxLayout>

GraphicsDesignPage::GraphicsDesignPage(QWidget *parent)
    : QWidget(parent)
    , m_panel(new GraphicsDesignPanel(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_panel);

    // Forward gcode signal
    connect(m_panel, &GraphicsDesignPanel::gcodeGenerated,
            this, &GraphicsDesignPage::gcodeGenerated);
}