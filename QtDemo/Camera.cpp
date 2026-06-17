// Camera.cpp
//
// SphinxLib-facing implementation. All windows.h / SphinxLib.h inclusion is
// confined to this file so the rest of the Qt app stays free of their macros.

// Winsock must precede windows.h; SphinxLib relies on the Win32 base types.
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>

#include "SphinxLib.h"
#include "darknoise.h"
#include "bayer.h"

#include "Camera.h"

#include <cstring>

#define BUFFER_COUNT 4

// ---- SDK callback plumbing -------------------------------------------------
// SphinxLib hands back info/error strings and message-channel events through C
// callbacks. Route them to the active Camera instance (single-camera demo).
namespace {
Camera *g_activeCamera = nullptr;

QString ipToString(quint32 ip)
{
    in_addr a;
    a.s_addr = ip;
    return QString::fromLatin1(inet_ntoa(a));
}

// Turn an SDK status code into a readable token (subset of the demo's table).
QString errString(WORD error)
{
    switch (error)
    {
    case GEV_STATUS_TIMEOUT:           return "STATUS_TIMEOUT";
    case GEV_STATUS_ACCESS_DENIED:     return "STATUS_ACCESS_DENIED";
    case GEV_STATUS_BUSY:              return "STATUS_BUSY";
    case GEV_STATUS_NOT_SUPPORTED:     return "STATUS_NOT_SUPPORTED";
    case GEV_STATUS_INVALID_PARAMETER: return "STATUS_INVALID_PARAMETER";
    case GEV_STATUS_CAMERA_NOT_INIT:   return "STATUS_CAMERA_NOT_INIT";
    case GEV_STATUS_GRAB_ERROR:        return "STATUS_GRAB_ERROR";
    case GEV_STATUS_XML_READ_ERROR:    return "STATUS_XML_READ_ERROR";
    default:                           return QString("error 0x%1").arg(error, 4, 16, QChar('0'));
    }
}

BYTE WINAPI errorCallback(BYTE /*cam*/, char *error_str)
{
    if (g_activeCamera && error_str)
        g_activeCamera->postLog(QString::fromLatin1(error_str));
    return 0;
}

BYTE WINAPI messageCallback(BYTE cam, MESSAGECHANNEL_PARAMETER mcparam)
{
    if (g_activeCamera)
        g_activeCamera->postLog(
            QString("Message channel (cam %1): event 0x%2")
                .arg(cam).arg(mcparam.EventID, 4, 16, QChar('0')));
    return 0;
}
} // namespace

// ---------------------------------------------------------------------------

Camera::Camera(QObject *parent) : QObject(parent)
{
    g_activeCamera = this;
}

Camera::~Camera()
{
    close();
    if (g_activeCamera == this)
        g_activeCamera = nullptr;
}

QVector<DeviceInfo> Camera::discover(QString *error)
{
    QVector<DeviceInfo> out;
    DISCOVERY dis;
    WORD err = GEVDiscovery(&dis, nullptr, 200, 0);
    if (err)
    {
        if (error) *error = QString("GEVDiscovery failed: %1").arg(errString(err));
        return out;
    }
    for (int i = 0; i < dis.Count; ++i)
    {
        const DEVICE_PARAM &p = dis.param[i];
        DeviceInfo d;
        d.manufacturer = QString::fromLatin1(reinterpret_cast<const char *>(p.manuf));
        d.model        = QString::fromLatin1(reinterpret_cast<const char *>(p.model));
        d.version      = QString::fromLatin1(reinterpret_cast<const char *>(p.version));
        d.rawIp          = p.IP;
        d.rawAdapterIp   = p.AdapterIP;
        d.rawAdapterMask = p.AdapterMask;
        d.rawAdapterName = QByteArray(p.adapter_name);
        d.ip         = ipToString(p.IP);
        d.adapterIp  = ipToString(p.AdapterIP);
        d.adapterName = QString::fromLatin1(p.adapter_name);
        out.push_back(d);
    }
    return out;
}

