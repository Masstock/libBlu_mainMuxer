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

  LIBBLU_HDMV_COM_DEBUG(
    " Initialization of limits.\n"
  );

  static const unsigned segNbLimitsEpoch[][HDMV_NB_SEGMENT_TYPES] = {
    {1, 0, 0, 256, 4096, 1}, /* IG */
    {0, 1, 8,   8,   64, 1}  /* PG */
  };
  static const unsigned segNbLimitsDS[][HDMV_NB_SEGMENT_TYPES] = {
    {1, 0, 0, 256, 4096, 1}, /* IG */
    {0, 1, 1,   8,   64, 1}  /* PG */
  };

  assert(ctx->type < ARRAY_SIZE(segNbLimitsEpoch));

  for (hdmv_segtype_idx idx = 0; idx < HDMV_NB_SEGMENT_TYPES; idx++) {
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

static int _initParsingOptionsHdmvContext(
  HdmvParsingOptions * optionsDst,
  LibbluESParsingSettings * settings
)
{
  const IniFileContextPtr conf = settings->options.confHandle;
  lbc * string;

  /* Default : */
  optionsDst->orderIgsSegByValue = true;
  optionsDst->orderPgsSegByValue = false;

  string = lookupIniFile(conf, "HDMV.ORDERIGSSEGMENTSBYVALUE");
  if (NULL != string) {
    bool value;

    if (lbc_atob(&value, string) < 0)
      LIBBLU_ERROR_RETURN(
        "Invalid boolean value setting 'orderIgsSegmentsByValue' in section "
        "[HDMV] of INI file.\n"
      );
    optionsDst->orderIgsSegByValue = value;
  }

  string = lookupIniFile(conf, "HDMV.ORDERPGSSEGMENTSBYVALUE");
  if (NULL != string) {
    bool value;

    if (lbc_atob(&value, string) < 0)
      LIBBLU_ERROR_RETURN(
        "Invalid boolean value setting 'orderPgsSegmentsByValue' in section "
        "[HDMV] of INI file.\n"
      );

    optionsDst->orderPgsSegByValue = value;
  }

  return 0;
}

HdmvContextPtr createHdmvContext(
  LibbluESParsingSettings * settings,
  const lbc * infilepath,
  HdmvStreamType type,
  bool generationMode
)
{
  HdmvContextPtr ctx;

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

  /* Set options: */
  /* 1. Based on arguments */
  ctx->param.generationMode      = generationMode;

  /* 2. Based on presence of IG/PG headers */
  bool nextByteIsSegHdr = isValidHdmvSegmentType(nextUint8(ctx->input.file));
  ctx->param.rawStreamInputMode = nextByteIsSegHdr;
  ctx->param.forceRetiming      = nextByteIsSegHdr;

  /* 3. Based on user input options */
  ctx->param.forceRetiming      |= settings->options.hdmv.forceRetiming;
  setInitialDelayHdmvContext(ctx, settings->options.hdmv.initialTimestamp);

  /* 4. Based on configuration file */
  if (_initParsingOptionsHdmvContext(&ctx->param.parsingOptions, settings) < 0)
    goto free_return;

  if (!ctx->param.forceRetiming) {
    /* Using input stream timestamps, do not touch segments order. */
    ctx->param.parsingOptions.orderIgsSegByValue = false;
    ctx->param.parsingOptions.orderPgsSegByValue = false;
  }

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
    "  - Initial timestamp: %" PRId64 ".\n",
    ctx->param.initialDelay
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
  script->ptsRef  = param.referenceTimecode * 300;
  script->bitrate = HDMV_RX_BITRATE;
  script->endPts  = (param.referenceTimecode + param.lastPresTimestamp) * 300;
  script->prop.codingType = codingTypeHdmvStreamType(type);
}

int closeHdmvContext(
  HdmvContextPtr ctx
)
{
  /* Complete script */
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
  return ctx->epoch.sequences[idx].pending;
}

static void _setPendingSequence(
  HdmvContextPtr ctx,
  hdmv_segtype_idx idx,
  HdmvSequencePtr seq
)
{
  ctx->epoch.sequences[idx].pending = seq;
}

static int _checkNewDisplaySetTransition(
  HdmvEpochState * epoch,
  HdmvCDParameters new_composition_desc,
  bool * duplicatedDS
)
{
  HdmvCDParameters prev_composition_desc = epoch->composition_descriptor;

  /* composition_number */
  uint16_t prev_composition_number = prev_composition_desc.composition_number;
  uint16_t new_composition_number = new_composition_desc.composition_number;
  bool sameCompoNb = (prev_composition_number == new_composition_number);

  if (
    ((prev_composition_number + 1) & 0xFFFF) != new_composition_number
    && !sameCompoNb
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
  *duplicatedDS = sameCompoNb;

  return 0;
}

static int _clearEpochHdmvContext(
  HdmvContextPtr ctx,
  HdmvCompositionState composition_state
)
{

  if (HDMV_COMPO_STATE_EPOCH_START == composition_state) {
    /* Epoch start, clear DS */
    for (unsigned i = 0; i < HDMV_NB_SEGMENT_TYPES; i++) {
      ctx->epoch.sequences[i].head = NULL;
      ctx->epoch.sequences[i].last = NULL;
    }

    resetHdmvSegmentsInventory(ctx->segInv);
    resetHdmvContextSegmentTypesCounter(&ctx->nbSequences);
  }

  for (unsigned i = 0; i < HDMV_NB_SEGMENT_TYPES; i++) {
    assert(NULL == ctx->epoch.sequences[i].pending); // This shall be ensured at DS completion.
    ctx->epoch.sequences[i].ds = NULL;
  }

  resetHdmvContextSegmentTypesCounter(&ctx->epoch.nbSequences);
  return 0;
}

static int _checkInitialCompositionDescriptor(
  HdmvCDParameters composition_descriptor
)
{
  // TODO: Support branching epoch_continue?
  if (HDMV_COMPO_STATE_EPOCH_START != composition_descriptor.composition_state)
    LIBBLU_HDMV_COM_ERROR_RETURN(
      "Unexpected first composition_descriptor composition_state, "
      "expect Epoch Start (is %s).\n",
      HdmvCompositionStateStr(composition_descriptor.composition_state)
    );

  return 0;
}

static int _checkVideoDescriptorChangeHdmvEpochState(
  HdmvEpochState * epoch,
  HdmvVDParameters video_descriptor
)
{

  if (!areIdenticalHdmvVDParameters(video_descriptor, epoch->video_descriptor)) {
    /* Non-identical video_descriptor */

    if (epoch->video_descriptor.video_width != video_descriptor.video_width)
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "HDMV video_descriptor video_width value is not constant "
        "(0x%04" PRIX16 " => 0x%04" PRIX16 ").\n",
        epoch->video_descriptor.video_width,
        video_descriptor.video_width
      );

    if (epoch->video_descriptor.video_height != video_descriptor.video_height)
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "HDMV video_descriptor video_height value is not constant "
        "(0x%04" PRIX16 " => 0x%04" PRIX16 ").\n",
        epoch->video_descriptor.video_height,
        video_descriptor.video_height
      );

    if (epoch->video_descriptor.frame_rate != video_descriptor.frame_rate)
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "HDMV video_descriptor frame_rate value is not constant "
        "(0x%02" PRIX8 " => 0x%02" PRIX8 ").\n",
        epoch->video_descriptor.frame_rate,
        video_descriptor.frame_rate
      );
  }

  return 0;
}

