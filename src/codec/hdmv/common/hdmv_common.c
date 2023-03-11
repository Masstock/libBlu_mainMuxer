#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include "hdmv_common.h"

/* ### HDMV Sequence : ##################################################### */

bool isDuplicateHdmvSequence(
  const HdmvSequencePtr first,
  const HdmvSequencePtr second
)
{
  if (first->type != second->type)
    return false;

  switch (first->type) {
    case HDMV_SEGMENT_TYPE_PDS:
      return constantHdmvPdsSegmentParameters(first->data.pd, second->data.pd);

    case HDMV_SEGMENT_TYPE_ODS:
    case HDMV_SEGMENT_TYPE_PCS:
    case HDMV_SEGMENT_TYPE_WDS:
    case HDMV_SEGMENT_TYPE_ICS:
      // TODO

    case HDMV_SEGMENT_TYPE_END:
    case HDMV_SEGMENT_TYPE_ERROR:
      break;
  }

  return true;
}

int parseFragmentHdmvSequence(
  HdmvSequencePtr dst,
  BitstreamReaderPtr input,
  size_t size
)
{

  if (lb_add_overflow(dst->fragmentsUsedLen, size))
    LIBBLU_HDMV_COM_ERROR_RETURN(
      "Reconstructed sequence data from segment fragments is too large.\n"
    );

  if (dst->fragmentsAllocatedLen < dst->fragmentsUsedLen + size) {
    /* Need sequence data size increasing */
    size_t newSize = dst->fragmentsUsedLen + size;
    uint8_t * newArray;

    if (NULL == (newArray = (uint8_t *) realloc(dst->fragments, newSize)))
      LIBBLU_HDMV_COM_ERROR_RETURN("Memory allocation error.\n");

    dst->fragments = newArray;
    dst->fragmentsAllocatedLen = newSize;
  }

  LIBBLU_HDMV_PARSER_DEBUG(
    "  size: %zu byte(s).\n",
    size
  );

  if (readBytes(input, dst->fragments + dst->fragmentsUsedLen, size) < 0)
    return -1;

  dst->fragmentsUsedLen += size;

  return 0;
}

/* ### HDMV Segments Inventory Pool : ###################################### */

static int increaseAllocationHdmvSegmentsInventoryPool(
  HdmvSegmentsInventoryPool * pool
)
{
  void * newSegment;
  size_t newSegmentSize;

  if (pool->nbAllocElemSegs <= pool->nbUsedElemSegs) {
    /* Reallocate segments array */
    void ** newArray;
    size_t i, newSize;

    newSize = GROW_ALLOCATION(
      pool->nbAllocElemSegs,
      HDMV_SEG_INV_POOL_DEFAULT_SEG_NB
    );
    if (lb_mul_overflow(newSize, sizeof(void *)))
      LIBBLU_HDMV_QUANT_ERROR_RETURN("Inventory segments number overflow.\n");

    newArray = (void **) realloc(pool->elemSegs, newSize * sizeof(void *));
    if (NULL == newArray)
      LIBBLU_HDMV_QUANT_ERROR_RETURN("Memory allocation error.\n");
    for (i = pool->nbAllocElemSegs; i < newSize; i++)
      newArray[i] = NULL;

    pool->elemSegs = newArray;
    pool->nbAllocElemSegs = newSize;
  }

  newSegmentSize = HDMV_SEG_INV_POOL_DEFAULT_SEG_SIZE << pool->nbUsedElemSegs;

  if (NULL == pool->elemSegs[pool->nbUsedElemSegs]) {
    if (lb_mul_overflow(newSegmentSize, pool->elemSize))
      LIBBLU_HDMV_QUANT_ERROR_RETURN("Inventory segment size overflow.\n");

    /* Use of calloc to avoid wild pointer-dereferences when cleaning mem. */
    if (NULL == (newSegment = calloc(newSegmentSize, pool->elemSize)))
      LIBBLU_HDMV_QUANT_ERROR_RETURN("Memory allocation error.\n");
    pool->elemSegs[pool->nbUsedElemSegs] = newSegment;
  }

  pool->nbUsedElemSegs++;
  pool->nbRemElem = newSegmentSize;

  return 0;
}

/* ### HDMV Segments Inventory : ########################################### */

HdmvSegmentsInventoryPtr createHdmvSegmentsInventory(
  void
)
{
  HdmvSegmentsInventoryPtr inv;

  inv = (HdmvSegmentsInventoryPtr) malloc(sizeof(HdmvSegmentsInventory));
  if (NULL == inv)
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");
  *inv = (HdmvSegmentsInventory) {0};

#define INIT_POOL(d, s)  initHdmvSegmentsInventoryPool(d, s)
  INIT_POOL(&inv->sequences, sizeof(HdmvSequence));
  INIT_POOL(&inv->segments, sizeof(HdmvSegment));
  INIT_POOL(&inv->segments, sizeof(HdmvSegment));
  INIT_POOL(&inv->interComposPages, sizeof(HdmvPageParameters));
  INIT_POOL(&inv->effectsWindows, sizeof(HdmvWindowInfoParameters));
  INIT_POOL(&inv->effects, sizeof(HdmvEffectInfoParameters));
  INIT_POOL(&inv->composition_objects, sizeof(HdmvCompositionObjectParameters));
  INIT_POOL(&inv->bogs, sizeof(HdmvButtonOverlapGroupParameters));
  INIT_POOL(&inv->buttons, sizeof(HdmvButtonParam));
  INIT_POOL(&inv->commands, sizeof(HdmvNavigationCommand));
  INIT_POOL(&inv->paletteEntries, sizeof(HdmvPaletteEntryParameters));
#undef INIT_POOL

  return inv;
}

