#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include "hdmv_context.h"

static int _initOutputHdmvContext(
  HdmvContextOutput * output,
  const LibbluESParsingSettings * settings
)
{
  LIBBLU_HDMV_COM_DEBUG(
    " Initialization of output.\n"
  );

  /* Copy script filepath */
  if (NULL == (output->filepath = lbc_strdup(settings->scriptFilepath)))
    LIBBLU_HDMV_COM_ERROR_RETURN("Memory allocation error.\n");

  /* Output script handle */
  if (NULL == (output->script = createNfspEsmsFileHandler(ES_HDMV, settings->options)))
    return -1;

  /* Script output file */
  if (NULL == (output->file = createBitstreamWriterDefBuf(settings->scriptFilepath)))
    return -1;

  return 0;
}

static bool _isInitializedOutputHdmvContext(
  HdmvContextOutput output
)
{
  return NULL != output.file;
}

static void _cleanOutputHdmvContext(
  HdmvContextOutput output
)
{
  free(output.filepath);
  destroyEsmsFileHandler(output.script);
  closeBitstreamWriter(output.file); /* This might be already closed */
}

static int _initInputHdmvContext(
  HdmvContextInput * input,
  HdmvContextOutput output,
  const lbc * infilepath
)
{
  LIBBLU_HDMV_COM_DEBUG(
    " Initialization of input.\n"
  );

  /* Input file */
  if (NULL == (input->file = createBitstreamReaderDefBuf(infilepath)))
    return -1;

  /* Input file script index */
  if (appendSourceFileEsms(output.script, infilepath, &input->idx) < 0)
    return -1;

  return 0;
}

static void _cleanInputHdmvContext(
  HdmvContextInput input
)
{
  closeBitstreamReader(input.file);
}

static void _initSequencesLimit(
  HdmvContextPtr ctx
)
{
  hdmv_segtype_idx idx;

  LIBBLU_HDMV_COM_DEBUG(
    " Initialization of limits.\n"
  );

  static const unsigned segNbLimitsEpoch[][HDMV_NB_SEGMENT_TYPES] = {
    {1, 0, 0, 256, 4096, 1}, /* IGS */
    {0, 1, 8,   8,   64, 1}, /* PGS */
  };
  static const unsigned segNbLimitsDS[][HDMV_NB_SEGMENT_TYPES] = {
    {1, 0, 0, 256, 4096, 1}, /* IGS */
    {0, 1, 1,   8,   64, 1}  /* PGS */
  };

  assert(ctx->type < ARRAY_SIZE(segNbLimitsEpoch));

  for (idx = 0; idx < HDMV_NB_SEGMENT_TYPES; idx++) {
    ctx->sequencesNbLimitPerEpoch[idx] = segNbLimitsEpoch[ctx->type][idx];
    ctx->sequencesNbLimitPerDS   [idx] = segNbLimitsDS   [ctx->type][idx];

    LIBBLU_HDMV_COM_DEBUG(
      "  - %s: %u sequence(s) per epoch, %u per Display Set.\n",
      segmentTypeIndexStr(idx),
      ctx->sequencesNbLimitPerEpoch[idx],
      ctx->sequencesNbLimitPerDS[idx]
    );
  }
}

static int _initSegInvHdmvContext(
  HdmvSegmentsInventoryPtr * inv
)
{
  LIBBLU_HDMV_COM_DEBUG(
    " Creation of the segments allocation inventory.\n"
  );

  if (NULL == (*inv = createHdmvSegmentsInventory()))
    return -1;

  return 0;
}

HdmvContextPtr createHdmvContext(
  LibbluESParsingSettings * settings,
  const lbc * infilepath,
  HdmvStreamType type
)
{
  HdmvContextPtr ctx;
  bool nextByteIsSegmentType;

  if (NULL == infilepath)
    infilepath = settings->esFilepath;

  LIBBLU_HDMV_COM_DEBUG(
    "Creation of the HDMV context for %s stream type.\n",
    HdmvStreamTypeStr(type)
  );

  if (NULL == (ctx = (HdmvContextPtr) calloc(1, sizeof(HdmvContext))))
    LIBBLU_HDMV_COM_ERROR_NRETURN("Memory allocation error.\n");
  ctx->type = type;

  if (_initOutputHdmvContext(&ctx->output, settings) < 0)
    goto free_return;
  if (_initInputHdmvContext(&ctx->input, ctx->output, infilepath) < 0)
    goto free_return;
  if (_initSegInvHdmvContext(&ctx->segInv) < 0)
    goto free_return;
  _initSequencesLimit(ctx);

  /* Add script header place holder (empty header): */
  LIBBLU_HDMV_COM_DEBUG(" Write script header place-holder.\n");
  if (writeEsmsHeader(ctx->output.file) < 0)
    goto free_return;

  /* Set input mode: */
  nextByteIsSegmentType = isValidHdmvSegmentType(nextUint8(ctx->input.file));
  ctx->param.rawStreamInputMode = nextByteIsSegmentType;
  ctx->param.forceRetiming      = nextByteIsSegmentType;

  LIBBLU_HDMV_COM_DEBUG(
    " Parsing settings:\n"
  );
  LIBBLU_HDMV_COM_DEBUG(
    "  - Raw input: %s.\n",
    BOOL_INFO(ctx->param.rawStreamInputMode)
  );
  if (ctx->param.rawStreamInputMode)
    LIBBLU_HDMV_COM_DEBUG("   -> Expect timecodes values.\n");
  LIBBLU_HDMV_COM_DEBUG(
    "  - Force computation of timing values: %s.\n",
    BOOL_INFO(ctx->param.forceRetiming)
  );

  LIBBLU_HDMV_COM_DEBUG(
    " Done.\n"
  );

  return ctx;

free_return:
  destroyHdmvContext(ctx);
  return NULL;
}

