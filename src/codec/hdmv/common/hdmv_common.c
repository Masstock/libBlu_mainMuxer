#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include "hdmv_common.h"

uint64_t computeHdmvOdsDecodeDuration(
  HdmvODataParameters param,
  HdmvStreamType type
)
{
  /** Compute DECODE_DURATION of ODS.
   *
   * Equation performed is (for Interactive Graphics):
   *
   *   DECODE_DURATION(n) =
   *     ceil(90000 * 8 * (object_width * object_height) / 64000000)
   * <=> ceil(9 * (object_width * object_height) / 800) // Compacted version
   *
   * or (for Presentation Graphics)
   *
   *   DECODE_DURATION(n) =
   *     ceil(90000 * 8 * (object_width * object_height) / 128000000)
   * <=> ceil(9 * (object_width * object_height) / 1600) // Compacted version
   *
   * with:
   *  90000         : PTS/DTS clock frequency (90kHz);
   *  64000000 or
   *  128000000     : Pixel Decoding Rate
   *                  (Rd = 64 Mb/s for IG, Rd = 128 Mb/s for PG).
   *  object_width  : object_width parameter parsed from n-ODS's Object_data().
   *  object_height : object_height parameter parsed from n-ODS's Object_data().
   */
  uint64_t picturePxSize = param.object_width * param.object_height;

  if (0 == picturePxSize)
    return 0; /* Empty object, no decoding delay. */

  return DIV_ROUND_UP(
    9 * picturePxSize,
    (type == HDMV_STREAM_TYPE_IGS) ? 800 : 1600
  );
}

/* ### HDMV Segments Inventory Pool : ###################################### */

static int increaseAllocationHdmvSegmentsInventoryPool(
  HdmvSegmentsInventoryPool * pool
)
{
  void * newSegment;
  size_t newSegmentSize;

  if (pool->allocatedElementsSegments <= pool->usedElementsSegments) {
    /* Reallocate segments array */
    void ** newArray;
    size_t i, newSize;

    newSize = GROW_ALLOCATION(
      pool->allocatedElementsSegments,
      HDMV_SEG_INV_POOL_DEFAULT_SEG_NB
    );
    if (lb_mul_overflow(newSize, sizeof(void *)))
      LIBBLU_HDMV_QUANT_ERROR_RETURN("Inventory segments number overflow.\n");

    newArray = (void **) realloc(pool->elements, newSize * sizeof(void *));
    if (NULL == newArray)
      LIBBLU_HDMV_QUANT_ERROR_RETURN("Memory allocation error.\n");
    for (i = pool->allocatedElementsSegments; i < newSize; i++)
      newArray[i] = NULL;

    pool->elements = newArray;
    pool->allocatedElementsSegments = newSize;
  }

  newSegmentSize =
    HDMV_SEG_INV_POOL_DEFAULT_SEG_SIZE
    << pool->usedElementsSegments
  ;

  if (NULL == pool->elements[pool->usedElementsSegments]) {
    if (lb_mul_overflow(newSegmentSize, pool->elementSize))
      LIBBLU_HDMV_QUANT_ERROR_RETURN("Inventory segment size overflow.\n");

    /* Use of calloc to avoid wild pointer-dereferences when cleaning mem. */
    if (NULL == (newSegment = calloc(newSegmentSize, pool->elementSize)))
      LIBBLU_HDMV_QUANT_ERROR_RETURN("Memory allocation error.\n");
    pool->elements[pool->usedElementsSegments] = newSegment;
  }

  pool->usedElementsSegments++;
  pool->remainingElements = newSegmentSize;

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
    if (!(p)->remainingElements) {                                            \
      if (increaseAllocationHdmvSegmentsInventoryPool(p) < 0)                 \
        return NULL;                                                          \
    }                                                                         \
    d = &(                                                                    \
      (tp) (p)->elements[(p)->usedElementsSegments-1]                         \
    )[--(p)->remainingElements];                                              \
  } while (0)

