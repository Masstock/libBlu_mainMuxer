#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "muxingContext.h"

static uint64_t _getMinInitialDelay(
  const LibbluStreamPtr *es_streams_arr,
  unsigned nb_es_streams
)
{
  uint64_t initial_delay = 0;

  for (unsigned i = 0; i < nb_es_streams; i++)
    initial_delay = MAX(initial_delay, es_streams_arr[i]->es.PTS_reference);

  return 300ull *initial_delay; // Convert to 27MHz
}

static int _addSystemStreamToContext(
  LibbluMuxingContext *ctx,
  LibbluStreamPtr sys_stream
)
{
  assert(!isESLibbluStream(sys_stream));

  int priority = 0;
  uint64_t pes_duration = PAT_DELAY;

  switch (sys_stream->type) {
  case TYPE_PCR:
    priority = 1;
    pes_duration = PCR_DELAY;
    break;

  case TYPE_SIT:
    priority = 2;
    pes_duration = SIT_DELAY;
    break;

  case TYPE_PMT:
    priority = 3;
    pes_duration = PMT_DELAY;
    break;

  case TYPE_PAT:
    priority = 4;
    pes_duration = PAT_DELAY;
    break;

  default:
    LIBBLU_WARNING("Use of an unknown type system stream.\n");
  }

  uint64_t pes_nb_tp = MAX(
    1, DIV_ROUND_UP(sys_stream->sys.table_data_length, TP_PAYLOAD_SIZE)
  );

  StreamHeapTimingInfos timing = {
    .tsPt = ctx->current_STC_TS,
    .tsPriority = priority,
    .pesDuration = pes_duration,
    .pesTsNb = pes_nb_tp,
    .tsDuration = pes_duration / pes_nb_tp
  };

  LIBBLU_DEBUG_COM(
    "Adding to context System stream PID 0x%04x: "
    "pesTsNb: %" PRIu64 ", tsPt: %" PRIu64 ", tsDuration: %" PRIu64 ".\n",
    sys_stream->pid, timing.pesTsNb, timing.tsPt, timing.tsDuration
  );

  return addStreamHeap(
    ctx->sys_stream_heap,
    timing,
    sys_stream
  );
}

static int _initPatSystemStream(
  LibbluMuxingContext *ctx
)
{
  /* TODO: Move to MuxingSettings. */
  static const uint16_t ts_id = 0x0000;
  static const unsigned nb_prgm = 2;
  static const uint16_t pmt_pids[2] = {SIT_PID, PMT_PID};

  PatParameters param;
  if (preparePATParam(&param, ts_id, nb_prgm, NULL, pmt_pids) < 0)
    return -1;
  ctx->pat_param = param;

  LibbluStreamPtr stream = createLibbluStream(TYPE_PAT, PAT_PID);
  if (NULL == stream)
    return -1;

  if (preparePATSystemStream(&stream->sys, param) < 0)
    goto free_return;

  if (_addSystemStreamToContext(ctx, stream) < 0)
    goto free_return;

  ctx->pat = stream;
  return 0;

free_return:
  destroyLibbluStream(stream);
  return -1;
}

static int _initPmtSystemStream(
  LibbluMuxingContext *ctx
)
{
  /* TODO: Move to MuxingSettings. */
  static const uint16_t main_prog_number = 0x0001;
  static const uint16_t PMT_pid = PMT_PID;

  uint16_t PCR_pid;
  if (ctx->settings_ptr->options.PCR_on_ES_packets)
    PCR_pid = ctx->settings_ptr->options.PCR_PID;
  else
    PCR_pid = PCR_PID;

  unsigned nb_ES = ctx->nb_elementary_streams;
  LibbluESProperties *ES_props          = malloc(nb_ES * sizeof(LibbluESProperties));
  LibbluESFmtProp    * ES_fmt_spec_props = malloc(nb_ES * sizeof(LibbluESFmtProp));
  uint16_t           * ES_pids           = malloc(nb_ES * sizeof(uint16_t));
  if (NULL == ES_props || NULL == ES_fmt_spec_props || NULL == ES_pids)
    LIBBLU_ERROR_RETURN("Memory allocation error.\n");

  for (unsigned i = 0; i < nb_ES; i++) {
    ES_props[i]          = ctx->elementary_streams[i]->es.prop;
    ES_fmt_spec_props[i] = ctx->elementary_streams[i]->es.fmt_prop;
    ES_pids[i]           = ctx->elementary_streams[i]->pid;
  }

  PmtParameters param;
  int ret = preparePMTParam(
    &param,
    ES_props,
    ES_fmt_spec_props,
    ES_pids,
    nb_ES,
    ctx->settings_ptr->DTCP_parameters,
    main_prog_number,
    PCR_pid
  );
  if (ret < 0)
    return -1;
  ctx->pmt_param = param;

  LibbluStreamPtr stream = createLibbluStream(TYPE_PMT, PMT_pid);
  if (NULL == stream)
    return -1;

  if (preparePMTSystemStream(&stream->sys, &param) < 0)
    goto free_return;

  if (_addSystemStreamToContext(ctx, stream) < 0)
    goto free_return;

  free(ES_props);
  free(ES_fmt_spec_props);
  free(ES_pids);

  ctx->pmt = stream;
  return 0;

free_return:
  free(ES_props);
  free(ES_fmt_spec_props);
  free(ES_pids);
  destroyLibbluStream(stream);
  return -1;
}

