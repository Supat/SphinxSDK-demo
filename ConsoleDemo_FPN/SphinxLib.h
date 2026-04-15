/*****************************************************************************/
/*
 *  SphinxLib.h -- header file for CANCamGigE library
 *
 *  (c) Copyright 2007 - 2018 Sensor to Image GmbH
 *
 * Version: 2.1.4 Date: 29.08.2018
 *
 * History:
 * Version: 1.0.0 Date: 19.07.2007
 * - initial realease
 * Version: 1.0.1 Date: 18.05.2009
 * - insert serial and userdef_name to DEVICE_PARAM struct
 * - change DEFAULT_PDATA to 2001 and DEFAULT_PCTRL to 2000
 * - insert GVSP_PIX_RGB565_PACKED format not GigE Vision compliant
 * Version: 1.0.2 Date: 18.05.2009
 * - insert EnumerationCount to FEATURE_PARAMETER struct
 * - insert TYPE_ENUMERATION
 * - change xml functions define
 * Version: 1.0.2 Date: 19.06.2009
 * - insert GEV_STATUS_FEATURE_NOT_AVAILABLE define
 * - insert GEV_STATUS_MATH_PARSER_ERROR define
 * - insert FloatMin, FloatMax, IsImplemented and IsAvailable in FEATURE_PARAMETER struct
 * - insert TYPE_FLOAT and TYPE_CATEGORY define
 * - insert VISIBILITY_INVISIBLE, VISIBILITY_BEGINNER, VISIBILITY_EXPERT and VISIBILITY_GURU define
 * - insert get_feature_float and set_feature_float function
 * - insert GEV_STATUS_FEATURE_ITEM_NOT_AVAILABLE define
 * - insert get_feature_display_name and get_feature_tooltip functions
 * Version: 1.0.3 Date: 23.07.2009
 * - insert set_read_write_parameter and get_read_write_parameter functions
 * - insert set_heartbeat_rate and get_heartbeat_rate functions [R9-13a]
 * - insert GEV_STATUS_NOT_SUPPORTED define
 * - insert test_packet function
 * Version: 1.0.5 Date: 26.10.2009
 * - insert get_filter_driver_version function
 * Version: 1.0.6 Date: 20.01.2010
 * - insert new GigE Vision 1.1 status codes and pixel formats
 * - insert new device-specific status codes
 * - insert set_channel_parameter and get_channel_parameter function
 * - insert SIGN_UNSIGNED and SIGN_SIGNED define
 * - insert IsLocked and Sign in FEATURE_PARAMETER struct
 * - insert TYPE_REGISTER define
 * - change GEVGetFeatureInteger function to get 64 bit integer
 * - change float to double
 * - insert get_subsub_feature_count and get_subsub_feature_name functions
 * - insert force_ip function
 * - insert get_feature_enable_status function
 * - Change library to SphinxLib CANCamGige.h -> SphinxLib.h
 * Version: 1.0.7 Date: 04.02.2010
 * - insert get_image_header function
 * - insert IMAGE_HEADER struct
 * Version: 2.0.1 Date: 17.03.2011
 * - insert error_func in CONNECTION struct
 * - insert FeatureList struct
 * - insert new function names
 * - insert GEV_STATUS_XML_SCHEMA_ERROR and GEV_STATUS_XML_STYLESHEET_ERROR define
 * - insert new parameter in GEVInit function
 * - insert new parameter in DEVICE_PARAM struct need for discovery funktion
 * - insert PaddingX, PaddingY and MissingPacket in IMAGE_HEADER
 * - insert new function GEVGetFeatureEnableStatus
 * - remove function GEVGetLocalError
 * - insert GEVGetDetailedLog and GEVSetDetailedLog functions
 * - insert GEVPacketResend funcktion
 * Version: 2.0.2 Date: 14.04.2011
 * - insert GEV_STATUS_TEST_PACKET_DATA_ERROR define
 * - change message channel callback function
 * Version: 2.0.3 Date: 02.08.2011
 * - added YCbCr709_422 and YCbCr709 pixrl format GigE Vision 2.0
 * - change GEVInit and GEVOpenStreamChannel functions
 * - added new discovery define DISCOVERY_STATUS_CONTROL_OPEN
 * - added open mode defines OPEN_ACCESS, EXCLUSIVE_ACCESS and CONTROL_ACCESS
 * Version: 2.0.4 Date: 15.05.2012
 * - added new pixel format names
 * - added address in FEATURE_PARAMETER struct
 * - added float pInc and Inc node
 * - change cc_heartbeat_rate to cc_heartbeat_timeout in CHANNEL_PARAMETER struct
 * Version: 2.0.5 Date: 12.12.2012
 * - added DisplayNotation and DisplayPrecision node in FEATURE_PARAMETER struct
 * - added cam_nr in mesage channel callback function
 * - added cam_nr in error callback function
 * Version: 2.0.6 Date: 13.12.2013
 * - added new GigE Vision (2.x) satus codes
 * - added new GigE Vision (2.x) event defines
 * - added GEVGetBufferCount and GEVSetBufferCount function to change buffer count
 * Version: 2.0.8 Date: 09.09.2014
 * - increment device/camera count to 20
 * - added secure transfer
 * Version: 2.0.9 Date: 11.02.2015
 * - added static lib define for GenTL
 * - change message channel callback to new parameter
 * - added GEVGetImageBuffer function
 * - added GEVGetImageRingBuffer function
 * - added GEVQueueRingBuffer function
 * - added GEVSetRingBuffer function
 * - added GEVReleaseRingBuffer function
 * - change parameter ptradr from DWORD to UINT64 in GEVGetPixelPtr
 * Version: 2.1.0 Date: 19.08.2015
 * - added GEVAcquisitionStartEx function to work without xml
 * - added GEVGetXmlSize and GEVGetXmlFile functions
 * - added GEV_STATUS_XML_DLL_NOT_FOUND define
 * - added GEV_STATUS_XML_NOT_INIT define
 * - added GEVGetPacketsOutOfOrder and GEVSetPacketsOutOfOrder functions
 * - added packet size minimum, maximum and increment to GEVTestFindMaxPacketSize function
 * - added InvalidatorCount in FEATURE_PARAMETER struct
 * - added GEVGetFeatureInvalidator function
 * - added GEVSetReadWriteMemoryCallback function
 * Version: 2.1.1 Date: 26.01.2017
 * - added WINAPI to GEVTestFindMaxPacketSize function
 * - added PollingTime in FEATURE_PARAMETER struct
 * - added GEV_STATUS_PACKET_RESEND status code
 * Version: 2.1.3 Date: 29.06.2017
 * - added new parameter to GEVTestPacketResend function
 * - supported register read/write log output
 * - added GVSP_PIX_PIXEL_SIZE
 * - added new parameter to IMAGE_HEADER struct
 * - added payload types
 * Version: 2.1.4 Date: 29.08.2018
 * - added GEVGetFeaturePort function
 * - added GEVSetSchemaPath function
 * - added GEVSetTraversingFirewallsInterval function
 * - BOOL type in Objective-C is signed char. (MacOS)
 * - added representation for IP address(IP version 4) and MAC address
 */
