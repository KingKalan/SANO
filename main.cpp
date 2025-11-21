#include "ui/mainwindow.h"

#include <QApplication>
#include <QSurfaceFormat>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // Set application metadata
    app.setApplicationName("SANo Emulator");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("SANo Project");
    
    // Set OpenGL version (3.3 Core Profile as per design doc)
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSwapInterval(1); // Enable vsync
    QSurfaceFormat::setDefaultFormat(format);
    
    // Create and show main window
    MainWindow window;
    window.show();
    
    return app.exec();
}
