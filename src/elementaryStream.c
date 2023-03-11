#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "elementaryStream.h"

/* ### ES settings definition : ############################################ */

int setFpsChangeLibbluESSettings(
  LibbluESSettings * dst,
  const lbc * expr
)
{
  HdmvFrameRateCode idc;

  if (parseFpsChange(expr, &idc) < 0)
    return -1;

  LIBBLU_ES_SETTINGS_SET_OPTION(dst, fpsChange, idc);
  return 0;
}

int setArChangeLibbluESSettings(
  LibbluESSettings * dst,
  const lbc * expr
)
{
  LibbluAspectRatioMod values;

  if (parseArChange(expr, &values) < 0)
    return -1;

  LIBBLU_ES_SETTINGS_SET_OPTION(dst, arChange, values);
  return 0;
}

int setLevelChangeLibbluESSettings(
  LibbluESSettings * dst,
  const lbc * expr
)
{
  uint8_t idc;

  if (parseLevelChange(expr, &idc) < 0)
    return -1;

  LIBBLU_ES_SETTINGS_SET_OPTION(dst, levelChange, idc);
  return 0;
}

int setPbrFilepathLibbluESSettings(
  LibbluESSettings * dst,
  const lbc * expr,
  const lbc * metaFilepath
)
{
  int ret;
  lbc path[PATH_BUFSIZE];
  lbc * pathDup;

  if (!lbc_cwk_path_is_absolute(expr))
    ret = lb_get_relative_fp_from_anchor(path, PATH_BUFSIZE, expr, metaFilepath);
  else
    ret = lbc_snprintf(path, PATH_BUFSIZE * sizeof(lbc), "%s", expr);
  if (ret < 0)
    return -1;

  if (lbc_access_fp(path, "r") < 0)
    return -1;

  if (NULL == (pathDup = lbc_strdup(path)))
    return -1;

  LIBBLU_ES_SETTINGS_SET_OPTION(dst, pbrFilepath, pathDup);
  return 0;
}

int setHdmvInitialTimestampLibbluESSettings(
  LibbluESSettings * dst,
  uint64_t value
)
{
#if 0 < LIBBLU_MIN_HDMV_INIT_TIMESTAMP
  if (value < LIBBLU_MIN_HDMV_INIT_TIMESTAMP)
    return -1;
#endif
  if (LIBBLU_MAX_HDMV_INIT_TIMESTAMP < value)
    return -1;

  LIBBLU_ES_SETTINGS_SET_OPTION(dst, hdmv.initialTimestamp, value);
  return 0;
}

static int _generateFilepath(
  lbc * path,
  size_t size,
  const lbc * filepath,
  const lbc * anchorFilepath
)
{
  if (lbc_cwk_path_is_absolute(filepath)) {
    if (lbc_snprintf(path, size, "%s", filepath) < 0)
      LIBBLU_ERROR(
        "Source filepath is invalid, %s (errno: %d).\n",
        strerror(errno), errno
      );
  }
  else {
    if (lb_get_relative_fp_from_anchor(path, size, filepath, anchorFilepath) < 0)
      return -1;
  }

  return 0;
}

int setMainESFilepathLibbluESSettings(
  LibbluESSettings * dst,
  const lbc * filepath,
  const lbc * anchorFilepath
)
{
  lbc path[PATH_BUFSIZE];

  assert(NULL != dst);
  assert(NULL != filepath);

  if (_generateFilepath(path, PATH_BUFSIZE, filepath, anchorFilepath) < 0)
    goto free_return;

  if (lb_gen_absolute_fp(&dst->filepath, path) < 0)
    goto free_return;

  return 0;

free_return:
  LIBBLU_ERROR_RETURN(
    "Unable to set main ES filepath '%" PRI_LBCS "'.\n",
    filepath
  );
}

