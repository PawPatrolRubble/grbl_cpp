#pragma once

#include <QDockWidget>

class QWidget;

/// A base class for dockable panels in the main window.
/// Provides a consistent interface for creating dockable widgets with minimal boilerplate.
class DockPanel : public QDockWidget
{
    Q_OBJECT

public:
    /// Create a dock panel with the given title and content widget.
    /// @param title       Window title for the dock.
    /// @param content     The widget to place inside the dock. Ownership is transferred.
    /// @param area        Default dock area (default: left).
    /// @param closable    Whether the dock can be closed (default: true).
    /// @param floatable   Whether the dock can be floated as a separate window (default: true).
    explicit DockPanel(const QString &title,
                       QWidget *content,
                       Qt::DockWidgetArea area = Qt::LeftDockWidgetArea,
                       bool closable = true,
                       bool floatable = true,
                       QWidget *parent = nullptr);

    ~DockPanel() override = default;

protected:
    /// Override this to provide a unique key for saving/restoring geometry.
    /// Default returns the windowTitle.
    virtual QString dockKey() const;

    /// Called when the dock is about to be saved. Override to persist panel-specific state.
    virtual void saveState() {}

    /// Called when the dock is being restored. Override to restore panel-specific state.
    virtual void restoreState() {}
};
