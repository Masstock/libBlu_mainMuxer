/** \~english
 * \file hdmv_context.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief HDMV bitstreams parsing context.
 *
 * \todo Concerns about ODS sequences/segments order: Implementation orders
 * ODS by object_id, Scenarist use reference order in PCS. Does not seems an
 * issue but might be investigated. (original decoding order is keeped if
 * timing values are copied from input).
 * \todo Timings computation not implemented.
 */

#ifndef __LIBBLU_MUXER__CODECS__HDMV__COMMON__CONTEXT_H__
#define __LIBBLU_MUXER__CODECS__HDMV__COMMON__CONTEXT_H__

#include "../../../esms/scriptCreation.h"
#include "../../../ini/iniHandler.h"
#include "../../../util.h"
#include "../../common/esParsingSettings.h"
#include "hdmv_check.h"
#include "hdmv_common.h"
#include "hdmv_timecodes.h"

typedef struct {
  bool generationMode;
  bool rawStreamInputMode;
  bool forceRetiming;

  int64_t referenceTimecode;
  int64_t initialDelay;

  HdmvParsingOptions parsingOptions;

  int64_t lastPresTimestamp;
} HdmvContextParameters;

typedef struct {
  BitstreamReaderPtr file;
  unsigned idx;              /**< Input file script index reference.         */
} HdmvContextInput;

typedef struct {
  lbc * filepath;            /**< Script filepath.                           */
  EsmsFileHeaderPtr script;  /**< Script building handle.                    */
  BitstreamWriterPtr file;   /**< Script file handle.                        */
} HdmvContextOutput;

typedef struct {
  HdmvContextInput input;
  HdmvContextOutput output;
  HdmvSegmentsInventoryPtr segInv;

  HdmvStreamType type;
  HdmvContextParameters param;
  HdmvTimecodes timecodes;

  HdmvEpochState epoch;  /**< Current Epoch Display Set (DS). */
  HdmvContextSegmentTypesCounter nbSequences;  /**< Number of sequences in epoch. */
  bool duplicatedDS;  /**< Current DS shall be a strict copy of previous one. */

  unsigned sequencesNbLimitPerEpoch[HDMV_NB_SEGMENT_TYPES];
  unsigned sequencesNbLimitPerDS[HDMV_NB_SEGMENT_TYPES];

  struct {
    HdmvContextSegmentTypesCounter nbSequences;
    HdmvContextSegmentTypesCounter nbSegments;
  } globalCounters;
  unsigned nbDisplaySets;
} HdmvContext, *HdmvContextPtr;

HdmvContextPtr createHdmvContext(
  LibbluESParsingSettings * settings,
  const lbc * infilepath,
  HdmvStreamType type,
  bool generationMode
);

void destroyHdmvContext(
  HdmvContextPtr ctx
);

int addOriginalFileHdmvContext(
  HdmvContextPtr ctx,
  const lbc * filepath
);

static inline int addTimecodeHdmvContext(
  HdmvContextPtr ctx,
  uint64_t value
)
{
  return addHdmvTimecodes(
    &ctx->timecodes,
    value
  );
}

static inline int addTimecodesHdmvContext(
  HdmvContextPtr ctx,
  HdmvTimecodes timecodes
)
{
  return copyHdmvTimecodes(
    &ctx->timecodes,
    timecodes
  );
}

int closeHdmvContext(
  HdmvContextPtr ctx
);

static inline void printContentHdmvContext(
  HdmvContextPtr ctx
)
{
  printContentHdmvContextSegmentTypesCounter(
    ctx->globalCounters.nbSegments,
    ctx->type
  );
}

static inline HdmvStreamType streamTypeHdmvContext(
  const HdmvContextPtr ctx
)
{
  return ctx->type;
}

static inline int64_t referenceTimecodeHdmvContext(
  const HdmvContextPtr ctx
)
{
  return ctx->param.referenceTimecode;
}