HdmvSequencePtr getHdmvSequenceHdmvSegmentsInventory(
  HdmvSegmentsInventoryPtr inv
)
{
  HdmvSequencePtr seq;

  GET_HDMV_SEGMENTS_INVENTORY_POOL(seq, HdmvSequence *, &inv->sequences);
  if (NULL == seq)
    return NULL;
  initHdmvSequence(seq);

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

/* ######################################################################### */

HdmvSegmentsContextPtr createHdmvSegmentsContext(
  HdmvStreamType streamType,
  BitstreamWriterPtr hdmvOutputScriptFile,
  EsmsFileHeaderPtr hdmvEsmsHeader,
  unsigned hdmvEsmsSrcFileIdx
)
{
  HdmvSegmentsContextPtr ctx;

  assert(NULL != hdmvEsmsHeader);

  ctx = (HdmvSegmentsContextPtr) malloc(sizeof(HdmvSegmentsContext));
  if (NULL == ctx)
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  *ctx = (HdmvSegmentsContext) {
    .streamType = streamType,
    .outputScriptFile = hdmvOutputScriptFile,
    .esmsHeader = hdmvEsmsHeader,
    .srcFileIdx = hdmvEsmsSrcFileIdx
  };

  if (NULL == (ctx->segInv = createHdmvSegmentsInventory()))
    goto free_return;

  return ctx;

free_return:
  destroyHdmvContext(ctx);
  return NULL;
}

void resetHdmvContext(
  HdmvSegmentsContextPtr ctx
)
{
  if (NULL == ctx)
    return;

  clearHdmvSegmentsInventory(ctx->segInv);

  ctx->segRefs.pcs = NULL;
  ctx->segRefs.wds = NULL;
  ctx->segRefs.ics = NULL;
  ctx->segRefs.pds = NULL;
  ctx->segRefs.ods = NULL;
  ctx->segRefs.end = NULL;

  ctx->curNbPcsSegments = 0;
  ctx->curNbWdsSegments = 0;
  ctx->curNbIcsSegments = 0;
  ctx->curNbPdsSegments = 0;
  ctx->curNbOdsSegments = 0;
  ctx->curNbEndSegments = 0;
  ctx->curEpochNbSegments = 0;
}

void destroyHdmvContext(HdmvSegmentsContextPtr ctx)
{
  if (NULL == ctx)
    return;

  destroyHdmvSegmentsInventory(ctx->segInv);
  free(ctx);
}

HdmvSequencePtr addSegmentToSequence(
  HdmvSegmentsInventoryPtr inv,
  HdmvSequencePtr * list,
  size_t maxNumberOfSequences,
  const HdmvSegmentParameters param,
  const HdmvSDParameters segSeqDesc
)
{
  HdmvSequencePtr seq;
  HdmvSegmentPtr seg, newSeg;
  size_t seqNb;
  bool brokenSequence;

  assert(NULL != inv);
  assert(NULL != list);
  assert(0x00 != param.type);

  if (isFirstInSequenceHdmvSDParameters(&segSeqDesc)) {
    HdmvSequencePtr newSeq;

    if (NULL != *list) {
      /* Check for previous sequence completion */
      seg = (*list)->segments->lastSegment;

      switch (seg->param.type) {
        case HDMV_SEGMENT_TYPE_PDS:
          brokenSequence = false; /* PDS segments does not compose sequences. */
          break;

        case HDMV_SEGMENT_TYPE_ODS:
          brokenSequence = !isLastInSequenceHdmvSDParameters(
            &seg->param.data.odsParam.sequence_descriptor
          ); /* Broken if last segment of last sequence is not marked. */
          break;

        case HDMV_SEGMENT_TYPE_PCS:
          brokenSequence = false; /* PCS segments does not compose sequences. */
          break;

        case HDMV_SEGMENT_TYPE_WDS:
          brokenSequence = false; /* WDS segments does not compose sequences. */
          break;

        case HDMV_SEGMENT_TYPE_ICS:
          brokenSequence = !isLastInSequenceHdmvSDParameters(
            &seg->param.data.igsParam.sequence_descriptor
          ); /* Broken if last segment of last sequence is not marked. */
          break;

        case HDMV_SEGMENT_TYPE_END:
          brokenSequence = false; /* END segments does not compose sequences. */
          break;

        default:
          LIBBLU_ERROR("Error segment encountered, broken program.\n");
          return NULL;
      }

      if (brokenSequence)
        LIBBLU_HDMV_COM_ERROR_NRETURN(
          "Missing a sequence completion segment, "
          "last sequence as never been finished.\n"
        );
    }

    /* Creating a new sequence */
    if (NULL == (newSeq = getHdmvSequenceHdmvSegmentsInventory(inv)))
      return NULL;
    newSeq->lastSequence = newSeq;
    newSeq->type = param.type;

    seqNb = 0;
    for (
      seq = *list;
      NULL != seq && NULL != seq->nextSequence;
      seq = seq->nextSequence
    ) {
      assert(seq->type == param.type);
      seq->lastSequence = newSeq;
      seqNb++;
    }

    if (NULL == seq)
      *list = newSeq;
    else
      seq->nextSequence = seq->lastSequence = newSeq;

    if (maxNumberOfSequences <= seqNb)
      LIBBLU_HDMV_COM_ERROR_NRETURN(
        "Too many %ss in one epoch.\n",
        HdmvSegmentTypeStr(param.type)
      );
  }
  else {
    if (NULL == *list)
      LIBBLU_HDMV_COM_ERROR_NRETURN(
        "Receive partial following segment, "
        "but no start segment has been parsed before.\n"
      );

    /* Check for sequence non-completion */
    seg = (*list)->lastSequence->segments->lastSegment;
    switch (seg->param.type) {
      case HDMV_SEGMENT_TYPE_PDS:
        brokenSequence = true; /* PDS segments does not compose sequences. */
        break;

      case HDMV_SEGMENT_TYPE_ODS:
        brokenSequence = isLastInSequenceHdmvSDParameters(
          &seg->param.data.odsParam.sequence_descriptor
        ); /* Broken if last segment was marked as last one. */
        break;

      case HDMV_SEGMENT_TYPE_PCS:
        brokenSequence = true; /* PCS segments does not compose sequences. */
        break;

      case HDMV_SEGMENT_TYPE_WDS:
        brokenSequence = true; /* WDS segments does not compose sequences. */
        break;

      case HDMV_SEGMENT_TYPE_ICS:
        brokenSequence = isLastInSequenceHdmvSDParameters(
          &seg->param.data.igsParam.sequence_descriptor
        ); /* Broken if last segment was marked as last one. */
        break;

      case HDMV_SEGMENT_TYPE_END:
        brokenSequence = true; /* END segments does not compose sequences. */
        break;

      default:
        LIBBLU_ERROR("Error segment encountered, broken program.\n");
        return NULL;
    }

    if (brokenSequence)
      LIBBLU_HDMV_COM_ERROR_NRETURN(
        "Receive a following segment after a sequence completion one.\n"
      );
  }

  seq = (*list)->lastSequence;

  if (NULL == (newSeg = getHdmvSegmentHdmvSegmentsInventory(inv)))
    return NULL;
  newSeg->param = param;
  newSeg->lastSegment = newSeg;

  for (
    seg = seq->segments;
    NULL != seg && NULL != seg->nextSegment;
    seg = seg->nextSegment
  )
    seg->lastSegment = newSeg;

  if (NULL == seg)
    seq->segments = newSeg;
  else
    seg->nextSegment = seg->lastSegment = newSeg;

  return seq;
}

int insertHdmvSegment(
  HdmvSegmentsContextPtr ctx,
  HdmvSegmentParameters seg,
  uint64_t pts,
  uint64_t dts
)
{
  int ret;
  bool dtsPresent;

  assert(NULL != ctx);
  assert(seg.type != HDMV_SEGMENT_TYPE_ERROR);

  dtsPresent = (
    seg.type == HDMV_SEGMENT_TYPE_ODS
    || seg.type == HDMV_SEGMENT_TYPE_PCS
    || seg.type == HDMV_SEGMENT_TYPE_WDS
    || seg.type == HDMV_SEGMENT_TYPE_ICS
  );

  ret = initEsmsHdmvPesFrame(
    ctx->esmsHeader, dtsPresent, pts, dts
  );

  if (ret < 0)
    return -1;

  ret = appendAddPesPayloadCommand(
    ctx->esmsHeader,
    ctx->srcFileIdx,
    0x0,
    seg.inputFileOffset,
    seg.length + HDMV_SEGMENT_HEADER_LENGTH
  );

  if (ret < 0)
    return -1;

  return writeEsmsPesPacket(ctx->outputScriptFile, ctx->esmsHeader);
}

int decodeHdmvVideoDescriptor(BitstreamReaderPtr hdmvInput, HdmvVDParameters * param)
{
  uint32_t value;

  assert(NULL != hdmvInput);
  assert(NULL != param);

  LIBBLU_HDMV_COM_DEBUG(" Video_descriptor():\n");

  /* [u16 video_width] */
  if (readValueBigEndian(hdmvInput, 2, &value) < 0)
    return -1;
  param->video_width = value;

  LIBBLU_HDMV_COM_DEBUG(
    "  video_width: %u (0x%x).\n",
    param->video_width, param->video_width
  );

  /* [u16 video_height] */
  if (readValueBigEndian(hdmvInput, 2, &value) < 0)
    return -1;
  param->video_height = value;

  LIBBLU_HDMV_COM_DEBUG(
    "  video_height: %u (0x%x).\n",
    param->video_height, param->video_height
  );

  /* [u4 frame_rate_id] [v4 reserved] */
  if (readValueBigEndian(hdmvInput, 1, &value) < 0)
    return -1;
  param->frame_rate = (value >> 4) & 0xF;

  LIBBLU_HDMV_COM_DEBUG(
    "  frame_rate_id: 0x%" PRIx8 " (%.3f).\n",
    param->frame_rate,
    frameRateCodeToFloat(param->frame_rate)
  );
  LIBBLU_HDMV_COM_DEBUG(
    "  reserved: 0x%" PRIx8 ".\n",
    value & 0xF
  );

  return 0;
}

int decodeHdmvCompositionDescriptor(
  BitstreamReaderPtr hdmvInput, HdmvCDParameters * param
)
{
  uint32_t value;

  assert(NULL != hdmvInput);
  assert(NULL != param);

  LIBBLU_HDMV_COM_DEBUG(" Composition_descriptor():\n");

  /* [u16 composition_number] */
  if (readValueBigEndian(hdmvInput, 2, &value) < 0)
    return -1;
  param->compositionNumber = value;

  LIBBLU_HDMV_COM_DEBUG(
    "  composition_number: %u (0x%x).\n",
    param->compositionNumber, param->compositionNumber
  );

  /* [u2 composition_state] [v6 reserved] */
  if (readValueBigEndian(hdmvInput, 1, &value) < 0)
    return -1;
  param->composition_state = (value >> 6) & 0x3;

  LIBBLU_HDMV_COM_DEBUG(
    "  composition_state: %s (0x%x).\n",
    hdmvCompositionStateStr(param->composition_state),
    param->composition_state
  );
  LIBBLU_HDMV_COM_DEBUG(
    "  reserved: 0x%x.\n",
    value & 0x3F
  );

  return 0;
}

int decodeHdmvSequenceDescriptor(
  BitstreamReaderPtr hdmvInput, HdmvSDParameters * param
)
{
  uint32_t value;

  assert(NULL != hdmvInput);
  assert(NULL != param);

  LIBBLU_HDMV_COM_DEBUG(" Sequence_descriptor():\n");

  /* [b1 first_in_sequence] [b1 last_in_sequence] [v6 reserved] */
  if (readValueBigEndian(hdmvInput, 1, &value) < 0)
    return -1;
  param->first_in_sequence = (value >> 7) & 0x1;
  param->last_in_sequence  = (value >> 6) & 0x1;

  LIBBLU_HDMV_COM_DEBUG(
    "  first_in_sequence: %s.\n",
    BOOL_STR(param->first_in_sequence)
  );
  LIBBLU_HDMV_COM_DEBUG(
    "  last_in_sequence: %s.\n",
    BOOL_STR(param->last_in_sequence)
  );
  LIBBLU_HDMV_COM_DEBUG(
    "  reserved: 0x%x.\n",
    value & 0x3F
  );

  return 0;
}

int readHdmvDataFragment(
  BitstreamReaderPtr hdmvInput,
  size_t length,
  uint8_t ** fragment,
  size_t * fragmentAllocatedLen,
  size_t * fragmentUsedLen
)
{
  uint8_t * newDataBlock;
  size_t newDataBlockLength;

  LIBBLU_HDMV_COM_DEBUG(" Data_fragment():\n");

  if (*fragmentAllocatedLen < *fragmentUsedLen + length) {
    /* Need segment length increasing */
    newDataBlockLength = *fragmentAllocatedLen + length;

    if (
      newDataBlockLength <= *fragmentAllocatedLen
      || newDataBlockLength < *fragmentUsedLen + length
    ) {
      /* Overflow */
      LIBBLU_HDMV_COM_ERROR_RETURN(
        "Segments recomposed data is too big.\n"
      );
    }

    newDataBlock = (uint8_t *) realloc(*fragment, newDataBlockLength);
    if (NULL == newDataBlock)
      LIBBLU_HDMV_COM_ERROR_RETURN("Memory allocation error.\n");
    *fragment = newDataBlock;
    *fragmentAllocatedLen = newDataBlockLength;
  }

  LIBBLU_HDMV_COM_DEBUG(
    "  length: %zu bytes.\n",
    length
  );

  if (
    readBytes(
      hdmvInput,
      *fragment + *fragmentUsedLen,
      length
    ) < 0
  )
    return -1;
  *fragmentUsedLen += length;
  return 0;
}

int decodeHdmvCompositionObject(
  BitstreamReaderPtr hdmvInput,
  HdmvCompositionObjectParameters * param
)
{
  uint32_t value;

  assert(NULL != hdmvInput);
  assert(NULL != param);

  /* [u16 object_id_ref] */
  if (readValueBigEndian(hdmvInput, 2, &value) < 0)
    return -1;
  param->object_id_ref = value & 0xFFFF;

  LIBBLU_HDMV_COM_DEBUG(
    "    object_id_ref: %" PRIu16 " (0x%" PRIx16 ").\n",
    param->object_id_ref,
    param->object_id_ref
  );

  /* [u8 window_id_ref] */
  if (readValueBigEndian(hdmvInput, 1, &value) < 0)
    return -1;
  param->window_id_ref = value & 0xFF;

  LIBBLU_HDMV_COM_DEBUG(
    "    window_id_ref: %" PRIu8 " (0x%" PRIx8 ").\n",
    param->window_id_ref,
    param->window_id_ref
  );

  /* [b1 object_cropped_flag] [b1 forced_on_flag] [v6 reserved] */
  if (readValueBigEndian(hdmvInput, 1, &value) < 0)
    return -1;
  param->object_cropped_flag = (value >> 7) & 0x1;
  param->forced_on_flag    = (value >> 6) & 0x1;

  LIBBLU_HDMV_COM_DEBUG(
    "    object_cropped_flag: %s (0b%x).\n",
    BOOL_STR(param->object_cropped_flag),
    param->object_cropped_flag
  );
  LIBBLU_HDMV_COM_DEBUG(
    "    forced_on_flag: %s (0b%x).\n",
    BOOL_STR(param->forced_on_flag),
    param->forced_on_flag
  );
  LIBBLU_HDMV_COM_DEBUG(
    "    reserved: 0x%x.\n",
    value & 0x3F
  );

  /* [u16 object_horizontal_position] */
  if (readValueBigEndian(hdmvInput, 2, &value) < 0)
    return -1;
  param->composition_object_horizontal_position = value & 0xFFFF;

  LIBBLU_HDMV_COM_DEBUG(
    "    object_horizontal_position: %u px (0x%x).\n",
    param->composition_object_horizontal_position,
    param->composition_object_horizontal_position
  );

  /* [u16 object_vertical_position] */
  if (readValueBigEndian(hdmvInput, 2, &value) < 0)
    return -1;
  param->composition_object_vertical_position = value & 0xFFFF;

  LIBBLU_HDMV_COM_DEBUG(
    "    object_vertical_position: %u px (0x%x).\n",
    param->composition_object_vertical_position,
    param->composition_object_vertical_position
  );

  if (param->object_cropped_flag) {
    LIBBLU_HDMV_COM_DEBUG(
      "    if (object_cropped_flag == 0b1):\n"
    );

    /* [u16 object_cropping_horizontal_position] */
    if (readValueBigEndian(hdmvInput, 2, &value) < 0)
      return -1;
    param->object_cropping.horizontal_position = value & 0xFFFF;

    LIBBLU_HDMV_COM_DEBUG(
      "     object_cropping_horizontal_position: %u px (0x%x).\n",
      param->object_cropping.horizontal_position,
      param->object_cropping.horizontal_position
    );

    /* [u16 object_cropping_vertical_position] */
    if (readValueBigEndian(hdmvInput, 2, &value) < 0)
      return -1;
    param->object_cropping.vertical_position = value & 0xFFFF;

    LIBBLU_HDMV_COM_DEBUG(
      "     object_cropping_vertical_position: %u px (0x%x).\n",
      param->object_cropping.vertical_position,
      param->object_cropping.vertical_position
    );

    /* [u16 object_cropping_width] */
    if (readValueBigEndian(hdmvInput, 2, &value) < 0)
      return -1;
    param->object_cropping.width = value & 0xFFFF;

    LIBBLU_HDMV_COM_DEBUG(
      "     object_cropping_width: %u px (0x%x).\n",
      param->object_cropping.width,
      param->object_cropping.width
    );

    /* [u16 object_cropping_height] */
    if (readValueBigEndian(hdmvInput, 2, &value) < 0)
      return -1;
    param->object_cropping.height = value & 0xFFFF;

    LIBBLU_HDMV_COM_DEBUG(
      "     object_cropping_height: %u px (0x%x).\n",
      param->object_cropping.height,
      param->object_cropping.height
    );
  }

  return 0;
}

int decodeHdmvPresentationComposition(
  BitstreamReaderPtr hdmvInput,
  HdmvSegmentsContextPtr ctx,
  HdmvPCParameters * param
)
{
  uint32_t value;
  uint8_t i;

  assert(NULL != hdmvInput);
  assert(NULL != param);

  LIBBLU_HDMV_COM_DEBUG(" Presentation_composition():\n");

  /* [b1 palette_update_flag] [v7 reserved] */
  if (readValueBigEndian(hdmvInput, 1, &value) < 0)
    return -1;
  param->palette_update_flag = value >> 7;

  LIBBLU_HDMV_COM_DEBUG(
    "  palette_update_flag: %s (0b%x).\n",
    BOOL_STR(param->palette_update_flag),
    param->palette_update_flag
  );
  LIBBLU_HDMV_COM_DEBUG(
    "  reserved: 0x%x.\n",
    value & 0x7F
  );

  /* [u8 palette_id_ref] */
  if (readValueBigEndian(hdmvInput, 1, &value) < 0)
    return -1;
  param->palette_id_ref = value;

  LIBBLU_HDMV_COM_DEBUG(
    "  palette_id_ref: %" PRIu8 " (0x%" PRIX8 ").\n",
    param->palette_id_ref,
    param->palette_id_ref
  );

  /* [u8 number_of_composition_objects] */
  if (readValueBigEndian(hdmvInput, 1, &value) < 0)
    return -1;
  param->number_of_composition_objects = value;

  LIBBLU_HDMV_COM_DEBUG(
    "  number_of_composition_objects: %" PRIu8 " (0x%" PRIX8 ").\n",
    param->number_of_composition_objects,
    param->number_of_composition_objects
  );

  if (0 < param->number_of_composition_objects) {
    /* Allocating presentation composition objects : */
    for (i = 0; i < param->number_of_composition_objects; i++) {
      param->composition_objects[i] =
        getHdmvCompositionObjectParametersHdmvSegmentsInventory(
          ctx->segInv
        )
      ;
      if (NULL == param->composition_objects[i])
        return -1;
    }

    LIBBLU_HDMV_COM_DEBUG("  Composition_objects():\n");

    for (i = 0; i < param->number_of_composition_objects; i++) {
      LIBBLU_HDMV_COM_DEBUG("   Composition_object(%" PRIu8 "):\n", i);

      if (decodeHdmvCompositionObject(hdmvInput, param->composition_objects[i]) < 0)
        return -1;
    }
  }

  return 0;
}

int decodeHdmvPcsSegment(
  BitstreamReaderPtr hdmvInput,
  HdmvSegmentsContextPtr ctx,
  HdmvSegmentParameters seg
)
{
  HdmvPcsSegmentParameters * param;

  const HdmvSDParameters pcsSequenceParam = {
    .first_in_sequence = true,
    .last_in_sequence = true
  };

  assert(NULL != hdmvInput);
  assert(NULL != ctx);
  assert(seg.type == HDMV_SEGMENT_TYPE_PCS);

  param = &seg.data.pcsParam;

  /* Video_descriptor() */
  if (decodeHdmvVideoDescriptor(hdmvInput, &param->video_descriptor) < 0)
    return -1;

  ctx->videoDesc = param->video_descriptor;

  /* Composition_descriptor() */
  if (decodeHdmvCompositionDescriptor(hdmvInput, &param->composition_descriptor) < 0)
    return -1;

  ctx->compoState = param->composition_descriptor.composition_state;

  /* Presentation_composition() */
  if (decodeHdmvPresentationComposition(hdmvInput, ctx, &param->presentation_composition) < 0)
    return -1;

  /* Add complete sequence to array. */
  if (
    NULL == addSegmentToSequence(
      ctx->segInv,
      &ctx->segRefs.pcs,
      PGS_MAX_NB_SEG_PCS,
      seg,
      pcsSequenceParam
    )
  )
    return -1;
  ctx->curNbPcsSegments++;
  ctx->nbPcsSegments++;

  return 0;
}

int decodeHdmvWindowInfo(
  BitstreamReaderPtr hdmvInput,
  HdmvWindowInfoParameters * param
)
{
  uint32_t value;

  assert(NULL != hdmvInput);
  assert(NULL != param);

  /* [u8 window_id] */
  if (readValueBigEndian(hdmvInput, 1, &value) < 0)
    return -1;
  param->window_id = value & 0xFF;

  LIBBLU_HDMV_COM_DEBUG(
    "    window_id: %" PRIu8 " (0x%" PRIx8 ").\n",
    param->window_id,
    param->window_id
  );

  /* [u16 window_horizontal_position] */
  if (readValueBigEndian(hdmvInput, 2, &value) < 0)
    return -1;
  param->window_horizontal_position = value & 0xFFFF;

  LIBBLU_HDMV_COM_DEBUG(
    "    window_horizontal_position: %" PRIu16 " px (0x%" PRIx16 ").\n",
    param->window_horizontal_position,
    param->window_horizontal_position
  );

  /* [u16 window_vertical_position] */
  if (readValueBigEndian(hdmvInput, 2, &value) < 0)
    return -1;
  param->window_vertical_position = value & 0xFFFF;

  LIBBLU_HDMV_COM_DEBUG(
    "    window_vertical_position: %" PRIu16 " px (0x%" PRIx16 ").\n",
    param->window_vertical_position,
    param->window_vertical_position
  );

  /* [u16 window_width] */
  if (readValueBigEndian(hdmvInput, 2, &value) < 0)
    return -1;
  param->window_width = value & 0xFFFF;

  LIBBLU_HDMV_COM_DEBUG(
    "    window_width: %" PRIu16 " px (0x%" PRIx16 ").\n",
    param->window_width,
    param->window_width
  );

  /* [u16 window_height] */
  if (readValueBigEndian(hdmvInput, 2, &value) < 0)
    return -1;
  param->window_height = value & 0xFFFF;

  LIBBLU_HDMV_COM_DEBUG(
    "    window_height: %" PRIu16 " px (0x%" PRIx16 ").\n",
    param->window_height,
    param->window_height
  );

  return 0;
}

int decodeHdmvWindowsDefinition(
  BitstreamReaderPtr hdmvInput,
  HdmvSegmentsContextPtr ctx,
  HdmvWDParameters * param
)
{
  uint32_t value;
  uint8_t i;

  LIBBLU_HDMV_COM_DEBUG(" Window_definition():\n");

  /* [u8 number_of_windows] */
  if (readValueBigEndian(hdmvInput, 1, &value) < 0)
    return -1;
  param->number_of_windows = value;

  LIBBLU_HDMV_COM_DEBUG(
    "  number_of_windows: %" PRIu8 " (0x%" PRIX8 ").\n",
    param->number_of_windows,
    param->number_of_windows
  );

  if (0 < param->number_of_windows) {
    /* Allocating presentation composition objects : */
    for (i = 0; i < param->number_of_windows; i++) {
      param->windows[i] =
        getHdmvWindowInfoParametersHdmvSegmentsInventory(
          ctx->segInv
        )
      ;
      if (NULL == param->windows[i])
        return -1;
    }

    LIBBLU_HDMV_COM_DEBUG("  Window_info():\n");

    for (i = 0; i < param->number_of_windows; i++) {
      LIBBLU_HDMV_COM_DEBUG("   Window_info(%" PRIu8 "):\n", i);

      if (decodeHdmvWindowInfo(hdmvInput, param->windows[i]) < 0)
        return -1;
    }
  }

  return 0;
}

int decodeHdmvWdsSegment(
  BitstreamReaderPtr hdmvInput,
  HdmvSegmentsContextPtr ctx,
  HdmvSegmentParameters seg
)
{
  HdmvWdsSegmentParameters * param;

  const HdmvSDParameters pcsSequenceParam = {
    .first_in_sequence = true,
    .last_in_sequence = true
  };

  assert(NULL != hdmvInput);
  assert(NULL != ctx);
  assert(seg.type == HDMV_SEGMENT_TYPE_WDS);

  param = &seg.data.wdsParam;

  /* Window_definition() */
  if (decodeHdmvWindowsDefinition(hdmvInput, ctx, &param->window_definition) < 0)
    return -1;

  /* Add complete sequence to array. */
  if (
    NULL == addSegmentToSequence(
      ctx->segInv,
      &ctx->segRefs.wds,
      PGS_MAX_NB_SEG_WDS,
      seg,
      pcsSequenceParam
    )
  )
    return -1;
  ctx->curNbWdsSegments++;
  ctx->nbWdsSegments++;

  return 0;
}

int readValueFromHdmvICS(
  uint8_t ** data,
  size_t * remainingData,
  size_t length,
  uint32_t * v
)
{
  size_t idx = 0;
  uint8_t x;

  assert(NULL != data);
  assert(NULL != remainingData);

  assert(NULL != *data);

  if (length == 0 || length > 4)
    LIBBLU_HDMV_COM_ERROR_RETURN("%s, unsupported length %zu.\n", length);

  *v = 0;
  while (length--) {
    if (0 == ((*remainingData)--))
      LIBBLU_HDMV_COM_ERROR_RETURN("Prematurate end of ICS data.\n");

    x = *((*data)++);

    *v |= (uint32_t) x << (8 * length);
    idx++;
  }

  return 0;
}

int decodeHdmvCompositionObjectFromData(
  uint8_t ** data, size_t * remainingDataLen,
  HdmvCompositionObjectParameters * param, const unsigned i
)
{
  uint32_t value;

  assert(NULL != data);
  assert(NULL != remainingDataLen);
  assert(NULL != param);

  LIBBLU_HDMV_COM_DEBUG("        Composition_object(%u):\n", i);

  /* [u16 object_id_ref] */
  if (readValueFromHdmvICS(data, remainingDataLen, 2, &value) < 0)
    return -1;
  param->object_id_ref = value & 0xFFFF;

  LIBBLU_HDMV_COM_DEBUG(
    "         object_id_ref: %" PRIu16 " (0x%" PRIx16 ").\n",
    param->object_id_ref,
    param->object_id_ref
  );

  /* [u16 window_id_ref] */
  if (readValueFromHdmvICS(data, remainingDataLen, 2, &value) < 0)
    return -1;
  param->window_id_ref = value & 0xFFFF;

  LIBBLU_HDMV_COM_DEBUG(
    "         window_id_ref: %" PRIu16 " (0x%" PRIx16 ").\n",
    param->window_id_ref,
    param->window_id_ref
  );

  /* [b1 object_cropped_flag] */
  if (readValueFromHdmvICS(data, remainingDataLen, 1, &value) < 0)
    return -1;
  param->object_cropped_flag = (value >> 7) & 0x1;

  LIBBLU_HDMV_COM_DEBUG(
    "         object_cropped_flag: %s (0b%x).\n",
    BOOL_STR(param->object_cropped_flag),
    param->object_cropped_flag
  );

  /* [v7 reserved] */
  LIBBLU_HDMV_COM_DEBUG(
    "         reserved: 0x%x.\n",
    value & 0x7F
  );

  /* [u16 object_horizontal_position] */
  if (readValueFromHdmvICS(data, remainingDataLen, 2, &value) < 0)
    return -1;
  param->composition_object_horizontal_position = value & 0xFFFF;

  LIBBLU_HDMV_COM_DEBUG(
    "         object_horizontal_position: %u px (0x%x).\n",
    param->composition_object_horizontal_position,
    param->composition_object_horizontal_position
  );

  /* [u16 object_vertical_position] */
  if (readValueFromHdmvICS(data, remainingDataLen, 2, &value) < 0)
    return -1;
  param->composition_object_vertical_position = value & 0xFFFF;

  LIBBLU_HDMV_COM_DEBUG(
    "         object_vertical_position: %u px (0x%x).\n",
    param->composition_object_vertical_position,
    param->composition_object_vertical_position
  );

  if (param->object_cropped_flag) {
    LIBBLU_HDMV_COM_DEBUG(
      "         if (object_cropped_flag == 0b1):\n"
    );

    /* [u16 object_cropping_horizontal_position] */
    if (readValueFromHdmvICS(data, remainingDataLen, 2, &value) < 0)
      return -1;
    param->object_cropping.horizontal_position = value & 0xFFFF;

    LIBBLU_HDMV_COM_DEBUG(
      "          object_cropping_horizontal_position: %u px (0x%x).\n",
      param->object_cropping.horizontal_position,
      param->object_cropping.horizontal_position
    );

    /* [u16 object_cropping_vertical_position] */
    if (readValueFromHdmvICS(data, remainingDataLen, 2, &value) < 0)
      return -1;
    param->object_cropping.vertical_position = value & 0xFFFF;

    LIBBLU_HDMV_COM_DEBUG(
      "          object_cropping_vertical_position: %u px (0x%x).\n",
      param->object_cropping.vertical_position,
      param->object_cropping.vertical_position
    );

    /* [u16 object_cropping_width] */
    if (readValueFromHdmvICS(data, remainingDataLen, 2, &value) < 0)
      return -1;
    param->object_cropping.width = value & 0xFFFF;

    LIBBLU_HDMV_COM_DEBUG(
      "          object_cropping_width: %u px (0x%x).\n",
      param->object_cropping.width,
      param->object_cropping.width
    );

    /* [u16 object_cropping_height] */
    if (readValueFromHdmvICS(data, remainingDataLen, 2, &value) < 0)
      return -1;
    param->object_cropping.height = value & 0xFFFF;

    LIBBLU_HDMV_COM_DEBUG(
      "          object_cropping_height: %u px (0x%x).\n",
      param->object_cropping.height,
      param->object_cropping.height
    );
  }

  return 0;
}

int decodeHdmvEffectInfoFromData(
  uint8_t ** data, size_t * remainingDataLen,
  HdmvSegmentsContextPtr ctx, HdmvEffectInfoParameters * param, const unsigned i
)
{
  uint32_t value;
  unsigned j;

  assert(NULL != data);
  assert(NULL != param);

  LIBBLU_HDMV_COM_DEBUG("      Effect_info(%u):\n", i);

  /* [u24 effect_duration] */
  if (readValueFromHdmvICS(data, remainingDataLen, 3, &value) < 0)
    return -1;
  param->effect_duration = value & 0xFFFFFF;

  LIBBLU_HDMV_COM_DEBUG(
    "       effect_duration: %" PRIu32 " (0x%" PRIx32 ").\n",
    param->effect_duration,
    param->effect_duration
  );

  /* [u8 palette_id_ref] */
  if (readValueFromHdmvICS(data, remainingDataLen, 1, &value) < 0)
    return -1;
  param->palette_id_ref = value & 0xFF;

  LIBBLU_HDMV_COM_DEBUG(
    "       palette_id_ref: %" PRIu8 " (0x%" PRIx8 ").\n",
    param->palette_id_ref,
    param->palette_id_ref
  );

  /* [u8 number_of_composition_objects] */
  if (readValueFromHdmvICS(data, remainingDataLen, 1, &value) < 0)
    return -1;
  param->number_of_composition_objects = value & 0xFF;

  LIBBLU_HDMV_COM_DEBUG(
    "       number_of_composition_objects: %" PRIu8 " objects (0x%" PRIx8 ").\n",
    param->number_of_composition_objects,
    param->number_of_composition_objects
  );

  if (0 < param->number_of_composition_objects) {
    /* Allocating composition objects : */
    for (j = 0; j < param->number_of_composition_objects; j++) {
      param->composition_objects[j] =
        getHdmvCompositionObjectParametersHdmvSegmentsInventory(
          ctx->segInv
        )
      ;
      if (NULL == param->composition_objects[j])
        return -1;
    }

    LIBBLU_HDMV_COM_DEBUG("       Composition objects():\n");
    for (j = 0; j < param->number_of_composition_objects; j++) {
      /* Composition_object() */
      if (decodeHdmvCompositionObjectFromData(data, remainingDataLen, param->composition_objects[j], j) < 0)
        return -1;
    }
  }

  return 0;
}

int decodeHdmvWindowInfoFromData(
  uint8_t ** data, size_t * remainingDataLen,
  HdmvWindowInfoParameters * param, const unsigned i
)
{
  uint32_t value;

  assert(NULL != data);
  assert(NULL != remainingDataLen);
  assert(NULL != param);

  LIBBLU_HDMV_COM_DEBUG("      Window_info(%u):\n", i);

  /* [u8 window_id] */
  if (readValueFromHdmvICS(data, remainingDataLen, 1, &value) < 0)
    return -1;
  param->window_id = value & 0xFF;

  LIBBLU_HDMV_COM_DEBUG(
    "       window_id: %" PRIu8 " (0x%" PRIx8 ").\n",
    param->window_id,
    param->window_id
  );

  /* [u16 window_horizontal_position] */
  if (readValueFromHdmvICS(data, remainingDataLen, 2, &value) < 0)
    return -1;
  param->window_horizontal_position = value & 0xFFFF;

  LIBBLU_HDMV_COM_DEBUG(
    "       window_horizontal_position: %" PRIu16 " px (0x%" PRIx16 ").\n",
    param->window_horizontal_position,
    param->window_horizontal_position
  );

  /* [u16 window_vertical_position] */
  if (readValueFromHdmvICS(data, remainingDataLen, 2, &value) < 0)
    return -1;
  param->window_vertical_position = value & 0xFFFF;

  LIBBLU_HDMV_COM_DEBUG(
    "       window_vertical_position: %" PRIu16 " px (0x%" PRIx16 ").\n",
    param->window_vertical_position,
    param->window_vertical_position
  );

  /* [u16 window_width] */
  if (readValueFromHdmvICS(data, remainingDataLen, 2, &value) < 0)
    return -1;
  param->window_width = value & 0xFFFF;

  LIBBLU_HDMV_COM_DEBUG(
    "       window_width: %" PRIu16 " px (0x%" PRIx16 ").\n",
    param->window_width,
    param->window_width
  );

  /* [u16 window_height] */
  if (readValueFromHdmvICS(data, remainingDataLen, 2, &value) < 0)
    return -1;
  param->window_height = value & 0xFFFF;

  LIBBLU_HDMV_COM_DEBUG(
    "       window_height: %" PRIu16 " px (0x%" PRIx16 ").\n",
    param->window_height,
    param->window_height
  );

  return 0;
}

int decodeHdmvEffectSequence(
  uint8_t ** data, size_t * remainingDataLen,
  HdmvSegmentsContextPtr ctx, HdmvEffectSequenceParameters * param
)
{
  uint32_t value;
  unsigned i;

  assert(NULL != data);
  assert(NULL != remainingDataLen);
  assert(NULL != param);

  LIBBLU_HDMV_COM_DEBUG("     Effect_sequence():\n");

  /* [u8 number_of_windows] */
  if (readValueFromHdmvICS(data, remainingDataLen, 1, &value) < 0)
    return -1;
  param->number_of_windows = value & 0xFF;

  LIBBLU_HDMV_COM_DEBUG(
    "      number_of_windows: %" PRIu8 " (0x%" PRIx8 ").\n",
    param->number_of_windows,
    param->number_of_windows
  );

  assert(param->number_of_windows <= HDMV_MAX_NB_ICS_WINDOWS); /* Shall always be the case. */

  if (0 < param->number_of_windows) {
    /* Allocating windows : */
    for (i = 0; i < param->number_of_windows; i++) {
      param->windows[i] = getHdmvWindowInfoParametersHdmvSegmentsInventory(
        ctx->segInv
      );
      if (NULL == param->windows[i])
        return -1;
    }

    LIBBLU_HDMV_COM_DEBUG("     Windows():\n");
    for (i = 0; i < param->number_of_windows; i++) {
      /* Window_info() */
      if (decodeHdmvWindowInfoFromData(data, remainingDataLen, param->windows[i], i) < 0)
        return -1;
    }
  }

  /* [u8 number_of_effects] */
  if (readValueFromHdmvICS(data, remainingDataLen, 1, &value) < 0)
    return -1;
  param->number_of_effects = value & 0xFF;

  LIBBLU_HDMV_COM_DEBUG(
    "      number_of_effects: %" PRIu8 " (0x%" PRIx8 ").\n",
    param->number_of_effects,
    param->number_of_effects
  );

  if (0 < param->number_of_effects) {
    /* Allocating effects : */
    for (i = 0; i < param->number_of_effects; i++) {
      param->effects[i] = getHdmvEffectInfoParametersHdmvSegmentsInventory(
        ctx->segInv
      );
      if (NULL == param->effects[i])
        return -1;
    }

    LIBBLU_HDMV_COM_DEBUG("      Effects():\n");
    for (i = 0; i < param->number_of_effects; i++) {
      /* Effect_info() */
      if (decodeHdmvEffectInfoFromData(data, remainingDataLen, ctx, param->effects[i], i) < 0)
        return -1;
    }
  }

  return 0;
}

int decodeHdmvButton(
  uint8_t ** data,
  size_t * remainingDataLen,
  HdmvSegmentsContextPtr ctx,
  HdmvButtonParam * param,
  const unsigned i
)
{
  uint32_t value;
  unsigned j;

  unsigned nbNavCommands;

  assert(NULL != data);
  assert(NULL != remainingDataLen);
  assert(NULL != ctx);
  assert(NULL != param);

  param->commands = NULL;

  LIBBLU_HDMV_COM_DEBUG("         Button(%u):\n", i);

  /* [u16 button_id] */
  if (readValueFromHdmvICS(data, remainingDataLen, 2, &value) < 0)
    return -1;
  param->button_id = value & 0xFFFF;

  LIBBLU_HDMV_COM_DEBUG(
    "          button_id: %" PRIu16 " (0x%" PRIx16 ").\n",
    param->button_id,
    param->button_id
  );

  /* [u16 button_numeric_select_value] */
  if (readValueFromHdmvICS(data, remainingDataLen, 2, &value) < 0)
    return -1;
  param->button_numeric_select_value = value & 0xFFFF;

  LIBBLU_HDMV_COM_DEBUG(
    "          button_numeric_select_value: %" PRIu16 " (0x%" PRIx16 ").\n",
    param->button_numeric_select_value,
    param->button_numeric_select_value
  );

  if (param->button_numeric_select_value == 0xFFFF)
    LIBBLU_HDMV_COM_DEBUG("           -> No value.\n");

  /* [b1 auto_action_flag] [v7 reserved] */
  if (readValueFromHdmvICS(data, remainingDataLen, 1, &value) < 0)
    return -1;
  param->auto_action = (value >> 7) & 0x1;

  LIBBLU_HDMV_COM_DEBUG(
    "          auto_action_flag: %s (0x%x).\n",
    BOOL_STR(param->auto_action),
    param->auto_action
  );

  LIBBLU_HDMV_COM_DEBUG(
    "          reserved: 0x%x.\n",
    value & 0x7F
  );

  /* [u16 button_horizontal_position] */
  if (readValueFromHdmvICS(data, remainingDataLen, 2, &value) < 0)
    return -1;
  param->button_horizontal_position = (uint16_t) value;

  LIBBLU_HDMV_COM_DEBUG(
    "          button_horizontal_position: %" PRIu16 " px (0x%" PRIX16 ").\n",
    param->button_horizontal_position,
    param->button_horizontal_position
  );

  /* [u16 button_vertical_position] */
  if (readValueFromHdmvICS(data, remainingDataLen, 2, &value) < 0)
    return -1;
  param->button_vertical_position = (uint16_t) value;

  LIBBLU_HDMV_COM_DEBUG(
    "          button_vertical_position: %" PRIu16 " px (0x%" PRIX16 ").\n",
    param->button_vertical_position,
    param->button_vertical_position
  );

  /* neighbor_info() */
  {
    LIBBLU_HDMV_COM_DEBUG("          Neighbor_info():\n");

    /* [u16 upper_button_id_ref] */
    if (readValueFromHdmvICS(data, remainingDataLen, 2, &value) < 0)
      return -1;
    param->neighbor_info.upper_button_id_ref = (uint16_t) value;

    LIBBLU_HDMV_COM_DEBUG(
      "           upper_button_id_ref: %" PRIu16 " (0x%" PRIx16 ").\n",
      param->neighbor_info.upper_button_id_ref,
      param->neighbor_info.upper_button_id_ref
    );

    /* [u16 lower_button_id_ref] */
    if (readValueFromHdmvICS(data, remainingDataLen, 2, &value) < 0)
      return -1;
    param->neighbor_info.lower_button_id_ref = value & 0xFFFF;

    LIBBLU_HDMV_COM_DEBUG(
      "           lower_button_id_ref: %" PRIu16 " (0x%" PRIx16 ").\n",
      param->neighbor_info.lower_button_id_ref,
      param->neighbor_info.lower_button_id_ref
    );

    /* [u16 left_button_id_ref] */
    if (readValueFromHdmvICS(data, remainingDataLen, 2, &value) < 0)
      return -1;
    param->neighbor_info.left_button_id_ref = value & 0xFFFF;

    LIBBLU_HDMV_COM_DEBUG(
      "           left_button_id_ref: %" PRIu16 " (0x%" PRIx16 ").\n",
      param->neighbor_info.left_button_id_ref,
      param->neighbor_info.left_button_id_ref
    );

    /* [u16 right_button_id_ref] */
    if (readValueFromHdmvICS(data, remainingDataLen, 2, &value) < 0)
      return -1;
    param->neighbor_info.right_button_id_ref = value & 0xFFFF;

    LIBBLU_HDMV_COM_DEBUG(
      "           right_button_id_ref: %" PRIu16 " (0x%" PRIx16 ").\n",
      param->neighbor_info.right_button_id_ref,
      param->neighbor_info.right_button_id_ref
    );
  }

  /* normal_state_info() */
  {
    LIBBLU_HDMV_COM_DEBUG("          Normal_state_info():\n");

    /* [u16 normal_start_object_id_ref] */
    if (readValueFromHdmvICS(data, remainingDataLen, 2, &value) < 0)
      return -1;
    param->normal_state_info.start_object_ref = value & 0xFFFF;

    LIBBLU_HDMV_COM_DEBUG(
      "           normal_start_object_id_ref: %" PRIu16 " (0x%" PRIx16 ").\n",
      param->normal_state_info.start_object_ref,
      param->normal_state_info.start_object_ref
    );

    /* [u16 normal_end_object_id_ref] */
    if (readValueFromHdmvICS(data, remainingDataLen, 2, &value) < 0)
      return -1;
    param->normal_state_info.end_object_ref = value & 0xFFFF;

    LIBBLU_HDMV_COM_DEBUG(
      "           normal_end_object_id_ref: %" PRIu16 " (0x%" PRIx16 ").\n",
      param->normal_state_info.end_object_ref,
      param->normal_state_info.end_object_ref
    );

    /* [b1 normal_repeat_flag] [b1 normal_complete_flag] [v6 reserved] */
    if (readValueFromHdmvICS(data, remainingDataLen, 1, &value) < 0)
      return -1;
    param->normal_state_info.repeat   = (value >> 7) & 0x1;

    LIBBLU_HDMV_COM_DEBUG(
      "           normal_repeat_flag: %s (0b%x).\n",
      BOOL_STR(param->normal_state_info.repeat),
      param->normal_state_info.repeat
    );

    param->normal_state_info.complete = (value >> 6) & 0x1;

    LIBBLU_HDMV_COM_DEBUG(
      "           normal_complete_flag: %s (0b%x).\n",
      BOOL_STR(param->normal_state_info.complete),
      param->normal_state_info.complete
    );

    LIBBLU_HDMV_COM_DEBUG(
      "           reserved: 0x%x.\n",
      value & 0x3F
    );
  }

  /* selected_state_info() */
  {
    LIBBLU_HDMV_COM_DEBUG("          Selected_state_info():\n");

    /* [u8 selected_state_sound_id_ref] */
    if (readValueFromHdmvICS(data, remainingDataLen, 1, &value) < 0)
      return -1;
    param->selected_state_info.sound_id_ref = value & 0xFF;

    LIBBLU_HDMV_COM_DEBUG(
      "           selected_state_sound_id_ref: %" PRIu8 " (0x%" PRIx8 ").\n",
      param->selected_state_info.sound_id_ref,
      param->selected_state_info.sound_id_ref
    );

    /* [u16 selected_start_object_id_ref] */
    if (readValueFromHdmvICS(data, remainingDataLen, 2, &value) < 0)
      return -1;
    param->selected_state_info.start_object_ref = value & 0xFFFF;

    LIBBLU_HDMV_COM_DEBUG(
      "           selected_start_object_id_ref: %" PRIu16 " (0x%" PRIx16 ").\n",
      param->selected_state_info.start_object_ref,
      param->selected_state_info.start_object_ref
    );

    /* [u16 selected_end_object_id_ref] */
    if (readValueFromHdmvICS(data, remainingDataLen, 2, &value) < 0)
      return -1;
    param->selected_state_info.end_object_ref = value & 0xFFFF;

    LIBBLU_HDMV_COM_DEBUG(
      "           selected_end_object_id_ref: %" PRIu16 " (0x%" PRIx16 ").\n",
      param->selected_state_info.end_object_ref,
      param->selected_state_info.end_object_ref
    );

    /* [b1 selected_repeat_flag] [b1 selected_complete_flag] [v6 reserved] */
    if (readValueFromHdmvICS(data, remainingDataLen, 1, &value) < 0)
      return -1;
    param->selected_state_info.repeat   = (value >> 7) & 0x1;

    LIBBLU_HDMV_COM_DEBUG(
      "           selected_repeat_flag: %s (0b%x).\n",
      BOOL_STR(param->selected_state_info.repeat),
      param->selected_state_info.repeat
    );

    param->selected_state_info.complete = (value >> 6) & 0x1;

    LIBBLU_HDMV_COM_DEBUG(
      "           selected_complete_flag: %s (0b%x).\n",
      BOOL_STR(param->selected_state_info.complete),
      param->selected_state_info.complete
    );

    LIBBLU_HDMV_COM_DEBUG(
      "           reserved: 0x%x.\n",
      value & 0x3F
    );
  }

  /* activated_state_info() */
  {
    LIBBLU_HDMV_COM_DEBUG("          Activated_state_info():\n");

    /* [u8 activated_state_sound_id_ref] */
    if (readValueFromHdmvICS(data, remainingDataLen, 1, &value) < 0)
      return -1;
    param->activated_state_info.sound_id_ref = value & 0xFF;

    LIBBLU_HDMV_COM_DEBUG(
      "           activated_state_sound_id_ref: %" PRIu8 " (0x%" PRIx8 ").\n",
      param->activated_state_info.sound_id_ref,
      param->activated_state_info.sound_id_ref
    );

    /* [u16 activated_start_object_id_ref] */
    if (readValueFromHdmvICS(data, remainingDataLen, 2, &value) < 0)
      return -1;
    param->activated_state_info.start_object_ref = value & 0xFFFF;

    LIBBLU_HDMV_COM_DEBUG(
      "           activated_start_object_id_ref: %" PRIu16 " (0x%" PRIx16 ").\n",
      param->activated_state_info.start_object_ref,
      param->activated_state_info.start_object_ref
    );

    /* [u16 activated_end_object_id_ref] */
    if (readValueFromHdmvICS(data, remainingDataLen, 2, &value) < 0)
      return -1;
    param->activated_state_info.end_object_ref = value & 0xFFFF;

    LIBBLU_HDMV_COM_DEBUG(
      "           activated_end_object_id_ref: %" PRIu16 " (0x%" PRIx16 ").\n",
      param->activated_state_info.end_object_ref,
      param->activated_state_info.end_object_ref
    );
  }

  /* [u16 number_of_navigation_commands] */
  if (readValueFromHdmvICS(data, remainingDataLen, 2, &value) < 0)
    return -1;
  nbNavCommands = value & 0xFFFF;

  LIBBLU_HDMV_COM_DEBUG(
    "          number_of_navigation_commands: %u (0x%x).\n",
    nbNavCommands,
    nbNavCommands
  );

  LIBBLU_HDMV_COM_DEBUG("          Navigation_commands():\n");
  {
    HdmvNavigationCommand * last = NULL;

    for (j = 0; j < nbNavCommands; j++) {
      HdmvNavigationCommand * com;

      /* Navigation_command() */
      com = getHdmvNavigationCommandHdmvSegmentsInventory(ctx->segInv);
      if (NULL == com)
        return -1;

      if (NULL != last)
        setNextHdmvNavigationCommand(last, com);
      else
        param->commands = com;

      /* [u32 opcode] */
      if (readValueFromHdmvICS(data, remainingDataLen, 4, &value) < 0)
        return -1;
      com->opCode = value;

      /* [u32 destination] */
      if (readValueFromHdmvICS(data, remainingDataLen, 4, &value) < 0)
        return -1;
      com->dst = value;

      /* [u32 source] */
      if (readValueFromHdmvICS(data, remainingDataLen, 4, &value) < 0)
        return -1;
      com->src = value;

      LIBBLU_HDMV_COM_DEBUG(
        "           Navigation_command"
        "(op=0x%08" PRIX32 ", dst=0x%08" PRIX32 ", src=0x%08" PRIX32 ");\n",
        com->opCode,
        com->dst,
        com->src
      );
      last = com;
    }
  }

  return 0;
}

int decodeHdmvButtonOverlapGroup(
  uint8_t ** data, size_t * remainingDataLen,
  HdmvSegmentsContextPtr ctx, HdmvButtonOverlapGroupParameters * param,
  const unsigned i
)
{
  uint32_t value;
  unsigned j;

  assert(NULL != data);
  assert(NULL != remainingDataLen);
  assert(NULL != ctx);
  assert(NULL != param);

  LIBBLU_HDMV_COM_DEBUG("       Button_overlap_group(%u):\n", i);

  /* [u16 default_valid_button_id_ref] */
  if (readValueFromHdmvICS(data, remainingDataLen, 2, &value) < 0)
    return -1;
  param->default_valid_button_id_ref = value & 0xFFFF;

  LIBBLU_HDMV_COM_DEBUG(
    "        default_valid_button_id_ref: %" PRIu16 " (0x%" PRIx16 ").\n",
    param->default_valid_button_id_ref,
    param->default_valid_button_id_ref
  );

  /* [u8 number_of_buttons] */
  if (readValueFromHdmvICS(data, remainingDataLen, 1, &value) < 0)
    return -1;
  param->number_of_buttons = value & 0xFF;

  LIBBLU_HDMV_COM_DEBUG(
    "        number_of_buttons: %u (0x%x).\n",
    param->number_of_buttons,
    param->number_of_buttons
  );

  if (0 < param->number_of_buttons) {
    /* Allocating buttons : */
    for (j = 0; j < param->number_of_buttons; j++) {
      param->buttons[j] = getHdmvButtonParamHdmvSegmentsInventory(ctx->segInv);
      if (NULL == param->buttons[j])
        return -1;
    }

    LIBBLU_HDMV_COM_DEBUG("        Buttons():\n");
    for (j = 0; j < param->number_of_buttons; j++) {
      /* Button() */
      if (decodeHdmvButton(data, remainingDataLen, ctx, param->buttons[j], j) < 0)
        return -1;
    }
  }

  return 0;
}

int decodeHdmvPage(
  uint8_t ** data, size_t * remainingDataLen,
  HdmvSegmentsContextPtr ctx,
  HdmvPageParameters * param,
  const unsigned i
)
{
  uint32_t value;
  unsigned j;

  assert(NULL != data);
  assert(NULL != remainingDataLen);
  assert(NULL != param);

  LIBBLU_HDMV_COM_DEBUG("   Page_info(%u):\n", i);

  /* [u8 page_id] */
  if (readValueFromHdmvICS(data, remainingDataLen, 1, &value) < 0)
    return -1;
  param->page_id = value & 0xFF;

  LIBBLU_HDMV_COM_DEBUG(
    "    page_id: %u (0x%x).\n",
    param->page_id,
    param->page_id
  );

  /* [u8 page_version_number] */
  if (readValueFromHdmvICS(data, remainingDataLen, 1, &value) < 0)
    return -1;
  param->page_version_number = value & 0xFF;

  LIBBLU_HDMV_COM_DEBUG(
    "    page_version_number: %u (0x%x).\n",
    param->page_version_number,
    param->page_version_number
  );

  /* [u64 UO_mask_table()] */
  if (readValueFromHdmvICS(data, remainingDataLen, 4, &value) < 0)
    return -1;
  param->UO_mask_table  = (uint64_t) value << 32;
  if (readValueFromHdmvICS(data, remainingDataLen, 4, &value) < 0)
    return -1;
  param->UO_mask_table |= (uint64_t) value;

  LIBBLU_HDMV_COM_DEBUG(
    "    UO_mask_table(): 0x%" PRIX64 ".\n",
    param->UO_mask_table
  );

  /* In_effects() */
  LIBBLU_HDMV_COM_DEBUG("    In_effects():\n");
  if (decodeHdmvEffectSequence(data, remainingDataLen, ctx, &param->in_effects) < 0)
    return -1;
  /* Out_effects() */
  LIBBLU_HDMV_COM_DEBUG("    Out_effects():\n");
  if (decodeHdmvEffectSequence(data, remainingDataLen, ctx, &param->out_effects) < 0)
    return -1;

  /* [u8 animation_frame_rate_code] */
  if (readValueFromHdmvICS(data, remainingDataLen, 1, &value) < 0)
    return -1;
  param->animation_frame_rate_code = value & 0xFF;

  LIBBLU_HDMV_COM_DEBUG(
    "    animation_frame_rate_code: %u (0x%x).\n",
    param->animation_frame_rate_code,
    param->animation_frame_rate_code
  );
  if (param->animation_frame_rate_code)
    LIBBLU_HDMV_COM_DEBUG("     -> Animation at 1/%u of frame-rate speed.\n", param->animation_frame_rate_code);
  else
    LIBBLU_HDMV_COM_DEBUG("     -> Only first picture of animation displayed.\n");

  /* [u16 default_selected_button_id_ref] */
  if (readValueFromHdmvICS(data, remainingDataLen, 2, &value) < 0)
    return -1;
  param->default_selected_button_id_ref = value & 0xFFFF;

  LIBBLU_HDMV_COM_DEBUG(
    "    default_selected_button_id_ref: %u (0x%x).\n",
    param->default_selected_button_id_ref,
    param->default_selected_button_id_ref
  );

  /* [u16 default_activated_button_id_ref] */
  if (readValueFromHdmvICS(data, remainingDataLen, 2, &value) < 0)
    return -1;
  param->default_activated_button_id_ref = value & 0xFFFF;

  LIBBLU_HDMV_COM_DEBUG(
    "    default_activated_button_id_ref: %u (0x%x).\n",
    param->default_activated_button_id_ref,
    param->default_activated_button_id_ref
  );

  /* [u8 palette_id_ref] */
  if (readValueFromHdmvICS(data, remainingDataLen, 1, &value) < 0)
    return -1;
  param->palette_id_ref = value & 0xFF;

  LIBBLU_HDMV_COM_DEBUG(
    "    palette_id_ref: %u (0x%x).\n",
    param->palette_id_ref,
    param->palette_id_ref
  );

  /* [u8 number_of_BOGs] */
  if (readValueFromHdmvICS(data, remainingDataLen, 1, &value) < 0)
    return -1;
  param->number_of_BOGs = value & 0xFF;

  LIBBLU_HDMV_COM_DEBUG(
    "    number_of_BOGs: %u BOGs (0x%x).\n",
    param->number_of_BOGs,
    param->number_of_BOGs
  );

  if (0 < param->number_of_BOGs) {
    /* Allocating BOGs : */
    for (j = 0; j < param->number_of_BOGs; j++) {
      param->bogs[j] =
        getHdmvButtonOverlapGroupParametersHdmvSegmentsInventory(
          ctx->segInv
        )
      ;
      if (NULL == param->bogs[j])
        return -1;
    }

    LIBBLU_HDMV_COM_DEBUG("      Button_overlap_groups():\n");
    for (j = 0; j < param->number_of_BOGs; j++) {
      /* Button_overlap_group() */
      if (decodeHdmvButtonOverlapGroup(data, remainingDataLen, ctx, param->bogs[j], j) < 0)
        return -1;
    }
  }

  return 0;
}

int decodeHdmvInteractiveComposition(
  uint8_t * data, size_t remainingDataLen,
  HdmvSegmentsContextPtr ctx, HdmvICParameters * param
)
{
  uint32_t value;
  unsigned i;

  LIBBLU_HDMV_COM_DEBUG(
    " Interactive_composition(): "
    "/* Recomposed from Interactive_composition_data_fragment() */\n"
  );

  /* [u24 interactive_composition_length] */
  if (readValueFromHdmvICS(&data, &remainingDataLen, 3, &value) < 0)
    return -1;
  param->interactive_composition_length = value & 0xFFFFFF;

  LIBBLU_HDMV_COM_DEBUG(
    "  interactive_composition_length: %u bytes (0x%x).\n",
    param->interactive_composition_length,
    param->interactive_composition_length
  );

  /* [b1 stream_model] [b1 user_interface_model] [v6 reserved] */
  if (readValueFromHdmvICS(&data, &remainingDataLen, 1, &value) < 0)
    return -1;
  param->stream_model        = (value >> 7) & 0x1;
  param->user_interface_model = (value >> 6) & 0x1;

  LIBBLU_HDMV_COM_DEBUG(
    "  stream_model: %s (0b%x).\n",
    (param->stream_model) ? "Multiplexed stream" : "Out of mux",
    param->stream_model
  );
  LIBBLU_HDMV_COM_DEBUG(
    "  user_interface_model: %s (0b%x).\n",
    (param->user_interface_model) ? "Normal menu" : "PoP-up menu",
    param->user_interface_model
  );
  LIBBLU_HDMV_COM_DEBUG(
    "  reserved: 0x%x.\n",
    value & 0x3F
  );

  if (param->stream_model == HDMV_STREAM_MODEL_OOM) {
    /* Out of Mux related parameters */
    LIBBLU_HDMV_COM_DEBUG("  OoM related parameters:\n");

    /* [v7 reserved] [u33 composition_time_out_pts] */
    if (readValueFromHdmvICS(&data, &remainingDataLen, 4, &value) < 0)
      return -1;
    param->oomRelatedParam.composition_time_out_pts =
      ((uint64_t) value & 0x1FFFFFF) << 8
    ;
    if (readValueFromHdmvICS(&data, &remainingDataLen, 1, &value) < 0)
      return -1;
    param->oomRelatedParam.composition_time_out_pts |=
      (uint64_t) value & 0xFF
    ;

    LIBBLU_HDMV_COM_DEBUG(
      "   composition_time_out_pts: %" PRIu64 " (0x%" PRIx64 ").\n",
      param->oomRelatedParam.composition_time_out_pts,
      param->oomRelatedParam.composition_time_out_pts
    );

    /* [v7 reserved] [u33 selection_time_out_pts] */
    if (readValueFromHdmvICS(&data, &remainingDataLen, 4, &value) < 0)
      return -1;
    param->oomRelatedParam.selection_time_out_pts =
      ((uint64_t) value & 0x1FFFFFF) << 8
    ;
    if (readValueFromHdmvICS(&data, &remainingDataLen, 1, &value) < 0)
      return -1;
    param->oomRelatedParam.selection_time_out_pts |=
      (uint64_t) value & 0xFF
    ;

    LIBBLU_HDMV_COM_DEBUG(
      "   selection_time_out_pts: %" PRIu64 " (0x%" PRIx64 ").\n",
      param->oomRelatedParam.selection_time_out_pts,
      param->oomRelatedParam.selection_time_out_pts
    );
  }

  /* [u24 user_time_out_duration] */
  if (readValueFromHdmvICS(&data, &remainingDataLen, 3, &value) < 0)
    return -1;
  param->user_time_out_duration =
    value & 0xFFFFFF
  ;

  LIBBLU_HDMV_COM_DEBUG(
    "  user_time_out_duration: %u (0x%x).\n",
    param->user_time_out_duration,
    param->user_time_out_duration
  );

  /* [u8 number_of_pages] */
  if (readValueFromHdmvICS(&data, &remainingDataLen, 1, &value) < 0)
    return -1;
  param->number_of_pages =
    value & 0xFF
  ;

  LIBBLU_HDMV_COM_DEBUG(
    "  number_of_pages: %u (0x%x).\n",
    param->number_of_pages,
    param->number_of_pages
  );

  assert(param->number_of_pages <= HDMV_MAX_NB_ICS_PAGES); /* Due to value size limitation, shall never happen. */

  if (0 < param->number_of_pages) {
    /* Allocating pages : */
    for (i = 0; i < param->number_of_pages; i++) {
      param->pages[i] = getHdmvPageParametersHdmvSegmentsInventory(
        ctx->segInv
      );
      if (NULL == param->pages[i])
        return -1;
    }

    LIBBLU_HDMV_COM_DEBUG("  Pages():\n");

    for (i = 0; i < param->number_of_pages; i++) {
      if (decodeHdmvPage(&data, &remainingDataLen, ctx, param->pages[i], i) < 0)
        return -1;
    }
  }

  return 0;
}

int decodeHdmvIcsSegment(
  BitstreamReaderPtr hdmvInput,
  HdmvSegmentsContextPtr ctx,
  HdmvSegmentParameters seg
)
{
  HdmvIcsSegmentParameters * param;
  HdmvSequencePtr seq;
  size_t dataFragmentLength;
  int64_t startOffset;
  int ret;

  assert(NULL != hdmvInput);
  assert(NULL != ctx);
  assert(HDMV_SEGMENT_TYPE_ICS == seg.type);

  param = &seg.data.igsParam;

  startOffset = tellPos(hdmvInput);

  /* Video_descriptor() */
  if (decodeHdmvVideoDescriptor(hdmvInput, &param->video_descriptor) < 0)
    return -1;

  if (!ctx->curNbIcsSegments)
    ctx->videoDesc = param->video_descriptor;

  /* Composition_descriptor() */
  if (decodeHdmvCompositionDescriptor(hdmvInput, &param->composition_descriptor) < 0)
    return -1;

  /* Sequence_descriptor() */
  if (decodeHdmvSequenceDescriptor(hdmvInput, &param->sequence_descriptor) < 0)
    return -1;

  /* Add segment to sequence */
  seq = addSegmentToSequence(
    ctx->segInv,
    &ctx->segRefs.ics,
    IGS_MAX_NB_SEG_ICS,
    seg,
    param->sequence_descriptor
  );
  if (NULL == seq)
    return -1;

  /* Interactive_composition_data_fragment() */
  dataFragmentLength = seg.length - (tellPos(hdmvInput) - startOffset);
  ret = readHdmvDataFragment(
    hdmvInput,
    dataFragmentLength,
    &seq->fragments,
    &seq->fragmentsAllocatedLen,
    &seq->fragmentsUsedLen
  );
  if (ret < 0)
    return -1;

  if (!ctx->curNbIcsSegments) {
    /* First ICS segment of epoch, defining reference parameters */
    ctx->videoDesc = param->video_descriptor;
  }

  if (isLastInSequenceHdmvSDParameters(&param->sequence_descriptor)) {
    /* Decode complete segment data. */

    /* Interactive_composition_data() */
    if (
      decodeHdmvInteractiveComposition(
        seq->fragments,
        seq->fragmentsUsedLen,
        ctx, &seq->data.interactiveComposition
      ) < 0
    )
      return -1;

    ctx->curNbIcsSegments++;
    ctx->nbIcsSegments++;
  }

  return 0;
}

int decodeHdmvPaletteDefinition(
  BitstreamReaderPtr hdmvInput, HdmvPDParameters * param
)
{
  uint32_t value;

  assert(NULL != hdmvInput);
  assert(NULL != param);

  LIBBLU_HDMV_COM_DEBUG(" Palette_definition():\n");

  /* [u8 palette_id] */
  if (readValueBigEndian(hdmvInput, 1, &value) < 0)
    return -1;
  param->palette_id = value & 0xFF;

  LIBBLU_HDMV_COM_DEBUG(
    "  palette_id: %" PRIu8 " (0x%" PRIx8 ").\n",
    param->palette_id,
    param->palette_id
  );

  /* [u8 palette_version_number] */
  if (readValueBigEndian(hdmvInput, 1, &value) < 0)
    return -1;
  param->palette_version_number = value & 0xFF;

  LIBBLU_HDMV_COM_DEBUG(
    "  palette_version_number: %" PRIu8 " (0x%" PRIx8 ").\n",
    param->palette_version_number,
    param->palette_version_number
  );

  return 0;
}

int decodeHdmvPaletteEntry(
  BitstreamReaderPtr hdmvInput,
  HdmvPaletteEntryParameters * param,
  const unsigned i
)
{
  uint32_t value;

  assert(NULL != hdmvInput);
  assert(NULL != param);

  LIBBLU_HDMV_COM_DEBUG("   Palette_entry(%u): ", i);

  /* [u8 palette_entry_id] */
  if (readValueBigEndian(hdmvInput, 1, &value) < 0)
    return -1;
  param->palette_entry_id = value & 0xFF;

  LIBBLU_HDMV_COM_DEBUG_NH(
    "palette_entry_id=%03u (0x%02X), ",
    param->palette_entry_id, param->palette_entry_id
  );

  /* [u8 Y_value] */
  if (readValueBigEndian(hdmvInput, 1, &value) < 0)
    return -1;
  param->y_value = value & 0xFF;

  LIBBLU_HDMV_COM_DEBUG_NH(
    "Y=%03u (0x%02X), ",
    param->y_value, param->y_value
  );

  /* [u8 Cr_value] */
  if (readValueBigEndian(hdmvInput, 1, &value) < 0)
    return -1;
  param->cr_value = value & 0xFF;

  LIBBLU_HDMV_COM_DEBUG_NH(
    "Cr=%03u (0x%02X), ",
    param->cr_value, param->cr_value
  );

  /* [u8 Cb_value] */
  if (readValueBigEndian(hdmvInput, 1, &value) < 0)
    return -1;
  param->cb_value = value & 0xFF;

  LIBBLU_HDMV_COM_DEBUG_NH(
    "Cb=%03u (0x%02X), ",
    param->cb_value, param->cb_value
  );

  /* [u8 T_value] */
  if (readValueBigEndian(hdmvInput, 1, &value) < 0)
    return -1;
  param->t_value = value & 0xFF;

  LIBBLU_HDMV_COM_DEBUG_NH(
    "T=%03u (0x%02X);\n",
    param->t_value, param->t_value
  );

  return 0;
}

int decodeHdmvPdsSegment(
  BitstreamReaderPtr hdmvInput,
  HdmvSegmentsContextPtr ctx,
  HdmvSegmentParameters seg
)
{
  HdmvPdsSegmentParameters * param;
  size_t paletteEntriesLength;
  int64_t startOffset;
  unsigned i;

  const HdmvSDParameters pdsSequenceParam = {
    .first_in_sequence = true,
    .last_in_sequence = true
  };

  assert(NULL != hdmvInput);
  assert(NULL != ctx);
  assert(seg.type == HDMV_SEGMENT_TYPE_PDS);

  param = &seg.data.pdsParam;

  startOffset = tellPos(hdmvInput);

  /* Palette_definition() */
  if (decodeHdmvPaletteDefinition(hdmvInput, &param->palette_definition) < 0)
    return -1;

  paletteEntriesLength = seg.length - (tellPos(hdmvInput) - startOffset);
  param->number_of_palette_entries = paletteEntriesLength / 5;

  if (0 != paletteEntriesLength % 5)
    LIBBLU_HDMV_COM_ERROR_RETURN(
      "Palette Definition Segment entries size "
      "is not a multiple of 5 (%zu bytes).\n",
      paletteEntriesLength
    );

  if (HDMV_MAX_NB_PDS_ENTRIES < param->number_of_palette_entries)
    LIBBLU_HDMV_COM_ERROR_RETURN(
      "Palette Definition Segment number of entries "
      "exceed " STR(HDMV_MAX_NB_PDS_ENTRIES) " (%u for %zu bytes).\n",
      param->number_of_palette_entries,
      paletteEntriesLength
    );

  if (0 < param->number_of_palette_entries) {
    /* Allocating palette palette_entries : */
    for (i = 0; i < param->number_of_palette_entries; i++) {
      param->palette_entries[i] =
        getHdmvPaletteEntryParametersHdmvSegmentsInventory(
          ctx->segInv
        )
      ;
      if (NULL == param->palette_entries[i])
        return -1;
    }

    LIBBLU_HDMV_COM_DEBUG("  Palette_entries():\n");

    for (i = 0; i < param->number_of_palette_entries; i++) {
      if (decodeHdmvPaletteEntry(hdmvInput, param->palette_entries[i], i) < 0)
        return -1;
    }
  }

  /* Add complete sequence to array. */
  if (
    NULL == addSegmentToSequence(
      ctx->segInv,
      &ctx->segRefs.pds,
      isIgsHdmvSegmentsContext(ctx) ? IGS_MAX_NB_SEG_PDS : PGS_MAX_NB_SEG_PDS,
      seg,
      pdsSequenceParam
    )
  )
    return -1;
  ctx->curNbPdsSegments++;
  ctx->nbPdsSegments++;

  return 0;
}

int decodeHdmvObjectData(
  uint8_t * data, size_t remainingDataLen, HdmvODataParameters * param
)
{
  uint32_t value;

  assert(NULL != data);

  LIBBLU_HDMV_COM_DEBUG(
    " Object_data(): /* Recomposed from object_data_fragment() */\n"
  );

  /* [u24 object_data_length] */
  if (readValueFromHdmvICS(&data, &remainingDataLen, 3, &value) < 0)
    return -1;
  param->object_data_length =
    value & 0xFFFFFF
  ;

  LIBBLU_HDMV_COM_DEBUG(
    "  object_data_length: %u bytes (0x%x).\n",
    param->object_data_length,
    param->object_data_length
  );

  /* [u16 object_width] */
  if (readValueFromHdmvICS(&data, &remainingDataLen, 2, &value) < 0)
    return -1;
  param->object_width = value & 0xFFFF;

  LIBBLU_HDMV_COM_DEBUG(
    "  object_width: %u px (0x%x).\n",
    param->object_width,
    param->object_width
  );

  /* [u16 object_height] */
  if (readValueFromHdmvICS(&data, &remainingDataLen, 2, &value) < 0)
    return -1;
  param->object_height = value & 0xFFFF;

  LIBBLU_HDMV_COM_DEBUG(
    "  object_height: %u px (0x%x).\n",
    param->object_height,
    param->object_height
  );

  return 0;
}

int decodeHdmvObjectDescriptor(
  BitstreamReaderPtr hdmvInput, HdmvODParameters * param
)
{
  uint32_t value;

  assert(NULL != hdmvInput);
  assert(NULL != param);

  LIBBLU_HDMV_COM_DEBUG(" Object_descriptor():\n");

  /* [u16 object_id] */
  if (readValueBigEndian(hdmvInput, 2, &value) < 0)
    return -1;
  param->object_id = value & 0xFFFF;

  LIBBLU_HDMV_COM_DEBUG(
    "  object_id: %" PRIu8 " (0x%" PRIx8 ").\n",
    param->object_id, param->object_id
  );

  /* [u8 object_version_number] */
  if (readValueBigEndian(hdmvInput, 1, &value) < 0)
    return -1;
  param->object_version_number = value & 0xFF;

  LIBBLU_HDMV_COM_DEBUG(
    "  object_version_number: %" PRIu8 " (0x%" PRIx8 ").\n",
    param->object_version_number, param->object_version_number
  );

  return 0;
}

int decodeHdmvOdsSegment(
  BitstreamReaderPtr hdmvInput,
  HdmvSegmentsContextPtr ctx,
  HdmvSegmentParameters seg
)
{
  HdmvOdsSegmentParameters * param;
  HdmvSequencePtr seq;
  size_t dataFragmentLength;
  int64_t startOffset;

  assert(NULL != hdmvInput);
  assert(NULL != ctx);
  assert(seg.type == HDMV_SEGMENT_TYPE_ODS);

  startOffset = tellPos(hdmvInput);

  param = &seg.data.odsParam;

  /* Object_descriptor() */
  if (decodeHdmvObjectDescriptor(hdmvInput, &param->object_descriptor) < 0)
    return -1;

  /* Sequence_descriptor() */
  if (decodeHdmvSequenceDescriptor(hdmvInput, &param->sequence_descriptor) < 0)
    return -1;

  /* Add segment to sequence */
  seq = addSegmentToSequence(
    ctx->segInv,
    &ctx->segRefs.ods,
    isIgsHdmvSegmentsContext(ctx) ? IGS_MAX_NB_SEG_ODS : PGS_MAX_NB_SEG_ODS,
    seg,
    param->sequence_descriptor
  );
  if (NULL == seq)
    return -1;

  /* Object_data_fragment() */
  dataFragmentLength = seg.length - (tellPos(hdmvInput) - startOffset);
  if (
    readHdmvDataFragment(
      hdmvInput, dataFragmentLength,
      &seq->fragments,
      &seq->fragmentsAllocatedLen,
      &seq->fragmentsUsedLen
    ) < 0
  )
    return -1;

  if (isLastInSequenceHdmvSDParameters(&param->sequence_descriptor)) {
    /* Decode complete segment data. */

    /* Interactive_composition_data() */
    if (
      decodeHdmvObjectData(
        seq->fragments,
        seq->fragmentsUsedLen,
        &seq->data.objectData
      ) < 0
    )
      return -1;

    ctx->curNbOdsSegments++;
    ctx->nbOdsSegments++;
  }

  return 0;
}

int decodeHdmvEndSegment(
  HdmvSegmentsContextPtr ctx,
  HdmvSegmentParameters seg
)
{
  HdmvSequencePtr seq;

  const HdmvSDParameters endSequenceParam = {
    .first_in_sequence = true,
    .last_in_sequence = true
  };

  assert(NULL != ctx);
  assert(seg.type == HDMV_SEGMENT_TYPE_END);

  /* Add segment to sequence */
  seq = addSegmentToSequence(
    ctx->segInv,
    &ctx->segRefs.end,
    IGS_MAX_NB_SEG_END,
    seg,
    endSequenceParam
  );
  if (NULL == seq)
    return -1;

  ctx->curNbEndSegments++;
  ctx->nbEndSegments++;
  return 0;
}

int parseHdmvSegmentDescriptor(
  BitstreamReaderPtr hdmvInput,
  HdmvSegmentParameters * param
)
{
  uint32_t value;

  assert(NULL != hdmvInput);
  assert(NULL != param);

  param->inputFileOffset = tellPos(hdmvInput);

  /* [u8 segment_type] */
  if (readValueBigEndian(hdmvInput, 1, &value) < 0)
    return -1;

  LIBBLU_HDMV_COM_DEBUG("0x%08" PRIX64 ": %s.\n", param->inputFileOffset, HdmvSegmentTypeStr(value));
  LIBBLU_HDMV_COM_DEBUG(" Segment_descriptor():\n");
  LIBBLU_HDMV_COM_DEBUG("  segment_type: %s (0x%" PRIx8 ").\n", HdmvSegmentTypeStr(value), value);

  switch (value) {
    case HDMV_SEGMENT_TYPE_PDS:
    case HDMV_SEGMENT_TYPE_ODS:
    case HDMV_SEGMENT_TYPE_ICS:
    case HDMV_SEGMENT_TYPE_PCS:
    case HDMV_SEGMENT_TYPE_WDS:
    case HDMV_SEGMENT_TYPE_END:
      param->type = value;
      break;

    default:
      LIBBLU_HDMV_COM_ERROR_RETURN(
        "Unknown segment_type value (0x%08" PRIX8 ").\n",
        value
      );
  }

  /* [u16 segment_length] */
  if (readValueBigEndian(hdmvInput, 2, &value) < 0)
    return -1;
  param->length = value;

  LIBBLU_HDMV_COM_DEBUG(
    "  segment_length: %" PRIu16 " bytes (0x%" PRIx16 ").\n",
    param->length,
    param->length
  );

  return 0;
}