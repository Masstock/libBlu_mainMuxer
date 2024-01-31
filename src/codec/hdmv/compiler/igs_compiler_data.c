#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include "igs_compiler_data.h"

/* ###### Add Entry : ###################################################### */

int addRefPictureIgsCompilerComposition(
  IgsCompilerComposition * dst,
  HdmvBitmap bitmap,
  const lbc * name
)
{
  /* Save reference picture in composition bitmaps */
  unsigned idx;
  if (addObjectIgsCompilerComposition(dst, bitmap, &idx) < 0)
    return -1;
  /* Use composition memory allocation */
  HdmvBitmap * bitmap_ptr = &dst->object_bitmaps[idx];
  return addHdmvPicturesIndexer(&dst->ref_pics_indexer, bitmap_ptr, name);
}

#define HDMV_COMPL_COMPO_DEFAULT_NB_OBJ  8

int addObjectIgsCompilerComposition(
  IgsCompilerComposition * dst,
  HdmvBitmap pic,
  unsigned * idx_ret
)
{
  assert(NULL != dst);

  if (dst->nb_allocated_object_bitmaps <= dst->nb_used_object_bitmaps) {
    size_t new_size = GROW_ALLOCATION(
      dst->nb_allocated_object_bitmaps,
      HDMV_COMPL_COMPO_DEFAULT_NB_OBJ
    );
    if (!new_size || lb_mul_overflow_size_t(new_size, sizeof(HdmvBitmap)))
      LIBBLU_HDMV_IGS_COMPL_ERROR_RETURN("Composition nb obj overflow.\n");

    HdmvBitmap * new_array = (HdmvBitmap *) realloc(
      dst->object_bitmaps,
      new_size * sizeof(HdmvBitmap)
    );

    dst->object_bitmaps = new_array;
    dst->nb_allocated_object_bitmaps = new_size;
  }

  if (NULL != idx_ret)
    *idx_ret = dst->nb_used_object_bitmaps;
  dst->object_bitmaps[dst->nb_used_object_bitmaps++] = pic;
  return 0;
}

#define HDMV_COMPL_COMPO_DEFAULT_NB_PAL  8

int addPaletteIgsCompilerComposition(
  IgsCompilerComposition * dst,
  HdmvPalette * pal,
  uint8_t * palette_id_ret
)
{
  assert(NULL != dst);
  assert(NULL != pal);

  if (HDMV_MAX_PALETTE_ID < dst->nb_used_palettes)
    LIBBLU_HDMV_IGS_COMPL_ERROR_RETURN("Too many palettes ids used.\n");

  if (dst->nb_allocated_palettes <= dst->nb_used_palettes) {
    unsigned new_size = GROW_ALLOCATION(
      dst->nb_allocated_palettes,
      HDMV_COMPL_COMPO_DEFAULT_NB_PAL
    );

    if (!new_size || lb_mul_overflow_size_t(new_size, sizeof(HdmvPalette)))
      LIBBLU_HDMV_IGS_COMPL_ERROR_RETURN("Composition nb pal overflow.\n");

    HdmvPalette * new_array = (HdmvPalette *) realloc(
      dst->palettes,
      new_size * sizeof(HdmvPalette)
    );

    dst->palettes = new_array;
    dst->nb_allocated_palettes = new_size;
  }

  if (NULL != palette_id_ret)
    *palette_id_ret = (uint8_t) dst->nb_used_palettes;
  memcpy(&dst->palettes[dst->nb_used_palettes++], pal, sizeof(HdmvPalette));

  return 0;
}