void destroyHdmvContext(
  HdmvContextPtr ctx
)
{
  if (NULL == ctx)
    return;

  _cleanOutputHdmvContext(ctx->output);
  _cleanInputHdmvContext(ctx->input);
  destroyHdmvSegmentsInventory(ctx->segInv);
  cleanHdmvTimecodes(ctx->timecodes);
  free(ctx);
}

int addOriginalFileHdmvContext(
  HdmvContextPtr ctx,
  const lbc * filepath
)
{
  if (appendSourceFileEsms(ctx->output.script, filepath, NULL) < 0)
    return -1;

  return 0;
}

void _setEsmsHeader(
  HdmvStreamType type,
  HdmvContextParameters param,
  EsmsFileHeaderPtr script
)
{
  script->ptsRef  = param.referenceClock * 300;
  script->bitrate = HDMV_RX_BITRATE;
  script->endPts  = param.lastClock;
  script->prop.codingType = codingTypeHdmvStreamType(type);
}

int closeHdmvContext(
  HdmvContextPtr ctx
)
{
  /* Complete script */
  /* [u8 endMarker] */
  if (writeByte(ctx->output.file, ESMS_SCRIPT_END_MARKER) < 0)
    return -1;

  _setEsmsHeader(ctx->type, ctx->param, ctx->output.script);

  if (addEsmsFileEnd(ctx->output.file, ctx->output.script) < 0)
    return -1;
  closeBitstreamWriter(ctx->output.file);
  ctx->output.file = NULL;

  /* Update header */
  if (updateEsmsHeader(ctx->output.filepath, ctx->output.script) < 0)
    return -1;

  return 0;
}

static HdmvSequencePtr _getPendingSequence(
  HdmvContextPtr ctx,
  hdmv_segtype_idx idx
)
{
  return ctx->displaySet.pendingSequences[idx];
}

static void _setPendingSequence(
  HdmvContextPtr ctx,
  hdmv_segtype_idx idx,
  HdmvSequencePtr seq
)
{
  ctx->displaySet.pendingSequences[idx] = seq;
}

static int _checkNewDisplaySetTransition(
  HdmvDisplaySet * ds,
  HdmvCDParameters new_composition_desc,
  bool * duplicatedDS
)
{
  HdmvCDParameters prev_composition_desc = ds->composition_descriptor;
  uint16_t prev_composition_number, new_composition_number;
  bool sameCompositionNumber;

  /* composition_number */
  prev_composition_number = prev_composition_desc.composition_number;
  new_composition_number = new_composition_desc.composition_number;
  sameCompositionNumber = (prev_composition_number == new_composition_number);

  if (
    ((prev_composition_number + 1) & 0xFFFF) != new_composition_number
    && !sameCompositionNumber
  ) {
    LIBBLU_HDMV_COM_ERROR_RETURN(
      "Unexpected 'composition_number'==0x%" PRIX16 ", %" PRIu16 " in new DS "
      "(0x%" PRIX16 ", %" PRIu16 " in previous one).\n",
      new_composition_number,
      new_composition_number,
      prev_composition_number,
      prev_composition_number
    );
  }

  /* Same composition_number, the DS shall be a duplicate of previous one */
  *duplicatedDS = sameCompositionNumber;

  return 0;
}