int setScriptFilepathLibbluESSettings(
  LibbluESSettings * dst,
  const lbc * filepath,
  const lbc * anchorFilepath
)
{
  lbc path[PATH_BUFSIZE];

  assert(NULL != dst);
  assert(NULL != filepath);

  if (_generateFilepath(path, PATH_BUFSIZE, filepath, anchorFilepath) < 0)
    goto free_return;

  if (lb_gen_absolute_fp(&dst->scriptFilepath, path) < 0)
    goto free_return;

  return 0;

free_return:
  LIBBLU_ERROR_RETURN(
    "Unable to set script filepath '%" PRI_LBCS "'.\n",
    filepath
  );
}

static int checkScriptFileLibbluES(
  LibbluESPtr es,
  LibbluESFormatUtilities * esAssociatedUtilities,
  bool forceRebuild
)
{
  int ret;

  LibbluESFormatUtilities utilities;
  uint64_t scriptFlags;

  LibbluESSettings * settings = es->settings;
  cleanLibbluESFormatUtilities(&utilities);

  /* Checking ESMS script file : */
  scriptFlags = computeFlagsLibbluESSettingsOptions(settings->options);
  LIBBLU_SCRIPT_DEBUG("Check predefined script filepath.\n");
  if (forceRebuild || isAValidESMSFile(settings->scriptFilepath, scriptFlags, NULL) < 0) {
    /* Not valid/missing/forced rebuilding */
    LibbluStreamCodingType expectedCodingType;

    if (forceRebuild)
      LIBBLU_SCRIPT_DEBUG("Forced rebuilding enabled, building script.\n");
    else
      LIBBLU_SCRIPT_DEBUG("Invalid script, building a new one.\n");

    if (NULL == settings->filepath)
      LIBBLU_ERROR_RETURN(
        "Unable to generate script '%" PRI_LBCS "', "
        "no source ES defined.\n",
        settings->scriptFilepath
      );

    if (0 <= settings->codingType)
      expectedCodingType = settings->codingType;
    else {
      LIBBLU_SCRIPT_DEBUG("Undefined stream coding type, use guessing.\n");
      expectedCodingType = guessESStreamCodingType(settings->filepath);
      if (expectedCodingType < 0)
        LIBBLU_ERROR_RETURN(
          "Unable to guess stream coding type of '%" PRI_LBCS "', "
          "file type is not recognized.\n",
          settings->filepath
        );
    }

    /* Init format utilities with expected stream coding type. */
    LIBBLU_SCRIPT_DEBUG("Defining ES associated utilities.\n");
    if (initLibbluESFormatUtilities(&utilities, expectedCodingType) < 0)
      return -1;
    *esAssociatedUtilities = utilities;

    LIBBLU_SCRIPT_DEBUG("Generate script.\n");
    ret = generateScriptES(
      utilities,
      settings->filepath,
      settings->scriptFilepath,
      settings->options
    );
    if (ret < 0)
      LIBBLU_ERROR_RETURN(
        "Invalid input file '%" PRI_LBCS "', "
        "unable to generate script.\n",
        settings->filepath
      );
  }

  return 0;
}

static int parseScriptLibbluES(
  LibbluESPtr es
)
{
  BitstreamReaderPtr script;
  int ret;

  LibbluESSettings * settings = es->settings;
  uint64_t refPts, endPts;

  /* Open script file */
  LIBBLU_SCRIPT_DEBUG("Opening script.\n");
  if (NULL == (script = createBitstreamReaderDefBuf(settings->scriptFilepath)))
    return -1;

  LIBBLU_SCRIPT_DEBUG("Parsing script properties.\n");

  /* Reading ES Properties section. */
  if (seekESPropertiesEsms(settings->scriptFilepath, script) < 0)
    goto free_return;
  if (parseESPropertiesHeaderEsms(script, &es->prop, &refPts, &endPts) < 0)
    goto free_return;
  es->refPts = refPts;
  es->endPts = endPts;
  if (parseESPropertiesSourceFilesEsms(script, &es->sourceFiles) < 0)
    goto free_return;

  if (isConcernedESFmtPropertiesEsms(es->prop)) {
    /* Reading ES Format Properties section according to stream type. */
    if (seekESFmtPropertiesEsms(settings->scriptFilepath, script) < 0)
      goto free_return;
    if (parseESFmtPropertiesEsms(script, &es->prop, &es->fmtSpecProp) < 0)
      goto free_return;
  }

  if ((ret = isPresentESDataBlocksDefinitionEsms(settings->scriptFilepath)) < 0)
    goto free_return;
  if (0 < ret) {
     /* Reading ES Data Blocks Definition section if present. */
    if (seekESDataBlocksDefinitionEsms(settings->scriptFilepath, script) < 0)
      goto free_return;
    if (parseESDataBlocksDefinitionEsms(script, &es->scriptDataSections) < 0)
      goto free_return;
  }

  /* Seek to the ES PES Cutting section. */
  if (seekESPesCuttingEsms(settings->scriptFilepath, script) < 0)
    goto free_return;
  if (checkDirectoryMagic(script, PES_CUTTING_HEADER, 4) < 0)
    goto free_return;

  es->scriptFile = script;
  return 0;

free_return:
  LIBBLU_ERROR(
    "Error happen during parsing of script \"%" PRI_LBCS "\".\n",
    settings->scriptFilepath
  );
  closeBitstreamReader(script);
  return -1;
}

