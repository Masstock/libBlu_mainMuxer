/** \~english
 * \file hdmv_common.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief HDMV bitstreams (IGS, PGS) common handling module.
 */

#ifndef __LIBBLU_MUXER__CODECS__HDMV__COMMON_H__
#define __LIBBLU_MUXER__CODECS__HDMV__COMMON_H__

#include "hdmv_error.h"
#include "hdmv_data.h"
#include "../../../util.h"
#include "../../../esms/scriptCreation.h"

#define ALLOC_SEQ_INCREMENT           32
#define ALLOC_SEG_INCREMENT           10
#define ALLOC_PAGE_INCREMENT          16
#define ALLOC_EFFECT_WINDOW_INCREMENT  4
#define ALLOC_EFFECT_INCREMENT         4
#define ALLOC_COMP_OBJ_INCREMENT       8
#define ALLOC_BOG_INCREMENT            8
#define ALLOC_BUTTON_INCREMENT        32
#define ALLOC_COMMAND_INCREMENT      128
#define ALLOC_MEM_BLK_IDX_INCREMENT    2
#define ALLOC_PAL_ENTRY_INCREMENT    128

/* ### Segments types indexes : ############################################ */

#define HDMV_NB_SEGMENT_TYPES  6

typedef int hdmv_segtype_idx;

static const HdmvSegmentType hdmv_segtype_indexes[] = {
  HDMV_SEGMENT_TYPE_ICS,
  HDMV_SEGMENT_TYPE_PCS,
  HDMV_SEGMENT_TYPE_WDS,
  HDMV_SEGMENT_TYPE_PDS,
  HDMV_SEGMENT_TYPE_ODS,
  HDMV_SEGMENT_TYPE_END
};

typedef enum {
  HDMV_SEGMENT_TYPE_ICS_IDX,
  HDMV_SEGMENT_TYPE_PCS_IDX,
  HDMV_SEGMENT_TYPE_WDS_IDX,
  HDMV_SEGMENT_TYPE_PDS_IDX,
  HDMV_SEGMENT_TYPE_ODS_IDX,
  HDMV_SEGMENT_TYPE_END_IDX
} hdmv_segtype_idx_value;

static inline bool isValidSegmentTypeIndexHdmvContext(
  hdmv_segtype_idx idx
)
{
  return 0 <= idx && idx < HDMV_NB_SEGMENT_TYPES;
}

static inline hdmv_segtype_idx segmentTypeIndexHdmvContext(
  HdmvSegmentType type
)
{
  unsigned i;

  assert(HDMV_NB_SEGMENT_TYPES == ARRAY_SIZE(hdmv_segtype_indexes));

  for (i = 0; i < ARRAY_SIZE(hdmv_segtype_indexes); i++) {
    if (hdmv_segtype_indexes[i] == type)
      return (int) i;
  }

  LIBBLU_HDMV_COM_ERROR_RETURN("Unexpected segment type 0x%02X.\n", type);
}

#define SEGMENT_TYPE_IDX_STR_SIZE  4

static inline const char * segmentTypeIndexStr(
  hdmv_segtype_idx idx
)
{
  static const char strings[][SEGMENT_TYPE_IDX_STR_SIZE] = {
    "ICS",
    "PCS",
    "WDS",
    "PDS",
    "ODS",
    "END"
  }; /**< Sizes are restricted to SEGMENT_TYPE_IDX_STR_SIZE characters
    (including NUL character) */

  assert(0 <= idx && idx < (int) ARRAY_SIZE(strings));

  return strings[idx];
}

/* ###  */

typedef struct {
  unsigned types[HDMV_NB_SEGMENT_TYPES];
  unsigned total;
} HdmvContextSegmentTypesCounter;

static inline void incByIdxHdmvContextSegmentTypesCounter(
  HdmvContextSegmentTypesCounter * counter,
  hdmv_segtype_idx idx
)
{
  assert(isValidSegmentTypeIndexHdmvContext(idx));

  counter->types[idx]++;
  counter->total++;
}

