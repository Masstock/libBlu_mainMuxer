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
  const LibbluStreamPtr * esStreams,
  unsigned nbEsStreams
)
{

  uint64_t initialDelay = 0;
  for (unsigned i = 0; i < nbEsStreams; i++)
    initialDelay = MAX(initialDelay, esStreams[i]->es.refPts);

  return initialDelay;
}

static int _addSystemStreamToContext(
  LibbluMuxingContextPtr ctx,
  LibbluStreamPtr sysStream
)
{
  assert(!isESLibbluStream(sysStream));

  int priority;
  uint64_t pesDuration;

  switch (sysStream->type) {
    case TYPE_PCR:
      priority = 1;
      pesDuration = PCR_DELAY;
      break;

    case TYPE_SIT:
      priority = 2;
      pesDuration = SIT_DELAY;
      break;

    case TYPE_PMT:
      priority = 3;
      pesDuration = PMT_DELAY;
      break;

    case TYPE_PAT:
      priority = 4;
      pesDuration = PAT_DELAY;
      break;

    default:
      LIBBLU_WARNING("Use of an unknown type system stream.\n");
      priority = 0; /* Unkwnown system stream, normal priority */
      pesDuration = PAT_DELAY;
  }

  uint64_t pesTsNb = MAX(
    1, DIV_ROUND_UP(sysStream->sys.dataLength, TP_PAYLOAD_SIZE)
  );

  StreamHeapTimingInfos timing = {
    .tsPt = ctx->currentStcTs,
    .tsPriority = priority,
    .pesDuration = pesDuration,
    .pesTsNb = pesTsNb,
    .tsDuration = pesDuration / pesTsNb
  };

  LIBBLU_DEBUG_COM(
    "Adding to context System stream PID 0x%04x: "
    "pesTsNb: %" PRIu64 ", tsPt: %" PRIu64 ", tsDuration: %" PRIu64 ".\n",
    sysStream->pid, timing.pesTsNb, timing.tsPt, timing.tsDuration
  );

  return addStreamHeap(
    ctx->systemStreamsHeap,
    timing,
    sysStream
  );
}

static int _initPatSystemStream(
  LibbluMuxingContextPtr ctx
)
{
  PatParameters param;
  LibbluStreamPtr stream;

  /* TODO: Move to MuxingSettings. */
  static const uint16_t tsId = 0x0000;
  static const unsigned nbPrograms = 2;
  static const uint16_t pmtPids[2] = {SIT_PID, PMT_PID};

  if (preparePATParam(&param, tsId, nbPrograms, NULL, pmtPids) < 0)
    return -1;
  ctx->patParam = param;

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
  LibbluESProperties esProperties[LIBBLU_MAX_NB_STREAMS];
  LibbluESFmtSpecProp esFmtSpecProperties[LIBBLU_MAX_NB_STREAMS];
  uint16_t esStreamsPids[LIBBLU_MAX_NB_STREAMS];
  uint16_t pcrPID;

  PmtParameters param;
  LibbluStreamPtr stream;

  /* TODO: Move to MuxingSettings. */
  static const uint16_t mainProgramNb = 0x0001;
  static const uint16_t pid = PMT_PID;

  if (ctx->settings.options.pcrOnESPackets)
    pcrPID = ctx->settings.options.pcrPID;
  else
    pcrPID = PCR_PID;

  unsigned nbEsStreams = nbESLibbluMuxingContext(ctx);
  assert(nbEsStreams <= LIBBLU_MAX_NB_STREAMS);

  for (unsigned i = 0; i < nbEsStreams; i++) {
    esProperties[i] = ctx->elementaryStreams[i]->es.prop;
    esFmtSpecProperties[i] = ctx->elementaryStreams[i]->es.fmtSpecProp;
    esStreamsPids[i] = ctx->elementaryStreams[i]->pid;
  }

  int ret = preparePMTParam(
    &param,
    esProperties,
    esFmtSpecProperties,
    esStreamsPids,
    nbEsStreams,
    ctx->settings.dtcpParameters,
    mainProgramNb,
    pcrPID
  );
  if (ret < 0)
    return -1;
  ctx->pmtParam = param;

  if (NULL == (stream = createLibbluStream(TYPE_PMT, pid)))
    return -1;

  if (preparePMTSystemStream(&stream->sys, param) < 0)
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
  SitParameters param;
  LibbluStreamPtr stream;

  /* TODO: Move to MuxingSettings. */
  static const uint16_t mainProgramNb = 0x0001;

  prepareSITParam(&param, ctx->settings.targetMuxingRate, mainProgramNb);
  ctx->sitParam = param;

  if (NULL == (stream = createLibbluStream(TYPE_SIT, SIT_PID)))
    return -1;

  if (prepareSITSystemStream(&stream->sys, param) < 0)
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
  ctx->currentStcTs = (uint64_t) MAX(0, value);
}