bool Camera::open(const DeviceInfo &dev, QString *error)
{
    if (m_open)
        close();

    CONNECTION con;
    std::memset(&con, 0, sizeof(con));
    con.IP_CANCam   = dev.rawIp;
    con.AdapterIP   = dev.rawAdapterIp;
    con.AdapterMask = dev.rawAdapterMask;
    con.PortCtrl    = 49149;   // 0 => SDK picks a port automatically
    con.PortData    = 49150;
    std::strncpy(con.adapter_name, dev.rawAdapterName.constData(), sizeof(con.adapter_name) - 1);

    WORD err = GEVInit(m_cam, &con, errorCallback, 0, EXCLUSIVE_ACCESS);
    if (err) { if (error) *error = "GEVInit: " + errString(err); return false; }

    err = GEVInitXml(m_cam);
    if (err) { if (error) *error = "GEVInitXml: " + errString(err); GEVClose(m_cam); return false; }

    // Optional; not all cameras expose a message channel.
    GEVSetMessageChannelCallback(m_cam, messageCallback);

    err = GEVOpenStreamChannel(m_cam, con.AdapterIP, con.PortData, 0);
    if (err) { if (error) *error = "GEVOpenStreamChannel: " + errString(err); GEVClose(m_cam); return false; }

    WORD maxPacket = 0;
    GEVTestFindMaxPacketSize(m_cam, &maxPacket, 1400, 9000, 4);
    GEVSetPacketResend(m_cam, 0);

    m_open = true;
    emit logMessage(QString("Connected to %1 (%2)").arg(dev.model, dev.ip));
    return true;
}

bool Camera::readGeometry(QString *error)
{
    INT64 v = 0;
    WORD err;

    err = GEVGetFeatureInteger(m_cam, (char *)"Width", &v);
    if (err) { if (error) *error = "read Width: " + errString(err); return false; }
    m_width = (unsigned)v;

    err = GEVGetFeatureInteger(m_cam, (char *)"Height", &v);
    if (err) { if (error) *error = "read Height: " + errString(err); return false; }
    m_height = (unsigned)v;

    err = GEVGetFeatureInteger(m_cam, (char *)"PixelFormat", &v);
    if (err) { if (error) *error = "read PixelFormat: " + errString(err); return false; }
    m_pixelFormat = (unsigned)v;
    m_bpp = (int)((GVSP_PIX_EFFECTIVE_PIXEL_SIZE_MASK & m_pixelFormat)
                  >> (GVSP_PIX_EFFECTIVE_PIXEL_SIZE_SHIFT + 3)) * 8;

    err = GEVGetFeatureInteger(m_cam, (char *)"PayloadSize", &v);
    if (err) { if (error) *error = "read PayloadSize: " + errString(err); return false; }
    m_imgSize = (unsigned)v;

    char model[256] = {0};
    if (GEVGetFeatureString(m_cam, (char *)"DeviceModelName", model) == 0)
        m_fpn = (std::strncmp(model, "GVRD-MRC HighSpeed", sizeof(model)) == 0);

    emit logMessage(QString("Geometry: %1x%2  bpp=%3  format=0x%4  FPN=%5")
                        .arg(m_width).arg(m_height).arg(m_bpp)
                        .arg(m_pixelFormat, 8, 16, QChar('0'))
                        .arg(m_fpn ? "on" : "off"));
    return true;
}

// Align a buffer pointer up to a 16-byte boundary (required by the SSE2 path
// and harmless for the OpenMP one). Uses uintptr_t to avoid pointer truncation.
static unsigned char *align16(std::vector<unsigned char> &storage, unsigned size)
{
    storage.assign(size + 16, 0);
    auto addr = reinterpret_cast<uintptr_t>(storage.data());
    uintptr_t pad = (16 - (addr % 16)) % 16;
    return storage.data() + pad;
}

void Camera::allocateBuffers()
{
    m_ring.assign(BUFFER_COUNT, nullptr);
    for (int i = 0; i < BUFFER_COUNT; ++i)
    {
        m_ring[i] = (unsigned char *)malloc(m_imgSize);
        GEVSetRingBuffer(m_cam, (WORD)i, m_ring[i]);
    }
    m_result = align16(m_resultAlloc, m_imgSize);
    m_dark   = align16(m_darkAlloc, m_imgSize);
    m_rgb.assign((size_t)m_width * m_height * 3, 0);
    m_darkCounter = 0;
    m_oldExposure = 0;
}

void Camera::freeBuffers()
{
    for (auto *p : m_ring)
        free(p);
    m_ring.clear();
    GEVReleaseRingBuffer(m_cam);
    m_result = m_dark = nullptr;
    m_resultAlloc.clear();
    m_darkAlloc.clear();
    m_rgb.clear();
}

