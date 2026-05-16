#ifndef TOOLBARWIDGET_H
#define TOOLBARWIDGET_H

#include <QWidget>
#include <QToolButton>

/// Vertical toolbar widget for graphics design tools.
/// Emits toolSelected(int) signal when a tool button is clicked.
/// Parent panel connects to this signal and handles tool switching logic.
class ToolBarWidget : public QWidget
{
    Q_OBJECT

public:
    /// Tool indices matching DesignCanvas::ToolType enum order.
    enum ToolIndex {
        Select     = 0,
        Line       = 1,
        Polyline   = 2,
        Rectangle  = 3,
        Circle     = 4,
        Arc        = 5,
        Text       = 6,
        DimLinear  = 7,
        DimAngular = 8,
        DimRadius  = 9,
        Move       = 10,
        Copy       = 11,
        Rotate     = 12,
        Trim       = 13,
        Extend     = 14,
        Chamfer    = 15,
        Fillet     = 16,
        Offset     = 17,
        Measure    = 18,
        Spline     = 19,
        ToolCount  = 20
    };

    explicit ToolBarWidget(QWidget *parent = nullptr);

    /// Set the checked state of a tool button by index.
    /// Used by parent panel to sync toolbar with canvas tool changes.
    void setToolChecked(int toolIndex);

signals:
    /// Emitted when a tool button is clicked.
    /// Parent panel should call canvas->setTool() and handle tool-specific UI.
    void toolSelected(int toolIndex);

private:
    QToolButton *m_buttons[ToolCount] = {};
};

#endif // TOOLBARWIDGET_H