int initEpochHdmvContext(
  HdmvContextPtr ctx,
  HdmvCDParameters composition_descriptor,
  HdmvVDParameters video_descriptor
)
{
  HdmvEpochState * epoch = &ctx->epoch;

  if (!_isInitializedOutputHdmvContext(ctx->output)) {
    LIBBLU_HDMV_COM_ERROR_RETURN(
      "Unable to init a new DS, "
      "HDMV context handle has been closed.\n"
    );
  }

  if (HDMV_DS_INITIALIZED == epoch->initUsage) {
    /* Presence of an already initialized DS without completion. */
    // TODO: Complete and add END segment
    LIBBLU_HDMV_COM_ERROR_RETURN(
      "Unimplemented automatic completion of not ended DS.\n"
    );
  }

  if (HDMV_DS_COMPLETED <= epoch->initUsage) {
    /* Check new DS composition descriptor continuation with previous one. */
    if (_checkNewDisplaySetTransition(epoch, composition_descriptor, &ctx->duplicatedDS) < 0)
      return -1;

    /* Check video_descriptor change */
    if (_checkVideoDescriptorChangeHdmvEpochState(epoch, video_descriptor) < 0)
      return -1;

    /* Clear for new DS */
    if (_clearEpochHdmvContext(ctx, composition_descriptor.composition_state) < 0)
      return -1;
  }
  else {
    /* Initial DS */
    if (_checkInitialCompositionDescriptor(composition_descriptor) < 0)
      return -1;
    // TODO: Check initial video_descriptor
  }

  epoch->composition_descriptor = composition_descriptor;
  epoch->video_descriptor = video_descriptor;
  epoch->initUsage = HDMV_DS_INITIALIZED;
  return 0;
}

static void _printDisplaySetContent(
  HdmvEpochState epoch,
  HdmvStreamType type
)
{
  LIBBLU_HDMV_COM_DEBUG(" Current number of segments per type:\n");
#define _G(_i)  getByIdxHdmvContextSegmentTypesCounter(epoch.nbSequences, _i)
#define _P(_n)  LIBBLU_HDMV_COM_DEBUG("  - "#_n": %u.\n", _G(HDMV_SEGMENT_TYPE_##_n##_IDX))

  switch (type) {
    case HDMV_STREAM_TYPE_IGS:
      _P(ICS);
      break;

    case HDMV_STREAM_TYPE_PGS:
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
    getTotalHdmvContextSegmentTypesCounter(epoch.nbSequences)
  );
}

static int _checkCompletionOfEachSequence(
  HdmvEpochState epoch
)
{
  bool notNull = false;

  for (hdmv_segtype_idx i = 0; i < HDMV_NB_SEGMENT_TYPES; i++)
    notNull |= (NULL != epoch.sequences[i].pending);

  return (notNull) ? -1 : 0;
}

static void _getNonCompletedSequencesNames(
  char * dst,
  HdmvEpochState epoch
)
{
  for (hdmv_segtype_idx i = 0; i < HDMV_NB_SEGMENT_TYPES; i++) {
    if (NULL != epoch.sequences[i].pending) {
      const char * name = segmentTypeIndexStr(i);
      strcpy(dst, name);
      dst += strlen(name);
    }
  }
}

static int _getDisplaySetPts(
  const HdmvContextPtr ctx,
  const HdmvEpochState * epoch,
  int64_t * value
)
{
  int64_t pts;

  switch (ctx->type) {
#define GET_PTS(idx)  epoch->sequences[idx].ds->segments->param.header.pts

    case HDMV_STREAM_TYPE_IGS:
      assert(NULL != epoch->sequences[HDMV_SEGMENT_TYPE_ICS_IDX].ds);
      pts = GET_PTS(HDMV_SEGMENT_TYPE_ICS_IDX); break;

    case HDMV_STREAM_TYPE_PGS:
      assert(NULL != epoch->sequences[HDMV_SEGMENT_TYPE_PCS_IDX].ds);
      pts = GET_PTS(HDMV_SEGMENT_TYPE_PCS_IDX); break;

#undef GET_PTS
  }

  pts -= ctx->param.initialDelay;
  if (pts < 0)
    LIBBLU_HDMV_COM_ERROR_RETURN("PTS value is below zero.\n");

  *value = pts;

  return 0;
}

static int64_t _computePlaneClearTime(
  const HdmvEpochState * epoch,
  HdmvStreamType stream_type
)
{
  /** Compute PLANE_CLEAR_TIME of Interactive Composition or Presentation
   * Composition, the duration required to clear the whole graphical plane.
   *
   * If stream type is Interactive Graphics:
   *
   *   PLANE_CLEAR_TIME =
   *     ceil(90000 * 8 * (video_width * video_height) / 128000000)
   * <=> ceil(9 * (video_width * video_height) / 1600)
   *
   * Otherwhise, if stream type is Presentation Graphics:
   *
   *   PLANE_CLEAR_TIME =
   *     ceil(90000 * 8 * (video_width * video_height) / 256000000)
   * <=> ceil(9 * (video_width * video_height) / 3200)
   *
   * where:
   *  90000        : PTS/DTS clock frequency (90kHz);
   *  128000000 or
   *  256000000    : Pixel Transfer Rate (Rc = 128 Mb/s for IG,
   *                 256 Mb/s for PG).
   *  video_width  : video_width parameter parsed from ICS/PCS's
   *                 Video_descriptor();
   *  video_height : video_height parameter parsed from ICS/PCS's
   *                 Video_descriptor().
   */
  HdmvVDParameters video_desc = epoch->video_descriptor;

  static const int64_t clearingRateDivider[] = {
    1600LL,  /*< IG (128Mbps)  */
    3200LL   /*< PG (256Mbps) */
  };

  return DIV_ROUND_UP(
    9LL * (video_desc.video_width * video_desc.video_height),
    clearingRateDivider[stream_type]
  );
}

static int32_t _computeODDecodeDuration(
  HdmvODParameters param,
  HdmvStreamType type
)
{
  /** Compute OD_DECODE_DURATION of Object Definition (OD).
   *
   * \code{.unparsed}
   * Equation performed is (for Interactive Graphics):
   *
   *   OD_DECODE_DURATION(OD) =
   *     ceil(90000 * 8 * (OD.object_width * OD.object_height) / 64000000)
   * <=> ceil(9 * (OD.object_width * OD.object_height) / 800) // Compacted version
   *
   * or (for Presentation Graphics):
   *
   *   OD_DECODE_DURATION(OD) =
   *     ceil(90000 * 8 * (OD.object_width * OD.object_height) / 128000000)
   * <=> ceil(9 * (OD.object_width * OD.object_height) / 1600) // Compacted version
   *
   * where:
   *  OD            : Object Definition.
   *  90000         : PTS/DTS clock frequency (90kHz);
   *  64000000 or
   *  128000000     : Pixel Decoding Rate
   *                  (Rd = 64 Mb/s for IG, Rd = 128 Mb/s for PG).
   *  object_width  : object_width parameter parsed from n-ODS's Object_data().
   *  object_height : object_height parameter parsed from n-ODS's Object_data().
   * \endcode
   */
  static const int32_t pixelDecodingRateDivider[] = {
    800,  /*< IG (64Mbps)  */
    1600  /*< PG (128Mbps) */
  };

  return DIV_ROUND_UP(
    9 * (param.object_width * param.object_height),
    pixelDecodingRateDivider[type]
  );
}