static int setPesPacketsDurationLibbluES(
  LibbluESPtr es
)
{
  LibbluESSettings * settings;
  LibbluESProperties * prop;

  double frameRate, sampleRate;

  settings = es->settings;
  prop = &es->prop;

  prop->pesNb = prop->pesNbSec = 0;
  prop->bitRateBasedDurationAlternativeMode = false;

  frameRate = sampleRate = -1.0;

  if (prop->type == ES_VIDEO) {
    if ((frameRate = frameRateCodeToDouble(prop->frameRate)) <= 0)
      LIBBLU_ERROR_RETURN(
        "Broken script '%" PRI_LBCS "', unknown frame-rate.\n",
        settings->scriptFilepath
      );
  }
  else if (prop->type == ES_AUDIO) {
    if ((sampleRate = sampleRateCodeToDouble(prop->sampleRate)) <= 0)
      LIBBLU_ERROR_RETURN(
        "Broken script '%" PRI_LBCS "', unknown sample-rate.\n",
        settings->scriptFilepath
      );
  }

  switch (prop->codingType) {
    case STREAM_CODING_TYPE_MPEG1: /* H.261 / MPEG-1 */
    case STREAM_CODING_TYPE_H262: /* H.262 / MPEG-2 */
    case STREAM_CODING_TYPE_AVC:   /* H.264 / AVC    */
    case STREAM_CODING_TYPE_VC1:   /* VC-1           */
    case STREAM_CODING_TYPE_HEVC:  /* H.265 / HEVC   */
      assert(0 < frameRate);
      prop->pesNb = frameRate;
      break;

    case STREAM_CODING_TYPE_LPCM:  /* LPCM */
      /* Frame duration: 1/200s */
      prop->pesNb = LPCM_PES_FRAMES_PER_SECOND;
      break;

    case STREAM_CODING_TYPE_AC3:   /* AC-3 */
      /* Frame duration: 1536 samples */
      assert(0 < sampleRate);
      prop->pesNb = sampleRate / 1536;
      break;

    case STREAM_CODING_TYPE_DTS:   /* DTS Coherent Acoustics */
      /* Frame duration: 512 samples */
      assert(0 < sampleRate);
      prop->pesNb = sampleRate / 512;
      break;

    case STREAM_CODING_TYPE_HDHR:  /* DTS-HDHR */
    case STREAM_CODING_TYPE_HDMA:  /* DTS-HDMA */
      /* Frame duration: 512 samples (2 consecutive frames) */
      assert(0 < sampleRate);
      prop->pesNb = sampleRate / 512;
      prop->pesNbSec = sampleRate / 512;
      break;

    case STREAM_CODING_TYPE_TRUEHD: /* Dolby TrueHD (+AC3 Core) */
      /* Frame duration: 1536 samples & 1/200s */
      assert(0 < sampleRate);
      prop->pesNb = sampleRate / 1536;
      prop->pesNbSec = 200;
      break;

    case STREAM_CODING_TYPE_EAC3:  /* EAC-3 (+AC3 Core) */
      /* Frame duration: 1536 samples (2 consecutive frames) */
      assert(0 < sampleRate);
      prop->pesNb = sampleRate / 1536;
      prop->pesNbSec = sampleRate / 1536;
      break;

    case STREAM_CODING_TYPE_DTSE_SEC: /* DTS-Express (Secondary track) */
      /* Frame duration: 4096 samples */
      assert(0 < sampleRate);
      prop->pesNb = sampleRate / 4096;
      prop->pesNbSec = sampleRate / 4096;
      break;

    case STREAM_CODING_TYPE_IG: /* Interactive Graphic Stream (IGS, Menus) */
    case STREAM_CODING_TYPE_PG: /* Presentation Graphic Stream (PGS, Subs) */
      prop->bitRateBasedDurationAlternativeMode = true;
      break;

    default:
      LIBBLU_ERROR_RETURN(
        "Missing PES packets timing information specification "
        "for %s stream coding type.\n",
        streamCodingTypeStr(prop->codingType)
      );
  }

  assert(
    0 < prop->pesNb
    || prop->bitRateBasedDurationAlternativeMode
  );

  return 0;
}

