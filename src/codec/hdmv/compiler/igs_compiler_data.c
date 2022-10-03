#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include "igs_compiler_data.h"

/* ###### Creation / Destruction : ######################################### */

IgsCompilerCompositionPtr createIgsCompilerComposition(
  void
)
{
  IgsCompilerCompositionPtr compo;

  compo = (IgsCompilerCompositionPtr) malloc(sizeof(IgsCompilerComposition));
  if (NULL == compo)
    LIBBLU_HDMV_IGS_COMPL_ERROR_NRETURN("Memory allocation error.\n");
  *compo = (IgsCompilerComposition) {0};

  if (NULL == (compo->refPics = createHdmvPicturesIndexer()))
    goto free_return;

  return compo;

free_return:
  destroyIgsCompilerComposition(compo);
  return NULL;
}

void destroyIgsCompilerComposition(
  IgsCompilerCompositionPtr compo
)
{
  unsigned i;

  if (NULL == compo)
    return;

  destroyHdmvPicturesIndexer(compo->refPics);

  for (i = 0; i < compo->nbUsedObjPics; i++)
    destroyHdmvPicture(compo->objPics[i]);
  free(compo->objPics);

  for (i = 0; i < compo->nbUsedPals; i++)
    destroyHdmvPaletteDefinition(compo->pals[i]);
  free(compo->pals);

  free(compo);
}

/* ###### Add Entry : ###################################################### */

int addObjectIgsCompilerComposition(
  IgsCompilerCompositionPtr dst,
  HdmvPicturePtr pic,
  unsigned * id
)
{
  assert(NULL != dst);
  assert(NULL != pic);

  if (dst->nbAllocatedObjPics <= dst->nbUsedObjPics) {
    HdmvPicturePtr * newArray;
    size_t newSize;

    newSize = GROW_ALLOCATION(
      dst->nbAllocatedObjPics,
      HDMV_COMPL_COMPO_DEFAULT_NB_OBJ
    );
    if (lb_mul_overflow(newSize, sizeof(HdmvPicturePtr)))
      LIBBLU_HDMV_IGS_COMPL_ERROR_RETURN("Composition nb obj overflow.\n");

    newArray = (HdmvPicturePtr *) realloc(
      dst->objPics,
      newSize * sizeof(HdmvPicturePtr)
    );

    dst->objPics = newArray;
    dst->nbAllocatedObjPics = newSize;
  }

  if (NULL != id)
    *id = dst->nbUsedObjPics;
  dst->objPics[dst->nbUsedObjPics++] = pic;

  return 0;
}

int addPaletteIgsCompilerComposition(
  IgsCompilerCompositionPtr dst,
  HdmvPaletteDefinitionPtr pal,
  unsigned * id
)
{
  assert(NULL != dst);
  assert(NULL != pal);

  if (dst->nbAllocatedPals <= dst->nbUsedPals) {
    HdmvPaletteDefinitionPtr * newArray;
    size_t newSize;

    newSize = GROW_ALLOCATION(
      dst->nbAllocatedPals,
      HDMV_COMPL_COMPO_DEFAULT_NB_PAL
    );
    if (lb_mul_overflow(newSize, sizeof(HdmvPaletteDefinitionPtr)))
      LIBBLU_HDMV_IGS_COMPL_ERROR_RETURN("Composition nb pal overflow.\n");

    newArray = (HdmvPaletteDefinitionPtr *) realloc(
      dst->pals,
      newSize * sizeof(HdmvPaletteDefinitionPtr)
    );

    dst->pals = newArray;
    dst->nbAllocatedPals = newSize;
  }

  if (NULL != id)
    *id = dst->nbUsedPals;
  dst->pals[dst->nbUsedPals++] = pal;

  return 0;
}