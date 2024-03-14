

#ifndef __LIBBLU_MUXER__CODECS__HDMV__BITMAP_H__
#define __LIBBLU_MUXER__CODECS__HDMV__BITMAP_H__

#include "../../../util.h"

#include "hdmv_rectangle.h"
#include "hdmv_color.h"
#include "hdmv_data.h"

#define HDMV_BM_MIN_SIZE  HDMV_OD_MIN_SIZE

#define HDMV_BM_MAX_SIZE  HDMV_OD_PG_MAX_SIZE

int checkBitmapDimensions(
  uint32_t width,
  uint32_t height
);

typedef struct {
  uint32_t *rgba;
  uint16_t width;
  uint16_t height;

  HdmvODSUsage usage;
  uint16_t object_id;
} HdmvBitmap;

int initHdmvBitmap(
  HdmvBitmap *bitmap,
  uint16_t width,
  uint16_t height
);

int dupHdmvBitmap(
  HdmvBitmap *dst,
  const HdmvBitmap *src
);

static inline void cleanHdmvBitmap(
  HdmvBitmap bitmap
)
{
  free(bitmap.rgba);
}

static inline bool isRectangleInsideHdmvBitmap(
  const HdmvBitmap *bitmap,
  HdmvRectangle rectangle
)
{
  if (bitmap->width < rectangle.x + rectangle.w)
    return false;
  if (bitmap->height < rectangle.y + rectangle.h)
    return false;
  return true;
}

static inline void fillHdmvBitmap(
  HdmvBitmap *bitmap,
  uint32_t rgba
)
{
  size_t size = 1ull *bitmap->width *bitmap->height;
  for (size_t i = 0; i < size; i++)
    bitmap->rgba[i] = rgba;
}

static inline void clearHdmvBitmap(
  HdmvBitmap *bitmap
)
{
  fillHdmvBitmap(bitmap, 0x00);
}

static inline void setPixelHdmvBitmap(
  HdmvBitmap *bitmap,
  uint32_t rgba,
  uint16_t x,
  uint16_t y
)
{
  bitmap->rgba[y *bitmap->width + x] = rgba;
}

int cropHdmvBitmap(
  HdmvBitmap *dst,
  HdmvRectangle rect
);

int cropCopyHdmvBitmap(
  HdmvBitmap *dst_ret,
  const HdmvBitmap *src,
  HdmvRectangle rect
);

#endif