static int _clearCurDisplaySet(
  HdmvContextPtr ctx,
  HdmvCompositionState composition_state
)
{
  unsigned i;

  if (HDMV_COMPO_STATE_EPOCH_START == composition_state) {
    /* Epoch start, clear DS */
    for (i = 0; i < HDMV_NB_SEGMENT_TYPES; i++) {
      ctx->displaySet.sequences[i] = NULL;
      ctx->displaySet.lastSequences[i] = NULL;
    }
    resetHdmvSegmentsInventory(ctx->segInv);
    resetHdmvContextSegmentTypesCounter(&ctx->nbSequences);
  }

  resetHdmvContextSegmentTypesCounter(&ctx->displaySet.nbSequences);
  return 0;
}

static int _checkInitialDisplaySet(
  HdmvCDParameters composition_state
)
{
  if (HDMV_COMPO_STATE_EPOCH_START != composition_state.composition_state)
    LIBBLU_HDMV_COM_ERROR_RETURN(
      "Unexpected first composition_state, "
      "not Epoch Start.\n"
    );

  return 0;
}

int initDisplaySetHdmvContext(
  HdmvContextPtr ctx,
  HdmvCDParameters composition_descriptor
)
{
  HdmvDisplaySet * ds = &ctx->displaySet;

  if (!_isInitializedOutputHdmvContext(ctx->output))
    LIBBLU_HDMV_COM_ERROR_RETURN(
      "Unable to init a new DS, HDMV context handle has been closed.\n"
    );

  if (HDMV_DS_INITIALIZED == ds->initUsage) {
    /* Presence of an already initialized DS without completion. */
    /* TODO: Complete and add END segment */
    LIBBLU_HDMV_COM_ERROR_RETURN(
      "Unimplemented automatic completion of not ended DS.\n"
    );
  }

  if (HDMV_DS_COMPLETED <= ds->initUsage) {
    /* Check new DS composition descriptor continuation with previous one. */
    if (_checkNewDisplaySetTransition(ds, composition_descriptor, &ctx->duplicatedDS) < 0)
      return -1;

    /* Clear for new DS */
    if (_clearCurDisplaySet(ctx, composition_descriptor.composition_state) < 0)
      return -1;
  }
  else {
    /* Initial DS */
    if (_checkInitialDisplaySet(composition_descriptor) < 0)
      return -1;
  }

  ds->composition_descriptor = composition_descriptor;
  ds->initUsage = HDMV_DS_INITIALIZED;
  return 0;
}

static void _printDisplaySetContent(
  HdmvDisplaySet ds,
  HdmvStreamType type
)
{
  LIBBLU_HDMV_COM_DEBUG(" Current number of segments per type:\n");
#define _G(_i)  getByIdxHdmvContextSegmentTypesCounter(ds.nbSequences, _i)
#define _P(_n)  LIBBLU_HDMV_COM_DEBUG("  - "#_n": %u.\n", _G(HDMV_SEGMENT_TYPE_##_n##_IDX))

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

  LIBBLU_HDMV_COM_DEBUG(
    "  TOTAL: %u.\n",
    getTotalHdmvContextSegmentTypesCounter(ds.nbSequences)
  );
}

static int _checkCompletionOfEachSequence(
  HdmvDisplaySet ds
)
{
  bool notNull = false;
  hdmv_segtype_idx i;

  for (i = 0; i < HDMV_NB_SEGMENT_TYPES; i++)
    notNull |= (NULL != ds.pendingSequences[i]);

  return (notNull) ? -1 : 0;
}

static void _getNonCompletedSequencesNames(
  char * dst,
  HdmvDisplaySet ds
)
{
  hdmv_segtype_idx i;

  for (i = 0; i < HDMV_NB_SEGMENT_TYPES; i++) {
    if (NULL != ds.pendingSequences[i]) {
      const char * name = segmentTypeIndexStr(i);
      strcpy(dst, name);
      dst += strlen(name);
    }
  }
}

static int _getDisplaySetPts(
  HdmvContextPtr ctx,
  HdmvDisplaySet * ds,
  uint64_t * value
)
{
  int64_t pts;

  if (HDMV_STREAM_TYPE_IGS == ctx->type)
    pts = ds->sequences[HDMV_SEGMENT_TYPE_ICS_IDX]->segments->param.header.pts;
  else /* HDMV_STREAM_TYPE_PGS == ctx->type */
    pts = ds->sequences[HDMV_SEGMENT_TYPE_PCS_IDX]->segments->param.header.pts;

  pts += (int64_t) ctx->param.referenceClock;
  if (pts < 0)
    LIBBLU_HDMV_COM_ERROR_RETURN("PTS value is lower than the first one.\n");
  *value = (uint64_t) pts;

  return 0;
}

static int _getDisplaySetPresentationTimecode(
  HdmvContextPtr ctx,
  HdmvDisplaySet * ds,
  uint64_t * value
)
{
  if (!ctx->param.rawStreamInputMode)
    return _getDisplaySetPts(ctx, ds, value);
  return getHdmvTimecodes(&ctx->timecodes, value);
}

