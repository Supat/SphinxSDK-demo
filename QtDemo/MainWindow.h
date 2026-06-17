// MainWindow.h
//
// Minimal control surface for the Camera: device list + refresh/connect,
// start/stop, save, and a live image view with a log pane.
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QImage>
#include <QVector>

#include "Camera.h"

class QComboBox;
class QPushButton;
class QLabel;
class QPlainTextEdit;
class ControlPanel;
class FeaturePanel;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onRefresh();
    void onConnect();
    void onStart();
    void onStop();
    void onSave();
    void onBayerChanged(int index);
    void onFrame(const QImage &image);
    void onLog(const QString &message);
    void onStreamingChanged(bool streaming);

private:
    void updateButtons();

    Camera m_camera;
    QVector<DeviceInfo> m_devices;
    QImage m_lastFrame;
    qint64 m_frameCount = 0;

    QComboBox *m_deviceCombo = nullptr;
    QComboBox *m_bayerCombo = nullptr;
    QPushButton *m_refreshBtn = nullptr;
    QPushButton *m_connectBtn = nullptr;
    QPushButton *m_startBtn = nullptr;
    QPushButton *m_stopBtn = nullptr;
    QPushButton *m_saveBtn = nullptr;
    QLabel *m_view = nullptr;
    QPlainTextEdit *m_log = nullptr;
    ControlPanel *m_controlPanel = nullptr;
    FeaturePanel *m_featurePanel = nullptr;
};

#endif // MAINWINDOW_H
