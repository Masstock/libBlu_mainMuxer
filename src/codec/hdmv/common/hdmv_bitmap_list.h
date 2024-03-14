/** \~english
 * \file hdmv_bitmap_list.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Unindexed list of images.
 */

#ifndef __LIBBLU_MUXER__CODECS__HDMV__COMMON__BITMAP_LIST_H__
#define __LIBBLU_MUXER__CODECS__HDMV__COMMON__BITMAP_LIST_H__

#include "hdmv_error.h"
#include "hdmv_bitmap.h"
#include "hdmv_bitmap_io.h"

/* ### HDMV Pictures list : ################################################ */

typedef struct {
  HdmvBitmap *bitmaps;
  unsigned nb_allocated_bitmaps;
  unsigned nb_bitmaps;
} HdmvBitmapList;

/* ###### Creation / Destruction : ######################################### */

static inline void cleanHdmvBitmapList(
  HdmvBitmapList list
)
{
  free(list.bitmaps);
}

/* ###### Accessors : ###################################################### */

static inline unsigned nbPicsHdmvBitmapList(
  const HdmvBitmapList list
)
{
  return list.nb_bitmaps;
}

/* ###### Operations : ##################################################### */

static inline void flushHdmvBitmapList(
  HdmvBitmapList *list
)
{
  list->nb_bitmaps = 0;
}

/* ###### Add Entry : ###################################################### */

int addHdmvBitmapList(
  HdmvBitmapList *dst,
  HdmvBitmap pic
);

/* ###### Get Entry : ###################################################### */

static inline int getHdmvBitmapList(
  HdmvBitmapList *list,
  HdmvBitmap *dst,
  unsigned index
)
{
  if (list->nb_bitmaps <= index)
    LIBBLU_HDMV_PIC_ERROR_RETURN(
      "Unknown picture id %u in list.\n",
      list->nb_bitmaps
    );

  *dst = list->bitmaps[index];

  return 0;
}

static inline int getRefHdmvBitmapList(
  HdmvBitmapList *list,
  HdmvBitmap ** dst,
  unsigned index
)
{
  if (list->nb_bitmaps <= index)
    LIBBLU_HDMV_PIC_ERROR_RETURN(
      "Unknown picture id %u in list.\n",
      list->nb_bitmaps
    );

  *dst = &list->bitmaps[index];

  return 0;
}

static inline bool iterateHdmvBitmapList(
  HdmvBitmapList *list,
  HdmvBitmap *dst,
  unsigned *index
)
{
  assert(NULL != index);

  if (list->nb_bitmaps <= *index)
    return false;

  *dst = list->bitmaps[(*index)++];
  return true;
}

void sortByUsageHdmvBitmapList(
  HdmvBitmapList *list
);

#endif