

#ifndef __LIBBLU_MUXER__CODECS__HDMV__RECTANGLE_H__
#define __LIBBLU_MUXER__CODECS__HDMV__RECTANGLE_H__

#include "../../../util.h"

typedef struct {
  uint16_t x;
  uint16_t y;
  uint16_t w;
  uint16_t h;
} HdmvRectangle;

static inline HdmvRectangle emptyRectangle(
  void
)
{
  return (HdmvRectangle) {0};
}

static inline unsigned areaRectangle(
  HdmvRectangle dim
)
{
  return dim.w *dim.h;
}

static inline bool isEmptyRectangle(
  HdmvRectangle dim
)
{
  return 0 == areaRectangle(dim);
}

static inline HdmvRectangle mergeRectangle(
  HdmvRectangle first,
  HdmvRectangle second
)
{
  if (isEmptyRectangle(first))
    return second;
  if (isEmptyRectangle(second))
    return first;
  uint16_t x = MIN(first.x, second.x);
  uint16_t y = MIN(first.y, second.y);
  return (HdmvRectangle) {
    .x = x,
    .y = y,
    .w = MAX(first.x + first.w - x, second.x + second.w - x),
    .h = MAX(first.y + first.h - y, second.y + second.h - y)
  };
}

static inline bool areCollidingRectangle(
  HdmvRectangle first,
  HdmvRectangle second
)
{
  return
    first.x < second.x + second.w
    && second.x < first.x + first.w
    && first.y < second.y + second.h
    && second.y < first.y + first.h
  ;
}

static inline bool isInsideRectangle(
  HdmvRectangle window,
  HdmvRectangle rectangle
)
{
  return
    window.x <= rectangle.x
    && rectangle.x + rectangle.w <= window.x + window.w
    && window.y <= rectangle.y
    && rectangle.y + rectangle.h <= window.y + window.h
  ;
}


#endif