static int _initPcrSystemStream(
  LibbluMuxingContext *ctx
)
{
  /* NOTE: Prepared regardless PCR_on_ES_packets to follow timings. */
  LibbluStreamPtr stream;

  if (NULL == (stream = createLibbluStream(TYPE_PCR, PCR_PID)))
    return -1;

  if (preparePCRSystemStream(&stream->sys) < 0)
    goto free_return;

  if (_addSystemStreamToContext(ctx, stream) < 0)
    goto free_return;

  ctx->pcr = stream;
  return 0;

free_return:
  destroyLibbluStream(stream);
  return -1;
}

static int _initSitSystemStream(
  LibbluMuxingContext *ctx
)
{
  /* TODO: Move to MuxingSettings. */
  static const uint16_t main_prog_number = 0x0001;

  SitParameters param;
  if (prepareSITParam(&param, ctx->settings_ptr->TS_recording_rate, main_prog_number) < 0)
    return -1;
  ctx->sit_param = param;

  LibbluStreamPtr stream;
  if (NULL == (stream = createLibbluStream(TYPE_SIT, SIT_PID)))
    return -1;

  if (prepareSITSystemStream(&stream->sys, &param) < 0)
    goto free_return;

  if (_addSystemStreamToContext(ctx, stream) < 0)
    goto free_return;

  ctx->sit = stream;
  return 0;

free_return:
  destroyLibbluStream(stream);
  return -1;
}

static int _initNullSystemStream(
  LibbluMuxingContext *ctx
)
{
  LibbluStreamPtr stream;

  if (NULL == (stream = createLibbluStream(TYPE_NULL, NULL_PID)))
    return -1;

  if (prepareNULLSystemStream(&stream->sys) < 0)
    goto free_return;

  ctx->null = stream;
  return 0;

free_return:
  destroyLibbluStream(stream);
  return -1;
}

/** \~english
 * \brief Return if the MPEG-TS T-STD / BDAV-STD buffering model is in use.
 *
 * \param ctx Muxer working context.
 * \return true The buffering model is active and must be used to monitor
 * buffering accuracy and compliance of generated transport stream.
 * \return false No buffering model is currently in use. Multiplexing is done
 * regardless of buffering considerations.
 */
static bool _isEnabledTStdModelLibbluMuxingContext(
  LibbluMuxingContext *ctx
)
{
  return !isVoidBufModelNode(ctx->t_STD_model);
}

static int _initSystemStreams(
  LibbluMuxingContext *ctx
)
{
  if (_initPatSystemStream(ctx) < 0)
    return -1;

  if (_initPmtSystemStream(ctx) < 0)
    return -1;

  if (_initPcrSystemStream(ctx) < 0)
    return -1;

  if (_initSitSystemStream(ctx) < 0)
    return -1;

  if (_initNullSystemStream(ctx) < 0)
    return -1;

  if (_isEnabledTStdModelLibbluMuxingContext(ctx)) {
    return addSystemToBdavStd(
      ctx->t_STD_sys_buffers_list,
      ctx->t_STD_model,
      ctx->current_STC_TS,
      ctx->settings_ptr->TS_recording_rate
    );
  }

  return 0;
}

static void _updateCurrentStcLibbluMuxingContext(
  LibbluMuxingContext *ctx,
  const double value
)
{
  assert(0.f <= value && !isinf(value));
  ctx->current_STC = value;
  ctx->current_STC_TS = (uint64_t) MAX(0ull, value);
}

static void _computeInitialTimings(
  LibbluMuxingContext *ctx
)
{
  uint64_t initial_PT = ctx->settings_ptr->presentation_start_time;

  LIBBLU_DEBUG_COM(
    "User requested initial Presentation Time: %" PRIu64 " (27MHz clock).\n",
    initial_PT
  );

  assert(0 < ctx->settings_ptr->TS_recording_rate);

  /* Define transport stream timestamps. */
  ctx->byte_STC_dur = 8.0 * MAIN_CLOCK_27MHZ / ctx->settings_ptr->TS_recording_rate;
  ctx->tp_SRC_dur = TP_SIZE * ctx->byte_STC_dur;
  ctx->tp_STC_dur_floor = (uint64_t) floor(ctx->tp_SRC_dur);
  ctx->tp_STC_dur_ceil  = (uint64_t) ceil(ctx->tp_SRC_dur);

  /* Compute minimal decoding delay due to ESs first DTS/PTS difference */
  uint64_t initial_decoding_delay = _getMinInitialDelay(
    ctx->elementary_streams,
    ctx->nb_elementary_streams
  );

  /* Initial timing model delay between muxer and demuxer STCs */
  uint64_t initial_STC_delay = (uint64_t) ceil(
    ctx->settings_ptr->initial_STD_STC_delay * MAIN_CLOCK_27MHZ
  );

  if (initial_PT < initial_decoding_delay + initial_STC_delay) {
    LIBBLU_WARNING(
      "Alignment of start PCR to minimal initial stream buffering delay "
      "to avoid negative PCR values.\n"
    );
    initial_PT = initial_decoding_delay + initial_STC_delay;
  }

  /* Compute initial muxing STC value as user choosen value minus delays. */
  _updateCurrentStcLibbluMuxingContext(
    ctx,
    ROUND_90KHZ_CLOCK(initial_PT - initial_decoding_delay) - initial_STC_delay
  );

  LIBBLU_DEBUG_COM(
    "Start PCR: %" PRIu64 ", "
    "initialDecodingDelay: %" PRIu64 ", "
    "stdBufferDelay: %" PRIu64 ".\n",
    initial_PT,
    initial_decoding_delay,
    initial_STC_delay
  );
  LIBBLU_DEBUG_COM(
    "Initial PCR: %f, Initial PT: %" PRIu64 ".\n",
    ctx->current_STC,
    ctx->current_STC_TS
  );

  /* Define System Time Clock, added value to default ES timestamps values. */
  ctx->initial_STC = initial_PT;
  ctx->STD_buf_delay = initial_STC_delay;
}

