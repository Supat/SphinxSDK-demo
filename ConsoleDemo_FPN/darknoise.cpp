/* darknoise.c
 * functions to subtract dark_img from a src_img and copy result to dest_img with SSE2 or parallel support
 * (c) 2016 MRC Systems GmbH
 * Author:sf
 * Version 1.0
 */

// for SSE

#include "darknoise.h"

void darknoise_bw_subtract_SSE2(u_char *dest_img,u_char *src_img, u_char * dark_img, int32_t width, int32_t height, int64_t img_size)
{
  int y,x;
  u_char *src_p, *dest_p, *dark_p;
  const u_char factor =3; // divided by 2->1.5
  const u_char shiftright = 1;
  short value;
  __m128i BP, P, BL, PL, BH, PH, cmp_res;
  __m128i const zero = _mm_setzero_si128();
  __m128i const _m_factor = _mm_set_epi16(factor,factor,factor,factor,factor,factor,factor,factor);
  __m128i const _saturated = _mm_set_epi8(0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff);

  //check alignemnt of pointers
  if ((u_long)dest_img % 16 == 0 && (u_long)src_img % 16 == 0)
    // use aligned SSE if aligned
    {
      src_p  = src_img;
      dark_p = dark_img;
      dest_p = dest_img;
      for (y = 0; y < img_size; y +=16)
        {
          BP = _mm_load_si128((__m128i*)dark_p);
          P  = _mm_load_si128((__m128i*)src_p);
#ifdef SATURATION
          // skip saturated pixels
          cmp_res = _mm_cmpeq_epi8(P, _saturated);
          BP = _mm_andnot_si128(cmp_res, BP);
#endif
          BL = _mm_unpacklo_epi8(BP,zero);
          PL = _mm_unpacklo_epi8(P,zero);
          PL = _mm_subs_epi16(PL, BL);
          PL = _mm_mullo_epi16(PL, _m_factor);
          PL = _mm_srai_epi16(PL, shiftright);   //divide by 2/shiftright 1
          //          PL = _mm_slli_epi16(PL, 2);
          BH = _mm_unpackhi_epi8(BP,zero);
          PH = _mm_unpackhi_epi8(P,zero);
          PH = _mm_subs_epi16(PH, BH);
          PH = _mm_mullo_epi16(PH, _m_factor);
          PH = _mm_srai_epi16(PH, shiftright);   //divide by 2/shiftright 1
          P  = _mm_packus_epi16(PL, PH);
          _mm_store_si128((__m128i*)dest_p, P);
          src_p += 16;
          dark_p += 16;
          dest_p += 16;
        }
    }
  else
    //in this case tha data is not alligned
    {
      src_p = src_img;
      dark_p = dark_img;
      dest_p = dest_img;
      for (y = 0; y < img_size; y += 16)
        {
          BP = _mm_load_si128((__m128i*)dark_p);   // dark_p  is allways aligned
          P = _mm_loadu_si128((__m128i*)src_p);
#ifdef SATURATION
          // skip saturated pixels
          cmp_res = _mm_cmpeq_epi8(P, _saturated);
          BP = _mm_andnot_si128(cmp_res, BP);
#endif
          BL = _mm_unpacklo_epi8(BP, zero);
          PL = _mm_unpacklo_epi8(P, zero);
          PL = _mm_subs_epi16(PL, BL);
          PL = _mm_mullo_epi16(PL, _m_factor);
          PL = _mm_srai_epi16(PL, shiftright);   //divide by 2/shiftright 1
          //          PL = _mm_slli_epi16(PL, 2);
          BH = _mm_unpackhi_epi8(BP, zero);
          PH = _mm_unpackhi_epi8(P, zero);
          PH = _mm_subs_epi16(PH, BH);
          PH = _mm_mullo_epi16(PH, _m_factor);
          PH = _mm_srai_epi16(PH, shiftright);   //divide by 2/shiftright 1
          P = _mm_packus_epi16(PL, PH);
          _mm_storeu_si128((__m128i*)dest_p, P);
          src_p += 16;
          dark_p += 16;
          dest_p += 16;
        }
    }
  //handle pixels at the end of the image
  int rest = img_size % 16;
  for (y = 0; y < rest; y++)
    {
#ifdef SATURATION
      if (*src_p == 255) // skip saturated pixels
        {
          *dest_p++ = *src_p++;
          dark_p++;
        }
      else
#endif
        {
          value = ((*src_p++ - *dark_p++) * factor) >> shiftright;
          if (value < 0)  *dest_p++ = 0;
          else
            if (value > 255)  *dest_p++ = 255;
            else  *dest_p++ = (u_char)value;
        }
    }
}

void darknoise_bw_subtract(u_char *dest_img,u_char *src_img, u_char * dark_img, int32_t width, int32_t height, int64_t img_size)
// only needed without SSE2
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
 
