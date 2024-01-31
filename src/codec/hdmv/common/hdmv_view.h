

#ifndef __LIBBLU_MUXER__CODECS__HDMV__COMMON__VIEW_H__
#define __LIBBLU_MUXER__CODECS__HDMV__COMMON__VIEW_H__

#include "hdmv_bitmap.h"
#include "hdmv_rectangle.h"

typedef struct {
  HdmvBitmap bitmap;
  HdmvRectangle rect;
} HdmvView;

#endif