static int _initiateElementaryStreamsTStdModel(
  LibbluMuxingContext *ctx
)
{

  for (unsigned i = 0; i < ctx->nb_elementary_streams; i++) {
    LibbluStreamPtr stream = ctx->elementary_streams[i];

    LIBBLU_DEBUG_COM(
      " Init in T-STD %u stream PID=0x%04" PRIX16 " stream_type=0x%02" PRIX8 ".\n",
      i, stream->pid, stream->es.prop.coding_type
    );

    /* Create the buffering chain associated to the ES' PID */
    if (addESToBdavStd(ctx->t_STD_model, &stream->es, stream->pid, ctx->current_STC_TS) < 0)
      return -1;
  }

  return 0;
}

static int _computePesTiming(
  StreamHeapTimingInfos *timing,
  const LibbluES *es,
  const LibbluMuxingContext *ctx
)
{
  if (es->current_pes_packet.dts < ctx->STD_buf_delay)
    LIBBLU_ERROR_RETURN(
      "Broken DTS/STD Buffering delay (%" PRIu64 " < %" PRIu64 ").\n",
      es->current_pes_packet.dts,
      ctx->STD_buf_delay
    );
  uint64_t dts = es->current_pes_packet.dts - ctx->STD_buf_delay;

  if (0 == (timing->bitrate = es->prop.bitrate))
    LIBBLU_ERROR_RETURN("Unable to get ES bitrate, broken script.");

  if (!es->prop.br_based_on_duration) {
    if (!es->current_pes_packet.extension_frame)
      timing->nb_pes_per_sec = es->prop.nb_pes_per_sec;
    else
      timing->nb_pes_per_sec = es->prop.nb_ped_sec_per_sec;
  }
  else {
    /**
     * Variable number of PES frames per second,
     * use rather PES size compared to bit-rate to compute PES frame duration.
     */
    timing->nb_pes_per_sec = es->prop.bitrate / (
      es->current_pes_packet.data.size * 8
    );
  }

  if (0 == timing->nb_pes_per_sec)
    LIBBLU_ERROR_RETURN("Unable to get ES nb_pes_per_sec, broken script.\n");

  size_t avg_pes_packet_size;
#if USE_AVERAGE_PES_SIZE
  if (0 == (avg_pes_packet_size = averageSizePesPacketLibbluES(es, 10)))
    LIBBLU_ERROR_RETURN("Unable to get a average PES packet size.\n");
#else
  avg_pes_packet_size = remainingPesDataLibbluES(es);
#endif

  avg_pes_packet_size = MAX(es->prop.bitrate / timing->nb_pes_per_sec / 8, avg_pes_packet_size);

  /** Compute timing values:
   * NOTE: Performed at each PES frame, preventing introduction
   * of variable parameters issues.
   */
  timing->pesDuration = floor(MAIN_CLOCK_27MHZ / timing->nb_pes_per_sec);
  timing->pesTsNb = DIV_ROUND_UP(avg_pes_packet_size, TP_PAYLOAD_SIZE);
  timing->tsDuration = timing->pesDuration / MAX(1, timing->pesTsNb);

#if SHIFT_PACKETS_BEFORE_DTS
  if (timing->pesTsNb * ctx->tp_STC_dur_ceil < dts)
    timing->tsPt = dts - timing->pesTsNb * ctx->tp_STC_dur_ceil;
  else
    timing->tsPt = dts;
#else
  timing->tsPt = dts;
#endif
  timing->tsPriority = 0; /* Normal priority. */

  return 0;
}

static int _genEsmsFilename(
  lbc ** dst_script_filepath_ret,
  const lbc *es_filepath,
  unsigned name_increment_suffix
)
{
  if (0 < name_increment_suffix)
    return lbc_asprintf(
      dst_script_filepath_ret, lbc_str("%s_%3u.ess"),
      es_filepath, name_increment_suffix
    );

  return lbc_asprintf(
    dst_script_filepath_ret, lbc_str("%s.ess"),
    es_filepath
  );
}

static bool _isSharedUsedScript(
  const lbc *created_script_filepath,
  lbc *const *already_used_script_filepaths_arr,
  unsigned already_used_script_filepaths_arr_size
)
{
  for (unsigned i = 0; i < already_used_script_filepaths_arr_size; i++) {
    if (lbc_equal(created_script_filepath, already_used_script_filepaths_arr[i]))
      return true;
  }

  return false;
}

