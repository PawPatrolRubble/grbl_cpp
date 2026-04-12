#include "DockPanel.h"

#include <QSettings>

DockPanel::DockPanel(const QString &title,
                     QWidget *content,
                     Qt::DockWidgetArea area,
                     bool closable,
                     bool floatable,
                     QWidget *parent)
    : QDockWidget(title, parent)
{
    setWidget(content);
    setAllowedAreas(Qt::AllDockWidgetAreas);
    setFeatures(
        (closable ? QDockWidget::DockWidgetClosable : QDockWidget::NoDockWidgetFeatures) |
        (floatable ? QDockWidget::DockWidgetFloatable : QDockWidget::NoDockWidgetFeatures) |
        QDockWidget::DockWidgetMovable
    );
}

QString DockPanel::dockKey() const
{
    return windowTitle();
}