bool Camera::start(QString *error)
{
    if (!m_open) { if (error) *error = "not connected"; return false; }
    if (m_streaming) return true;

    if (!readGeometry(error))
        return false;

    allocateBuffers();

    WORD err = GEVAcquisitionStart(m_cam, 0);
    if (err)
    {
        if (error) *error = "GEVAcquisitionStart: " + errString(err);
        freeBuffers();
        return false;
    }

    m_kill = false;
    m_streaming = true;
    emit streamingChanged(true);
    m_thread = std::thread(&Camera::grabLoop, this);
    return true;
}

void Camera::stop()
{
    if (!m_streaming)
        return;
    m_kill = true;
    if (m_thread.joinable())
        m_thread.join();

    GEVAcquisitionStop(m_cam);
    freeBuffers();

    m_streaming = false;
    emit streamingChanged(false);
}

void Camera::close()
{
    stop();
    if (m_open)
    {
        GEVCloseStreamChannel(m_cam);
        GEVClose(m_cam);
        m_open = false;
    }
}

const unsigned char *Camera::processFrame(unsigned char *src)
{
    if (!m_fpn)
        return src;

    if (m_darkCounter > 10)
    {
        // Steady state: subtract the stored dark reference.
        darknoise_bw_subtract(m_result, src, m_dark,
                              (int32_t)m_width, (int32_t)m_height, (int64_t)m_imgSize);
        return m_result;
    }
    else if (m_darkCounter == 10)
    {
        // Frame captured at ExposureTime=0 -> this is the dark reference.
        std::memcpy(m_dark, src, m_imgSize);
        GEVSetFeatureInteger(m_cam, (char *)"ExposureTime", m_oldExposure);
        ++m_darkCounter;
        return src;
    }
    else if (m_darkCounter == 0)
    {
        // Remember the exposure, then drop it to 0 to capture darkness.
        GEVGetFeatureInteger(m_cam, (char *)"ExposureTime", &m_oldExposure);
        GEVSetFeatureInteger(m_cam, (char *)"ExposureTime", 0);
        ++m_darkCounter;
        return src;
    }
    // frames 1..9: let exposure settle
    ++m_darkCounter;
    return src;
}

QImage Camera::toQImage(const unsigned char *buf) const
{
    if (m_pixelFormat == GVSP_PIX_BAYGR8 || m_pixelFormat == GVSP_PIX_BAYRG8)
    {
        // Map the UI's 0..3 index to dc1394 tiles. The "right" tile depends on
        // both the sensor's color-filter phase and the byte order of the sink
        // (QImage::Format_RGB888 is true R,G,B — unlike save_bmp's BGR), so it
        // is exposed as a live override rather than hard-coded.
        static const int kTiles[4] = {
            BAYER_COLOR_FILTER_RGGB,  // 0
            BAYER_COLOR_FILTER_GBRG,  // 1
            BAYER_COLOR_FILTER_GRBG,  // 2
            BAYER_COLOR_FILTER_BGGR,  // 3
        };
        int ov = m_bayerOverride.load();
        int order;
        if (ov >= 0 && ov < 4)
            order = kTiles[ov];
        else  // auto: true tile for each GigE format with real-RGB output
            order = (m_pixelFormat == GVSP_PIX_BAYGR8)
                        ? BAYER_COLOR_FILTER_GRBG
                        : BAYER_COLOR_FILTER_RGGB;
        bayer_Bilinear(buf, m_rgb.data(), (int)m_width, (int)m_height, order);
        return QImage(m_rgb.data(), (int)m_width, (int)m_height,
                      (int)m_width * 3, QImage::Format_RGB888);
    }
    // Everything else is treated as 8-bit mono (the demo's common case).
    return QImage(buf, (int)m_width, (int)m_height,
                  (int)m_width, QImage::Format_Grayscale8);
}

void Camera::grabLoop()
{
    IMAGE_HEADER header;
    while (!m_kill)
    {
        BYTE index = 0;
        WORD err = GEVGetImageRingBuffer(m_cam, &header, &index);
        if (err)
        {
            // Transient grab errors are common; log and keep going.
            emit logMessage("grab: " + errString(err));
        }
        else
        {
            const unsigned char *out = processFrame(m_ring[index]);
            // .copy() detaches from the ring buffer, which is requeued below.
            emit frameReady(toQImage(out).copy());
        }
        GEVQueueRingBuffer(m_cam, index);
    }
}