static inline int64_t initialDelayHdmvContext(
  const HdmvContextPtr ctx
)
{
  return ctx->param.initialDelay;
}

static inline HdmvSegmentsInventoryPtr segInvHdmvContext(
  HdmvContextPtr ctx
)
{
  return ctx->segInv;
}

static inline BitstreamReaderPtr inputHdmvContext(
  HdmvContextPtr ctx
)
{
  return ctx->input.file;
}

static inline int readValueHdmvContext(
  HdmvContextPtr ctx,
  size_t length,
  uint32_t * value
)
{
  return readValueBigEndian(inputHdmvContext(ctx), length, value);
}

static inline bool isEofHdmvContext(
  const HdmvContextPtr ctx
)
{
  return isEof(inputHdmvContext(ctx));
}

static inline void setReferenceTimecodeHdmvContext(
  HdmvContextPtr ctx,
  int64_t timecode
)
{
  ctx->param.referenceTimecode = timecode;
}

static inline void setInitialDelayHdmvContext(
  HdmvContextPtr ctx,
  int64_t delay
)
{
  ctx->param.initialDelay = delay;
}

/** \~english
 * \brief Initiate a new Display Set.
 *
 * \param ctx
 * \param composition_descriptor
 * \param video_descriptor
 * \return int
 *
 * Takes the composition_descriptor of the first Display Set segment/sequence.
 */
int initEpochHdmvContext(
  HdmvContextPtr ctx,
  HdmvCDParameters composition_descriptor,
  HdmvVDParameters video_descriptor
);

/** \~english
 * \brief Complete pending Display Set.
 *
 * \param ctx
 * \return int
 */
int completeDisplaySetHdmvContext(
  HdmvContextPtr ctx
);

/** \~english
 * \brief Add a segment in associated sequence in pending Display Set of of
 * the current Epoch.
 *
 * \param ctx
 * \param sequenceTypeIndex
 * \param segmentParam
 * \return HdmvSequencePtr
 *
 * If the segment location is at the start of a sequence, a new one is
 * initiated.
 * If the segment location is at the end of a sequence, the sequence must
 * be marked as completed after decoding using
 * #completeSeqDisplaySetHdmvContext().
 *
 * Unexpected sequence states can occur if:
 *  - A sequence continuity or sequence completion segment is received without
 *  a prior associated sequence start segment;
 *  - A sequence start segment is not preceded by a completion segment.
 */
HdmvSequencePtr addSegToSeqDisplaySetHdmvContext(
  HdmvContextPtr ctx,
  hdmv_segtype_idx sequenceTypeIndex,
  HdmvSegmentParameters segmentParam,
  HdmvSequenceLocation location
);

/** \~english
 * \brief Mark as completed
 *
 * \param ctx
 * \param seq
 * \return int
 */
int completeSeqDisplaySetHdmvContext(
  HdmvContextPtr ctx,
  hdmv_segtype_idx sequenceTypeIndex
);

static inline void incrementSegmentsNbHdmvContext(
  HdmvContextPtr ctx,
  hdmv_segtype_idx idx
)
{
  incByIdxHdmvContextSegmentTypesCounter(&ctx->globalCounters.nbSegments, idx);
}

static inline void incrementSequencesNbDSHdmvContext(
  HdmvContextPtr ctx,
  hdmv_segtype_idx idx
)
{
  incByIdxHdmvContextSegmentTypesCounter(&ctx->epoch.nbSequences, idx);
  incByIdxHdmvContextSegmentTypesCounter(&ctx->globalCounters.nbSequences, idx);
}

static inline void incrementSequencesNbEpochHdmvContext(
  HdmvContextPtr ctx,
  hdmv_segtype_idx idx
)
{
  incByIdxHdmvContextSegmentTypesCounter(&ctx->nbSequences, idx);
  // incByIdxHdmvContextSegmentTypesCounter(&ctx->epoch.nbSequences, idx);
  // incByIdxHdmvContextSegmentTypesCounter(&ctx->globalCounters.nbSequences, idx);
}

#endif