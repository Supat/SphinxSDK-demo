"""ctypes binding for the MRC Systems Sphinx SDK (SphinxLib.dll).

Mirrors the C++ `Camera` wrapper: discovery, connect, a threaded-friendly grab
path, FPN dark-frame subtraction, and the generic GenICam feature API. Frames
are delivered as NumPy arrays.

The SDK is a flat C API (`GEV*`, __stdcall / WINAPI), so we load it with WinDLL
and declare arg/return types explicitly. Struct layouts follow SphinxLib.h
exactly (default MSVC packing == ctypes natural alignment on x64).
"""
from __future__ import annotations

import os
import shutil
import ctypes as C
from ctypes import (
    Structure, POINTER, byref,
    c_uint8, c_uint16, c_uint32, c_int32, c_int64, c_uint64,
    c_int, c_double, c_char, c_char_p, c_void_p,
)
from dataclasses import dataclass, field

import numpy as np

# ---- Win32 base types ------------------------------------------------------
BYTE, WORD, DWORD, BOOL = c_uint8, c_uint16, c_uint32, c_int
INT64, ULONGLONG = c_int64, c_uint64

# ---- SDK constants (from SphinxLib.h) --------------------------------------
EXCLUSIVE_ACCESS = 1

GVSP_PIX_MONO8 = 0x01080001
GVSP_PIX_BAYGR8 = 0x01080008
GVSP_PIX_BAYRG8 = 0x01080009
GVSP_PIX_EFFECTIVE_PIXEL_SIZE_MASK = 0x00FF0000
GVSP_PIX_EFFECTIVE_PIXEL_SIZE_SHIFT = 16

TYPE_INTEGER, TYPE_FLOAT, TYPE_STRING = 2, 3, 4
TYPE_ENUMERATION, TYPE_COMMAND, TYPE_BOOLEAN = 5, 6, 7
TYPE_CATEGORY, TYPE_FEATURE, TYPE_REGISTER, TYPE_PORT = 0, 1, 8, 9

ACCESS_MODE_RO, ACCESS_MODE_RW, ACCESS_MODE_WO = 0x524F, 0x5257, 0x574F

_TYPE_NAMES = {
    TYPE_INTEGER: "integer", TYPE_FLOAT: "float", TYPE_STRING: "string",
    TYPE_ENUMERATION: "enum", TYPE_COMMAND: "command", TYPE_BOOLEAN: "bool",
    TYPE_CATEGORY: "category", TYPE_FEATURE: "feature",
    TYPE_REGISTER: "register", TYPE_PORT: "port",
}

MAX_ADAPTER_NAME = 256 + 4


# ---- structs ---------------------------------------------------------------
class DEVICE_PARAM(Structure):
    _fields_ = [
        ("IP", DWORD),
        ("manuf", BYTE * 32),
        ("model", BYTE * 32),
        ("version", BYTE * 32),
        ("AdapterIP", DWORD),
        ("AdapterMask", DWORD),
        ("Mac", BYTE * 6),
        ("subnet", DWORD),
        ("gateway", DWORD),
        ("adapter_name", c_char * MAX_ADAPTER_NAME),
        ("serial", BYTE * 16),
        ("userdef_name", BYTE * 16),
        ("status", BYTE),
    ]


class DISCOVERY(Structure):
    _fields_ = [("Count", BYTE), ("param", DEVICE_PARAM * 20)]


class CONNECTION(Structure):
    _fields_ = [
        ("IP_CANCam", DWORD),
        ("PortData", WORD),
        ("PortCtrl", WORD),
        ("AdapterIP", DWORD),
        ("AdapterMask", DWORD),
        ("adapter_name", c_char * MAX_ADAPTER_NAME),
    ]


class IMAGE_HEADER(Structure):
    _fields_ = [
        ("FrameCounter", INT64),
        ("TimeStamp", ULONGLONG),
        ("PixelType", DWORD),
        ("SizeX", DWORD),
        ("SizeY", DWORD),
        ("OffsetX", DWORD),
        ("OffsetY", DWORD),
        ("PaddingX", WORD),
        ("PaddingY", WORD),
        ("MissingPacket", c_int),
        ("PayloadType", WORD),
        ("ChunkDataPayloadLength", DWORD),
        ("ChunkLayoutId", DWORD),
    ]