static int _findValidESScript(
  lbc ** dst_script_filepath_arr,
  const LibbluESSettings *es_settings_arr,
  unsigned es_idx
)
{
  assert(NULL != dst_script_filepath_arr);
  assert(NULL != es_settings_arr);

  const LibbluESSettings *cur_es_settings = &es_settings_arr[es_idx];
  lbc *result_script_filepath = NULL;
  unsigned name_increment = 0;

  if (NULL != cur_es_settings->es_script_filepath) {
    // Requested script filepath
    result_script_filepath = lbc_strdup(cur_es_settings->es_script_filepath);
    if (NULL == result_script_filepath)
      LIBBLU_ERROR_RETURN("Memory allocation error.\n");
  }
  else {
    int ret = _genEsmsFilename(
      &result_script_filepath,
      cur_es_settings->es_filepath,
      name_increment++
    );
    if (ret < 0)
      LIBBLU_ERROR_RETURN("Memory allocation error.\n");
  }

  uint64_t script_expected_flags = computeFlagsLibbluESSettingsOptions(
    cur_es_settings->options
  );

  while (
    isAValidESMSFile(result_script_filepath, script_expected_flags, NULL) < 0
    && _isSharedUsedScript(result_script_filepath, dst_script_filepath_arr, es_idx)
    && name_increment < 100
  ) {
    free(result_script_filepath); // Release previously allocated name

    int ret = _genEsmsFilename(
      &result_script_filepath,
      cur_es_settings->es_script_filepath,
      name_increment++
    ); // Generate a new filename
    if (ret < 0)
      LIBBLU_ERROR_RETURN("Memory allocation error.\n");
  }

  /* Save resulting filepath */
  dst_script_filepath_arr[es_idx] = result_script_filepath;

  return 0;
}

static int _prepareLibbluESStream(
  LibbluMuxingContext *ctx,
  lbc ** script_filepath_array_ref,
  unsigned es_stream_id,
  bool do_force_rebuild_script
)
{
  assert(0 < ctx->settings_ptr->nb_used_es);

  LIBBLU_DEBUG_COM(" Find/check script filepath.\n");

  /* Find/Generate a valid script filepath */
  const LibbluESSettings *es_settings_arr = ctx->settings_ptr->es_settings;
  if (_findValidESScript(script_filepath_array_ref, es_settings_arr, es_stream_id) < 0)
    return -1;

  LIBBLU_DEBUG_COM(" Creation of the Elementary Stream handle.\n");

  /* Use a fake PID value (0x0000), real PID value requested later. */
  LibbluStreamPtr stream = createLibbluStream(TYPE_ES, 0x0000);
  if (NULL == stream)
    goto free_return;

  /* Set settings reference and script filename */
  stream->es.settings_ref    = &ctx->settings_ptr->es_settings[es_stream_id];
  stream->es.script_filepath = script_filepath_array_ref[es_stream_id];

  LIBBLU_DEBUG_COM(" Preparation of the ES handle.\n");

  /* Get the forced script rebuilding option */
  do_force_rebuild_script |= stream->es.settings_ref->options.force_script_generation;

  LIBBLU_DEBUG_COM(
    " Force rebuilding of script: %s.\n",
    BOOL_STR(do_force_rebuild_script)
  );

  LibbluESFormatUtilities utilities;
  if (prepareLibbluES(&stream->es, &utilities, do_force_rebuild_script) < 0)
   goto free_return;

  LIBBLU_DEBUG_COM(" Request and set PID value.\n");

  if (requestESPIDLibbluStream(&ctx->PID_values, &stream->pid, stream) < 0)
    goto free_return;

  ctx->elementary_streams[es_stream_id]           = stream;
  ctx->elementary_streams_utilities[es_stream_id] = utilities;
  return 0;

free_return:
  destroyLibbluStream(stream);
  return -1;
}

