/** \~english
 * \file elementaryStream.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Elementary stream handling module.
 */

#ifndef __LIBBLU_MUXER__ELEMENTARY_STREAM_H__
#define __LIBBLU_MUXER__ELEMENTARY_STREAM_H__

#include "codecsUtilities.h"
#include "elementaryStreamOptions.h"
#include "elementaryStreamPesProperties.h"
#include "elementaryStreamProperties.h"
#include "esms/scriptData.h"
#include "esms/scriptParsing.h"
#include "packetIdentifier.h"
#include "streamCodingType.h"
#include "tStdVerifier/bufferingModel.h"
#include "util.h"

/** \~english
 * \brief Elementary Stream settings.
 *
 */
typedef struct {
  lbc * filepath;               /**< Main ES absolute filepath. This value
    can't be NULL.                                                           */
  lbc * scriptFilepath;         /**< Linked ESMS absolute filepath. If this
    value is NULL, the program will use a valid filename. If path pointed
    script is invalid or incompatible (or force rebuild is active). A new
    script will be generated from supplied main ES filepath.                 */

  LibbluStreamCodingType codingType;  /**< Expected stream coding type. If
    equal to a negative value, the program will try to guess file type.      */
  uint16_t pid;                 /**< Requested PID value. If equal to 0, the
    program will select the best matching PID value.
    \note Some PID values are reserved and can't be used (see H.222
    standard).                                                               */

  LibbluESSettingsOptions options;  /**< ES options.                         */
} LibbluESSettings;

#define LIBBLU_ES_SETTINGS_RESET_OPTIONS(es)                                  \
  ((es)->options = (LibbluESSettingsOptions) {0})

#define LIBBLU_ES_SETTINGS_SET_OPTION(es, opname, val)                        \
  ((es)->options.opname = (val))

#define LIBBLU_ES_SETTINGS_OPTION(es, opname)                                 \
  ((es)->options.opname)

/** \~english
 * \brief Initiate supplied #LibbluESSettings structure.
 *
 * \param dst ES settings to initiate.
 * \return LibbluESSettings* Return dst.
 *
 * The returned pointer is the supplied one (no return value is reserved to
 * indicate an error). This enable chaining calls.
 *
 * \note #LibbluESSettings 'pid' and 'options' members are
 * initialized to 0.
 */
static inline LibbluESSettings * initLibbluESSettings(
  LibbluESSettings * dst
)
{
  assert(NULL != dst);

  *dst = (LibbluESSettings) {
    .codingType = -1
  };

  return dst;
}

static inline void cleanLibbluESSettings(
  LibbluESSettings settings
)
{
  free(settings.filepath);
  free(settings.scriptFilepath);
  free(settings.options.pbrFilepath);
}

/* ### ES settings definition : ############################################ */

int setFpsChangeLibbluESSettings(
  LibbluESSettings * dst,
  const lbc * expr
);

int setArChangeLibbluESSettings(
  LibbluESSettings * dst,
  const lbc * expr
);

int setLevelChangeLibbluESSettings(
  LibbluESSettings * dst,
  const lbc * expr
);

#define LIBBLU_MIN_HDMV_INIT_TIMESTAMP  0

#define LIBBLU_MAX_HDMV_INIT_TIMESTAMP  0x7FFFFFFF

int setHdmvInitialTimestampLibbluESSettings(
  LibbluESSettings * dst,
  uint64_t value
);

/** \~english
 * \brief Set the ES attached main source file parsed to generate ESMS.
 *
 * \param dst
 * \param filepath Main ES filepath. The path can be relative or absolute
 * (converted to absolute).
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int setMainESFilepathLibbluESSettings(
  LibbluESSettings * dst,
  const lbc * filepath,
  const lbc * anchorFilepath
);

int setScriptFilepathLibbluESSettings(
  LibbluESSettings * dst,
  const lbc * filepath,
  const lbc * anchorFilepath
);

static inline void setExpectedCodingTypeLibbluESSettings(
  LibbluESSettings * dst,
  LibbluStreamCodingType codingType
)
{
  dst->codingType = codingType;
}

static inline void setRequestedPIDLibbluESSettings(
  LibbluESSettings * dst,
  uint16_t pid
)
{
  dst->pid = pid;
}

/** \~english
 * \brief Elementary Stream handling structure.
 *
 */
