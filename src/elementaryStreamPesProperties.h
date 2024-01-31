

#ifndef __LIBBLU_MUXER__ELEMENTARY_STREAM_PES_PROPERTIES_H__
#define __LIBBLU_MUXER__ELEMENTARY_STREAM_PES_PROPERTIES_H__

#include "streamCodingType.h"
#include "esms/scriptData.h"
#include "pesPackets.h"

/* ### ES Pes Packet Properties : ########################################## */

/** \~english
 * \brief Elementary Stream PES packet properties.
 *
 * Structure used for the pending PES packet of an Elementary Stream. Since
 * PES packets size may vary for variable bitrate ES, data array use may vary
 * and is tracked using #dataUsedLength and #dataAllocatedLength fields.
 */
typedef struct {
  uint64_t pts;            /**< PES packet PTS (Presentation Time Stamp) in
    27MHz clock.                                                             */
  uint64_t dts;            /**< PES packet DTS (Decoding Time Stamp) in
    27MHz clock.
    \note This field is currently only used if #extensionFrame == true.       */

  bool extensionFrame;     /**< Is an extension frame (used to set current
    PES frame as a nasted extension sub-stream and use correct timing
    parameters).                                                             */
  bool dtsPresent;         /**< DTS (Decoding TimeStamp) field presence.     */
  bool extDataPresent;     /**< PES packet header extension data presence.   */

  EsmsPesPacketExtData extData;  /**< PES packet header extension
    data. Used if #extDataPresent is set to true.                            */

  size_t payloadSize;
} LibbluESPesPacketProperties;

typedef int (*LibbluPesPacketHeaderPrep_fun) (
  PesPacketHeaderParam *,
  LibbluESPesPacketProperties,
  LibbluStreamCodingType
);

int prepareLibbluESPesPacketProperties(
  LibbluESPesPacketProperties * dst,
  EsmsPesPacket * esms_pes,
  uint64_t initial_STC,
  uint64_t referentialTs
);

int preparePesHeaderCommon(
  PesPacketHeaderParam * dst,
  LibbluESPesPacketProperties prop,
  LibbluStreamCodingType codingType
);

/* ### ES Pes Packet Properties Node : ##################################### */

#if 0

typedef struct LibbluESPesPacketPropertiesNode {
  struct LibbluESPesPacketPropertiesNode * next;

  LibbluESPesPacketProperties prop;
  EsmsCommandNodePtr commands;
  size_t size;
} LibbluESPesPacketPropertiesNode, *LibbluESPesPacketPropertiesNodePtr;

LibbluESPesPacketPropertiesNodePtr createLibbluESPesPacketPropertiesNode(
  void
);

void destroyLibbluESPesPacketPropertiesNode(
  LibbluESPesPacketPropertiesNodePtr node,
  bool recursive
);

LibbluESPesPacketPropertiesNodePtr prepareLibbluESPesPacketPropertiesNode(
  EsmsPesPacketNodePtr scriptNode,
  uint64_t initial_STC,
  uint64_t referentialTs,
  LibbluPesPacketHeaderPrep_fun preparePesHeader,
  LibbluStreamCodingType codingType
);

static inline void attachLibbluESPesPacketPropertiesNode(
  LibbluESPesPacketPropertiesNodePtr root,
  const LibbluESPesPacketPropertiesNodePtr sibling
)
{
  assert(NULL != root);

  root->next = sibling;
}

size_t averageSizeLibbluESPesPacketPropertiesNode(
  const LibbluESPesPacketPropertiesNodePtr root,
  unsigned maxNbSamples
);

#endif

/* ### ES Pes Packet Data : ################################################ */

typedef struct {
  uint8_t * data;       /**< PES packet data.                                */
  uint32_t alloc_size;  /**< PES packet data array allocated size in bytes.  */

  uint32_t offset;  /**< PES packet data writing offset.                     */
  uint32_t size;    /**< PES packet data array used size in bytes.           */
} LibbluESPesPacketData;

static inline void initLibbluESPesPacketData(
  LibbluESPesPacketData * dst
)
{
  *dst = (LibbluESPesPacketData) {
    0
  };
}

static inline void cleanLibbluESPesPacketData(
  LibbluESPesPacketData pes_packet_data
)
{
  free(pes_packet_data.data);
}

int allocateLibbluESPesPacketData(
  LibbluESPesPacketData * dst,
  uint32_t size
);

#endif