#include <stdio.h>
#include <stdlib.h>

#include "hdmv_bitmap.h"

int checkBitmapDimensions(
  uint32_t width,
  uint32_t height
)
{
  bool valid = true;

  if (width < HDMV_BM_MIN_SIZE) {
    LIBBLU_HDMV_PIC_ERROR("Picture width is below %u px minimum.", HDMV_BM_MIN_SIZE);
    valid = false;
  }
  if (HDMV_BM_MAX_SIZE < width) {
    LIBBLU_HDMV_PIC_ERROR("Picture width exceed %u px limit.", HDMV_BM_MAX_SIZE);
    valid = false;
  }

  if (height < HDMV_BM_MIN_SIZE) {
    LIBBLU_HDMV_PIC_ERROR("Picture height is below %u px minimum.", HDMV_BM_MIN_SIZE);
    valid = false;
  }
  if (HDMV_BM_MAX_SIZE < height) {
    LIBBLU_HDMV_PIC_ERROR("Picture height exceed %u px limit.", HDMV_BM_MAX_SIZE);
    valid = false;
  }

  return (valid) ? 0 : -1;
}

int initHdmvBitmap(
  HdmvBitmap * bitmap,
  uint16_t width,
  uint16_t height
)
{
  if (checkBitmapDimensions(width, height) < 0)
    return -1;

  uint32_t * buf = calloc(width * height, sizeof(uint32_t));
  if (NULL == buf)
    return -1;

  *bitmap = (HdmvBitmap) {
    .rgba = buf,
    .width = width,
    .height = height
  };
  return 0;
}

int dupHdmvBitmap(
  HdmvBitmap * dst,
  const HdmvBitmap * src
)
{
  if (initHdmvBitmap(dst, src->width, src->height) < 0)
    return -1;
  memcpy(dst->rgba, src->rgba, src->width * src->height * sizeof(uint32_t));
  return 0;
}

int cropHdmvBitmap(
  HdmvBitmap * dst,
  HdmvRectangle rect
)
{
  assert(NULL != dst);

  if (!isRectangleInsideHdmvBitmap(dst, rect))
    LIBBLU_HDMV_PIC_ERROR_RETURN("Cropping rectangle is outside bitmap.\n");

  size_t off = 0;
  for (uint16_t j = 0; j < rect.h; j++) {
    for (uint16_t i = 0; i < rect.w; i++) {
      dst->rgba[off++] = dst->rgba[
        (rect.y + j) * dst->width + (rect.x + i)
      ];
    }
  }

  return 0;
}

int cropCopyHdmvBitmap(
  HdmvBitmap * dst_ret,
  const HdmvBitmap * src,
  HdmvRectangle rect
)
{
  assert(NULL != dst_ret);
  assert(NULL != src);
  assert(src != dst_ret);

  if (!isRectangleInsideHdmvBitmap(src, rect))
    LIBBLU_HDMV_PIC_ERROR_RETURN("Cropping rectangle is outside bitmap.\n");

  HdmvBitmap dst;
  if (initHdmvBitmap(&dst, rect.w, rect.h) < 0)
    return -1;

  size_t off = 0;
  for (uint16_t j = 0; j < rect.h; j++) {
    for (uint16_t i = 0; i < rect.w; i++) {
      dst.rgba[off++] = src->rgba[
        (rect.y + j) * src->width + (rect.x + i)
      ];
    }
  }

  *dst_ret = dst;
  return 0;
}