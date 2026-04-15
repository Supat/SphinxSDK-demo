#ifndef bayer_h
#define bayer_h
#include <stdint.h>

#ifndef __c99
#define restrict __restrict__
#endif
#ifdef _WIN32
#define restrict __restrict
#endif

#define RGB_PSIZE 3
typedef enum {
    BAYER_METHOD_NEAREST=0,
    BAYER_METHOD_SIMPLE,
    BAYER_METHOD_BILINEAR,
    BAYER_METHOD_HQLINEAR,
    BAYER_METHOD_DOWNSAMPLE,
    BAYER_METHOD_EDGESENSE,
    BAYER_METHOD_VNG,
    BAYER_METHOD_AHD
} dc1394bayer_method_t;

typedef enum {
    BAYER_COLOR_FILTER_RGGB = 512,
    BAYER_COLOR_FILTER_GBRG,
    BAYER_COLOR_FILTER_GRBG,
    BAYER_COLOR_FILTER_BGGR
} dc1394color_filter_t ;
#define BAYER_COLOR_FILTER_MIN        BAYER_COLOR_FILTER_RGGB
#define BAYER_COLOR_FILTER_MAX        BAYER_COLOR_FILTER_BGGR
#define BAYER_COLOR_FILTER_NUM       (BAYER_COLOR_FILTER_MAX - BAYER_COLOR_FILTER_MIN + 1)

/**
 * Error codes returned by most libdc1394 functions.
 *
 * General rule: 0 is success, negative denotes a problem.
 */
typedef enum {
    BAYER_SUCCESS                     =  0,
    BAYER_FAILURE                     = -1,
    BAYER_NOT_A_CAMERA                = -2,
    BAYER_FUNCTION_NOT_SUPPORTED      = -3,
    BAYER_CAMERA_NOT_INITIALIZED      = -4,
    BAYER_MEMORY_ALLOCATION_FAILURE   = -5,
    BAYER_TAGGED_REGISTER_NOT_FOUND   = -6,
    BAYER_NO_ISO_CHANNEL              = -7,
    BAYER_NO_BANDWIDTH                = -8,
    BAYER_IOCTL_FAILURE               = -9,
    BAYER_CAPTURE_IS_NOT_SET          = -10,
    BAYER_CAPTURE_IS_RUNNING          = -11,
    BAYER_RAW1394_FAILURE             = -12,
    BAYER_FORMAT7_ERROR_FLAG_1        = -13,
    BAYER_FORMAT7_ERROR_FLAG_2        = -14,
    BAYER_INVALID_ARGUMENT_VALUE      = -15,
    BAYER_REQ_VALUE_OUTSIDE_RANGE     = -16,
    BAYER_INVALID_FEATURE             = -17,
    BAYER_INVALID_VIDEO_FORMAT        = -18,
    BAYER_INVALID_VIDEO_MODE          = -19,
    BAYER_INVALID_FRAMERATE           = -20,
    BAYER_INVALID_TRIGGER_MODE        = -21,
    BAYER_INVALID_TRIGGER_SOURCE      = -22,
    BAYER_INVALID_ISO_SPEED           = -23,
    BAYER_INVALID_IIDC_VERSION        = -24,
    BAYER_INVALID_COLOR_CODING        = -25,
    BAYER_INVALID_COLOR_FILTER        = -26,
    BAYER_INVALID_CAPTURE_POLICY      = -27,
    BAYER_INVALID_ERROR_CODE          = -28,
    BAYER_INVALID_BAYER_METHOD        = -29,
    BAYER_INVALID_VIDEO1394_DEVICE    = -30,
    BAYER_INVALID_OPERATION_MODE      = -31,
    BAYER_INVALID_TRIGGER_POLARITY    = -32,
    BAYER_INVALID_FEATURE_MODE        = -33,
    BAYER_INVALID_LOG_TYPE            = -34,
    BAYER_INVALID_BYTE_ORDER          = -35,
    BAYER_INVALID_STEREO_METHOD       = -36,
    BAYER_BASLER_NO_MORE_SFF_CHUNKS   = -37,
    BAYER_BASLER_CORRUPTED_SFF_CHUNK  = -38,
    BAYER_BASLER_UNKNOWN_SFF_CHUNK    = -39
} bayer_error_t;
#define BAYER_ERROR_MIN  BAYER_BASLER_UNKNOWN_SFF_CHUNK
#define BAYER_ERROR_MAX  BAYER_SUCCESS
#define BAYER_ERROR_NUM (BAYER_ERROR_MAX-BAYER_ERROR_MIN+1)

typedef enum {
    BAYER_FALSE= 0,
    BAYER_TRUE
} dc1394bool_t;



#ifdef __cplusplus
extern "C" {
#endif
bayer_error_t
bayer_Bilinear(const uint8_t *__restrict bayer, uint8_t *__restrict rgb, int sx, int sy, int tile);
//bayer_Bilinear(const uint8_t * bayer, uint8_t * rgb, uint32_t sx, uint32_t sy, dc1394color_filter_t tile);

#ifdef __cplusplus
}
#endif //extern "C"


#endif
