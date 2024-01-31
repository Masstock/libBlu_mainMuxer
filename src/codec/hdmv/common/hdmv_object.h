

#ifndef __LIBBLU_MUXER__CODECS__HDMV__COMMON__OBJECT_H__
#define __LIBBLU_MUXER__CODECS__HDMV__COMMON__OBJECT_H__

#include "hdmv_bitmap.h"
#include "hdmv_palette.h"
#include "hdmv_paletized_bitmap.h"
#include "hdmv_rectangle.h"
#include "hdmv_data.h"

typedef struct {
  HdmvODescParameters desc;
  HdmvPalletizedBitmap pal_bitmap;

  uint8_t * rle;
  uint32_t rle_size;
} HdmvObject;

int initHdmvObject(
  HdmvObject * dst,
  HdmvPalletizedBitmap pal_bitmap
);

static inline void cleanHdmvObject(
  HdmvObject obj
)
{
  cleanHdmvPaletizedBitmap(obj.pal_bitmap);
  free(obj.rle);
}

static inline uint32_t decodedSizeHdmvObject(
  HdmvObject obj
)
{
  return obj.pal_bitmap.width * obj.pal_bitmap.height;
}

bool performRleHdmvObject(
  HdmvObject * obj,
  uint16_t * problematic_line_ret
);

static inline uint8_t * getRleHdmvObject(
  HdmvObject * obj
)
{
  uint16_t line;
  if (!obj->rle_size && !performRleHdmvObject(obj, &line))
    LIBBLU_HDMV_COM_ERROR(
      "Unable to generate RLE, compressed line %u exceed %u + 2 bytes.\n",
      line, obj->pal_bitmap.width
    );
  return obj->rle;
}

typedef struct {
  uint8_t mapping[HDMV_MAX_NB_PDS_ENTRIES]; // Entry i maps to mapping[i]
} HdmvPaletteUpdate;

bool testUpdateHdmvObject(
  HdmvObject dst,
  HdmvPalletizedBitmap src_pal_bm,
  int offset_x,
  int offset_y,
  HdmvPaletteUpdate * update_ret
);

int applyUpdateHdmvObject(
  HdmvObject * dst,
  HdmvPalletizedBitmap src_pal_bm
);

#endif
