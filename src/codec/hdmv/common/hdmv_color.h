

#ifndef __LIBBLU_MUXER__CODECS__HDMV__COMMON__COLOR_H__
#define __LIBBLU_MUXER__CODECS__HDMV__COMMON__COLOR_H__

#define C_R 0
#define C_G 1
#define C_B 2
#define C_Y 0
#define C_CB 1
#define C_CR 2
#define C_A 3

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

static inline double getR_RGBA(uint32_t color)
{
  return ((color >> 24) & 0xFF) / 255.;
}

static inline double getG_RGBA(uint32_t color)
{
  return ((color >> 16) & 0xFF) / 255.;
}

static inline double getB_RGBA(uint32_t color)
{
  return ((color >>  8) & 0xFF) / 255.;
}

static inline double getA_RGBA(uint32_t color)
{
  return ((color      ) & 0xFF) / 255.;
}

static uint32_t lb_clip_double_32(
  double value
)
{
  int value_int = (int) 255. * value;

  if (255 < value_int)
    return 255u;
  if (value_int < 0)
    return 0u;
  return (uint32_t) (value_int & 0xFF);
}

static inline uint32_t getRGBA(
  double r,
  double g,
  double b,
  double a
)
{
  return (
    (lb_clip_double_32(r)   << 24)
    | (lb_clip_double_32(g) << 16)
    | (lb_clip_double_32(b) <<  8)
    | lb_clip_double_32(a)
  );
}

#endif