int initLibbluMuxingContext(
  LibbluMuxingContext *dst,
  const LibbluMuxingSettings *settings_ptr
)
{
  unsigned nb_ES = settings_ptr->nb_used_es;
  if (0 == nb_ES)
    LIBBLU_ERROR_RETURN(
      "Illegal multiplex settings, unexpected number of Elementary Streams "
      "(%u).\n",
      nb_ES
    );

  LIBBLU_DEBUG_COM("Creating Muxing Context.\n");

  /* Set default fields to avoid undefined behaviour in case of error leading
  to a free_return escape. */
  *dst = (LibbluMuxingContext) {
    .settings_ptr          = settings_ptr,
    .nb_elementary_streams = nb_ES
  };

  initLibbluPIDValues(&dst->PID_values, setBDAVStdLibbluNumberOfESTypes);

  /* Interpret PCR carrying options */
  dst->PCR_param.carried_by_ES = dst->settings_ptr->options.PCR_on_ES_packets;;
  if (dst->PCR_param.carried_by_ES) {
    dst->PCR_param.carrying_ES_PID = dst->settings_ptr->options.PCR_PID;
    dst->PCR_param.injection_required = false;
  }

  LIBBLU_DEBUG_COM("Creating System Packets streams muxing queue heap.\n");
  if (NULL == (dst->sys_stream_heap = createStreamHeap()))
    goto free_return;

  LIBBLU_DEBUG_COM("Creating ES streams muxing queue heap.\n");
  if (NULL == (dst->elm_stream_heap = createStreamHeap()))
    goto free_return;

  if (!dst->settings_ptr->options.disable_buffering_model) {
    /* If not disabled, the T-STD/BDAV-STD buffering model is builded. */
    LIBBLU_DEBUG_COM("Building the T-STD/BDAV-STD buffering model.\n");
    if (createBdavStd(&dst->t_STD_model) < 0)
      goto free_return;
    /* Create the buffers list associated to the system streams */
    if (NULL == (dst->t_STD_sys_buffers_list = createBufModelBuffersList()))
      goto free_return;
  }

  LIBBLU_DEBUG_COM("Initialization of Elementary Streams.\n");

  /* Create arrays for ES */
  dst->elementary_streams           = malloc(nb_ES *sizeof(LibbluStreamPtr));
  dst->elementary_streams_utilities = malloc(nb_ES *sizeof(LibbluESFormatUtilities));
  if (NULL == dst->elementary_streams || NULL == dst->elementary_streams_utilities)
    LIBBLU_ERROR_FRETURN("Memory allocation error.\n");

  /* Create the array to store ES scripts filenames */
  lbc ** script_filepath_array = malloc(nb_ES *sizeof(lbc *));
  if (NULL == script_filepath_array)
    LIBBLU_ERROR_FRETURN("Memory allocation error.\n");

  bool force_script_generation = dst->settings_ptr->options.force_script_generation;
    // General setting

  for (unsigned i = 0; i < settings_ptr->nb_used_es; i++) {
    if (_prepareLibbluESStream(dst, script_filepath_array, i, force_script_generation) < 0) {
      free(script_filepath_array);
      goto free_return;
    }
  }

  free(script_filepath_array); // Free the temporary filenames array
    // DO NOT free filepaths as these are used by streams

  /* Compute initial timing values in accordance with each ES timings */
  LIBBLU_DEBUG_COM("Computing the initial timing values.\n");
  _computeInitialTimings(dst);

  /* Init System streams */
  LIBBLU_DEBUG_COM("Initialization of Systeam Streams.\n");
  if (_initSystemStreams(dst) < 0)
    goto free_return;

  /* Initiate the T-STD buffering chain with computed timings */
  if (_isEnabledTStdModelLibbluMuxingContext(dst)) {
    LIBBLU_DEBUG_COM("Initiate the T-STD/BDAV-STD buffering model.\n");
    if (_initiateElementaryStreamsTStdModel(dst) < 0)
      goto free_return;
  }

  /* Build the first PES packets to set mux priorities */
  LIBBLU_DEBUG_COM("Initial PES packets building.\n");
  for (unsigned i = 0; i < settings_ptr->nb_used_es; i++) {
    LibbluStreamPtr stream            = dst->elementary_streams[i];
    LibbluESFormatUtilities utilities = dst->elementary_streams_utilities[i];

    LIBBLU_DEBUG_COM(" Building the next PES packets.\n");
    int ret = buildNextPesPacketLibbluES(
      &stream->es,
      stream->pid,
      dst->initial_STC,
      utilities.preparePesHeader
    );
    switch (ret) {
    case 0: /* No more PES packet */
      LIBBLU_ERROR_FRETURN(
        "Empty script, unable to build a single PES packet, "
        "'%" PRI_LBCS "'.\n",
        stream->es.script_filepath
      );
    case 1: /* Success */
      break;
    default: /* Error */
      goto free_return;
    }

    LIBBLU_DEBUG_COM(" Use properties to set timestamps.\n");

    StreamHeapTimingInfos timing;
    if (_computePesTiming(&timing, &stream->es, dst) < 0)
      goto free_return;

    LIBBLU_DEBUG_COM(
      "Adding to context ES PID 0x%04" PRIX16 ": "
      "pesTsNb: %" PRIu64 ", "
      "tsPt: %" PRIu64 ".\n",
      stream->pid,
      timing.pesTsNb,
      timing.tsPt
    );

    LIBBLU_DEBUG_COM(" Putting initialized ES in the ES heap.\n");
    if (addStreamHeap(dst->elm_stream_heap, timing, stream) < 0)
      goto free_return;
  }

  LIBBLU_DEBUG_COM("Muxing context initialization success.\n");
  return 0;

free_return:
  cleanLibbluMuxingContext(*dst);
  return -1;
}

/** \~english
 * \brief Write if enabled a tp_extra_header() defined in BDAV specifications.
 *
 * \param ctx Muxer working context.
 * \param output Output bitstream.
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
static int _writeTpExtraHeaderIfRequired(
  LibbluMuxingContext *ctx,
  BitstreamWriterPtr output
)
{
  if (!ctx->settings_ptr->options.write_TP_extra_headers)
    return 0;

  ctx->nb_bytes_written += TP_EXTRA_HEADER_SIZE;
  return writeTpExtraHeader(output, ctx->current_STC_TS);
}

/** \~english
 * \brief Check the required behaviour of PCR field injection.
 *
 * \param ctx Muxer working context.
 * \return true PCR is carried by a ES, no system transport packet shall be
 * emited.
 * \return false PCR is carried by independant transport packets with a
 * reserved PID, one shall be emited.
 *
 * Return true if PCR fields are carried in adaptation field of a Elementary
 * Stream transport packets. In that case, a PCR injection request is made and
 * no transport packet shall emited now.
 * Otherwise, PCR fields are carried in adaptation fields of independant
 * transport packets with a reserved PID, implemented as a system stream.
 * In that case, a system transport packet shall be emited.
 */
static bool _checkPcrInjection(
  LibbluMuxingContext *ctx
)
{
  if (!ctx->PCR_param.carried_by_ES)
    return false;

  ctx->PCR_param.injection_required = true;
  return true;
}

/** \~english
 * \brief Check if the supplied amount of bytes may produce overflow on the
 * stream associated branch of the MPEG-TS T-STD/BDAV-STD buffering model.
 *
 * \param ctx Muxer working context.
 * \param stream Studied model branch associated stream.
 * \param size Data amount in bytes.
 * \return
 *
 * the given 'stream' object is used by the modelizer to determine the flow
 * of the given bytes.
 */