/* Interactive Graphics Stream : */

#define ENABLE_ODS_TRANSFER_DURATION  true

#if ENABLE_ODS_TRANSFER_DURATION

static int32_t _computeODTransferDuration(
  HdmvODParameters objectDefinition
)
{
  /** Compute OD_TRANSFER_DURATION of Object Definition (OD).
   *
   * Equation performed is:
   *
   * \code{.unparsed}
   *  OD_TRANSFER_DURATION(DS_n):
   *    return OD_DECODE_DURATION(OD) * 9
   *
   * where:
   *  OD_DECODE_DURATION : Equation described in _computeODDecodeDuration().
   * \endcode
   */
  return 9 * _computeODDecodeDuration(
    objectDefinition,
    HDMV_STREAM_TYPE_IGS
  );
}

#endif

static int64_t _computeICDecodeDuration(
  const HdmvEpochState * epoch
)
{
  /** Compute the IC_DECODE_DURATION time value of the Interactive Composition
   * (IC) from the Display Set n (DS_n). This value is the initialization time
   * required to decode all graphical objects (contained in Object Definition
   * Segments, ODS) required for the presentation of the IC and to clear the
   * graphical plane if necessary.
   *
   * This process is partially described in patent [3], supplement by
   * experimentation.
   *
   * Equation performed is:
   *
   * \code{.unparsed}
   *  IC_DECODE_DURATION(DS_n):
   *    if (DS_n[ICS].composition_state == EPOCH_START)
   *      return MAX(OBJ_DECODE_DURATION(DS_n), PLANE_CLEAR_TIME(DS_n))
   *    else
   *      return OBJ_DECODE_DURATION(DS_n)
   *
   * where:
   *  OBJ_DECODE_DURATION() : Equation described below, corresponding to the
   *                          required time to decode and transfer needed
   *                          graphical objects from the Object Definition
   *                          Segments (ODSs) of the DS_n.
   *  PLANE_CLEAR_TIME()    : Minimal amount of time required to erase the
   *                          graphical plane before drawing decoded graphical
   *                          objects data according to the IC.
   *                          This duration is only required if the DS is the
   *                          first one of a new Epoch (EPOCH_START).
   *                          Computed using #_computePlaneClearTime().
   * \endcode
   *
   * Compute of OBJ_DECODE_DURATION is as followed:
   *
   * \code{.unparsed}
   *  OBJ_DECODE_DURATION():
   *    decode_duration = 0
   *    for (i = 0; i < DS_n[ICS].number_of_pages; i++) {
   *      for each (OD of DS_n[ICS].PAGE[i].button) {
   *        decode_duration += OD_DECODE_DURATION(OD)
   *        if (ODS is not the last of DS_n) {
   *          decode_duration += OD_TRANSFER_DURATION(OD)
   *        }
   *      }
   *    }
   *    return decode_duration
   *
   * where:
   *  OD_DECODE_DURATION()   : Object Definition decode duration, done between
   *                           Stream Graphics Processor and the Object Buffer.
   *                           Described in #_computeODDecodeDuration().
   *  OD_TRANSFER_DURATION() : Object Definition transfer duration, from the
   *                           Object Buffer to the Graphics Plane.
   *                           Described in #_computeODTransferDuration().
   * \endcode
   */

  LIBBLU_HDMV_TSC_DEBUG(
    "    Compute DECODE_DURATION of IG Display Set:\n"
  );

  // TODO: Fix to only count OD present in current Display Set (other are already decoded)

  int64_t objDecodeDuration = 0;
  HdmvSequencePtr seq = getDSSequenceByIdxHdmvEpochState(epoch, HDMV_SEGMENT_TYPE_ODS_IDX);
  for (; NULL != seq; seq = seq->nextSequenceDS) {
    assert(seq->type == HDMV_SEGMENT_TYPE_ODS);

    LIBBLU_HDMV_TSC_DEBUG(
      "      - ODS id=0x%04" PRIX16 " version_number=%" PRIu8 ":\n",
      seq->data.od.object_descriptor.object_id,
      seq->data.od.object_descriptor.object_version_number
    );

    /* OD_DECODE_DURATION() */
    objDecodeDuration += seq->decodeDuration = _computeODDecodeDuration(
      seq->data.od,
      HDMV_STREAM_TYPE_IGS
    );

    LIBBLU_HDMV_TSC_DEBUG(
      "       => OD_DECODE_DURATION: %" PRId64 "\n",
      seq->decodeDuration
    );

#if ENABLE_ODS_TRANSFER_DURATION
    /* OD_TRANSFER_DURATION() */
    if (NULL != seq->nextSequence) {
      objDecodeDuration += seq->transferDuration = _computeODTransferDuration(
        seq->data.od
      );

      LIBBLU_HDMV_TSC_DEBUG(
        "       => OD_TRANSFER_DURATION: %" PRId64 "\n",
        seq->transferDuration
      );
    }
#else
    seq->transferDuration = 0;
#endif
  }

  LIBBLU_HDMV_TSC_DEBUG(
    "     => OBJ_DECODE_DURATION: %" PRId64 " ticks.\n",
    objDecodeDuration
  );

  if (HDMV_COMPO_STATE_EPOCH_START == epoch->composition_descriptor.composition_state) {
    int64_t planeClearTime = _computePlaneClearTime(epoch, HDMV_STREAM_TYPE_IGS);

    LIBBLU_HDMV_TSC_DEBUG(
      "     => PLANE_CLEAR_TIME: %" PRId64 " ticks.\n",
      planeClearTime
    );

    /* IC_DECODE_DURATION = MAX(PLANE_CLEAR_TIME, OBJ_DECODE_DURATION) */
    return MAX(planeClearTime, objDecodeDuration);
  }

  /* IC_DECODE_DURATION = OBJ_DECODE_DURATION */
  return objDecodeDuration;
}