static inline int incHdmvContextSegmentTypesCounter(
  HdmvContextSegmentTypesCounter * counter,
  HdmvSegmentType type
)
{
  hdmv_segtype_idx idx;

  if ((idx = segmentTypeIndexHdmvContext(type)) < 0)
    return -1;

  incByIdxHdmvContextSegmentTypesCounter(counter, idx);
  return 0;
}

static inline int addByIdxHdmvContextSegmentTypesCounter(
  HdmvContextSegmentTypesCounter * counter,
  hdmv_segtype_idx idx,
  unsigned nb
)
{
  if (lb_uadd_overflow(counter->types[idx], nb))
    LIBBLU_HDMV_COM_ERROR_RETURN(
      "%u segments number overflow.\n",
      idx
    );
  if (lb_uadd_overflow(counter->total, nb))
    LIBBLU_HDMV_COM_ERROR_RETURN(
      "Segments number overflow.\n"
    );

  counter->types[idx] += nb;
  counter->total += nb;

  return 0;
}

static inline int addHdmvContextSegmentTypesCounter(
  HdmvContextSegmentTypesCounter * counter,
  HdmvSegmentType type,
  unsigned nb
)
{
  hdmv_segtype_idx idx;

  if ((idx = segmentTypeIndexHdmvContext(type)) < 0)
    return -1;
  return addByIdxHdmvContextSegmentTypesCounter(counter, idx, nb);
}

static inline unsigned getTotalHdmvContextSegmentTypesCounter(
  HdmvContextSegmentTypesCounter counter
)
{
  return counter.total;
}

static inline unsigned getByIdxHdmvContextSegmentTypesCounter(
  const HdmvContextSegmentTypesCounter counter,
  hdmv_segtype_idx idx
)
{
  assert(isValidSegmentTypeIndexHdmvContext(idx));

  return counter.types[idx];
}

static inline void resetHdmvContextSegmentTypesCounter(
  HdmvContextSegmentTypesCounter * counter
)
{
  *counter = (HdmvContextSegmentTypesCounter) {
    0
  };
}

static inline void printContentHdmvContextSegmentTypesCounter(
  const HdmvContextSegmentTypesCounter cnt,
  HdmvStreamType type
)
{
#define _G(_i)  getByIdxHdmvContextSegmentTypesCounter(cnt, _i)
#define _P(_n)  lbc_printf(" - "#_n": %u.\n", _G(HDMV_SEGMENT_TYPE_##_n##_IDX))

  if (HDMV_STREAM_TYPE_IGS == type) {
    _P(ICS);
  }
  else { /* HDMV_STREAM_TYPE_PGS == type */
    _P(PCS);
    _P(WDS);
  }

  _P(PDS);
  _P(ODS);
  _P(END);

#undef _P
#undef _G

  lbc_printf(
    " TOTAL: %u.\n",
    getTotalHdmvContextSegmentTypesCounter(cnt)
  );
}

/* ### HDMV Segment : ###################################################### */

typedef struct HdmvSegment {
  struct HdmvSegment * nextSegment;
  HdmvSegmentParameters param;
} HdmvSegment, *HdmvSegmentPtr;

static inline void initHdmvSegment(
  HdmvSegment * dst
)
{
  *dst = (HdmvSegment) {
    0
  };
}

/* ### HDMV Sequence : ##################################################### */

typedef struct HdmvSequence {
  struct HdmvSequence * nextSequence;
  unsigned displaySetIdx;  /**< Associated DS number from which this
    sequence is from. */

  HdmvSegmentType type;
  HdmvSegmentPtr segments;
  HdmvSegmentPtr lastSegment;

  uint8_t * fragments;
  size_t fragmentsAllocatedLen;
  size_t fragmentsUsedLen;
  size_t fragmentsParsedOff;

  union {
    HdmvPdsSegmentParameters pd;  /**< Palette Definition.                   */
    HdmvODParameters         od;  /**< Object Definition.                    */
    HdmvPcsSegmentParameters pc;  /**< Presentation Composition.             */
    HdmvWdsSegmentParameters wd;  /**< Window Definition.                    */
    HdmvICParameters         ic;  /**< Interactive Composition.              */
  } data;

  uint64_t pts;
  uint64_t dts;
} HdmvSequence, *HdmvSequencePtr;

