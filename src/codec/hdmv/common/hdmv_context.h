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
  bool generation_mode;
  bool raw_stream_input_mode;
  bool force_retiming;

  int64_t ref_timestamp;
  int64_t initial_delay;

  HdmvParsingOptions parsing_options;

  int64_t last_ds_pres_time;
  int64_t last_ds_end_time;
} HdmvContextParameters;

typedef struct {
  BitstreamReaderPtr file;  /**< Source file handle.                         */
  uint8_t idx;              /**< Input file script index reference.          */
} HdmvContextInput;

typedef struct {
  lbc * filepath;           /**< Script filepath.                            */
  EsmsHandlerPtr script;    /**< Script building handle.                     */
  BitstreamWriterPtr file;  /**< Script file handle.                         */
} HdmvContextOutput;

typedef struct {
  HdmvContextInput input;
  HdmvContextOutput output;
  HdmvSegmentsInventory seg_inv;

  HdmvStreamType type;
  HdmvContextParameters param;
  HdmvTimecodes timecodes;

  HdmvDSState ds;  /**< Current Epoch Display Set (DS). */
  HdmvContextSegmentTypesCounter nb_seq;  /**< Number of sequences in epoch. */
  bool is_dup_DS;  /**< Current DS shall be a strict copy of previous one. */
  unsigned last_compo_nb_parsed;

  unsigned seq_nb_limit_per_epoch[HDMV_NB_SEGMENT_TYPES];
  unsigned seq_nb_limit_per_DS[HDMV_NB_SEGMENT_TYPES];

  struct {
    HdmvContextSegmentTypesCounter nb_seq;
    HdmvContextSegmentTypesCounter nb_seg;
  } global_counters;
  unsigned nb_DS;
  unsigned nb_epochs;
} HdmvContext, *HdmvContextPtr;

HdmvContextPtr createHdmvContext(
  LibbluESParsingSettings * settings,
  const lbc * infilepath,
  HdmvStreamType type,
  bool generation_mode
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
    ctx->global_counters.nb_seg,
    ctx->type
  );
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
  uint64_t * value
)
{
  return readValue64BigEndian(inputHdmvContext(ctx), length, value);
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
  ctx->param.ref_timestamp = timecode;
}

static inline void setInitialDelayHdmvContext(
  HdmvContextPtr ctx,
  int64_t delay
)
{
  ctx->param.initial_delay = delay;
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
int completeDSHdmvContext(
  HdmvContextPtr ctx
);

/** \~english
 * \brief Add a segment in associated sequence in pending Display Set of of
 * the current Epoch.
 *
 * \param ctx
 * \param idx
 * \param seg_param
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
  hdmv_segtype_idx idx,
  HdmvSegmentParameters seg_param,
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
  hdmv_segtype_idx idx
);

static inline void incrementSegmentsNbHdmvContext(
  HdmvContextPtr ctx,
  hdmv_segtype_idx idx
)
{
  incByIdxHdmvContextSegmentTypesCounter(
    &ctx->global_counters.nb_seg,
    idx
  );
}

static inline void incrementSequencesNbDSHdmvContext(
  HdmvContextPtr ctx,
  hdmv_segtype_idx idx
)
{
  incByIdxHdmvContextSegmentTypesCounter(&ctx->ds.nb_seq, idx);
  incByIdxHdmvContextSegmentTypesCounter(&ctx->global_counters.nb_seq, idx);
}

static inline void incrementSequencesNbEpochHdmvContext(
  HdmvContextPtr ctx,
  hdmv_segtype_idx idx
)
{
  incByIdxHdmvContextSegmentTypesCounter(&ctx->nb_seq, idx);
  // incByIdxHdmvContextSegmentTypesCounter(&ctx->epoch.nb_seq, idx);
  // incByIdxHdmvContextSegmentTypesCounter(&ctx->global_counters.nb_seq, idx);
}

#endif