#include "ToolBarWidget.h"

#include <QVBoxLayout>

ToolBarWidget::ToolBarWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("sketchToolBar");
    setFixedWidth(44);
    
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);
    
    // Tool definitions: icon text, tooltip, shortcut
    struct ToolDef {
        const char *icon;
        const char *tooltip;
        const char *shortcut;
    };
    
    static const ToolDef tools[ToolCount] = {
        {"↖", "Select entities (Esc)", "Esc"},
        {"∕", "Line tool (L)", "L"},
        {"⌇", "Polyline tool (P)", "P"},
        {"▭", "Rectangle tool (R)", "R"},
        {"○", "Circle tool (C)", "C"},
        {"◠", "Arc tool (A)", "A"},
        {"T", "Text tool (T)", "Ctrl+T"},
        {"↔", "Linear dimension (D)", "D"},
        {"∠", "Angular dimension", "Ctrl+D"},
        {"⦿", "Radius dimension", "Ctrl+R"},
        {"✂", "Trim tool (X)", "X"},
        {"⌁", "Extend tool (E)", "E"},
        {"∟", "Chamfer tool (H)", "H"},
        {"⌒", "Fillet tool (F)", "F"},
        {"∥", "Offset tool (O)", "O"},
        {"⌖", "Measure distance/angle (M)", "M"},
        {"⌣", "Spline tool (B)", "B"},
    };
    
    for (int i = 0; i < ToolCount; ++i) {
        auto *btn = new QToolButton();
        btn->setText(tools[i].icon);
        btn->setToolTip(QString("%1\n%2").arg(tools[i].tooltip, tools[i].shortcut));
        btn->setCheckable(true);
        btn->setAutoRaise(true);
        btn->setFixedSize(36, 36);
        btn->setStyleSheet(
            "QToolButton {"
            "  background: #ffffff;"
            "  border: 1px solid #d0d0d0;"
            "  border-radius: 4px;"
            "  font-size: 18px;"
            "}"
            "QToolButton:hover {"
            "  background: #e8f0f8;"
            "  border-color: #a0c4e0;"
            "}"
            "QToolButton:checked {"
            "  background: #dce8f4;"
            "  border: 2px solid #5090c0;"
            "  color: #205080;"
            "}"
        );
        
        connect(btn, &QToolButton::clicked, this, [this, i]() {
            emit toolSelected(i);
        });
        
        m_buttons[i] = btn;
        layout->addWidget(btn);
    }
    
    layout->addStretch();
    
    // Set first button (Select) as checked by default
    m_buttons[Select]->setChecked(true);
}

void ToolBarWidget::setToolChecked(int toolIndex)
{
    for (int i = 0; i < ToolCount; ++i) {
        m_buttons[i]->setChecked(i == toolIndex);
    }
}