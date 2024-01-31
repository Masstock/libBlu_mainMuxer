

#ifndef __LIBBLU_MUXER__CODECS__HDMV__COMMON__PALETIZED_BITMAP_H__
#define __LIBBLU_MUXER__CODECS__HDMV__COMMON__PALETIZED_BITMAP_H__

#include "hdmv_quantizer.h"
#include "hdmv_bitmap.h"
#include "hdmv_palette.h"
#include "hdmv_view.h"

typedef enum {
  HDMV_PIC_CDM_DISABLED,
  HDMV_PIC_CDM_FLOYD_STEINBERG
} HdmvColorDitheringMethod;

typedef struct {
  uint8_t * data;
  uint16_t width;
  uint16_t height;
} HdmvPalletizedBitmap;

static inline void cleanHdmvPaletizedBitmap(
  HdmvPalletizedBitmap pal_bitmap
)
{
  free(pal_bitmap.data);
}

int getPalletizedHdmvBitmap(
  HdmvPalletizedBitmap * result,
  const HdmvBitmap src_bitmap,
  const HdmvPalette * palette,
  HdmvColorDitheringMethod dither_method
);

#endif
