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
  const LibbluStreamPtr * es_streams_arr,
  unsigned nb_es_streams
)
{
  uint64_t initial_delay = 0;

  for (unsigned i = 0; i < nb_es_streams; i++)
    initial_delay = MAX(initial_delay, es_streams_arr[i]->es.PTS_reference);

  return 300ull * initial_delay; // Convert to 27MHz
}

static int _addSystemStreamToContext(
  LibbluMuxingContextPtr ctx,
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
    .tsPt = ctx->currentStcTs,
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
    ctx->systemStreamsHeap,
    timing,
    sys_stream
  );
}

static int _initPatSystemStream(
  LibbluMuxingContextPtr ctx
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

  LibbluStreamPtr stream;
  if (NULL == (stream = createLibbluStream(TYPE_PAT, PAT_PID)))
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
  LibbluMuxingContextPtr ctx
)
{
  /* TODO: Move to MuxingSettings. */
  static const uint16_t main_prog_number = 0x0001;
  static const uint16_t PMT_pid = PMT_PID;

  uint16_t PCR_pid;
  if (ctx->settings.options.pcrOnESPackets)
    PCR_pid = ctx->settings.options.pcrPID;
  else
    PCR_pid = PCR_PID;

  unsigned nb_ES = nbESLibbluMuxingContext(ctx);
  assert(nb_ES <= LIBBLU_MAX_NB_STREAMS);

  LibbluESProperties ES_props[LIBBLU_MAX_NB_STREAMS];
  LibbluESFmtProp ES_fmt_spec_props[LIBBLU_MAX_NB_STREAMS];
  uint16_t ES_pids[LIBBLU_MAX_NB_STREAMS];
  for (unsigned i = 0; i < nb_ES; i++) {
    ES_props[i] = ctx->elementaryStreams[i]->es.prop;
    ES_fmt_spec_props[i] = ctx->elementaryStreams[i]->es.fmt_prop;
    ES_pids[i] = ctx->elementaryStreams[i]->pid;
  }

  PmtParameters param;
  int ret = preparePMTParam(
    &param,
    ES_props,
    ES_fmt_spec_props,
    ES_pids,
    nb_ES,
    ctx->settings.dtcpParameters,
    main_prog_number,
    PCR_pid
  );
  if (ret < 0)
    return -1;
  ctx->pmtParam = param;

  LibbluStreamPtr stream;
  if (NULL == (stream = createLibbluStream(TYPE_PMT, PMT_pid)))
    return -1;

  if (preparePMTSystemStream(&stream->sys, &param) < 0)
    goto free_return;

  if (_addSystemStreamToContext(ctx, stream) < 0)
    goto free_return;

  ctx->pmt = stream;
  return 0;

free_return:
  destroyLibbluStream(stream);
  return -1;
}

static int _initPcrSystemStream(
  LibbluMuxingContextPtr ctx
)
{
  /* NOTE: Prepared regardless pcrOnESPackets to follow timings. */
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
  LibbluMuxingContextPtr ctx
)
{
  /* TODO: Move to MuxingSettings. */
  static const uint16_t main_prog_number = 0x0001;

  SitParameters param;
  prepareSITParam(&param, ctx->settings.targetMuxingRate, main_prog_number);
  ctx->sitParam = param;

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
  LibbluMuxingContextPtr ctx
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
  LibbluMuxingContextPtr ctx
)
{
  return !isVoidBufModelNode(ctx->tStdModel);
}

static int _initSystemStreams(
  LibbluMuxingContextPtr ctx
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
      ctx->tStdSystemBuffersList,
      ctx->tStdModel,
      ctx->currentStcTs,
      ctx->settings.targetMuxingRate
    );
  }

  return 0;
}

static void _updateCurrentStcLibbluMuxingContext(
  LibbluMuxingContextPtr ctx,
  const double value
)
{
  ctx->currentStc = value;
  ctx->currentStcTs = (uint64_t) MAX(0ull, value);
}

