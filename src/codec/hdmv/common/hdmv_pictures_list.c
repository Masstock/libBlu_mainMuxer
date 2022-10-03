#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include "hdmv_pictures_list.h"

/* ### HDMV Pictures list : ################################################ */
/* ###### Creation / Destruction : ######################################### */

HdmvPicturesListPtr createHdmvPicturesList(
  void
)
{
  HdmvPicturesListPtr list;

  list = (HdmvPicturesListPtr) malloc(sizeof(HdmvPicturesList));
  if (NULL == list)
    LIBBLU_HDMV_PIC_ERROR_NRETURN("Memory allocation error.\n");

  *list = (HdmvPicturesList) {
    0
  };

  return list;
}

/* ###### Operations : ##################################################### */

int setPaletteHdmvPicturesList(
  HdmvPicturesListPtr list,
  HdmvPaletteDefinitionPtr pal,
  HdmvPictureColorDitheringMethod ditherMeth
)
{
  unsigned i;

  for (i = 0; i < list->nbUsedPics; i++) {
    if (setPaletteHdmvPicture(list->pics[i], pal, ditherMeth) < 0)
      return -1;
  }

  return 0;
}

/* ###### Add Entry : ###################################################### */

int addHdmvPicturesList(
  HdmvPicturesListPtr dst,
  HdmvPicturePtr pic
)
{
  assert(NULL != dst);
  assert(NULL != pic);

  if (dst->nbAllocatedPics <= dst->nbUsedPics) {
    /* Increase list allocation size if required */
    unsigned newSize;
    HdmvPicturePtr * newArray;

    newSize = GROW_ALLOCATION(
      dst->nbAllocatedPics,
      HDMV_PIC_LIST_DEFAULT_NB_PICS
    );
    if (lb_mul_overflow(newSize, sizeof(HdmvPicturePtr)))
      LIBBLU_HDMV_PIC_ERROR_RETURN("Picture list size overflow.\n");

    newArray = (HdmvPicturePtr *) realloc(dst->pics, newSize * sizeof(HdmvPicturePtr));
    if (NULL == newArray)
      LIBBLU_HDMV_PIC_ERROR_RETURN("Memory allocation error.\n");

    dst->pics = newArray;
    dst->nbAllocatedPics = newSize;
  }

  dst->pics[dst->nbUsedPics++] = pic;

  return 0;
}