/*****************************************************************************/
#ifndef __SPHINXLIB_H
#define __SPHINXLIB_H

#ifndef WIN32
#define WINAPI
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef u_int32_t DWORD;
typedef int64_t INT64;
typedef u_int64_t UINT64;
#ifdef __APPLE__
typedef signed char BOOL;
#else
typedef int BOOL;
#endif
typedef u_int64_t ULONGLONG;
typedef int32_t INT;
#endif


// Pixel Format
// new name 2.0
#define GVSP_PIX_MONO1P               0x01010037
#define GVSP_PIX_MONO2P               0x01020038
#define GVSP_PIX_MONO4P               0x01040039

#define GVSP_PIX_MONO8                0x01080001
#define GVSP_PIX_MONO8_SIGNED         0x01080002
// new name 2.0
#define GVSP_PIX_MONO8S               0x01080002
#define GVSP_PIX_MONO10               0x01100003
#define GVSP_PIX_MONO10_PACKED        0x010C0004
#define GVSP_PIX_MONO12               0x01100005
#define GVSP_PIX_MONO12_PACKED        0x010C0006
#define GVSP_PIX_MONO14               0x01100025
#define GVSP_PIX_MONO16               0x01100007
#define GVSP_PIX_BAYGR8               0x01080008
#define GVSP_PIX_BAYRG8               0x01080009
#define GVSP_PIX_BAYGB8               0x0108000A
#define GVSP_PIX_BAYBG8               0x0108000B
#define GVSP_PIX_BAYGR10              0x0110000C
#define GVSP_PIX_BAYRG10              0x0110000D
#define GVSP_PIX_BAYGB10              0x0110000E
#define GVSP_PIX_BAYBG10              0x0110000F
#define GVSP_PIX_BAYGR12              0x01100010
#define GVSP_PIX_BAYRG12              0x01100011
#define GVSP_PIX_BAYGB12              0x01100012
#define GVSP_PIX_BAYBG12              0x01100013
#define GVSP_PIX_BAYGR10_PACKED       0x010C0026
#define GVSP_PIX_BAYRG10_PACKED       0x010C0027
#define GVSP_PIX_BAYGB10_PACKED       0x010C0028
#define GVSP_PIX_BAYBG10_PACKED       0x010C0029
#define GVSP_PIX_BAYGR12_PACKED       0x010C002A
#define GVSP_PIX_BAYRG12_PACKED       0x010C002B
#define GVSP_PIX_BAYGB12_PACKED       0x010C002C
#define GVSP_PIX_BAYBG12_PACKED       0x010C002D
#define GVSP_PIX_BAYGR16_PACKED       0x0110002E
#define GVSP_PIX_BAYRG16_PACKED       0x0110002F
#define GVSP_PIX_BAYGB16_PACKED       0x01100030
#define GVSP_PIX_BAYBG16_PACKED       0x01100031
#define GVSP_PIX_RGB8_PACKED          0x02180014
// new name 2.0
#define GVSP_PIX_RGB8                 0x02180014

#define GVSP_PIX_BGR8_PACKED          0x02180015
// new name 2.0
#define GVSP_PIX_BGR8                 0x02180015

#define GVSP_PIX_RGBA8_PACKED         0x02200016
// new name 2.0
#define GVSP_PIX_RGBA8                 0x02200016

#define GVSP_PIX_BGRA8_PACKED         0x02200017
// new name 2.0
#define GVSP_PIX_BGRA8                0x02200017

#define GVSP_PIX_RGB10_PACKED         0x02300018
// new name 2.0
#define GVSP_PIX_RGB10                0x02300018

#define GVSP_PIX_BGR10_PACKED         0x02300019
// new name 2.0
#define GVSP_PIX_BGR10                0x02300019

#define GVSP_PIX_RGB12_PACKED         0x0230001A
// new name 2.0
#define GVSP_PIX_RGB12                0x0230001A

#define GVSP_PIX_BGR12_PACKED         0x0230001B
// new name 2.0
#define GVSP_PIX_BGR12                0x0230001B

#define GVSP_PIX_RGB16_PACKED         0x02300033
// new name 2.0
#define GVSP_PIX_RGB16                0x02300033