int prepareLibbluES(
  LibbluESPtr es,
  LibbluESFormatUtilities * esAssociatedUtilities,
  bool forceRebuild
)
{
  LibbluESFormatUtilities utilities;

  /* Check and/or generate ES script */
  cleanLibbluESFormatUtilities(&utilities);
  if (checkScriptFileLibbluES(es, &utilities, forceRebuild) < 0)
    return -1;

  /* Open and parse ES script */
  if (parseScriptLibbluES(es) < 0)
    return -1;

  /* Open each source file */
  if (openAllEsmsESSourceFiles(&es->sourceFiles) < 0)
    return -1;

  if (!utilities.initialized) {
    /* No script generation done, use script content */
    /* Init format utilities according to script stream coding type. */
    if (initLibbluESFormatUtilities(&utilities, es->prop.codingType) < 0)
      return -1;
  }

  LIBBLU_SCRIPT_DEBUG("Set PES packets timing informations.\n");
  if (setPesPacketsDurationLibbluES(es) < 0)
    return -1;

  if (NULL != esAssociatedUtilities)
    *esAssociatedUtilities = utilities;

  return 0;
}

static EsmsPesPacketNodePtr extractPesPacketScriptsQueueLibbluES(
  LibbluESPtr es
)
{
  EsmsPesPacketNodePtr node;

  if (NULL != (node = es->pesPacketsScriptsQueue)) {
    es->pesPacketsScriptsQueue = node->next;
    es->pesPacketsScriptsQueueSize--;
  }

  return node;
}

static int addPesPacketToBdavStdLibbluES(
  LibbluESPtr es,
  LibbluESPesPacketProperties prop,
  uint16_t pid,
  uint64_t refPcr
)
{
  uint64_t removalTimestamp, outputTimestamp;

  assert(NULL != es->lnkdBufList);

  if (
    es->prop.codingType == STREAM_CODING_TYPE_AVC
    && prop.extDataPresent
  ) {
    removalTimestamp = prop.extData.h264.cpbRemovalTime + refPcr;
    outputTimestamp = prop.extData.h264.dpbOutputTime + refPcr;
  }
  else {
    removalTimestamp = prop.dts;
    outputTimestamp = prop.pts;
  }

  LIBBLU_T_STD_VERIF_DECL_DEBUG(
    "Registering %zu+%zu=%zu bytes of PES for PID 0x%04" PRIX16 ", "
    "Remove: %" PRIu64 ", Output: %" PRIu64 ".\n",
    prop.headerSize,
    prop.payloadSize,
    prop.headerSize + prop.payloadSize,
    pid,
    removalTimestamp,
    outputTimestamp
  );

  if (es->prop.type == ES_VIDEO) {
    if (
      addFrameToBufferFromList(
        es->lnkdBufList, MULTIPLEX_BUFFER, NULL,
        (BufModelBufferFrame) {
          .headerSize = prop.headerSize * 8,
          .dataSize = prop.payloadSize * 8,
          .removalTimestamp = removalTimestamp,
          .outputDataSize = 0,
          .doNotRemove = false
        }
      ) < 0
    )
      return -1;

    return addFrameToBufferFromList(
      es->lnkdBufList, ELEMENTARY_BUFFER, NULL,
      (BufModelBufferFrame) {
        .headerSize = 0,
        .dataSize = prop.payloadSize * 8,
        .removalTimestamp = outputTimestamp,
        .outputDataSize = 0,
        .doNotRemove = false
      }
    );
  }

  return addFrameToBufferFromList(
    es->lnkdBufList, MAIN_BUFFER, NULL,
    (BufModelBufferFrame) {
      .headerSize = prop.headerSize * 8,
      .dataSize = prop.payloadSize * 8,
      .removalTimestamp = removalTimestamp,
      .outputDataSize = 0,
      .doNotRemove = false
    }
  );
}