typedef struct {
  LibbluESSettings * settings;  /**< Pointer to ES settings.   */
  LibbluESProperties prop;      /**< ES properties.            */
  LibbluESFmtSpecProp fmtSpecProp;  /**<
    ES format specific properties.                                           */

  /* Timing information */
  uint64_t refPts;
  uint64_t startPts;
  uint64_t endPts;

  BufModelBuffersListPtr lnkdBufList;         /**< ES linked buffering model
    buffers list.                                                            */

  /* Script related */
  BitstreamReaderPtr scriptFile;
  EsmsESSourceFiles sourceFiles;
  EsmsDataBlocks scriptDataSections;
  EsmsCommandParsingData scriptCommandParsing;

#if 0
  /* Input stream files */
  lbc * streamFilesfilenames[ESMS_MAX_ALLOWED_DIR];
  BitstreamReaderPtr streamFilesHandles[ESMS_MAX_ALLOWED_DIR];
  unsigned nbStreamFiles;
#endif

  /* PES packets */
  EsmsPesPacketNodePtr pesPacketsScriptsQueue;
  EsmsPesPacketNodePtr pesPacketsScriptsQueueLastNode;
  unsigned pesPacketsScriptsQueueSize;

  LibbluESPesPacketPropertiesNodePtr pesPacketsQueue;
  LibbluESPesPacketPropertiesNodePtr pesPacketsQueueLastNode;
  unsigned pesPacketsQueueSize;

  struct {
    LibbluESPesPacketProperties prop;
    LibbluESPesPacketData data;
  } curPesPacket;

  /* Progression related */
  bool parsedProperties;
  bool initializedPesCutting;
  bool endOfScriptReached;

  unsigned nbPesPacketsMuxed;
} LibbluES, *LibbluESPtr;

#define LIBBLU_ES_MIN_BUF_PES_SCRIPT_PACKETS  50
#define LIBBLU_ES_INC_BUF_PES_SCRIPT_PACKETS  200
#define LIBBLU_ES_MIN_BUF_PES_PACKETS  30
#define LIBBLU_ES_INC_BUF_PES_PACKETS  50

static inline void initLibbluES(
  LibbluES * dst,
  LibbluESSettings * settings
)
{
  *dst = (LibbluES) {
    .settings = settings,
    .prop = {
      .type = -1,
      .codingType = -1
    },
  };

  initLibbluESFmtSpecProp(&dst->fmtSpecProp, FMT_SPEC_INFOS_NONE);
  initEsmsDataBlocks(&dst->scriptDataSections);
  initEsmsESSourceFiles(&dst->sourceFiles);
  initLibbluESPesPacketData(&dst->curPesPacket.data);
}

static inline void cleanLibbluES(
  LibbluES es
)
{
  free(es.fmtSpecProp.sharedPtr);
  destroyBufModelBuffersList(es.lnkdBufList);
  closeBitstreamReader(es.scriptFile);
  cleanEsmsESSourceFiles(es.sourceFiles);
  cleanEsmsDataBlocks(es.scriptDataSections);
  cleanEsmsCommandParsingData(es.scriptCommandParsing);
  destroyEsmsPesPacketNode(es.pesPacketsScriptsQueue, true);
  destroyLibbluESPesPacketPropertiesNode(es.pesPacketsQueue, true);
  cleanLibbluESPesPacketData(es.curPesPacket.data);
}

int prepareLibbluES(
  LibbluESPtr es,
  LibbluESFormatUtilities * esAssociatedUtilities,
  bool forceRebuild
);

/** \~english
 * \brief Return the number of bytes remaining in the current PES packet.
 *
 * \param stream Elementary Stream handle.
 * \return size_t Number of remaining bytes in the ES current PES packet.
 */
static inline size_t remainingPesDataLibbluES(
  LibbluES es
)
{
  return
    es.curPesPacket.data.dataUsedSize
    - es.curPesPacket.data.dataOffset
  ;
}

static inline bool endOfPesPacketsScriptsQueueLibbluES(
  LibbluES es
)
{
  return
    isEndReachedESPesCuttingEsms(es.scriptFile)
    && (0 == es.pesPacketsScriptsQueueSize)
  ;
}

static inline bool isPayloadUnitStartLibbluES(
  LibbluES es
)
{
  return 0 == es.curPesPacket.data.dataOffset;
}

int buildAndQueueNextPesPacketPropertiesLibbluES(
  LibbluESPtr es,
  uint64_t refPcr,
  LibbluESPesPacketHeaderPrepFun preparePesHeader
);

size_t averageSizePesPacketLibbluES(
  const LibbluESPtr es,
  unsigned maxNbSamples
);

int buildPesPacketDataLibbluES(
  const LibbluESPtr es,
  LibbluESPesPacketData * dst,
  const LibbluESPesPacketPropertiesNodePtr node
);

int buildNextPesPacketLibbluES(
  LibbluESPtr es,
  uint16_t pid,
  uint64_t refPcr,
  LibbluESPesPacketHeaderPrepFun preparePesHeader
);

#endif