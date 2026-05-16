#include "ToolBarWidget.h"

#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>

namespace {
void addGroupLabel(QVBoxLayout *layout, const QString &text)
{
    auto *label = new QLabel(text);
    label->setAlignment(Qt::AlignCenter);
    label->setToolTip(text);
    label->setStyleSheet(
        "QLabel {"
        "  color: #667085;"
        "  font-size: 9px;"
        "  font-weight: 600;"
        "  padding: 5px 0 1px 0;"
        "}"
    );
    layout->addWidget(label);
}

void addGroupSeparator(QVBoxLayout *layout)
{
    auto *separator = new QFrame();
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Plain);
    separator->setFixedHeight(1);
    separator->setStyleSheet("QFrame { color: #d0d5dd; background: #d0d5dd; margin: 4px 6px; }");
    layout->addWidget(separator);
}
} // namespace

ToolBarWidget::ToolBarWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("sketchToolBar");
    setFixedWidth(54);
    
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);
    
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
        {"∠", "Angular dimension (M)", "M"},
        {"⦿", "Radius dimension", "Ctrl+R"},
        {"⇢", "Move tool (G)", "G"},
        {"⧉", "Copy tool (Y)", "Y"},
        {"⟳", "Rotate tool (Q)", "Q"},
        {"✂", "Trim tool (X)", "X"},
        {"⌁", "Extend tool (E)", "E"},
        {"∟", "Chamfer tool (H)", "H"},
        {"⌒", "Fillet tool (F)", "F"},
        {"∥", "Offset tool (O)", "O"},
        {"⌖", "Legacy measure slot", ""},
        {"⌣", "Spline tool (B)", "B"},
    };

    auto addToolButton = [this, layout](int toolIndex) {
        auto *btn = new QToolButton();
        btn->setText(tools[toolIndex].icon);
        btn->setToolTip(QString("%1\n%2").arg(tools[toolIndex].tooltip, tools[toolIndex].shortcut));
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
        
        connect(btn, &QToolButton::clicked, this, [this, toolIndex]() {
            emit toolSelected(toolIndex);
        });
        
        m_buttons[toolIndex] = btn;
        layout->addWidget(btn);
    };

    addGroupLabel(layout, "Sketch");
    const int sketchTools[] = {Select, Line, Polyline, Rectangle, Circle, Arc, Text, Spline};
    for (int toolIndex : sketchTools)
        addToolButton(toolIndex);

    addGroupSeparator(layout);
    addGroupLabel(layout, "Modify");
    const int modifyTools[] = {Move, Copy, Rotate, Trim, Extend, Chamfer, Fillet, Offset};
    for (int toolIndex : modifyTools)
        addToolButton(toolIndex);

    addGroupSeparator(layout);
    addGroupLabel(layout, "Measure");
    const int measureTools[] = {DimLinear, DimAngular, DimRadius};
    for (int toolIndex : measureTools)
        addToolButton(toolIndex);
    
    layout->addStretch();
    
    // Set first button (Select) as checked by default
    m_buttons[Select]->setChecked(true);
}

void ToolBarWidget::setToolChecked(int toolIndex)
{
    for (int i = 0; i < ToolCount; ++i) {
        if (m_buttons[i])
            m_buttons[i]->setChecked(i == toolIndex);
    }
}