

#ifndef __LIBBLU_MUXER__CODECS__HDMV__COMMON__COLOR_H__
#define __LIBBLU_MUXER__CODECS__HDMV__COMMON__COLOR_H__

#define C_R 0
#define C_G 1
#define C_B 2
#define C_Y 0
#define C_CB 1
#define C_CR 2
#define C_A 3

typedef struct {
  int r;
  int g;
  int b;
  int a;
} IntRgba;

static inline void printIntRgba(
  IntRgba rgba
)
{
  printf("%d %d %d %d\n", rgba.r, rgba.g, rgba.b, rgba.a);
}

#define GET_CHANNEL(vect, n)  (((vect) >> ((n) << 3)) & 0xFF)
#define CHANNEL_VALUE(value, n)  (((value) & 0xFF) << ((n) << 3))
#define GET_DIST(vect)                                                        \
  (                                                                           \
    GET_CHANNEL(vect, C_R) * GET_CHANNEL(vect, C_R)                           \
    + GET_CHANNEL(vect, C_G) * GET_CHANNEL(vect, C_G)                         \
    + GET_CHANNEL(vect, C_B) * GET_CHANNEL(vect, C_B)                         \
    + GET_CHANNEL(vect, C_A) * GET_CHANNEL(vect, C_A)                         \
  )
#define CLIP_UINT8(x) (MIN(MAX(x, 0), 255))

#endif