int buildAndQueueNextPesPacketPropertiesLibbluES(
  LibbluESPtr es,
  uint64_t refPcr,
  LibbluESPesPacketHeaderPrepFun preparePesHeader
)
{
  EsmsPesPacketNodePtr esmsNode;
  LibbluESPesPacketPropertiesNodePtr node;

  if (
    es->pesPacketsScriptsQueueSize < LIBBLU_ES_MIN_BUF_PES_SCRIPT_PACKETS
    && !es->endOfScriptReached
  ) {
    unsigned i;

    for (
      i = 0;
      i < LIBBLU_ES_INC_BUF_PES_SCRIPT_PACKETS
      && !isEndReachedESPesCuttingEsms(es->scriptFile);
      i++
    ) {

      esmsNode = parseFrameNodeESPesCuttingEsms(
        es->scriptFile,
        es->prop.codingType,
        &es->scriptCommandParsing
      );
      if (NULL == esmsNode)
        return -1;

      if (NULL == es->pesPacketsScriptsQueue)
        es->pesPacketsScriptsQueue = esmsNode;
      else
        attachEsmsPesPacketNode(es->pesPacketsScriptsQueueLastNode, esmsNode);
      es->pesPacketsScriptsQueueLastNode = esmsNode;
      es->pesPacketsScriptsQueueSize++;
    }

    if (isEndReachedESPesCuttingEsms(es->scriptFile))
      es->endOfScriptReached = true;
  }

  if (NULL == (esmsNode = extractPesPacketScriptsQueueLibbluES(es)))
    LIBBLU_ERROR_RETURN(
      "Unable to build next PES packet, end of script reached.\n"
    );

  node = prepareLibbluESPesPacketPropertiesNode(
    esmsNode,
    refPcr,
    es->refPts,
    preparePesHeader,
    es->prop.codingType
  );
  if (NULL == node)
    return -1;

  destroyEsmsPesPacketNode(esmsNode, false);

  if (NULL == es->pesPacketsQueue)
    es->pesPacketsQueue = node;
  else
    attachLibbluESPesPacketPropertiesNode(es->pesPacketsQueueLastNode, node);
  es->pesPacketsQueueLastNode = node;
  es->pesPacketsQueueSize++;

  return 0;
}

size_t averageSizePesPacketLibbluES(
  const LibbluESPtr es,
  unsigned maxNbSamples
)
{
  return averageSizeLibbluESPesPacketPropertiesNode(
    es->pesPacketsQueueLastNode,
    maxNbSamples
  );
}

static int writePesHeaderLibbluESPesPacketData(
  LibbluESPesPacketData * dst,
  PesPacketHeaderParam prop,
  size_t expectedSize
)
{
  size_t pesHeaderWrittenSize;

  pesHeaderWrittenSize = writePesHeader(dst->data, 0x0, prop);
  if (expectedSize != pesHeaderWrittenSize)
    LIBBLU_ERROR_RETURN(
      "Incorrect ESMS PES header length, expect %zu, set %zu bytes.\n",
      expectedSize,
      pesHeaderWrittenSize
    );

  return 0;
}

