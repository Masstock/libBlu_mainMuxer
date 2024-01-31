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
  output->filepath = lbc_strdup(settings->scriptFilepath);
  if (NULL == output->filepath)
    LIBBLU_HDMV_COM_ERROR_RETURN("Memory allocation error.\n");

  /* Output script handle */
  output->script = createEsmsHandler(ES_HDMV, settings->options, FMT_SPEC_INFOS_NONE);
  if (NULL == output->script)
    return -1;

  /* Script output file */
  output->file = createBitstreamWriterDefBuf(settings->scriptFilepath);
  if (NULL == output->file)
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
  destroyEsmsHandler(output.script);
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
  if (appendSourceFileEsmsHandler(output.script, infilepath, &input->idx) < 0)
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

  static const unsigned seg_nb_limits_epoch[][HDMV_NB_SEGMENT_TYPES] = {
    {1, 0, 0, 256, HDMV_OD_IG_MAX_NB_OBJ, 1}, /* IG */
    {0, 1, 8,   8, HDMV_OD_PG_MAX_NB_OBJ, 8}  /* PG */
  };
  static const unsigned seg_nb_limits_DS[][HDMV_NB_SEGMENT_TYPES] = {
    {1, 0, 0, 256, HDMV_OD_IG_MAX_NB_OBJ, 1}, /* IG */
    {0, 1, 1,   8, HDMV_OD_PG_MAX_NB_OBJ, 1}  /* PG */
  };

  assert(ctx->type < ARRAY_SIZE(seg_nb_limits_epoch));

  for (hdmv_segtype_idx idx = 0; idx < HDMV_NB_SEGMENT_TYPES; idx++) {
    ctx->seq_nb_limit_per_epoch[idx] = seg_nb_limits_epoch[ctx->type][idx];
    ctx->seq_nb_limit_per_DS   [idx] = seg_nb_limits_DS   [ctx->type][idx];

    LIBBLU_HDMV_COM_DEBUG(
      "  - %s: %u sequence(s) per epoch, %u per Display Set.\n",
      segmentTypeIndexStr(idx),
      ctx->seq_nb_limit_per_epoch[idx],
      ctx->seq_nb_limit_per_DS[idx]
    );
  }
}

static int _initParsingOptionsHdmvContext(
  HdmvParsingOptions * options_dst,
  LibbluESParsingSettings * settings
)
{
  const IniFileContextPtr conf = settings->options.confHandle;
  lbc * string;

  /* Default : */
  options_dst->orderIgsSegByValue = true;
  options_dst->orderPgsSegByValue = false;

  string = lookupIniFile(conf, "HDMV.ORDERIGSSEGMENTSBYVALUE");
  if (NULL != string) {
    bool value;

    if (lbc_atob(&value, string) < 0)
      LIBBLU_ERROR_RETURN(
        "Invalid boolean value setting 'orderIgsSegmentsByValue' in section "
        "[HDMV] of INI file.\n"
      );
    options_dst->orderIgsSegByValue = value;
  }

  string = lookupIniFile(conf, "HDMV.ORDERPGSSEGMENTSBYVALUE");
  if (NULL != string) {
    bool value;

    if (lbc_atob(&value, string) < 0)
      LIBBLU_ERROR_RETURN(
        "Invalid boolean value setting 'orderPgsSegmentsByValue' in section "
        "[HDMV] of INI file.\n"
      );

    options_dst->orderPgsSegByValue = value;
  }

  return 0;
}