static void _computeInitialTimings(
  LibbluMuxingContextPtr ctx
)
{
  uint64_t initial_PT = ctx->settings.initialPresentationTime;

  LIBBLU_DEBUG_COM(
    "User requested initial Presentation Time: %" PRIu64 " (27MHz clock).\n",
    initial_PT
  );

  /* Define transport stream timestamps. */
  ctx->byteStcDuration = 8.0 * MAIN_CLOCK_27MHZ / ctx->settings.targetMuxingRate;
  ctx->tpStcDuration = TP_SIZE * ctx->byteStcDuration;
  ctx->tpStcDuration_floor = (uint64_t) floor(ctx->tpStcDuration);
  ctx->tpStcDuration_ceil  = (uint64_t) ceil(ctx->tpStcDuration);

  /* Compute minimal decoding delay due to ESs first DTS/PTS difference */
  uint64_t initial_decoding_delay = _getMinInitialDelay(
    ctx->elementaryStreams,
    nbESLibbluMuxingContext(ctx)
  );

  /* Initial timing model delay between muxer and demuxer STCs */
  uint64_t STD_buffering_delay = (uint64_t) ceil(
    ctx->settings.initialTStdBufDuration * MAIN_CLOCK_27MHZ
  );

  if (initial_PT < initial_decoding_delay + STD_buffering_delay) {
    LIBBLU_WARNING(
      "Alignment of start PCR to minimal initial stream buffering delay "
      "to avoid negative PCR values.\n"
    );
    initial_PT = initial_decoding_delay + STD_buffering_delay;
  }

  /* Compute initial muxing STC value as user choosen value minus delays. */
  _updateCurrentStcLibbluMuxingContext(
    ctx,
    ROUND_90KHZ_CLOCK(initial_PT - initial_decoding_delay) - STD_buffering_delay
  );

  LIBBLU_DEBUG_COM(
    "Start PCR: %" PRIu64 ", "
    "initialDecodingDelay: %" PRIu64 ", "
    "stdBufferDelay: %" PRIu64 ".\n",
    initial_PT,
    initial_decoding_delay,
    STD_buffering_delay
  );
  LIBBLU_DEBUG_COM(
    "Initial PCR: %f, Initial PT: %" PRIu64 ".\n",
    ctx->currentStc,
    ctx->currentStcTs
  );

  /* Define System Time Clock, added value to default ES timestamps values. */
  ctx->initial_STC = initial_PT;
  ctx->stdBufDelay = STD_buffering_delay;
}

static int _initiateElementaryStreamsTStdModel(
  LibbluMuxingContextPtr ctx
)
{

  for (unsigned i = 0; i < nbESLibbluMuxingContext(ctx); i++) {
    LibbluStreamPtr stream = ctx->elementaryStreams[i];
    LIBBLU_DEBUG_COM(
      " Init in T-STD %u stream PID=0x%04" PRIX16 " stream_type=0x%02" PRIX8 ".\n",
      i, stream->pid, stream->es.prop.coding_type
    );

    /* Create the buffering chain associated to the ES' PID */
    if (addESToBdavStd(ctx->tStdModel, &stream->es, stream->pid, ctx->currentStcTs) < 0)
      return -1;
  }

  return 0;
}

static int _computePesTiming(
  StreamHeapTimingInfos * timing,
  const LibbluES * es,
  const LibbluMuxingContextPtr ctx
)
{
  if (es->current_pes_packet.dts < ctx->stdBufDelay)
    LIBBLU_ERROR_RETURN(
      "Broken DTS/STD Buffering delay (%" PRIu64 " < %" PRIu64 ").\n",
      es->current_pes_packet.dts,
      ctx->stdBufDelay
    );
  uint64_t dts = es->current_pes_packet.dts - ctx->stdBufDelay;

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
  if (timing->pesTsNb * ctx->tpStcDuration_ceil < dts)
    timing->tsPt = dts - timing->pesTsNb * ctx->tpStcDuration_ceil;
  else
    timing->tsPt = dts;
#else
  timing->tsPt = dts;
#endif
  timing->tsPriority = 0; /* Normal priority. */

  return 0;
}

static int _genEsmsFilename(
  const lbc * streamFilepath,
  unsigned nameIncrement,
  lbc * buffer,
  size_t bufferSize
)
{
  if (0 < nameIncrement)
    return lbc_snprintf(
      buffer, bufferSize, "%" PRI_LBCS "_%3u.ess",
      streamFilepath, nameIncrement
    );

  return lbc_snprintf(
    buffer, bufferSize, "%" PRI_LBCS ".ess",
    streamFilepath
  );
}