static uint64_t _checkBufferingModelAvailability(
  LibbluMuxingContext *ctx,
  LibbluStreamPtr stream,
  size_t size
)
{
  uint64_t delay = 0;

  int ret = checkBufModel(
    ctx->settings_ptr->options.buffering_model_options,
    ctx->t_STD_model,
    ctx->current_STC_TS,
    size * 8,
    ctx->settings_ptr->TS_recording_rate,
    stream,
    &delay
  );

  if (ret)
    return 0;
  return delay + 1;
}

static int _putDataToBufferingModel(
  LibbluMuxingContext *ctx,
  LibbluStreamPtr stream,
  size_t size
)
{
  int ret;

  LIBBLU_T_STD_VERIF_DEBUG(
    "Injection of %zu bytes of PID 0x%04" PRIX16 " at %" PRIu64 " ticks.\n",
    size, stream->pid, ctx->current_STC_TS
  );

  ret = updateBufModel(
    ctx->settings_ptr->options.buffering_model_options,
    ctx->t_STD_model,
    ctx->current_STC_TS,
    size * 8,
    ctx->settings_ptr->TS_recording_rate,
    stream
  );
  if (ret < 0) {
    /* Error case */
    LIBBLU_ERROR(
      " -> %s PID: 0x%04" PRIX16 ".\n",
      isESLibbluStream(stream) ? "Elementary stream" : "System stream",
      stream->pid,
      ctx->current_STC_TS
    );
    printBufModelBufferingChain(ctx->t_STD_model);
  }

  return ret;
}

static bool _pcrInjectionRequired(
  LibbluMuxingContext *ctx,
  uint16_t pid
)
{
  return
    ctx->PCR_param.carried_by_ES
    && ctx->PCR_param.injection_required
    && ctx->PCR_param.carrying_ES_PID == pid
  ;
}

static void _increaseCurrentStcLibbluMuxingContext(
  LibbluMuxingContext *ctx
)
{
  assert(0.f < ctx->tp_SRC_dur && !isinf(ctx->tp_SRC_dur));
  _updateCurrentStcLibbluMuxingContext(
    ctx,
    ctx->current_STC + ctx->tp_SRC_dur
  );
}

/** \~english
 * \brief Attempt to mux a system stream packet.
 *
 * \param ctx Muxer working context.
 * \param output Output bitstream.
 * \return int Upon success, the number of packets written is returned (not
 * exceeding one). This value can be zero if no timestamp has been reached.
 * Otherwise, if an error has been encountered, a negative value is returned.
 *
 * This function look for a system stream with reached transport packet
 * emission timestamp.
 *
 * If every timestamp is greater than context 'current_STC_TS' value, it means no
 * system packet is currently required and so none shall be muxed (and so this
 * function return zero). In this situation, a Elementary Stream may be
 * attempted to be muxed. If even no Elementary Stream can be muxed (see
 * associated function), a NULL packet must be added to ensure CBR mux
 * requirements. In case of VBR mux, NULL packet insertion can be skipped.
 *
 * If the lowest timestamp is less than or equal to the context 'current_STC_TS'
 * value, a transport packet is generated and written (and this function
 * return one).
 *
 * In all cases, after muxing one transport packet of any type (or none if no
 * system, neither ES timestamp is reached and NULL packet insertion
 * is skipped, producing a VBR mux), PCR value (and so 'current_STC_TS') must be
 * increased prior to next packet insertion using
 * #_increaseCurrentStcLibbluMuxingContext().
 */
static int _muxNextSystemStreamPacket(
  LibbluMuxingContext *ctx,
  BitstreamWriterPtr output
)
{
  /* Check if smallest timestamp is less than or equal to current_STC_TS */
  if (!streamIsReadyStreamHeap(ctx->sys_stream_heap, ctx->current_STC_TS))
    return 0; /* Timestamp not reached, no tp to mux. */

  StreamHeapTimingInfos tp_timing;
  LibbluStreamPtr stream;
  extractStreamHeap(ctx->sys_stream_heap, &tp_timing, &stream);

  bool is_injected_tp = false;

  if (stream->type == TYPE_PCR && _checkPcrInjection(ctx)) {
    /* PCR system stream timestamp reached, but PCR fields are carried by
    ES transport packet and so no specific packet shall be emited. Skip
    transport packet generation. */
    incrementTPTimestampStreamHeapTimingInfos(&tp_timing);
  }
  else {
    /* Normal transport packet injection. */
    bool is_tstd_supported = false;

    is_injected_tp = true; // By default, inject the packet
    if (_isEnabledTStdModelLibbluMuxingContext(ctx)) {
      /* Check if the stream buffering is monitored */
      is_tstd_supported = isManagedSystemBdavStd(
        stream->pid,
        ctx->pat_param
      );

      if (is_tstd_supported) {
        /* Inject the packet only if it does not overflow. */
        is_injected_tp = (0 == _checkBufferingModelAvailability(
          ctx, stream, TP_SIZE
        ));
      }
    }

    if (is_injected_tp) {
      /* Only inject the packet if it does not cause overflow, or if its
      associated stream is not managed. */

      bool pcr_pres = (
        (stream->type == TYPE_PCR)
        || _pcrInjectionRequired(ctx, stream->pid)
      );
      uint64_t pcr_val = computePcrFieldValue(
        ctx->current_STC, ctx->byte_STC_dur
      );

      /* Prepare the tansport packet header */
      TPHeaderParameters tp_header = prepareTPHeader(
        stream, pcr_pres, pcr_val
      );

      /* Write if required by muxing options the tp_extra_header() */
      if (_writeTpExtraHeaderIfRequired(ctx, output) < 0)
        return -1;

      /* Write the current transport packet */
      uint8_t hdr_size, pl_size;
      if (writeTransportPacket(output, stream, &tp_header, &hdr_size, &pl_size) < 0)
        return -1;

      LIBBLU_DEBUG(
        LIBBLU_DEBUG_MUXER_DECISION, "Muxer decisions",
        "0x%" PRIX64 " - %" PRIu64 ", Sys packet muxed (0x%04" PRIX16 ").\n",
        ctx->current_STC_TS,
        ctx->current_STC_TS,
        stream->pid
      );

      ctx->nb_tp_muxed++;
      ctx->nb_bytes_written += TP_SIZE;

      if (is_tstd_supported) {
        /* If the stream is buffer managed, inject the written bytes
        in the model. */
        uint8_t tp_size = hdr_size + pl_size;

        /* Register the packet */
        if (addSystemFramesToBdavStd(ctx->t_STD_sys_buffers_list, hdr_size, pl_size) < 0)
          return -1;

        /* Put the packet data */
        if (_putDataToBufferingModel(ctx, stream, tp_size) < 0)
          return -1;
      }

      if (stream->sys.first_full_table_supplied) {
        /* Increment the timestamp only after the table has been fully
        emitted once. */
        incrementTPTimestampStreamHeapTimingInfos(&tp_timing);
      }
    }
  }

  if (addStreamHeap(ctx->sys_stream_heap, tp_timing, stream) < 0)
    return -1;

  return is_injected_tp; /* 0: No packet written, 1: One packet written */
}

