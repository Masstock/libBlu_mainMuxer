/** \~english
 * \file hdmv_pictures_list.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Unindexed list of images.
 */

#ifndef __LIBBLU_MUXER__CODECS__HDMV__COMMON__LIST_H__
#define __LIBBLU_MUXER__CODECS__HDMV__COMMON__LIST_H__

#include "hdmv_error.h"
#include "hdmv_pictures_common.h"
#include "hdmv_pictures_io.h"

/* ### HDMV Pictures list : ################################################ */

typedef struct {
  HdmvPicturePtr * pics;
  unsigned nbAllocatedPics;
  unsigned nbUsedPics;
} HdmvPicturesList, *HdmvPicturesListPtr;

/* ###### Creation / Destruction : ######################################### */

HdmvPicturesListPtr createHdmvPicturesList(
  void
);

static inline void destroyHdmvPicturesList(
  HdmvPicturesListPtr list
)
{
  unsigned i;

  for (i = 0; i < list->nbUsedPics; i++)
    destroyHdmvPicture(list->pics[i]);
  free(list->pics);
  free(list);
}

/* ###### Accessors : ###################################################### */

static inline unsigned nbPicsHdmvPicturesList(
  const HdmvPicturesListPtr list
)
{
  return list->nbUsedPics;
}

/* ###### Operations : ##################################################### */

int setPaletteHdmvPicturesList(
  HdmvPicturesListPtr list,
  HdmvPaletteDefinitionPtr pal,
  HdmvPictureColorDitheringMethod ditherMeth
);

static inline void flushHdmvPicturesList(
  HdmvPicturesListPtr list
)
{
  list->nbUsedPics = 0;
}

/* ###### Add Entry : ###################################################### */

#define HDMV_PIC_LIST_DEFAULT_NB_PICS  8

int addHdmvPicturesList(
  HdmvPicturesListPtr dst,
  HdmvPicturePtr pic
);

/* ###### Get Entry : ###################################################### */

static inline HdmvPicturePtr getHdmvPicturesList(
  HdmvPicturesListPtr list,
  unsigned index
)
{
  if (list->nbUsedPics <= index)
    LIBBLU_HDMV_PIC_ERROR_NRETURN(
      "Unknown picture id %u in list.\n",
      list->nbUsedPics
    );

  return list->pics[index];
}

static inline HdmvPicturePtr iterateHdmvPicturesList(
  HdmvPicturesListPtr list,
  unsigned * index
)
{
  assert(NULL != index);

  if (list->nbUsedPics <= *index)
    return NULL;
  return list->pics[(*index)++];
}

#endif