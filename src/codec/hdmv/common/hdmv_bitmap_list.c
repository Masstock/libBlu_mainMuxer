#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include "hdmv_bitmap_list.h"

#define HDMV_PIC_LIST_DEFAULT_NB_PICS  8

int addHdmvBitmapList(
  HdmvBitmapList * dst,
  HdmvBitmap pic
)
{
  assert(NULL != dst);

  if (dst->nb_allocated_bitmaps <= dst->nb_bitmaps) {
    /* Increase list allocation size if required */
    size_t new_size = GROW_ALLOCATION(
      dst->nb_allocated_bitmaps,
      HDMV_PIC_LIST_DEFAULT_NB_PICS
    );
    if (!new_size || lb_mul_overflow_size_t(new_size, sizeof(HdmvBitmap *)))
      LIBBLU_HDMV_PIC_ERROR_RETURN("Picture list size overflow.\n");

    HdmvBitmap * new_array = (HdmvBitmap *) realloc(
      dst->bitmaps, new_size * sizeof(HdmvBitmap)
    );
    if (NULL == new_array)
      LIBBLU_HDMV_PIC_ERROR_RETURN("Memory allocation error.\n");

    dst->bitmaps = new_array;
    dst->nb_allocated_bitmaps = new_size;
  }

  dst->bitmaps[dst->nb_bitmaps++] = pic;
  return 0;
}
