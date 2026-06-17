// MainWindow.cpp
#include "MainWindow.h"
#include "ControlPanel.h"
#include "FeaturePanel.h"

#include <QComboBox>
#include <QDockWidget>
#include <QTabWidget>
#include <QPushButton>
#include <QLabel>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QPixmap>
#include <QDateTime>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("Sphinx SDK — Qt Demo");

    // --- controls row ---
    m_deviceCombo = new QComboBox;
    m_deviceCombo->setMinimumWidth(280);
    m_refreshBtn = new QPushButton("Refresh");
    m_connectBtn = new QPushButton("Connect");
    m_startBtn   = new QPushButton("Start");
    m_stopBtn    = new QPushButton("Stop");
    m_saveBtn    = new QPushButton("Save…");

    // Bayer tile selector for color cameras. "Auto" uses the format-derived
    // tile; the explicit entries let the user fix an R/B or green-phase swap.
    m_bayerCombo = new QComboBox;
    m_bayerCombo->addItem("Auto");   // -> override -1
    m_bayerCombo->addItem("RGGB");   // -> 0
    m_bayerCombo->addItem("GBRG");   // -> 1
    m_bayerCombo->addItem("GRBG");   // -> 2
    m_bayerCombo->addItem("BGGR");   // -> 3

    auto *controls = new QHBoxLayout;
    controls->addWidget(new QLabel("Device:"));
    controls->addWidget(m_deviceCombo, 1);
    controls->addWidget(m_refreshBtn);
    controls->addWidget(m_connectBtn);
    controls->addSpacing(16);
    controls->addWidget(m_startBtn);
    controls->addWidget(m_stopBtn);
    controls->addWidget(m_saveBtn);
    controls->addSpacing(16);
    controls->addWidget(new QLabel("Bayer:"));
    controls->addWidget(m_bayerCombo);

    // --- live view ---
    m_view = new QLabel("No image");
    m_view->setAlignment(Qt::AlignCenter);
    m_view->setMinimumSize(640, 480);
    m_view->setStyleSheet("background:#202020; color:#888;");
    auto *scroll = new QScrollArea;
    scroll->setWidget(m_view);
    scroll->setWidgetResizable(true);

    // --- log ---
    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(500);
    m_log->setFixedHeight(120);

    auto *central = new QWidget;
    auto *layout = new QVBoxLayout(central);
    layout->addLayout(controls);
    layout->addWidget(scroll, 1);
    layout->addWidget(m_log);
    setCentralWidget(central);

    // --- settings sidebar: targeted controls + full feature grid ---
    m_controlPanel = new ControlPanel(&m_camera);
    m_featurePanel = new FeaturePanel(&m_camera);
    auto *tabs = new QTabWidget;
    tabs->addTab(m_controlPanel, "Controls");
    tabs->addTab(m_featurePanel, "All Features");
    auto *dock = new QDockWidget("Camera Settings", this);
    dock->setWidget(tabs);
    dock->setMinimumWidth(340);
    addDockWidget(Qt::RightDockWidgetArea, dock);

    statusBar()->showMessage("Ready");

    // --- wiring ---
    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::onRefresh);
    connect(m_connectBtn, &QPushButton::clicked, this, &MainWindow::onConnect);
    connect(m_startBtn,   &QPushButton::clicked, this, &MainWindow::onStart);
    connect(m_stopBtn,    &QPushButton::clicked, this, &MainWindow::onStop);
    connect(m_saveBtn,    &QPushButton::clicked, this, &MainWindow::onSave);
    connect(m_bayerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onBayerChanged);

    connect(&m_camera, &Camera::frameReady,       this, &MainWindow::onFrame);
    connect(&m_camera, &Camera::logMessage,       this, &MainWindow::onLog);
    connect(&m_camera, &Camera::streamingChanged, this, &MainWindow::onStreamingChanged);

    // GRBG was verified correct for this camera; make it the default tile.
    m_bayerCombo->setCurrentIndex(3);  // "GRBG"

    updateButtons();
    onRefresh();
}

MainWindow::~MainWindow()
{
    m_camera.close();
}

void MainWindow::onRefresh()
{
    QString err;
    m_devices = Camera::discover(&err);
    m_deviceCombo->clear();
    for (const DeviceInfo &d : m_devices)
        m_deviceCombo->addItem(QString("%1 — %2 (%3)").arg(d.model, d.manufacturer, d.ip));

    if (m_devices.isEmpty())
        onLog(err.isEmpty() ? "No cameras found." : err);
    else
        onLog(QString("Found %1 device(s).").arg(m_devices.size()));
    updateButtons();
}

void MainWindow::onConnect()
{
    int idx = m_deviceCombo->currentIndex();
    if (idx < 0 || idx >= m_devices.size())
        return;

    QString err;
    if (!m_camera.open(m_devices[idx], &err))
        QMessageBox::warning(this, "Connect failed", err);
    else
    {
        m_controlPanel->refresh();
        m_featurePanel->refresh();
    }
    updateButtons();
}

void MainWindow::onStart()
{
    m_frameCount = 0;
    QString err;
    if (!m_camera.start(&err))
        QMessageBox::warning(this, "Start failed", err);
}

void MainWindow::onStop()
{
    m_camera.stop();
}

void MainWindow::onSave()
{
    if (m_lastFrame.isNull())
    {
        QMessageBox::information(this, "Save", "No frame captured yet.");
        return;
    }
    QString path = QFileDialog::getSaveFileName(
        this, "Save image", "image.png", "Images (*.png *.bmp *.jpg)");
    if (path.isEmpty())
        return;
    if (m_lastFrame.save(path))
        onLog("Saved " + path);
    else
        QMessageBox::warning(this, "Save failed", "Could not write " + path);
}

void MainWindow::onBayerChanged(int index)
{
    // Combo index 0 = "Auto" -> override -1; 1..4 -> tile 0..3.
    m_camera.setBayerPattern(index - 1);
    onLog("Bayer pattern: " + m_bayerCombo->currentText());
}

void MainWindow::onFrame(const QImage &image)
{
    m_lastFrame = image;
    ++m_frameCount;
    // Scale to the view while preserving aspect ratio.
    m_view->setPixmap(QPixmap::fromImage(image)
                          .scaled(m_view->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    statusBar()->showMessage(QString("Frame %1   %2x%3")
                                 .arg(m_frameCount).arg(image.width()).arg(image.height()));
}

void MainWindow::onLog(const QString &message)
{
    m_log->appendPlainText(QDateTime::currentDateTime().toString("HH:mm:ss ") + message);
}

void MainWindow::onStreamingChanged(bool streaming)
{
    Q_UNUSED(streaming);
    updateButtons();
}

void MainWindow::updateButtons()
{
    const bool streaming = m_camera.isStreaming();
    const bool open = m_camera.isOpen();
    m_deviceCombo->setEnabled(!streaming);
    m_refreshBtn->setEnabled(!streaming);
    m_connectBtn->setEnabled(!streaming && !m_devices.isEmpty());
    m_startBtn->setEnabled(open && !streaming);
    m_stopBtn->setEnabled(streaming);
    m_saveBtn->setEnabled(!m_lastFrame.isNull());
}
