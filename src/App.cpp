#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QStyleFactory>
#include <QPalette>
#include <QColor>
#include "App.h"
#include "ui/MainWindow.h"

void setupAppInfo()
{
    QCoreApplication::setOrganizationName("GrblSender");
    QCoreApplication::setApplicationName("GRBL G-Code Sender");
    QCoreApplication::setApplicationVersion("1.0.0");
}

void setupDarkPalette()
{
    auto darkPalette = QPalette();
    
    // Core colors
    QColor windowColor(30, 30, 46);       // #1e1e2e
    QColor windowLight(37, 37, 54);       // #252536
    QColor baseColor(50, 50, 74);         // #32324a
    QColor buttonColor(61, 61, 92);       // #3d3d5c
    QColor highlightColor(124, 111, 247); // #7c6ff7
    QColor textBright(224, 224, 255);     // #e0e0ff
    QColor textDim(160, 160, 192);        // #a0a0c0
    
    darkPalette.setColor(QPalette::Window, windowColor);
    darkPalette.setColor(QPalette::WindowText, textBright);
    darkPalette.setColor(QPalette::Base, baseColor);
    darkPalette.setColor(QPalette::AlternateBase, windowLight);
    darkPalette.setColor(QPalette::Text, textBright);
    darkPalette.setColor(QPalette::Button, buttonColor);
    darkPalette.setColor(QPalette::ButtonText, textBright);
    darkPalette.setColor(QPalette::Highlight, highlightColor);
    darkPalette.setColor(QPalette::HighlightedText, Qt::white);
    darkPalette.setColor(QPalette::Link, highlightColor);
    darkPalette.setColor(QPalette::LinkVisited, QColor(149, 136, 255));
    
    // Disabled state
    darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, textDim);
    darkPalette.setColor(QPalette::Disabled, QPalette::Text, textDim);
    darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, textDim);
    darkPalette.setColor(QPalette::Disabled, QPalette::Button, windowLight);
    
    QApplication::setPalette(darkPalette);
}

void loadStyleSheet(QApplication &app)
{
    // Use Fusion style — fully respects QSS and palette
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    
    setupDarkPalette();

    QFile styleFile(":/resources/style.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        app.setStyleSheet(styleFile.readAll());
        styleFile.close();
    }
}

int runApplication(int argc, char *argv[])
{
    QApplication app(argc, argv);
    setupAppInfo();
    loadStyleSheet(app);

    MainWindow window;
    window.show();

    return app.exec();
}