static uint64_t _computeDisplaySetPlaneClearTime(
  HdmvContextPtr ctx
)
{
  /**
   * PLANE_CLEAR_TIME =
   *     ceil(90000 * 8 * (video_width * video_height) / 128000000)
   * <=> ceil(9 * (video_width * video_height) / 1600)
   *
   * with:
   *  90000        : PTS/DTS clock frequency (90kHz);
   *  128000000    : Pixel Decoding Rate (Rd = 128 Mb/s).
   *  video_width  : video_width parameter parsed from ICS's
   *                 Video_descriptor();
   *  video_height : video_height parameter parsed from ICS's
   *                 Video_descriptor().
   */
  HdmvVDParameters video_desc = ctx->videoDesc;

  return DIV_ROUND_UP(
    9 * (video_desc.video_width * video_desc.video_height),
    1600
  );
}

/** \~english
 * \brief
 *
 * \param type
 * \return uint64_t
 *
 * Reduced pixel decoding rate divider. See _computeDecodeDurationOds().
 */
static uint64_t _pixelDecodingRateDividerValue(
  HdmvStreamType type
)
{
  static const uint64_t values[] = {
    800,  /*< IGS (64Mbps)  */
    1600  /*< PGS (128Mbps) */
  };

  return values[type];
}

static uint64_t _computeDecodeDurationOds(
  HdmvODParameters param,
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
  return DIV_ROUND_UP(
    9 * (param.object_width * param.object_height),
    _pixelDecodingRateDividerValue(type)
  );
}

static uint64_t _computeTransferDurationOds(
  HdmvODParameters objectDefinition
)
{
  /** Compute TRANSFER_DURATION of ODS.
   *
   * Equation performed is:
   *
   *   TRANSFER_DURATION(n) =
   *     DECODE_DURATION(n) * 9
   *
   * with:
   *  DECODE_DURATION : Equation described in _computeDecodeDurationOds().
   */
  return 9 * _computeDecodeDurationOds(
    objectDefinition,
    HDMV_STREAM_TYPE_IGS
  );
}

static uint64_t _computeIgsDisplaySetDecodeDuration(
  HdmvContextPtr ctx,
  const HdmvDisplaySet * ds
)
{
  /** Compute DECODE_DURATION of IGS Display Set (Used as time reference for
   * whole DS).
   *
   * Equation performed is:
   *
   *   DECODE_DURATION = MAX(DECODE_DURATION_ODS, PLANE_CLEAR_TIME)
   *
   * with:
   *  DECODE_DURATION_ODS : Equation described below.
   *  PLANE_CLEAR_TIME    : Equation described in
   *                        #_computeDisplaySetPlaneClearTime().
   *
   * Compute of DECODE_DURATION_ODS:
   *
   *   DECODE_DURATION_ODS = 0
   *   for (i = 0; i < nb_ODS; i++) {
   *     // For each ODS in DS
   *     DECODE_DURATION_ODS += DECODE_DURATION(i)
   *     if (i != nb_ODS-1) {
   *       // Not the last ODS in DS
   *       DECODE_DURATION_ODS += TRANSFER_DURATION(i)
   *     }
   *   }
   *
   * with:
   *  nb_ODS              : Number of ODS in DS.
   *  DECODE_DURATION()   : ODS decode duration, done between Stream Graphics
   *                        Processor and the Object Buffer. Desribed in
   *                        _computeDecodeDurationOds().
   *  TRANSFER_DURATION() : ODS transfer duration, from the Object Buffer to
   *                        the Graphics Plane. Described in
   *                        _computeTransferDurationOds().
   */
  HdmvSequencePtr seq;

  uint64_t planeClearTime = _computeDisplaySetPlaneClearTime(ctx);
  uint64_t decodeDuration;

  decodeDuration = 0;
  for (
    seq = getSequenceByIdxHdmvDisplaySet(ds, HDMV_SEGMENT_TYPE_ODS_IDX);
    NULL != seq;
    seq = seq->nextSequence
  ) {
    assert(seq->type == HDMV_SEGMENT_TYPE_ODS);

    if (!isFromDisplaySetNbHdmvSequence(seq, ctx->nbDisplaySets))
      continue;

    /* DECODE_DURATION() */
    decodeDuration += _computeDecodeDurationOds(
      seq->data.od,
      HDMV_STREAM_TYPE_IGS
    );

    /* TRANSFER_DURATION() */
    if (NULL != seq->nextSequence) {
      decodeDuration += _computeTransferDurationOds(
        seq->data.od
      );
    }
  }

  /* DECODE_DURATION = MAX(DECODE_DURATION_ODS, PLANE_CLEAR_TIME) */
  return MAX(planeClearTime, decodeDuration);
}