static int64_t _computeICTransferDuration(
  const HdmvEpochState * epoch
)
{
  /** Compute the TRANSFER_DURATION time value of the Interactive Composition
   * (IC) from the Display Set n (DS_n). This value is the minimal delay
   * required to tranfer graphical objects data needed for initial
   * presentation of the IC from the Object Buffer (DB) to the Graphics Plane.
   *
   * The initial presentation being the first Page of the IC, if an in_effect
   * animation is provided, graphical objects used in the animation are the
   * ones considered as making the initial presentation. Otherwise (no
   * in_effect provided), graphical objects used by the buttons are considered
   * as making the initial presentation.
   *
   * This process is partially described in patent [3], supplement by
   * experimentation.
   *
   * \code{.unparsed}
   *  IC_TRANSFER_DURATION(DS_n):
   *    if (0 < DS_n[ICS].PAGE[0].IN_EFFECTS.number_of_effects)
   *      return EFFECT_TRANSFER_DURATION(DS_n)
   *    else
   *      return PAGE_TRANSFER_DURATION(DS_n)
   *
   * where:
   *  EFFECT_TRANSFER_DURATION() : Equation described below, transfer duration
   *                               of the graphical objects data needed by
   *                               the IC first page in_effect animation steps.
   *  PAGE_TRANSFER_DURATION()   : Equation described below, transfer duration
   *                               of the graphical objects data needed by
   *                               the buttons of the IC first page.
   *
   * NOTE: Patent [3] equation (fig.35&36) describe different cases depending
   * on specifiation or not of a default button, making a difference between
   * buttons 'normal state' graphics and 'selected state' graphics. In fact
   * this difference is only relevant if the default selected button has
   * a graphical object specified for the 'selected state' but not for
   * the 'normal state', since objects used in the each state of a button
   * must share the same size (if present).
   * \endcode
   *
   * Equation for the EFFECT_TRANSFER_DURATION value is as followed:
   *
   * \code{.unparsed}
   *  EFFECT_TRANSFER_DURATION(DS_n):
   *    return 90000 * SUM(DS_n[ICS].PAGE[0].IN_EFFECTS.windows) / 128000000
   *
   * where:
   *  90000     : Frequency of the 90kHz clock used by PTS/DTS fields (90kHz).
   *  SUM()     : Sum of the size in pixels of each window.
   *  128000000 : Transfer data rate between Object Buffer and Graphics Plane
   *              in HDMV Interactive Graphics buffering model (128Mbps).
   * \endcode
   *
   * Equation for the PAGE_TRANSFER_DURATION value is as followed:
   *
   * \code{.unparsed}
   *  PAGE_TRANSFER_DURATION(DS_n):
   *    return 90000 * SUM(DS_n[ICS].PAGE[0].buttons) / 128000000
   *
   * where:
   *  90000     : Frequency of the 90kHz clock used by PTS/DTS fields (90kHz).
   *  SUM()     : Sum of the size in pixels of each button. This sum use the
   *              size of the normal state object or of the selected state
   *              if no normal state object is defined (or none if the button
   *              does not have a normal state and no selected state neither).
   *  128000000 : Transfer data rate between Object Buffer and Graphics Plane
   *              in HDMV Interactive Graphics buffering model (128Mbps).
   *
   * NOTE: If no default button is defined, this equation use a worst-case
   * scenario, considering any button of the page as selectable.
   * \endcode
   */
  HdmvSequencePtr seq = getDSSequenceByIdxHdmvEpochState(epoch, HDMV_SEGMENT_TYPE_ICS_IDX);

  assert(NULL != seq); // ICS shall be present.
  assert(NULL == seq->nextSequenceDS); // ICS shall be unique.

  if (!seq->data.ic.number_of_pages)
    return 0; // No page, empty Interactive Composition.

  HdmvPageParameters * page = seq->data.ic.pages[0];
  int64_t size = 0;

  if (0 < page->in_effects.number_of_effects) {
    /* EFFECT_TRANSFER_DURATION() */
    LIBBLU_HDMV_TSC_DEBUG(
      "      -> Compute EFFECT_TRANSFER_DURATION:\n"
    );

    for (unsigned i = 0; i < page->in_effects.number_of_windows; i++) {
      HdmvWindowInfoParameters * win = page->in_effects.windows[i];
      size += 1LL * win->window_width * win->window_height;

      LIBBLU_HDMV_TSC_DEBUG(
        "       - Window %u: %ux%u.\n",
        i,
        win->window_width,
        win->window_height
      );
    }
  }
  else {
    /* PAGE_TRANSFER_DURATION */
    LIBBLU_HDMV_TSC_DEBUG(
      "      -> Compute PAGE_TRANSFER_DURATION:\n"
    );

    for (unsigned i = 0; i < page->number_of_BOGs; i++) {
      HdmvButtonOverlapGroupParameters * bog = page->bogs[i];

      for (unsigned j = 0; j < bog->number_of_buttons; j++) {
        HdmvButtonParam * btn = bog->buttons[j];

        if (btn->button_id == bog->default_valid_button_id_ref) {
          size += 1LL * btn->max_initial_width * btn->max_initial_height;

          LIBBLU_HDMV_TSC_DEBUG(
            "       - Button %u: %ux%u.\n",
            btn->button_id,
            btn->max_initial_width,
            btn->max_initial_height
          );
          break;
        }
      }
    }
  }

  int64_t transferDuration = DIV_ROUND_UP(9LL * size, 1600);

  LIBBLU_HDMV_TSC_DEBUG(
    "     => TRANSFER_DURATION: %" PRId64 " ticks.\n",
    transferDuration
  );

  return transferDuration;
}

/** \~english
 * \brief Compute Interactive Graphics Display Set decoding duration.
 *
 * \param epoch
 * \return int64_t
 *
 * Compute the Display Set n (DS_n) Interactive Graphics Segments PTS-DTS delta
 * required. Written DECODE_DURATION for IG DS_n, this value is the minimal
 * required initialization duration for DS_n of an Interactive Graphics stream.
 * Aka the required amount of time to decode and transfer all needed graphical
 * objects data for the Interactive Composition (IC), clear if needed the
 * graphical plane and then draw decoded graphical objects data to the graph-
 * ical plane.
 *
 * This process is partially described in patent [3], supplement by experi-
 * mentation.
 *
 * Equation performed is:
 *
 * \code{.unparsed}
 *  DECODE_DURATION(DS_n):
 *    return IC_DECODE_DURATION(DS_n) + IC_TRANSFER_DURATION(DS_n)
 *
 * where:
 *  IC_DECODE_DURATION()   : Time required to decode graphics objects needed
 *                           for the presentation of the DS_n.
 *                           Described in #_computeICDecodeDuration().
 *  IC_TRANSFER_DURATION() : Time required to transfer decoded graphics
 *                           objects data needed for the presentation of
 *                           the display composition.
 *                           Desribed in #_computeICTransferDuration().
 * \endcode
 */
static int64_t _computeIGDisplaySetDecodeDuration(
  const HdmvEpochState * epoch
)
{
  return _computeICDecodeDuration(epoch) + _computeICTransferDuration(epoch);
}

