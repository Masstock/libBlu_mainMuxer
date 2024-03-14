

#ifndef __LIBBLU_MUXER__CODECS__HDMV__COMMON__OBJECT_H__
#define __LIBBLU_MUXER__CODECS__HDMV__COMMON__OBJECT_H__

#include "hdmv_bitmap.h"
#include "hdmv_palette.h"
#include "hdmv_paletized_bitmap.h"
#include "hdmv_rectangle.h"
#include "hdmv_data.h"

typedef struct {
  HdmvPalletizedBitmap pal_bitmap;

  uint8_t *rle;
  uint32_t rle_size;

  HdmvODescParameters desc; /**< User defined values. Initialized to 0. */
} HdmvObject;

int initFromPalletizedHdmvObject(
  HdmvObject *dst,
  HdmvPalletizedBitmap pal_bitmap
);

int initHdmvObject(
  HdmvObject *dst,
  const uint8_t *rle,
  uint32_t rle_size,
  uint16_t width,
  uint16_t height
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
  return obj.pal_bitmap.width *obj.pal_bitmap.height;
}

int compressRleHdmvObject(
  HdmvObject *obj,
  unsigned *longuest_compressed_line_size_ret,
  uint16_t *longuest_compressed_line_ret
);

static inline uint8_t *getOrCompressRleHdmvObject(
  HdmvObject *obj,
  unsigned *longuest_compressed_line_size_ret,
  uint16_t *longuest_compressed_line_ret
)
{
  unsigned *max_size_ret = longuest_compressed_line_size_ret;
  uint16_t *max_ret      = longuest_compressed_line_ret;
  if (!obj->rle_size && compressRleHdmvObject(obj, max_size_ret, max_ret) < 0)
    return NULL;
  return obj->rle;
}

static inline uint8_t *getRleHdmvObject(
  const HdmvObject *obj
)
{
  if (!obj->rle_size)
    LIBBLU_HDMV_COM_ERROR_NRETURN("RLE not generated.\n");
  return obj->rle;
}

int decompressRleHdmvObject(
  HdmvObject *obj,
  unsigned *longuest_compressed_line_size_ret,
  uint16_t *longuest_compressed_line_ret
);

typedef struct {
  uint8_t mapping[HDMV_NB_PAL_ENTRIES]; // Entry i maps to mapping[i]
} HdmvPaletteUpdate;

bool testUpdateHdmvObject(
  HdmvObject dst,
  HdmvPalletizedBitmap src_pal_bm,
  int offset_x,
  int offset_y,
  HdmvPaletteUpdate *update_ret
);

int applyUpdateHdmvObject(
  HdmvObject *dst,
  HdmvPalletizedBitmap src_pal_bm
);

#endif
