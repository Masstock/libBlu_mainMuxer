#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
  HdmvContext * ctx
)
{
  LIBBLU_HDMV_COM_DEBUG(
    " Initialization of limits.\n"
  );

  static const unsigned seg_nb_limits_epoch[][HDMV_NB_SEGMENT_TYPES] = {
    {1, 0, 0, 256, HDMV_IG_MAX_NB_OBJ, 1}, /* IG */
    {0, 1, 8,   8, HDMV_PG_MAX_NB_OBJ, 8}  /* PG */
  };
  static const unsigned seg_nb_limits_DS[][HDMV_NB_SEGMENT_TYPES] = {
    {1, 0, 0, 256, HDMV_IG_MAX_NB_OBJ, 1}, /* IG */
    {0, 1, 1,   8, HDMV_PG_MAX_NB_OBJ, 1}  /* PG */
  };

  assert(ctx->epoch.type < ARRAY_SIZE(seg_nb_limits_epoch));

  for (hdmv_segtype_idx idx = 0; idx < HDMV_NB_SEGMENT_TYPES; idx++) {
    ctx->seq_nb_limit_per_epoch[idx] = seg_nb_limits_epoch[ctx->epoch.type][idx];
    ctx->seq_nb_limit_per_DS   [idx] = seg_nb_limits_DS   [ctx->epoch.type][idx];

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
  const IniFileContext conf_hdl = settings->options.conf_hdl;
  lbc * string;

  /* Default : */
  options_dst->igs_od_decode_delay_factor = 0.f;
  options_dst->igs_use_earlier_transfer   = false;
  options_dst->enable_timestamps_debug    = false;

  string = lookupIniFile(conf_hdl, "HDMV.IGSOBJECTDECODEDELAYFACTOR");
  if (NULL != string) {
    lbc * end_ptr;
    float value = lbc_strtof(string, &end_ptr);

    if (end_ptr == string || errno == ERANGE || value < 0.f || 10.f < value)
      LIBBLU_ERROR_RETURN(
        "Invalid float value setting 'igsObjectDecodeDelayFactor' in section "
        "[HDMV] of INI file.\n"
      );

    options_dst->igs_od_decode_delay_factor = value;
  }

  string = lookupIniFile(conf_hdl, "HDMV.IGSUSEEARLIERTRANSFER");
  if (NULL != string) {
    bool value;

    if (lbc_atob(&value, string) < 0)
      LIBBLU_ERROR_RETURN(
        "Invalid boolean value setting 'IgsUseEarlierTransfer' in section "
        "[HDMV] of INI file.\n"
      );

    options_dst->igs_use_earlier_transfer = value;
  }

  string = lookupIniFile(conf_hdl, "HDMV.DEBUGTIMESTAMPS");
  if (NULL != string) {
    bool value;

    if (lbc_atob(&value, string) < 0)
      LIBBLU_ERROR_RETURN(
        "Invalid boolean value setting 'debugTimestamps' in section "
        "[HDMV] of INI file.\n"
      );

    options_dst->enable_timestamps_debug = value;
  }

  return 0;
}

HdmvContext * createHdmvContext(
  LibbluESParsingSettings * settings,
  const lbc * infilepath,
  HdmvStreamType type,
  bool generation_mode
)
{
  TOC();

  assert(NULL != settings);

  if (NULL == infilepath)
    infilepath = settings->esFilepath;

  LIBBLU_HDMV_COM_DEBUG(
    "Creation of the HDMV context for %s stream type.\n",
    HdmvStreamTypeStr(type)
  );
  LIBBLU_HDMV_COM_DEBUG(
    " Input file: '%s'.\n",
    infilepath
  );

  HdmvContext * ctx = malloc(sizeof(HdmvContext));
  if (NULL == ctx)
    LIBBLU_HDMV_COM_ERROR_NRETURN("Memory allocation error.\n");
  *ctx = (HdmvContext) {
    .param = {
      .generation_mode = generation_mode
    },
    .cur_segment_type = -1,
    .epoch.type = type
  };

  if (_initOutputHdmvContext(&ctx->output, settings) < 0)
    goto free_return;
  if (_initInputHdmvContext(&ctx->input, ctx->output, infilepath) < 0)
    goto free_return;
  _initSequencesLimit(ctx);

  /* Add script header place holder (empty header): */
  LIBBLU_HDMV_COM_DEBUG(" Write script header place-holder.\n");
  if (writeEsmsHeader(ctx->output.file) < 0)
    goto free_return;

  /* Set options: */
  /* 1. Based on presence of IG/PG headers */
  bool next_byte_seg_hdr = isValidHdmvSegmentType(nextUint8(ctx->input.file));
  ctx->param.raw_stream_input_mode = next_byte_seg_hdr;
  ctx->param.force_retiming        = next_byte_seg_hdr;

  /* 2. Based on configuration file */
  if (_initParsingOptionsHdmvContext(&ctx->options, settings) < 0)
    goto free_return;

  /* 3. Based on user input options */
  ctx->param.force_retiming       |= settings->options.hdmv.force_retiming;
  ctx->param.initial_delay         = settings->options.hdmv.initial_timestamp;

  LIBBLU_HDMV_COM_DEBUG(
    " Parsing settings summary:\n"
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
  HdmvContext * ctx
)
{
  if (NULL == ctx)
    return;
  _cleanOutputHdmvContext(ctx->output);
  _cleanInputHdmvContext(ctx->input);
  cleanHdmvTimecodes(ctx->timecodes);
  cleanHdmvDSState(ctx->ds[0]);
  cleanHdmvDSState(ctx->ds[1]);
  cleanHdmvEpochState(ctx->epoch);
  free(ctx);
}

int addOriginalFileHdmvContext(
  HdmvContext * ctx,
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
  HdmvContext * ctx
)
{
  /* Complete script */
  _setEsmsHeader(ctx->epoch.type, ctx->param, ctx->output.script);

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
  HdmvContext * ctx,
  hdmv_segtype_idx idx
)
{
  return getCurDSHdmvContext(ctx)->sequences[idx].pending;
}

static void _setPendingSequence(
  HdmvContext * ctx,
  hdmv_segtype_idx idx,
  HdmvSequencePtr seq
)
{
  getCurDSHdmvContext(ctx)->sequences[idx].pending = seq;
}

static int _checkNewDisplaySetTransition(
  const HdmvDSState * prev_ds_state,
  HdmvCDParameters new_compo_desc,
  bool * is_dup_DS_ret
)
{
  HdmvCDParameters prev_compo_desc = prev_ds_state->compo_desc;

  /* composition_number */
  uint16_t prev_compo_nb = prev_compo_desc.composition_number;
  uint16_t new_compo_nb  = new_compo_desc.composition_number;

  bool is_same_compo_nb   = (  prev_compo_nb                == new_compo_nb);
  bool is_update_compo_nb = (((prev_compo_nb + 1) & 0xFFFF) == new_compo_nb);

  if (!is_same_compo_nb && !is_update_compo_nb)
    LIBBLU_HDMV_PARSER_ERROR_RETURN(
      "Invalid composition segment, "
      "unexpected 'composition_number' not equal or incremented correctly "
      "(old: 0x%04X / new: 0x%04X).\n",
      prev_compo_nb,
      new_compo_nb
    );

  if (is_same_compo_nb) {
    if (HDMV_COMPO_STATE_EPOCH_START == new_compo_desc.composition_state)
      LIBBLU_HDMV_PARSER_ERROR_RETURN(
        "Invalid composition segment, non-Display Update composition "
        "shall not have an Epoch Start composition state.\n"
      );
    if (HDMV_COMPO_STATE_EPOCH_CONTINUE == new_compo_desc.composition_state)
      LIBBLU_HDMV_PARSER_ERROR_RETURN(
        "Invalid composition segment, non-Display Update composition "
        "shall not have an Epoch Continue composition state.\n"
      );
  }

  *is_dup_DS_ret = is_same_compo_nb;
  return 0;
}

static int _clearEpochHdmvContext(
  HdmvContext * ctx,
  HdmvCompositionState composition_state
)
{
  switch (composition_state) {
  case HDMV_COMPO_STATE_NORMAL:
    resetNormalHdmvEpochState(&ctx->epoch);
    break;
  case HDMV_COMPO_STATE_ACQUISITION_POINT:
    resetAcquisitionPointHdmvEpochState(&ctx->epoch);
    break;
  case HDMV_COMPO_STATE_EPOCH_START:
  case HDMV_COMPO_STATE_EPOCH_CONTINUE:
    resetEpochStartHdmvEpochState(&ctx->epoch);
    resetHdmvContextSegmentTypesCounter(&ctx->nb_seq);
    break;
  }

  return 0;
}

static int _checkInitialCompositionDescriptor(
  HdmvCDParameters composition_descriptor
)
{
  // TODO: Additional checks for Epoch Continue
  HdmvCompositionState state = composition_descriptor.composition_state;

  if (
    HDMV_COMPO_STATE_EPOCH_START != state
    && HDMV_COMPO_STATE_EPOCH_CONTINUE != state
  ) {
    LIBBLU_HDMV_PARSER_ERROR_RETURN(
      "Unexpected first composition segment state %s, "
      "shall be Epoch Start or Epoch Continue.\n",
      HdmvCompositionStateStr(composition_descriptor.composition_state)
    );
  }

  return 0;
}

static int _checkVideoDescriptorChangeHdmvDSState(
  const HdmvEpochState * epoch_state,
  HdmvVDParameters video_descriptor
)
{
  bool invalid = false;

#define CHECK_PARAM(name)                                                     \
  do {                                                                        \
    if (epoch_state->video_descriptor.name != video_descriptor.name) {        \
      LIBBLU_HDMV_PARSER_ERROR(                                               \
        "Invalid newer Video Descriptor value, "                              \
        "'" #name "' shall remain the same accross the entire bitstream.\n"   \
      );                                                                      \
      invalid = true;                                                         \
    }                                                                         \
  } while (0)

  CHECK_PARAM(video_width);
  CHECK_PARAM(video_height);
  CHECK_PARAM(frame_rate);

  if (invalid)
    LIBBLU_HDMV_PARSER_ERROR_RETURN(
      "Values in Video Descriptor must remain "
      "constant over the entire bitstream.\n"
    );

  return 0;
}

int initEpochHdmvContext(
  HdmvContext * ctx,
  HdmvCDParameters composition_descriptor,
  HdmvVDParameters video_descriptor
)
{
  LIBBLU_HDMV_COM_DEBUG("Initialization of a new Epoch.\n");

  if (!_isInitializedOutputHdmvContext(ctx->output)) {
    LIBBLU_HDMV_COM_ERROR_RETURN(
      "Unable to init a new DS, "
      "HDMV context handle has been closed.\n"
    );
  }

  switchCurDSHdmvContext(ctx);
  HdmvDSState * prev_ds_state = getPrevDSHdmvContext(ctx);
  HdmvDSState *  new_ds_state = getCurDSHdmvContext(ctx);

  if (HDMV_DS_INITIALIZED == prev_ds_state->init_usage) {
    // The previous DS was not completed.
    // TODO: Complete and add END segment
    LIBBLU_HDMV_PARSER_ERROR_RETURN(
      "A new composition segment has been reached, "
      "but the previous DS has not been completed with an END segment.\n"
    );
  }

  if (HDMV_DS_COMPLETED <= prev_ds_state->init_usage) {
    if (HDMV_COMPO_STATE_EPOCH_CONTINUE == composition_descriptor.composition_state)
      LIBBLU_HDMV_PARSER_ERROR_RETURN(
        "Invalid composition state 'Epoch Continue' "
        "in a non-first Display Set.\n"
      );

    /* Check new DS composition descriptor continuation with previous one. */
    if (_checkNewDisplaySetTransition(prev_ds_state, composition_descriptor, &ctx->is_dup_DS) < 0)
      return -1;

    /* Check video_descriptor change */
    if (_checkVideoDescriptorChangeHdmvDSState(&ctx->epoch, video_descriptor) < 0)
      return -1;

    /* Clear for new DS */
    if (_clearEpochHdmvContext(ctx, composition_descriptor.composition_state) < 0)
      return -1;
  }
  else {
    /* Initial DS */
    if (_checkInitialCompositionDescriptor(composition_descriptor) < 0)
      return -1;

    // TODO: Check initial video_descriptor against video properties
    ctx->epoch.video_descriptor = video_descriptor;
  }

  new_ds_state->compo_desc = composition_descriptor;
  new_ds_state->init_usage = HDMV_DS_INITIALIZED;

  return 0;
}

static void _printDisplaySetContent(
  const HdmvDSState * ds_state,
  HdmvStreamType type
)
{
  LIBBLU_HDMV_COM_DEBUG(" Current number of segments per type:\n");

  unsigned total = 0;

#define _G(idx)  ds_state->sequences[idx].ds_nb_seq
  // TODO

#define _P(n) \
  do { \
    unsigned _nb = _G(HDMV_SEGMENT_TYPE_##n##_IDX); \
    LIBBLU_HDMV_COM_DEBUG("  - "#n": %u.\n", _nb); \
    total += _nb; \
  } while (0)

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
    total
  );
}

static int _checkCompletionOfEachSequence(
  const HdmvDSState * ds_state
)
{
  bool not_null = false;
  for (hdmv_segtype_idx i = 0; i < HDMV_NB_SEGMENT_TYPES; i++)
    not_null |= (NULL != ds_state->sequences[i].pending);
  return (not_null) ? -1 : 0;
}

static void _getNonCompletedSequencesNames(
  char dst[static HDMV_NB_SEGMENT_TYPES * SEGMENT_TYPE_IDX_STR_SIZE],
  const HdmvDSState * ds_state
)
{
  for (hdmv_segtype_idx i = 0; i < HDMV_NB_SEGMENT_TYPES; i++) {
    if (NULL != ds_state->sequences[i].pending) {
      const char * name = segmentTypeIndexStr(i);
      strcpy(dst, name);
      dst += strlen(name);
    }
  }
}

static int _getDisplaySetPresentationTimeCurDS(
  HdmvContext * ctx,
  int64_t * pts_ret
)
{
  const HdmvDSState * cur_ds = getCurDSHdmvContext(ctx);

  HdmvSequencePtr seq;
  if (HDMV_STREAM_TYPE_IGS == ctx->epoch.type)
    seq = getFirstSequenceByIdxHdmvDSState(cur_ds, HDMV_SEGMENT_TYPE_ICS_IDX);
  else // HDMV_STREAM_TYPE_PGS == ctx->epoch.type
    seq = getFirstSequenceByIdxHdmvDSState(cur_ds, HDMV_SEGMENT_TYPE_PCS_IDX);
  if (NULL == seq)
    return -1;

  int64_t pts = seq->segments->param.timestamps_header.pts;

  if (pts < ctx->param.initial_delay)
    LIBBLU_HDMV_TC_ERROR_RETURN(
      "Unexpected negative presentation timestamp, "
      "initial delay value is too high (%" PRId64 " < %" PRId64 ").\n",
      pts,
      ctx->param.initial_delay
    );

  if (NULL != pts_ret)
    *pts_ret = pts - ctx->param.initial_delay;
  return 0;
}

/* Interactive Graphics Stream : */

static int64_t _computeODSDecodeDuration(
  const HdmvDSState * ds,
  unsigned last_ODS_idx,
  float igs_od_decode_delay_factor
)
{
  int64_t odj_dec_duration = 0;

  HdmvSequencePtr seq = getFirstODSSequenceHdmvDSState(ds);
  for (
    unsigned idx = 0;
    NULL != seq && idx <= last_ODS_idx;
    idx++, seq = seq->next_sequence
  ) {
    assert(seq->type == HDMV_SEGMENT_TYPE_ODS);

    LIBBLU_HDMV_TSC_DEBUG(
      "   - ODS id=0x%04" PRIX16 " version_number=%" PRIu8 ":\n",
      seq->data.od.object_descriptor.object_id,
      seq->data.od.object_descriptor.object_version_number
    );

    /* OD_DECODE_DURATION() */
    int32_t od_decode_duration = computeObjectDataDecodeDuration(
      HDMV_STREAM_TYPE_IGS,
      seq->data.od.object_width,
      seq->data.od.object_height
    );
    odj_dec_duration += od_decode_duration;

    LIBBLU_HDMV_TSC_DEBUG(
      "    => OD_DECODE_DURATION: %" PRId64 "\n",
      od_decode_duration
    );

    /* OD_DECODE_DELAY() */
    if (NULL != seq->next_sequence) {
      int32_t od_decode_delay = computeObjectDataDecodeDelay(
        HDMV_STREAM_TYPE_IGS,
        seq->data.od.object_width,
        seq->data.od.object_height,
        igs_od_decode_delay_factor
      );
      odj_dec_duration += od_decode_delay;

      LIBBLU_HDMV_TSC_DEBUG(
        "    => OD_DECODE_DELAY: %" PRId64 "\n",
        od_decode_delay
      );
    }
  }

  return odj_dec_duration;
}

static int64_t _computeICAllODSDecodeDuration(
  const HdmvDSState * ds,
  float igs_od_decode_delay_factor
)
{
  LIBBLU_HDMV_TSC_DEBUG(
    "  Compute OBJ_DECODE_DURATION of IG Display Set "
    "(DECODE_ALL_OD_DURATION):\n"
  );

  // Decode all ODS
  return _computeODSDecodeDuration(
    ds, UINT_MAX, igs_od_decode_delay_factor
  );
}

static int _retrieveODSIdxDS(
  HdmvSeqIndexer * indexer,
  uint16_t object_id,
  unsigned * ods_idx_ret
)
{
  if (0xFFFF == object_id)
    return 0; // No object

  HdmvSequencePtr seq;
  if (getODSSeqIndexer(indexer, object_id, &seq) < 0)
    return -1;

  assert(seq->type == HDMV_SEGMENT_TYPE_ODS);
  assert(seq->data.od.object_descriptor.object_id == object_id);

  LIBBLU_HDMV_TSC_DEBUG(
    "    Retrieve ODS ('object_id' == 0x%04" PRIX16 ") at index %u.\n",
    object_id,
    seq->idx
  );

  *ods_idx_ret = seq->idx;
  return 1;
}

static int _retrieveODSIdxEffectSequence(
  const HdmvEffectSequenceParameters * effect_seq,
  HdmvSeqIndexer * indexer,
  unsigned * last_req_ods_ref,
  unsigned * nb_objects_ret
)
{
  unsigned last_req_ods = *last_req_ods_ref;
  unsigned nb_objects   = 0;

  for (uint8_t eff_i = 0; eff_i < effect_seq->number_of_effects; eff_i++) {
    const HdmvEffectInfoParameters * eff = &effect_seq->effects[eff_i];
    for (uint8_t co_i = 0; co_i < eff->number_of_composition_objects; co_i++) {
      const HdmvCOParameters * co = &eff->composition_objects[co_i];

      unsigned ods_idx;
      int ret = _retrieveODSIdxDS(indexer, co->object_id_ref, &ods_idx);
      if (ret < 0)
        return -1;

      if (0 < ret) {
        last_req_ods = MAX(last_req_ods, ods_idx);
        nb_objects++;
      }
    }
  }

  *last_req_ods_ref = last_req_ods;
  *nb_objects_ret   = nb_objects;
  return 0;
}

static const HdmvButtonParam ** _indexBOGsDefaultValidButton(
  const HdmvPageParameters * page
)
{
  const HdmvButtonParam ** list = calloc(
    page->number_of_BOGs,
    sizeof(HdmvButtonParam *)
  );
  if (NULL == list)
    LIBBLU_HDMV_COM_ERROR_NRETURN("Memory allocation error.\n");

  for (uint8_t bog_i = 0; bog_i < page->number_of_BOGs; bog_i++) {
    const HdmvButtonOverlapGroupParameters * bog = &page->bogs[bog_i];

    if (0xFFFF == bog->default_valid_button_id_ref)
      continue; // No default valid button for this BOG (NULL)

    for (uint8_t btn_i = 0; btn_i < bog->number_of_buttons; btn_i++) {
      const HdmvButtonParam * btn = &bog->buttons[btn_i];
      if (bog->default_valid_button_id_ref == btn->button_id) {
        list[bog_i] = btn; // Found!
        break;
      }
    }

    assert(NULL != list[bog_i]); // default_valid_button_id_ref shall exist
  }

  return list;
}

static int _retrieveODSIdxSeq(
  uint16_t start_object_id_ref,
  uint16_t end_object_id_ref,
  HdmvSeqIndexer * indexer,
  unsigned * last_req_ods_ref,
  unsigned * nb_objects_ret
)
{
  unsigned last_req_ods = *last_req_ods_ref;
  unsigned nb_objects   = 0;

  if (0xFFFF != start_object_id_ref && 0xFFFF != end_object_id_ref) {
    uint16_t start, end;
    if (start_object_id_ref <= end_object_id_ref)
      start = start_object_id_ref, end = end_object_id_ref;
    else
      start = end_object_id_ref, end = start_object_id_ref; // Reverse order

    for (uint16_t obj_id_ref = start; obj_id_ref <= end; obj_id_ref++) {
      unsigned ods_idx;
      int ret = _retrieveODSIdxDS(indexer, obj_id_ref, &ods_idx);
      if (ret < 0)
        return -1;
      if (0 < ret) {
        last_req_ods = MAX(last_req_ods, ods_idx);
        nb_objects++;
      }
    }
  }

  *last_req_ods_ref = last_req_ods;
  *nb_objects_ret   = nb_objects;

  return 0;
}

static int _findLastReqODSPage0(
  const HdmvPageParameters * page_0,
  HdmvSeqIndexer * indexer,
  unsigned * last_req_ods_idx_ret
)
{
  unsigned last_req_ods_idx = 0u;

  LIBBLU_HDMV_TSC_DEBUG(
    "   Retrieving the last object required for the first "
    "Display Update of page 0.\n"
  );

  // Look for objects used in in_effects() of page_0
  unsigned nb_obj_in_effects;
  if (_retrieveODSIdxEffectSequence(&page_0->in_effects, indexer, &last_req_ods_idx, &nb_obj_in_effects) < 0)
    return -1;

  LIBBLU_HDMV_TSC_DEBUG(
    "   Number of ODS required for in_effect(): %u object(s).\n",
    nb_obj_in_effects
  );

  // Fetch for each bog its default valid button
  const HdmvButtonParam ** bogs_def_btn = _indexBOGsDefaultValidButton(
    page_0
  );
  if (NULL == bogs_def_btn)
    return -1;

  // Look for objects used in the normal_state_info structure of the default
  // valid button of BOGs on page_0
  unsigned nb_obj_nml_state = 0u;

  for (uint8_t bog_i = 0; bog_i < page_0->number_of_BOGs; bog_i++) {
    const HdmvButtonParam * btn = bogs_def_btn[bog_i];
    if (NULL == btn)
      continue; // No button in BOG displayed (valid) by default

    HdmvButtonNormalStateInfoParam nsi = btn->normal_state_info;

    unsigned nb_obj;
    if (_retrieveODSIdxSeq(nsi.start_object_id_ref, nsi.end_object_id_ref, indexer, &last_req_ods_idx, &nb_obj) < 0)
      goto free_return;
    nb_obj_nml_state += nb_obj;
  }

  LIBBLU_HDMV_TSC_DEBUG(
    "   Number of ODS required for normal_state_info(): %u object(s).\n",
    nb_obj_nml_state
  );

  // Look for objects used in the selected_state_info structure of the default
  // valid button of BOGs on page_0
  uint16_t def_sel_btn_id_ref = page_0->default_selected_button_id_ref;
  unsigned nb_obj_sel_state = 0u;

  for (uint8_t bog_i = 0; bog_i < page_0->number_of_BOGs; bog_i++) {
    const HdmvButtonParam * btn = bogs_def_btn[bog_i];
    if (NULL == btn)
      continue; // No button in BOG displayed (valid) by default

    HdmvButtonSelectedStateInfoParam ssi = btn->selected_state_info;

    unsigned nb_obj;
    if (_retrieveODSIdxSeq(ssi.start_object_id_ref, ssi.end_object_id_ref, indexer, &last_req_ods_idx, &nb_obj) < 0)
      goto free_return;
    nb_obj_sel_state += nb_obj;

    if (def_sel_btn_id_ref == btn->button_id) {
      // [3] Fig.32A case: Last required ODS is the first one used for the
      // default selected valid button selected_state (or the last ODS used
      // in in_effect/normal_state)
      LIBBLU_HDMV_TSC_DEBUG(
        "   Default selected button reached "
        "('button_id' == 0x%04" PRIx16 " in BOG #%u).\n",
        btn->button_id,
        bog_i
      );
      goto completed;
    }
  }

completed:
  LIBBLU_HDMV_TSC_DEBUG(
    "   Number of ODS required for selected_state_info(): %u object(s).\n",
    nb_obj_sel_state
  );

  free(bogs_def_btn);

  LIBBLU_HDMV_TSC_DEBUG(
    "   Last required ODS index: %u.\n",
    last_req_ods_idx
  );

  if (0 == nb_obj_in_effects && 0 == nb_obj_nml_state && 0 == nb_obj_sel_state) {
    // [3] Fig.33C case: No ODS required by first page
    LIBBLU_HDMV_TSC_DEBUG("   No ODS required by first page.\n");
    return 0;
  }

  *last_req_ods_idx_ret = last_req_ods_idx;
  return 1; // At least one object is used

free_return:
  free(bogs_def_btn);
  return -1;
}

static const HdmvPageParameters * _getDSICPage0(
  const HdmvDSState * ds
)
{
  const HdmvSequencePtr ics_seq = getICSSequenceHdmvDSState(ds);
  if (NULL == ics_seq)
    LIBBLU_ERROR_EXIT("Broken program.\n");

  if (!ics_seq->data.ic.number_of_pages)
    return NULL; // No page
  return &ics_seq->data.ic.pages[0];
}

static int _indexODSofDS(
  HdmvSeqIndexer * indexer,
  const HdmvDSState * ds
)
{
  HdmvSequencePtr seq = getFirstODSSequenceHdmvDSState(ds);
  for (unsigned i = 0; NULL != seq; i++, seq = seq->next_sequence) {
    uint16_t object_id = seq->data.od.object_descriptor.object_id;
    assert(i == seq->idx);
    if (putHdmvSeqIndexer(indexer, object_id, seq) < 0)
      return -1;
  }

  return 0;
}

static int64_t _computeICReqODSDecodeDuration(
  const HdmvDSState * ds,
  float igs_od_decode_delay_factor
)
{
  LIBBLU_HDMV_TSC_DEBUG(
    "  Compute OBJ_DECODE_DURATION of IG Display Set "
    "(DECODE_REQ_OD_DURATION):\n"
  );

  // Fetch Interactive Composition first page (page_id == 0)
  const HdmvPageParameters * page_0 = _getDSICPage0(ds);
  if (NULL == page_0) {
    LIBBLU_HDMV_TSC_DEBUG("   No page, no object to decode.\n");
    return 0;
  }

  // Index all ODS by their 'object_id' to speed-up fetching
  HdmvSeqIndexer ods_indexer;
  initHdmvSeqIndexer(&ods_indexer);
  if (_indexODSofDS(&ods_indexer, ds) < 0)
    return -1;

  unsigned last_req_ods_idx;
  int ret = _findLastReqODSPage0(page_0, &ods_indexer, &last_req_ods_idx);
  cleanHdmvSeqIndexer(ods_indexer);

  if (ret < 0)
    return -1; // Error

  // Compute the decode_duration
  int64_t decode_duration = 0;

  if (0 < ret) {
    // At least one object is required for the first Display Update (of page_0)
    // Decode all ODS up to this object
    decode_duration += _computeODSDecodeDuration(
      ds,
      last_req_ods_idx,
      igs_od_decode_delay_factor
    );
  }

  return decode_duration;
}

static int64_t _computeICDecodeDuration(
  const HdmvDSState * ds,
  const HdmvEpochState * epoch_state,
  float dec_delay_factor,
  bool use_earlier_transfer
)
{
  /**
   * Calculates the decoding time IC_DECODE_DURATION of the Interactive
   * Composition (IC) of Display Set n (DS_n). This value corresponds to the
   * time required to clear the graphics plane, decode the necessary Object
   * Definitions (OD) and transfer the decoded pixels.
   *
   * The first variant of the algorithm (with the DECODE_ALL_OD_DURATION
   * function) takes into account the time necessary to decode all the ODs of
   * the Display Set, the second (with the DECODE_REQ_OD_DURATION function)
   * only takes into account the decoding of the objects used for the first
   * Display Update of the composition (display of the first page or its
   * in_effects() sequence).
   *
   * This process is partially described in patent [3], supplemented by
   * experiments on authored BD from Scenarist BD (DECODE_ALL_OD_DURATION case)
   * or Blu-Print (DECODE_REQ_OD_DURATION case).
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
   *  OBJ_DECODE_DURATION() : One of two functions (DECODE_ALL_OD_DURATION or
   *    DECODE_REQ_OD_DURATION) described below, returning the duration
   *    required to decode objects and transfer them from the Coded Object
   *    Buffer (EB) to the Decoded Object Buffer (DB).
   *  PLANE_CLEAR_TIME()    : Minimum time required to initialize the graphical
   *    plane before drawing the decoded objects by applying composition.
   *    This delay is only necessary for the beginning of Epoch.
   * \endcode
   */

  int64_t obj_decode_duration =
    (use_earlier_transfer)
    ? _computeICReqODSDecodeDuration(ds, dec_delay_factor)
    : _computeICAllODSDecodeDuration(ds, dec_delay_factor)
  ;

  if (obj_decode_duration < 0)
    return -1;

  LIBBLU_HDMV_TSC_DEBUG(
    "  => OBJ_DECODE_DURATION: %" PRId64 " ticks.\n",
    obj_decode_duration
  );

  if (isCompoStateHdmvDSState(ds, HDMV_COMPO_STATE_EPOCH_START)) {
    int64_t plane_clear_time = getHDMVPlaneClearTime(
      HDMV_STREAM_TYPE_IGS,
      epoch_state->video_descriptor.video_width,
      epoch_state->video_descriptor.video_height
    );

    LIBBLU_HDMV_TSC_DEBUG(
      "  => PLANE_CLEAR_TIME: %" PRId64 " ticks.\n",
      plane_clear_time
    );

    /* IC_DECODE_DURATION = MAX(PLANE_CLEAR_TIME, OBJ_DECODE_DURATION) */
    return MAX(plane_clear_time, obj_decode_duration);
  }

  /* IC_DECODE_DURATION = OBJ_DECODE_DURATION */
  return obj_decode_duration;
}

static int64_t _getButtonSize(
  const HdmvButtonParam btn
)
{
  // Return size in bytes
  return 1ll * btn.btn_obj_height * btn.btn_obj_width;
}

static int64_t _getNormalStateButtonSize(
  const HdmvButtonParam btn
)
{
  if (0xFFFF == btn.normal_state_info.start_object_id_ref)
    return 0ll;
  return _getButtonSize(btn);
}

static int64_t _getSelectedStateButtonSize(
  const HdmvButtonParam btn
)
{
  if (0xFFFF == btn.selected_state_info.start_object_id_ref)
    return 0ll;
  return _getButtonSize(btn);
}

static int64_t _computeICTransferDuration(
  HdmvSequencePtr ics_seq
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
  assert(isUniqueInDisplaySetHdmvSequence(ics_seq)); // ICS shall be unique.

  if (!ics_seq->data.ic.number_of_pages)
    return 0; // No page, empty Interactive Composition.

  const HdmvPageParameters page_0 = ics_seq->data.ic.pages[0];
  int64_t size = 0ll;

  if (0 < page_0.in_effects.number_of_effects) {
    /* EFFECT_TRANSFER_DURATION() */
    LIBBLU_HDMV_TSC_DEBUG(
      "  -> Compute EFFECT_TRANSFER_DURATION:\n"
    );

    for (unsigned i = 0; i < page_0.in_effects.number_of_windows; i++) {
      HdmvWindowInfoParameters win = page_0.in_effects.windows[i];
      size += 1LL * win.window_width * win.window_height;

      LIBBLU_HDMV_TSC_DEBUG(
        "   - Window %u: %ux%u.\n",
        i,
        win.window_width,
        win.window_height
      );
    }
  }
  else if (0xFFFF == page_0.default_selected_button_id_ref) {
    /* PAGE_NO_DEFAULT_TRANSFER_DURATION() */
    LIBBLU_HDMV_TSC_DEBUG(
      "  -> Compute PAGE_NO_DEFAULT_TRANSFER_DURATION:\n"
    );

    HdmvButtonParam * largest_btn = NULL;
    int64_t largest_btn_size = 0ll;

    for (uint8_t i = 0; i < page_0.number_of_BOGs; i++) {
      const HdmvButtonOverlapGroupParameters bog = page_0.bogs[i];

      if (0xFFFF == bog.default_valid_button_id_ref)
        continue;

      for (uint8_t j = 0; j < bog.number_of_buttons; j++) {
        const HdmvButtonParam btn = bog.buttons[j];

        if (btn.button_id == bog.default_valid_button_id_ref) {
          int64_t btn_size = _getButtonSize(btn);

          if (largest_btn_size < btn_size) {
            largest_btn = &page_0.bogs[i].buttons[j];
            largest_btn_size = btn_size;
          }

          size += _getNormalStateButtonSize(btn);
          break;
        }
      }
    }

    if (NULL != largest_btn) {
      size += largest_btn_size;
      size -= _getNormalStateButtonSize(*largest_btn);
    }

    LIBBLU_HDMV_TSC_DEBUG(
      "   Total page size: %" PRId64 " byte(s).\n",
      size
    );
  }
  else {
    /* PAGE_DEFAULT_TRANSFER_DURATION() */
    LIBBLU_HDMV_TSC_DEBUG(
      "  -> Compute PAGE_DEFAULT_TRANSFER_DURATION:\n"
    );

    for (uint8_t i = 0; i < page_0.number_of_BOGs; i++) {
      const HdmvButtonOverlapGroupParameters bog = page_0.bogs[i];

      if (0xFFFF == bog.default_valid_button_id_ref)
        continue;

      for (uint8_t j = 0; j < bog.number_of_buttons; j++) {
        const HdmvButtonParam btn = bog.buttons[j];

        if (btn.button_id == page_0.default_selected_button_id_ref) {
          size += _getSelectedStateButtonSize(btn);
          break;
        }
        if (btn.button_id == bog.default_valid_button_id_ref) {
          size += _getNormalStateButtonSize(btn);
          break;
        }
      }
    }

    LIBBLU_HDMV_TSC_DEBUG(
      "   Total page size: %" PRId64 " byte(s).\n",
      size
    );
  }

  return DIV_ROUND_UP(9LL * size, 1600);
}

/** \~english
 * \brief Compute Interactive Graphics Display Set decoding duration.
 *
 * \return int64_t
 *
 * Compute the Display Set n (DS_n) Interactive Graphics Segments PTS-DTS delta
 * required. Written IG_DECODE_DURATION for IG DS_n, this value is the minimal
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
 *  IG_DECODE_DURATION(DS_n):
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
  const HdmvDSState * cur_ds,
  const HdmvEpochState * epoch_state,
  float dec_delay_factor,
  bool use_earlier_transfer
)
{
  LIBBLU_HDMV_TSC_DEBUG("Computing IG_DECODE_DURATION:\n");
  LIBBLU_HDMV_TSC_DEBUG(" ODS decoding delay factor: %.2f.\n", dec_delay_factor);
  LIBBLU_HDMV_TSC_DEBUG(" Minimization: %s.\n", BOOL_STR(use_earlier_transfer));

  int64_t decode_duration = _computeICDecodeDuration(
    cur_ds,
    epoch_state,
    dec_delay_factor,
    use_earlier_transfer
  );
  if (decode_duration < 0)
    return -1;

  LIBBLU_HDMV_TSC_DEBUG(
    " => IC_DECODE_DURATION(DS_n): %" PRId64 " ticks.\n",
    decode_duration
  );

  int64_t transfer_duration = _computeICTransferDuration(
    getICSSequenceHdmvDSState(cur_ds)
  );
  if (transfer_duration < 0)
    return -1;

  LIBBLU_HDMV_TSC_DEBUG(
    " => IC_TRANSFER_DURATION(DS_n): %" PRId64 " ticks.\n",
    transfer_duration
  );

  LIBBLU_HDMV_TSC_DEBUG(
    "=> IG_DECODE_DURATION(DS_n): %" PRId64 " ticks.\n",
    decode_duration + transfer_duration
  );

  return decode_duration + transfer_duration;
}

static int64_t _computeCurIGDisplaySetDecodeDuration(
  HdmvContext * ctx
)
{
  return _computeIGDisplaySetDecodeDuration(
    getCurDSHdmvContext(ctx),
    &ctx->epoch,
    ctx->options.igs_od_decode_delay_factor,
    ctx->options.igs_use_earlier_transfer
  );
}

static int _applyTimestampsCurIGDisplaySet(
  HdmvContext * ctx,
  int64_t pres_time,
  int64_t decode_duration,
  int64_t * end_time_ret
)
{
  HdmvDSState * cur_ds = getCurDSHdmvContext(ctx);
  int64_t decode_time = pres_time - decode_duration;

  float decode_delay_factor = ctx->options.igs_od_decode_delay_factor;
  int64_t ref_ts            = ctx->param.ref_timestamp;

  /* ICS */ {
    LIBBLU_HDMV_COM_DEBUG("   - %s:\n", HdmvSegmentTypeStr(HDMV_SEGMENT_TYPE_ICS));

    HdmvSequencePtr seq = getICSSequenceHdmvDSState(cur_ds);
    seq->pts = ref_ts + pres_time;
    seq->dts = ref_ts + decode_time;

    LIBBLU_HDMV_COM_DEBUG(
      "    - PTS: %" PRId64 " DTS: %" PRId64 "\n",
      seq->pts - ref_ts,
      seq->dts - ref_ts
    );
  }

  /* PDS */ {
    LIBBLU_HDMV_COM_DEBUG("   - %s:\n", HdmvSegmentTypeStr(HDMV_SEGMENT_TYPE_PDS));

    for (
      HdmvSequencePtr seq = getFirstPDSSequenceHdmvDSState(cur_ds);
      NULL != seq;
      seq = seq->next_sequence
    ) {
      seq->pts = seq->dts = ref_ts + decode_time;

      LIBBLU_HDMV_COM_DEBUG(
      "    - PTS: %" PRId64 " DTS: %" PRId64 "\n",
      seq->pts - ref_ts,
      seq->dts - ref_ts
    );
    }
  }

  /* ODS */ {
    LIBBLU_HDMV_COM_DEBUG("   - %s:\n", HdmvSegmentTypeStr(HDMV_SEGMENT_TYPE_ODS));

    for (
      HdmvSequencePtr seq = getFirstODSSequenceHdmvDSState(cur_ds);
      NULL != seq;
      seq = seq->next_sequence
    ) {
      seq->dts = ref_ts + decode_time;

      decode_time += computeObjectDataDecodeDuration(
        HDMV_STREAM_TYPE_IGS,
        seq->data.od.object_width,
        seq->data.od.object_height
      );

      seq->pts = ref_ts + decode_time;

      if (NULL != seq->next_sequence)
        decode_time += computeObjectDataDecodeDelay(
          HDMV_STREAM_TYPE_IGS,
          seq->data.od.object_width,
          seq->data.od.object_height,
          decode_delay_factor
        );

      LIBBLU_HDMV_COM_DEBUG(
        "    - 0x%04" PRIX16 " PTS: %" PRId64 " DTS: %" PRId64 "\n",
        seq->data.od.object_descriptor.object_id,
        seq->pts - ref_ts,
        seq->dts - ref_ts
      );
    }
  }

  /* END */ {
    LIBBLU_HDMV_COM_DEBUG("   - %s:\n", HdmvSegmentTypeStr(HDMV_SEGMENT_TYPE_END));

    HdmvSequencePtr seq = getENDSequenceHdmvDSState(cur_ds);
    seq->pts = seq->dts = ref_ts + decode_time;

    LIBBLU_HDMV_COM_DEBUG(
      "    - PTS: %" PRId64 " DTS: %" PRId64 "\n",
      seq->pts - ref_ts,
      seq->dts - ref_ts
    );
  }

  *end_time_ret = decode_time;
  return 0;
}

/* Presentation Graphics Stream : */

static int64_t _computePlaneInitializationTimeCurDS(
  HdmvContext * ctx
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
  const HdmvDSState * cur_ds = getCurDSHdmvContext(ctx);
  const HdmvEpochState * epoch_state = &ctx->epoch;

  if (isCompoStateHdmvDSState(cur_ds, HDMV_COMPO_STATE_EPOCH_START)) {
    /* Epoch start, clear whole graphical plane */
    return getHDMVPlaneClearTime(
      HDMV_STREAM_TYPE_PGS,
      epoch_state->video_descriptor.video_width,
      epoch_state->video_descriptor.video_height
    );
  }

  /* Not epoch start, clear only empty windows */
  HdmvPCParameters pc;
  if (!fetchCurrentPCHdmvEpochState(epoch_state, &pc, NULL))
    LIBBLU_HDMV_TSC_ERROR_RETURN(
      "Internal error, unable to recover Presentation Composition.\n"
    );

  HdmvWDParameters wd;
  if (!fetchCurrentWDHdmvEpochState(epoch_state, false, &wd))
    LIBBLU_HDMV_TSC_ERROR_RETURN(
      "Internal error, unable to recover Window Definition.\n"
    );

  int64_t init_duration = 0;

  for (unsigned i = 0; i < wd.number_of_windows; i++) {
    HdmvWindowInfoParameters window = wd.windows[i];
    bool empty_window = true;

    for (unsigned j = 0; j < pc.number_of_composition_objects; j++) {
      if (window.window_id == pc.composition_objects[j].window_id_ref) {
        /* A composition object use the window, mark as not empty */
        empty_window = false;
        break;
      }
    }

    if (empty_window) {
      /* Empty window, clear it */
      init_duration += DIV_ROUND_UP(
        9LL * window.window_width * window.window_height,
        3200
      );
    }
  }

  // Add 1 tick delay, observed to avoid WDS DTS being equal to its PTS.
  return init_duration + 1;
}

static int64_t _computeAndSetODSDecodeDuration(
  const HdmvEpochState * epoch_state,
  uint16_t object_id
)
{
  HdmvODParameters od;
  if (!existsObjectHdmvEpochState(epoch_state, object_id, &od))
    return 0; // !EXISTS(object_id, DS_n)

  int32_t decode_duration = computeObjectDataDecodeDuration(
    HDMV_STREAM_TYPE_PGS,
    od.object_width,
    od.object_height
  );

  return decode_duration;
}

static int64_t _computeWindowTransferDuration(
  const HdmvEpochState * epoch_state,
  uint8_t window_id
)
{
  HdmvWDParameters wd;
  if (!fetchCurrentWDHdmvEpochState(epoch_state, false, &wd))
    LIBBLU_HDMV_TSC_ERROR_RETURN(
      "Internal error, unable to recover Window Definition.\n"
    );

  if (wd.number_of_windows <= window_id)
    LIBBLU_HDMV_TSC_ERROR_RETURN(
      "Internal error, undefined 'window_id' == 0x%02" PRIX8 ".\n",
      window_id
    );

  const HdmvWindowInfoParameters window = wd.windows[window_id];

  return DIV_ROUND_UP(
    9LL * window.window_width * window.window_height,
    3200
  );
}

static int64_t _getWindowDrawingDuration(
  const HdmvEpochState * epoch_state
)
{
  HdmvWDParameters wd;
  if (!fetchCurrentWDHdmvEpochState(epoch_state, false, &wd))
    LIBBLU_HDMV_TSC_ERROR_RETURN(
      "Internal error, unable to recover Window Definition.\n"
    );

  int64_t size = 0;
  for (unsigned i = 0; i < wd.number_of_windows; i++) {
    const HdmvWindowInfoParameters window = wd.windows[i];
    size += window.window_width * window.window_height;
  }

  /* ceil(90000 * 8 * size / 3200) */
  int64_t drawing_duration = DIV_ROUND_UP(9LL * size, 3200);

  return drawing_duration;
}

static int64_t _computeCurPGDisplaySetDecodeDuration(
  HdmvContext * ctx
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
  const HdmvEpochState * epoch_state = &ctx->epoch;

  LIBBLU_HDMV_TSC_DEBUG(
    "    Compute PG_DECODE_DURATION of PG Display Set:\n"
  );

  int64_t decode_duration = _computePlaneInitializationTimeCurDS(ctx);
    // Time required to clear Graphical Plane (or empty Windows).

  LIBBLU_HDMV_TSC_DEBUG(
    "     => PLANE_INITIALIZATION_TIME(DS_n): %" PRId64 " ticks.\n",
    decode_duration
  );

  HdmvPCParameters pc;
  if (!fetchCurrentPCHdmvEpochState(epoch_state, &pc, NULL))
    LIBBLU_HDMV_TSC_ERROR_RETURN(
      "Internal error, unable to recover Presentation Composition.\n"
    );

  if (2 == pc.number_of_composition_objects) {
    const HdmvCOParameters obj0 = pc.composition_objects[0];
    int64_t obj_0_decode_duration = _computeAndSetODSDecodeDuration(
      epoch_state, obj0.object_id_ref
    );

    LIBBLU_HDMV_TSC_DEBUG(
      "      => ODS_DECODE_DURATION(DS_n.PCS.OBJ[0]): %" PRId64 " ticks.\n",
      obj_0_decode_duration
    );

    const HdmvCOParameters obj1 = pc.composition_objects[1];
    int64_t obj_1_decode_duration = _computeAndSetODSDecodeDuration(
      epoch_state, obj1.object_id_ref
    );

    LIBBLU_HDMV_TSC_DEBUG(
      "      => ODS_DECODE_DURATION(DS_n.PCS.OBJ[1]): %" PRId64 " ticks.\n",
      obj_1_decode_duration
    );

    decode_duration = MAX(decode_duration, obj_0_decode_duration);

    if (obj0.window_id_ref == obj1.window_id_ref) {
      decode_duration = MAX(decode_duration, obj_0_decode_duration + obj_1_decode_duration);

      int64_t obj_0_transfer_duration = _computeWindowTransferDuration(
        epoch_state, obj0.window_id_ref
      );
      if (obj_0_transfer_duration < 0)
        return -1;

      LIBBLU_HDMV_TSC_DEBUG(
        "      => WINDOW_TRANSFER_DURATION(DS_n.PCS.OBJ[0]): %" PRId64 " ticks.\n",
        obj_0_transfer_duration
      );

      decode_duration += obj_0_transfer_duration;
    }
    else {
      int64_t obj_0_transfer_duration = _computeWindowTransferDuration(
        epoch_state, obj0.window_id_ref
      );
      if (obj_0_transfer_duration < 0)
        return -1;

      LIBBLU_HDMV_TSC_DEBUG(
        "      => WINDOW_TRANSFER_DURATION(DS_n.PCS.OBJ[0]): %" PRId64 " ticks.\n",
        obj_0_transfer_duration
      );

      decode_duration += obj_0_transfer_duration;
      decode_duration = MAX(decode_duration, obj_0_decode_duration + obj_1_decode_duration);

      int64_t obj_1_transfer_duration = _computeWindowTransferDuration(
        epoch_state, obj1.window_id_ref
      );
      if (obj_1_transfer_duration < 0)
        return -1;

      LIBBLU_HDMV_TSC_DEBUG(
        "      => WINDOW_TRANSFER_DURATION(DS_n.PCS.OBJ[1]): %" PRId64 " ticks.\n",
        obj_1_transfer_duration
      );

      decode_duration += obj_1_transfer_duration;
    }
  }
  else if (1 == pc.number_of_composition_objects) {
    const HdmvCOParameters obj0 = pc.composition_objects[0];
    int64_t obj_0_decode_duration = _computeAndSetODSDecodeDuration(
      epoch_state, obj0.object_id_ref
    );

    LIBBLU_HDMV_TSC_DEBUG(
      "     => ODS_DECODE_DURATION(DS_n.PCS.OBJ[0]): %" PRId64 " ticks.\n",
      obj_0_decode_duration
    );

    decode_duration = MAX(decode_duration, obj_0_decode_duration);

    int64_t obj_0_transfer_duration = _computeWindowTransferDuration(
      epoch_state, obj0.window_id_ref
    );
    if (obj_0_transfer_duration < 0)
        return -1;

    LIBBLU_HDMV_TSC_DEBUG(
      "      => WINDOW_TRANSFER_DURATION(DS_n.PCS.OBJ[0]): %" PRId64 " ticks.\n",
      obj_0_transfer_duration
    );

    decode_duration += obj_0_transfer_duration;
  }

  LIBBLU_HDMV_TSC_DEBUG(
    "     => PG_DECODE_DURATION(DS_n): %" PRId64 " ticks.\n",
    decode_duration
  );

  int64_t min_drawing_duration = _getWindowDrawingDuration(epoch_state);
  if (min_drawing_duration < 0)
    return -1;

  LIBBLU_HDMV_TSC_DEBUG(
    "     => MIN_DRAWING_DURATION(DS_n.WDS): %" PRId64 " ticks.\n",
    min_drawing_duration
  );

  return decode_duration;
}

static int _applyTimestampsCurPGDisplaySet(
  HdmvContext * ctx,
  int64_t pres_time,
  int64_t decode_duration,
  int64_t * end_time_ret
)
{
  HdmvDSState * cur_ds = getCurDSHdmvContext(ctx);
  int64_t ref_ts = ctx->param.ref_timestamp;

  int64_t initial_decode_time = pres_time - decode_duration;

  /* PCS */ {
    LIBBLU_HDMV_COM_DEBUG("    - %s:\n", HdmvSegmentTypeStr(HDMV_SEGMENT_TYPE_PCS));

    HdmvSequencePtr seq = getPCSSequenceHdmvDSState(cur_ds);
    seq->pts = ref_ts + pres_time;
    seq->dts = ref_ts + initial_decode_time;

    LIBBLU_HDMV_COM_DEBUG(
      "     - PTS: %" PRId64 " DTS: %" PRId64 "\n",
      seq->pts - ref_ts,
      seq->dts - ref_ts
    );
  }

  /* WDS */ {
    LIBBLU_HDMV_COM_DEBUG("    - %s:\n", HdmvSegmentTypeStr(HDMV_SEGMENT_TYPE_WDS));

    HdmvSequencePtr seq = getWDSSequenceHdmvDSState(cur_ds);
    if (NULL != seq) {
      assert(isUniqueInDisplaySetHdmvSequence(seq));
      seq->pts = ref_ts + pres_time - _getWindowDrawingDuration(&ctx->epoch);
      seq->dts = ref_ts + initial_decode_time;

      LIBBLU_HDMV_COM_DEBUG(
        "     - PTS: %" PRId64 " DTS: %" PRId64 "\n",
        seq->pts - ref_ts,
        seq->dts - ref_ts
      );
    }
  }

  /* PDS */ {
    LIBBLU_HDMV_COM_DEBUG("    - %s:\n", HdmvSegmentTypeStr(HDMV_SEGMENT_TYPE_PDS));

    for (
      HdmvSequencePtr seq = getFirstPDSSequenceHdmvDSState(cur_ds);
      NULL != seq;
      seq = seq->next_sequence
    ) {
      seq->pts = seq->dts = ref_ts + initial_decode_time;

      LIBBLU_HDMV_COM_DEBUG(
        "     - PTS: %" PRId64 " DTS: %" PRId64 "\n",
        seq->pts - ref_ts,
        seq->dts - ref_ts
      );
    }
  }

  /* ODS */ {
    LIBBLU_HDMV_COM_DEBUG("    - %s:\n", HdmvSegmentTypeStr(HDMV_SEGMENT_TYPE_ODS));

    for (
      HdmvSequencePtr seq = getFirstODSSequenceHdmvDSState(cur_ds);
      NULL != seq;
      seq = seq->next_sequence
    ) {
      seq->dts = ref_ts + initial_decode_time;

      initial_decode_time += computeObjectDataDecodeDuration(
        HDMV_STREAM_TYPE_PGS,
        seq->data.od.object_width,
        seq->data.od.object_height
      );

      seq->pts = ref_ts + initial_decode_time;

#if 0
      if (NULL != seq->next_sequence)
        decode_time += computeObjectDataDecodeDelay(
          HDMV_STREAM_TYPE_PGS,
          seq->data.od.object_width,
          seq->data.od.object_height,
          decode_delay_factor
        );
#endif

      LIBBLU_HDMV_COM_DEBUG(
        "     - PTS: %" PRId64 " DTS: %" PRId64 "\n",
        seq->pts - ref_ts,
        seq->dts - ref_ts
      );
    }

    // assert(initial_decode_time <= pres_time); // Might be less
  }

  /* END */ {
    LIBBLU_HDMV_COM_DEBUG("    - %s:\n", HdmvSegmentTypeStr(HDMV_SEGMENT_TYPE_END));

    HdmvSequencePtr seq = getENDSequenceHdmvDSState(cur_ds);
    seq->pts = seq->dts = ref_ts + initial_decode_time;

    LIBBLU_HDMV_COM_DEBUG(
      "     - PTS: %" PRId64 " DTS: %" PRId64 "\n",
      seq->pts - ref_ts,
      seq->dts - ref_ts
    );
  }

  *end_time_ret = initial_decode_time;
  return 0;
}

static void _debugCompareComputedTimestampsCurDS(
  HdmvContext * ctx
)
{
  const HdmvDSState * cur_ds = getCurDSHdmvContext(ctx);

  int64_t ref_timestamp = ctx->param.ref_timestamp;
  int64_t initial_delay = ctx->param.initial_delay;
  bool issue = false;

  for (hdmv_segtype_idx idx = 0; idx < HDMV_NB_SEGMENT_TYPES; idx++) {
    HdmvSequencePtr seq = cur_ds->sequences[idx].ds_head;

    for (unsigned i = 0; NULL != seq; seq = seq->next_sequence, i++) {
      LIBBLU_HDMV_COM_DEBUG("    - %s:\n", HdmvSegmentTypeStr(seq->type));

      int64_t dts = seq->dts + initial_delay - ref_timestamp;
      int64_t pts = seq->pts + initial_delay - ref_timestamp;

      LIBBLU_HDMV_COM_DEBUG(
        "     - PTS: %" PRId64 " (%" PRId64 ") DTS: %" PRId64 " (%" PRId64 ")\n",
        seq->pts - ref_timestamp,
        pts,
        seq->dts - ref_timestamp,
        pts != dts ? dts : 0
      );

      int32_t expected_dts = seq->segments->param.timestamps_header.dts;

      if ((0 < expected_dts || dts != pts) && dts != expected_dts) {
        LIBBLU_HDMV_TSC_WARNING(
          " Mismatch %s-%u DTS: exp %" PRId32 " got %" PRId64 ".\n",
          HdmvSegmentTypeStr(seq->type), i,
          expected_dts,
          dts
        );
        issue = true;
      }

      int32_t expected_pts = seq->segments->param.timestamps_header.pts;

      if (pts != expected_pts) {
        LIBBLU_HDMV_TSC_WARNING(
          " Mismatch %s-%u PTS: exp %" PRId32 " got %" PRId64 ".\n",
          HdmvSegmentTypeStr(seq->type), i,
          expected_pts,
          pts
        );
        issue = true;
      }
    }
  }

  if (issue) {
    LIBBLU_HDMV_TSC_WARNING(
      "Issue with DS #%u, "
      "'composition_number' == %u, 'composition_state' == %s.\n",
      ctx->nb_DS,
      cur_ds->compo_desc.composition_number,
      HdmvCompositionStateStr(cur_ds->compo_desc.composition_state)
    );
  }
}

static int _checkEarliestDTSPTSValue(
  const HdmvSequencePtr seq
)
{
  if (seq->has_earliest_available_pts && seq->pts < seq->earliest_available_pts) {
    switch (seq->type) {
    case HDMV_SEGMENT_TYPE_PDS:
      LIBBLU_HDMV_CK_TC_FAIL_RETURN(
        "Invalid Display Set, the calculated initial decoding time of a "
        "PDS carrying a Palette update overlaps the active period of a "
        "composition from a previous DS which uses the Palette "
        "('palette_id' == 0x%02" PRIX8 ", "
        "'palette_version_number' == 0x%02" PRIX8 ", "
        "computed PTS: %" PRId64 ", "
        "earliest available PTS: %" PRId64 ").\n",
        seq->data.pd.palette_descriptor.palette_id,
        seq->data.pd.palette_descriptor.palette_version_number,
        seq->pts,
        seq->earliest_available_pts
      ); break;

    case HDMV_SEGMENT_TYPE_ODS:
      LIBBLU_HDMV_CK_TC_FAIL_RETURN(
        "Invalid Display Set, the calculated initial decoding time of a "
        "ODS carrying an Object update overlaps the active period of a "
        "composition from a previous DS which uses the Object "
        "('object_id' == 0x%04" PRIX16 ", "
        "'object_version_number' == 0x%02" PRIX8 ", "
        "computed PTS: %" PRId64 ", "
        "earliest available PTS: %" PRId64 ").\n",
        seq->data.od.object_descriptor.object_id,
        seq->data.od.object_descriptor.object_version_number,
        seq->pts,
        seq->earliest_available_pts
      ); break;

    default: // This is unexpected
      LIBBLU_HDMV_CK_TC_FAIL_RETURN(
        "Invalid Display Set, the calculated initial decoding time of a "
        "Definition Segment carrying an update overlaps the active period "
        "of a composition from a previous DS which uses the definition "
        "('segment_type' == %s, "
        "computed PTS: %" PRId64 ", "
        "earliest available PTS: %" PRId64 ").\n",
        HdmvSegmentTypeStr(seq->type),
        seq->pts,
        seq->earliest_available_pts
      ); break;
    }
  }

  if (seq->has_earliest_available_dts && seq->dts < seq->earliest_available_dts) {
    switch (seq->type) {
    case HDMV_SEGMENT_TYPE_PCS:
      LIBBLU_HDMV_CK_TC_FAIL_RETURN(
        "Invalid Display Set, the number of active Presentation "
        "Compositions exceeds 8, newer PCS overwrites a composition that "
        "has not yet been displayed "
        "(overwritten PCS PTS: %" PRId64 ", "
        "newer PCS DTS: %" PRId64 ").\n",
        seq->earliest_available_dts,
        seq->pts
      ); break;

    default: // This is unexpected
      LIBBLU_HDMV_CK_TC_FAIL_RETURN(
        "Invalid Display Set, the calculated initial decoding time of a "
        "Composition Segment causes the overwriting of a composition "
        "that has not yet been displayed "
        "('segment_type' == %s, "
        "overwritten PCS PTS: %" PRId64 ", "
        "newer PCS DTS: %" PRId64 ").\n",
        HdmvSegmentTypeStr(seq->type),
        seq->earliest_available_dts,
        seq->pts
      ); break;
    }
  }

  return 0;
}

static int _checkEarliestDTSPTSValueDS(
  const HdmvDSState * cur_ds
)
{
  for (hdmv_segtype_idx idx = 0; idx < HDMV_NB_SEGMENT_TYPES; idx++) {
    HdmvSequencePtr seq = cur_ds->sequences[idx].ds_head;
    for (unsigned i = 0; NULL != seq; seq = seq->next_sequence, i++) {
      if (_checkEarliestDTSPTSValue(seq) < 0)
        return -1;
    }
  }

  return 0;
}

static int _checkDTSPTSValue(
  HdmvContext * ctx,
  const HdmvSequencePtr seq,
  bool earliest_ts_already_checked
)
{
  const HdmvDSState * cur_ds = getCurDSHdmvContext(ctx);

  HdmvVDParameters video_desc = ctx->epoch.video_descriptor;

  // This value is used to convert back time values to original timestamps.
  int64_t ts_diff = ctx->param.initial_delay - ctx->param.ref_timestamp;

  int64_t seq_dts = seq->dts;
  int64_t seq_pts = seq->pts;

  /* ### Earliest possible timestamps check ### */
  if (!earliest_ts_already_checked) {
    if (_checkEarliestDTSPTSValue(seq) < 0)
      return -1;
  }

  /* ### IG/PG Common constraints ### */

  if (HDMV_SEGMENT_TYPE_PDS == seq->type) {
    /* PDS */

    /**
     * PTS(DS_n[PDS]) >= PTS(DS_m[ICS|PCS])
     * where DS_m is a prior DS which uses the Palette updated by PDS of DS_n
     */
    if (seq->has_earliest_available_pts && seq_pts < seq->earliest_available_pts)
      LIBBLU_HDMV_CK_TC_FAIL_RETURN(
        "Invalid Display Set, a PDS carries an update to a Palette which is "
        "currently required by a composition of a prior DS "
        "('palette_id' == 0x%02" PRIX8 ", "
        "'palette_version_number' == 0x%02" PRIX8 ", "
        "Segment PTS: %" PRId64 ", "
        "earliest available PTS: %" PRId64 ").\n",
        seq->data.pd.palette_descriptor.palette_id,
        seq->data.pd.palette_descriptor.palette_version_number,
        seq_pts + ts_diff,
        seq->earliest_available_pts + ts_diff
      );

    if (NULL == seq->prev_sequence) {
      /* First PDS (PDS_1) */

      HdmvSequencePtr cs_seq = getICSPCSSequenceHdmvDSState(cur_ds);
      if (NULL == cs_seq)
        return -1; // Unexpected missing ICS/PCS

      /* DTS(DS_n[ICS|PCS]) <= PTS(DS_n[PDS_1]) */
      if (cs_seq->dts > seq_pts)
        LIBBLU_HDMV_CK_TC_FAIL_RETURN(
          "Invalid Display Set, first Palette Definition Segment PTS value must "
          "be greater than or equal to Interactive/Presentation Composition Segment DTS value "
          "(DTS(DS_n[ICS|PCS]) == %" PRId64 ", PTS(DS_n[PDS_1]) == %" PRId64 ").\n",
          cs_seq->pts + ts_diff,
          seq_pts + ts_diff
        );
    }
    else {
      /* Non-first PDS (PDS_j) */
      HdmvSequencePtr prev_pds_seq = seq->prev_sequence;

      /* PTS(DS_n[PDS]_j-1) <= PTS(DS_n[PDS]_j) */
      if (prev_pds_seq->pts > seq_pts)
        LIBBLU_HDMV_CK_TC_FAIL_RETURN(
          "Invalid Display Set, Palette Definition Segment PTS value must "
          "be greater than or equal to previous Palette Definition Segment PTS value "
          "(PTS(DS_n[PDS]_j-1) == %" PRId64 ", PTS(DS_n[PDS]_j) == %" PRId64 ").\n",
          prev_pds_seq->pts + ts_diff,
          seq_pts + ts_diff
        );
    }

    HdmvSequencePtr end_seq = getENDSequenceHdmvDSState(cur_ds);
    if (NULL == end_seq)
      return -1; // Unexpected missing ICS/PCS

    /* PTS(DS_n[PDS_j]) <= PTS(DS_n[END]) */
    if (seq_pts > end_seq->pts)
      LIBBLU_HDMV_CK_TC_FAIL_RETURN(
        "Invalid Display Set, Palette Definition Segment PTS value must "
        "be less than or equal to END of Display Set Segment PTS value "
        "(PTS(DS_n[PDS_j]) == %" PRId64 ", PTS(DS_n[END]) == %" PRId64 ").\n",
        seq_pts + ts_diff,
        end_seq->pts + ts_diff
      );
  }
  else if (HDMV_SEGMENT_TYPE_ODS == seq->type) {
    /* ODS */

    /**
     * PTS(DS_n[ODS]) >= PTS(DS_m[ICS|PCS])
     * where DS_m is a prior DS which uses the Object updated by ODS of DS_n
     */
    if (seq->has_earliest_available_pts && seq_pts < seq->earliest_available_pts)
      LIBBLU_HDMV_CK_TC_FAIL_RETURN(
        "Invalid Display Set, a ODS carries an update to an Object which is "
        "currently required by a composition of a prior DS "
        "('object_id' == 0x%02" PRIX8 ", "
        "'object_version_number' == 0x%02" PRIX8 ", "
        "Segment PTS: %" PRId64 ", "
        "earliest available PTS: %" PRId64 ").\n",
        seq->data.od.object_descriptor.object_id,
        seq->data.od.object_descriptor.object_version_number,
        seq_pts + ts_diff,
        seq->earliest_available_pts + ts_diff
      );

    /* PTS(DS_n[ODS_j]) = DTS(DS_n[ODS_j]) + ceil((90000 * SIZE(DS_n[ODS_j])) / Rd) */
    int64_t decode_duration = computeObjectDataDecodeDuration(
      ctx->epoch.type,
      seq->data.od.object_width,
      seq->data.od.object_height
    );

    if (seq_pts != seq_dts + decode_duration)
      LIBBLU_HDMV_CK_TC_FAIL_RETURN(
        "Invalid Display Set, Object Definition Segment PTS value must "
        "be equal to its DTS value plus object decoding duration "
        "(PTS(DS_n[ODS_j]) == %" PRId64 ", DTS(DS_n[ODS_j]) == %" PRId64 ", "
        "decode_duration == %" PRId64 ", delta == %" PRId64 ").\n",
        seq_pts + ts_diff,
        seq_dts + ts_diff,
        decode_duration,
        seq_pts - seq_dts
      );

    if (NULL == seq->prev_sequence) {
      /* First ODS (ODS_1) */

      HdmvSequencePtr cs_seq = getICSPCSSequenceHdmvDSState(cur_ds);
      if (NULL == cs_seq)
        return -1; // Unexpected missing ICS/PCS

      /* DTS(DS_n[ICS|PCS]) <= DTS(DS_n[ODS_1]) */
      if (cs_seq->dts > seq_dts)
        LIBBLU_HDMV_CK_TC_FAIL_RETURN(
          "Invalid Display Set, first Object Definition Segment DTS value must "
          "be greater than or equal to Interactive/Presentation Composition Segment DTS value "
          "(DTS(DS_n[ICS|PCS]) == %" PRId64 ", DTS(DS_n[ODS_1]) == %" PRId64 ").\n",
          cs_seq->dts + ts_diff,
          seq_dts + ts_diff
        );

      HdmvSequencePtr pds_seq = getLastPDSSequenceHdmvDSState(cur_ds);
      if (NULL != pds_seq) {
        /* PTS(DS_n(PDS_last)) <= DTS(DS_n[ODS_1]) */
        if (pds_seq->pts > seq_dts)
          LIBBLU_HDMV_CK_TC_FAIL_RETURN(
            "Invalid Display Set, first Object Definition Segment DTS value must "
            "be greater than or equal to last Palette Definition Segment PTS value "
            "(PTS(DS_n(PDS_last)) == %" PRId64 ", DTS(DS_n[ODS_1]) == %" PRId64 ").\n",
            pds_seq->pts + ts_diff,
            seq_dts + ts_diff
          );
      }
    }
    else {
      /* Non-first ODS (ODS_j) */

      /* PTS(DS_n[ODS_j-1]) <= DTS(DS_n[ODS_j]) */
      HdmvSequencePtr prev_ods_seq = seq->prev_sequence;
      if (prev_ods_seq->pts > seq_dts)
        LIBBLU_HDMV_CK_TC_FAIL_RETURN(
          "Invalid Display Set, Object Definition Segment DTS value must "
          "be greater than or equal to last ODS PTS value "
          "(PTS(DS_n[ODS_j-1]) == %" PRId64 ", DTS(DS_n[ODS_j]) == %" PRId64 ").\n",
          prev_ods_seq->pts + ts_diff,
          seq_dts + ts_diff
        );
    }
  }
  else if (HDMV_SEGMENT_TYPE_ICS == seq->type || HDMV_SEGMENT_TYPE_PCS == seq->type) {
    /* ICS/PCS */

    if (0 < cur_ds->ds_idx) {
      /* Non first DS */
      HdmvDSState * prev_ds = getPrevDSHdmvContext(ctx);

      HdmvSequencePtr end_seq = getENDSequenceHdmvDSState(prev_ds);
      if (NULL == end_seq)
        return -1; // Unexpected missing END

      /* PTS(DS_n-1[END]) <= DTS(DS_n[ICS|PCS]) */
      if (end_seq->pts > seq_dts)
        LIBBLU_HDMV_CK_TC_FAIL_RETURN(
          "Invalid Display Set, Interactive/Presentation Composition Segment DTS "
          "value must be greater than or equal to previous Display Set END "
          "of Display Set Segment PTS value "
          "(PTS(DS_n-1[END]) == %" PRId64 ", DTS(DS_n[ICS|PCS]) == %" PRId64 ").\n",
          end_seq->pts + ts_diff,
          seq_dts + ts_diff
        );

      HdmvSequencePtr prev_cs_seq = getICSPCSSequenceHdmvDSState(prev_ds);
      if (NULL == prev_cs_seq)
        return -1; // Unexpected missing ICS/PCS

      if (isEpochStartHdmvCDParameters(cur_ds->compo_desc)) {
        /* PTS(EPOCH_m-1[DS_last[ICS|PCS]]) <= DTS(EPOCH_m[DS_first[ICS|PCS]]) */
        if (prev_cs_seq->pts > seq_dts)
          LIBBLU_HDMV_CK_TC_FAIL_RETURN(
            "Invalid Display Set, Interactive/Presentation Composition Segment DTS "
            "value must be greater than or equal to the PTS value of the last "
            "Display Set ICS/PCS from previous Epoch "
            "(PTS(EPOCH_m-1[DS_last[ICS|PCS]]) == %" PRId64 ", "
            "DTS(EPOCH_m[DS_first[ICS|PCS]]) == %" PRId64 ").\n",
            prev_cs_seq->pts + ts_diff,
            seq_dts + ts_diff
          );
      }

      int64_t frame_dur = getFrameDurHdmvVDParameters(video_desc);

      /* PTS(DS_n-1[ICS|PCS]) + frame_duration <= PTS(DS_n[ICS|PCS]) */
      if (prev_cs_seq->pts + frame_dur > seq_pts)
        LIBBLU_HDMV_CK_TC_FAIL_RETURN(
          "Invalid Display Set, Interactive/Presentation Composition Segment PTS "
          "value must be greater than or equal to previous Display Set ICS/PCS PTS value "
          "(PTS[DS_n-1[ICS|PCS]] == %" PRId64 ", "
          "frame duration at %s FPS == %" PRId64 ", "
          "PTS[DS_n[ICS|PCS]] == %" PRId64 ").\n",
          prev_cs_seq->pts + ts_diff,
          HdmvFrameRateCodeStr(video_desc.frame_rate), frame_dur,
          seq_pts + ts_diff
        );
    }
  }
  else if (HDMV_SEGMENT_TYPE_END == seq->type) {
    /* END */

    HdmvSequencePtr cs_seq = getICSPCSSequenceHdmvDSState(cur_ds);
    if (NULL == cs_seq)
      return -1; // Unexpected missing ICS/PCS

    /* DTS(DS_n[ICS|PCS]) <= PTS(DS_n[END]) */
    if (cs_seq->dts > seq_pts)
      LIBBLU_HDMV_CK_TC_FAIL_RETURN(
        "Invalid Display Set, END of Display Set Segment PTS value must "
        "be greater than or equal to Interactive/Presentation Composition Segment DTS value "
        "(DTS(DS_n[ICS|PCS]) == %" PRId64 ", PTS(DS_n[END]) == %" PRId64 ").\n",
        cs_seq->pts + ts_diff,
        seq_pts + ts_diff
      );

    HdmvSequencePtr ods_seq = getLastODSSequenceHdmvDSState(cur_ds);
    if (NULL != ods_seq) {
      /* PTS(DS_n[ODS_last]) = PTS(DS_n[END]) */
      if (ods_seq->pts != seq_pts)
        LIBBLU_HDMV_CK_TC_FAIL_RETURN(
          "Invalid Display Set, END of Display Set Segment PTS value must "
          "be equal to last Object Definition Segment PTS value "
          "(PTS(DS_n[ODS_last]) == %" PRId64 ", PTS(DS_n[END]) == %" PRId64 ").\n",
          ods_seq->pts + ts_diff,
          seq_pts + ts_diff
        );
    }
  }

  /* ### IGS specific constraints ### */

  if (HDMV_STREAM_TYPE_IGS == ctx->epoch.type) {
    if (HDMV_SEGMENT_TYPE_ICS == seq->type) {
      /* ICS */

      if (HDMV_STREAM_MODEL_OOM != seq->data.ic.stream_model) {
        /* PTS(EPOCH_m[DS_n[ICS]]) alignment */
        int64_t exp_pts;
        if (!isFrameTcAlignedHdmvVDParameters(video_desc, seq->pts + ts_diff, &exp_pts))
          LIBBLU_HDMV_CK_TC_FAIL_RETURN(
            "Invalid Display Set, Interactive Composition Segment PTS "
            "value must be aligned to a video frame timestamp "
            "(PTS(DS_n[ICS]) == %" PRId64 ", "
            "frame_rate == %s, "
            "closest timestamp == %" PRId64 ").\n",
            seq_pts + ts_diff,
            HdmvFrameRateCodeStr(video_desc.frame_rate),
            exp_pts
          );
      }

      int64_t ig_decode_duration = _computeIGDisplaySetDecodeDuration(
        cur_ds,
        &ctx->epoch,
        0.f,
        true
      );
      if (ig_decode_duration < 0)
        LIBBLU_HDMV_CK_ERROR_RETURN(
          "Unable to compute IG Display Set minimal decode duration.\n"
        );

      /**
       * DTS(EPOCH_m[DS_n[ICS]]) + IG_DECODE_DURATION(EPOCH_m[DS_n])
       * <= PTS(EPOCH_m[DS_n[ICS]])
       */
      if (seq_dts + ig_decode_duration > seq_pts)
        LIBBLU_HDMV_CK_TC_FAIL_RETURN(
          "Interactive Composition decoding duration is too short, "
          "the Display Set cannot be decoded correctly "
          "(DTS[DS_n[ICS]] == %" PRId64 ", "
          "PTS[DS_n[ICS]] == %" PRId64 ", "
          "difference == %" PRId64 ", "
          "IG_DECODE_DURATION == %" PRId64 ").\n",
          seq_dts + ts_diff,
          seq_pts + ts_diff,
          seq_pts - seq_dts,
          ig_decode_duration
        );

      if (0 < cur_ds->ds_idx) {
        HdmvDSState * prev_ds = getPrevDSHdmvContext(ctx);
        HdmvSequencePtr prev_ics_seq = getICSSequenceHdmvDSState(prev_ds);
        if (NULL == prev_ics_seq)
          return -1; // Unexpected missing prev ICS/PCS

        /* PTS(DS_n-1[ICS]) <= DTS(DS_n[ICS]) */
        if (prev_ics_seq->pts > seq_dts)
          LIBBLU_HDMV_CK_TC_FAIL_RETURN(
            "Interactive Composition presentation times too close together, "
            "previous DS Interactive Composition presentation timestamp is "
            "spaced by less than current IC required decode time "
            "(PTS[DS_n-1[ICS]] == %" PRId64 ", "
            "DTS[DS_n[ICS]] == %" PRId64 ").\n",
            prev_ics_seq->pts + ts_diff,
            seq_dts + ts_diff
          );
      }

      /**
       * PTS(EPOCH_m[DS_first[ICS]])
       * < EPOCH_m[DS_n[ICS]].interactive_composition().composition_time_out_pts
       * if composition_time_out_pts is non-zero
       */
      int64_t compo_time_out_pts = seq->data.ic.composition_time_out_pts;
      if (0 < compo_time_out_pts && compo_time_out_pts <= seq_pts + ts_diff)
        LIBBLU_HDMV_CK_TC_FAIL_RETURN(
          "Invalid Interactive Composition, "
          "composition time out PTS if non-zero shall be greater than "
          "the PTS value of associated Interactive Composition Segment "
          "('composition_time_out_pts' == %" PRId64 ", "
          "PTS(ICS) == %" PRId64 ").\n",
          compo_time_out_pts,
          seq_pts + ts_diff
        );

      int64_t sel_time_out_pts = seq->data.ic.selection_time_out_pts;
      /**
       * PTS(EPOCH_m[DS_first[ICS]])
       * < EPOCH_m[DS_n[ICS]].interactive_composition().selection_time_out_pts
       * if selection_time_out_pts is non-zero
       */
      if (0 < sel_time_out_pts && sel_time_out_pts <= seq_pts + ts_diff)
        LIBBLU_HDMV_CK_TC_FAIL_RETURN(
          "Invalid Interactive Composition, "
          "selection time out PTS if non-zero shall be greater than "
          "the ICS Presentation TimeStamp "
          "('selection_time_out_pts' == %" PRId64 ", "
          "PTS(ICS) == %" PRId64 ").\n",
          sel_time_out_pts,
          seq_pts + ts_diff
        );

      if (0 < cur_ds->ds_idx && isEpochStartHdmvCDParameters(cur_ds->compo_desc)) {
        HdmvDSState * prev_ds = getPrevDSHdmvContext(ctx);
        HdmvSequencePtr ics_seq = getICSSequenceHdmvDSState(prev_ds);
        if (NULL == ics_seq)
          return -1; // Unexpected missing prev ICS

        /**
         * EPOCH_m-1[DS_n[ICS]].interactive_composition().composition_time_out_pts
         * < DTS(EPOCH_m[DS_first[ICS]])
         * if composition_time_out_pts is non-zero
         */
        int64_t prev_compo_time_out_pts = ics_seq->data.ic.composition_time_out_pts;
        if (prev_compo_time_out_pts && seq_dts + ts_diff <= prev_compo_time_out_pts)
          LIBBLU_HDMV_CK_TC_FAIL_RETURN(
            "Invalid previous Display Set Interactive Composition, "
            "composition time out PTS value of previous Epoch ICs must be "
            "less than the DTS value of the first DS ICS from the following Epoch "
            "('composition_time_out_pts' == %" PRId64 ", DTS == %" PRId64 ").\n",
            prev_compo_time_out_pts,
            seq_dts + ts_diff
          );

        /**
         * EPOCH_m-1[DS_n[ICS]].interactive_composition().selection_time_out_pts
         * < DTS(EPOCH_m[DS_first[ICS]])
         * if selection_time_out_pts is non-zero
         */
        int64_t prev_sel_time_out_pts = ics_seq->data.ic.selection_time_out_pts;
        if (prev_sel_time_out_pts && seq_dts + ts_diff <= prev_sel_time_out_pts)
          LIBBLU_HDMV_CK_WARNING(
            "Issue with previous Display Set Interactive Composition, "
            "selection time out PTS value of previous Epoch ICs should be "
            "less than the DTS value of the first DS ICS from the following Epoch "
            "('selection_time_out_pts' == %" PRId64 ", DTS == %" PRId64 ").\n",
            prev_sel_time_out_pts,
            seq_dts + ts_diff
          );
      }
    }
    else if (HDMV_SEGMENT_TYPE_END == seq->type) {
      HdmvSequencePtr cs_seq = getICSSequenceHdmvDSState(cur_ds);
      if (NULL == cs_seq)
        return -1; // Unexpected missing ICS

      /* Sample request if not PTS(DS_n[END]) <= PTS(DS_n[ICS]) */
      if (seq_pts > cs_seq->pts)
        LIBBLU_HDMV_CK_WARNING(
          "Sample request, End of Display Set PTS value is greater than "
          "PTS value of Interactive Composition Segment "
          "(PTS(DS_n[END]) == %" PRId64 ", PTS(DS_n[ICS]) == %" PRId64 ").\n",
          seq_pts + ts_diff,
          cs_seq->pts + ts_diff
        );
    }
  }

  /* ### PGS specific constraints ### */

  if (HDMV_STREAM_TYPE_PGS == ctx->epoch.type) {
    if (HDMV_SEGMENT_TYPE_PDS == seq->type) {
      if (NULL == seq->prev_sequence) {
        /* First PDS (PDS_1) */

        HdmvSequencePtr wds_seq = getWDSSequenceHdmvDSState(cur_ds);

        /* DTS(DS_n[WDS]) <= PTS(DS_n[PDS_1]) if EXISTS(DS_n[WDS]) */
        if (NULL != wds_seq && wds_seq->dts > seq_pts)
          LIBBLU_HDMV_CK_TC_FAIL_RETURN(
            "Invalid Display Set, first Palette Definition Segment PTS value must "
            "be greater than or equal to Window Definition Segment DTS value "
            "(DTS(DS_n[WDS]) == %" PRId64 ", PTS(DS_n[PDS_1]) == %" PRId64 ").\n",
            wds_seq->dts + ts_diff,
            seq_pts + ts_diff
          );
      }
    }
    else if (HDMV_SEGMENT_TYPE_ODS == seq->type) {
      if (NULL == seq->prev_sequence) {
        /* First ODS (ODS_1) */

        HdmvSequencePtr wds_seq = getWDSSequenceHdmvDSState(cur_ds);

        /* DTS(DS_n[WDS]) <= PTS(DS_n[ODS_1]) if EXISTS(DS_n[WDS]) */
        if (NULL != wds_seq && wds_seq->dts > seq_pts)
          LIBBLU_HDMV_CK_TC_FAIL_RETURN(
            "Invalid Display Set, first Object Definition Segment PTS value must "
            "be greater than or equal to Window Definition Segment DTS value "
            "(DTS(DS_n[WDS]) == %" PRId64 ", PTS(DS_n[ODS_1]) == %" PRId64 ").\n",
            wds_seq->dts + ts_diff,
            seq_pts + ts_diff
          );
      }
    }
    else if (HDMV_SEGMENT_TYPE_PCS == seq->type) {

      /* PTS(EPOCH_m[DS_n[PCS]]) alignment */
      int64_t exp_pts;
      if (!isFrameTcAlignedHdmvVDParameters(video_desc, seq->pts + ts_diff, &exp_pts))
        LIBBLU_HDMV_CK_TC_FAIL_RETURN(
          "Invalid Display Set, Presentation Composition Segment PTS "
          "value must be aligned to a video frame timestamp "
          "(PTS(DS_n[PCS]) == %" PRId64 ", "
          "frame_rate == %s, "
          "closest timestamp == %" PRId64 ").\n",
          seq_pts + ts_diff,
          HdmvFrameRateCodeStr(video_desc.frame_rate),
          exp_pts
        );

      if (0 < cur_ds->ds_idx) {
        HdmvDSState * prev_ds = getPrevDSHdmvContext(ctx);

        HdmvSequencePtr prev_pcs_seq = getPCSSequenceHdmvDSState(prev_ds);
        if (NULL == prev_pcs_seq)
          return -1; // Unexpected missing PCS

        if (!seq->data.pc.palette_update_flag) {
          /* NOT a Palette update only Display Update */
          HdmvSequencePtr wds_seq = getWDSSequenceHdmvDSState(cur_ds);
          if (NULL == wds_seq)
            LIBBLU_HDMV_CK_ERROR_RETURN(
              "Internal error, "
              "unexpected missing WDS for a previous Display Update DS.\n"
            );

          /**
           * PTS(DS_n-1[PCS])
           * + ceil((90000 * SUM_{i=0}^{i<DS_n-1[WDS].number_of_windows}(
           *     SIZE(DS_n[WDS].window[i]))) / 256000000)
           * <= PTS(DS_n[PCS])
           */
          int64_t win_fill_dur = getFillDurationHdmvWD(wds_seq->data.wd);
          if (prev_pcs_seq->pts + win_fill_dur > seq_pts)
            LIBBLU_HDMV_CK_TC_FAIL_RETURN(
              "Invalid Display Set, Presentation Composition Segment PTS value must "
              "be greater than or equal to the previous Display Set PCS PTS "
              "value plus the amount of time required to fill previous Display "
              "Set windows (PTS(DS_n-1[PCS]) == %" PRId64 ", "
              "previous windows fill duration == %" PRId64 ", "
              "PTS(DS_n[PCS]) == %" PRId64 ").\n",
              prev_pcs_seq->pts + ts_diff,
              win_fill_dur,
              seq_pts + ts_diff
            );
        }
      }
      else if (HDMV_SEGMENT_TYPE_WDS == seq->type) {
        HdmvSequencePtr pcs_seq = getPCSSequenceHdmvDSState(cur_ds);
        if (NULL == pcs_seq)
          return -1; // Unexpected missing PCS

        /* DTS(DS_n[PCS]) <= PTS(DS_n[WDS]) */
        if (pcs_seq->dts > seq_pts)
          LIBBLU_HDMV_CK_TC_FAIL_RETURN(
            "Invalid Display Set, Window Definition Segment PTS value must "
            "be greater than or equal to Presentation Composition Segment DTS value "
            "(DTS(DS_n[PCS]) == %" PRId64 ", PTS(DS_n[WDS]) == %" PRId64 ").\n",
            pcs_seq->pts + ts_diff,
            seq_pts + ts_diff
          );

        /**
         * PTS(DS_n[WDS]) =
         * PTS(DS_n[PCS])
         * - ceil((90000 * SUM_{i=0}^{i<DS_n[WDS].number_of_windows}(
         *     SIZE(DS_n[WDS].window[i]))) / 256000000)
         */
        int64_t win_fill_dur = getFillDurationHdmvWD(seq->data.wd);
        if (seq_pts != pcs_seq->pts - win_fill_dur)
          LIBBLU_HDMV_CK_TC_FAIL_RETURN(
            "Invalid Display Set, Window Definition Segment PTS value must "
            "be equal to Presentation Composition Segment PTS value "
            "minus the amount of time required to fill windows "
            "(DTS(DS_n[WDS]) == %" PRId64 ", "
            "windows fill duration == %" PRId64 ", "
            "PTS(DS_n[PCS]) == %" PRId64 ").\n",
            seq_pts + ts_diff,
            win_fill_dur,
            pcs_seq->pts + ts_diff
          );
      }
    }
    else if (HDMV_SEGMENT_TYPE_END == seq->type) {
      /* END */

      HdmvSequencePtr cs_seq = getPCSSequenceHdmvDSState(cur_ds);
      if (NULL == cs_seq)
        return -1; // Unexpected missing PCS

      /* Sample request if not PTS(DS_n[END]) <= PTS(DS_n[PCS]) */
      if (seq_pts > cs_seq->pts)
        LIBBLU_HDMV_CK_WARNING(
          "Sample request, End of Display Set PTS value is greater than "
          "PTS value of Interactive Composition Segment "
          "(PTS(DS_n[END]) == %" PRId64 ", PTS(DS_n[ICS]) == %" PRId64 ").\n",
          seq_pts + ts_diff,
          cs_seq->pts + ts_diff
        );

      HdmvSequencePtr wds_seq = getWDSSequenceHdmvDSState(cur_ds);

      /* DTS(DS_n[WDS]) <= PTS(DS_n[END]) if EXISTS(DS_n[WDS]) */
      if (NULL != wds_seq && wds_seq->dts > seq_pts)
        LIBBLU_HDMV_CK_TC_FAIL_RETURN(
          "Invalid Display Set, End of Display Set Segment PTS value must "
          "be greater than or equal to Window Definition Segment DTS value "
          "(DTS(DS_n[WDS]) == %" PRId64 ", PTS(DS_n[END]) == %" PRId64 ").\n",
          wds_seq->dts + ts_diff,
          seq_pts + ts_diff
        );
    }
  }

  return 0;
}

static int _computeTimestampsDisplaySet(
  HdmvContext * ctx,
  int64_t pres_time
)
{
  LIBBLU_HDMV_COM_DEBUG(
    "   Requested presentation time: %" PRId64 " ticks.\n",
    pres_time
  );

  int64_t decode_dur;
  if (HDMV_STREAM_TYPE_IGS == ctx->epoch.type)
    decode_dur = _computeCurIGDisplaySetDecodeDuration(ctx);
  else // HDMV_STREAM_TYPE_PGS == ctx->epoch.type
    decode_dur = _computeCurPGDisplaySetDecodeDuration(ctx);
  if (decode_dur < 0)
    return -1;

  LIBBLU_HDMV_COM_DEBUG(
    "   DS decode duration: %" PRId64 " ticks.\n",
    decode_dur
  );

  if (0 == ctx->nb_DS) {
    /* First display set, init reference clock to handle negative timestamps. */
    if (pres_time < decode_dur) {
      LIBBLU_HDMV_COM_DEBUG(
        "   Set reference timestamp: %" PRId64 " ticks.\n",
        decode_dur - pres_time
      );

      ctx->param.ref_timestamp = decode_dur - pres_time;
    }
  }
  else {
    /* Not the first DS, check its decoding period does not overlap previous DS one. */
    if (pres_time - decode_dur < ctx->param.last_ds_pres_time) {
      if (HDMV_STREAM_TYPE_IGS == ctx->epoch.type)
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

  /* Apply timestamps */
  int ret;
  int64_t end_time;
  if (HDMV_STREAM_TYPE_IGS == ctx->epoch.type)
    ret = _applyTimestampsCurIGDisplaySet(ctx, pres_time, decode_dur, &end_time);
  else // HDMV_STREAM_TYPE_PGS == ctx->epoch.type
    ret = _applyTimestampsCurPGDisplaySet(ctx, pres_time, decode_dur, &end_time);
  if (ret < 0)
    return -1;

  /* Check timestamps DS overlap conditions */
  HdmvDSState * cur_ds = getCurDSHdmvContext(ctx);
  if (_checkEarliestDTSPTSValueDS(cur_ds) < 0)
    return -1;

  /* Optionally check that the values are compliant */
  if (ctx->options.enable_timestamps_debug) {
    if (!ctx->param.raw_stream_input_mode) {
      LIBBLU_HDMV_COM_DEBUG("   Compare with original timestamps:\n");
      _debugCompareComputedTimestampsCurDS(ctx);
    }

    /* Check DTS/PTS values according to HDMV contraints */
    for (hdmv_segtype_idx idx = 0; idx < HDMV_NB_SEGMENT_TYPES; idx++) {
      HdmvSequencePtr seq = getFirstSequenceByIdxHdmvDSState(cur_ds, idx);
      for (unsigned seq_i = 0; NULL != seq; seq_i++, seq = seq->next_sequence) {
        if (_checkDTSPTSValue(ctx, seq, true) < 0)
          LIBBLU_HDMV_CK_ERROR_RETURN(
            "Error while checking %s #%u from DS #%u\n",
            segmentTypeIndexStr(idx),
            seq_i,
            cur_ds->ds_idx
          );
      }
    }
  }

  ctx->param.last_ds_pres_time = pres_time;
  ctx->param.last_ds_end_time  = end_time;
  return 0;
}

static int _copySegmentsTimestampsDisplaySetHdmvContext(
  HdmvContext * ctx
)
{
  const HdmvDSState * cur_ds = getCurDSHdmvContext(ctx);
  int64_t ref_timestamp = ctx->param.ref_timestamp;
  int64_t last_pts      = ctx->param.last_ds_pres_time;

  for (hdmv_segtype_idx idx = 0; idx < HDMV_NB_SEGMENT_TYPES; idx++) {
    bool disp_list_name = false;

    HdmvSequencePtr seq = getFirstSequenceByIdxHdmvDSState(cur_ds, idx);
    for (unsigned i = 0; NULL != seq; i++, seq = seq->next_sequence) {
      HdmvSegmentParameters segment = seq->segments->param;

      int64_t pts = segment.timestamps_header.pts - ctx->param.initial_delay;
      seq->pts = pts + ref_timestamp;
      int64_t dts = segment.timestamps_header.dts - ctx->param.initial_delay;
      seq->dts = dts + ref_timestamp;

      last_pts = MAX(last_pts, seq->pts);

      if (!disp_list_name) {
        LIBBLU_HDMV_COM_DEBUG("   - %s:\n", HdmvSegmentTypeStr(seq->type));
        disp_list_name = true;
      }

      LIBBLU_HDMV_COM_DEBUG(
        "    - %u) PTS: %" PRId64 " DTS: %" PRId64 " (%" PRId64 ") ",
        i,
        seq->pts - ctx->param.ref_timestamp,
        seq->dts - ctx->param.ref_timestamp,
        (0 != segment.timestamps_header.dts) ? seq->pts - seq->dts : 0
      );

      switch (seq->type) {
      case HDMV_SEGMENT_TYPE_ODS:
        LIBBLU_HDMV_COM_DEBUG_NH(
          "('object_id': 0x%04" PRIX16 ", "
          "'object_width': %" PRIu16 ", "
          "'object_height': %" PRIu16 ")\n",
          seq->data.od.object_descriptor.object_id,
          seq->data.od.object_width,
          seq->data.od.object_height
        );
        break;
      default:
        LIBBLU_HDMV_COM_DEBUG_NH("\n");
      }
    }
  }

  /* Check DTS/PTS values according to HDMV contraints */
  for (hdmv_segtype_idx type_idx = 0; type_idx < HDMV_NB_SEGMENT_TYPES; type_idx++) {
    HdmvSequencePtr seq = getFirstSequenceByIdxHdmvDSState(cur_ds, type_idx);
    for (unsigned seq_i = 0; NULL != seq; seq_i++, seq = seq->next_sequence) {
      if (_checkDTSPTSValue(ctx, seq, false) < 0)
        LIBBLU_HDMV_CK_ERROR_RETURN(
          "Error while checking %s #%u from DS #%u\n",
          segmentTypeIndexStr(type_idx),
          seq_i,
          cur_ds->ds_idx
        );
    }
  }

  ctx->param.last_ds_pres_time = last_pts;
  return 0;
}

int _setTimestampsDisplaySet(
  HdmvContext * ctx,
  int64_t pres_time
)
{
  if (ctx->param.force_retiming) {
    LIBBLU_HDMV_COM_DEBUG("  Compute timestamps:\n");
    return _computeTimestampsDisplaySet(ctx, pres_time);
  }

  LIBBLU_HDMV_COM_DEBUG("  Copying timestamps from headers:\n");
  return _copySegmentsTimestampsDisplaySetHdmvContext(ctx);
}

int _insertSegmentInScript(
  HdmvContext * ctx,
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

  assert(0 == (pts >> 33));
  assert(!dts_present || 0 == (dts >> 33));

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
  HdmvContext * ctx,
  HdmvDSState * epoch
)
{

  for (hdmv_segtype_idx idx = 0; idx < HDMV_NB_SEGMENT_TYPES; idx++) {
    HdmvSequencePtr seq = getFirstSequenceByIdxHdmvDSState(epoch, idx);

    for (; NULL != seq; seq = seq->next_sequence) {
      uint64_t pts = (uint64_t) seq->pts;
      uint64_t dts = (uint64_t) seq->dts;

      assert(segmentTypeIndexHdmvContext(seq->type) == idx);

      for (HdmvSegmentPtr seg = seq->segments; NULL != seg; seg = seg->next_segment) {
        if (_insertSegmentInScript(ctx, seg->param, pts, dts) < 0)
          return -1;
      }
    }
  }

  return 0;
}

static int _getCurDSPresentationTime(
  HdmvContext * ctx,
  int64_t * pres_time_ret
)
{
  /**
   * Return presentation time as an offset from initial_delay.
   * ie. an offset from the first possible presentation timestamp.
   */

  if (ctx->param.raw_stream_input_mode) {
    int64_t pres_time;
    if (getHdmvTimecodes(&ctx->timecodes, &pres_time) < 0)
      return -1;

    if (pres_time < ctx->param.initial_delay)
      LIBBLU_HDMV_TC_ERROR_RETURN(
        "Unexpected negative presentation time, "
        "initial delay value is too high (%" PRId64 " < %" PRId64 ").\n",
        pres_time,
        ctx->param.initial_delay
      );

    if (NULL != pres_time_ret)
      *pres_time_ret = pres_time - ctx->param.initial_delay;
    return 0;
  }

  return _getDisplaySetPresentationTimeCurDS(ctx, pres_time_ret);
}

int completeCurDSHdmvContext(
  HdmvContext * ctx
)
{
  HdmvDSState * cur_ds = getCurDSHdmvContext(ctx);

  if (cur_ds->init_usage == HDMV_DS_COMPLETED)
    return 0; // This shall only happen at stream end
  if (cur_ds->init_usage == HDMV_DS_NON_INITIALIZED)
    LIBBLU_HDMV_COM_ERROR_RETURN(
      "Unexpected completion of a non-initialized DS.\n"
    );

  LIBBLU_HDMV_COM_DEBUG("Processing complete Display Set...\n");
  _printDisplaySetContent(cur_ds, ctx->epoch.type);

  /* Check completion of possible sequence of segments. */
  if (_checkCompletionOfEachSequence(cur_ds)) {
    /* Presence of non completed sequences, building list string. */
    char names[HDMV_NB_SEGMENT_TYPES * SEGMENT_TYPE_IDX_STR_SIZE];
    _getNonCompletedSequencesNames(names, cur_ds);

    LIBBLU_HDMV_COM_ERROR_RETURN(
      "Non-completed segment sequence(s) present in DS: %s.\n",
      names
    );
  }

  if (!cur_ds->total_nb_seq) {
    // This shall only happen at stream end
    LIBBLU_HDMV_COM_DEBUG(" Empty Display Set, skipping.\n");
    return 0;
  }

  /* Get Display Set presentation time stamp */
  int64_t pres_time;
  if (_getCurDSPresentationTime(ctx, &pres_time) < 0)
    return -1;

  cur_ds->epoch_idx = ctx->nb_epochs;
  cur_ds->ds_idx    = ctx->nb_DS;

  LIBBLU_HDMV_COM_DEBUG(" Checking Display Set.\n");
  if (ctx->is_dup_DS) {
    /* Check non-Display Update DS */
    const HdmvDSState * prev_ds_state = getPrevDSHdmvContext(ctx);
    if (checkDuplicatedDSHdmvDSState(prev_ds_state, cur_ds, &ctx->epoch) < 0)
      return -1;
  }
  else {
    /* Check DS and update Epoch memory state */
    if (checkAndUpdateHdmvDSState(cur_ds, &ctx->epoch, pres_time) < 0)
      return -1;

    /* Check Epoch Decoded Objects buffer (DB) occupancy */
    if (checkObjectsBufferingHdmvEpochState(&ctx->epoch) < 0)
      return -1;
  }

  /* Set decoding duration/timestamps: */
  LIBBLU_HDMV_COM_DEBUG(" Set timestamps of the Display Set:\n");
  if (_setTimestampsDisplaySet(ctx, pres_time) < 0)
    return -1;

  LIBBLU_HDMV_COM_DEBUG(
    " Registering %u sequences...\n",
    cur_ds->total_nb_seq
  );

  if (_registeringSegmentsDisplaySet(ctx, cur_ds) < 0)
    return -1;

  cur_ds->init_usage = HDMV_DS_COMPLETED;

  LIBBLU_HDMV_COM_DEBUG(" Done.\n");

  if (HDMV_COMPO_STATE_EPOCH_START == cur_ds->compo_desc.composition_state)
    ctx->nb_epochs++;
  ctx->nb_DS++;

  return 0;
}

static HdmvSequencePtr _initNewSequenceInCurEpoch(
  HdmvContext * ctx,
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

  HdmvDSState * cur_ds = getCurDSHdmvContext(ctx);

  /* Check the number of already parsed sequences */
  unsigned seq_type_max_nb_DS = ctx->seq_nb_limit_per_DS[idx];
  unsigned seq_type_cur_nb_DS = cur_ds->sequences[idx].ds_nb_seq;

  if (seq_type_max_nb_DS <= seq_type_cur_nb_DS) {
    if (!seq_type_max_nb_DS)
      LIBBLU_HDMV_COM_ERROR_NRETURN(
        "Forbidden %s segment type in a %s stream.\n",
        HdmvSegmentTypeStr(segment.desc.segment_type),
        HdmvStreamTypeStr(ctx->epoch.type)
      );
    LIBBLU_HDMV_COM_ERROR_NRETURN(
      "Too many %ss in one display set.\n",
      HdmvSegmentTypeStr(segment.desc.segment_type)
    );
  }

  /* Creating a new orphan sequence */
  if (NULL == (seq = createHdmvSequence()))
    return NULL;
  seq->type = segment.desc.segment_type;

  /* Set as the pending sequence */
  _setPendingSequence(ctx, idx, seq);

  return seq;
}

HdmvSequencePtr addSegToSeqDisplaySetHdmvContext(
  HdmvContext * ctx,
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
  HdmvSegmentPtr seg = createHdmvSegment(seg_param);
  if (NULL == seg)
    return NULL;

  /* Attach the newer sequence (to the last sequence in list) */
  if (NULL == seq->segments) /* Or as the new list header if its empty */
    seq->segments = seg;
  else
    seq->last_segment->next_segment = seg;
  seq->last_segment = seg;

  return seq;
}

int completeSeqDisplaySetHdmvContext(
  HdmvContext * ctx,
  hdmv_segtype_idx idx
)
{
  /* Check if index correspond to a valid pending sequence */
  HdmvSequencePtr sequence = _getPendingSequence(ctx, idx);
  if (NULL == sequence)
    LIBBLU_HDMV_COM_ERROR_RETURN("No sequence to complete.\n");

  /* Add the sequence to the list of DS completed sequences */
  addDSSequenceByIdxHdmvDSState(getCurDSHdmvContext(ctx), idx, sequence);

  incrementSequencesNbDSHdmvContext(ctx, idx);
  incrementSequencesNbEpochHdmvContext(ctx, idx);

  /* Reset pending sequence */
  _setPendingSequence(ctx, idx, NULL);

  return 0;
}