static uint64_t _computePgsDisplaySetDecodeDuration(
  const HdmvDisplaySet * ds
)
{
  (void) ds;

  LIBBLU_TODO();
}

int _computeTimestampsDisplaySet(
  HdmvContextPtr ctx,
  HdmvDisplaySet * ds
)
{
  uint64_t timecode, decodeDuration;

  LIBBLU_HDMV_COM_DEBUG("  NOTE: Compute time values:\n");

  if (_getDisplaySetPresentationTimecode(ctx, ds, &timecode) < 0)
    return -1;

  LIBBLU_HDMV_COM_DEBUG(
    "  Requested presentation time: %" PRIu64 " ticks.\n",
    timecode
  );

  if (HDMV_STREAM_TYPE_IGS == ctx->type)
    decodeDuration = _computeIgsDisplaySetDecodeDuration(ctx, ds);
  else /* HDMV_STREAM_TYPE_PGS == ctx->type */
    decodeDuration = _computePgsDisplaySetDecodeDuration(ds);

  LIBBLU_HDMV_COM_DEBUG(
    "  DS decoding duration: %zu ticks.\n",
    decodeDuration
  );

  if (!ctx->nbDisplaySets) {
    /* First display set, init reference clock to handle negative timestamps */
    if (timecode < decodeDuration)
      ctx->param.referenceClock = decodeDuration - timecode;
  }

  if (timecode + ctx->param.referenceClock < decodeDuration)
    LIBBLU_HDMV_COM_ERROR_RETURN(
      "Invalid timecode, decoding period starts before first Display Set.\n"
    );
  if (timecode + ctx->param.referenceClock - decodeDuration < ctx->param.lastClock)
    LIBBLU_HDMV_COM_ERROR_RETURN(
      "Invalid timecode, decoding period of current DS overlaps previous DS.\n"
    );

  LIBBLU_TODO();

  ctx->param.lastClock = timecode + ctx->param.referenceClock;
  return 0;
}

int _copySegmentsTimestampsDisplaySet(
  HdmvContextPtr ctx,
  HdmvDisplaySet * ds
)
{
  hdmv_segtype_idx idx;

  LIBBLU_HDMV_COM_DEBUG("  NOTE: Using input file headers time values:\n");

  for (idx = 0; idx < HDMV_NB_SEGMENT_TYPES; idx++) {
    HdmvSequencePtr seq = getSequenceByIdxHdmvDisplaySet(ds, idx);
    bool displayedListName = false;

    for (; NULL != seq; seq = seq->nextSequence) {
      seq->pts = seq->segments->param.header.pts + ctx->param.referenceClock;
      seq->dts = seq->segments->param.header.dts + ctx->param.referenceClock;

      if (!isFromDisplaySetNbHdmvSequence(seq, ctx->nbDisplaySets))
        continue;

      if (!displayedListName) {
        LIBBLU_HDMV_COM_DEBUG("   - %s:\n", segmentTypeIndexStr(idx));
        displayedListName = true;
      }

      LIBBLU_HDMV_COM_DEBUG(
        "    - PTS: %" PRIu64 " DTS: %" PRIu64 "\n",
        seq->pts,
        seq->dts
      );
    }
  }

  return 0;
}

int _setTimestampsDisplaySet(
  HdmvContextPtr ctx,
  HdmvDisplaySet * ds
)
{
  if (ctx->param.forceRetiming)
    return _computeTimestampsDisplaySet(ctx, ds);
  return _copySegmentsTimestampsDisplaySet(ctx, ds);
}

int _insertSegmentInScript(
  HdmvContextPtr ctx,
  HdmvSegmentParameters seg,
  uint64_t pts,
  uint64_t dts
)
{
  int ret;
  bool dtsPresent;

  assert(NULL != ctx);

  dtsPresent = (
    seg.type == HDMV_SEGMENT_TYPE_ODS
    || seg.type == HDMV_SEGMENT_TYPE_PCS
    || seg.type == HDMV_SEGMENT_TYPE_WDS
    || seg.type == HDMV_SEGMENT_TYPE_ICS
  );

  ret = initEsmsHdmvPesFrame(
    ctx->output.script,
    dtsPresent,
    pts,
    dts
  );
  if (ret < 0)
    return -1;

  ret = appendAddPesPayloadCommand(
    ctx->output.script,
    ctx->input.idx,
    0x0,
    seg.inputFileOffset,
    seg.length + HDMV_SEGMENT_HEADER_LENGTH
  );
  if (ret < 0)
    return -1;

  return writeEsmsPesPacket(
    ctx->output.file,
    ctx->output.script
  );
}