static void _applyTimestampsIGDisplaySet(
  HdmvEpochState * epoch,
  int64_t presentationTimestamp,
  int64_t decodeDuration,
  int64_t referenceTimecode
)
{
  int64_t decodeTimestamp = presentationTimestamp - decodeDuration;
  (void) referenceTimecode;

  /* ICS */ {
    LIBBLU_HDMV_COM_DEBUG("   - %s:\n", HdmvSegmentTypeStr(HDMV_SEGMENT_TYPE_ICS));

    HdmvSequencePtr seq = getDSSequenceByIdxHdmvEpochState(epoch, HDMV_SEGMENT_TYPE_ICS_IDX);
    assert(isUniqueInDisplaySetHdmvSequence(seq));
    seq->pts = presentationTimestamp;
    seq->dts = decodeTimestamp;

    LIBBLU_HDMV_COM_DEBUG(
      "    - PTS: %" PRId64 " DTS: %" PRId64 "\n",
      seq->pts - referenceTimecode,
      seq->dts - referenceTimecode
    );
  }

  /* PDS */ {
    LIBBLU_HDMV_COM_DEBUG("   - %s:\n", HdmvSegmentTypeStr(HDMV_SEGMENT_TYPE_PDS));

    for (
      HdmvSequencePtr seq = getDSSequenceByIdxHdmvEpochState(epoch, HDMV_SEGMENT_TYPE_PDS_IDX);
      NULL != seq;
      seq = seq->nextSequenceDS
    ) {
      seq->pts = decodeTimestamp;
      seq->dts = 0;

      LIBBLU_HDMV_COM_DEBUG(
      "    - PTS: %" PRId64 " DTS: %" PRId64 "\n",
      seq->pts - referenceTimecode,
      seq->dts - referenceTimecode
    );
    }
  }

  /* ODS */ {
    LIBBLU_HDMV_COM_DEBUG("   - %s:\n", HdmvSegmentTypeStr(HDMV_SEGMENT_TYPE_ODS));

    for (
      HdmvSequencePtr seq = getDSSequenceByIdxHdmvEpochState(epoch, HDMV_SEGMENT_TYPE_ODS_IDX);
      NULL != seq;
      seq = seq->nextSequenceDS
    ) {
      seq->dts = decodeTimestamp;
      decodeTimestamp += seq->decodeDuration;
      seq->pts = decodeTimestamp;
      decodeTimestamp += seq->transferDuration;

      LIBBLU_HDMV_COM_DEBUG(
      "    - PTS: %" PRId64 " DTS: %" PRId64 "\n",
      seq->pts - referenceTimecode,
      seq->dts - referenceTimecode
    );
    }

    assert(decodeTimestamp <= presentationTimestamp); // Might be less
  }

  /* END */ {
    LIBBLU_HDMV_COM_DEBUG("   - %s:\n", HdmvSegmentTypeStr(HDMV_SEGMENT_TYPE_END));

    HdmvSequencePtr seq = getDSSequenceByIdxHdmvEpochState(epoch, HDMV_SEGMENT_TYPE_END_IDX);
    assert(isUniqueInDisplaySetHdmvSequence(seq));
    seq->pts = decodeTimestamp;
    seq->dts = 0;

    LIBBLU_HDMV_COM_DEBUG(
      "    - PTS: %" PRId64 " DTS: %" PRId64 "\n",
      seq->pts - referenceTimecode,
      seq->dts - referenceTimecode
    );
  }
}

/* Presentation Graphics Stream : */

static int64_t _computePlaneInitializationTime(
  const HdmvEpochState * epoch
)
{
  /** Compute PLANE_INITIALIZATION_TIME of current PG Display Set (DS_n) from
   * a Presentation Graphics Stream, the duration required to initialize
   * the graphical plane before composition objects writing.
   *
   * \code{.unparsed}
   *  PLANE_INITIALIZATION_TIME(DS_n):
   *    if (DS_n.PCS.composition_state == EPOCH_START) {
   *      return PLANE_CLEAR_TIME(DS_n)
   *    }
   *    else {
   *      inialize_duration = 0;
   *      for (i = 0; i < DS_n.WDS.num_windows; i++) {
   *        if (EMPTY(DS_n.WDS.WIN[i])) {
   *          inialize_duration += ceil(
   *            90000 * 8 * DS_n.WDS.window[i].window_width * DS_n.WDS.window[i].window_height
   *            / 256000000
   *          )
   *        }
   *      }
   *      return inialize_duration
   *    }
   *
   * where:
   *  PLANE_CLEAR_TIME() : Graphical plane clearing duration compute function.
   *                       Described in #_computePlaneClearTime().
   *  90000         : PTS/DTS clock frequency (90kHz);
   *  256000000     : Pixel Transfer Rate (256 Mb/s for PG).
   */

  HdmvCompositionState composition_state = epoch->composition_descriptor.composition_state;
  if (HDMV_COMPO_STATE_EPOCH_START == composition_state) {
    /* Epoch start, clear whole graphical plane */
    return _computePlaneClearTime(epoch, HDMV_STREAM_TYPE_PGS);
  }

  /* Not epoch start, clear only empty windows */
  HdmvPCParameters pc = epoch->sequences[HDMV_SEGMENT_TYPE_PCS_IDX].ds->data.pc;
  HdmvWDParameters wd = epoch->sequences[HDMV_SEGMENT_TYPE_WDS_IDX].ds->data.wd;

  int64_t initialize_duration = 0;
  int64_t clearing_delay = 0;

  for (unsigned i = 0; i < wd.number_of_windows; i++) {
    HdmvWindowInfoParameters * window = wd.windows[i];

    bool empty = true;
    for (unsigned j = 0; j < pc.number_of_composition_objects; j++) {
      if (window->window_id == pc.composition_objects[j]->window_id_ref) {
        /* A composition object use the window, mark as not empty */
        empty = false;
        break;
      }
    }

    if (empty) {
      /* Empty window, clear it */
      initialize_duration += DIV_ROUND_UP(
        9LL * window->window_width * window->window_height,
        3200
      );
      clearing_delay = 1; // Extra clearing room observed
    }
  }

  return initialize_duration + clearing_delay;
}

static int64_t _computeAndSetODSDecodeDuration(
  const HdmvEpochState * epoch,
  uint16_t object_id
)
{

  HdmvSequencePtr ods = getDSSequenceByIdxHdmvEpochState(epoch, HDMV_SEGMENT_TYPE_ODS_IDX);
  for (; NULL != ods; ods = ods->nextSequenceDS) {
    if (object_id == ods->data.od.object_descriptor.object_id)
      break;
  }

  if (NULL == ods)
    return 0; // !EXISTS(object_id, DS_n)

  int32_t decode_duration = _computeODDecodeDuration(ods->data.od, HDMV_STREAM_TYPE_PGS);

  ods->decodeDuration = decode_duration;
  ods->transferDuration = 0;

  return decode_duration;
}

static int64_t _computeWindowTransferDuration(
  const HdmvEpochState * epoch,
  uint16_t window_id
)
{
  HdmvWDParameters wd = epoch->sequences[HDMV_SEGMENT_TYPE_WDS_IDX].ds->data.wd;

  HdmvWindowInfoParameters * window = NULL;
  for (unsigned i = 0; i < wd.number_of_windows; i++) {
    if (window_id == wd.windows[i]->window_id) {
      window = wd.windows[i];
      break;
    }
  }

  assert(NULL != window); // Enforced at check

  return DIV_ROUND_UP(
    9LL * window->window_width * window->window_height,
    3200
  );
}