HdmvContextPtr createHdmvContext(
  LibbluESParsingSettings * settings,
  const lbc * infilepath,
  HdmvStreamType type,
  bool generation_mode
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
  initHdmvSegmentsInventory(&ctx->seg_inv);
  _initSequencesLimit(ctx);

  /* Add script header place holder (empty header): */
  LIBBLU_HDMV_COM_DEBUG(" Write script header place-holder.\n");
  if (writeEsmsHeader(ctx->output.file) < 0)
    goto free_return;

  /* Set options: */
  /* 1. Based on arguments */
  ctx->param.generation_mode       = generation_mode;

  /* 2. Based on presence of IG/PG headers */
  bool next_byte_seg_hdr = isValidHdmvSegmentType(nextUint8(ctx->input.file));
  ctx->param.raw_stream_input_mode = next_byte_seg_hdr;
  ctx->param.force_retiming        = next_byte_seg_hdr;

  /* 3. Based on user input options */
  ctx->param.force_retiming       |= settings->options.hdmv.force_retiming;
  setInitialDelayHdmvContext(ctx, settings->options.hdmv.initialTimestamp);

  /* 4. Based on configuration file */
  if (_initParsingOptionsHdmvContext(&ctx->param.parsing_options, settings) < 0)
    goto free_return;

  if (!ctx->param.force_retiming) {
    /* Using input stream timestamps, do not touch segments order. */
    ctx->param.parsing_options.orderIgsSegByValue = false;
    ctx->param.parsing_options.orderPgsSegByValue = false;
  }

  LIBBLU_HDMV_COM_DEBUG(
    " Parsing settings:\n"
  );
  LIBBLU_HDMV_COM_DEBUG(
    "  - Raw input: %s.\n",
    BOOL_INFO(ctx->param.raw_stream_input_mode)
  );
  if (ctx->param.raw_stream_input_mode)
    LIBBLU_HDMV_COM_DEBUG("   -> Expect timecodes values.\n");
  LIBBLU_HDMV_COM_DEBUG(
    "  - Force computation of timing values: %s.\n",
    BOOL_INFO(ctx->param.force_retiming)
  );
  LIBBLU_HDMV_COM_DEBUG(
    "  - Initial timestamp: %" PRId64 ".\n",
    ctx->param.initial_delay
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
  cleanHdmvSegmentsInventory(ctx->seg_inv);
  cleanHdmvTimecodes(ctx->timecodes);
  free(ctx);
}

int addOriginalFileHdmvContext(
  HdmvContextPtr ctx,
  const lbc * filepath
)
{
  if (appendSourceFileEsmsHandler(ctx->output.script, filepath, NULL) < 0)
    return -1;

  return 0;
}

void _setEsmsHeader(
  HdmvStreamType type,
  HdmvContextParameters param,
  EsmsHandlerPtr script
)
{
  script->PTS_reference    = param.ref_timestamp;
  script->bitrate          = HDMV_RX_BITRATE;
  script->PTS_final        = (param.ref_timestamp + param.last_ds_pres_time);
  script->prop.coding_type = codingTypeHdmvStreamType(type);
}

int closeHdmvContext(
  HdmvContextPtr ctx
)
{
  /* Complete script */
  _setEsmsHeader(ctx->type, ctx->param, ctx->output.script);

  if (completePesCuttingScriptEsmsHandler(ctx->output.file, ctx->output.script) < 0)
    return -1;
  closeBitstreamWriter(ctx->output.file);
  ctx->output.file = NULL;

  /* Update header */
  if (updateEsmsFile(ctx->output.filepath, ctx->output.script) < 0)
    return -1;

  return 0;
}

static HdmvSequencePtr _getPendingSequence(
  HdmvContextPtr ctx,
  hdmv_segtype_idx idx
)
{
  return ctx->ds.cur_ds.sequences[idx].pending;
}

static void _setPendingSequence(
  HdmvContextPtr ctx,
  hdmv_segtype_idx idx,
  HdmvSequencePtr seq
)
{
  ctx->ds.cur_ds.sequences[idx].pending = seq;
}

static int _checkNewDisplaySetTransition(
  HdmvDSState * ds_state,
  HdmvCDParameters new_composition_desc,
  bool * is_dup_DS
)
{
  HdmvCDParameters prev_composition_desc = ds_state->cur_ds.composition_descriptor;

  /* composition_number */
  uint16_t prev_composition_number = prev_composition_desc.composition_number;
  uint16_t new_composition_number = new_composition_desc.composition_number;
  bool same_cnb = (prev_composition_number == new_composition_number);

  if (
    ((prev_composition_number + 1) & 0xFFFF) != new_composition_number
    && !same_cnb
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
  *is_dup_DS = same_cnb;

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
      ctx->ds.sequences[i].head = NULL;
      ctx->ds.sequences[i].last = NULL;
    }

    resetHdmvSegmentsInventory(&ctx->seg_inv);
    resetHdmvContextSegmentTypesCounter(&ctx->nb_seq);
  }

  for (unsigned i = 0; i < HDMV_NB_SEGMENT_TYPES; i++) {
    assert(NULL == ctx->ds.cur_ds.sequences[i].pending);
      // This shall be ensured at DS completion.
    ctx->ds.cur_ds.sequences[i].ds = NULL;
  }

  resetHdmvContextSegmentTypesCounter(&ctx->ds.nb_seq);
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

static int _checkVideoDescriptorChangeHdmvDSState(
  HdmvDSState * epoch,
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
  HdmvDSState * ds_state = &ctx->ds;

  if (!_isInitializedOutputHdmvContext(ctx->output)) {
    LIBBLU_HDMV_COM_ERROR_RETURN(
      "Unable to init a new DS, "
      "HDMV context handle has been closed.\n"
    );
  }

  if (HDMV_DS_INITIALIZED == ds_state->cur_ds.init_usage) {
    /* Presence of an already initialized DS without completion. */
    // TODO: Complete and add END segment
    LIBBLU_HDMV_COM_ERROR_RETURN(
      "Unimplemented automatic completion of not ended DS.\n"
    );
  }

  if (HDMV_DS_COMPLETED <= ds_state->cur_ds.init_usage) {
    /* Check new DS composition descriptor continuation with previous one. */
    if (_checkNewDisplaySetTransition(ds_state, composition_descriptor, &ctx->is_dup_DS) < 0)
      return -1;

    /* Check video_descriptor change */
    if (_checkVideoDescriptorChangeHdmvDSState(ds_state, video_descriptor) < 0)
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

  ds_state->video_descriptor = video_descriptor;

  ds_state->cur_ds.init_usage = HDMV_DS_INITIALIZED;
  ds_state->cur_ds.composition_descriptor = composition_descriptor;

  if (composition_descriptor.composition_state == HDMV_COMPO_STATE_EPOCH_START)
    ctx->nb_epochs++;
  return 0;
}

static void _printDisplaySetContent(
  HdmvDSState ds_state,
  HdmvStreamType type
)
{
  LIBBLU_HDMV_COM_DEBUG(" Current number of segments per type:\n");
#define _G(_i)  getByIdxHdmvContextSegmentTypesCounter(ds_state.nb_seq, _i)
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
    getTotalHdmvContextSegmentTypesCounter(ds_state.nb_seq)
  );
}

static int _checkCompletionOfEachSequence(
  HdmvDSState ds_state
)
{
  bool not_null = false;
  for (hdmv_segtype_idx i = 0; i < HDMV_NB_SEGMENT_TYPES; i++)
    not_null |= (NULL != ds_state.cur_ds.sequences[i].pending);
  return (not_null) ? -1 : 0;
}

static void _getNonCompletedSequencesNames(
  char * dst,
  HdmvDSState ds_state
)
{
  for (hdmv_segtype_idx i = 0; i < HDMV_NB_SEGMENT_TYPES; i++) {
    if (NULL != ds_state.cur_ds.sequences[i].pending) {
      const char * name = segmentTypeIndexStr(i);
      strcpy(dst, name);
      dst += strlen(name);
    }
  }
}

static int _getDisplaySetPts(
  const HdmvContextPtr ctx,
  const HdmvDSState * ds_state,
  int64_t * value
)
{
  int64_t pts;

  switch (ctx->type) {
#define GET_PTS(idx)  ds_state->cur_ds.sequences[idx].ds->segments->param.timestamps_header.pts

    case HDMV_STREAM_TYPE_IGS:
      assert(NULL != ds_state->cur_ds.sequences[HDMV_SEGMENT_TYPE_ICS_IDX].ds);
      pts = GET_PTS(HDMV_SEGMENT_TYPE_ICS_IDX); break;

    case HDMV_STREAM_TYPE_PGS:
      assert(NULL != ds_state->cur_ds.sequences[HDMV_SEGMENT_TYPE_PCS_IDX].ds);
      pts = GET_PTS(HDMV_SEGMENT_TYPE_PCS_IDX); break;

#undef GET_PTS
  }

  pts -= ctx->param.initial_delay;
  if (pts < 0)
    LIBBLU_HDMV_COM_ERROR_RETURN("PTS value is below zero.\n");

  *value = pts;

  return 0;
}

/* Interactive Graphics Stream : */

#define ENABLE_ODS_TRANSFER_DURATION  true

#if ENABLE_ODS_TRANSFER_DURATION

static int32_t _computeODTransferDuration(
  HdmvODParameters od_param
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
   *  OD_DECODE_DURATION : Equation described in computeObjectDataDecodeDuration().
   * \endcode
   */
  return 9 * computeObjectDataDecodeDuration(
    HDMV_STREAM_TYPE_IGS,
    od_param.object_width,
    od_param.object_height
  );
}

#endif

static int64_t _computeICDecodeDuration(
  const HdmvDSState * ds_state
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
   *      return MAX(OBJ_DECODE_DURATION(DS_n), PLANE_CLEAR_TIME(IG_stream_type, DS_n))
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
   *                          Computed using #getHDMVPlaneClearTime().
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
   *                           Described in #computeObjectDataDecodeDuration().
   *  OD_TRANSFER_DURATION() : Object Definition transfer duration, from the
   *                           Object Buffer to the Graphics Plane.
   *                           Described in #_computeODTransferDuration().
   * \endcode
   */

  LIBBLU_HDMV_TSC_DEBUG(
    "    Compute DECODE_DURATION of IG Display Set:\n"
  );

  // TODO: Fix to only count OD present in current Display Set (other are already decoded)

  int64_t odj_dec_duration = 0;
  HdmvSequencePtr seq = getDSSequenceByIdxHdmvDSState(ds_state, HDMV_SEGMENT_TYPE_ODS_IDX);
  for (; NULL != seq; seq = seq->nextSequenceDS) {
    assert(seq->type == HDMV_SEGMENT_TYPE_ODS);

    LIBBLU_HDMV_TSC_DEBUG(
      "      - ODS id=0x%04" PRIX16 " version_number=%" PRIu8 ":\n",
      seq->data.od.object_descriptor.object_id,
      seq->data.od.object_descriptor.object_version_number
    );

    /* OD_DECODE_DURATION() */
    odj_dec_duration += seq->decode_duration = computeObjectDataDecodeDuration(
      HDMV_STREAM_TYPE_IGS,
      seq->data.od.object_width,
      seq->data.od.object_height
    );

    LIBBLU_HDMV_TSC_DEBUG(
      "       => OD_DECODE_DURATION: %" PRId64 "\n",
      seq->decode_duration
    );

#if ENABLE_ODS_TRANSFER_DURATION
    /* OD_TRANSFER_DURATION() */
    if (NULL != seq->nextSequence) {
      odj_dec_duration += seq->transfer_duration = _computeODTransferDuration(
        seq->data.od
      );

      LIBBLU_HDMV_TSC_DEBUG(
        "       => OD_TRANSFER_DURATION: %" PRId64 "\n",
        seq->transfer_duration
      );
    }
#else
    seq->transferDuration = 0;
#endif
  }

  LIBBLU_HDMV_TSC_DEBUG(
    "     => OBJ_DECODE_DURATION: %" PRId64 " ticks.\n",
    odj_dec_duration
  );

  if (isCompoStateHdmvDSState(ds_state, HDMV_COMPO_STATE_EPOCH_START)) {
    int64_t plane_clear_time = getHDMVPlaneClearTime(
      HDMV_STREAM_TYPE_IGS,
      ds_state->video_descriptor.video_width,
      ds_state->video_descriptor.video_height
    );

    LIBBLU_HDMV_TSC_DEBUG(
      "     => PLANE_CLEAR_TIME: %" PRId64 " ticks.\n",
      plane_clear_time
    );

    /* IC_DECODE_DURATION = MAX(PLANE_CLEAR_TIME, OBJ_DECODE_DURATION) */
    return MAX(plane_clear_time, odj_dec_duration);
  }

  /* IC_DECODE_DURATION = OBJ_DECODE_DURATION */
  return odj_dec_duration;
}

static int64_t _computeICTransferDuration(
  const HdmvDSState * ds_state
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
  HdmvSequencePtr seq = getDSSequenceByIdxHdmvDSState(ds_state, HDMV_SEGMENT_TYPE_ICS_IDX);

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

  int64_t transfer_dur = DIV_ROUND_UP(9LL * size, 1600);

  LIBBLU_HDMV_TSC_DEBUG(
    "     => TRANSFER_DURATION: %" PRId64 " ticks.\n",
    transfer_dur
  );

  return transfer_dur;
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
  const HdmvDSState * ds_state
)
{
  return _computeICDecodeDuration(ds_state) + _computeICTransferDuration(ds_state);
}

static void _applyTimestampsIGDisplaySet(
  HdmvDSState * ds_state,
  int64_t pres_time,
  int64_t decode_duration,
  int64_t ref_tc,
  int64_t *end_time
)
{
  int64_t decode_time = pres_time - decode_duration;

  /* ICS */ {
    LIBBLU_HDMV_COM_DEBUG("   - %s:\n", HdmvSegmentTypeStr(HDMV_SEGMENT_TYPE_ICS));

    HdmvSequencePtr seq = getDSSequenceByIdxHdmvDSState(ds_state, HDMV_SEGMENT_TYPE_ICS_IDX);
    assert(isUniqueInDisplaySetHdmvSequence(seq));
    seq->pts = ref_tc + pres_time;
    seq->dts = ref_tc + decode_time;

    LIBBLU_HDMV_COM_DEBUG(
      "    - PTS: %" PRId64 " DTS: %" PRId64 "\n",
      seq->pts - ref_tc,
      seq->dts - ref_tc
    );
  }

  /* PDS */ {
    LIBBLU_HDMV_COM_DEBUG("   - %s:\n", HdmvSegmentTypeStr(HDMV_SEGMENT_TYPE_PDS));

    for (
      HdmvSequencePtr seq = getDSSequenceByIdxHdmvDSState(ds_state, HDMV_SEGMENT_TYPE_PDS_IDX);
      NULL != seq;
      seq = seq->nextSequenceDS
    ) {
      seq->pts = ref_tc + decode_time;
      seq->dts = 0;

      LIBBLU_HDMV_COM_DEBUG(
      "    - PTS: %" PRId64 " DTS: %" PRId64 "\n",
      seq->pts - ref_tc,
      seq->dts - ref_tc
    );
    }
  }

  /* ODS */ {
    LIBBLU_HDMV_COM_DEBUG("   - %s:\n", HdmvSegmentTypeStr(HDMV_SEGMENT_TYPE_ODS));

    for (
      HdmvSequencePtr seq = getDSSequenceByIdxHdmvDSState(ds_state, HDMV_SEGMENT_TYPE_ODS_IDX);
      NULL != seq;
      seq = seq->nextSequenceDS
    ) {
      seq->dts = ref_tc + decode_time;
      decode_time += seq->decode_duration;
      seq->pts = ref_tc + decode_time;
      decode_time += seq->transfer_duration;

      LIBBLU_HDMV_COM_DEBUG(
      "    - PTS: %" PRId64 " DTS: %" PRId64 "\n",
      seq->pts - ref_tc,
      seq->dts - ref_tc
    );
    }

    assert(decode_time <= pres_time); // Might be less
  }

  /* END */ {
    LIBBLU_HDMV_COM_DEBUG("   - %s:\n", HdmvSegmentTypeStr(HDMV_SEGMENT_TYPE_END));

    HdmvSequencePtr seq = getDSSequenceByIdxHdmvDSState(ds_state, HDMV_SEGMENT_TYPE_END_IDX);
    assert(isUniqueInDisplaySetHdmvSequence(seq));
    seq->pts = ref_tc + decode_time;
    seq->dts = 0;

    LIBBLU_HDMV_COM_DEBUG(
      "    - PTS: %" PRId64 " DTS: %" PRId64 "\n",
      seq->pts - ref_tc,
      seq->dts - ref_tc
    );
  }

  *end_time = decode_time;
}

/* Presentation Graphics Stream : */

static int64_t _computePlaneInitializationTime(
  const HdmvDSState * ds_state
)
{
  /** Compute PLANE_INITIALIZATION_TIME of current PG Display Set (DS_n) from
   * a Presentation Graphics Stream, the duration required to initialize
   * the graphical plane before composition objects writing.
   *
   * \code{.unparsed}
   *  PLANE_INITIALIZATION_TIME(DS_n):
   *    if (DS_n.PCS.composition_state == EPOCH_START) {
   *      return PLANE_CLEAR_TIME(PG_stream_type, DS_n)
   *    }
   *    else {
   *      init_duration = 0;
   *      for (i = 0; i < DS_n.WDS.num_windows; i++) {
   *        if (EMPTY(DS_n.WDS.WIN[i])) {
   *          init_duration += ceil(90000 * SIZE(DS_n.WDS.window[i]) / 256000000)
   *        }
   *      }
   *      return init_duration + 1 // Extra tick
   *    }
   *
   * where:
   *  PLANE_CLEAR_TIME() : Graphical plane clearing duration compute function.
   *                       Described in #getHDMVPlaneClearTime().
   *  90000     : PTS/DTS clock frequency (90kHz);
   *  SIZE()    : Size of the window in bits (window_width * window_height * 8).
   *  256000000 : Pixel Transfer Rate (256 Mb/s for PG).
   *
   * note: The 1 tick added to init_duration is to avoid WDS DTS being equal
   * to its PTS, which shall not happen according to the MPEG-2 TS standard.
   * \endcode
   */

  if (isCompoStateHdmvDSState(ds_state, HDMV_COMPO_STATE_EPOCH_START)) {
    /* Epoch start, clear whole graphical plane */
    return getHDMVPlaneClearTime(
      HDMV_STREAM_TYPE_PGS,
      ds_state->video_descriptor.video_width,
      ds_state->video_descriptor.video_height
    );
  }

  /* Not epoch start, clear only empty windows */
  HdmvPCParameters pc = ds_state->sequences[HDMV_SEGMENT_TYPE_PCS_IDX].last->data.pc;
  HdmvWDParameters wd = ds_state->sequences[HDMV_SEGMENT_TYPE_WDS_IDX].last->data.wd;
  int64_t init_duration = 0;

  for (unsigned i = 0; i < wd.number_of_windows; i++) {
    HdmvWindowInfoParameters * window = wd.windows[i];
    bool empty_window = true;

    for (unsigned j = 0; j < pc.number_of_composition_objects; j++) {
      if (window->window_id == pc.composition_objects[j]->window_id_ref) {
        /* A composition object use the window, mark as not empty */
        empty_window = false;
        break;
      }
    }

    if (empty_window) {
      /* Empty window, clear it */
      init_duration += DIV_ROUND_UP(
        9LL * window->window_width * window->window_height,
        3200
      );
    }
  }

  // Add 1 tick delay, observed to avoid WDS DTS being equal to its PTS.
  return init_duration + 1;
}

static int64_t _computeAndSetODSDecodeDuration(
  const HdmvDSState * ds_state,
  uint16_t object_id
)
{

  HdmvSequencePtr ods = getDSSequenceByIdxHdmvDSState(ds_state, HDMV_SEGMENT_TYPE_ODS_IDX);
  for (; NULL != ods; ods = ods->nextSequenceDS) {
    if (object_id == ods->data.od.object_descriptor.object_id)
      break; // Found!
  }

  if (NULL == ods)
    return 0; // !EXISTS(object_id, DS_n)

  int32_t decode_duration = computeObjectDataDecodeDuration(
    HDMV_STREAM_TYPE_PGS,
    ods->data.od.object_width,
    ods->data.od.object_height
  );
  ods->decode_duration   = decode_duration;
  ods->transfer_duration = 0;

  return decode_duration;
}

static int64_t _computeWindowTransferDuration(
  const HdmvDSState * ds_state,
  uint16_t window_id
)
{
  HdmvWDParameters wd = ds_state->sequences[HDMV_SEGMENT_TYPE_WDS_IDX].last->data.wd;

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
  const HdmvDSState * ds_state
)
{
  HdmvSequencePtr wds = ds_state->sequences[HDMV_SEGMENT_TYPE_WDS_IDX].last;
  assert(isUniqueInDisplaySetHdmvSequence(wds));

  const HdmvWDParameters wd = wds->data.wd;

  int64_t size = 0;
  for (unsigned i = 0; i < wd.number_of_windows; i++) {
    const HdmvWindowInfoParameters * window = wd.windows[i];
    size += window->window_width * window->window_height;
  }

  /* ceil(90000 * 8 * size / 3200) */
  int64_t drawing_duration = DIV_ROUND_UP(9LL * size, 3200);

  wds->decode_duration = drawing_duration;
  return drawing_duration;
}

static int64_t _computePGDisplaySetDecodeDuration(
  const HdmvDSState * ds_state
)
{
  /** Compute this PCS from Display Set n (DS_n) PTS-DTS difference required.
   * This value is the minimal required initialization duration PGS DS_n.
   *
   * Compute the minimal delta between Presentation Composition Segment (PCS)
   * DTS and PTS values. Aka the amount of time required to decode and transfer
   * DS graphical objects from Coded Data Buffer to the Decoded Object Buffer
   * plus to draw the composition windows on the Graphical Plane.
   */

  LIBBLU_HDMV_TSC_DEBUG(
    "    Compute DECODE_DURATION of PG Display Set:\n"
  );

  int64_t decode_duration = _computePlaneInitializationTime(ds_state);
    // Time required to clear Graphical Plane (or empty Windows).

  LIBBLU_HDMV_TSC_DEBUG(
    "     => PLANE_INITIALIZATION_TIME(DS_n): %" PRId64 " ticks.\n",
    decode_duration
  );

  HdmvPCParameters pc = ds_state->sequences[HDMV_SEGMENT_TYPE_PCS_IDX].last->data.pc;

  if (2 == pc.number_of_composition_objects) {
    const HdmvCOParameters * obj0 = pc.composition_objects[0];
    int64_t obj_0_decode_duration = _computeAndSetODSDecodeDuration(ds_state, obj0->object_id_ref);

    LIBBLU_HDMV_TSC_DEBUG(
      "      => ODS_DECODE_DURATION(DS_n.PCS.OBJ[0]): %" PRId64 " ticks.\n",
      obj_0_decode_duration
    );

    const HdmvCOParameters * obj1 = pc.composition_objects[1];
    int64_t obj_1_decode_duration = _computeAndSetODSDecodeDuration(ds_state, obj1->object_id_ref);

    LIBBLU_HDMV_TSC_DEBUG(
      "      => ODS_DECODE_DURATION(DS_n.PCS.OBJ[1]): %" PRId64 " ticks.\n",
      obj_1_decode_duration
    );

    decode_duration = MAX(decode_duration, obj_0_decode_duration);

    if (obj0->window_id_ref == obj1->window_id_ref) {
      decode_duration = MAX(decode_duration, obj_0_decode_duration + obj_1_decode_duration);

      int64_t obj_0_transfer_duration = _computeWindowTransferDuration(ds_state, obj0->window_id_ref);

      LIBBLU_HDMV_TSC_DEBUG(
        "      => WINDOW_TRANSFER_DURATION(DS_n.PCS.OBJ[0]): %" PRId64 " ticks.\n",
        obj_0_transfer_duration
      );

      decode_duration += obj_0_transfer_duration;
    }
    else {
      int64_t obj_0_transfer_duration = _computeWindowTransferDuration(ds_state, obj0->window_id_ref);

      LIBBLU_HDMV_TSC_DEBUG(
        "      => WINDOW_TRANSFER_DURATION(DS_n.PCS.OBJ[0]): %" PRId64 " ticks.\n",
        obj_0_transfer_duration
      );

      decode_duration += obj_0_transfer_duration;
      decode_duration = MAX(decode_duration, obj_0_decode_duration + obj_1_decode_duration);

      int64_t obj_1_transfer_duration = _computeWindowTransferDuration(ds_state, obj1->window_id_ref);

      LIBBLU_HDMV_TSC_DEBUG(
        "      => WINDOW_TRANSFER_DURATION(DS_n.PCS.OBJ[1]): %" PRId64 " ticks.\n",
        obj_1_transfer_duration
      );

      decode_duration += obj_1_transfer_duration;
    }
  }
  else if (1 == pc.number_of_composition_objects) {
    const HdmvCOParameters * obj0 = pc.composition_objects[0];
    int64_t obj_0_decode_duration = _computeAndSetODSDecodeDuration(ds_state, obj0->object_id_ref);

    LIBBLU_HDMV_TSC_DEBUG(
      "     => ODS_DECODE_DURATION(DS_n.PCS.OBJ[0]): %" PRId64 " ticks.\n",
      obj_0_decode_duration
    );

    decode_duration = MAX(decode_duration, obj_0_decode_duration);

    int64_t obj_0_transfer_duration = _computeWindowTransferDuration(ds_state, obj0->window_id_ref);

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

  int64_t min_drawing_duration = _setWindowDrawingDuration(ds_state);

  LIBBLU_HDMV_TSC_DEBUG(
    "     => MIN_DRAWING_DURATION(DS_n.WDS): %" PRId64 " ticks.\n",
    min_drawing_duration
  );

  return decode_duration;
}

static void _applyTimestampsPGDisplaySet(
  HdmvDSState * epoch,
  int64_t pres_time,
  int64_t decode_duration,
  int64_t ref_tc,
  int64_t *end_time
)
{
  int64_t decode_time = pres_time - decode_duration;

  /* PCS */ {
    LIBBLU_HDMV_COM_DEBUG("    - %s:\n", HdmvSegmentTypeStr(HDMV_SEGMENT_TYPE_PCS));

    HdmvSequencePtr seq = getDSSequenceByIdxHdmvDSState(epoch, HDMV_SEGMENT_TYPE_PCS_IDX);
    assert(isUniqueInDisplaySetHdmvSequence(seq));
    seq->pts = ref_tc + pres_time;
    seq->dts = ref_tc + decode_time;

    LIBBLU_HDMV_COM_DEBUG(
      "     - PTS: %" PRId64 " DTS: %" PRId64 "\n",
      seq->pts - ref_tc,
      seq->dts - ref_tc
    );
  }

  /* WDS */ {
    LIBBLU_HDMV_COM_DEBUG("    - %s:\n", HdmvSegmentTypeStr(HDMV_SEGMENT_TYPE_WDS));

    HdmvSequencePtr seq = getDSSequenceByIdxHdmvDSState(epoch, HDMV_SEGMENT_TYPE_WDS_IDX);
    if (NULL != seq) {
      assert(isUniqueInDisplaySetHdmvSequence(seq));
      seq->pts = ref_tc + pres_time - seq->decode_duration;
      seq->dts = ref_tc + decode_time;

      LIBBLU_HDMV_COM_DEBUG(
        "     - PTS: %" PRId64 " DTS: %" PRId64 "\n",
        seq->pts - ref_tc,
        seq->dts - ref_tc
      );
    }
  }

  /* PDS */ {
    LIBBLU_HDMV_COM_DEBUG("    - %s:\n", HdmvSegmentTypeStr(HDMV_SEGMENT_TYPE_PDS));

    for (
      HdmvSequencePtr seq = getDSSequenceByIdxHdmvDSState(epoch, HDMV_SEGMENT_TYPE_PDS_IDX);
      NULL != seq;
      seq = seq->nextSequenceDS
    ) {
      seq->pts = ref_tc + decode_time;
      seq->dts = 0;

      LIBBLU_HDMV_COM_DEBUG(
        "     - PTS: %" PRId64 " DTS: %" PRId64 "\n",
        seq->pts - ref_tc,
        seq->dts - ref_tc
      );
    }
  }

  /* ODS */ {
    LIBBLU_HDMV_COM_DEBUG("    - %s:\n", HdmvSegmentTypeStr(HDMV_SEGMENT_TYPE_ODS));

    for (
      HdmvSequencePtr seq = getDSSequenceByIdxHdmvDSState(epoch, HDMV_SEGMENT_TYPE_ODS_IDX);
      NULL != seq;
      seq = seq->nextSequenceDS
    ) {
      seq->dts = ref_tc + decode_time;
      decode_time += seq->decode_duration;
      seq->pts = ref_tc + decode_time;
      decode_time += seq->transfer_duration;

      LIBBLU_HDMV_COM_DEBUG(
        "     - PTS: %" PRId64 " DTS: %" PRId64 "\n",
        seq->pts - ref_tc,
        seq->dts - ref_tc
      );
    }

    assert(decode_time <= pres_time); // Might be less
  }

  /* END */ {
    LIBBLU_HDMV_COM_DEBUG("    - %s:\n", HdmvSegmentTypeStr(HDMV_SEGMENT_TYPE_END));

    HdmvSequencePtr seq = getDSSequenceByIdxHdmvDSState(epoch, HDMV_SEGMENT_TYPE_END_IDX);
    assert(isUniqueInDisplaySetHdmvSequence(seq));
    seq->pts = ref_tc + decode_time;
    seq->dts = 0;

    LIBBLU_HDMV_COM_DEBUG(
      "     - PTS: %" PRId64 " DTS: %" PRId64 "\n",
      seq->pts - ref_tc,
      seq->dts - ref_tc
    );
  }

  *end_time = decode_time;
}

#define ENABLE_DEBUG_TIMESTAMPS  true

#if ENABLE_DEBUG_TIMESTAMPS

static void _debugCompareComputedTimestampsDisplaySet(
  const HdmvContextPtr ctx,
  const HdmvDSState * ds_state
)
{
  int64_t ref_tc = ctx->param.ref_timestamp;
  bool issue = false;

  for (hdmv_segtype_idx idx = 0; idx < HDMV_NB_SEGMENT_TYPES; idx++) {
    HdmvSequencePtr seq = ds_state->cur_ds.sequences[idx].ds;

    for (unsigned i = 0; NULL != seq; seq = seq->nextSequenceDS, i++) {
      LIBBLU_HDMV_COM_DEBUG("    - %s:\n", HdmvSegmentTypeStr(seq->type));
      LIBBLU_HDMV_COM_DEBUG(
        "     - PTS: %" PRId64 " DTS: %" PRId64 "\n",
        seq->pts - ref_tc,
        seq->dts - ref_tc
      );

      int64_t dts = seq->dts - ref_tc + ctx->param.initial_delay;

      if (seq->dts != 0 && dts != seq->segments->param.timestamps_header.dts) {
        LIBBLU_HDMV_TSC_WARNING(
          " Mismatch %s-%u DTS: exp %" PRId32 " got %" PRId64 ".\n",
          HdmvSegmentTypeStr(seq->type), i,
          seq->segments->param.timestamps_header.dts,
          dts
        );
        issue = true;
      }

      int64_t pts = seq->pts - ref_tc + ctx->param.initial_delay;

      if (pts != seq->segments->param.timestamps_header.pts) {
        LIBBLU_HDMV_TSC_WARNING(
          " Mismatch %s-%u PTS: exp %" PRId32 " got %" PRId64 ".\n",
          HdmvSegmentTypeStr(seq->type), i,
          seq->segments->param.timestamps_header.pts,
          pts
        );
        issue = true;
      }
    }
  }

  if (issue) {
    LIBBLU_HDMV_TSC_WARNING(
      "Issue with DS #%u, composition_number: %u, composition_state: %s.\n",
      ctx->nb_DS,
      ds_state->cur_ds.composition_descriptor.composition_number,
      HdmvCompositionStateStr(ds_state->cur_ds.composition_descriptor.composition_state)
    );
  }
}

#endif

static int _computeTimestampsDisplaySet(
  HdmvContextPtr ctx
)
{
  HdmvDSState * ds_state = &ctx->ds;
  int64_t pres_time;
  int ret;

  /* get Display Set presentation timecode */
  if (ctx->param.raw_stream_input_mode)
    ret = getHdmvTimecodes(&ctx->timecodes, &pres_time);
  else
    ret = _getDisplaySetPts(ctx, ds_state, &pres_time);
  if (ret < 0)
    return -1;

  LIBBLU_HDMV_COM_DEBUG(
    "   Requested presentation time: %" PRId64 " ticks.\n",
    pres_time
  );

  int64_t decode_dur;
  switch (ctx->type) {
    case HDMV_STREAM_TYPE_IGS:
      decode_dur = _computeIGDisplaySetDecodeDuration(ds_state); break;
    case HDMV_STREAM_TYPE_PGS:
      decode_dur = _computePGDisplaySetDecodeDuration(ds_state); break;
  }

  LIBBLU_HDMV_COM_DEBUG(
    "   DS decode duration: %" PRId64 " ticks.\n",
    decode_dur
  );

  if (0 == ctx->nb_DS) {
    /* First display set, init reference clock to handle negative timestamps. */
    if (pres_time < decode_dur) {
      LIBBLU_HDMV_COM_DEBUG(
        "   Set reference timecode: %" PRId64 " ticks.\n",
        decode_dur - pres_time
      );

      ctx->param.ref_timestamp = decode_dur - pres_time;
    }
  }
  else {
    /* Not the first DS, check its decoding period does not overlap previous DS one. */
    if (pres_time - decode_dur < ctx->param.last_ds_pres_time) {
      if (ctx->type == HDMV_STREAM_TYPE_IGS)
        LIBBLU_HDMV_COM_ERROR_RETURN(
          "Invalid timecode, decoding periods overlap is forbidden for IGS.\n"
        );

      if (pres_time - decode_dur < ctx->param.last_ds_end_time)
        LIBBLU_HDMV_COM_ERROR_RETURN(
          "Invalid timecode, decoding period starts before the end of the "
          "previous DS one (%" PRId64 " - %" PRId64 " < %" PRId64 ").\n",
          pres_time, decode_dur, ctx->param.last_ds_end_time
        );
    }
  }

  int64_t ref_tc = ctx->param.ref_timestamp;
  int64_t end_time;

  switch (ctx->type) {
    case HDMV_STREAM_TYPE_IGS:
      _applyTimestampsIGDisplaySet(ds_state, pres_time, decode_dur, ref_tc, &end_time); break;
    case HDMV_STREAM_TYPE_PGS:
      _applyTimestampsPGDisplaySet(ds_state, pres_time, decode_dur, ref_tc, &end_time); break;
  }

#if ENABLE_DEBUG_TIMESTAMPS
  if (!ctx->param.raw_stream_input_mode) {
    LIBBLU_HDMV_COM_DEBUG("   Compare with original timestamps:\n");
    _debugCompareComputedTimestampsDisplaySet(ctx, ds_state);
  }
#endif

  ctx->param.last_ds_pres_time = pres_time;
  ctx->param.last_ds_end_time  = end_time;
  return 0;
}

static void _copySegmentsTimestampsDisplaySetHdmvContext(
  HdmvContextPtr ctx
)
{
  int64_t time_offset = ctx->param.ref_timestamp - ctx->param.initial_delay;
  int64_t last_pts = ctx->param.last_ds_pres_time;

  for (hdmv_segtype_idx idx = 0; idx < HDMV_NB_SEGMENT_TYPES; idx++) {
    bool displayedListName = false;

    HdmvSequencePtr seq = getDSSequenceByIdxHdmvDSState(&ctx->ds, idx);
    for (; NULL != seq; seq = seq->nextSequenceDS) {
      HdmvSegmentParameters segment = seq->segments->param;

      seq->pts = segment.timestamps_header.pts + time_offset;
      if (0 < segment.timestamps_header.dts)
        seq->dts = segment.timestamps_header.dts + time_offset;
      else
        seq->dts = 0;

      last_pts = MAX(last_pts, segment.timestamps_header.pts + time_offset);

      if (!displayedListName) {
        LIBBLU_HDMV_COM_DEBUG("   - %s:\n", HdmvSegmentTypeStr(seq->type));
        displayedListName = true;
      }

      LIBBLU_HDMV_COM_DEBUG(
        "    - PTS: %" PRId64 " DTS: %" PRId64 "\n",
        seq->pts - ctx->param.ref_timestamp,
        seq->dts - ctx->param.ref_timestamp
      );
    }
  }

  ctx->param.last_ds_pres_time = last_pts;
}

int _setTimestampsDisplaySet(
  HdmvContextPtr ctx
)
{
  if (ctx->param.force_retiming) {
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

  bool dts_present = (
    seg.desc.segment_type == HDMV_SEGMENT_TYPE_ODS
    || seg.desc.segment_type == HDMV_SEGMENT_TYPE_PCS
    || seg.desc.segment_type == HDMV_SEGMENT_TYPE_WDS
    || seg.desc.segment_type == HDMV_SEGMENT_TYPE_ICS
  );

  if (initHDMVPesPacketEsmsHandler(ctx->output.script, dts_present, pts, dts) < 0)
    return -1;

  int ret = appendAddPesPayloadCommandEsmsHandler(
    ctx->output.script,
    ctx->input.idx,
    0x0,
    seg.orig_file_offset,
    seg.desc.segment_length + HDMV_SEGMENT_HEADER_LENGTH
  );
  if (ret < 0)
    return -1;

  return writePesPacketEsmsHandler(
    ctx->output.file,
    ctx->output.script
  );
}

static int _registeringSegmentsDisplaySet(
  HdmvContextPtr ctx,
  HdmvDSState * epoch
)
{

  for (hdmv_segtype_idx idx = 0; idx < HDMV_NB_SEGMENT_TYPES; idx++) {
    HdmvSequencePtr seq = getDSSequenceByIdxHdmvDSState(epoch, idx);

    for (; NULL != seq; seq = seq->nextSequenceDS) {
      HdmvSegmentPtr seg;

      uint64_t pts = (uint64_t) seq->pts;
      uint64_t dts = (uint64_t) seq->dts;

      assert(segmentTypeIndexHdmvContext(seq->type) == idx);

      for (seg = seq->segments; NULL != seg; seg = seg->nextSegment) {
        if (_insertSegmentInScript(ctx, seg->param, pts, dts) < 0)
          return -1;
      }
    }
  }

  return 0;
}

int completeDSHdmvContext(
  HdmvContextPtr ctx
)
{

  if (ctx->ds.cur_ds.init_usage == HDMV_DS_COMPLETED)
    return 0; /* Already completed */

  LIBBLU_HDMV_COM_DEBUG("Processing complete Display Set...\n");
  _printDisplaySetContent(ctx->ds, ctx->type);

  if (_checkCompletionOfEachSequence(ctx->ds)) {
    /* Presence of non completed sequences, building list string. */
    char names[HDMV_NB_SEGMENT_TYPES * SEGMENT_TYPE_IDX_STR_SIZE];
    _getNonCompletedSequencesNames(names, ctx->ds);

    LIBBLU_HDMV_COM_ERROR_RETURN(
      "Non-completed sequence(s) present in DS: %s.\n",
      names
    );
  }

  if (!getNbSequencesHdmvDSState(&ctx->ds)) {
    LIBBLU_HDMV_COM_DEBUG(" Empty Display Set, skipping.\n");
    return 0;
  }

  if (
    ctx->is_dup_DS
    && !isCompoStateHdmvDSState(&ctx->ds, HDMV_COMPO_STATE_NORMAL)
  ) {
    // FIXME: Implement accurate way to check same composition_number.
    // Currently DTS/PTS is broken.
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
    unsigned last_DS_idx = ctx->nb_DS - 1;

    if (checkDuplicatedDSHdmvDSState(&ctx->ds, last_DS_idx) < 0)
      return -1;
  }
  else {
    int ret;
    LIBBLU_HDMV_CK_DEBUG(" Checking and building Display Set:\n");

    /* Check Epoch Objects Buffers */
    if (checkObjectsBufferingHdmvDSState(&ctx->ds, ctx->type) < 0)
      return -1;

    /* Build the display set */
    ret = checkAndBuildDisplaySetHdmvDSState(
      ctx->param.parsing_options,
      ctx->type,
      &ctx->ds,
      ctx->nb_DS
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
    getTotalHdmvContextSegmentTypesCounter(ctx->ds.nb_seq)
  );

  if (_registeringSegmentsDisplaySet(ctx, &ctx->ds) < 0)
    return -1;

  ctx->ds.cur_ds.init_usage = HDMV_DS_COMPLETED;

  LIBBLU_HDMV_COM_DEBUG(" Done.\n");
  ctx->nb_DS++;
  return 0;
}

static HdmvSequencePtr _initNewSequenceInCurEpoch(
  HdmvContextPtr ctx,
  hdmv_segtype_idx idx,
  HdmvSegmentParameters segment
)
{
  /* Fetch the (potential) pending sequence slot */
  HdmvSequencePtr seq = _getPendingSequence(ctx, idx);
  if (NULL != seq) {
    /* Not NULL, a non completed sequence is already pending */
    LIBBLU_HDMV_COM_ERROR_NRETURN(
      "Missing a sequence completion segment, "
      "last sequence never completed.\n"
    );
  }

  /* Check the number of already parsed sequences */
  unsigned seq_type_max_nb_DS = ctx->seq_nb_limit_per_DS[idx];
  unsigned seq_type_cur_nb_DS = getByIdxHdmvContextSegmentTypesCounter(
    ctx->ds.nb_seq,
    idx
  );

  if (seq_type_max_nb_DS <= seq_type_cur_nb_DS) {
    if (!seq_type_max_nb_DS)
      LIBBLU_HDMV_COM_ERROR_NRETURN(
        "Forbidden %s segment type in a %s stream.\n",
        HdmvSegmentTypeStr(segment.desc.segment_type),
        HdmvStreamTypeStr(ctx->type)
      );
    LIBBLU_HDMV_COM_ERROR_NRETURN(
      "Too many %ss in one display set.\n",
      HdmvSegmentTypeStr(segment.desc.segment_type)
    );
  }

  /* Creating a new orphan sequence */
  if (NULL == (seq = getHdmvSequenceHdmvSegmentsInventory(&ctx->seg_inv)))
    return NULL;
  seq->type = segment.desc.segment_type;

  /* Set as the pending sequence */
  _setPendingSequence(ctx, idx, seq);

  return seq;
}

HdmvSequencePtr addSegToSeqDisplaySetHdmvContext(
  HdmvContextPtr ctx,
  hdmv_segtype_idx idx,
  HdmvSegmentParameters seg_param,
  HdmvSequenceLocation location
)
{
  assert(isValidSegmentTypeIndexHdmvContext(idx));

  HdmvSequencePtr seq;
  if (isFirstSegmentHdmvSequenceLocation(location)) {
    /* First segment in sequence, initiate a new sequence in current epoch */
    if (NULL == (seq = _initNewSequenceInCurEpoch(ctx, idx, seg_param)))
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
  HdmvSegmentPtr seg = getHdmvSegmentHdmvSegmentsInventory(&ctx->seg_inv);
  if (NULL == seg)
    return NULL;
  seg->param = seg_param;

  /* Attach the newer sequence (to the last sequence in list) */
  if (NULL == seq->segments) /* Or as the new list header if its empty */
    seq->segments = seg;
  else
    seq->lastSegment->nextSegment = seg;
  seq->lastSegment = seg;

  return seq;
}

static uint32_t _getIdSequence(
  const HdmvSequencePtr sequence
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
  const HdmvSequencePtr src
)
{
  assert(dst->type == src->type);

  switch (dst->type) {
    case HDMV_SEGMENT_TYPE_PDS:
      if (updateHdmvPDSegmentParameters(&dst->data.pd, &src->data.pd) < 0)
        return -1;
      break;
    case HDMV_SEGMENT_TYPE_ODS:
      if (updateHdmvObjectDataParameters(&dst->data.od, &src->data.od) < 0)
        return -1;
      break;
    case HDMV_SEGMENT_TYPE_PCS:
      if (updateHdmvPCParameters(&dst->data.pc, &src->data.pc) < 0)
        return -1;
      break;
    case HDMV_SEGMENT_TYPE_WDS:
      if (updateHdmvWDParameters(&dst->data.wd, &src->data.wd) < 0)
        return -1;
      break;
    case HDMV_SEGMENT_TYPE_ICS:
      if (updateHdmvICParameters(&dst->data.ic, &src->data.ic) < 0)
        return -1;
      break;
    default:
      LIBBLU_HDMV_COM_ERROR_RETURN(
        "Broken program, shall not attempt to update segment of type %s.\n",
        HdmvSegmentTypeStr(dst->type)
      );
  }

  dst->nextSequenceDS    = NULL;
  dst->displaySetIdx     = src->displaySetIdx;
  dst->segments          = src->segments;
  dst->lastSegment       = src->lastSegment;
  dst->pts               = 0;
  dst->dts               = 0;
  dst->decode_duration   = 0;
  dst->transfer_duration = 0;

  return 0;
}

// static bool _isPaletteUpdatePCSHdmvSequence(
//   const HdmvSequencePtr seq
// )
// {
//   assert(HDMV_SEGMENT_TYPE_PCS == seq->type);
//   return seq->data.pc.palette_update_flag;
// }

static bool _isUpdatableSequence(
  const HdmvSequencePtr dst,
  const HdmvSequencePtr src
)
{
  assert(dst->type == src->type);

  switch (dst->type) {
    case HDMV_SEGMENT_TYPE_PDS:
      return _getIdSequence(dst) == _getIdSequence(src);
    case HDMV_SEGMENT_TYPE_ODS:
      return _getIdSequence(dst) == _getIdSequence(src);
    case HDMV_SEGMENT_TYPE_PCS:
      return true;  // Always check for update PCS
    case HDMV_SEGMENT_TYPE_WDS:
      return true;  // Always check for same values WDS
    case HDMV_SEGMENT_TYPE_ICS:
      return true;  // Always check for update ICS
    case HDMV_SEGMENT_TYPE_END:
      return false; // Never check END
    default:
      break;
  }

  LIBBLU_HDMV_COM_ERROR_RETURN(
    "Broken program, unknown segment type 0x%02" PRIX8 ".\n",
    dst->type
  );
}

static int _applySequenceUpdate(
  HdmvSequencePtr dst,
  const HdmvSequencePtr src
)
{

  for (; NULL != dst; dst = dst->nextSequence) {
    if (_isUpdatableSequence(dst, src)) {
      if (_updateSequence(dst, src) < 0)
        return -1;
      return 1;
    }
  }

  return 0;
}

// static int _checkNumberOfSequencesInEpoch(
//   HdmvContextPtr ctx,
//   hdmv_segtype_idx idx,
//   HdmvSegmentType type
// )
// {
//   unsigned seq_type_max_nb_epoch = ctx->seq_nb_limit_per_epoch[idx];
//   unsigned seq_type_cur_nb_epoch = getByIdxHdmvContextSegmentTypesCounter(
//     ctx->nb_seq,
//     idx
//   );

//   if (seq_type_max_nb_epoch <= seq_type_cur_nb_epoch) {
//     if (!seq_type_max_nb_epoch)
//       LIBBLU_HDMV_COM_ERROR_RETURN(
//         "Forbidden %s segment type in a %s stream.\n",
//         HdmvSegmentTypeStr(type),
//         HdmvStreamTypeStr(ctx->type)
//       );
//     LIBBLU_HDMV_COM_ERROR_RETURN(
//       "Too many %ss in one ds.\n",
//       HdmvSegmentTypeStr(type)
//     );
//   }

//   return 0;
// }

int completeSeqDisplaySetHdmvContext(
  HdmvContextPtr ctx,
  hdmv_segtype_idx idx
)
{
  /* Check if index correspond to a valid pending sequence */
  HdmvSequencePtr sequence = _getPendingSequence(ctx, idx);
  if (NULL == sequence)
    LIBBLU_HDMV_COM_ERROR_RETURN("No sequence to complete.\n");
  sequence->displaySetIdx = ctx->nb_DS;

  /* Get the header of completed sequences list of DS */
  HdmvSequencePtr seq_list_hdr = ctx->ds.cur_ds.sequences[idx].ds;

  if (ctx->is_dup_DS) {
    /* Special case, this DS is supposed to be a duplicate of the previous one. */
    if (_insertDuplicatedSequence(seq_list_hdr, sequence) < 0)
      return -1;

    /* Update Display Order if not identical. */
    // TODO: Does duplicated sequences must be identical ? If so use DO to check.

    goto success;
  }

  if (NULL == seq_list_hdr) {
    /* If the list is empty, the pending is the new header */
    ctx->ds.cur_ds.sequences[idx].ds_head = sequence;
    ctx->ds.cur_ds.sequences[idx].ds_last = sequence;

    incrementSequencesNbEpochHdmvContext(ctx, idx);
    goto success;
  }

  /* Insert in list */
  /* Check if sequence is an update of a previous DS */
  int ret = _applySequenceUpdate(seq_list_hdr, sequence);
  if (ret < 0)
    return -1;

  if (!ret) {
    /* The sequence is not an update, insert in list */

    // /* Check the number of sequences in current Epoch */
    // if (_checkNumberOfSequencesInEpoch(ctx, idx, seq_list_hdr->type) < 0)
    //   return -1;

    ctx->ds.cur_ds.sequences[idx].ds_last->nextSequence = sequence;
    ctx->ds.sequences[idx].last->nextSequence = sequence;
    ctx->ds.sequences[idx].last = sequence;

    incrementSequencesNbEpochHdmvContext(ctx, idx);
  }

success:
  incrementSequencesNbDSHdmvContext(ctx, idx);
  /* Reset pending sequence */
  _setPendingSequence(ctx, idx, NULL);
  return 0;
}