static int _retriveAssociatedESUtilities(
  const LibbluMuxingContext *ctx,
  const LibbluStreamPtr stream,
  LibbluESFormatUtilities *utils
)
{

  for (unsigned i = 0; i < ctx->nb_elementary_streams; i++) {
    if (ctx->elementary_streams[i] == stream) {
      if (NULL != utils)
        *utils = ctx->elementary_streams_utilities[i];
      return 0;
    }
  }

  return -1;
}

static int _muxNextESPacket(
  LibbluMuxingContext *ctx,
  BitstreamWriterPtr output
)
{
  StreamHeapTimingInfos tp_timings;
  TPHeaderParameters tp_header;

  LibbluStreamPtr stream = NULL;
  bool is_tstd_supported = false;

  while (
    NULL == stream
    && streamIsReadyStreamHeap(ctx->elm_stream_heap, ctx->current_STC_TS)
  ) {
    extractStreamHeap(ctx->elm_stream_heap, &tp_timings, &stream);

    is_tstd_supported = (NULL != stream->es.lnkd_tstd_buf_list);

    if (is_tstd_supported) {
      bool pcr_inject_req = _pcrInjectionRequired(ctx, stream->pid);
      uint64_t pcr_val = computePcrFieldValue(ctx->current_STC, ctx->byte_STC_dur);

      tp_header = prepareTPHeader(stream, pcr_inject_req, pcr_val);

      uint64_t delay = _checkBufferingModelAvailability(ctx, stream, TP_SIZE);
      if (0 < delay) {
        /* ES tp insertion leads to overflow, increase its timestamp and try
        with another ES. */

        LIBBLU_T_STD_VERIF_TEST_DEBUG(
          "Skipping injection PID 0x%04" PRIX16 " at %" PRIu64 ".\n",
          stream->pid,
          ctx->current_STC_TS
        );

#if 0
        incrementTPTimestampStreamHeapTimingInfos(&tpTimeData);
#else
        tp_timings.tsPt += delay;
#endif

        if (addStreamHeap(ctx->elm_stream_heap, tp_timings, stream) < 0)
          return -1;
        stream = NULL; /* Reset to keep in loop */
      }
    }
  }

  if (NULL == stream)
    return 0; /* Nothing currently to mux */

  if (!is_tstd_supported) {
    /* Prepare tp header (it has already been prepared if T-STD buf model
    is used. */
    bool pcr_inject_req = _pcrInjectionRequired(ctx, stream->pid);
    uint64_t pcr_val = computePcrFieldValue(ctx->current_STC, ctx->byte_STC_dur);

    tp_header = prepareTPHeader(stream, pcr_inject_req, pcr_val);
  }

  /* Write if required by muxing options the tp_extra_header() */
  if (_writeTpExtraHeaderIfRequired(ctx, output) < 0)
    return -1;

  /* Write the current transport packet */
  uint8_t hdr_size, pl_size;
  if (writeTransportPacket(output, stream, &tp_header, &hdr_size, &pl_size) < 0)
    return -1;

  LIBBLU_DEBUG(
    LIBBLU_DEBUG_MUXER_DECISION, "Muxer decisions",
    "0x%" PRIX64 " - %" PRIu64 ", ES packet muxed "
    "(0x%04" PRIX16 ", tsPt: %" PRIu64 ").\n",
    ctx->current_STC_TS,
    ctx->current_STC_TS,
    stream->pid,
    tp_timings.tsPt
  );

  ctx->nb_tp_muxed++;
  ctx->nb_bytes_written += TP_SIZE;

  if (is_tstd_supported) {
    /* If T-STD buffer model is used, inject the written bytes in the model. */
    size_t tp_size = hdr_size + pl_size;

    LIBBLU_T_STD_VERIF_DECL_DEBUG(
      "Registering %zu+%zu=%zu bytes of TP for PID 0x%04" PRIX16 ".\n",
      hdr_size, pl_size, tp_size,
      stream->pid
    );

    /* Register the packet */
    int ret = addESTsFrameToBdavStd(
      stream->es.lnkd_tstd_buf_list,
      hdr_size,
      pl_size
    );
    if (ret < 0)
      return -1;

    /* Put the packet data */
    if (_putDataToBufferingModel(ctx, stream, tp_size) < 0)
      return -1;
  }

  /* Check remaining data in processed PES packet : */
  if (0 == remainingPesDataLibbluES(&stream->es)) {
    /* If no more data, build new PES packet */
    LibbluESFormatUtilities utilities;

    if (_retriveAssociatedESUtilities(ctx, stream, &utilities) < 0)
      LIBBLU_ERROR_RETURN("Unable to retrive ES associated utilities.\n");

    int ret = buildNextPesPacketLibbluES(
      &stream->es,
      stream->pid,
      ctx->initial_STC,
      utilities.preparePesHeader
    );
    switch (ret) {
    case 0:
      return 1; /* No more data */
    case 1:
      break; /* Next PES packet successfully builded */
    default:
      return -1; /* Error */
    }

    LIBBLU_DEBUG(
      LIBBLU_DEBUG_PES_BUILDING, "PES building",
      "PID 0x%04" PRIX16 ", %zu bytes, "
      "DTS: %" PRIu64 ", PTS: %" PRIu64 ", current_STC_TS: %" PRIu64
      "(end of script ? %s, Queued frames nb: %zu).\n",
      stream->pid,
      stream->es.current_pes_packet.data.size,
      stream->es.current_pes_packet.dts,
      stream->es.current_pes_packet.pts,
      ctx->current_STC_TS,
      BOOL_INFO(stream->es.script.end_reached),
      nbEntriesCircularBuffer(&stream->es.script.pes_packets_queue)
    );

    if (_computePesTiming(&tp_timings, &stream->es, ctx) < 0)
      return -1;

    LIBBLU_DEBUG(
      LIBBLU_DEBUG_PES_BUILDING, "PES building",
      " => pesTsNb: %" PRIu64 ", tsPt: %" PRIu64 ".\n",
      tp_timings.pesTsNb,
      tp_timings.tsPt
    );
  }
  else {
#if 1
      incrementTPTimestampStreamHeapTimingInfos(&tp_timings);
#else
      tpTimeData.tsPt += ctx->tp_STC_dur_floor;
#endif
  }

  if (stream->pid == ctx->elementary_streams[0]->pid) {
    /* Update progression bar : */
    ctx->progress =
      (double) (
        stream->es.current_pes_packet.pts
        - ctx->initial_STC
        + (300ull * stream->es.PTS_reference)
      ) / (300ull * stream->es.PTS_final)
    ;
  }

  if (addStreamHeap(ctx->elm_stream_heap, tp_timings, stream) < 0)
    return -1;
  return 1;
}

