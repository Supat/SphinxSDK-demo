// Camera.h
//
// Qt wrapper around the MRC Sphinx SDK (SphinxLib). Encapsulates discovery,
// connection, the streaming grab loop, and the optional FPN dark-frame
// subtraction so the UI never touches the GEV* API directly.
//
// Design notes:
//  - No SphinxLib / windows.h types leak into this header (they pull in macros
//    that clash with Qt). The SDK-specific code lives entirely in Camera.cpp.
//  - The grab loop runs on its own std::thread. Frames are delivered to the GUI
//    thread through the frameReady() signal (Qt auto-promotes it to a queued
//    connection because the worker thread differs from the receiver's thread).
#ifndef CAMERA_H
#define CAMERA_H

#include <QObject>
#include <QImage>
#include <QString>
#include <QVector>
#include <QByteArray>

#include <atomic>
#include <thread>
#include <vector>

// A discovered GigE Vision device. Carries both display strings and the raw
// connection fields needed to build a SphinxLib CONNECTION.
struct DeviceInfo
{
    QString manufacturer;
    QString model;
    QString version;
    QString ip;          // human-readable, e.g. "192.168.1.10"
    QString adapterIp;
    QString adapterName;

    // raw fields copied verbatim from the SDK's DEVICE_PARAM
    quint32 rawIp = 0;
    quint32 rawAdapterIp = 0;
    quint32 rawAdapterMask = 0;
    QByteArray rawAdapterName;
};

class Camera : public QObject
{
    Q_OBJECT
public:
    explicit Camera(QObject *parent = nullptr);
    ~Camera() override;

    // Enumerate cameras on the network. Static: needs no open connection.
    static QVector<DeviceInfo> discover(QString *error = nullptr);

    // Connect to a discovered device (GEVInit .. GEVOpenStreamChannel).
    bool open(const DeviceInfo &dev, QString *error = nullptr);

    // Begin continuous acquisition; spawns the grab thread.
    bool start(QString *error = nullptr);

    // Signal the grab thread to stop and join it.
    void stop();

    // Stop (if needed) and tear down the connection.
    void close();

    bool isOpen() const { return m_open; }
    bool isStreaming() const { return m_streaming; }

    // Forwarder so the SDK's C callbacks (which live outside the class and
    // therefore cannot touch the protected signals) can surface log lines.
    void postLog(const QString &message) { emit logMessage(message); }

signals:
    void frameReady(const QImage &image);     // emitted from the grab thread
    void logMessage(const QString &message);
    void streamingChanged(bool streaming);

private:
    void grabLoop();                            // runs on m_thread
    bool readGeometry(QString *error);          // Width/Height/PixelFormat/PayloadSize
    void allocateBuffers();
    void freeBuffers();
    const unsigned char *processFrame(unsigned char *src);  // FPN path
    QImage toQImage(const unsigned char *buf) const;

    unsigned char m_cam = 1;        // SDK camera id (1-based)
    bool m_open = false;
    std::atomic<bool> m_streaming{false};
    std::atomic<bool> m_kill{false};
    std::thread m_thread;

    // image geometry, filled by readGeometry()
    unsigned m_width = 0;
    unsigned m_height = 0;
    unsigned m_imgSize = 0;
    unsigned m_pixelFormat = 0;
    int m_bpp = 0;
    bool m_fpn = false;             // true for "GVRD-MRC HighSpeed"

    // FPN dark-frame state
    int m_darkCounter = 0;
    long long m_oldExposure = 0;

    // buffers (raw pointers into the aligned vectors below)
    std::vector<unsigned char *> m_ring;
    std::vector<unsigned char> m_resultAlloc;
    std::vector<unsigned char> m_darkAlloc;
    mutable std::vector<unsigned char> m_rgb;   // demosaic scratch
    unsigned char *m_result = nullptr;
    unsigned char *m_dark = nullptr;
};

#endif // CAMERA_H