static inline void resetHdmvSequence(
  HdmvSequencePtr dst
)
{
  *dst = (HdmvSequence) {
    .fragments = dst->fragments,
    .fragmentsAllocatedLen = dst->fragmentsAllocatedLen
  };
}

static inline void cleanHdmvSequence(
  HdmvSequence seq
)
{
  free(seq.fragments);
}

static inline bool isFromDisplaySetNbHdmvSequence(
  const HdmvSequencePtr sequence,
  unsigned displaySetNumber
)
{
  return sequence->displaySetIdx == displaySetNumber;
}

static inline bool isDuplicateHdmvSequence(
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
);

static inline int readValueFromHdmvSequence(
  HdmvSequencePtr sequence,
  size_t size,
  uint32_t * value
)
{
  assert(NULL != sequence);
  assert(NULL != sequence->fragments);
  assert(size <= 4);
  assert(NULL != value);

  *value = 0;
  while (size--) {
    uint32_t byte;

    if (sequence->fragmentsUsedLen <= sequence->fragmentsParsedOff)
      LIBBLU_HDMV_COM_ERROR_RETURN("Prematurate end of sequence data.\n");

    byte = sequence->fragments[sequence->fragmentsParsedOff++];
    *value |= byte << (8 * size);
  }

  return 0;
}

/* ### HDMV Display Set : ################################################## */

typedef enum {
  HDMV_DS_NON_INITIALIZED,
  HDMV_DS_COMPLETED,
  HDMV_DS_INITIALIZED
} HdmvDisplaySetUsageState;

typedef struct {
  HdmvCDParameters composition_descriptor;
  HdmvDisplaySetUsageState initUsage;
  // bool duplicatedDS;

  HdmvSequencePtr sequences[HDMV_NB_SEGMENT_TYPES];      /**< Linked lists of
    each type of sequence. */
  HdmvSequencePtr lastSequences[HDMV_NB_SEGMENT_TYPES];  /**< Direct pointer
    to the last sequence node of each sequence type linked list. */
  HdmvSequencePtr pendingSequences[HDMV_NB_SEGMENT_TYPES];
  HdmvContextSegmentTypesCounter nbSequences;  /**< Number of sequences in DS. */
} HdmvDisplaySet;

static inline void initHdmvDisplaySet(
  HdmvDisplaySet * dst
)
{
  *dst = (HdmvDisplaySet) {0};
  resetHdmvContextSegmentTypesCounter(&dst->nbSequences);
}

static inline void incNbSequencesHdmvDisplaySet(
  HdmvDisplaySet * ds,
  hdmv_segtype_idx idx
)
{
  incByIdxHdmvContextSegmentTypesCounter(&ds->nbSequences, idx);
}

static inline HdmvSequencePtr getSequenceByIdxHdmvDisplaySet(
  const HdmvDisplaySet * ds,
  hdmv_segtype_idx idx
)
{
  return ds->sequences[idx];
}

static inline unsigned getNbSequencesHdmvDisplaySet(
  const HdmvDisplaySet ds
)
{
  return getTotalHdmvContextSegmentTypesCounter(ds.nbSequences);
}

/* ### HDMV Segments Inventory Pool : ###################################### */

#define HDMV_SEG_INV_POOL_DEFAULT_SEG_NB  4
#define HDMV_SEG_INV_POOL_DEFAULT_SEG_SIZE  8

typedef struct {
  void ** elements;
  size_t elementSize;
  size_t allocatedElementsSegments;
  size_t usedElementsSegments;
  size_t remainingElements;
} HdmvSegmentsInventoryPool;

static inline void initHdmvSegmentsInventoryPool(
  HdmvSegmentsInventoryPool * dst,
  size_t elementSize
)
{
  *dst = (HdmvSegmentsInventoryPool) {
    .elementSize = elementSize
  };
}

static inline void cleanHdmvSegmentsInventoryPool(
  HdmvSegmentsInventoryPool pool
)
{
  size_t i;

  for (i = 0; i < pool.allocatedElementsSegments; i++)
    free(pool.elements[i]);
  free(pool.elements);
}

static inline void resetHdmvSegmentsInventoryPool(
  HdmvSegmentsInventoryPool * pool
)
{
  pool->usedElementsSegments = 0;
  pool->remainingElements = 0;
}