int _registeringSegmentsDisplaySet(
  HdmvContextPtr ctx,
  HdmvDisplaySet * ds
)
{
  hdmv_segtype_idx idx;

  for (idx = 0; idx < HDMV_NB_SEGMENT_TYPES; idx++) {
    HdmvSequencePtr seq = getSequenceByIdxHdmvDisplaySet(ds, idx);

    for (; NULL != seq; seq = seq->nextSequence) {
      HdmvSegmentPtr seg;
      uint64_t pts = seq->pts * 300;
      uint64_t dts = seq->dts * 300;

      if (!isFromDisplaySetNbHdmvSequence(seq, ctx->nbDisplaySets))
        continue;

      for (seg = seq->segments; NULL != seg; seg = seg->nextSegment) {
        if (_insertSegmentInScript(ctx, seg->param, pts, dts) < 0)
          return -1;
      }
    }
  }

  return 0;
}

int completeDisplaySetHdmvContext(
  HdmvContextPtr ctx
)
{

  if (ctx->displaySet.initUsage == HDMV_DS_COMPLETED)
    return 0; /* Already completed */

  LIBBLU_HDMV_COM_DEBUG("Processing complete Display Set...\n");
  _printDisplaySetContent(ctx->displaySet, ctx->type);

  if (_checkCompletionOfEachSequence(ctx->displaySet)) {
    /* Presence of non completed sequences, building list string. */
    char names[HDMV_NB_SEGMENT_TYPES * SEGMENT_TYPE_IDX_STR_SIZE];
    _getNonCompletedSequencesNames(names, ctx->displaySet);

    LIBBLU_HDMV_COM_ERROR_RETURN(
      "Non-completed sequence(s) present in DS: %s.\n",
      names
    );
  }

  if (!getNbSequencesHdmvDisplaySet(ctx->displaySet)) {
    LIBBLU_HDMV_COM_DEBUG(" Empty Display Set, skipping.\n");
    return 0;
  }

  if (ctx->duplicatedDS) {
    /**
     * Special process for duplicated DS (DS sharing same ID as last one),
     * normal checks already be done on original DS, only check presence of
     * each updated sequence.
     * The verification is performed by verifying the absence (since normally
     * refreshed) of sequences whose displaySetIdx field is equal to the
     * number of the last DS (the original). These fields shall be equal
     * to the number of the pending DS or to any older DS if the DS is
     * a normal/epoch continue DS.
     */
    unsigned lastDSIdx = ctx->nbDisplaySets - 1;

    if (checkDuplicatedHdmvDisplaySet(&ctx->displaySet, lastDSIdx) < 0)
      return -1;
  }
  else {
    LIBBLU_HDMV_CK_DEBUG(" Checking Display Set:\n");
#if 0

    if (checkHdmvDisplaySet(&ctx->displaySet, ctx->type, ctx->nbDisplaySets) < 0)
      return -1;
#endif
    if (checkObjectsBufferingHdmvDisplaySet(&ctx->displaySet, ctx->type) < 0)
      return -1;
  }

  /* Set decoding duration/timestamps: */
  LIBBLU_HDMV_COM_DEBUG(" Computation of time values:\n");
  if (_setTimestampsDisplaySet(ctx, &ctx->displaySet) < 0)
    return -1;

  LIBBLU_HDMV_COM_DEBUG(
    " Registering %u sequences...\n",
    getTotalHdmvContextSegmentTypesCounter(ctx->displaySet.nbSequences)
  );

  if (_registeringSegmentsDisplaySet(ctx, &ctx->displaySet) < 0)
    return -1;

  ctx->displaySet.initUsage = HDMV_DS_COMPLETED;

  LIBBLU_HDMV_COM_DEBUG(" Done.\n");
  ctx->nbDisplaySets++;
  return 0;
}