static void _computeInitialTimings(
  LibbluMuxingContextPtr ctx
)
{
  uint64_t startPcr = ctx->settings.initialPresentationTime;

  LIBBLU_DEBUG_COM(
    "User requested startPcr: %" PRIu64 " (27MHz clock).\n",
    startPcr
  );

  /* Define transport stream timestamps. */
  ctx->byteStcDuration = 8.0 * MAIN_CLOCK_27MHZ / ctx->settings.targetMuxingRate;
  ctx->tpStcDuration = TP_SIZE * ctx->byteStcDuration;
  ctx->tpStcDuration_floor = (uint64_t) floor(ctx->tpStcDuration);
  ctx->tpStcDuration_ceil  = (uint64_t) ceil(ctx->tpStcDuration);

  /* Compute minimal decoding delay due to ESs first DTS/PTS difference */
  uint64_t initialDecodingDelay = _getMinInitialDelay(
    ctx->elementaryStreams,
    nbESLibbluMuxingContext(ctx)
  );

  /* Initial timing model delay between muxer and demuxer STCs */
  uint64_t stdBufferDelay = (uint64_t) ceil(
    ctx->settings.initialTStdBufDuration * MAIN_CLOCK_27MHZ
  );

  if (startPcr < initialDecodingDelay + stdBufferDelay) {
    LIBBLU_WARNING(
      "Alignment of start PCR to minimal initial stream buffering delay "
      "to avoid negative PCR values.\n"
    );
    startPcr = initialDecodingDelay + stdBufferDelay;
  }

  /* Compute initial muxing STC value as user choosen value minus delays. */
  _updateCurrentStcLibbluMuxingContext(
    ctx,
    ROUND_90KHZ_CLOCK(startPcr - initialDecodingDelay) - stdBufferDelay
  );

  LIBBLU_DEBUG_COM(
    "Start PCR: %" PRIu64 ", "
    "initialDecodingDelay: %" PRIu64 ", "
    "stdBufferDelay: %" PRIu64 ".\n",
    startPcr,
    initialDecodingDelay,
    stdBufferDelay
  );
  LIBBLU_DEBUG_COM(
    "Initial PCR: %f, Initial PT: %" PRIu64 ".\n",
    ctx->currentStc,
    ctx->currentStcTs
  );

  /* Define System Time Clock, added value to default ES timestamps values. */
  ctx->referentialStc = startPcr;
  ctx->stdBufDelay = stdBufferDelay;
}

static int _initiateElementaryStreamsTStdModel(
  LibbluMuxingContextPtr ctx
)
{

  for (unsigned i = 0; i < nbESLibbluMuxingContext(ctx); i++) {
    LibbluStreamPtr stream = ctx->elementaryStreams[i];

    /* Create the buffering chain associated to the ES' PID */
    if (addESToBdavStd(ctx->tStdModel, &stream->es, stream->pid, ctx->currentStcTs) < 0)
      return -1;
  }

  return 0;
}

