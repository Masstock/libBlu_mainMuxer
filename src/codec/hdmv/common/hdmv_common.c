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
      return constantHdmvPDSegmentParameters(first->data.pd, second->data.pd);

    case HDMV_SEGMENT_TYPE_ODS:
    case HDMV_SEGMENT_TYPE_PCS:
    case HDMV_SEGMENT_TYPE_WDS:
    case HDMV_SEGMENT_TYPE_ICS:
      // TODO

    case HDMV_SEGMENT_TYPE_END:
      break;
  }

  return true;
}

int parseFragmentHdmvSequence(
  HdmvSequencePtr dst,
  BitstreamReaderPtr input,
  uint16_t fragment_length
)
{

  if (lb_u32add_overflow(dst->fragments_used_len, fragment_length))
    LIBBLU_HDMV_COM_ERROR_RETURN(
      "Reconstructed sequence data from segment fragments is too large.\n"
    );

  if (dst->fragments_allocated_len < dst->fragments_used_len + fragment_length) {
    /* Need sequence data size increasing */
    uint32_t new_size = dst->fragments_used_len + fragment_length;

    uint8_t * new_array = (uint8_t *) realloc(dst->fragments, new_size);
    if (NULL == new_array)
      LIBBLU_HDMV_COM_ERROR_RETURN("Memory allocation error.\n");

    dst->fragments = new_array;
    dst->fragments_allocated_len = new_size;
  }

  LIBBLU_HDMV_PARSER_DEBUG(
    "  size: %" PRIu16 " byte(s).\n",
    fragment_length
  );

  if (readBytes(input, &dst->fragments[dst->fragments_used_len], fragment_length) < 0)
    return -1;
  dst->fragments_used_len += fragment_length;

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
    if (lb_mul_overflow_size_t(newSize, sizeof(void *)))
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
    if (lb_mul_overflow_size_t(newSegmentSize, pool->elemSize))
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

void initHdmvSegmentsInventory(
  HdmvSegmentsInventory * dst
)
{
  HdmvSegmentsInventory inv = {0};

#define INIT_POOL(d, s)  initHdmvSegmentsInventoryPool(d, s)
  INIT_POOL(&inv.sequences, sizeof(HdmvSequence));
  INIT_POOL(&inv.segments, sizeof(HdmvSegment));
  INIT_POOL(&inv.segments, sizeof(HdmvSegment));
  INIT_POOL(&inv.pages, sizeof(HdmvPageParameters));
  INIT_POOL(&inv.windows, sizeof(HdmvWindowInfoParameters));
  INIT_POOL(&inv.effects, sizeof(HdmvEffectInfoParameters));
  INIT_POOL(&inv.compo_obj, sizeof(HdmvCOParameters));
  INIT_POOL(&inv.bogs, sizeof(HdmvButtonOverlapGroupParameters));
  INIT_POOL(&inv.buttons, sizeof(HdmvButtonParam));
  INIT_POOL(&inv.commands, sizeof(HdmvNavigationCommand));
  INIT_POOL(&inv.pal_entries, sizeof(HdmvPaletteEntryParameters));
#undef INIT_POOL

  *dst = inv;
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
  HdmvSegmentsInventory * inv
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
  HdmvSegmentsInventory * inv
)
{
  HdmvSegmentPtr seg;

  GET_HDMV_SEGMENTS_INVENTORY_POOL(seg, HdmvSegment *, &inv->segments);
  if (NULL == seg)
    return NULL;
  initHdmvSegment(seg);

  return seg;
}

HdmvPageParameters * getHdmvPageParamHdmvSegmentsInventory(
  HdmvSegmentsInventory * inv
)
{
  HdmvPageParameters * page;

  GET_HDMV_SEGMENTS_INVENTORY_POOL(page, HdmvPageParameters *, &inv->pages);
  if (NULL == page)
    return NULL;
  initHdmvPageParameters(page);

  return page;
}

HdmvWindowInfoParameters * getHdmvWinInfoParamHdmvSegmentsInventory(
  HdmvSegmentsInventory * inv
)
{
  HdmvWindowInfoParameters * win;

  GET_HDMV_SEGMENTS_INVENTORY_POOL(win, HdmvWindowInfoParameters *, &inv->windows);
  if (NULL == win)
    return NULL;
  initHdmvWindowInfoParameters(win);

  return win;
}

HdmvEffectInfoParameters * getHdmvEffInfoParamHdmvSegmentsInventory(
  HdmvSegmentsInventory * inv
)
{
  HdmvEffectInfoParameters * eff;

  GET_HDMV_SEGMENTS_INVENTORY_POOL(eff, HdmvEffectInfoParameters *, &inv->effects);
  if (NULL == eff)
    return NULL;
  initHdmvEffectInfoParameters(eff);

  return eff;
}

HdmvCOParameters * getHdmvCompoObjParamHdmvSegmentsInventory(
  HdmvSegmentsInventory * inv
)
{
  HdmvCOParameters * compo;

  GET_HDMV_SEGMENTS_INVENTORY_POOL(compo, HdmvCOParameters *, &inv->compo_obj);
  if (NULL == compo)
    return NULL;
  initHdmvCompositionObjectParameters(compo);

  return compo;
}

HdmvButtonOverlapGroupParameters * getHdmvBOGParamHdmvSegmentsInventory(
  HdmvSegmentsInventory * inv
)
{
  HdmvButtonOverlapGroupParameters * bog;

  GET_HDMV_SEGMENTS_INVENTORY_POOL(bog, HdmvButtonOverlapGroupParameters *, &inv->bogs);
  if (NULL == bog)
    return NULL;
  initHdmvButtonOverlapGroupParameters(bog);

  return bog;
}

HdmvButtonParam * getHdmvBtnParamHdmvSegmentsInventory(
  HdmvSegmentsInventory * inv
)
{
  HdmvButtonParam * btn;

  GET_HDMV_SEGMENTS_INVENTORY_POOL(btn, HdmvButtonParam *, &inv->buttons);
  if (NULL == btn)
    return NULL;
  initHdmvButtonParam(btn);

  return btn;
}

HdmvNavigationCommand * getHdmvNaviComHdmvSegmentsInventory(
  HdmvSegmentsInventory * inv
)
{
  HdmvNavigationCommand * com;

  GET_HDMV_SEGMENTS_INVENTORY_POOL(com, HdmvNavigationCommand *, &inv->commands);
  if (NULL == com)
    return NULL;
  initHdmvNavigationCommand(com);

  return com;
}

HdmvPaletteEntryParameters * getHdmvPalEntryParamHdmvSegmentsInventory(
  HdmvSegmentsInventory * inv
)
{
  HdmvPaletteEntryParameters * entry;

  GET_HDMV_SEGMENTS_INVENTORY_POOL(entry, HdmvPaletteEntryParameters *, &inv->pal_entries);
  if (NULL == entry)
    return NULL;
  initHdmvPaletteEntryParameters(entry);

  return entry;
}

#undef INCREMENT_POOL
#undef GET_POOL