#define GVSP_PIX_RGB10V1_PACKED       0x0220001C
#define GVSP_PIX_RGB10V2_PACKED       0x0220001D
// new name 2.0
#define GVSP_PIX_RGB10P32             0x0220001D

#define GVSP_PIX_RGB12V1_PACKED       0x02240034

#define GVSP_PIX_RGB565_PACKED        0x02100035
// new name 2.0
#define GVSP_PIX_RGB565P              0x02100035

#define GVSP_PIX_BGR565_PACKED        0x02100036
// new name 2.0
#define GVSP_PIX_BGR565P              0x02100036


#define GVSP_PIX_YUV411_PACKED        0x020C001E
// new name 2.0
#define GVSP_PIX_YUV411_8_UYYVYY      0x020C001E

#define GVSP_PIX_YUV422_PACKED        0x0210001F
// new name 2.0
#define GVSP_PIX_YUV422_8_UYVY        0x0210001F

#define GVSP_PIX_YUV422_YUYV_PACKED   0x02100032
// new name 2.0
#define GVSP_PIX_YUV422_8             0x02100032

#define GVSP_PIX_YUV444_PACKED        0x02180020
// new name 2.0
#define GVSP_PIX_YUV8_UYV             0x02180020

#define GVSP_PIX_YCBCR_8_CBYCR        0x0218003A

#define GVSP_PIX_YCBCR422_8           0x0210003B

#define GVSP_PIX_YCBCR422_8_CBYCRY    0x02100043

#define GVSP_PIX_YCBCR411_8_CBYYCRYY  0x020C003C

#define GVSP_PIX_YCbCr601_8_CbYCr     0x0218003D
#define GVSP_PIX_YCbCr601_422_8       0x0218003E
#define GVSP_PIX_YCbCr601_422_8_CbYCrY    0x02100044
#define GVSP_PIX_YCbCr601_422_8_CbYYCrYY  0x020C003F


#define GVSP_PIX_YCBCR709_8_CBYCR         0x02180040
#define GVSP_PIX_YCbCr709_422_8           0x02100041
#define GVSP_PIX_YCbCr709_422_8_CbYCrY    0x02100045
#define GVSP_PIX_YCbCr709_422_8_CbYYCrYY  0x020C0042

#define GVSP_PIX_RGB8_PLANAR          0x02180021
#define GVSP_PIX_RGB10_PLANAR         0x02300022
#define GVSP_PIX_RGB12_PLANAR         0x02300023
#define GVSP_PIX_RGB16_PLANAR         0x02300024

#define GVSP_PIX_EFFECTIVE_PIXEL_SIZE_MASK  0x00FF0000
#define GVSP_PIX_EFFECTIVE_PIXEL_SIZE_SHIFT 16
#define GVSP_PIX_PIXEL_SIZE(X) ((X & GVSP_PIX_EFFECTIVE_PIXEL_SIZE_MASK) >> GVSP_PIX_EFFECTIVE_PIXEL_SIZE_SHIFT)

// Standard Status Codes
#define GEV_STATUS_SUCCESS                              0x0000
#define GEV_STATUS_PACKET_RESEND                        0x0100
#define GEV_STATUS_NOT_IMPLEMENTED                      0x8001
#define GEV_STATUS_INVALID_PARAMETER                    0x8002
#define GEV_STATUS_INVALID_ADDRESS                      0x8003
#define GEV_STATUS_WRITE_PROTECT                        0x8004
#define GEV_STATUS_BAD_ALIGNMENT                        0x8005
#define GEV_STATUS_ACCESS_DENIED                        0x8006
#define GEV_STATUS_BUSY                                 0x8007
#define GEV_STATUS_LOCAL_PROBLEM                        0x8008
#define GEV_STATUS_MSG_MISMATCH                         0x8009
#define GEV_STATUS_INVALID_PROTOCOL                     0x800A
#define GEV_STATUS_NO_MSG                               0x800B
#define GEV_STATUS_PACKET_UNAVAILABLE                   0x800C
#define GEV_STATUS_DATA_OVERRUN                         0x800D
#define GEV_STATUS_INVALID_HEADER                       0x800E
#define GEV_STATUS_WRONG_CONFIG                         0x800F
#define GEV_STATUS_PACKET_NOT_YET_AVAILABLE             0x8010
#define GEV_STATUS_PACKET_AND_PREV_REMOVED_FROM_MEMORY  0x8011
#define GEV_STATUS_PACKET_REMOVED_FROM_MEMORY           0x8012
#define GEV_STATUS_NO_REF_TIME1                         0x8013
#define GEV_STATUS_PACKET_TEMPORARILY_UNAVAILABLE       0x8014
#define GEV_STATUS_OVERFLOW                             0x8015
#define GEV_STATUS_ACTION_LATE                          0x8016
#define GEV_STATUS_ERROR                                0x8FFF