static bool _isSharedUsedScript(
  const lbc * scriptFilepath,
  LibbluESSettings * alreadyRegisteredES,
  unsigned alreadyRegisteredESNb
)
{
  unsigned i;

  for (i = 0; i < alreadyRegisteredESNb; i++) {
    if (lbc_equal(scriptFilepath, alreadyRegisteredES[i].scriptFilepath))
      return true;
  }

  return false;
}

static int _findValidESScript(
  LibbluESSettings * settings,
  LibbluESSettings * ardyRegES,
  unsigned ardyRegESNb
)
{
  lbc scriptFilepath[PATH_BUFSIZE];
  uint64_t scriptFlags;
  unsigned increment;
  int fpSize;

  increment = 0;

  if (NULL != settings->scriptFilepath)
    fpSize = lbc_snprintf(
      scriptFilepath,
      PATH_BUFSIZE,
      "%" PRI_LBCS,
      settings->scriptFilepath
    );
  else
    fpSize = _genEsmsFilename(
      settings->filepath,
      increment++,
      scriptFilepath,
      PATH_BUFSIZE
    );

  if (fpSize < 0)
    LIBBLU_ERROR_RETURN(
      "Unable to generate a valid script filename, "
      "error while copying filepaths, %s (errno: %d).\n",
      strerror(errno), errno
    );
  if (PATH_BUFSIZE <= fpSize)
    LIBBLU_ERROR_RETURN(
      "Unable to generate a valid script filename, "
      "ES filepath length exceed limits.\n"
    );

  scriptFlags = computeFlagsLibbluESSettingsOptions(settings->options);

  while (
    isAValidESMSFile(scriptFilepath, scriptFlags, NULL) < 0
    && _isSharedUsedScript(scriptFilepath, ardyRegES, ardyRegESNb)
    && increment < 100
  ) {
    fpSize = _genEsmsFilename(
      settings->filepath,
      increment++,
      scriptFilepath,
      PATH_BUFSIZE
    );

    if (fpSize < 0)
      LIBBLU_ERROR_RETURN(
        "Unable to generate a valid script filename, "
        "error while copying filepaths, %s (errno: %d).\n",
        strerror(errno), errno
      );
    if (PATH_BUFSIZE <= fpSize)
      LIBBLU_ERROR_RETURN(
        "Unable to generate a valid script filename, "
        "ES filepath length exceed limits.\n"
      );
  }

  free(settings->scriptFilepath);
  if (NULL == (settings->scriptFilepath = lbc_strdup(scriptFilepath)))
    LIBBLU_ERROR_RETURN("Memory allocation error.\n");

  return 0;
}

static int _prepareLibbluESStream(
  LibbluMuxingContextPtr ctx,
  unsigned es_stream_id,
  bool do_force_rebuild_script
)
{
  uint16_t pid;

  LIBBLU_DEBUG_COM(" Find/check script filepath.\n");

  LibbluESSettings * es_settings_arr = ctx->settings.inputStreams;
  LibbluESSettings * es_settings = &es_settings_arr[es_stream_id];
  if (_findValidESScript(es_settings, es_settings_arr, es_stream_id) < 0)
    return -1;

  LIBBLU_DEBUG_COM(" Creation of the Elementary Stream handle.\n");

  /* Use a fake PID value (0x0000), real PID value requested later. */
  LibbluStreamPtr stream;
  if (NULL == (stream = createLibbluStream(TYPE_ES, 0x0000)))
    goto free_return;
  stream->es.settings_ref = es_settings;

  LIBBLU_DEBUG_COM(" Preparation of the ES handle.\n");

  /* Get the forced script rebuilding option */
  do_force_rebuild_script |= es_settings->options.forcedScriptBuilding;

  LIBBLU_DEBUG_COM(
    " Force rebuilding of script: %s.\n",
    BOOL_STR(do_force_rebuild_script)
  );

  LibbluESFormatUtilities utilities;
  if (prepareLibbluES(&stream->es, &utilities, do_force_rebuild_script) < 0)
   goto free_return;

  LIBBLU_DEBUG_COM(" Request and set PID value.\n");

  if (requestESPIDLibbluStream(&ctx->pidValues, &pid, stream) < 0)
    goto free_return;
  stream->pid = pid;

  ctx->elementaryStreams[es_stream_id] = stream;
  ctx->elementaryStreamsUtilities[es_stream_id] = utilities;
  return 0;

free_return:
  destroyLibbluStream(stream);
  return -1;
}