/* ###### Inventory content fetching functions : ########################### */

/** \~english
 * \brief
 *
 * \param d Pointer destination variable.
 * \param tp Type pointer.
 * \param p Source pool.
 */
#define GET_HDMV_SEGMENTS_INVENTORY_POOL(d, tp, p)                            \
  do {                                                                        \
    if (!(p)->nbRemElem) {                                                    \
      if (increaseAllocationHdmvSegmentsInventoryPool(p) < 0)                 \
        return NULL;                                                          \
    }                                                                         \
    d = &(                                                                    \
      (tp) (p)->elemSegs[(p)->nbUsedElemSegs-1]                               \
    )[--(p)->nbRemElem];                                                      \
  } while (0)

HdmvSequencePtr getHdmvSequenceHdmvSegmentsInventory(
  HdmvSegmentsInventoryPtr inv
)
{
  HdmvSequencePtr seq;

  GET_HDMV_SEGMENTS_INVENTORY_POOL(seq, HdmvSequence *, &inv->sequences);
  if (NULL == seq)
    return NULL;
  resetHdmvSequence(seq);

  return seq;
}

HdmvSegmentPtr getHdmvSegmentHdmvSegmentsInventory(
  HdmvSegmentsInventoryPtr inv
)
{
  HdmvSegmentPtr seg;

  GET_HDMV_SEGMENTS_INVENTORY_POOL(seg, HdmvSegment *, &inv->segments);
  if (NULL == seg)
    return NULL;
  initHdmvSegment(seg);

  return seg;
}

HdmvPageParameters * getHdmvPageParametersHdmvSegmentsInventory(
  HdmvSegmentsInventoryPtr inv
)
{
  HdmvPageParameters * page;

  GET_HDMV_SEGMENTS_INVENTORY_POOL(page, HdmvPageParameters *, &inv->interComposPages);
  if (NULL == page)
    return NULL;
  initHdmvPageParameters(page);

  return page;
}

HdmvWindowInfoParameters * getHdmvWindowInfoParametersHdmvSegmentsInventory(
  HdmvSegmentsInventoryPtr inv
)
{
  HdmvWindowInfoParameters * win;

  GET_HDMV_SEGMENTS_INVENTORY_POOL(win, HdmvWindowInfoParameters *, &inv->effectsWindows);
  if (NULL == win)
    return NULL;
  initHdmvWindowInfoParameters(win);

  return win;
}

HdmvEffectInfoParameters * getHdmvEffectInfoParametersHdmvSegmentsInventory(
  HdmvSegmentsInventoryPtr inv
)
{
  HdmvEffectInfoParameters * eff;

  GET_HDMV_SEGMENTS_INVENTORY_POOL(eff, HdmvEffectInfoParameters *, &inv->effects);
  if (NULL == eff)
    return NULL;
  initHdmvEffectInfoParameters(eff);

  return eff;
}

HdmvCompositionObjectParameters * getHdmvCompositionObjectParametersHdmvSegmentsInventory(
  HdmvSegmentsInventoryPtr inv
)
{
  HdmvCompositionObjectParameters * compo;

  GET_HDMV_SEGMENTS_INVENTORY_POOL(compo, HdmvCompositionObjectParameters *, &inv->composition_objects);
  if (NULL == compo)
    return NULL;
  initHdmvCompositionObjectParameters(compo);

  return compo;
}

HdmvButtonOverlapGroupParameters * getHdmvButtonOverlapGroupParametersHdmvSegmentsInventory(
  HdmvSegmentsInventoryPtr inv
)
{
  HdmvButtonOverlapGroupParameters * bog;

  GET_HDMV_SEGMENTS_INVENTORY_POOL(bog, HdmvButtonOverlapGroupParameters *, &inv->bogs);
  if (NULL == bog)
    return NULL;
  initHdmvButtonOverlapGroupParameters(bog);

  return bog;
}

HdmvButtonParam * getHdmvButtonParamHdmvSegmentsInventory(
  HdmvSegmentsInventoryPtr inv
)
{
  HdmvButtonParam * btn;

  GET_HDMV_SEGMENTS_INVENTORY_POOL(btn, HdmvButtonParam *, &inv->buttons);
  if (NULL == btn)
    return NULL;
  initHdmvButtonParam(btn);

  return btn;
}

HdmvNavigationCommand * getHdmvNavigationCommandHdmvSegmentsInventory(
  HdmvSegmentsInventoryPtr inv
)
{
  HdmvNavigationCommand * com;

  GET_HDMV_SEGMENTS_INVENTORY_POOL(com, HdmvNavigationCommand *, &inv->commands);
  if (NULL == com)
    return NULL;
  initHdmvNavigationCommand(com);

  return com;
}

HdmvPaletteEntryParameters * getHdmvPaletteEntryParametersHdmvSegmentsInventory(
  HdmvSegmentsInventoryPtr inv
)
{
  HdmvPaletteEntryParameters * entry;

  GET_HDMV_SEGMENTS_INVENTORY_POOL(entry, HdmvPaletteEntryParameters *, &inv->paletteEntries);
  if (NULL == entry)
    return NULL;
  initHdmvPaletteEntryParameters(entry);

  return entry;
}

#undef INCREMENT_POOL
#undef GET_POOL