// Device-Specific Status Code
#define GEV_STATUS_CAMERA_NOT_INIT                0xC001
#define GEV_STATUS_CAMERA_ALWAYS_INIT             0xC002
#define GEV_STATUS_CANNOT_CREATE_SOCKET           0xC003
#define GEV_STATUS_SEND_ERROR                     0xC004
#define GEV_STATUS_RECEIVE_ERROR                  0xC005
#define GEV_STATUS_CAMERA_NOT_FOUND               0xC006
#define GEV_STATUS_CANNOT_ALLOC_MEMORY            0xC007
#define GEV_STATUS_TIMEOUT                        0xC008
#define GEV_STATUS_SOCKET_ERROR                   0xC009
#define GEV_STATUS_INVALID_ACK                    0xC00A
#define GEV_STATUS_CANNOT_START_THREAD            0xC00B
#define GEV_STATUS_CANNOT_SET_SOCKET_OPT          0xC00C
#define GEV_STATUS_CANNOT_OPEN_DRIVER             0xC00D
#define GEV_STATUS_HEARTBEAT_READ_ERROR           0xC00E
#define GEV_STATUS_EVALUATION_EXPIRED             0xC00F
#define GEV_STATUS_GRAB_ERROR                     0xC010
#define GEV_STATUS_DRIVER_READ_ERROR              0xC011
#define GEV_STATUS_XML_READ_ERROR                 0xC012
#define GEV_STATUS_XML_OPEN_ERROR                 0xC013
#define GEV_STATUS_XML_FEATURE_ERROR              0xC014
#define GEV_STATUS_XML_COMMAND_ERROR              0xC015
#define GEV_STATUS_GAIN_NOT_SUPPORTED             0xC016
#define GEV_STATUS_EXPOSURE_NOT_SUPPORTED         0xC017
#define GEV_STATUS_CANNOT_GET_ADAPTER_INFO        0xC018
#define GEV_STATUS_ERROR_INVALID_HANDLE           0xC019
#define GEV_STATUS_CLINK_SET_BAUD                 0xC01A
#define GEV_STATUS_CLINK_SEND_BUFFER_FULL         0xC01B
#define GEV_STATUS_CLINK_RECEIVE_BUFFER_NO_DATA   0xC01C
#define GEV_STATUS_FEATURE_NOT_AVAILABLE          0xC01D
#define GEV_STATUS_MATH_PARSER_ERROR              0xC01E
#define GEV_STATUS_FEATURE_ITEM_NOT_AVAILABLE     0xC01F
#define GEV_STATUS_NOT_SUPPORTED                  0xC020
#define GEV_STATUS_GET_URL_ERROR                  0xC021
#define GEV_STATUS_READ_XML_MEM_ERROR             0xC022
#define GEV_STATUS_XML_SIZE_ERROR                 0xC023
#define GEV_STATUS_XML_ZIP_ERROR                  0xC024
#define GEV_STATUS_XML_ROOT_ERROR                 0xC025
#define GEV_STATUS_XML_FILE_ERROR                 0xC026
#define GEV_STATUS_DIFFERENT_IMAGE_HEADER         0xC027
#define GEV_STATUS_XML_SCHEMA_ERROR               0xC028
#define GEV_STATUS_XML_STYLESHEET_ERROR           0xC029
#define GEV_STATUS_FEATURE_LIST_ERROR             0xC02A
#define GEV_STATUS_ALREADY_OPEN                   0xC02B
#define GEV_STATUS_TEST_PACKET_DATA_ERROR         0xC02C
#define GEV_STATUS_FEATURE_NOT_FLOAT              0xC02D
#define GEV_STATUS_FEATURE_NOT_INTEGER            0xC02E
#define GEV_STATUS_XML_DLL_NOT_FOUND              0xC02F
#define GEV_STATUS_XML_NOT_INIT                   0xC030
#define GEV_STATUS_NOT_SAME_SUBNET                0xC031

// event message channel
#define EVENT_TRIGGER               0x0002
#define EVENT_START_EXPOSUE         0x0003
#define EVENT_STOP_EXPOSUE          0x0004
#define EVENT_START_TRANSFER        0x0005
#define EVENT_STOP_TRANSFER         0x0006
#define EVENT_PRIMARY_APP_SWITCH    0x0007
#define EVENT_LINK_SPEED_CHANGE     0x0008
#define EVENT_ACTION_LATE           0x0009
#define EVENT_ERROR_BEGIN           0x8001
#define EVENT_ERROR_END             0x8FFF
#define EVENT_DEVICE_SPECIFIC       0x9000

// Port's
#define DEFAULT_PDATA       2001
#define DEFAULT_PCTRL       2000

#define FEATURE_NAME_COUNT 100

#define TYPE_CATEGORY     0
#define TYPE_FEATURE      1
#define TYPE_INTEGER      2
#define TYPE_FLOAT        3
#define TYPE_STRING       4
#define TYPE_ENUMERATION  5
#define TYPE_COMMAND      6
#define TYPE_BOOLEAN      7
#define TYPE_REGISTER     8
#define TYPE_PORT         9

#define ACCESS_MODE_RO    0x524F
#define ACCESS_MODE_RW    0x5257
#define ACCESS_MODE_WO    0x574F

#define VISIBILITY_INVISIBLE 0
#define VISIBILITY_BEGINNER  1
#define VISIBILITY_EXPERT    2
#define VISIBILITY_GURU      3

#define SIGN_UNSIGNED   0
#define SIGN_SIGNED     1

#define REPRESENTATION_LINEAR       0 // Slider with linear behaviour
#define REPRESENTATION_LOGARITHMIC  1 // Slider with logarithmic behaviour
#define REPRESENTATION_BOOLEAN      2 // Checkbox
#define REPRESENTATION_PURE_NUMBER  3 // Decimal number in an edit control
#define REPRESENTATION_HEX_NUMBER   4 // Hex number in an edit control
#define REPRESENTATION_UNDEFINDED   5 // Undefinded Representation
#define REPRESENTATION_IPV4ADDRESS  6 // IP address(IP version 4)
#define REPRESENTATION_MACADDRESS   7 // MAC address

#define DISPLAY_NOTATION_AUTOMATIC  0
#define DISPLAY_NOTATION_FIXED      1
#define DISPLAY_NOTATION_SCIENTIFIC 2

#define HEX_NUMBER    1