LibbluMuxingContextPtr createLibbluMuxingContext(
  LibbluMuxingSettings settings
)
{
  LibbluMuxingContextPtr ctx;
  int ret;

  if (
    !settings.nbInputStreams
    || LIBBLU_MAX_NB_STREAMS < settings.nbInputStreams
  )
    LIBBLU_ERROR_NRETURN(
      "Illegal multiplex settings, unexpected number of Elementary Streams "
      "(%u).\n",
      settings.nbInputStreams
    );

  LIBBLU_DEBUG_COM("Creating Muxing Context.\n");

  ctx = (LibbluMuxingContextPtr) malloc(sizeof(LibbluMuxingContext));
  if (NULL == ctx)
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  /* Set default fields to avoid undefined behaviour in case of error leading
  to a free_return escape. */
  *ctx = (LibbluMuxingContext) {
    .settings = settings
  };

  initLibbluPIDValues(&ctx->pidValues, setBDAVStdLibbluNumberOfESTypes);

  bool tStdBufModelEnabled = !LIBBLU_MUX_SETTINGS_OPTION(&settings, disableBufModel);
  bool forceRebuildAllScripts = LIBBLU_MUX_SETTINGS_OPTION(&settings, forcedScriptBuilding);

  /* Interpret PCR carrying options */
  ctx->pcrParam.carriedByES = LIBBLU_MUX_SETTINGS_OPTION(
    &ctx->settings, pcrOnESPackets
  );
  if (ctx->pcrParam.carriedByES) {
    ctx->pcrParam.esCarryingPID = LIBBLU_MUX_SETTINGS_OPTION(
      &ctx->settings, pcrPID
    );
    ctx->pcrParam.injectionRequired = false;
  }

  LIBBLU_DEBUG_COM("Creating System Packets streams muxing queue heap.\n");
  if (NULL == (ctx->systemStreamsHeap = createStreamHeap()))
    goto free_return;

  LIBBLU_DEBUG_COM("Creating ES streams muxing queue heap.\n");
  if (NULL == (ctx->elementaryStreamsHeap = createStreamHeap()))
    goto free_return;

  if (tStdBufModelEnabled) {
    /* If not disabled, the T-STD/BDAV-STD buffering model is builded. */
    LIBBLU_DEBUG_COM("Building the T-STD/BDAV-STD buffering model.\n");
    if (createBdavStd(&ctx->tStdModel) < 0)
      goto free_return;
    /* Create the buffers list associated to the system streams */
    if (NULL == (ctx->tStdSystemBuffersList = createBufModelBuffersList()))
      goto free_return;
  }

  LIBBLU_DEBUG_COM("Initialization of Elementary Streams.\n");
  for (unsigned i = 0; i < ctx->settings.nbInputStreams; i++) {
    if (_prepareLibbluESStream(ctx, i, forceRebuildAllScripts) < 0)
      goto free_return;
  }

  /* Compute initial timing values in accordance with each ES timings */
  LIBBLU_DEBUG_COM("Computing the initial timing values.\n");
  _computeInitialTimings(ctx);

  /* Init System streams */
  LIBBLU_DEBUG_COM("Initialization of Systeam Streams.\n");
  if (_initSystemStreams(ctx) < 0)
    goto free_return;

  /* Initiate the T-STD buffering chain with computed timings */
  if (_isEnabledTStdModelLibbluMuxingContext(ctx)) {
    LIBBLU_DEBUG_COM("Initiate the T-STD/BDAV-STD buffering model.\n");
    if (_initiateElementaryStreamsTStdModel(ctx) < 0)
      goto free_return;
  }

  /* Build the first PES packets to set mux priorities */
  LIBBLU_DEBUG_COM("Initial PES packets building.\n");
  for (unsigned i = 0; i < ctx->settings.nbInputStreams; i++) {
    StreamHeapTimingInfos timing;
    LibbluESFormatUtilities utilities;

    LibbluStreamPtr stream = ctx->elementaryStreams[i];
    utilities = ctx->elementaryStreamsUtilities[i];

    LIBBLU_DEBUG_COM(" Building the next PES packets.\n");
    ret = buildNextPesPacketLibbluES(
      &stream->es,
      stream->pid,
      ctx->initial_STC,
      utilities.preparePesHeader
    );
    switch (ret) {
    case 0: /* No more PES packet */
      LIBBLU_ERROR_FRETURN(
        "Empty script, unable to build a single PES packet, "
        "'%" PRI_LBCS "'.\n",
        stream->es.settings_ref->scriptFilepath
      );
    case 1: /* Success */
      break;
    default: /* Error */
      goto free_return;
    }

    LIBBLU_DEBUG_COM(" Use properties to set timestamps.\n");
    if (_computePesTiming(&timing, &stream->es, ctx) < 0)
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
    if (addStreamHeap(ctx->elementaryStreamsHeap, timing, stream) < 0)
      goto free_return;
  }

  LIBBLU_DEBUG_COM("Muxing context initialization success.\n");
  return ctx;

free_return:
  destroyLibbluMuxingContext(ctx);
  return NULL;
}

