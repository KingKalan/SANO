#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QTimer>

// Forward declarations
class Emulator;
class Displaywidget;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

/**
 * Main window for SANo Emulator
 * 
 * Provides:
 * - Menu bar (File, Emulation)
 * - Display widget (320x240 upscaled)
 * - Status bar (FPS, emulation status)
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Menu actions - File
    void onLoadROM();
    void onExit();
    
    // Menu actions - Emulation
    void onReset();
    void onPause(bool checked);
    
    // Update timer
    void onUpdateFrame();

private:
    // UI
    Ui::MainWindow *ui;
    Displaywidget *displayWidget;
    
    // Emulator
    Emulator *emulator;
    
    // Timing
    QTimer *frameTimer;
    int frameCount;
    qint64 lastFPSUpdate;
    
    // Methods
    void setupEmulator();
    void setupConnections();
    void updateStatusBar();
    void closeEvent(QCloseEvent *event) override;
};

#endif // MAINWINDOW_H
