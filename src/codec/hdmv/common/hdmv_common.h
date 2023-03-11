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

typedef struct {
  bool orderIgsSegByValue;
  bool orderPgsSegByValue;
} HdmvParsingOptions;

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
  struct HdmvSequence * nextSequenceDS;
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
    HdmvPCParameters         pc;  /**< Presentation Composition.             */
    HdmvWDParameters         wd;  /**< Window Definition.                    */
    HdmvICParameters         ic;  /**< Interactive Composition.              */
  } data;

  int64_t pts;  /**< Presentation Time Stamp, in 90kHz clock ticks. */
  int64_t dts;  /**< Decoding Time Stamp, in 90kHz clock ticks. */
  int32_t decodeDuration; /**< For Object Definition sequences, the duration
    required to decode graphical objects data. For Window Definition
    sequences, the required minimal amount of time to draw graphics.         */
  int32_t transferDuration;
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

static inline bool isUniqueInDisplaySetHdmvSequence(
  const HdmvSequencePtr sequence
)
{
  return NULL != sequence && NULL == sequence->nextSequenceDS;
}

static inline bool isFromIdxDisplaySetHdmvSequence(
  const HdmvSequencePtr sequence,
  unsigned displaySetIdx
)
{
  return sequence->displaySetIdx == displaySetIdx;
}

bool isDuplicateHdmvSequence(
  const HdmvSequencePtr first,
  const HdmvSequencePtr second
);

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

/** \~english
 * \brief Display Set memory content.
 *
 */
typedef struct {
  HdmvCDParameters composition_descriptor;  /**< Current Display Set composition_descriptor. */
  HdmvVDParameters video_descriptor;
  HdmvDisplaySetUsageState initUsage;

  struct {
    HdmvSequencePtr head;       /**< Sequences linked list. */
    HdmvSequencePtr last;       /**< Pointer to the last sequence in the linked list.  */
    HdmvSequencePtr pending;    /**< Pending decoded sequence. */
    HdmvSequencePtr ds;         /**< Sequences present in pending display set. */
  } sequences[HDMV_NB_SEGMENT_TYPES];

  HdmvContextSegmentTypesCounter nbSequences;  /**< Number of sequences in the pending DS. */
} HdmvEpochState;

static inline void initHdmvEpochState(
  HdmvEpochState * dst
)
{
  *dst = (HdmvEpochState) {0};
  resetHdmvContextSegmentTypesCounter(&dst->nbSequences);
}

static inline void incNbSequencesHdmvEpochState(
  HdmvEpochState * epoch,
  hdmv_segtype_idx idx
)
{
  incByIdxHdmvContextSegmentTypesCounter(&epoch->nbSequences, idx);
}

static inline HdmvSequencePtr getSequenceByIdxHdmvEpochState(
  const HdmvEpochState * epoch,
  hdmv_segtype_idx idx
)
{
  return epoch->sequences[idx].head;
}

static inline void setDSSequenceByIdxHdmvEpochState(
  HdmvEpochState * epoch,
  hdmv_segtype_idx idx,
  HdmvSequencePtr sequence
)
{
  epoch->sequences[idx].ds = sequence;
}

static inline HdmvSequencePtr getDSSequenceByIdxHdmvEpochState(
  const HdmvEpochState * epoch,
  hdmv_segtype_idx idx
)
{
  return epoch->sequences[idx].ds;
}

#define getfirstDOSequenceByIdxHdmvEpochState 0

static inline unsigned getNbSequencesHdmvEpochState(
  const HdmvEpochState ds
)
{
  return getTotalHdmvContextSegmentTypesCounter(ds.nbSequences);
}

/* ### HDMV Segments Inventory Pool : ###################################### */

/** \~english
 * \brief HDMV elements inventory allocation pool initial number of segments.
 *
 */
#define HDMV_SEG_INV_POOL_DEFAULT_SEG_NB  4

/** \~english
 * \brief HDMV elements inventory allocation pool first segment size.
 *
 */
#define HDMV_SEG_INV_POOL_DEFAULT_SEG_SIZE  8

/** \~english
 * \brief HDMV elements inventory allocation pool.
 *
 * This structure is used to provide efficient memory allocation of used HDMV
 * structures. It is used to optimize allocation of an unpredictable number
 * of reusable structures. In HDMV context, structures can be reused at an
 * epoch start since the memory is supposed to be wiped out.
 *
 * elemSegs is a list of incrementally sized lists (called 'segments')
 * containing slots of size elemSize. First elemSegs list index (0) is a
 * segment of #HDMV_SEG_INV_POOL_DEFAULT_SEG_SIZE slots, each of elemSize
 * bytes (so a array of HDMV_SEG_INV_POOL_DEFAULT_SEG_SIZE * elemSize bytes).
 * Each following segment (next index of elemSegs array) size is the double of
 * the previous one.
 *
 * This is a illustration of the elemSegs 2-D array structure:<br />
 * [[0, 1, 2, 3, 4, 5, 6, 7, 8], [9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
 * 20, 21, 22, 23, 24], ...]
 */
typedef struct {
  void ** elemSegs;  /**< 2-D array of allocated elements. */
  size_t elemSize;   /**< Size of one allocated element in bytes. */
  size_t nbAllocElemSegs;  /**< Total elemSegs top array allocated size. */
  size_t nbUsedElemSegs;  /**< Current number of segments used in elemSegs. */
  size_t nbRemElem;  /**< Number of remaining elements in currently used segment. */
} HdmvSegmentsInventoryPool;

static inline void initHdmvSegmentsInventoryPool(
  HdmvSegmentsInventoryPool * dst,
  size_t elemSize
)
{
  *dst = (HdmvSegmentsInventoryPool) {
    .elemSize = elemSize
  };
}

static inline void cleanHdmvSegmentsInventoryPool(
  HdmvSegmentsInventoryPool pool
)
{
  size_t i;

  for (i = 0; i < pool.nbAllocElemSegs; i++)
    free(pool.elemSegs[i]);
  free(pool.elemSegs);
}

static inline void resetHdmvSegmentsInventoryPool(
  HdmvSegmentsInventoryPool * pool
)
{
  pool->nbUsedElemSegs = 0;
  pool->nbRemElem = 0;
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
    for (_i = 0; _i < (p).nbAllocElemSegs; _i++) {                            \
      if (NULL != (p).elemSegs[_i]) {                                         \
        _seg_sz = HDMV_SEG_INV_POOL_DEFAULT_SEG_SIZE << _i;                   \
                                                                              \
        for (_j = 0; _j < _seg_sz; _j++)                                      \
          c(((t *) ((p).elemSegs[_i]))[_j]);                                  \
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

#endif