void destroyLibbluMuxingContext(
  LibbluMuxingContextPtr ctx
)
{
  if (NULL == ctx)
    return;

  cleanLibbluMuxingSettings(ctx->settings);
  cleanLibbluPIDValues(ctx->pidValues);
  for (unsigned i = 0; i < LIBBLU_MAX_NB_STREAMS; i++)
    destroyLibbluStream(ctx->elementaryStreams[i]);

  destroyStreamHeap(ctx->systemStreamsHeap);
  destroyStreamHeap(ctx->elementaryStreamsHeap);

  destroyLibbluStream(ctx->pat);
  destroyLibbluStream(ctx->pmt);
  destroyLibbluStream(ctx->sit);
  destroyLibbluStream(ctx->pcr);
  destroyLibbluStream(ctx->null);

  destroyBufModel(ctx->tStdModel);
  destroyBufModelBuffersList(ctx->tStdSystemBuffersList);

  free(ctx);
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
  LibbluMuxingContextPtr ctx,
  BitstreamWriterPtr output
)
{
  if (!LIBBLU_MUX_SETTINGS_OPTION(&ctx->settings, writeTPExtraHeaders))
    return 0;

  ctx->nbBytesWritten += TP_EXTRA_HEADER_SIZE;
  return writeTpExtraHeader(output, ctx->currentStcTs);
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
  LibbluMuxingContextPtr ctx
)
{
  if (!ctx->pcrParam.carriedByES)
    return false;

  ctx->pcrParam.injectionRequired = true;
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
  LibbluMuxingContextPtr ctx,
  LibbluStreamPtr stream,
  size_t size
)
{
  uint64_t delay = 0;

  int ret = checkBufModel(
    ctx->settings.options.bufModelOptions,
    ctx->tStdModel,
    ctx->currentStcTs,
    size * 8,
    ctx->settings.targetMuxingRate,
    stream,
    &delay
  );

  if (ret)
    return 0;
  return delay + 1;
}

static int _putDataToBufferingModel(
  LibbluMuxingContextPtr ctx,
  LibbluStreamPtr stream,
  size_t size
)
{
  int ret;

  LIBBLU_T_STD_VERIF_DEBUG(
    "Injection of %zu bytes of PID 0x%04" PRIX16 " at %" PRIu64 " ticks.\n",
    size, stream->pid, ctx->currentStcTs
  );

  ret = updateBufModel(
    ctx->settings.options.bufModelOptions,
    ctx->tStdModel,
    ctx->currentStcTs,
    size * 8,
    ctx->settings.targetMuxingRate,
    stream
  );
  if (ret < 0) {
    /* Error case */
    LIBBLU_ERROR(
      " -> %s PID: 0x%04" PRIX16 ".\n",
      isESLibbluStream(stream) ? "Elementary stream" : "System stream",
      stream->pid,
      ctx->currentStcTs
    );
    printBufModelBufferingChain(ctx->tStdModel);
  }

  return ret;
}

