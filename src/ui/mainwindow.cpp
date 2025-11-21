#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "displaywidget.h"
#include "emulator.h"
#include <chrono>
#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QCloseEvent>
#include <QDateTime>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , emulator(nullptr)
    , frameTimer(nullptr)
    , frameCount(0)
    , lastFPSUpdate(0)
{
    ui->setupUi(this);
    
    // Get reference to the display widget from the UI
    displayWidget = ui->displayWidget;
    
    // Setup emulator
    setupEmulator();
    
    // Connect signals/slots
    setupConnections();
    
    // Setup frame timer (60 Hz)
    frameTimer = new QTimer(this);
    connect(frameTimer, &QTimer::timeout, this, &MainWindow::onUpdateFrame);
    frameTimer->start(16); // ~60 FPS (16.67ms)
    
    // Initial status
    updateStatusBar();
}

MainWindow::~MainWindow() {
    if (emulator) {
        emulator->stop();
        delete emulator;
    }
    delete ui;
}

void MainWindow::setupEmulator() {
    emulator = new Emulator();
    
    if (!emulator->initialize()) {
        QMessageBox::critical(this, "Error", "Failed to initialize emulator!");
        return;
    }
    
    // Connect emulator to display widget
    if (displayWidget) {
        displayWidget->setEmulator(emulator);
    }
}

void MainWindow::setupConnections() {
    // File menu
    connect(ui->actionLoad_ROM, &QAction::triggered, this, &MainWindow::onLoadROM);
    connect(ui->actionExit, &QAction::triggered, this, &MainWindow::onExit);
    
    // Emulation menu
    connect(ui->actionReset, &QAction::triggered, this, &MainWindow::onReset);
    connect(ui->actionPause, &QAction::triggered, this, &MainWindow::onPause);
}

void MainWindow::onLoadROM() {
    QString filename = QFileDialog::getOpenFileName(
        this,
        "Load ROM",
        QString(),
        "ROM Files (*.sno *.bin);;All Files (*.*)"
    );
    
    if (filename.isEmpty()) {
        return;
    }
    
    // Stop emulation
    if (emulator && emulator->isRunning()) {
        emulator->stop();
    }
    
    // Load ROM
    if (emulator && emulator->loadROM(filename.toStdString())) {
        statusBar()->showMessage("ROM loaded: " + filename, 3000);
        
        // Reset and start
        emulator->reset();
        emulator->run();
        
        // Unpause if paused
        if (ui->actionPause->isChecked()) {
            ui->actionPause->setChecked(false);
        }
    } else {
        QMessageBox::critical(this, "Error", "Failed to load ROM: " + filename);
    }
}

void MainWindow::onExit() {
    close();
}

void MainWindow::onReset() {
    if (emulator && emulator->isROMLoaded()) {
        emulator->reset();
        statusBar()->showMessage("Emulator reset", 2000);
    }
}

void MainWindow::onPause(bool checked) {
    if (!emulator) return;
    
    if (checked) {
        emulator->pause();
        statusBar()->showMessage("Paused");
    } else {
        emulator->resume();
        statusBar()->showMessage("Running");
    }
}

void MainWindow::onUpdateFrame() {
    if (!emulator || !emulator->isRunning()) {
        return;
    }
    
    // Run one frame of emulation
    if (!emulator->isPaused()) {
        emulator->runFrame();
    }
    
    // Update display
    if (displayWidget) {
        displayWidget->update();
    }
    
    // Update FPS counter
    frameCount++;
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    if (lastFPSUpdate == 0) {
        lastFPSUpdate = currentTime;
    }
    
    qint64 elapsed = currentTime - lastFPSUpdate;
    if (elapsed >= 1000) { // Update every second
        double fps = (frameCount * 1000.0) / elapsed;
        updateStatusBar();
        
        frameCount = 0;
        lastFPSUpdate = currentTime;
    }
}

void MainWindow::updateStatusBar() {
    QString status;

    if (emulator) {
        if (emulator->isROMLoaded()) {
            if (emulator->isPaused()) {
                status = "Paused";
            } else if (emulator->isRunning()) {
                // Calculate FPS
                static uint64_t lastFrameCount = 0;
                static auto lastTime = std::chrono::steady_clock::now();
                static double fps = 0.0;

                auto now = std::chrono::steady_clock::now();
                double elapsed = std::chrono::duration<double>(now - lastTime).count();

                if (elapsed >= 0.5) {  // Update FPS every 0.5 seconds
                    uint64_t currentFrames = emulator->getFrameCount();
                    fps = (currentFrames - lastFrameCount) / elapsed;
                    lastFrameCount = currentFrames;
                    lastTime = now;
                }

                status = QString("Running | FPS: %1")
                .arg(fps, 0, 'f', 1);
            } else {
                status = "Stopped";
            }
        } else {
            status = "No ROM loaded";
        }
    } else {
        status = "Emulator not initialized";
    }

    statusBar()->showMessage(status);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (emulator) {
        emulator->stop();
    }
    event->accept();
}
