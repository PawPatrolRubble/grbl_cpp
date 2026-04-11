#include <QApplication>
#include <QCoreApplication>
#include "App.h"
#include "ui/MainWindow.h"

void setupAppInfo()
{
    QCoreApplication::setOrganizationName("GrblSender");
    QCoreApplication::setApplicationName("GRBL G-Code Sender");
    QCoreApplication::setApplicationVersion("1.0.0");
}

int runApplication(int argc, char *argv[])
{
    QApplication app(argc, argv);
    setupAppInfo();

    MainWindow window;
    window.show();

    return app.exec();
}