static int64_t _setWindowDrawingDuration(
  const HdmvEpochState * epoch
)
{
  HdmvSequencePtr wds = epoch->sequences[HDMV_SEGMENT_TYPE_WDS_IDX].ds;
  assert(isUniqueInDisplaySetHdmvSequence(wds));

  const HdmvWDParameters wd = wds->data.wd;

  int64_t size = 0;
  for (unsigned i = 0; i < wd.number_of_windows; i++) {
    const HdmvWindowInfoParameters * window = wd.windows[i];
    size += window->window_width * window->window_height;
  }

  /* ceil(90000 * 8 * size / 3200) */
  int64_t drawing_duration = DIV_ROUND_UP(9LL * size, 3200);

  wds->decodeDuration = drawing_duration;
  return drawing_duration;
}

static int64_t _computePGDisplaySetDecodeDuration(
  const HdmvEpochState * epoch
)
{
  /** Compute this PCS from Display Set n (DS_n) PTS-DTS difference required.
   * This value is the minimal required initialization duration PGS DS_n.
   *
   * Compute the minimal delta between Presentation Composition Segment (PCS)
   * DTS and PTS values. Aka the required amount of time to decode and transfer
   * all needed graphical objects data for the Presentation Composition (PC),
   * clear if needed the graphical plane and windows, and then draw decoded
   * graphical objects data to the graphical plane.
   *
   */

  LIBBLU_HDMV_TSC_DEBUG(
    "    Compute DECODE_DURATION of PG Display Set:\n"
  );

  int64_t decode_duration = _computePlaneInitializationTime(epoch);

  LIBBLU_HDMV_TSC_DEBUG(
    "     => PLANE_INITIALIZATION_TIME(DS_n): %" PRId64 " ticks.\n",
    decode_duration
  );

  HdmvPCParameters pc = epoch->sequences[HDMV_SEGMENT_TYPE_PCS_IDX].ds->data.pc;

  if (2 == pc.number_of_composition_objects) {
    const HdmvCompositionObjectParameters * obj0 = pc.composition_objects[0];
    int64_t obj_0_decode_duration = _computeAndSetODSDecodeDuration(epoch, obj0->object_id_ref);

    LIBBLU_HDMV_TSC_DEBUG(
      "      => ODS_DECODE_DURATION(DS_n.PCS.OBJ[0]): %" PRId64 " ticks.\n",
      obj_0_decode_duration
    );

    const HdmvCompositionObjectParameters * obj1 = pc.composition_objects[1];
    int64_t obj_1_decode_duration = _computeAndSetODSDecodeDuration(epoch, obj1->object_id_ref);

    LIBBLU_HDMV_TSC_DEBUG(
      "      => ODS_DECODE_DURATION(DS_n.PCS.OBJ[1]): %" PRId64 " ticks.\n",
      obj_1_decode_duration
    );

    decode_duration = MAX(decode_duration, obj_0_decode_duration);

    if (obj0->window_id_ref == obj1->window_id_ref) {
      decode_duration = MAX(decode_duration, obj_0_decode_duration + obj_1_decode_duration);

      int64_t obj_0_transfer_duration = _computeWindowTransferDuration(epoch, obj0->window_id_ref);

      LIBBLU_HDMV_TSC_DEBUG(
        "      => WINDOW_TRANSFER_DURATION(DS_n.PCS.OBJ[0]): %" PRId64 " ticks.\n",
        obj_0_transfer_duration
      );

      decode_duration += obj_0_transfer_duration;
    }
    else {
      int64_t obj_0_transfer_duration = _computeWindowTransferDuration(epoch, obj0->window_id_ref);

      LIBBLU_HDMV_TSC_DEBUG(
        "      => WINDOW_TRANSFER_DURATION(DS_n.PCS.OBJ[0]): %" PRId64 " ticks.\n",
        obj_0_transfer_duration
      );

      decode_duration += obj_0_transfer_duration;
      decode_duration = MAX(decode_duration, obj_0_decode_duration + obj_1_decode_duration);

      int64_t obj_1_transfer_duration = _computeWindowTransferDuration(epoch, obj1->window_id_ref);

      LIBBLU_HDMV_TSC_DEBUG(
        "      => WINDOW_TRANSFER_DURATION(DS_n.PCS.OBJ[1]): %" PRId64 " ticks.\n",
        obj_1_transfer_duration
      );

      decode_duration += obj_1_transfer_duration;
    }
  }
  else if (1 == pc.number_of_composition_objects) {
    const HdmvCompositionObjectParameters * obj0 = pc.composition_objects[0];
    int64_t obj_0_decode_duration = _computeAndSetODSDecodeDuration(epoch, obj0->object_id_ref);

    LIBBLU_HDMV_TSC_DEBUG(
      "     => ODS_DECODE_DURATION(DS_n.PCS.OBJ[0]): %" PRId64 " ticks.\n",
      obj_0_decode_duration
    );

    decode_duration = MAX(decode_duration, obj_0_decode_duration);

    int64_t obj_0_transfer_duration = _computeWindowTransferDuration(epoch, obj0->window_id_ref);

      LIBBLU_HDMV_TSC_DEBUG(
        "      => WINDOW_TRANSFER_DURATION(DS_n.PCS.OBJ[0]): %" PRId64 " ticks.\n",
        obj_0_transfer_duration
      );

    decode_duration += obj_0_transfer_duration;
  }

  LIBBLU_HDMV_TSC_DEBUG(
    "     => DECODE_DURATION(DS_n): %" PRId64 " ticks.\n",
    decode_duration
  );

  int64_t min_drawing_duration = _setWindowDrawingDuration(epoch);

  LIBBLU_HDMV_TSC_DEBUG(
    "     => MIN_DRAWING_DURATION(DS_n.WDS): %" PRId64 " ticks.\n",
    min_drawing_duration
  );

  return decode_duration;
}

