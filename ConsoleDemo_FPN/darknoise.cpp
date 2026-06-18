/* darknoise.c
 * subtract dark_img from src_img and copy the result to dest_img (OpenMP)
 * (c) 2016 MRC Systems GmbH
 * Author:sf
 * Version 1.0
 */

#include "darknoise.h"

void darknoise_bw_subtract(u_char *dest_img,u_char *src_img, u_char * dark_img, int32_t width, int32_t height, int64_t img_size)
{
    int y,x;
    u_char *src_p, *dest_p, *dark_p;
    const u_char factor =3; // divided by 2->1.5
    const u_char shiftright = 1;
    short value;

     {
#pragma omp parallel private(src_p,dest_p,dark_p,x,value)  num_threads(4)
      {
#pragma omp for schedule(static, 10)
        for (y=0 ; y < height; y++)
          {
            src_p = src_img + y * width;
            dark_p = dark_img + y * width;
            dest_p = dest_img + y * width;
            for (x=0; x<width; x++)
#ifdef SATURATION
            if (*src_p == 255) // skip saturated pixels
              {
                *dest_p++ = *src_p++;
                dark_p++;
              }
            else
#endif
              {
                value = ((*src_p++ - *dark_p++) *factor )>>shiftright;
                if (value < 0)  *dest_p++ = 0;
                else
                  if (value > 255)  *dest_p++ = 255;
                  else  *dest_p++ = value;
              }
          }
      }
    }
}
 