/* ### HDMV Segments Inventory : ########################################### */

typedef struct {
  HdmvSegmentsInventoryPool sequences;
  HdmvSegmentsInventoryPool segments;
  HdmvSegmentsInventoryPool interComposPages;
  HdmvSegmentsInventoryPool effectsWindows;
  HdmvSegmentsInventoryPool effects;
  HdmvSegmentsInventoryPool composition_objects;
  HdmvSegmentsInventoryPool bogs;
  HdmvSegmentsInventoryPool buttons;
  HdmvSegmentsInventoryPool commands;
  HdmvSegmentsInventoryPool paletteEntries;
} HdmvSegmentsInventory, *HdmvSegmentsInventoryPtr;

HdmvSegmentsInventoryPtr createHdmvSegmentsInventory(
  void
);

#define CLEAN_POOL_ELEMENTS(p, t, c)                                          \
  do {                                                                        \
    size_t _i, _seg_sz, _j;                                                   \
                                                                              \
    for (_i = 0; _i < (p).allocatedElementsSegments; _i++) {                  \
      if (NULL != (p).elements[_i]) {                                         \
        _seg_sz = HDMV_SEG_INV_POOL_DEFAULT_SEG_SIZE << _i;                   \
                                                                              \
        for (_j = 0; _j < _seg_sz; _j++)                                      \
          c(((t *) ((p).elements[_i]))[_j]);                                  \
      }                                                                       \
    }                                                                         \
  } while (0)

static inline void destroyHdmvSegmentsInventory(
  HdmvSegmentsInventoryPtr inv
)
{
  if (NULL == inv)
    return;

  CLEAN_POOL_ELEMENTS(inv->sequences, HdmvSequence, cleanHdmvSequence);

#define CLEAN_POOL(p)  cleanHdmvSegmentsInventoryPool(p)
  CLEAN_POOL(inv->sequences);
  CLEAN_POOL(inv->segments);
  CLEAN_POOL(inv->interComposPages);
  CLEAN_POOL(inv->effectsWindows);
  CLEAN_POOL(inv->effects);
  CLEAN_POOL(inv->composition_objects);
  CLEAN_POOL(inv->bogs);
  CLEAN_POOL(inv->buttons);
  CLEAN_POOL(inv->commands);
  CLEAN_POOL(inv->paletteEntries);
#undef CLEAN_POOL

  free(inv);
}

static inline void resetHdmvSegmentsInventory(
  HdmvSegmentsInventoryPtr inv
)
{
#define RESET_POOL(d)  resetHdmvSegmentsInventoryPool(d)
  RESET_POOL(&inv->sequences);
  RESET_POOL(&inv->segments);
  RESET_POOL(&inv->interComposPages);
  RESET_POOL(&inv->effectsWindows);
  RESET_POOL(&inv->effects);
  RESET_POOL(&inv->composition_objects);
  RESET_POOL(&inv->bogs);
  RESET_POOL(&inv->buttons);
  RESET_POOL(&inv->commands);
  RESET_POOL(&inv->paletteEntries);
#undef RESET_POOL
}

/* ###### Inventory content fetching functions : ########################### */

HdmvSequencePtr getHdmvSequenceHdmvSegmentsInventory(
  HdmvSegmentsInventoryPtr inv
);

HdmvSegmentPtr getHdmvSegmentHdmvSegmentsInventory(
  HdmvSegmentsInventoryPtr inv
);

HdmvPageParameters * getHdmvPageParametersHdmvSegmentsInventory(
  HdmvSegmentsInventoryPtr inv
);

HdmvWindowInfoParameters * getHdmvWindowInfoParametersHdmvSegmentsInventory(
  HdmvSegmentsInventoryPtr inv
);

HdmvEffectInfoParameters * getHdmvEffectInfoParametersHdmvSegmentsInventory(
  HdmvSegmentsInventoryPtr inv
);

HdmvCompositionObjectParameters * getHdmvCompositionObjectParametersHdmvSegmentsInventory(
  HdmvSegmentsInventoryPtr inv
);

HdmvButtonOverlapGroupParameters * getHdmvButtonOverlapGroupParametersHdmvSegmentsInventory(
  HdmvSegmentsInventoryPtr inv
);

