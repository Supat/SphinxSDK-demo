/* darknoise.h
 * subtract dark_img from src_img and copy the result to dest_img (OpenMP)
 * (c) 2016 MRC Systems GmbH
 * Author:sf
 * Version 1.0
 */
#ifndef DARKNOISE_H
#define DARKNOISE_H

// for compatibility with MSC
#ifdef _MSC_VER
#include <stdint.h>
typedef unsigned char     u_char;
typedef unsigned long     u_long;
#endif


#ifdef __cplusplus
extern "C" {
#endif
void darknoise_bw_subtract(u_char *dest_img,u_char *src_img, u_char * dark_img, int32_t width, int32_t height, int64_t img_size);
#ifdef __cplusplus
}
#endif //extern "C"

#endif //DARKNOISE_H
