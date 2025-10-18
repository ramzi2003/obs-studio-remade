// Simple test file to verify recording overlay integration
// This file can be removed after testing

#include "recording-overlay.hpp"
#include <QApplication>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    RecordingOverlay overlay;
    overlay.showOverlay();
    
    // Auto-close after 5 seconds for testing
    QTimer::singleShot(5000, &app, &QApplication::quit);
    
    return app.exec();
}