class FEATURE_PARAMETER(Structure):
    _fields_ = [
        ("Type", BYTE),
        ("Min", INT64), ("Max", INT64),
        ("OnValue", DWORD), ("OffValue", DWORD),
        ("AccessMode", WORD), ("Representation", WORD),
        ("Inc", DWORD), ("CommandValue", DWORD), ("Length", DWORD),
        ("EnumerationCount", BYTE), ("Visibility", BYTE),
        ("FloatMin", c_double), ("FloatMax", c_double), ("FloatInc", c_double),
        ("IsImplemented", BYTE), ("IsAvailable", BYTE),
        ("IsLocked", BYTE), ("Sign", BYTE),
        ("Address", DWORD),
        ("DisplayNotation", BYTE), ("DisplayPrecision", BYTE),
        ("InvalidatorCount", BYTE),
        ("PollingTime", INT64),
    ]


class FeatureList(Structure):
    pass


FeatureList._fields_ = [
    ("Next", POINTER(FeatureList)),
    ("Index", DWORD),
    ("Name", c_char_p),
    ("Type", BYTE),
    ("Level", BYTE),
]

ERROR_CALLBACK = C.WINFUNCTYPE(BYTE, BYTE, c_char_p)


# ---- DLL loading -----------------------------------------------------------
def _default_sdk_dir() -> str:
    here = os.path.dirname(os.path.abspath(__file__))
    return os.path.normpath(os.path.join(here, "..", "SphinxLib"))


SDK_EXTRAS = ""  # set by _load_dll(); directory holding libxml2/libxslt + schemas


def _ensure_dll_dir() -> None:
    """(Re)assert the SDK's DLL directory. Some GEV* calls reset it, so this is
    called again right before GEVInitXml, which loads libxml2 from that dir."""
    if SDK_EXTRAS:
        k = C.windll.kernel32
        k.SetDllDirectoryW.argtypes = [C.c_wchar_p]
        k.SetDllDirectoryW.restype = C.c_bool
        k.SetDllDirectoryW(SDK_EXTRAS)


_DLL_PATH = ""


def _load_dll() -> C.WinDLL:
    global SDK_EXTRAS, _DLL_PATH
    sdk = os.environ.get("SPHINX_SDK_DIR", _default_sdk_dir())
    rel64 = os.path.join(sdk, "Release64")
    extras = os.path.join(sdk, "Extras64")
    SDK_EXTRAS = extras
    _DLL_PATH = os.path.join(rel64, "SphinxLib.dll")
    # Let the loader find SphinxLib.dll + its libxml2/libxslt/MathParser deps.
    for d in (rel64, extras):
        if os.path.isdir(d):
            os.add_dll_directory(d)
            os.environ["PATH"] = d + os.pathsep + os.environ.get("PATH", "")
    # SphinxLib loads its XML helpers (libxml2/libxslt/MathParser) and the
    # GenApi schemas from its OWN directory at GEVInitXml time — not from the
    # DLL search path, SetDllDirectory, or PATH (all of which we tried and which
    # the SDK ignores for this). So mirror the C++ build's "everything beside
    # the binary" layout by copying the Extras64 runtime next to SphinxLib.dll.
    if os.path.isdir(extras) and os.path.isdir(rel64):
        C.windll.kernel32.SetDllDirectoryW(extras)  # harmless extra hint
        for fn in os.listdir(extras):
            dst = os.path.join(rel64, fn)
            if not os.path.exists(dst):
                shutil.copy2(os.path.join(extras, fn), dst)
    dll_path = os.path.join(rel64, "SphinxLib.dll")
    if not os.path.exists(dll_path):
        raise FileNotFoundError(
            f"SphinxLib.dll not found at {dll_path}. "
            f"Set SPHINX_SDK_DIR to the SDK root."
        )
    return C.WinDLL(dll_path)


_lib = _load_dll()


def _proto(name, restype, argtypes):
    fn = getattr(_lib, name)
    fn.restype = restype
    fn.argtypes = argtypes
    return fn