static bool _pcrInjectionRequired(
  LibbluMuxingContextPtr ctx,
  uint16_t pid
)
{
  return
    ctx->pcrParam.carriedByES
    && ctx->pcrParam.injectionRequired
    && ctx->pcrParam.esCarryingPID == pid
  ;
}

static void _increaseCurrentStcLibbluMuxingContext(
  LibbluMuxingContextPtr ctx
)
{
  _updateCurrentStcLibbluMuxingContext(
    ctx,
    ctx->currentStc + ctx->tpStcDuration
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
 * If every timestamp is greater than context 'currentStcTs' value, it means no
 * system packet is currently required and so none shall be muxed (and so this
 * function return zero). In this situation, a Elementary Stream may be
 * attempted to be muxed. If even no Elementary Stream can be muxed (see
 * associated function), a NULL packet must be added to ensure CBR mux
 * requirements. In case of VBR mux, NULL packet insertion can be skipped.
 *
 * If the lowest timestamp is less than or equal to the context 'currentStcTs'
 * value, a transport packet is generated and written (and this function
 * return one).
 *
 * In all cases, after muxing one transport packet of any type (or none if no
 * system, neither ES timestamp is reached and NULL packet insertion
 * is skipped, producing a VBR mux), PCR value (and so 'currentStcTs') must be
 * increased prior to next packet insertion using
 * #_increaseCurrentStcLibbluMuxingContext().
 */
static int _muxNextSystemStreamPacket(
  LibbluMuxingContextPtr ctx,
  BitstreamWriterPtr output
)
{
  /* Check if smallest timestamp is less than or equal to currentStcTs */
  if (!streamIsReadyStreamHeap(ctx->systemStreamsHeap, ctx->currentStcTs))
    return 0; /* Timestamp not reached, no tp to mux. */

  StreamHeapTimingInfos tp_timing;
  LibbluStreamPtr stream;
  extractStreamHeap(ctx->systemStreamsHeap, &tp_timing, &stream);

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
        ctx->currentStc, ctx->byteStcDuration
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
        ctx->currentStcTs,
        ctx->currentStcTs,
        stream->pid
      );

      ctx->nbTsPacketsMuxed++;
      ctx->nbBytesWritten += TP_SIZE;

      if (is_tstd_supported) {
        /* If the stream is buffer managed, inject the written bytes
        in the model. */
        uint8_t tp_size = hdr_size + pl_size;

        /* Register the packet */
        if (addSystemFramesToBdavStd(ctx->tStdSystemBuffersList, hdr_size, pl_size) < 0)
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

  if (addStreamHeap(ctx->systemStreamsHeap, tp_timing, stream) < 0)
    return -1;

  return is_injected_tp; /* 0: No packet written, 1: One packet written */
}

static int _retriveAssociatedESUtilities(
  const LibbluMuxingContextPtr ctx,
  LibbluStreamPtr stream,
  LibbluESFormatUtilities * utils
)
{

  for (unsigned i = 0; i < nbESLibbluMuxingContext(ctx); i++) {
    if (ctx->elementaryStreams[i] == stream) {
      if (NULL != utils)
        *utils = ctx->elementaryStreamsUtilities[i];
      return 0;
    }
  }

  return -1;
}

static int _muxNextESPacket(
  LibbluMuxingContextPtr ctx,
  BitstreamWriterPtr output
)
{
  StreamHeapTimingInfos tp_timings;
  TPHeaderParameters tp_header;
  bool is_tstd_supported;

  LibbluStreamPtr stream = NULL;
  while (
    NULL == stream
    && streamIsReadyStreamHeap(ctx->elementaryStreamsHeap, ctx->currentStcTs)
  ) {
    extractStreamHeap(ctx->elementaryStreamsHeap, &tp_timings, &stream);

    is_tstd_supported = (NULL != stream->es.lnkdBufList);

    if (is_tstd_supported) {
      bool pcr_inject_req = _pcrInjectionRequired(ctx, stream->pid);
      uint64_t pcr_val = computePcrFieldValue(ctx->currentStc, ctx->byteStcDuration);

      tp_header = prepareTPHeader(stream, pcr_inject_req, pcr_val);

      uint64_t delay = _checkBufferingModelAvailability(ctx, stream, TP_SIZE);
      if (0 < delay) {
        /* ES tp insertion leads to overflow, increase its timestamp and try
        with another ES. */

        LIBBLU_T_STD_VERIF_TEST_DEBUG(
          "Skipping injection PID 0x%04" PRIX16 " at %" PRIu64 ".\n",
          stream->pid,
          ctx->currentStcTs
        );

#if 0
        incrementTPTimestampStreamHeapTimingInfos(&tpTimeData);
#else
        tp_timings.tsPt += delay;
#endif

        if (addStreamHeap(ctx->elementaryStreamsHeap, tp_timings, stream) < 0)
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
    uint64_t pcr_val = computePcrFieldValue(ctx->currentStc, ctx->byteStcDuration);

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
    ctx->currentStcTs,
    ctx->currentStcTs,
    stream->pid,
    tp_timings.tsPt
  );

  ctx->nbTsPacketsMuxed++;
  ctx->nbBytesWritten += TP_SIZE;

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
      stream->es.lnkdBufList,
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
      "DTS: %" PRIu64 ", PTS: %" PRIu64 ", currentStcTs: %" PRIu64
      "(end of script ? %s, Queued frames nb: %zu).\n",
      stream->pid,
      stream->es.current_pes_packet.data.size,
      stream->es.current_pes_packet.dts,
      stream->es.current_pes_packet.pts,
      ctx->currentStcTs,
      BOOL_INFO(stream->es.endOfScriptReached),
      nbEntriesCircularBuffer(&stream->es.pesPacketsScriptsQueue_)
    );

#if 1
    if (_computePesTiming(&tp_timings, &stream->es, ctx) < 0)
      return -1;
#endif

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
      tpTimeData.tsPt += ctx->tpStcDuration_floor;
#endif
  }

  if (stream->pid == ctx->elementaryStreams[0]->pid) {
    /* Update progression bar : */
    ctx->progress =
      (double) (
        stream->es.current_pes_packet.pts
        - ctx->initial_STC
        + (300ull * stream->es.PTS_reference)
      ) / (300ull * stream->es.PTS_final)
    ;
  }

  if (addStreamHeap(ctx->elementaryStreamsHeap, tp_timings, stream) < 0)
    return -1;
  return 1;
}

static int _muxNullPacket(
  LibbluMuxingContextPtr ctx,
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
    ctx->currentStcTs,
    ctx->currentStcTs
  );

  ctx->nbTsPacketsMuxed++;
  ctx->nbBytesWritten += TP_SIZE;

  return 0;
}

int muxNextPacketLibbluMuxingContext(
  LibbluMuxingContextPtr ctx,
  BitstreamWriterPtr output
)
{
  int nbMuxedPackets;

  assert(NULL != ctx);
  assert(NULL != output);

  /* Try to mux System tp */
  if ((nbMuxedPackets = _muxNextSystemStreamPacket(ctx, output)) < 0)
    return -1; /* Error */
  if (0 < nbMuxedPackets)
    goto success; /* System tp muxed with success */

  /* Otherwise, try to mux a Elementary Stream tp */
  if ((nbMuxedPackets = _muxNextESPacket(ctx, output)) < 0)
    return -1; /* Error */
  if (0 < nbMuxedPackets)
    goto success; /* ES tp muxed with success */

  /* Otherwise, put a NULL packet to ensure CBR mux (or do nothing if VBR) */
  if (LIBBLU_MUX_SETTINGS_OPTION(&ctx->settings, cbrMuxing)) {
    if (_muxNullPacket(ctx, output) < 0)
      return -1; /* Error */
  }

success:
  /* Increase System Time Clock (and 'currentStcTs') */
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
  LibbluMuxingContextPtr ctx,
  BitstreamWriterPtr output
)
{
  assert(NULL != ctx);
  assert(NULL != output);

  while (ctx->nbTsPacketsMuxed % 32) {
    if (_muxNullPacket(ctx, output) < 0)
      return -1; /* Error */
    _increaseCurrentStcLibbluMuxingContext(ctx);
  }

  return 0;
}