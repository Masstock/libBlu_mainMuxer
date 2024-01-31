

#ifndef __LIBBLU_MUXER__CODECS__HDMV__COMMON__PALETTE_GEN_H__
#define __LIBBLU_MUXER__CODECS__HDMV__COMMON__PALETTE_GEN_H__

#include "hdmv_error.h"
#include "hdmv_palette.h"
#include "hdmv_bitmap_indexer.h"
#include "hdmv_bitmap_list.h"
#include "hdmv_quantizer.h"

int buildPaletteListHdmvPalette(
  HdmvPalette * dst,
  HdmvBitmapList * list,
  unsigned target_nb_colors,
  HdmvQuantHexTreeNodesInventory * inv
);

#endif