# Function prototypes (subset used by this wrapper).
_GEVDiscovery = _proto("GEVDiscovery", WORD, [POINTER(DISCOVERY), c_void_p, DWORD, BOOL])
_GEVInit = _proto("GEVInit", WORD, [BYTE, POINTER(CONNECTION), ERROR_CALLBACK, BYTE, BYTE])
_GEVClose = _proto("GEVClose", WORD, [BYTE])
_GEVInitXml = _proto("GEVInitXml", WORD, [BYTE])
_GEVOpenStreamChannel = _proto("GEVOpenStreamChannel", WORD, [BYTE, DWORD, WORD, DWORD])
_GEVCloseStreamChannel = _proto("GEVCloseStreamChannel", WORD, [BYTE])
_GEVTestFindMaxPacketSize = _proto("GEVTestFindMaxPacketSize", WORD, [BYTE, POINTER(WORD), WORD, WORD, WORD])
_GEVSetPacketResend = _proto("GEVSetPacketResend", WORD, [BYTE, BYTE])
_GEVAcquisitionStart = _proto("GEVAcquisitionStart", WORD, [BYTE, DWORD])
_GEVAcquisitionStop = _proto("GEVAcquisitionStop", WORD, [BYTE])
_GEVGetImageRingBuffer = _proto("GEVGetImageRingBuffer", WORD, [BYTE, POINTER(IMAGE_HEADER), POINTER(BYTE)])
_GEVQueueRingBuffer = _proto("GEVQueueRingBuffer", WORD, [BYTE, BYTE])
_GEVSetRingBuffer = _proto("GEVSetRingBuffer", WORD, [BYTE, WORD, c_void_p])
_GEVReleaseRingBuffer = _proto("GEVReleaseRingBuffer", WORD, [BYTE])

_GEVGetFeatureInteger = _proto("GEVGetFeatureInteger", WORD, [BYTE, c_char_p, POINTER(INT64)])
_GEVSetFeatureInteger = _proto("GEVSetFeatureInteger", WORD, [BYTE, c_char_p, INT64])
_GEVGetFeatureFloat = _proto("GEVGetFeatureFloat", WORD, [BYTE, c_char_p, POINTER(c_double)])
_GEVSetFeatureFloat = _proto("GEVSetFeatureFloat", WORD, [BYTE, c_char_p, c_double])
_GEVGetFeatureString = _proto("GEVGetFeatureString", WORD, [BYTE, c_char_p, c_char_p])
_GEVSetFeatureString = _proto("GEVSetFeatureString", WORD, [BYTE, c_char_p, c_char_p])
_GEVGetFeatureBoolean = _proto("GEVGetFeatureBoolean", WORD, [BYTE, c_char_p, POINTER(DWORD)])
_GEVSetFeatureBoolean = _proto("GEVSetFeatureBoolean", WORD, [BYTE, c_char_p, DWORD])
_GEVGetFeatureEnumeration = _proto("GEVGetFeatureEnumeration", WORD, [BYTE, c_char_p, c_char_p, c_int])
_GEVSetFeatureEnumeration = _proto("GEVSetFeatureEnumeration", WORD, [BYTE, c_char_p, c_char_p, c_int])
_GEVGetFeatureEnumerationName = _proto("GEVGetFeatureEnumerationName", WORD, [BYTE, c_char_p, BYTE, c_char_p, c_int])
_GEVSetFeatureCommand = _proto("GEVSetFeatureCommand", WORD, [BYTE, c_char_p, DWORD])
_GEVGetFeatureParameter = _proto("GEVGetFeatureParameter", WORD, [BYTE, c_char_p, POINTER(FEATURE_PARAMETER)])
_GEVGetFeatureList = _proto("GEVGetFeatureList", WORD, [BYTE, POINTER(POINTER(FeatureList)), POINTER(BYTE)])
_GEVGetFeatureDisplayName = _proto("GEVGetFeatureDisplayName", WORD, [BYTE, c_char_p, c_char_p, c_int])
_GEVGetFeatureTooltip = _proto("GEVGetFeatureTooltip", WORD, [BYTE, c_char_p, c_char_p, c_int])
_GEVGetFeatureUnit = _proto("GEVGetFeatureUnit", WORD, [BYTE, c_char_p, c_char_p, c_int])


def _bytes_to_str(buf) -> str:
    raw = bytes(buf)
    return raw.split(b"\x00", 1)[0].decode("latin-1")


# ---- public data types -----------------------------------------------------
@dataclass
class DeviceInfo:
    index: int
    manufacturer: str
    model: str
    version: str
    ip: str
    adapter_ip: str
    adapter_name: str
    _raw_ip: int = 0
    _raw_adapter_ip: int = 0
    _raw_adapter_mask: int = 0
    _raw_adapter_name: bytes = b""


