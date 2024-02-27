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

#include "hdmv_check.h"
#include "hdmv_common.h"
#include "hdmv_seq_indexer.h"
#include "hdmv_timecodes.h"

#include "../../../esms/scriptCreation.h"
#include "../../../ini/iniHandler.h"
#include "../../../util.h"
#include "../../common/esParsingSettings.h"

#define ENABLE_DEBUG_TIMESTAMPS

typedef struct {
  bool generation_mode;
  bool raw_stream_input_mode;
  bool force_retiming;

  int64_t ref_timestamp;  /**< Referential "zero" timestamp. This value is
    used to avoid negative timestamps (e.g. if a presentation timestamp is
    set to the first video frame, decoding timestamp might be negative).     */
  int64_t initial_delay;  /**< Timeline origin of the input stream, aka
    timestamp of time 0. This value is used to convert the timestamps of
    the input stream to a timeline whose origin is zero.                     */

  /* Retiming related: */
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
  HdmvParsingOptions options;

  HdmvContextParameters param;
  HdmvTimecodes timecodes;

  hdmv_segtype_idx cur_segment_type;
  HdmvDSState ds[2];
  unsigned cur_ds;
  bool is_dup_DS;  /**< Current DS shall be a strict copy of previous one.   */

  HdmvEpochState epoch;

  HdmvContextSegmentTypesCounter nb_seq;  /**< Number of sequences in epoch. */

  unsigned last_compo_nb_parsed;

  unsigned seq_nb_limit_per_epoch[HDMV_NB_SEGMENT_TYPES];
  unsigned seq_nb_limit_per_DS[HDMV_NB_SEGMENT_TYPES];

  struct {
    HdmvContextSegmentTypesCounter nb_seq;
    HdmvContextSegmentTypesCounter nb_seg;
  } global_counters;
  unsigned nb_DS;
  unsigned nb_epochs;
} HdmvContext;

static inline HdmvDSState * getPrevDSHdmvContext(
  HdmvContext * ctx
)
{
  return &ctx->ds[ctx->cur_ds ^ 1];
}

static inline HdmvDSState * getCurDSHdmvContext(
  HdmvContext * ctx
)
{
  return &ctx->ds[ctx->cur_ds];
}

static inline void switchCurDSHdmvContext(
  HdmvContext * ctx
)
{
  ctx->cur_ds ^= 0x1; // Switch DS

  /* Clean current DS */
  cleanHdmvDSState(ctx->ds[ctx->cur_ds]);
  memset(&ctx->ds[ctx->cur_ds], 0x0, sizeof(HdmvDSState));
}

HdmvContext * createHdmvContext(
  LibbluESParsingSettings * settings,
  const lbc * infilepath,
  HdmvStreamType type,
  bool generation_mode
);

void destroyHdmvContext(
  HdmvContext * ctx
);

int addOriginalFileHdmvContext(
  HdmvContext * ctx,
  const lbc * filepath
);

static inline int addTimecodeHdmvContext(
  HdmvContext * ctx,
  uint64_t value
)
{
  return addHdmvTimecodes(
    &ctx->timecodes,
    value
  );
}

static inline int addTimecodesHdmvContext(
  HdmvContext * ctx,
  HdmvTimecodes timecodes
)
{
  return copyHdmvTimecodes(
    &ctx->timecodes,
    timecodes
  );
}

int closeHdmvContext(
  HdmvContext * ctx
);

static inline void printContentHdmvContext(
  const HdmvContext * ctx
)
{
  printContentHdmvContextSegmentTypesCounter(
    ctx->global_counters.nb_seg,
    ctx->epoch.type
  );
}

static inline BitstreamReaderPtr inputHdmvContext(
  HdmvContext * ctx
)
{
  return ctx->input.file;
}

static inline int readValueHdmvContext(
  HdmvContext * ctx,
  size_t length,
  uint64_t * value
)
{
  return readValue64BigEndian(inputHdmvContext(ctx), length, value);
}

static inline bool isEofHdmvContext(
  const HdmvContext * ctx
)
{
  return isEof(ctx->input.file);
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
  HdmvContext * ctx,
  HdmvCDParameters composition_descriptor,
  HdmvVDParameters video_descriptor
);

/** \~english
 * \brief Complete pending Display Set.
 *
 * \param ctx
 * \return int
 */
int completeCurDSHdmvContext(
  HdmvContext * ctx
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
  HdmvContext * ctx,
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
  HdmvContext * ctx,
  hdmv_segtype_idx idx
);

static inline void incrementSegmentsNbHdmvContext(
  HdmvContext * ctx,
  hdmv_segtype_idx idx
)
{
  incByIdxHdmvContextSegmentTypesCounter(
    &ctx->global_counters.nb_seg,
    idx
  );
}

static inline void incrementSequencesNbDSHdmvContext(
  HdmvContext * ctx,
  hdmv_segtype_idx idx
)
{
  incByIdxHdmvContextSegmentTypesCounter(&ctx->global_counters.nb_seq, idx);
}

static inline void incrementSequencesNbEpochHdmvContext(
  HdmvContext * ctx,
  hdmv_segtype_idx idx
)
{
  incByIdxHdmvContextSegmentTypesCounter(&ctx->nb_seq, idx);
}

#endif