static void _applyTimestampsPGDisplaySet(
  HdmvEpochState * epoch,
  int64_t presentationTimestamp,
  int64_t decodeDuration,
  int64_t referenceTimecode
)
{
  int64_t decodeTimestamp = presentationTimestamp - decodeDuration;
  (void) referenceTimecode;

  /* PCS */ {
    LIBBLU_HDMV_COM_DEBUG("   - %s:\n", HdmvSegmentTypeStr(HDMV_SEGMENT_TYPE_PCS));

    HdmvSequencePtr seq = getDSSequenceByIdxHdmvEpochState(epoch, HDMV_SEGMENT_TYPE_PCS_IDX);
    assert(isUniqueInDisplaySetHdmvSequence(seq));
    seq->pts = presentationTimestamp;
    seq->dts = decodeTimestamp;

    LIBBLU_HDMV_COM_DEBUG(
      "    - PTS: %" PRId64 " DTS: %" PRId64 "\n",
      seq->pts - referenceTimecode,
      seq->dts - referenceTimecode
    );
  }

  /* WDS */ {
    LIBBLU_HDMV_COM_DEBUG("   - %s:\n", HdmvSegmentTypeStr(HDMV_SEGMENT_TYPE_WDS));

    HdmvSequencePtr seq = getDSSequenceByIdxHdmvEpochState(epoch, HDMV_SEGMENT_TYPE_WDS_IDX);
    assert(isUniqueInDisplaySetHdmvSequence(seq));
    seq->pts = presentationTimestamp - seq->decodeDuration;
    seq->dts = decodeTimestamp;

    LIBBLU_HDMV_COM_DEBUG(
      "    - PTS: %" PRId64 " DTS: %" PRId64 "\n",
      seq->pts - referenceTimecode,
      seq->dts - referenceTimecode
    );
  }

  /* PDS */ {
    LIBBLU_HDMV_COM_DEBUG("   - %s:\n", HdmvSegmentTypeStr(HDMV_SEGMENT_TYPE_PDS));

    for (
      HdmvSequencePtr seq = getDSSequenceByIdxHdmvEpochState(epoch, HDMV_SEGMENT_TYPE_PDS_IDX);
      NULL != seq;
      seq = seq->nextSequenceDS
    ) {
      seq->pts = decodeTimestamp;
      seq->dts = 0;

      LIBBLU_HDMV_COM_DEBUG(
        "    - PTS: %" PRId64 " DTS: %" PRId64 "\n",
        seq->pts - referenceTimecode,
        seq->dts - referenceTimecode
      );
    }
  }

  /* ODS */ {
    LIBBLU_HDMV_COM_DEBUG("   - %s:\n", HdmvSegmentTypeStr(HDMV_SEGMENT_TYPE_ODS));

    for (
      HdmvSequencePtr seq = getDSSequenceByIdxHdmvEpochState(epoch, HDMV_SEGMENT_TYPE_ODS_IDX);
      NULL != seq;
      seq = seq->nextSequenceDS
    ) {
      seq->dts = decodeTimestamp;
      decodeTimestamp += seq->decodeDuration;
      seq->pts = decodeTimestamp;
      decodeTimestamp += seq->transferDuration;

      LIBBLU_HDMV_COM_DEBUG(
        "    - PTS: %" PRId64 " DTS: %" PRId64 "\n",
        seq->pts - referenceTimecode,
        seq->dts - referenceTimecode
      );
    }

    assert(decodeTimestamp <= presentationTimestamp); // Might be less
  }

  /* END */ {
    LIBBLU_HDMV_COM_DEBUG("   - %s:\n", HdmvSegmentTypeStr(HDMV_SEGMENT_TYPE_END));

    HdmvSequencePtr seq = getDSSequenceByIdxHdmvEpochState(epoch, HDMV_SEGMENT_TYPE_END_IDX);
    assert(isUniqueInDisplaySetHdmvSequence(seq));
    seq->pts = decodeTimestamp;
    seq->dts = 0;

    LIBBLU_HDMV_COM_DEBUG(
      "    - PTS: %" PRId64 " DTS: %" PRId64 "\n",
      seq->pts - referenceTimecode,
      seq->dts - referenceTimecode
    );
  }
}

#define ENABLE_DEBUG_TIMESTAMPS  true

#if ENABLE_DEBUG_TIMESTAMPS

static void _debugCompareComputedTimestampsDisplaySet(
  const HdmvContextPtr ctx,
  const HdmvEpochState * epoch
)
{
  for (hdmv_segtype_idx idx = 0; idx < HDMV_NB_SEGMENT_TYPES; idx++) {
    HdmvSequencePtr seq = epoch->sequences[idx].ds;

    for (unsigned i = 0; NULL != seq; seq = seq->nextSequenceDS, i++) {
      int64_t computedDts = seq->dts - ctx->param.referenceTimecode + ctx->param.initialDelay;

      if (seq->dts != 0 && computedDts != seq->segments->param.header.dts)
        LIBBLU_HDMV_TSC_WARNING(
          "Mismatch %s-%u DTS: exp %" PRId32 " got %" PRId64 ".\n",
          HdmvSegmentTypeStr(seq->type), i,
          seq->segments->param.header.dts,
          computedDts
        );

      int64_t computedPts = seq->pts - ctx->param.referenceTimecode + ctx->param.initialDelay;

      if (computedPts != seq->segments->param.header.pts)
        LIBBLU_HDMV_TSC_WARNING(
          "Mismatch %s-%u PTS: exp %" PRId32 " got %" PRId64 ".\n",
          HdmvSegmentTypeStr(seq->type), i,
          seq->segments->param.header.pts,
          computedPts
        );
    }
  }
}

#endif

static int _computeTimestampsDisplaySet(
  HdmvContextPtr ctx
)
{
  HdmvEpochState * epoch = &ctx->epoch;
  int64_t presTime;
  int ret;

  /* get Display Set presentation timecode */
  if (ctx->param.rawStreamInputMode)
    ret = getHdmvTimecodes(&ctx->timecodes, &presTime);
  else
    ret = _getDisplaySetPts(ctx, epoch, &presTime);
  if (ret < 0)
    return -1;

  LIBBLU_HDMV_COM_DEBUG(
    "   Requested presentation time: %" PRId64 " ticks.\n",
    presTime
  );

  int64_t decodeDuration;
  switch (ctx->type) {
    case HDMV_STREAM_TYPE_IGS:
      decodeDuration = _computeIGDisplaySetDecodeDuration(epoch);
      break;
    case HDMV_STREAM_TYPE_PGS:
      decodeDuration = _computePGDisplaySetDecodeDuration(epoch);
      break;
  }

  LIBBLU_HDMV_COM_DEBUG(
    "   DS decode duration: %" PRId64 " ticks.\n",
    decodeDuration
  );

  if (0 == ctx->nbDisplaySets) {
    /* First display set, init reference clock to handle negative timestamps. */
    if (presTime < decodeDuration) {
      LIBBLU_HDMV_COM_DEBUG(
        "   Set reference timecode: %" PRId64 " ticks.\n",
        decodeDuration - presTime
      );

      ctx->param.referenceTimecode = decodeDuration - presTime;
    }
  }
  else {
    /* Not the first DS, check its decoding period does not overlap previous DS one. */
    if (presTime - decodeDuration < ctx->param.lastPresTimestamp)
      LIBBLU_HDMV_COM_ERROR_RETURN(
        "Invalid timecode, decoding period of current DS overlaps previous DS.\n"
      );
  }

  int64_t presTs = ctx->param.referenceTimecode + presTime;
  int64_t refTc = ctx->param.referenceTimecode;

  switch (ctx->type) {
    case HDMV_STREAM_TYPE_IGS:
      _applyTimestampsIGDisplaySet(epoch, presTs, decodeDuration, refTc);
      break;

    case HDMV_STREAM_TYPE_PGS:
      _applyTimestampsPGDisplaySet(epoch, presTs, decodeDuration, refTc);
      break;
  }

#if ENABLE_DEBUG_TIMESTAMPS
  if (!ctx->param.rawStreamInputMode)
    _debugCompareComputedTimestampsDisplaySet(ctx, epoch);
#endif

  ctx->param.lastPresTimestamp = presTime;
  return 0;
}