static int _computePesTiming(
  StreamHeapTimingInfos * timing,
  const LibbluESPtr es,
  const LibbluMuxingContextPtr ctx
)
{
  uint64_t dts;
  size_t avgPesPacketSize;

  assert(ctx->stdBufDelay <= es->curPesPacket.prop.dts);
  dts = es->curPesPacket.prop.dts - ctx->stdBufDelay;

  if (0 == (timing->bitrate = es->prop.bitrate))
    LIBBLU_ERROR_RETURN("Unable to get ES bitrate, broken script.");

  if (!es->prop.bitRateBasedDurationAlternativeMode) {
    if (!es->curPesPacket.prop.extensionFrame)
      timing->pesNb = es->prop.pesNb;
    else
      timing->pesNb = es->prop.pesNbSec;
  }
  else {
    /**
     * Variable number of PES frames per second,
     * use rather PES size compared to bit-rate to compute PES frame duration.
     */
    timing->pesNb = es->prop.bitrate / (
      es->curPesPacket.data.dataUsedSize * 8
    );
  }

  if (0 == timing->pesNb)
    LIBBLU_ERROR_RETURN("Unable to get ES pesNb, broken script.\n");

#if USE_AVERAGE_PES_SIZE
  if (0 == (avgPesPacketSize = averageSizePesPacketLibbluES(es, 10)))
    LIBBLU_ERROR_RETURN("Unable to get a average PES packet size.\n");
#else
  avgPesPacketSize = remainingPesDataLibbluES(*es);
#endif

  avgPesPacketSize = MAX(es->prop.bitrate / timing->pesNb / 8, avgPesPacketSize);

  /** Compute timing values:
   * NOTE: Performed at each PES frame, preventing introduction
   * of variable parameters issues.
   */
  timing->pesDuration = floor(MAIN_CLOCK_27MHZ / timing->pesNb);
  timing->pesTsNb = DIV_ROUND_UP(avgPesPacketSize, TP_PAYLOAD_SIZE);
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
  unsigned esStrmId,
  bool forceRebuildScript
)
{
  LibbluESSettings * esSettings;
  LibbluStreamPtr stream;
  LibbluESFormatUtilities utilities;
  uint16_t pid;

  LIBBLU_DEBUG_COM(" Find/check script filepath.\n");

  esSettings = &ctx->settings.inputStreams[esStrmId];
  if (_findValidESScript(esSettings, ctx->settings.inputStreams, esStrmId) < 0)
    return -1;

  LIBBLU_DEBUG_COM(" Creation of the Elementary Stream handle.\n");

  /* Use a fake PID value (0x0000), real PID value requested later. */
  if (NULL == (stream = createLibbluStream(TYPE_ES, 0x0000)))
    goto free_return;
  initLibbluES(&stream->es, esSettings);

  LIBBLU_DEBUG_COM(" Preparation of the ES handle.\n");

  /* Get the forced script rebuilding option */
  forceRebuildScript |= esSettings->options.forcedScriptBuilding;

  LIBBLU_DEBUG_COM(
    " Force rebuilding of script: %s.\n",
    BOOL_STR(forceRebuildScript)
  );

  if (prepareLibbluES(&stream->es, &utilities, forceRebuildScript) < 0)
   goto free_return;;

  LIBBLU_DEBUG_COM(" Request and set PID value.\n");

  if (requestESPIDLibbluStream(&ctx->pidValues, &pid, stream) < 0)
    goto free_return;
  stream->pid = pid;

  ctx->elementaryStreams[esStrmId] = stream;
  ctx->elementaryStreamsUtilities[esStrmId] = utilities;
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
  setDefaultPatParameters(&ctx->patParam, 0x0000, 0x0);
  setDefaultPmtParameters(&ctx->pmtParam, 0x0001, PMT_PID);
  setDefaultSitParameters(&ctx->sitParam);

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
    LIBBLU_DEBUG_COM("Feeding the T-STD/BDAV-STD buffering model.\n");
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
      ctx->referentialStc,
      utilities.preparePesHeader
    );
    switch (ret) {
      case 0: /* No more PES packet */
        LIBBLU_ERROR_FRETURN(
          "Empty script, unable to build a single PES packet, "
          "'%" PRI_LBCS "'.\n",
          stream->es.settings->scriptFilepath
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

  cleanPatParameters(ctx->patParam);
  cleanPmtParameters(ctx->pmtParam);
  cleanSitParameters(ctx->sitParam);

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
 * \return true The specified amount of data will produce overflow if
 * transmitted.
 * \return false The specified amount of data can be delivered without
 * overflow.
 *
 * the given 'stream' object is used by the modelizer to determine the flow
 * of the given bytes.
 */
static bool _checkBufferingModelAvailability(
  LibbluMuxingContextPtr ctx,
  LibbluStreamPtr stream,
  size_t size
)
{
  return checkBufModel(
    ctx->settings.options.bufModelOptions,
    ctx->tStdModel,
    ctx->currentStcTs,
    size * 8,
    ctx->settings.targetMuxingRate,
    stream
  );
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
  int ret;

  StreamHeapTimingInfos tpTimeData;
  LibbluStreamPtr tpStream;
  bool injectedPacket;

  /* Check if smallest timestamp is less than or equal to currentStcTs */
  if (!streamIsReadyStreamHeap(ctx->systemStreamsHeap, ctx->currentStcTs))
    return 0; /* Timestamp not reached, no tp to mux. */
  extractStreamHeap(ctx->systemStreamsHeap, &tpTimeData, &tpStream);

  if (tpStream->type == TYPE_PCR && _checkPcrInjection(ctx)) {
    /* PCR system stream timestamp reached, but PCR fields are carried by
    ES transport packet and so no specific packet shall be emited. Skip
    transport packet generation. */
    injectedPacket = false;
    incrementTPTimestampStreamHeapTimingInfos(&tpTimeData);
  }
  else {
    /* Normal transport packet injection. */
    bool isTStdManagedStream = false;

    injectedPacket = true; /* By default, inject the packet */
    if (_isEnabledTStdModelLibbluMuxingContext(ctx)) {
      /* Check if the stream buffering is monitored */
      isTStdManagedStream = isManagedSystemBdavStd(
        tpStream->pid,
        ctx->patParam
      );

      if (isTStdManagedStream) {
        /* Inject the packet only if it does not overflow. */
        injectedPacket = _checkBufferingModelAvailability(
          ctx, tpStream, TP_SIZE
        );
      }
    }

    if (injectedPacket) {
      /* Only inject the packet if it does not cause overflow, or if its
      associated stream is not managed. */
      bool pcrPresence;
      uint64_t pcrValue;

      TPHeaderParameters header;
      size_t headerSize, payloadSize;

      pcrPresence = (
        (tpStream->type == TYPE_PCR)
        || _pcrInjectionRequired(ctx, tpStream->pid)
      );
      pcrValue = computePcrFieldValue(ctx->currentStc, ctx->byteStcDuration);

      /* Prepare the tansport packet header */
      prepareTPHeader(&header, tpStream, pcrPresence, pcrValue);

      /* Write if required by muxing options the tp_extra_header() */
      if (_writeTpExtraHeaderIfRequired(ctx, output) < 0)
        return -1;

      /* Write the current transport packet */
      ret = writeTransportPacket(
        output, tpStream, header,
        &headerSize,
        &payloadSize
      );
      if (ret < 0)
        return -1;

      LIBBLU_DEBUG(
        LIBBLU_DEBUG_MUXER_DECISION, "Muxer decisions",
        "0x%" PRIX64 " - %" PRIu64 ", Sys packet muxed (0x%04" PRIX16 ").\n",
        ctx->currentStcTs,
        ctx->currentStcTs,
        tpStream->pid
      );

      ctx->nbTsPacketsMuxed++;
      ctx->nbBytesWritten += TP_SIZE;

      if (isTStdManagedStream) {
        /* If the stream is buffer managed, inject the written bytes
        in the model. */
        size_t packetSize = headerSize + payloadSize;

        /* Register the packet */
        ret = addSystemFramesToBdavStd(
          ctx->tStdSystemBuffersList,
          headerSize,
          payloadSize
        );
        if (ret < 0)
          return -1;

        /* Put the packet data */
        if (_putDataToBufferingModel(ctx, tpStream, packetSize) < 0)
          return -1;
      }

      if (tpStream->sys.firstFullTableSupplied) {
        /* Increment the timestamp only after the table has been fully
        emitted once. */
        incrementTPTimestampStreamHeapTimingInfos(&tpTimeData);
      }
    }
  }

  if (addStreamHeap(ctx->systemStreamsHeap, tpTimeData, tpStream) < 0)
    return -1;

  return injectedPacket; /* 0: No packet written, 1: One packet written */
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
  int ret;

  StreamHeapTimingInfos tpTimeData;
  LibbluStreamPtr stream;
  TPHeaderParameters tpHeader;
  bool isTStdManagedStream;

  stream = NULL;
  while (
    NULL == stream
    && streamIsReadyStreamHeap(ctx->elementaryStreamsHeap, ctx->currentStcTs)
  ) {
    extractStreamHeap(ctx->elementaryStreamsHeap, &tpTimeData, &stream);

    isTStdManagedStream = (NULL != stream->es.lnkdBufList);

    if (isTStdManagedStream) {
      bool pcrInjection = _pcrInjectionRequired(ctx, stream->pid);
      uint64_t pcrValue = computePcrFieldValue(ctx->currentStc, ctx->byteStcDuration);

      prepareTPHeader(&tpHeader, stream, pcrInjection, pcrValue);

      if (!_checkBufferingModelAvailability(ctx, stream, TP_SIZE)) {
        /* ES tp insertion leads to overflow, increase its timestamp and try
        with another ES. */

        LIBBLU_T_STD_VERIF_TEST_DEBUG(
          "Skipping injection PID 0x%04" PRIX16 " at %" PRIi64 ".\n",
          stream->pid,
          ctx->currentStcTs
        );

#if 1
        incrementTPTimestampStreamHeapTimingInfos(&tpTimeData);
#else
        tpTimeData.tsPt += ctx->tpStcDuration_floor;
        /* tpTimeData.tsPt = ctx->currentStcTs + ctx->tpStcDuration_floor; */
#endif

        if (addStreamHeap(ctx->elementaryStreamsHeap, tpTimeData, stream) < 0)
          return -1;
        stream = NULL; /* Reset to keep in loop */
      }
    }
  }

  if (NULL == stream)
    return 0; /* Nothing currently to mux */

  if (!isTStdManagedStream) {
    /* Prepare tp header (it has already been prepared if T-STD buf model
    is used. */
    bool pcrInjection = _pcrInjectionRequired(ctx, stream->pid);
    uint64_t pcrValue = computePcrFieldValue(ctx->currentStc, ctx->byteStcDuration);

    prepareTPHeader(&tpHeader, stream, pcrInjection, pcrValue);
  }

  /* Write if required by muxing options the tp_extra_header() */
  if (_writeTpExtraHeaderIfRequired(ctx, output) < 0)
    return -1;

  /* Write the current transport packet */
  size_t headerSize, payloadSize;

  if (writeTransportPacket(output, stream, tpHeader, &headerSize, &payloadSize) < 0)
    return -1;

  LIBBLU_DEBUG(
    LIBBLU_DEBUG_MUXER_DECISION, "Muxer decisions",
    "0x%" PRIX64 " - %" PRIu64 ", ES packet muxed "
    "(0x%04" PRIX16 ", tsPt: %" PRIu64 ").\n",
    ctx->currentStcTs,
    ctx->currentStcTs,
    stream->pid,
    tpTimeData.tsPt
  );

  ctx->nbTsPacketsMuxed++;
  ctx->nbBytesWritten += TP_SIZE;

  if (isTStdManagedStream) {
    /* If T-STD buffer model is used, inject the written bytes in the model. */
    size_t packetSize = headerSize + payloadSize;

    LIBBLU_T_STD_VERIF_DECL_DEBUG(
      "Registering %zu+%zu=%zu bytes of TP for PID 0x%04" PRIX16 ".\n",
      headerSize, payloadSize, packetSize,
      stream->pid
    );

    /* Register the packet */
    ret = addESTsFrameToBdavStd(
      stream->es.lnkdBufList,
      headerSize,
      payloadSize
    );
    if (ret < 0)
      return -1;

    /* Put the packet data */
    if (_putDataToBufferingModel(ctx, stream, packetSize) < 0)
      return -1;
  }

  /* Check remaining data in processed PES packet : */
  if (0 == remainingPesDataLibbluES(stream->es)) {
    /* If no more data, build new PES packet */
    LibbluESFormatUtilities utilities;

    if (_retriveAssociatedESUtilities(ctx, stream, &utilities) < 0)
      LIBBLU_ERROR_RETURN("Unable to retrive ES associated utilities.\n");

    ret = buildNextPesPacketLibbluES(
      &stream->es,
      stream->pid,
      ctx->referentialStc,
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
      "(end of script ? %s, Queued frames nb: %u).\n",
      stream->pid,
      stream->es.curPesPacket.data.dataUsedSize,
      stream->es.curPesPacket.prop.dts,
      stream->es.curPesPacket.prop.pts,
      ctx->currentStcTs,
      BOOL_INFO(stream->es.endOfScriptReached),
      stream->es.pesPacketsScriptsQueueSize
    );

#if 1
    if (_computePesTiming(&tpTimeData, &stream->es, ctx) < 0)
      return -1;
#endif

    LIBBLU_DEBUG(
      LIBBLU_DEBUG_PES_BUILDING, "PES building",
      " => pesTsNb: %" PRIu64 ", tsPt: %" PRIu64 ".\n",
      tpTimeData.pesTsNb,
      tpTimeData.tsPt
    );
  }
  else {
#if 1
      incrementTPTimestampStreamHeapTimingInfos(&tpTimeData);
#else
      tpTimeData.tsPt += ctx->tpStcDuration_floor;
#endif
  }

  if (stream->pid == ctx->elementaryStreams[0]->pid) {
    /* Update progression bar : */
    ctx->progress =
      (double) (
        stream->es.curPesPacket.prop.pts
        - ctx->referentialStc
        + stream->es.refPts
      ) / stream->es.endPts
    ;
  }

  if (addStreamHeap(ctx->elementaryStreamsHeap, tpTimeData, stream) < 0)
    return -1;
  return 1;
}

static int _muxNullPacket(
  LibbluMuxingContextPtr ctx,
  BitstreamWriterPtr output
)
{
  TPHeaderParameters header;

  /* Prepare the tansport packet header */
  /* NOTE: Null packets cannot carry PCR */
  prepareTPHeader(&header, ctx->null, false, 0);

  /* Write if required by muxing options the tp_extra_header() */
  if (_writeTpExtraHeaderIfRequired(ctx, output) < 0)
    return -1;

  if (writeTransportPacket(output, ctx->null, header, NULL, NULL) < 0)
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