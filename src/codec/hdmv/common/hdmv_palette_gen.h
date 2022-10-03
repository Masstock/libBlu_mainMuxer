

#ifndef __LIBBLU_MUXER__CODECS__HDMV__COMMON__PALETTE_GEN_H__
#define __LIBBLU_MUXER__CODECS__HDMV__COMMON__PALETTE_GEN_H__

#include "hdmv_error.h"
#include "hdmv_palette_def.h"
#include "hdmv_pictures_common.h"
#include "hdmv_pictures_indexer.h"
#include "hdmv_pictures_list.h"
#include "hdmv_pictures_quantizer.h"

int buildPaletteListHdmvPalette(
  HdmvPaletteDefinitionPtr dst,
  const HdmvPicturesListPtr list,
  size_t targetedNbColors,
  HdmvQuantHexTreeNodesInventoryPtr inv
);

#endif