static int _muxNullPacket(
  LibbluMuxingContext *ctx,
  BitstreamWriterPtr output
)
{
  /* Prepare the tansport packet header */
  /* NOTE: Null packets cannot carry PCR */
  TPHeaderParameters tp_header = prepareTPHeader(ctx->null, false, 0);

  /* Write if required by muxing options the tp_extra_header() */
  if (_writeTpExtraHeaderIfRequired(ctx, output) < 0)
    return -1;

  if (writeTransportPacket(output, ctx->null, &tp_header, NULL, NULL) < 0)
    return -1;

  LIBBLU_DEBUG(
    LIBBLU_DEBUG_MUXER_DECISION, "Muxer decisions",
    "0x%" PRIX64 " - %" PRIu64 ", NULL packet muxed.\n",
    ctx->current_STC_TS,
    ctx->current_STC_TS
  );

  ctx->nb_tp_muxed++;
  ctx->nb_bytes_written += TP_SIZE;

  return 0;
}

int muxNextPacketLibbluMuxingContext(
  LibbluMuxingContext *ctx,
  BitstreamWriterPtr output
)
{
  int nb_muxed_packets;

  assert(NULL != ctx);
  assert(NULL != output);

  /* Try to mux System tp */
  if ((nb_muxed_packets = _muxNextSystemStreamPacket(ctx, output)) < 0)
    return -1; /* Error */
  if (0 < nb_muxed_packets)
    goto success; /* System tp muxed with success */

  /* Otherwise, try to mux a Elementary Stream tp */
  if ((nb_muxed_packets = _muxNextESPacket(ctx, output)) < 0)
    return -1; /* Error */
  if (0 < nb_muxed_packets)
    goto success; /* ES tp muxed with success */

  /* Otherwise, put a NULL packet to ensure CBR mux (or do nothing if VBR) */
  if (ctx->settings_ptr->options.CBR_mux) {
    if (_muxNullPacket(ctx, output) < 0)
      return -1; /* Error */
  }

success:
  /* Increase System Time Clock (and 'current_STC_TS') */
  _increaseCurrentStcLibbluMuxingContext(ctx);

  return 0;
}

/** \~english
 * \brief Align stream to Aligned Unit boundaries (32 TPs) as specified in BD
 * specifications.
 *
 * \param ctx
 * \param output
 * \return int
 *
 * Padding is done using Null packets.
 */
int padAlignedUnitLibbluMuxingContext(
  LibbluMuxingContext *ctx,
  BitstreamWriterPtr output
)
{
  assert(NULL != ctx);
  assert(NULL != output);

  while (ctx->nb_tp_muxed % 32) {
    if (_muxNullPacket(ctx, output) < 0)
      return -1; /* Error */
    _increaseCurrentStcLibbluMuxingContext(ctx);
  }

  return 0;
}