@dataclass
class FeatureInfo:
    name: str
    type: str = "unknown"
    display_name: str = ""
    tooltip: str = ""
    unit: str = ""
    readable: bool = False
    writable: bool = False
    available: bool = False
    int_min: int = 0
    int_max: int = 0
    int_inc: int = 1
    float_min: float = 0.0
    float_max: float = 0.0
    enum_entries: list = field(default_factory=list)


def _ip_str(raw: int) -> str:
    return ".".join(str((raw >> (8 * i)) & 0xFF) for i in range(4))


# keep a reference so the callback isn't garbage collected
def _default_error_cb(cam, msg):
    if msg:
        print(f"[SphinxLib] {msg.decode('latin-1', 'replace')}")
    return 0


_ERR_CB = ERROR_CALLBACK(_default_error_cb)


class SphinxError(RuntimeError):
    def __init__(self, where: str, code: int):
        super().__init__(f"{where} failed (status 0x{code:04X})")
        self.code = code


class Camera:
    """High-level wrapper around one Sphinx camera (SDK id `cam`)."""

    def __init__(self, cam: int = 1):
        self.cam = cam
        self.is_open = False
        self.width = self.height = 0
        self.img_size = 0
        self.pixel_format = 0
        self.bpp = 0
        self.is_color = False
        self.fpn = False
        # FPN dark-frame state (mirrors the C++ demo)
        self._dark = None
        self._dark_counter = 0
        self._old_exposure = 0
        self.fpn_enabled = True
        # ring buffer
        self._ring: list[np.ndarray] = []
        self._streaming = False
        # OpenCV Bayer code; verified GRBG on the test camera (see Qt demo).
        self.bayer_code = None  # set lazily to cv2.COLOR_BayerGB2BGR-style

    # ---- discovery ----
    @staticmethod
    def discover() -> list[DeviceInfo]:
        dis = DISCOVERY()
        err = _GEVDiscovery(byref(dis), None, 200, 0)
        if err:
            raise SphinxError("GEVDiscovery", err)
        out = []
        for i in range(dis.Count):
            p = dis.param[i]
            out.append(DeviceInfo(
                index=i,
                manufacturer=_bytes_to_str(p.manuf),
                model=_bytes_to_str(p.model),
                version=_bytes_to_str(p.version),
                ip=_ip_str(p.IP),
                adapter_ip=_ip_str(p.AdapterIP),
                adapter_name=p.adapter_name.decode("latin-1", "replace"),
                _raw_ip=p.IP,
                _raw_adapter_ip=p.AdapterIP,
                _raw_adapter_mask=p.AdapterMask,
                _raw_adapter_name=p.adapter_name,
            ))
        return out

    # ---- connection ----
    def open(self, dev: DeviceInfo) -> None:
        con = CONNECTION()
        con.IP_CANCam = dev._raw_ip
        con.AdapterIP = dev._raw_adapter_ip
        con.AdapterMask = dev._raw_adapter_mask
        con.PortCtrl = 49149
        con.PortData = 49150
        con.adapter_name = dev._raw_adapter_name

        err = _GEVInit(self.cam, byref(con), _ERR_CB, 0, EXCLUSIVE_ACCESS)
        if err:
            raise SphinxError("GEVInit", err)
        _ensure_dll_dir()   # GEVInit may have reset the DLL search dir
        err = _GEVInitXml(self.cam)
        if err:
            _GEVClose(self.cam)
            raise SphinxError("GEVInitXml", err)
        err = _GEVOpenStreamChannel(self.cam, con.AdapterIP, con.PortData, 0)
        if err:
            _GEVClose(self.cam)
            raise SphinxError("GEVOpenStreamChannel", err)

        max_packet = WORD(0)
        _GEVTestFindMaxPacketSize(self.cam, byref(max_packet), 1400, 9000, 4)
        _GEVSetPacketResend(self.cam, 0)
        self.is_open = True

    def close(self) -> None:
        if self._streaming:
            self.stop()
        if self.is_open:
            _GEVCloseStreamChannel(self.cam)
            _GEVClose(self.cam)
            self.is_open = False

    # ---- geometry ----
    def _read_geometry(self) -> None:
        self.width = self.get_int("Width")
        self.height = self.get_int("Height")
        self.pixel_format = self.get_int("PixelFormat")
        self.img_size = self.get_int("PayloadSize")
        bits = (self.pixel_format & GVSP_PIX_EFFECTIVE_PIXEL_SIZE_MASK) \
            >> (GVSP_PIX_EFFECTIVE_PIXEL_SIZE_SHIFT + 3)
        self.bpp = bits * 8
        self.is_color = self.pixel_format in (GVSP_PIX_BAYGR8, GVSP_PIX_BAYRG8)
        try:
            self.fpn = self.get_string("DeviceModelName") == "GVRD-MRC HighSpeed"
        except SphinxError:
            self.fpn = False

    # ---- acquisition ----
    def start(self, buffer_count: int = 4) -> None:
        if not self.is_open:
            raise RuntimeError("camera not open")
        self._read_geometry()
        self._ring = [np.zeros(self.img_size, dtype=np.uint8) for _ in range(buffer_count)]
        for i, buf in enumerate(self._ring):
            _GEVSetRingBuffer(self.cam, i, buf.ctypes.data_as(c_void_p))
        self._dark = None
        self._dark_counter = 0
        err = _GEVAcquisitionStart(self.cam, 0)
        if err:
            raise SphinxError("GEVAcquisitionStart", err)
        self._streaming = True

    def stop(self) -> None:
        if not self._streaming:
            return
        _GEVAcquisitionStop(self.cam)
        _GEVReleaseRingBuffer(self.cam)
        self._ring = []
        self._streaming = False

    def get_frame(self, raw: bool = False):
        """Grab one frame. Returns (image: np.ndarray, header: IMAGE_HEADER).

        With raw=False, applies FPN subtraction (mono) / Bayer demosaic (color)
        and returns an RGB or grayscale image ready for display or MediaPipe.
        """
        hdr = IMAGE_HEADER()
        idx = BYTE(0)
        err = _GEVGetImageRingBuffer(self.cam, byref(hdr), byref(idx))
        try:
            if err:
                raise SphinxError("GEVGetImageRingBuffer", err)
            src = self._ring[idx.value]
            out = src if raw else self._process(src)
            return out.copy(), hdr
        finally:
            _GEVQueueRingBuffer(self.cam, idx.value)

    def _process(self, src: np.ndarray) -> np.ndarray:
        mono = src[: self.width * self.height].reshape(self.height, self.width)
        if self.fpn and self.fpn_enabled:
            mono = self._apply_fpn(mono)
        if self.is_color:
            return self._demosaic(mono)
        return mono

    def _apply_fpn(self, mono: np.ndarray) -> np.ndarray:
        c = self._dark_counter
        if c > 10:
            diff = (mono.astype(np.int16) - self._dark.astype(np.int16)) * 1.5
            return np.clip(diff, 0, 255).astype(np.uint8)
        if c == 10:
            self._dark = mono.copy()
            self.set_int("ExposureTime", self._old_exposure)
            self._dark_counter += 1
        elif c == 0:
            self._old_exposure = self.get_int("ExposureTime")
            self.set_int("ExposureTime", 0)
            self._dark_counter += 1
        else:
            self._dark_counter += 1
        return mono

    def _demosaic(self, mono: np.ndarray) -> np.ndarray:
        import cv2
        if self.bayer_code is None:
            # GRBG verified correct for this camera in the Qt demo.
            self.bayer_code = cv2.COLOR_BayerGB2RGB if self.pixel_format == GVSP_PIX_BAYGR8 \
                else cv2.COLOR_BayerRG2RGB
        return cv2.cvtColor(mono, self.bayer_code)

    # ---- generic feature access ----
    def get_int(self, name: str) -> int:
        v = INT64(0)
        err = _GEVGetFeatureInteger(self.cam, name.encode("latin-1"), byref(v))
        if err:
            raise SphinxError(f"GetInteger({name})", err)
        return v.value

    def set_int(self, name: str, value: int) -> None:
        err = _GEVSetFeatureInteger(self.cam, name.encode("latin-1"), value)
        if err:
            raise SphinxError(f"SetInteger({name})", err)

    def get_float(self, name: str) -> float:
        v = c_double(0)
        err = _GEVGetFeatureFloat(self.cam, name.encode("latin-1"), byref(v))
        if err:
            raise SphinxError(f"GetFloat({name})", err)
        return v.value

    def set_float(self, name: str, value: float) -> None:
        err = _GEVSetFeatureFloat(self.cam, name.encode("latin-1"), value)
        if err:
            raise SphinxError(f"SetFloat({name})", err)

    def get_string(self, name: str) -> str:
        buf = C.create_string_buffer(512)
        err = _GEVGetFeatureString(self.cam, name.encode("latin-1"), buf)
        if err:
            raise SphinxError(f"GetString({name})", err)
        return buf.value.decode("latin-1")

    def set_string(self, name: str, value: str) -> None:
        err = _GEVSetFeatureString(self.cam, name.encode("latin-1"), value.encode("latin-1"))
        if err:
            raise SphinxError(f"SetString({name})", err)

    def get_bool(self, name: str) -> bool:
        v = DWORD(0)
        err = _GEVGetFeatureBoolean(self.cam, name.encode("latin-1"), byref(v))
        if err:
            raise SphinxError(f"GetBool({name})", err)
        return bool(v.value)

    def set_bool(self, name: str, value: bool) -> None:
        err = _GEVSetFeatureBoolean(self.cam, name.encode("latin-1"), 1 if value else 0)
        if err:
            raise SphinxError(f"SetBool({name})", err)

    def get_enum(self, name: str) -> str:
        buf = C.create_string_buffer(128)
        err = _GEVGetFeatureEnumeration(self.cam, name.encode("latin-1"), buf, 128)
        if err:
            raise SphinxError(f"GetEnum({name})", err)
        return buf.value.decode("latin-1")

    def set_enum(self, name: str, entry: str) -> None:
        e = entry.encode("latin-1")
        err = _GEVSetFeatureEnumeration(self.cam, name.encode("latin-1"), e, len(e))
        if err:
            raise SphinxError(f"SetEnum({name})", err)

    def command(self, name: str) -> None:
        err = _GEVSetFeatureCommand(self.cam, name.encode("latin-1"), 1)
        if err:
            raise SphinxError(f"Command({name})", err)

    def describe(self, name: str) -> FeatureInfo:
        fi = FeatureInfo(name=name, display_name=name)
        fp = FEATURE_PARAMETER()
        if _GEVGetFeatureParameter(self.cam, name.encode("latin-1"), byref(fp)) != 0:
            return fi
        fi.type = _TYPE_NAMES.get(fp.Type, "unknown")
        # Show any implemented feature: some cameras report IsAvailable=0 for
        # implemented, writable nodes (e.g. ExposureTime), which would otherwise
        # hide them from the browser/controls.
        fi.available = bool(fp.IsImplemented) or bool(fp.IsAvailable)
        fi.readable = fp.AccessMode in (ACCESS_MODE_RO, ACCESS_MODE_RW)
        fi.writable = fp.AccessMode in (ACCESS_MODE_WO, ACCESS_MODE_RW) and not fp.IsLocked
        fi.int_min, fi.int_max = fp.Min, fp.Max
        fi.int_inc = fp.Inc or 1
        fi.float_min, fi.float_max = fp.FloatMin, fp.FloatMax

        def _q(fn):
            b = C.create_string_buffer(256)
            return b.value.decode("latin-1") if fn(self.cam, name.encode("latin-1"), b, 256) == 0 else ""

        fi.display_name = _q(_GEVGetFeatureDisplayName) or name
        fi.tooltip = _q(_GEVGetFeatureTooltip)
        fi.unit = _q(_GEVGetFeatureUnit)
        if fp.Type == TYPE_ENUMERATION:
            for i in range(fp.EnumerationCount):
                eb = C.create_string_buffer(128)
                if _GEVGetFeatureEnumerationName(self.cam, name.encode("latin-1"), i, eb, 128) == 0:
                    fi.enum_entries.append(eb.value.decode("latin-1"))
        return fi

    def feature_list(self, max_level: int = 99) -> list[FeatureInfo]:
        head = POINTER(FeatureList)()
        level = BYTE(0)
        if _GEVGetFeatureList(self.cam, byref(head), byref(level)) != 0:
            return []
        out, node = [], head
        # The list node's Type is STRUCTURAL: 0 = category, 1 = feature (leaf).
        # The real value type (integer/float/enum/...) comes from describe().
        editable = {"integer", "float", "string", "enum", "bool", "command"}
        while node:
            n = node.contents
            if n.Name and n.Level <= max_level and n.Type != TYPE_CATEGORY:
                fi = self.describe(n.Name.decode("latin-1"))
                if fi.available and fi.type in editable:
                    out.append(fi)
            node = n.Next
        return out

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()
