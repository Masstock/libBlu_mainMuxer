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
  LibbluESSettings * settings_ref;  /**< Pointer to ES settings.   */
  LibbluESProperties prop;      /**< ES properties.            */
  LibbluESFmtProp fmt_prop;  /**<
    ES format specific properties.                                           */

  /* Timing information */
  uint64_t PTS_reference;  /**< Referential 'zero' timestamp in 90kHz clock ticks. */
  uint64_t PTS_final;      /**< Last PTS in 90kHz clock ticks. */
  uint64_t startPts;

  BufModelBuffersListPtr lnkdBufList;         /**< ES linked buffering model
    buffers list.                                                            */

  /* Script related */
  BitstreamReaderPtr scriptFile;
  EsmsESSourceFiles sourceFiles;
  EsmsDataBlocks scriptDataSections;
  EsmsCommandParsingData commands_data_parsing;

#if 0
  /* Input stream files */
  lbc * streamFilesfilenames[ESMS_MAX_ALLOWED_DIR];
  BitstreamReaderPtr streamFilesHandles[ESMS_MAX_ALLOWED_DIR];
  unsigned nbStreamFiles;
#endif

  /* PES packets */
  CircularBuffer pesPacketsScriptsQueue_; // type: EsmsPesPacket

  struct {
    PesPacketHeaderParam header;
    LibbluESPesPacketData data;

    uint64_t pts;
    uint64_t dts;
    bool extension_frame;
  } current_pes_packet;

  /* Progression related */
  bool parsedProperties;
  bool initializedPesCutting;
  bool endOfScriptReached;

  unsigned nbPesPacketsMuxed;
} LibbluES;

#define LIBBLU_ES_MIN_BUF_PES_SCRIPT_PACKETS  50
#define LIBBLU_ES_INC_BUF_PES_SCRIPT_PACKETS  200
#define LIBBLU_ES_MIN_BUF_PES_PACKETS  30
#define LIBBLU_ES_INC_BUF_PES_PACKETS  50

static inline void cleanLibbluES(
  LibbluES es
)
{
  free(es.fmt_prop.shared_ptr);
  destroyBufModelBuffersList(es.lnkdBufList);
  closeBitstreamReader(es.scriptFile);
  cleanEsmsESSourceFiles(es.sourceFiles);
  cleanEsmsDataBlocks(es.scriptDataSections);
  cleanEsmsCommandParsingData(es.commands_data_parsing);

  // Flush circular buffer:
  EsmsPesPacket * esms_pes_packet;
  while (NULL != (esms_pes_packet = popCircularBuffer(&es.pesPacketsScriptsQueue_))) {
    cleanEsmsPesPacketPtr(esms_pes_packet);
  }
  cleanCircularBuffer(es.pesPacketsScriptsQueue_);

  cleanLibbluESPesPacketData(es.current_pes_packet.data);
}

int prepareLibbluES(
  LibbluES * es,
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
  const LibbluES * es
)
{
  return
    es->current_pes_packet.data.size
    - es->current_pes_packet.data.offset
  ;
}

#if 0

static inline bool endOfPesPacketsScriptsQueueLibbluES(
  LibbluES es
)
{
  return
    isEndReachedESPesCuttingEsms(es.scriptFile)
    && (0 == nbEntriesCircularBuffer(&es.pesPacketsScriptsQueue_))
  ;
}

#endif

static inline bool isPayloadUnitStartLibbluES(
  LibbluES es
)
{
  return (0 == es.current_pes_packet.data.offset);
}

// size_t averageSizePesPacketLibbluES(
//   const LibbluES * es,
//   unsigned maxNbSamples
// );

#if 0

int buildPesPacketDataLibbluES(
  const LibbluES * es,
  LibbluESPesPacketData * dst,
  const LibbluESPesPacketPropertiesNodePtr node
);

#endif

int buildNextPesPacketLibbluES(
  LibbluES * es,
  uint16_t pid,
  uint64_t refPcr,
  LibbluPesPacketHeaderPrep_fun preparePesHeader
);

#endif