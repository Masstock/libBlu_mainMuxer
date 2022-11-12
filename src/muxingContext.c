#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "muxingContext.h"

static uint64_t getMinInitialDelay(
  const LibbluStreamPtr * esStreams,
  unsigned nbEsStreams
)
{
  uint64_t initialDelay;
  unsigned i;

  initialDelay = 0;
  for (i = 0; i < nbEsStreams; i++)
    initialDelay = MAX(initialDelay, esStreams[i]->es.refPts);

  return initialDelay;
}

static int addSystemStreamToContext(
  LibbluMuxingContextPtr ctx,
  LibbluStreamPtr sysStream
)
{
  StreamHeapTimingInfos timing;

  int priority;
  uint64_t pesDuration;
  uint64_t pesTsNb;

  assert(!isESLibbluStream(sysStream));

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

  pesTsNb = DIV_ROUND_UP(
    sysStream->sys.dataLength,
    TP_SIZE - TP_HEADER_SIZE
  );
  pesTsNb = MAX(pesTsNb, 1);

  timing = (StreamHeapTimingInfos) {
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
  /* LIBBLU_DEBUG_COM(
    "Adding to context System stream PID 0x%04x: "
    "pesTsNb: %" PRIu64 ", tsPt: %" PRIu64 ".\n",
    sysStream->pid, timing.pesTsNb, timing.tsPt
  ); */

  return addStreamHeap(
    ctx->systemStreamsHeap,
    timing,
    sysStream
  );
}

static int initPatSystemStream(
  LibbluMuxingContextPtr ctx
)
{
  PatParameters param;

  /* TODO: Move to MuxingSettings. */
  static const uint16_t tsId = 0x0000;
  static const unsigned nbPrograms = 2;
  static const uint16_t pmtPids[2] = {SIT_PID, PMT_PID};

  if (preparePATParam(&param, tsId, nbPrograms, NULL, pmtPids) < 0)
    return -1;
  ctx->patParam = param;

  if (NULL == (ctx->pat = createPATSystemLibbluStream(param, PAT_PID)))
    return -1;

  return addSystemStreamToContext(ctx, ctx->pat);
}

static int initPmtSystemStream(
  LibbluMuxingContextPtr ctx
)
{
  int ret;
  unsigned i;

  PmtParameters param;
  uint16_t pcrPID;

  LibbluESProperties esProperties[LIBBLU_MAX_NB_STREAMS];
  LibbluESFmtSpecProp esFmtSpecProperties[LIBBLU_MAX_NB_STREAMS];
  uint16_t esStreamsPids[LIBBLU_MAX_NB_STREAMS];
  unsigned nbEsStreams;

  /* TODO: Move to MuxingSettings. */
  static const uint16_t mainPmtProgramNb = 0x0001;
  static const uint16_t pmtPid = PMT_PID;

  if (ctx->settings.options.pcrOnESPackets)
    pcrPID = ctx->settings.options.pcrPID;
  else
    pcrPID = PCR_PID;

  nbEsStreams = nbESLibbluMuxingContext(ctx);
  assert(nbEsStreams <= LIBBLU_MAX_NB_STREAMS);

  for (i = 0; i < nbEsStreams; i++) {
    esProperties[i] = ctx->elementaryStreams[i]->es.prop;
    esFmtSpecProperties[i] = ctx->elementaryStreams[i]->es.fmtSpecProp;
    esStreamsPids[i] = ctx->elementaryStreams[i]->pid;
  }

  ret = preparePMTParam(
    &param,
    esProperties,
    esFmtSpecProperties,
    esStreamsPids,
    nbEsStreams,
    ctx->settings.dtcpParameters,
    mainPmtProgramNb,
    pcrPID
  );
  if (ret < 0)
    return -1;
  ctx->pmtParam = param;

  if (NULL == (ctx->pmt = createPMTSystemLibbluStream(param, pmtPid)))
    return -1;

  return addSystemStreamToContext(ctx, ctx->pmt);
}

static int initPcrSystemStream(
  LibbluMuxingContextPtr ctx
)
{
  /* NOTE: Prepared regardless pcrOnESPackets to follow timings. */
  if (NULL == (ctx->pcr = createPCRSystemLibbluStream(PCR_PID)))
    return -1;

  return addSystemStreamToContext(ctx, ctx->pcr);
}

static int initSitSystemStream(
  LibbluMuxingContextPtr ctx
)
{
  SitParameters param;

  /* TODO: Move to MuxingSettings. */
  static const uint16_t mainPmtProgramNb = 0x0001;

  prepareSITParam(
    &param,
    ctx->settings.targetMuxingRate,
    mainPmtProgramNb
  );
  ctx->sitParam = param;

  if (NULL == (ctx->sit = createSITSystemLibbluStream(param, SIT_PID)))
    return -1;

  return addSystemStreamToContext(ctx, ctx->sit);
}

static int initNullSystemStream(
  LibbluMuxingContextPtr ctx
)
{
  if (NULL == (ctx->null = createNULLSystemLibbluStream(NULL_PID)))
    return -1;
  return 0;
}

static int initSystemStreams(
  LibbluMuxingContextPtr ctx
)
{
  if (initPatSystemStream(ctx) < 0)
    return -1;

  if (initPmtSystemStream(ctx) < 0)
    return -1;

  if (initPcrSystemStream(ctx) < 0)
    return -1;

  if (initSitSystemStream(ctx) < 0)
    return -1;

  if (initNullSystemStream(ctx) < 0)
    return -1;

  if (isEnabledTStdModelLibbluMuxingContext(ctx))
    return addSystemToBdavStd(
      ctx->tStdSystemBuffersList,
      ctx->tStdModel,
      ctx->currentStcTs,
      ctx->settings.targetMuxingRate
    );

  return 0;
}

static void computeInitialTimings(
  LibbluMuxingContextPtr ctx
)
{
  uint64_t startPcr, initialDecodingDelay, stdBufferDelay;

  startPcr = ctx->settings.initialPresentationTime;

  LIBBLU_DEBUG_COM(
    "User requested startPcr: %" PRIu64 " (27MHz clock).\n",
    startPcr
  );

  /* Define transport stream timestamps. */
  ctx->byteStcDuration =
    (double) MAIN_CLOCK_27MHZ * 8
    / ctx->settings.targetMuxingRate
  ;
  ctx->tpStcDuration = TP_SIZE * ctx->byteStcDuration;
  ctx->tpStcIncrementation = (uint64_t) floor(ctx->tpStcDuration);

  /* Compute minimal decoding delay due to ESs first DTS/PTS difference */
  initialDecodingDelay = getMinInitialDelay(
    ctx->elementaryStreams,
    nbESLibbluMuxingContext(ctx)
  );

  /* Initial timing model delay between muxer and demuxer STCs */
  stdBufferDelay = (uint64_t) ceil(
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
  updateCurrentStcLibbluMuxingContext(
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

static int initiateElementaryStreamsTStdModel(
  LibbluMuxingContextPtr ctx
)
{
  unsigned i;
  LibbluStreamPtr stream;

  for (i = 0; i < nbESLibbluMuxingContext(ctx); i++) {
    stream = ctx->elementaryStreams[i];

    /* Create the buffering chain associated to the ES' PID */
    if (addESToBdavStd(ctx->tStdModel, &stream->es, stream->pid, ctx->currentStcTs) < 0)
      return -1;
  }

  return 0;
}

static int calcPesTiming(
  StreamHeapTimingInfos * timing,
  const LibbluESPtr es,
  uint64_t stdBufDelay
)
{
  uint64_t dts;
  size_t avgPesPacketSize;

  assert(stdBufDelay <= es->curPesPacket.prop.dts);
  dts = es->curPesPacket.prop.dts - stdBufDelay;

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
  timing->pesTsNb = DIV_ROUND_UP(avgPesPacketSize, TP_SIZE - TP_HEADER_SIZE);
  timing->tsDuration = timing->pesDuration / MAX(1, timing->pesTsNb);

#if SHIFT_PACKETS_BEFORE_DTS
  if (timing->pesTsNb * 846 < dts)
    timing->tsPt = dts - timing->pesTsNb * 846;
  else
    timing->tsPt = dts;
#else
  timing->tsPt = dts;
#endif
  timing->tsPriority = 0; /* Normal priority. */

  return 0;
}

static int genEsmsFilename(
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

static bool isSharedUsedScript(
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

static int findValidESScript(
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
    fpSize = genEsmsFilename(
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
    && isSharedUsedScript(scriptFilepath, ardyRegES, ardyRegESNb)
    && increment < 100
  ) {
    fpSize = genEsmsFilename(
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

LibbluMuxingContextPtr createLibbluMuxingContext(
  LibbluMuxingSettings settings
)
{
  LibbluMuxingContextPtr ctx;

  int ret;
  unsigned i;

  bool tStdBufModelEnabled;

  LibbluStreamPtr stream;

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

  tStdBufModelEnabled = !LIBBLU_MUX_SETTINGS_OPTION(&settings, disableTStdBufVerifier);

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
  for (i = 0; i < ctx->settings.nbInputStreams; i++) {
    LibbluESSettings * esSettings;
    bool forceRebuildScript;
    uint16_t pid;

    LibbluESFormatUtilities utilities;

    esSettings = ctx->settings.inputStreams + i;

    /* Find/check script filename */
    LIBBLU_DEBUG_COM(" Check script filepath.\n");
    if (findValidESScript(esSettings, ctx->settings.inputStreams, i) < 0)
      goto free_return;

    LIBBLU_DEBUG_COM(" Creation of the Elementary Stream handle.\n");
    stream = createElementaryLibbluStream(esSettings);
    if (NULL == stream)
      goto free_return;
    ctx->elementaryStreams[i] = stream;

    /* Get the forced script rebuilding option */
    forceRebuildScript = esSettings->options.forcedScriptBuilding;

    /* Prepare the ES */
    LIBBLU_DEBUG_COM(" Preparation of the Elementary Stream handle.\n");
    if (prepareLibbluES(&stream->es, &utilities, forceRebuildScript) < 0)
      goto free_return;

    /* Choose and set stream PID value */
    LIBBLU_DEBUG_COM(" Request a PID value.\n");
    if (requestESPIDLibbluStream(&ctx->pidValues, &pid, stream) < 0)
      goto free_return;
    setPIDLibbluStream(stream, pid);

    ctx->elementaryStreamsUtilities[i] = utilities;
  }

  /* Compute initial timing values in accordance with each ES timings */
  LIBBLU_DEBUG_COM("Computing the initial timing values.\n");
  computeInitialTimings(ctx);

  /* Init System streams */
  LIBBLU_DEBUG_COM("Initialization of Systeam Streams.\n");
  if (initSystemStreams(ctx) < 0)
    goto free_return;

  /* Initiate the T-STD buffering chain with computed timings */
  if (isEnabledTStdModelLibbluMuxingContext(ctx)) {
    LIBBLU_DEBUG_COM("Feeding the T-STD/BDAV-STD buffering model.\n");
    if (initiateElementaryStreamsTStdModel(ctx) < 0)
      goto free_return;
  }

  /* Build the first PES packets to set mux priorities */
  LIBBLU_DEBUG_COM("Initial PES packets building.\n");
  for (i = 0; i < ctx->settings.nbInputStreams; i++) {
    StreamHeapTimingInfos timing;
    LibbluESFormatUtilities utilities;

    stream = ctx->elementaryStreams[i];
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
          "Empty script, unable to build a single PES packet.\n"
        );
      case 1: /* Success */
        break;
      default: /* Error */
        goto free_return;
    }

    LIBBLU_DEBUG_COM(" Use properties to set timestamps.\n");
    if (calcPesTiming(&timing, &stream->es, ctx->stdBufDelay) < 0)
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
  unsigned i;

  if (NULL == ctx)
    return;

  cleanLibbluMuxingSettings(ctx->settings);
  cleanLibbluPIDValues(ctx->pidValues);
  for (i = 0; i < LIBBLU_MAX_NB_STREAMS; i++)
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
static int writeTpExtraHeaderIfRequired(
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
static bool checkPcrInjection(
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
static bool checkBufferingModelAvailability(
  LibbluMuxingContextPtr ctx,
  LibbluStreamPtr stream,
  size_t size
)
{
  return checkBufModel(
    ctx->tStdModel,
    ctx->currentStcTs,
    size * 8,
    ctx->settings.targetMuxingRate,
    stream
  );
}

int putDataToBufferingModel(
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
 * #increaseCurrentStcLibbluMuxingContext().
 */
static int muxNextSystemStreamPacket(
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

  if (tpStream->type == TYPE_PCR && checkPcrInjection(ctx)) {
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
    if (isEnabledTStdModelLibbluMuxingContext(ctx)) {
      /* Check if the stream buffering is monitored */
      isTStdManagedStream = isManagedSystemBdavStd(
        tpStream->pid,
        ctx->patParam
      );

      if (isTStdManagedStream) {
        /* Inject the packet only if it does not overflow. */
        injectedPacket = checkBufferingModelAvailability(
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
        || pcrInjectionRequired(ctx, tpStream->pid)
      );
      pcrValue = computePcrFieldValue(ctx->currentStc, ctx->byteStcDuration);

      /* Prepare the tansport packet header */
      prepareTPHeader(&header, tpStream, pcrPresence, pcrValue);

      /* Write if required by muxing options the tp_extra_header() */
      if (writeTpExtraHeaderIfRequired(ctx, output) < 0)
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
        if (putDataToBufferingModel(ctx, tpStream, packetSize) < 0)
          return -1;
      }

      if (tpStream->sys.firstFullTableSupplied) {
        /* Increment the timestamp only after the table has been fully
        emitted once. */
        incrementTPTimestampStreamHeapTimingInfos(&tpTimeData);
      }
    }
  }

  /* Re-insert the system stream to the heap */
  if (addStreamHeap(ctx->systemStreamsHeap, tpTimeData, tpStream) < 0)
    return -1;
  return injectedPacket; /* 0: No packet written, 1: One packet written */
}

static int muxNextESPacket(
  LibbluMuxingContextPtr ctx,
  BitstreamWriterPtr output
)
{
  int ret;

  StreamHeapTimingInfos tpTimeData;
  LibbluStreamPtr tpStream;
  TPHeaderParameters tpHeader;
  bool isTStdManagedStream;

  bool pcrInjection;
  uint64_t pcrValue;

  size_t headerSize, payloadSize;

  tpStream = NULL;
  while (
    NULL == tpStream
    && streamIsReadyStreamHeap(ctx->elementaryStreamsHeap, ctx->currentStcTs)
  ) {
    extractStreamHeap(ctx->elementaryStreamsHeap, &tpTimeData, &tpStream);

    isTStdManagedStream = (NULL != tpStream->es.lnkdBufList);

    if (isTStdManagedStream) {
      pcrInjection = pcrInjectionRequired(ctx, tpStream->pid);
      pcrValue = computePcrFieldValue(ctx->currentStc, ctx->byteStcDuration);

      prepareTPHeader(&tpHeader, tpStream, pcrInjection, pcrValue);

      if (!checkBufferingModelAvailability(ctx, tpStream, TP_SIZE)) {
        /* ES tp insertion leads to overflow, increase its timestamp and try
        with another ES. */

        LIBBLU_T_STD_VERIF_TEST_DEBUG(
          "Skipping injection PID 0x%04" PRIX16 " at %" PRIi64 ".\n",
          tpStream->pid,
          ctx->currentStcTs
        );
        /* printf("%" PRIu64 "\n", tpTimeData.tsPt); */

        /* printBufModelBufferingChain(ctx->tStdModel); */

#if 1
        incrementTPTimestampStreamHeapTimingInfos(&tpTimeData);
#else
        tpTimeData.tsPt += ctx->tpStcIncrementation;
        /* tpTimeData.tsPt = ctx->currentStcTs + ctx->tpStcIncrementation; */
#endif

        if (addStreamHeap(ctx->elementaryStreamsHeap, tpTimeData, tpStream) < 0)
          return -1;
        tpStream = NULL; /* Reset to keep in loop */
      }
    }
  }

  if (NULL == tpStream)
    return 0; /* Nothing currently to mux */

  if (!isTStdManagedStream) {
    /* Prepare tp header (it has already been prepared if T-STD buf model
    is used. */
    pcrInjection = pcrInjectionRequired(ctx, tpStream->pid);
    pcrValue = computePcrFieldValue(ctx->currentStc, ctx->byteStcDuration);

    prepareTPHeader(&tpHeader, tpStream, pcrInjection, pcrValue);
  }

  /* Write if required by muxing options the tp_extra_header() */
  if (writeTpExtraHeaderIfRequired(ctx, output) < 0)
    return -1;

  /* Write the current transport packet */
  ret = writeTransportPacket(
    output, tpStream, tpHeader,
    &headerSize,
    &payloadSize
  );
  if (ret < 0)
    return -1;

  LIBBLU_DEBUG(
    LIBBLU_DEBUG_MUXER_DECISION, "Muxer decisions",
    "0x%" PRIX64 " - %" PRIu64 ", ES packet muxed "
    "(0x%04" PRIX16 ", tsPt: %" PRIu64 ").\n",
    ctx->currentStcTs,
    ctx->currentStcTs,
    tpStream->pid,
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
      tpStream->pid
    );

    /* Register the packet */
    ret = addESTsFrameToBdavStd(
      tpStream->es.lnkdBufList,
      headerSize,
      payloadSize
    );
    if (ret < 0)
      return -1;

    /* Put the packet data */
    if (putDataToBufferingModel(ctx, tpStream, packetSize) < 0)
      return -1;
  }

  /* Check remaining data in processed PES packet : */
  if (0 == remainingPesDataLibbluES(tpStream->es)) {
    /* If no more data, build new PES packet */
    LibbluESFormatUtilities utilities;

    if (retriveAssociatedESUtilities(ctx, tpStream, &utilities) < 0)
      LIBBLU_ERROR_RETURN("Unable to retrive ES associated utilities.\n");

    ret = buildNextPesPacketLibbluES(
      &tpStream->es,
      tpStream->pid,
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
      tpStream->pid,
      tpStream->es.curPesPacket.data.dataUsedSize,
      tpStream->es.curPesPacket.prop.dts,
      tpStream->es.curPesPacket.prop.pts,
      ctx->currentStcTs,
      BOOL_INFO(tpStream->es.endOfScriptReached),
      tpStream->es.pesPacketsScriptsQueueSize
    );

#if 1
    if (calcPesTiming(&tpTimeData, &tpStream->es, ctx->stdBufDelay) < 0)
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
      tpTimeData.tsPt += ctx->tpStcIncrementation;
#endif
  }

  /* Update progression bar : */
  if (tpStream->pid == ctx->elementaryStreams[0]->pid)
    ctx->progress =
      (double) (
        tpStream->es.curPesPacket.prop.pts
        - ctx->referentialStc
        + tpStream->es.refPts
      ) / tpStream->es.endPts
    ;

  if (addStreamHeap(ctx->elementaryStreamsHeap, tpTimeData, tpStream) < 0)
    return -1;
  return 1;
}

static int muxNullPacket(
  LibbluMuxingContextPtr ctx,
  BitstreamWriterPtr output
)
{
  TPHeaderParameters header;

  /* Prepare the tansport packet header */
  /* NOTE: Null packets cannot carry PCR */
  prepareTPHeader(&header, ctx->null, false, 0);

  /* Write if required by muxing options the tp_extra_header() */
  if (writeTpExtraHeaderIfRequired(ctx, output) < 0)
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
  if ((nbMuxedPackets = muxNextSystemStreamPacket(ctx, output)) < 0)
    return -1; /* Error */
  if (0 < nbMuxedPackets)
    goto success; /* System tp muxed with success */

  /* Otherwise, try to mux a Elementary Stream tp */
  if ((nbMuxedPackets = muxNextESPacket(ctx, output)) < 0)
    return -1; /* Error */
  if (0 < nbMuxedPackets)
    goto success; /* ES tp muxed with success */

  /* Otherwise, put a NULL packet to ensure CBR mux (or do nothing if VBR) */
  if (LIBBLU_MUX_SETTINGS_OPTION(&ctx->settings, cbrMuxing)) {
    if (muxNullPacket(ctx, output) < 0)
      return -1; /* Error */
  }

success:
  /* Increase System Time Clock (and 'currentStcTs') */
  increaseCurrentStcLibbluMuxingContext(ctx);

  return 0;
}

int padAlignedUnitLibbluMuxingContext(
  LibbluMuxingContextPtr ctx,
  BitstreamWriterPtr output
)
{
  assert(NULL != ctx);
  assert(NULL != output);

  while (ctx->nbTsPacketsMuxed % 32) {
    if (muxNullPacket(ctx, output) < 0)
      return -1; /* Error */
    increaseCurrentStcLibbluMuxingContext(ctx);
  }

  return 0;
}