static HdmvSequencePtr _initNewSequenceInCurEpoch(
  HdmvContextPtr ctx,
  hdmv_segtype_idx idx,
  HdmvSegmentParameters segment
)
{
  HdmvSequencePtr seq;
  unsigned curSeqTypeNb, seqTypeNbUpLimitDS;

  /* Fetch the (potential) pending sequence slot */
  if (NULL != (seq = _getPendingSequence(ctx, idx))) {
    /* Not NULL, a non completed sequence is already pending */
    LIBBLU_HDMV_COM_ERROR_NRETURN(
      "Missing a sequence completion segment, "
      "last sequence never completed.\n"
    );
  }

  /* Check the number of already parsed sequences */
  seqTypeNbUpLimitDS = ctx->sequencesNbLimitPerDS[idx];
  curSeqTypeNb = getByIdxHdmvContextSegmentTypesCounter(
    ctx->displaySet.nbSequences,
    idx
  );

  if (seqTypeNbUpLimitDS <= curSeqTypeNb) {
    if (!seqTypeNbUpLimitDS)
      LIBBLU_HDMV_COM_ERROR_NRETURN(
        "Forbidden %s segment type in a %s stream.\n",
        HdmvSegmentTypeStr(segment.type),
        HdmvStreamTypeStr(ctx->type)
      );
    LIBBLU_HDMV_COM_ERROR_NRETURN(
      "Too many %ss in one display set.\n",
      HdmvSegmentTypeStr(segment.type)
    );
  }

  /* Creating a new orphan sequence */
  if (NULL == (seq = getHdmvSequenceHdmvSegmentsInventory(ctx->segInv)))
    return NULL;
  seq->type = segment.type;

  /* Set as the pending sequence */
  _setPendingSequence(ctx, idx, seq);

  return seq;
}

HdmvSequencePtr addSegToSeqDisplaySetHdmvContext(
  HdmvContextPtr ctx,
  hdmv_segtype_idx idx,
  HdmvSegmentParameters segmentParam,
  HdmvSequenceLocation location
)
{
  HdmvSequencePtr seq;
  HdmvSegmentPtr seg;

  assert(isValidSegmentTypeIndexHdmvContext(idx));

  if (isFirstSegmentHdmvSequenceLocation(location)) {
    /* First segment in sequence, initiate a new sequence in current epoch */
    if (NULL == (seq = _initNewSequenceInCurEpoch(ctx, idx, segmentParam)))
      return NULL;
  }
  else {
    /* Not the first segment in sequence, find the pending sequence */
    if (NULL == (seq = _getPendingSequence(ctx, idx))) {
      /* No pending sequence, error */
      LIBBLU_HDMV_COM_ERROR_NRETURN(
        "Receive a sequence continuation segment "
        "after a sequence completion one, "
        "missing a sequence start segment.\n"
      );
    }
  }

  /* Initiate a new segment node in sequence */
  if (NULL == (seg = getHdmvSegmentHdmvSegmentsInventory(ctx->segInv)))
    return NULL;
  seg->param = segmentParam;

  /* Attach the newer sequence (to the last sequence in list) */
  if (NULL == seq->segments) /* Or as the new list header if its empty */
    seq->segments = seg;
  else
    seq->lastSegment->nextSegment = seg;
  seq->lastSegment = seg;

  return seq;
}

static uint32_t _getIdSequence(
  HdmvSequencePtr sequence
)
{
  assert(NULL != sequence);

  switch (sequence->type) {
    case HDMV_SEGMENT_TYPE_PDS:
      return sequence->data.pd.palette_descriptor.palette_id;
    case HDMV_SEGMENT_TYPE_ODS:
      return sequence->data.od.object_descriptor.object_id;
    default:
      break;
  }

  return 0; /* Always update other kind of segments */
}

/** \~english
 * \brief Checks if sequence is a duplicate of one in the supplied list.
 *
 * \param list
 * \param seq
 * \return int
 *
 * In case of success (the sequence is effectively a duplicate), the duplicated
 * sequence present in the list had its #displaySetIdx field is replaced.
 *
 */
static int _insertDuplicatedSequence(
  HdmvSequencePtr list,
  HdmvSequencePtr seq
)
{
  HdmvSequencePtr node;
  uint32_t seqId;

  assert(list->type == seq->type);

  seqId = _getIdSequence(seq);
  for (node = list; NULL != node; node = node->nextSequence) {
    if (seqId == _getIdSequence(node)) {
      if (!isDuplicateHdmvSequence(node, seq))
        LIBBLU_HDMV_COM_ERROR_RETURN(
          "Broken DS, shall be a duplicate of the previous DS "
          "(segments content mismatch).\n"
        );

      if (node->displaySetIdx + 1 != seq->displaySetIdx)
        LIBBLU_HDMV_COM_ERROR_RETURN(
          "Broken DS, shall be a duplicate of the previous DS "
          "(segment not updated in last DS).\n"
        );

      node->displaySetIdx = seq->displaySetIdx;
      node->segments = seq->segments;

      return 0;
    }
  }

  LIBBLU_HDMV_COM_ERROR_RETURN(
    "Broken DS, shall be a duplicate of the previous DS "
    "(segment not present in last DS).\n"
  );
}