int buildPesPacketDataLibbluES(
  const LibbluESPtr es,
  LibbluESPesPacketData * dst,
  const LibbluESPesPacketPropertiesNodePtr node
)
{
  int ret;

  uint8_t * payload;
  size_t payloadSize;

  EsmsCommandNodePtr commandNode;

  if (allocateLibbluESPesPacketData(dst, node->size) < 0)
    return -1;
  dst->dataOffset = 0;
  dst->dataUsedSize = 0;

  if (writePesHeaderLibbluESPesPacketData(dst, node->prop.header, node->prop.headerSize) < 0)
    return -1;
  payload = dst->data + node->prop.headerSize;
  payloadSize = node->prop.payloadSize;

  for (
    commandNode = node->commands;
    NULL != commandNode;
    commandNode = commandNode->next
  ) {
    EsmsCommand command = commandNode->command;

    ret = 0;
    switch (command.type) {
      case ESMS_ADD_DATA:
        ret = applyEsmsAddDataCommand(
          command.data.addData,
          payload,
          payloadSize
        );
        break;

      case ESMS_CHANGE_BYTEORDER:
        ret = applyEsmsChangeByteOrderCommand(
          command.data.changeByteOrder,
          payload,
          payloadSize
        );
        break;

      case ESMS_ADD_PAYLOAD_DATA:
        ret = applyEsmsAddPesPayloadCommand(
          command.data.addPesPayload,
          payload,
          payloadSize,
          handlesEsmsESSourceFiles(es->sourceFiles),
          nbUsedFilesEsmsESSourceFiles(es->sourceFiles)
        );
        break;

      case ESMS_ADD_PADDING_DATA:
        ret = applyEsmsAddPaddingCommand(
          command.data.addPadding,
          payload,
          payloadSize
        );
        break;

      case ESMS_ADD_DATA_SECTION:
        ret = applyEsmsAddDataBlockCommand(
          command.data.addDataBlock,
          payload,
          payloadSize,
          es->scriptDataSections
        );
    }

    if (ret < 0)
      return -1;
  }

  dst->dataUsedSize = node->prop.headerSize + node->prop.payloadSize;

  return 0;
}

static inline LibbluESPesPacketPropertiesNodePtr extractPesPacketQueueLibbluES(
  LibbluESPtr es
)
{
  LibbluESPesPacketPropertiesNodePtr node;

  if (NULL != (node = es->pesPacketsQueue)) {
    es->pesPacketsQueue = node->next;
    es->pesPacketsQueueSize--;
  }

  return node;
}

int buildNextPesPacketLibbluES(
  LibbluESPtr es,
  uint16_t pid,
  uint64_t refPcr,
  LibbluESPesPacketHeaderPrepFun preparePesHeader
)
{
  LibbluESPesPacketPropertiesNodePtr node;

  if (es->pesPacketsQueueSize < LIBBLU_ES_MIN_BUF_PES_PACKETS) {
    unsigned i;

    for (
      i = 0;
      i < LIBBLU_ES_INC_BUF_PES_PACKETS
      && !endOfPesPacketsScriptsQueueLibbluES(*es);
      i++
    ) {
      if (buildAndQueueNextPesPacketPropertiesLibbluES(es, refPcr, preparePesHeader) < 0)
        return -1;
    }
  }

  if (NULL == (node = extractPesPacketQueueLibbluES(es)))
    return 0; /* Empty queue */

  if (buildPesPacketDataLibbluES(es, &es->curPesPacket.data, node) < 0)
    goto free_return;
  es->curPesPacket.prop = node->prop;

  /* Add to stream buffering model chain if used */
  if (NULL != es->lnkdBufList) {
    if (addPesPacketToBdavStdLibbluES(es, node->prop, pid, refPcr) < 0)
      return -1;
  }

  destroyLibbluESPesPacketPropertiesNode(node, false);
  return 1;

free_return:
  destroyLibbluESPesPacketPropertiesNode(node, false);
  return -1;
}