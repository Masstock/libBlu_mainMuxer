

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

  /* EsmsCommandNodePtr buildingCommands; */
  size_t payloadSize;
  PesPacketHeaderParam header;
  size_t headerSize;
} LibbluESPesPacketProperties;

typedef int (*LibbluESPesPacketHeaderPrepFun) (
  PesPacketHeaderParam *,
  LibbluESPesPacketProperties,
  LibbluStreamCodingType
);

int prepareLibbluESPesPacketProperties(
  LibbluESPesPacketProperties * dst,
  EsmsPesPacketNodePtr scriptNode,
  uint64_t referentialPcr,
  uint64_t referentialTs,
  LibbluESPesPacketHeaderPrepFun preparePesHeader,
  LibbluStreamCodingType codingType
);

int preparePesHeaderCommon(
  PesPacketHeaderParam * dst,
  LibbluESPesPacketProperties prop,
  LibbluStreamCodingType codingType
);

/* ### ES Pes Packet Properties Node : ##################################### */

typedef struct LibbluESPesPacketPropertiesNode {
  struct LibbluESPesPacketPropertiesNode * next;

  LibbluESPesPacketProperties prop;
  EsmsCommandNodePtr commands;
  size_t size;
} LibbluESPesPacketPropertiesNode, *LibbluESPesPacketPropertiesNodePtr;

LibbluESPesPacketPropertiesNodePtr createLibbluESPesPacketPropertiesNode(
  void
);

static inline void destroyLibbluESPesPacketPropertiesNode(
  LibbluESPesPacketPropertiesNodePtr node,
  bool recursive
)
{
  if (NULL == node)
    return;

  if (recursive)
    destroyLibbluESPesPacketPropertiesNode(node->next, true);

  destroyEsmsCommandNode(node->commands, true);
  free(node);
}

LibbluESPesPacketPropertiesNodePtr prepareLibbluESPesPacketPropertiesNode(
  EsmsPesPacketNodePtr scriptNode,
  uint64_t referentialPcr,
  uint64_t referentialTs,
  LibbluESPesPacketHeaderPrepFun preparePesHeader,
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

/* ### ES Pes Packet Data : ################################################ */

typedef struct {
  uint8_t * data;
  size_t dataOffset;         /**< Current PES packet data writing offset.    */
  size_t dataUsedSize;       /**< PES packet data array used size in
    bytes.                                                                   */
  size_t dataAllocatedSize;  /**< PES packet data array allocated size in
    bytes.                                                                   */
} LibbluESPesPacketData;

static inline void initLibbluESPesPacketData(
  LibbluESPesPacketData * dst
)
{
  assert(NULL != dst);

  *dst = (LibbluESPesPacketData) {
    .data = NULL,
    .dataOffset = 0,
    .dataUsedSize = 0,
    .dataAllocatedSize = 0,
  };
}

static inline void cleanLibbluESPesPacketData(
  LibbluESPesPacketData data
)
{
  free(data.data);
}

int allocateLibbluESPesPacketData(
  LibbluESPesPacketData * dst,
  size_t size
);

#endif