static int _updateSequence(
  HdmvSequencePtr dst,
  HdmvSequencePtr src
)
{
  assert(dst->type == src->type);

  switch (dst->type) {
    case HDMV_SEGMENT_TYPE_PDS: {
      if (updateHdmvPdsSegmentParameters(&dst->data.pd, src->data.pd) < 0)
        return -1;
      break;
    }

    case HDMV_SEGMENT_TYPE_ODS: {
      if (updateHdmvObjectDataParameters(&dst->data.od, src->data.od) < 0)
        return -1;
      break;
    }

    case HDMV_SEGMENT_TYPE_PCS:
      dst->data.pc = src->data.pc; break;
    case HDMV_SEGMENT_TYPE_WDS:
      dst->data.wd = src->data.wd; break;
    case HDMV_SEGMENT_TYPE_ICS:
      dst->data.ic = src->data.ic; break;
    case HDMV_SEGMENT_TYPE_END:
      break;

    default:
      assert(0);
      return -1; /* Not updatable */
  }

  dst->displaySetIdx = src->displaySetIdx;
  dst->segments      = src->segments;
  dst->lastSegment   = src->lastSegment;

  return 0;
}

static int _applySequenceUpdate(
  HdmvSequencePtr dst,
  HdmvSequencePtr seq
)
{
  uint32_t lstSeqId;

  lstSeqId = _getIdSequence(seq);

  /* Apply update on the segment sharing the same ID */
  for (; NULL != dst; dst = dst->nextSequence) {
    if (_getIdSequence(dst) == lstSeqId) {
      /* Current sequence shares same ID as the last one */
      if (_updateSequence(dst, seq) < 0)
        return -1;
      return 1;
    }
  }

  return 0;
}

static int _checkNumberOfSequencesInEpoch(
  HdmvContextPtr ctx,
  hdmv_segtype_idx idx,
  HdmvSegmentType type
)
{
  unsigned curSeqTypeNb, seqTypeNbUpLimitEpoch;

  seqTypeNbUpLimitEpoch = ctx->sequencesNbLimitPerEpoch[idx];
  curSeqTypeNb = getByIdxHdmvContextSegmentTypesCounter(
    ctx->nbSequences,
    idx
  );

  if (seqTypeNbUpLimitEpoch <= curSeqTypeNb) {
    if (!seqTypeNbUpLimitEpoch)
      LIBBLU_HDMV_COM_ERROR_RETURN(
        "Forbidden %s segment type in a %s stream.\n",
        HdmvSegmentTypeStr(type),
        HdmvStreamTypeStr(ctx->type)
      );
    LIBBLU_HDMV_COM_ERROR_RETURN(
      "Too many %ss in one epoch.\n",
      HdmvSegmentTypeStr(type)
    );
  }

  return 0;
}

int completeSeqDisplaySetHdmvContext(
  HdmvContextPtr ctx,
  hdmv_segtype_idx idx
)
{
  HdmvSequencePtr sequence, sequenceListHdr;
  int ret;

  /* Check if index correspond to a valid pending sequence */
  if (NULL == (sequence = _getPendingSequence(ctx, idx)))
    LIBBLU_HDMV_COM_ERROR_RETURN("No sequence to complete.\n");
  sequence->displaySetIdx = ctx->nbDisplaySets;

  /* Get the header of completed sequences list of DS */
  sequenceListHdr = ctx->displaySet.sequences[idx];

  if (ctx->duplicatedDS) {
    /* Special case, this DS is supposed to be a duplicate of the previous one. */
    if (_insertDuplicatedSequence(sequenceListHdr, sequence) < 0)
      return -1;
    incrementSequencesNbDSHdmvContext(ctx, idx);
    goto success;
  }

  if (NULL == sequenceListHdr) {
    /* If the list is empty, the pending is the new header */
    ctx->displaySet.sequences[idx] = sequence;
    ctx->displaySet.lastSequences[idx] = sequence;
    incrementSequencesNbEpochHdmvContext(ctx, idx);
    goto success;
  }

  /* Insert in list */
  /* Check if sequence is an update of a previous DS */
  if ((ret = _applySequenceUpdate(sequenceListHdr, sequence)) < 0)
    return -1;

  if (!ret) {
    /* The sequence is not an update, insert in list */

    /* Check the number of sequences in current Epoch */
    if (_checkNumberOfSequencesInEpoch(ctx, idx, sequenceListHdr->type) < 0)
      return -1;

    ctx->displaySet.lastSequences[idx]->nextSequence = sequence;
    ctx->displaySet.lastSequences[idx] = sequence;

#if 0
    sequence->nextSequence = sequenceListHdr;
    ctx->displaySet.sequences[idx] = sequence;
#endif

    incrementSequencesNbEpochHdmvContext(ctx, idx);
  }

success:
  incrementSequencesNbDSHdmvContext(ctx, idx);
  /* Reset pending sequence */
  _setPendingSequence(ctx, idx, NULL);
  return 0;
}