HdmvButtonParam * getHdmvButtonParamHdmvSegmentsInventory(
  HdmvSegmentsInventoryPtr inv
);

HdmvNavigationCommand * getHdmvNavigationCommandHdmvSegmentsInventory(
  HdmvSegmentsInventoryPtr inv
);

HdmvPaletteEntryParameters * getHdmvPaletteEntryParametersHdmvSegmentsInventory(
  HdmvSegmentsInventoryPtr inv
);

/* ######################################################################### */

#if 0

typedef struct {
  HdmvSDParameters sequence_descriptor;

  uint8_t * data;
  size_t dataLength;
} HdmvSegmentFragment;

typedef struct {
  /* Input parameters: */
  HdmvStreamType streamType;
  bool rawStreamInputMode;
  bool forceRetiming;

  struct {
    /* Used if !rawStreamInputMode: */
    int64_t pts;
    int64_t dts;
  } curSegProperties;
  int64_t referenceStartClock;
  uint64_t lastClock;

  /* Segments storage: */
  HdmvSegmentsInventoryPtr segInv;

  struct {
    HdmvSequencePtr pcs; /* Presentation Composition Segment */
    HdmvSequencePtr wds; /* Window Definition Segment */
    HdmvSequencePtr ics; /* Interactive Composition Segment */
    HdmvSequencePtr pds; /* Palette Definition Segments */
    HdmvSequencePtr ods; /* Object Definition Segments */
    HdmvSequencePtr end; /* End Segment */
  } segRefs;

  HdmvVDParameters videoDesc; /* Only defined if 0 < curNbIcsSegments */
  HdmvCompositionState compoState; /* Only defined if 0 < curNbIcsSegments */

  unsigned curNbPcsSegments;
  unsigned curNbWdsSegments;
  unsigned curNbIcsSegments;
  unsigned curNbPdsSegments;
  unsigned curNbOdsSegments;
  unsigned curNbEndSegments;
  unsigned curEpochNbSegments;

  /* Output: */
  BitstreamWriterPtr outputScriptFile;
  EsmsFileHeaderPtr esmsHeader;
  unsigned srcFileIdx;

  /* Statistics: */
  unsigned nbPcsSegments;
  unsigned nbWdsSegments;
  unsigned nbIcsSegments;
  unsigned nbPdsSegments;
  unsigned nbOdsSegments;
  unsigned nbEndSegments;
  unsigned totalNbSegments;
} HdmvSegmentsContext, *HdmvSegmentsContextPtr;

HdmvSegmentsContextPtr createHdmvSegmentsContext(
  HdmvStreamType streamType,
  BitstreamWriterPtr hdmvOutputScriptFile,
  EsmsFileHeaderPtr hdmvEsmsHeader,
  unsigned hdmvEsmsSrcFileIdx
);

void resetHdmvContext(
  HdmvSegmentsContextPtr ctx
);

void destroyHdmvContext(
  HdmvSegmentsContextPtr ctx
);

static inline bool isIgsHdmvSegmentsContext(
  const HdmvSegmentsContextPtr ctx
)
{
  return HDMV_STREAM_TYPE_IGS == ctx->streamType;
}

HdmvSequencePtr addSegmentToSequence(
  HdmvSegmentsInventoryPtr inv,
  HdmvSequencePtr * list,
  size_t maxNumberOfSequences,
  const HdmvSegmentParameters param,
  const HdmvSDParameters segSeqDesc
);

int insertHdmvSegment(
  HdmvSegmentsContextPtr ctx,
  HdmvSegmentParameters seg,
  uint64_t pts,
  uint64_t dts
);

int decodeHdmvVideoDescriptor(
  BitstreamReaderPtr hdmvInput,
  HdmvVDParameters * param
);

int decodeHdmvCompositionDescriptor(
  BitstreamReaderPtr hdmvInput,
  HdmvCDParameters * param
);

int decodeHdmvSequenceDescriptor(
  BitstreamReaderPtr hdmvInput,
  HdmvSDParameters * param
);