static void _copySegmentsTimestampsDisplaySetHdmvContext(
  HdmvContextPtr ctx
)
{
  int64_t timeOffset = ctx->param.referenceTimecode - ctx->param.initialDelay;
  int64_t lastPts = ctx->param.lastPresTimestamp;

  for (hdmv_segtype_idx idx = 0; idx < HDMV_NB_SEGMENT_TYPES; idx++) {
    bool displayedListName = false;

    HdmvSequencePtr seq = getDSSequenceByIdxHdmvEpochState(&ctx->epoch, idx);
    for (; NULL != seq; seq = seq->nextSequenceDS) {
      HdmvSegmentParameters segment = seq->segments->param;

      seq->pts = segment.header.pts + timeOffset;
      seq->dts = (0 < segment.header.dts) ? segment.header.dts + timeOffset : 0;

      lastPts = MAX(lastPts, segment.header.pts + timeOffset);

      if (!displayedListName) {
        LIBBLU_HDMV_COM_DEBUG("   - %s:\n", HdmvSegmentTypeStr(seq->type));
        displayedListName = true;
      }

      LIBBLU_HDMV_COM_DEBUG(
        "    - PTS: %" PRId64 " DTS: %" PRId64 "\n",
        seq->pts - ctx->param.referenceTimecode,
        seq->dts - ctx->param.referenceTimecode
      );
    }
  }

  ctx->param.lastPresTimestamp = lastPts;
}

int _setTimestampsDisplaySet(
  HdmvContextPtr ctx
)
{
  if (ctx->param.forceRetiming) {
    LIBBLU_HDMV_COM_DEBUG("  Compute timestamps:\n");
    return _computeTimestampsDisplaySet(ctx);
  }

  LIBBLU_HDMV_COM_DEBUG("  Copying timestamps from headers:\n");
  _copySegmentsTimestampsDisplaySetHdmvContext(ctx);
  return 0;
}

int _insertSegmentInScript(
  HdmvContextPtr ctx,
  HdmvSegmentParameters seg,
  uint64_t pts,
  uint64_t dts
)
{

  assert(NULL != ctx);

  bool dtsPresent = (
    seg.type == HDMV_SEGMENT_TYPE_ODS
    || seg.type == HDMV_SEGMENT_TYPE_PCS
    || seg.type == HDMV_SEGMENT_TYPE_WDS
    || seg.type == HDMV_SEGMENT_TYPE_ICS
  );

  if (initEsmsHdmvPesFrame(ctx->output.script, dtsPresent, pts, dts) < 0)
    return -1;

  int ret = appendAddPesPayloadCommand(
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

static int _registeringSegmentsDisplaySet(
  HdmvContextPtr ctx,
  HdmvEpochState * epoch
)
{

  for (hdmv_segtype_idx idx = 0; idx < HDMV_NB_SEGMENT_TYPES; idx++) {
    HdmvSequencePtr seq = getDSSequenceByIdxHdmvEpochState(epoch, idx);

    for (; NULL != seq; seq = seq->nextSequenceDS) {
      HdmvSegmentPtr seg;

      uint64_t pts = (uint64_t) seq->pts * 300;
      uint64_t dts = (uint64_t) seq->dts * 300;

      assert(segmentTypeIndexHdmvContext(seq->type) == idx);

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

  if (ctx->epoch.initUsage == HDMV_DS_COMPLETED)
    return 0; /* Already completed */

  LIBBLU_HDMV_COM_DEBUG("Processing complete Display Set...\n");
  _printDisplaySetContent(ctx->epoch, ctx->type);

  if (_checkCompletionOfEachSequence(ctx->epoch)) {
    /* Presence of non completed sequences, building list string. */
    char names[HDMV_NB_SEGMENT_TYPES * SEGMENT_TYPE_IDX_STR_SIZE];
    _getNonCompletedSequencesNames(names, ctx->epoch);

    LIBBLU_HDMV_COM_ERROR_RETURN(
      "Non-completed sequence(s) present in DS: %s.\n",
      names
    );
  }

  if (!getNbSequencesHdmvEpochState(ctx->epoch)) {
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

    if (checkDuplicatedDSHdmvEpochState(&ctx->epoch, lastDSIdx) < 0)
      return -1;
  }
  else {
    int ret;
    LIBBLU_HDMV_CK_DEBUG(" Checking and building Display Set:\n");

    /* Check Epoch Objects Buffers */
    if (checkObjectsBufferingHdmvEpochState(&ctx->epoch, ctx->type) < 0)
      return -1;

    /* Build the display set */
    ret = checkAndBuildDisplaySetHdmvEpochState(
      ctx->param.parsingOptions,
      ctx->type,
      &ctx->epoch,
      ctx->nbDisplaySets
    );
    if (ret < 0)
      return -1;
  }

  /* Set decoding duration/timestamps: */
  LIBBLU_HDMV_COM_DEBUG(" Set timestamps of the Display Set:\n");
  if (_setTimestampsDisplaySet(ctx) < 0)
    return -1;

  LIBBLU_HDMV_COM_DEBUG(
    " Registering %u sequences...\n",
    getTotalHdmvContextSegmentTypesCounter(ctx->epoch.nbSequences)
  );

  if (_registeringSegmentsDisplaySet(ctx, &ctx->epoch) < 0)
    return -1;

  ctx->epoch.initUsage = HDMV_DS_COMPLETED;

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
    ctx->epoch.nbSequences,
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
      dst->data.pc = src->data.pc;
      break;

    case HDMV_SEGMENT_TYPE_WDS:
      dst->data.wd = src->data.wd;
      break;

    case HDMV_SEGMENT_TYPE_ICS: {
      if (updateHdmvICParameters(&dst->data.ic, src->data.ic) < 0)
        return -1;
      break;
    }

    case HDMV_SEGMENT_TYPE_END:
    case HDMV_SEGMENT_TYPE_ERROR:
      break;
  }

  dst->nextSequenceDS = NULL;
  dst->displaySetIdx  = src->displaySetIdx;
  dst->segments       = src->segments;
  dst->lastSegment    = src->lastSegment;

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
    assert(dst->type == seq->type);

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
  sequenceListHdr = ctx->epoch.sequences[idx].head;

  if (ctx->duplicatedDS) {
    /* Special case, this DS is supposed to be a duplicate of the previous one. */
    if (_insertDuplicatedSequence(sequenceListHdr, sequence) < 0)
      return -1;

    /* Update Display Order if not identical. */
    // TODO: Does duplicated sequences must be identical ? If so use DO to check.

    goto success;
  }

  if (NULL == sequenceListHdr) {
    /* If the list is empty, the pending is the new header */
    ctx->epoch.sequences[idx].head = sequence;
    ctx->epoch.sequences[idx].last = sequence;

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

    ctx->epoch.sequences[idx].last->nextSequence = sequence;
    ctx->epoch.sequences[idx].last = sequence;

    incrementSequencesNbEpochHdmvContext(ctx, idx);
  }

success:
  incrementSequencesNbDSHdmvContext(ctx, idx);
  /* Reset pending sequence */
  _setPendingSequence(ctx, idx, NULL);
  return 0;
}