#define DISCOVERY_STATUS_OK               0
#define DISCOVERY_STATUS_ALREADY_OPEN    1
#define DISCOVERY_STATUS_NOT_SAME_SUBNET  2
#define DISCOVERY_STATUS_CONTROL_OPEN     3

#define CONNECTION_STATUS_OK                 0
#define CONNECTION_STATUS_TIMEOUT            1
#define CONNECTION_STATUS_ACCESS_DENIED      2

#define OPEN_ACCESS       0
#define EXCLUSIVE_ACCESS  1
#define CONTROL_ACCESS    2

#define DETAILED_LOG_OFF       0
#define DETAILED_LOG_INFO      1
#define DETAILED_LOG_WARNING   2
#define DETAILED_LOG_ERROR     4
#define DETAILED_LOG_REGISTER  8

#define MAX_ADAPTER_NAME_LEN   256 // arb.

#define PAYLOAD_TYPE_IMAGE                  0x0001
#define PAYLOAD_TYPE_IMAGE_EXTENDED_CHUNK   0x4001
#define PAYLOAD_TYPE_CHUNK                  0x0004

typedef struct _FeatureList FeatureList;
typedef FeatureList *FeatureListPtr;
struct _FeatureList {
    FeatureListPtr Next;
    DWORD Index;
    char *Name;
//    char *DisplayName;
//    char *ToolTip;
    BYTE Type;
    BYTE Level;
//    BYTE Visibility;
};

typedef struct
{
  BYTE Type;
  INT64 Min;
  INT64 Max;
  DWORD OnValue;
  DWORD OffValue;
  WORD AccessMode;
  WORD Representation;
  DWORD Inc;
  DWORD CommandValue;
  DWORD Length;
  BYTE EnumerationCount;
  BYTE Visibility;
  double FloatMin;
  double FloatMax;
  double FloatInc;
  BYTE IsImplemented;
  BYTE IsAvailable;
  BYTE IsLocked;
  BYTE Sign;
  DWORD Address;
  BYTE DisplayNotation;
  BYTE DisplayPrecision;
  BYTE InvalidatorCount;
  INT64 PollingTime;
} FEATURE_PARAMETER;

typedef struct
{
    DWORD IP;
    BYTE manuf[32];
    BYTE model[32];
    BYTE version[32];
    DWORD AdapterIP;
    DWORD AdapterMask;
    BYTE Mac[6];
    DWORD subnet;
    DWORD gateway;
    char adapter_name[MAX_ADAPTER_NAME_LEN + 4];
    BYTE serial[16];
    BYTE userdef_name[16];
    BYTE status;
} DEVICE_PARAM;

typedef struct
{
    BYTE Count;
    DEVICE_PARAM param[20];
} DISCOVERY;

typedef struct
{
    DWORD IP_CANCam;
    WORD PortData;
    WORD PortCtrl;
    DWORD AdapterIP;
    DWORD AdapterMask;
    char adapter_name[MAX_ADAPTER_NAME_LEN + 4];
} CONNECTION;

typedef struct
{
    BYTE receive_count;
    BYTE transmit_count;
    BYTE transmit_buffer_full;
    BYTE transmit_buffer_empty;
    BYTE reveice_buffer_full;
    BYTE reveice_data_available;
} CLINK_STATUS;

typedef struct
{
    DWORD cc_heartbeat_timeout;
    DWORD cc_timeout;
    BYTE cc_retry;
    DWORD sc_timeout;
    BYTE sc_packet_resend;
    DWORD sc_image_wait_timeout;
} CHANNEL_PARAMETER;

typedef struct
{
  INT64 FrameCounter;
  ULONGLONG TimeStamp;
  DWORD PixelType;
  DWORD SizeX;
  DWORD SizeY;
  DWORD OffsetX;
  DWORD OffsetY;
  WORD PaddingX;
  WORD PaddingY;
  INT  MissingPacket;
  WORD PayloadType;
  DWORD ChunkDataPayloadLength;
  DWORD ChunkLayoutId;
} IMAGE_HEADER,*pIMAGE_HEADER;

typedef struct
{
  WORD  EventID;
  WORD  StreamChannelIndex;
  ULONGLONG  BlockID;
  ULONGLONG  TimeStamp;
  BYTE  *Data;
  WORD DataLength;
} MESSAGECHANNEL_PARAMETER;

typedef BYTE ( WINAPI *DISCOVERY_CALLBACK_FUNC )(int s_cnt);
typedef BYTE ( WINAPI *MESSAGECHANNEL_CALLBACK_FUNC )(BYTE cam_nr, MESSAGECHANNEL_PARAMETER mcparam);
typedef BYTE ( WINAPI *ERROR_CALLBACK_FUNC )(BYTE cam_nr, char *error_str);
typedef BYTE ( WINAPI *SECURE_TRANSFER_CALLBACK_FUNC )(BYTE cam_nr);
typedef BYTE ( WINAPI *READ_WRITE_MEM_CALLBACK_FUNC )(BYTE cam_nr, int s_cnt);

