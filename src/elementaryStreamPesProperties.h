

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
  bool extension_frame;  /**< Is an extension frame (used to set current
    PES frame as a nasted extension sub-stream and use correct timing
    parameters).                                                             */
  bool dts_present;      /**< DTS (Decoding TimeStamp) field presence.       */

  uint64_t pts;  /**< PES packet PTS (Presentation TimeStamp) in 27MHz
    clock ticks unit.                                                        */
  uint64_t dts;  /**< PES packet DTS (Decoding TimeStamp) in 27MHz clock
    ticks unit. This field is currently only used if
    #extension_frame == true.                                                */

  EsmsPesPacketExtData extension_data;  /**< PES packet header extension
    data. Used if #extension_data_pres is set to true.                       */
  bool extension_data_pres;             /**< PES packet header extension
    data presence.                                                           */

  size_t payload_size;
} LibbluESPesPacketProperties;

typedef int (*LibbluPesPacketHeaderPrep_fun) (
  PesPacketHeaderParam *,
  LibbluESPesPacketProperties,
  LibbluStreamCodingType
);

int prepareLibbluESPesPacketProperties(
  LibbluESPesPacketProperties *dst,
  EsmsPesPacket *esms_pes,
  uint64_t ref_STC_timestamp,
  uint64_t referentialTs
);

int preparePesHeaderCommon(
  PesPacketHeaderParam *dst,
  LibbluESPesPacketProperties prop,
  LibbluStreamCodingType codingType
);

/* ### ES Pes Packet Data : ################################################ */

typedef struct {
  uint8_t *data;       /**< PES packet data.                                */
  uint32_t alloc_size;  /**< PES packet data array allocated size in bytes.  */

  uint32_t offset;  /**< PES packet data writing offset.                     */
  uint32_t size;    /**< PES packet data array used size in bytes.           */
} LibbluESPesPacketData;

static inline void initLibbluESPesPacketData(
  LibbluESPesPacketData *dst
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
  LibbluESPesPacketData *dst,
  uint32_t size
);

#endif