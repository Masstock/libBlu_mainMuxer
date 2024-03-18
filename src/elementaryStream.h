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
  lbc *es_filepath;         /**< Main ES absolute filepath. This value
    can't be NULL.                                                           */
  lbc *es_script_filepath;  /**< Linked ESMS absolute filepath. If this
    value is NULL, the program will use a valid filename. If path pointed
    script is invalid or incompatible (or force rebuild is active). A new
    script will be generated from supplied main ES filepath.                 */

  LibbluStreamCodingType coding_type;  /**< Expected stream coding type. If
    equal to a negative value, the program will try to guess file type.      */

  uint16_t pid;  /**< Requested PID value. If equal to 0x0000, the
    the best matching PID value will be picked. Not all values are allowed
    as some are reserved.                                                    */

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
  LibbluESSettings *dst
)
{
  assert(NULL != dst);

  *dst = (LibbluESSettings) {
    .coding_type = -1
  };

  return dst;
}

static inline void cleanLibbluESSettings(
  LibbluESSettings settings
)
{
  free(settings.es_filepath);
  free(settings.es_script_filepath);
  free(settings.options.pbr_filepath);
}

/* ### ES settings definition : ############################################ */

int setFpsChangeLibbluESSettings(
  LibbluESSettings *dst,
  const lbc *expr
);

int setArChangeLibbluESSettings(
  LibbluESSettings *dst,
  const lbc *expr
);

int setLevelChangeLibbluESSettings(
  LibbluESSettings *dst,
  const lbc *expr
);

#define LIBBLU_MIN_HDMV_INIT_TIMESTAMP  0

#define LIBBLU_MAX_HDMV_INIT_TIMESTAMP  0x7FFFFFFF

int setHdmvInitialTimestampLibbluESSettings(
  LibbluESSettings *dst,
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
  LibbluESSettings *dst,
  const lbc *filepath,
  const lbc *anchorFilepath
);

int setScriptFilepathLibbluESSettings(
  LibbluESSettings *dst,
  const lbc *filepath,
  const lbc *anchorFilepath
);

static inline void setExpectedCodingTypeLibbluESSettings(
  LibbluESSettings *dst,
  LibbluStreamCodingType codingType
)
{
  dst->coding_type = codingType;
}

static inline void setRequestedPIDLibbluESSettings(
  LibbluESSettings *dst,
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
  const LibbluESSettings *settings_ref;  /**< Pointer to ES settings.       */
  lbc *script_filepath;

  LibbluESProperties prop;   /**< ES properties.                             */
  LibbluESFmtProp fmt_prop;  /**< ES format specific properties.             */

  /* Timing information */
  uint64_t PTS_reference;  /**< Referential 'zero' timestamp in 90kHz clock
    ticks.                                                                   */
  uint64_t PTS_final;      /**< Last PTS in 90kHz clock ticks.               */

  BufModelBuffersListPtr lnkd_tstd_buf_list;  /**< ES linked buffering model
    buffers list.                                                            */

  struct {
    BitstreamReaderPtr bs_handle;
    EsmsESSourceFiles es_source_files;
    EsmsDataBlocks data_sections;
    EsmsCommandParsingData commands_data_parsing;

    CircularBuffer pes_packets_queue; // type: EsmsPesPacket
    bool end_reached;
  } script;

  struct {
    PesPacketHeaderParam header;
    LibbluESPesPacketData data;
    bool extension_frame;

    uint64_t PTS; /**< PES packet PTS (Presentation TimeStamp) in 27MHz
    clock ticks unit.                                                        */
    uint64_t DTS; /**< PES packet DTS (Decoding TimeStamp) in 27MHz clock
    ticks unit. This field is currently only used if
    #extension_frame == true.                                                */
  } current_pes_packet;
} LibbluES;

#define LIBBLU_ES_MIN_BUF_PES_SCRIPT_PACKETS  50
#define LIBBLU_ES_INC_BUF_PES_SCRIPT_PACKETS  200
#define LIBBLU_ES_MIN_BUF_PES_PACKETS  30
#define LIBBLU_ES_INC_BUF_PES_PACKETS  50

static inline void cleanLibbluES(
  LibbluES es
)
{
  free(es.script_filepath);
  free(es.fmt_prop.shared_ptr);
  destroyBufModelBuffersList(es.lnkd_tstd_buf_list);
  closeBitstreamReader(es.script.bs_handle);
  cleanEsmsESSourceFiles(es.script.es_source_files);
  cleanEsmsDataBlocks(es.script.data_sections);
  cleanEsmsCommandParsingData(es.script.commands_data_parsing);

  // Flush circular buffer:
  EsmsPesPacket *esms_pes_packet;
  while (NULL != (esms_pes_packet = popCircularBuffer(&es.script.pes_packets_queue))) {
    cleanEsmsPesPacketPtr(esms_pes_packet);
  }
  cleanCircularBuffer(es.script.pes_packets_queue);

  cleanLibbluESPesPacketData(es.current_pes_packet.data);
}

int prepareLibbluES(
  LibbluES *es,
  LibbluESFormatUtilities *es_utilities_ret,
  bool force_script_build
);

/** \~english
 * \brief Return the number of bytes remaining in the current PES packet.
 *
 * \param stream Elementary Stream handle.
 * \return size_t Number of remaining bytes in the ES current PES packet.
 */
static inline size_t remainingPesDataLibbluES(
  const LibbluES *es
)
{
  return
    es->current_pes_packet.data.size
    - es->current_pes_packet.data.offset
  ;
}

static inline bool isPayloadUnitStartLibbluES(
  LibbluES es
)
{
  return (0 == es.current_pes_packet.data.offset);
}

int buildNextPesPacketLibbluES(
  LibbluES *es,
  uint16_t pid,
  uint64_t refPcr,
  LibbluPesPacketHeaderPrep_fun preparePesHeader
);

#endif