#ifdef __cplusplus
extern "C" {
#endif

// *************** General functions *********************
WORD WINAPI GEVDiscovery(DISCOVERY *dis, DISCOVERY_CALLBACK_FUNC c_func, DWORD d_timeout, BOOL ignore_subnet);
WORD WINAPI GEVInit(BYTE cam_nr, CONNECTION *con, ERROR_CALLBACK_FUNC error_func, BYTE save_xml, BYTE open_mode);
WORD WINAPI GEVClose(BYTE cam_nr);

WORD WINAPI GEVOpenStreamChannel(BYTE cam_nr, DWORD ip, WORD port, DWORD multicast);
WORD WINAPI GEVCloseStreamChannel(BYTE cam_nr);

WORD WINAPI GEVInitFilterDriver(BYTE cam_nr);
WORD WINAPI GEVCloseFilterDriver(BYTE cam_nr);

WORD WINAPI GEVWriteRegister(BYTE cam_nr, DWORD cmd, BYTE cnt, DWORD *pptr);
WORD WINAPI GEVReadRegister(BYTE cam_nr, DWORD cmd, BYTE cnt, DWORD *pptr);

WORD WINAPI GEVWriteMemory(BYTE cam_nr, DWORD maddr, DWORD cnt, BYTE *pptr);
WORD WINAPI GEVReadMemory(BYTE cam_nr, DWORD maddr, DWORD cnt, BYTE *pptr);

WORD WINAPI GEVSetReadWriteMemoryCallback(BYTE cam_nr, READ_WRITE_MEM_CALLBACK_FUNC c_func);

#ifdef WIN32
WORD WINAPI GEVGetPixelPtr(BYTE cam_nr, DWORD offset, UINT64 *ptradr);
#else
WORD WINAPI GEVGetPixelPtr(BYTE cam_nr, DWORD offset, unsigned long *ptradr);
#endif

WORD WINAPI GEVSetMemorySize(BYTE cam_nr, DWORD mem_size);
WORD WINAPI GEVGetMemorySize(BYTE cam_nr, DWORD *mem_size);

WORD WINAPI GEVSetMessageChannelCallback(BYTE cam_nr, MESSAGECHANNEL_CALLBACK_FUNC c_func);

WORD WINAPI GEVSetReadWriteParameter(BYTE cam_nr, DWORD ack_timeout, BYTE retry_count);
WORD WINAPI GEVGetReadWriteParameter(BYTE cam_nr, DWORD *ack_timeout, BYTE *retry_count);

WORD WINAPI GEVSetNetConfig(BYTE cam_nr, BYTE dhcp, DWORD ip, DWORD subnet, DWORD gateway);
WORD WINAPI GEVGetNetConfig(BYTE cam_nr, BYTE *dhcp, DWORD *ip, DWORD *subnet, DWORD *gateway);

WORD WINAPI GEVForceIp(DWORD new_ip, DWORD subnet, DWORD gateway, BYTE *mac);

WORD WINAPI GEVGetFilterDriverVersion(BYTE cam_nr, BYTE *version_major, BYTE *version_minor);

WORD WINAPI GEVSetChannelParameter(BYTE cam_nr, CHANNEL_PARAMETER cparam);
WORD WINAPI GEVGetChannelParameter(BYTE cam_nr, CHANNEL_PARAMETER *cparam);

WORD WINAPI GEVSetHeartbeatRate(BYTE cam_nr, DWORD heartbeat_rate);
WORD WINAPI GEVGetHeartbeatRate(BYTE cam_nr, DWORD *heartbeat_rate);

WORD WINAPI GEVTestPacket(BYTE cam_nr, DWORD *packet_size);
WORD WINAPI GEVGetConnectionStatus(BYTE cam_nr, BYTE *status, BYTE *eval);

WORD WINAPI GEVGetTimeOneTick(BYTE cam_nr, double *ttimes);

WORD WINAPI GEVGetDetailedLog(BYTE cam_nr, BYTE *on_off);
WORD WINAPI GEVSetDetailedLog(BYTE cam_nr, BYTE on_off);

WORD WINAPI GEVSetActionCommand(BYTE cam_nr, DWORD device_key, DWORD group_key, DWORD group_mask, DWORD action_time);

WORD WINAPI GEVSetTraversingFirewallsInterval(BYTE cam_nr, DWORD interval);

// ***************** xml functions ******************
WORD WINAPI GEVGetFeatureList(BYTE cam_nr, FeatureListPtr *featureListPtr, BYTE *maxLevel );

WORD WINAPI GEVGetFeatureParameter(BYTE cam_nr, char *feature_name, FEATURE_PARAMETER *f_param);

WORD WINAPI GEVGetFeatureInteger(BYTE cam_nr, char *feature_name, INT64 *int_value);
WORD WINAPI GEVSetFeatureInteger(BYTE cam_nr, char *feature_name, INT64 int_value);

WORD WINAPI GEVGetFeatureString(BYTE cam_nr, char *feature_name, char *str_value);
WORD WINAPI GEVSetFeatureString(BYTE cam_nr, char *feature_name, char *str_value);

WORD WINAPI GEVGetFeatureBoolean(BYTE cam_nr, char *feature_name, DWORD *bool_value);
WORD WINAPI GEVSetFeatureBoolean(BYTE cam_nr, char *feature_name, DWORD bool_value);

WORD WINAPI GEVGetFeatureCommand(BYTE cam_nr, char *feature_name, DWORD *cmd_value);
WORD WINAPI GEVSetFeatureCommand(BYTE cam_nr, char *feature_name, DWORD cmd_value);

WORD WINAPI GEVSetFeatureEnumeration(BYTE cam_nr, char *feature_name, char *enum_name, int str_len);
WORD WINAPI GEVGetFeatureEnumeration(BYTE cam_nr, char *feature_name, char *enum_name, int str_len);
WORD WINAPI GEVGetFeatureEnumerationName(BYTE cam_nr, char *feature_name, BYTE enum_index, char *enum_name, int str_len);

WORD WINAPI GEVGetFeatureFloat(BYTE cam_nr, char *feature_name, double *float_value);
WORD WINAPI GEVSetFeatureFloat(BYTE cam_nr, char *feature_name, double float_value);

WORD WINAPI GEVGetFeatureDisplayName(BYTE cam_nr, char *feature_name, char *display_name, int display_name_length);
WORD WINAPI GEVGetFeatureTooltip(BYTE cam_nr, char *feature_name, char *tooltip_name, int tooltip_name_length);

WORD WINAPI GEVSetXmlFile(BYTE cam_nr,char *xml_name);
WORD WINAPI GEVInitXml(BYTE cam_nr);

WORD WINAPI GEVGetFeatureRegister(BYTE cam_nr, char *feature_name, DWORD len, BYTE *pbuffer);
WORD WINAPI GEVSetFeatureRegister(BYTE cam_nr, char *feature_name,  DWORD len, BYTE *pbuffer);

WORD WINAPI GEVGetFeatureUnit(BYTE cam_nr, char *feature_name, char *unit_name, int unit_name_length);

WORD WINAPI GEVGetFeatureEnableStatus(BYTE cam_nr, char *feature_name, BYTE *enable);

WORD WINAPI GEVGetXmlSize(BYTE cam_nr, DWORD *size);
WORD WINAPI GEVGetXmlFile(BYTE cam_nr, BYTE **xmlfile);

WORD WINAPI GEVGetFeatureInvalidator(BYTE cam_nr, char *feature_name, BYTE index, char *invalidator_name, int str_len);

WORD WINAPI GEVGetFeaturePort(BYTE cam_nr, char *feature_name, char *port_name, int port_name_length);

WORD WINAPI GEVSetSchemaPath(BYTE cam_nr, char *schema_path);

// ***************** Camera functions ******************
WORD WINAPI GEVAcquisitionStart(BYTE cam_nr, DWORD number_images_to_acquire);
WORD WINAPI GEVAcquisitionStartEx(BYTE cam_nr, DWORD number_images_to_acquire, DWORD image_size);
WORD WINAPI GEVAcquisitionStop(BYTE cam_nr);

WORD WINAPI GEVGetImage(BYTE cam_nr, IMAGE_HEADER *image_header);
WORD WINAPI GEVGetImageBuffer(BYTE cam_nr, IMAGE_HEADER *image_header, BYTE *image_buffer);

WORD WINAPI GEVGetImageRingBuffer(BYTE cam_nr, IMAGE_HEADER *image_header, BYTE *image_buffer_index);
WORD WINAPI GEVQueueRingBuffer(BYTE cam_nr, BYTE image_buffer_index);
WORD WINAPI GEVSetRingBuffer(BYTE cam_nr, WORD index, void *buffer);
WORD WINAPI GEVReleaseRingBuffer(BYTE cam_nr);

WORD WINAPI GEVSetPacketResend(BYTE cam_nr, BYTE enable);
WORD WINAPI GEVGetPacketResend(BYTE cam_nr, BYTE *enable);
WORD WINAPI GEVPacketResend(BYTE cam_nr, WORD stream_channel, INT64 block_id, DWORD first_packet_id, DWORD last_packet_id);

WORD WINAPI GEVGetImageFPS(BYTE cam_nr, double *fps);

WORD WINAPI GEVSetBufferCount(BYTE cam_nr, WORD count);
WORD WINAPI GEVGetBufferCount(BYTE cam_nr, WORD *count);
WORD WINAPI GEVGetFreeBufferCount(BYTE cam_nr, WORD *count);

WORD WINAPI GEVSetSecureTransfer(BYTE cam_nr, BYTE enable, SECURE_TRANSFER_CALLBACK_FUNC c_func);
WORD WINAPI GEVGetSecureTransfer(BYTE cam_nr, BYTE *enable);

WORD WINAPI GEVGetPacketsOutOfOrder(BYTE cam_nr, BYTE *packets_out_of_order);
WORD WINAPI GEVSetPacketsOutOfOrder(BYTE cam_nr, BYTE packets_out_of_order);

// ***************** CANCam Camera functions ******************
WORD WINAPI CANCamGetPixelFormat(BYTE cam_nr,DWORD *pixfmt);
WORD WINAPI CANCamGetMaxVideoWindow(BYTE cam_nr,WORD *max_width, WORD *max_height);
WORD WINAPI CANCamGetVideoWindow(BYTE cam_nr,WORD *x_start, WORD *y_start, WORD *x_len, WORD *y_len);
WORD WINAPI CANCamSetVideoWindow(BYTE cam_nr,WORD x_start, WORD y_start, WORD x_len, WORD y_len);
WORD WINAPI CANCamGetGain(BYTE cam_nr,BYTE *gain);
WORD WINAPI CANCamSetGain(BYTE cam_nr,BYTE gain);
WORD WINAPI CANCamGetGainMinMax(BYTE cam_nr,BYTE *gain_min, BYTE *gain_max);
WORD WINAPI CANCamSetGainAuto(BYTE cam_nr,BYTE on_off);
WORD WINAPI CANCamGetExposureTime(BYTE cam_nr,DWORD *exposure_time);
WORD WINAPI CANCamSetExposureTime(BYTE cam_nr,DWORD exposure_time);
WORD WINAPI CANCamGetExposureTimeMinMax(BYTE cam_nr,DWORD *exposure_min, DWORD *exposure_max);
WORD WINAPI CANCamSetExposureTimeAuto(BYTE cam_nr,BYTE on_off);
WORD WINAPI CANCamSetLut(BYTE cam_nr,WORD index, BYTE value);
WORD WINAPI CANCamGetLut(BYTE cam_nr,WORD index, BYTE *value);

// ***************** CANCam Camera Link functions ******************
WORD WINAPI CANCamInitClinkSerial(BYTE cam_nr,DWORD baud);
WORD WINAPI CANCamGetClinkStatus(BYTE cam_nr, CLINK_STATUS *status);
WORD WINAPI CANCamSendClink(BYTE cam_nr, WORD count, BYTE *send_buffer);
WORD WINAPI CANCamReceiveClink(BYTE cam_nr, WORD count_buffer, BYTE *recv_buffer, WORD *recv_count);


// ***************** Test functions ******************
WORD WINAPI GEVTestPacketResend(BYTE cam_nr, BYTE on_off, WORD packet_number, WORD count);
WORD WINAPI GEVTestFindMaxPacketSize(BYTE cam_nr, WORD *packet_size, WORD ps_min, WORD ps_max, WORD ps_inc);

// old functions
#define discovery_cancam GEVDiscovery
#define init_cancam GEVInit
#define close_cancam GEVClose
#define init_filter_driver GEVInitFilterDriver
#define close_filter_driver GEVCloseFilterDriver
#define send_val GEVWriteRegister
#define receive GEVReadRegister
#define get_pixel_ptr GEVGetPixelPtr
#define set_memory_size GEVSetMemorySize
#define get_memory_size GEVGetMemorySize
#define set_lut CANCamSetLut
#define get_lut CANCamGetLut
#define set_message_channel_callback GEVSetMessageChannelCallback
#define receive_memory GEVReadMemory
#define send_memory GEVWriteMemory
#define set_net_config GEVSetNetConfig
#define get_net_config GEVGetNetConfig
#define force_ip GEVForceIp
#define set_read_write_parameter GEVSetReadWriteParameter
#define get_read_write_parameter GEVGetReadWriteParameter
#define set_heartbeat_rate GEVSetHeartbeatrate
#define get_heartbeat_rate GEVGetHeartbeatrate
#define test_packet GEVTestPacket
#define get_filter_driver_version GEVGetFilterDriverVersion
#define set_channel_parameter GEVSetChannelParameter
#define get_channel_parameter GEVGetChannelParameter

WORD WINAPI get_root_count(BYTE cam_nr, BYTE *root_cnt);
WORD WINAPI get_root_name(BYTE cam_nr, BYTE root_index, char *root_name, int str_len);
WORD WINAPI get_feature_count(BYTE cam_nr, BYTE root_index, BYTE *feature_cnt);
WORD WINAPI get_feature_name(BYTE cam_nr, BYTE root_index, BYTE feature_index, char *feature_name, int str_len);
WORD WINAPI get_sub_feature_name(BYTE cam_nr, BYTE root_index, BYTE feature_index, BYTE sub_feature_index, char *sub_feature_name, int str_len);
WORD WINAPI get_sub_feature_count(BYTE cam_nr, BYTE root_index, BYTE feature_index, BYTE *sub_feature_cnt);
WORD WINAPI get_subsub_feature_count(BYTE cam_nr, BYTE root_index, BYTE feature_index, BYTE sub_feature_index, BYTE *subsub_feature_cnt);
WORD WINAPI get_subsub_feature_name(BYTE cam_nr, BYTE root_index, BYTE feature_index, BYTE sub_feature_index, BYTE subsub_feature_index, char *subsub_feature_name, int str_len);

#define get_feature_parameter GEVGetFeatureParameter
#define get_feature_integer GEVGetFeatureInteger
#define set_feature_integer GEVSetFeatureInteger
#define get_feature_string GEVGetFeatureString
#define set_feature_string GEVSetFeatureString
#define get_feature_boolean GEVGetFeatureBoolean
#define set_feature_boolean GEVSetFeatureBoolean
#define get_feature_command GEVGetFeatureCommand
#define set_feature_command GEVSetFeatureCommand
#define set_feature_enumeration GEVSetFeatureEnumeration
#define get_feature_enumeration GEVGetFeatureEnumeration
#define get_feature_enumeration_name GEVGetFeatureEnumerationName
#define get_feature_float GEVGetFeatureFloat
#define set_feature_float GEVSetFeatureFloat
#define get_feature_display_name GEVGetFeatureDisplayName
#define get_feature_tooltip GEVGetFeatureTooltip
#define set_xml_file GEVSetXmlFile
#define get_feature_register GEVGetFeatureRegister
#define set_feature_register GEVSetFeatureRegister
#define get_feature_unit GEVGetFeatureUnit
#define get_feature_enable_status GEVGetFeatureEnableStatus

#define start_grab GEVAcquisitionStart
#define stop_grab GEVAcquisitionStop
WORD WINAPI grab_image(BYTE cam_nr,DWORD offset, DWORD *t_cnt);
WORD WINAPI get_image_header(BYTE cam_nr, IMAGE_HEADER *image_header);
#define set_packet_resend GEVSetPacketResend
#define get_packet_resend GEVGetPacketResend
#define get_image_fps GEVGetImageFPS

#define get_pixel_format CANCamGetPixelFormat
#define get_max_video_window CANCamGetMaxVideoWindow
#define get_video_window CANCamGetVideoWindow
#define set_video_window CANCamSetVideoWindow
#define get_gain CANCamGetGain
#define set_gain CANCamSetGain
#define get_gain_min_max CANCamGetGainMinMax
#define set_gain_auto CANCamSetGainAuto
#define get_exposure_time CANCamGetExposureTime
#define set_exposure_time CANCamSetExposureTime
#define get_exposure_time_min_max CANCamGetExposureTimeMinMax
#define set_exposure_time_auto CANCamSetExposureTimeAuto
#define set_lut CANCamSetLut
#define get_lut CANCamGetLut

#define init_clink_serial CANCamInitClinkSerial
#define get_clink_status CANCamGetClinkStatus
#define send_clink CANCamSendClink
#define receive_clink CANCamReceiveClink


#ifdef __cplusplus
}
#endif

#endif