int readHdmvDataFragment(
  BitstreamReaderPtr hdmvInput,
  size_t length,
  uint8_t ** fragment,
  size_t * fragmentAllocatedLen,
  size_t * fragmentUsedLen
);

int decodeHdmvCompositionObject(
  BitstreamReaderPtr hdmvInput,
  HdmvCompositionObjectParameters * param
);

int decodeHdmvPresentationComposition(
  BitstreamReaderPtr hdmvInput,
  HdmvSegmentsContextPtr ctx,
  HdmvPCParameters * param
);

int decodeHdmvPcsSegment(
  BitstreamReaderPtr hdmvInput,
  HdmvSegmentsContextPtr ctx,
  HdmvSegmentParameters seg
);

int decodeHdmvWdsSegment(
  BitstreamReaderPtr hdmvInput,
  HdmvSegmentsContextPtr ctx,
  HdmvSegmentParameters seg
);

int decodeHdmvWindowsDefinition(
  BitstreamReaderPtr hdmvInput,
  HdmvSegmentsContextPtr ctx,
  HdmvWDParameters * param
);

int readValueFromHdmvICS(
  uint8_t ** data,
  size_t * remainingData,
  size_t length,
  uint32_t * v
);

int decodeHdmvCompositionObjectFromData(
  uint8_t ** data,
  size_t * remainingDataLen,
  HdmvCompositionObjectParameters * param,
  const unsigned i
);

int decodeHdmvEffectInfoFromData(
  uint8_t ** data,
  size_t * remainingDataLen,
  HdmvSegmentsContextPtr ctx,
  HdmvEffectInfoParameters * param,
  const unsigned i
);

int decodeHdmvWindowInfoFromData(
  uint8_t ** data,
  size_t * remainingDataLen,
  HdmvWindowInfoParameters * param,
  const unsigned i
);

int decodeHdmvEffectSequence(
  uint8_t ** data,
  size_t * remainingDataLen,
  HdmvSegmentsContextPtr ctx,
  HdmvEffectSequenceParameters * param
);

int decodeHdmvButton(
  uint8_t ** data,
  size_t * remainingDataLen,
  HdmvSegmentsContextPtr ctx,
  HdmvButtonParam * param,
  const unsigned i
);

int decodeHdmvButtonOverlapGroup(
  uint8_t ** data,
  size_t * remainingDataLen,
  HdmvSegmentsContextPtr ctx,
  HdmvButtonOverlapGroupParameters * param,
  const unsigned i
);

int decodeHdmvPage(
  uint8_t ** data,
  size_t * remainingDataLen,
  HdmvSegmentsContextPtr ctx,
  HdmvPageParameters * param,
  const unsigned i
);

int decodeHdmvInteractiveComposition(
  uint8_t * data,
  size_t remainingDataLen,
  HdmvSegmentsContextPtr ctx,
  HdmvICParameters * param
);

int decodeHdmvIcsSegment(
  BitstreamReaderPtr hdmvInput,
  HdmvSegmentsContextPtr ctx,
  HdmvSegmentParameters seg
);

int decodeHdmvPaletteDefinition(
  BitstreamReaderPtr hdmvInput,
  HdmvPDParameters * param
);

int decodeHdmvPaletteEntry(
  BitstreamReaderPtr hdmvInput,
  HdmvPaletteEntryParameters * param,
  const unsigned i
);

int decodeHdmvPdsSegment(
  BitstreamReaderPtr hdmvInput,
  HdmvSegmentsContextPtr ctx,
  HdmvSegmentParameters seg
);

int decodeHdmvObjectData(
  uint8_t * data,
  size_t remainingDataLen,
  HdmvODParameters * param
);

int decodeHdmvObjectDescriptor(
  BitstreamReaderPtr hdmvInput,
  HdmvODescParameters * param
);

int decodeHdmvOdsSegment(
  BitstreamReaderPtr hdmvInput,
  HdmvSegmentsContextPtr ctx,
  HdmvSegmentParameters seg
);

int decodeHdmvEndSegment(
  HdmvSegmentsContextPtr ctx,
  HdmvSegmentParameters seg
);

int parseHdmvSegmentDescriptor(
  BitstreamReaderPtr hdmvInput,
  HdmvSegmentParameters * param
);

#endif

#endif