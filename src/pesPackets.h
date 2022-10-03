/** \~english
 * \file pesPackets.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief PES frames generation module.
 */

#ifndef __LIBBLU_MUXER__PES_PACKETS_H__
#define __LIBBLU_MUXER__PES_PACKETS_H__

#include "streamCodingType.h"
#include "util.h"

#define IS_SHORT_PES_HEADER(stream_id)                                        \
  (                                                                           \
    (stream_id) == 0xBC                                                       \
    || (stream_id) == 0xBE                                                    \
    || (stream_id) == 0xBF                                                    \
    || (stream_id) == 0xF0                                                    \
    || (stream_id) == 0xF1                                                    \
    || (stream_id) == 0xF2                                                    \
    || (stream_id) == 0xF8                                                    \
    || (stream_id) == 0xFF                                                    \
  )

#define PES_PACKET_START_CODE_PREFIX     0x00000100
#define PES_PRE_HEADER_LEN           0x6
#define PES_PRE_STREAM_HEADER_LEN    0x3

#define TRICKMODE_FAST_FORWARD      0x00
#define TRICKMODE_SLOW_MOTION       0x01
#define TRICKMODE_FREEZE_FRAME      0x02
#define TRICKMODE_FAST_REVERSE      0x03
#define TRICKMODE_SLOW_REVERSE      0x04

typedef struct {
  bool pesPrivateDataFlag;        /* PES_private_data_flag                */
  bool packHeaderFieldFlag;       /* pack_header_field_flag               */
  bool programPacketSequenceCounterFlag; /* program_packet_sequence_counter_flag     */
  bool pStdBufferFlag;            /* P-STD_buffer_flag                    */
  bool pesExtensionFlag2;         /* PES_extension_flag_2                 */

  uint8_t * pesPrivateData;       /* PES_private_data                     */

  uint8_t packFieldLength;        /* pack_field_length                    */
  uint8_t * packHeader;           /* pack_header()                        */

  uint8_t programPacketSequenceCounter;  /* program_packet_sequence_counter          */
  bool mpeg1H262Identifier;      /* MPEG1_H262_identifier               */
  uint8_t originalStuffLength;    /* original_stuff_length                */

  bool pStdBufferScale;           /* P-STD_buffer_scale                   */
  uint8_t pStdBufferSize;         /* P-STD_buffer_size                    */

  uint8_t pesExtensionFieldLength;       /* PES_extension_field_length               */
  bool streamIdExtensionFlag;     /* stream_id_extension_flag             */
  uint8_t streamIdExtension;      /* stream_id_extension                  */

  bool trefExtensionFlag;         /* tref_extension_flag                  */
  uint32_t tref;                  /* TREF                                 */
} PesPacketHeaderExtensionParam;

typedef struct {
  uint32_t packetStartCode;       /* packet_start_code_prefix + stream_id */
  uint32_t pesPacketLength;       /* PES_packet_length                    */

  uint8_t pesScramblingControl;   /* PES_scrambling_control               */
  bool pesPriority;               /* PES_priority                         */
  bool dataAlignmentIndicator;    /* data_alignment_indicator             */
  bool copyright;                 /* copyright                            */
  bool originalOrCopy;            /* original_or_copy                     */
  bool ptsFlag;                   /* PTS_DTS_flags                        */
  bool dtsFlag;                   /*  "                                   */
  bool escrFlag;                  /* ESCR_flag                            */
  bool esRateFlag;                /* ES_rate_flag                         */
  bool dsmTrickModeFlag;          /* DSM_trick_mode_flag                  */
  bool additionalCopyInfoFlag;    /* additional_copy_info_flag            */
  bool pesCrcFlag;                /* PES_CRC_flag                         */
  bool pesExtFlag;                /* PES_extension_flag                   */

  uint8_t pesHeaderDataLen;       /* PES_header_data_length               */

  uint64_t pts;                   /* PTS                                  */
  uint64_t dts;                   /* DTS                                  */

  uint64_t escr;                  /* ESCR                                 */

  uint32_t esRate;                /* ES_rate                              */

  struct {
    uint8_t trickModeControl;     /* trick_mode_control                   */

    uint8_t fieldId;              /* field_id                             */
    bool intraSlideRefresh;       /* intra_slice_refresh                  */
    uint8_t frequencyTruncation;  /* frequency_truncation                 */
    uint8_t repCntrl;             /* rep_cntrl                            */
  } dsmTrickModeParam;

  uint8_t additionalCopyInfo;     /* additional_copy_info                 */

  uint16_t previousPesPacketCrc;  /* previous_PES_packet_CRC              */

  PesPacketHeaderExtensionParam extParam;
} PesPacketHeaderParam;

static inline size_t computePesPacketHeaderLen(
  PesPacketHeaderParam headerParam
)
{
  return
    headerParam.pesHeaderDataLen
    + PES_PRE_HEADER_LEN
    + (
      IS_SHORT_PES_HEADER(headerParam.packetStartCode & 0xFF) ?
        0 : PES_PRE_STREAM_HEADER_LEN
    )
  ;
}

static inline void setLengthPesPacketHeaderParam(
  PesPacketHeaderParam * dst,
  size_t headerSize,
  size_t payloadSize
)
{
  if (!headerSize)
    headerSize = computePesPacketHeaderLen(*dst);
  /* minus packet_start_code_prefix, stream_id and PES_packet_length fields. */
  dst->pesPacketLength = headerSize + payloadSize - PES_PRE_HEADER_LEN;
}

size_t writePesHeader(
  uint8_t * packetData,
  size_t offset,
  const PesPacketHeaderParam param
);

#endif