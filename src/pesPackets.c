#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include "pesPackets.h"

size_t writePesHeader(uint8_t * packetData, size_t offset, const PesPacketHeaderParam param)
{
  uint64_t pts, dts;
  uint64_t escrBase;
  uint8_t escrExt, packetStartCodePrefix;
  size_t pesHeaderDataOff, pesExtensionFieldOff, i;

  /* [u24 packet_start_code_prefix] [u8 stream_id] */
  packetData[offset++] = (param.packetStartCode   >> 24) & 0xFF;
  packetData[offset++] = (param.packetStartCode   >> 16) & 0xFF;
  packetData[offset++] = (param.packetStartCode   >>  8) & 0xFF;
  packetData[offset++] = (param.packetStartCode        ) & 0xFF;

  /* [u16 PES_packet_length] */
  if (param.pesPacketLength < 0xFFFF) {
    packetData[offset++] = (param.pesPacketLength >>  8) & 0xFF;
    packetData[offset++] = (param.pesPacketLength      ) & 0xFF;
  }
  else {
    packetData[offset++] = 0x00;
    packetData[offset++] = 0x00;
  }

  packetStartCodePrefix = param.packetStartCode & 0xFF;

  if (
    packetStartCodePrefix != 0xBC && /* program_stream_map               */
    packetStartCodePrefix != 0xBE && /* padding_stream                   */
    packetStartCodePrefix != 0xBF && /* private_stream_2                 */
    packetStartCodePrefix != 0xF0 && /* ECM                              */
    packetStartCodePrefix != 0xF1 && /* EMM                              */
    packetStartCodePrefix != 0xF2 && /* DSMCC_stream                     */
    packetStartCodePrefix != 0xF8 && /* ITU-T Rec. H.222.1 type E stream */
    packetStartCodePrefix != 0xFF    /* program_stream_directory         */
  ) {

    /*
      [v2 '10'] [u2 PES_scrambling_control]
      [b1 PES_priority] [b1 data_alignment_indicator] [b1 copyright] [b1 original_or_copy]
    */
    packetData[offset++] =
      0x80                                                    |
      ((param.pesScramblingControl               & 0x3) << 4) |
      ((param.pesPriority                     ) ? 0x08 : 0x0) |
      ((param.dataAlignmentIndicator          ) ? 0x04 : 0x0) |
      ((param.copyright                       ) ? 0x02 : 0x0) |
      ((param.originalOrCopy                  ) ? 0x01 : 0x0)
    ;

    /*
      [u2 PTS_DTS_flags] [b1 ESCR_flag] [b1 ES_rate_flag]
      [b1 DSM_trick_mode_flag] [b1 additional_copy_info_flag] [b1 PES_CRC_flag] [b1 PES_extension_flag]
    */
    packetData[offset++] =
      ((param.ptsFlag                         ) ? 0x80 : 0x0) |
      ((param.dtsFlag                         ) ? 0x40 : 0x0) |
      ((param.escrFlag                        ) ? 0x20 : 0x0) |
      ((param.esRateFlag                      ) ? 0x10 : 0x0) |
      ((param.dsmTrickModeFlag                ) ? 0x08 : 0x0) |
      ((param.additionalCopyInfoFlag          ) ? 0x04 : 0x0) |
      ((param.pesCrcFlag                      ) ? 0x02 : 0x0) |
      ((param.pesExtFlag                      ) ? 0x01 : 0x0)
    ;

    /* [u8 PES_header_data_length] */
    packetData[offset++] = param.pesHeaderDataLen & 0xFF;
    pesHeaderDataOff = offset - 1;

    if (param.ptsFlag) {
      pts = param.pts / 300;

      /* [v4 '0010'/'0011'] [u3 PTS[32..30]] [v1 marker_bit] */
      packetData[offset++] =
        ((param.ptsFlag                       ) ? 0x20 : 0x0) |
        ((param.dtsFlag                       ) ? 0x10 : 0x0) |
        (((pts                            >> 30) & 0x7) << 1) |
        0x1
      ;

      /* [u15 PTS[29..15]] [v1 marker_bit] */
      packetData[offset++] = ((pts >> 22) & 0xFF);
      packetData[offset++] = ((pts >> 14) & 0xFE) | 0x1;

      /* [u15 PTS[14..0]] [v1 marker_bit] */
      packetData[offset++] = ((pts >>  7) & 0xFF);
      packetData[offset++] = ((pts        & 0x7F) << 1) | 0x1;

      if (param.dtsFlag) {
        dts = param.dts / 300;

        /* [v4 '0001'] [u3 DTS[32..30]] [v1 marker_bit] */
        packetData[offset++] =
          ((param.dtsFlag                &  0x1) << 4) |
          (((dts                  >> 30) &  0x7) << 1) |
          0x1;

        /* [u15 DTS[29..15]] [v1 marker_bit] */
        packetData[offset++] =
          (dts              >> 22) & 0xFF;
        packetData[offset++] =
          (((dts            >> 15) & 0x7F) << 1) |
          0x1;

        /* [u15 DTS[14..0]] [v1 marker_bit] */
        packetData[offset++] =
          (dts              >>  7) & 0xFF;
        packetData[offset++] =
          ((dts                    & 0x7F) << 1) |
          0x1;
      }
    }

    if (param.escrFlag) {
      escrBase = param.escr / 300;
      escrExt  = param.escr % 300;

      /*
        [v2 reserved] [u3 ESCR_base[32..30]] [v1 marker_bit]
        [u15 ESCR_base[29..15]] [v1 marker_bit]
        [u15 ESCR_base[14..0]] [v1 marker_bit]
        [u9 ESCR_extension] [v1 marker_bit]
      */
      packetData[offset++] =
       (((escrBase  >> 30) & 0x07) << 3) |
        ((escrBase  >> 28) & 0x03)       |
        0xC4
      ;
      packetData[offset++] = ((escrBase >> 20) & 0xFF);
      packetData[offset++] =
       (((escrBase  >> 15) & 0x1F) << 3) |
        ((escrBase  >> 13) & 0x03)       |
        0x04
      ;
      packetData[offset++] = ((escrBase >>  5) & 0xFF);
      packetData[offset++] =
        ((escrBase         & 0x1F) << 3) |
        ((escrExt   >>  7) &  0x3)       |
        0x04
      ;
      packetData[offset++] =
       (((escrExt        ) & 0x7F) << 1) |
        0x01
      ;
    }

    if (param.esRateFlag) {
      /* [v1 marker_bit] [u22 ES_rate] [v1 marker_bit] */
      packetData[offset++] = ((param.esRate >> 15) & 0x7F) | 0x80;
      packetData[offset++] =  (param.esRate >>  7) & 0xFF;
      packetData[offset++] = ((param.esRate        & 0x7F) << 1) | 0x01;
    }

    if (param.dsmTrickModeFlag) {
      /* [u3 trick_mode_control] */
      packetData[offset] = (param.dsmTrickModeParam.trickModeControl & 0x7) << 5;

      if (
        param.dsmTrickModeParam.trickModeControl == TRICKMODE_FAST_FORWARD ||
        param.dsmTrickModeParam.trickModeControl == TRICKMODE_FAST_REVERSE
      ) {
        /* [u2 field_id] [b1 intra_slice_refresh] [u2 frequency_truncation] */
        packetData[offset++] |=
         ((param.dsmTrickModeParam.fieldId             & 0x3) << 3) |
          (param.dsmTrickModeParam.intraSlideRefresh          << 2) |
          (param.dsmTrickModeParam.frequencyTruncation & 0x3);
      }
      else if (
        param.dsmTrickModeParam.trickModeControl == TRICKMODE_SLOW_MOTION ||
        param.dsmTrickModeParam.trickModeControl == TRICKMODE_SLOW_REVERSE
      ) {
        /* [u5 rep_cntrl] */
        packetData[offset++] |= param.dsmTrickModeParam.repCntrl & 0x1F;
      }
      else if (param.dsmTrickModeParam.trickModeControl == TRICKMODE_FREEZE_FRAME) {
        /* [u2 field_id] [v3 reserved] */
        packetData[offset++] |= ((param.dsmTrickModeParam.fieldId & 0x3) << 3) | 0x7;
      }
      else {
        /* [v5 reserved] */
        packetData[offset++] |= 0x1F;
      }
    }

    if (param.additionalCopyInfoFlag) {
      /* [v1 reserved] [u7 additional_copy_info] */
      packetData[offset++] = (param.additionalCopyInfo & 0x7F) | 0x80;
    }

    if (param.pesCrcFlag) {
      /* [u16 previous_PES_packet_CRC] */
      packetData[offset++] = param.previousPesPacketCrc >> 8;
      packetData[offset++] = param.previousPesPacketCrc & 0xFF;
    }

    if (param.pesExtFlag) {
      /*
        [b1 PES_private_data_flag] [b1 pack_header_field_flag]
        [b1 program_packet_sequence_counter_flag] [b1 P-STD_buffer_flag]
        [v3 reserved 0b000] [b1 PES_extension_flag_2]
      */
      packetData[offset++] =
        ((param.extParam.pesPrivateDataFlag               ) ? 0x80 : 0x0) |
        ((param.extParam.packHeaderFieldFlag              ) ? 0x40 : 0x0) |
        ((param.extParam.programPacketSequenceCounterFlag ) ? 0x20 : 0x0) |
        ((param.extParam.pStdBufferFlag                   ) ? 0x10 : 0x0) |
        ((param.extParam.pesExtensionFlag2                ) ? 0x01 : 0x0)
      ;

      if (param.extParam.pesPrivateDataFlag) {
        /* [u128 PES_private_data] */
        memcpy(&(packetData[offset]), param.extParam.pesPrivateData, (size_t) 16);
        offset += 16;
      }

      if (param.extParam.packHeaderFieldFlag) {
        /* [u8 pack_field_length] */
        packetData[offset++] = param.extParam.packFieldLength;
        /* [vn pack_field_length] */
        memcpy(&(packetData[offset]), param.extParam.packHeader, (size_t) packetData[offset]);
        offset += param.extParam.packFieldLength;
      }

      if (param.extParam.programPacketSequenceCounterFlag) {
        /* [v1 marker_bit] [u7 program_packet_sequence_counter] */
        packetData[offset++] = (param.extParam.programPacketSequenceCounter & 0x7F) | 0x80;

        /* [v1 marker_bit] [b1 MPEG1_H262_identifier] [u6 original_stuff_length] */
        packetData[offset++] =
          ((param.extParam.mpeg1H262Identifier) ? 0x40 : 0x0) |
          (param.extParam.originalStuffLength & 0x3F) |
          0x80
        ;
      }

      if (param.extParam.pStdBufferFlag) {
        /* [v2 '01'] [b1 P-STD_buffer_scale] [u13 P-STD_buffer_size] */
        packetData[offset++] =
          ((param.extParam.pStdBufferScale) ? 0x20 : 0x0) |
          ((param.extParam.pStdBufferSize >> 8) & 0x1F) |
          0x40
        ;
        packetData[offset++] = (param.extParam.pStdBufferSize & 0xFF);
      }

      if (param.extParam.pesExtensionFlag2) {
        /* [v1 marker_bit] [u7 PES_extension_field_length] */
        packetData[offset++] = (param.extParam.pesExtensionFieldLength & 0x7F) + 0x80;
        pesExtensionFieldOff = offset - 1;

        /* [b1 stream_id_extension_flag] */
        packetData[offset] = (param.extParam.streamIdExtensionFlag) ? 0x80 : 0x0;

        if (!param.extParam.streamIdExtensionFlag) {
          /* [u7 stream_id_extension] */
          packetData[offset++] |= param.extParam.streamIdExtension & 0x7F;
        }
        else {
          /* [v6 reserved] [b1 tref_extension_flag] */
          packetData[offset++] |= param.extParam.trefExtensionFlag | 0x7E;

          if (param.extParam.trefExtensionFlag) {
            /* [v4 reserved] [u3 TREF[32..30]] [v1 marker_bit] */
            packetData[offset++] = (((param.extParam.tref >> 30) & 0x7) << 1) + 0xF1;

            /* [u15 TREF[29..15]] [v1 marker_bit] */
            packetData[offset++] =   (param.extParam.tref >> 22) & 0xFF;
            packetData[offset++] = (((param.extParam.tref >> 15) & 0x7F) << 1) + 0x01;

            /* [u15 TREF[14..0]] [v1 marker_bit] */
            packetData[offset++] =   (param.extParam.tref >>  7) & 0xFF;
            packetData[offset++] =  ((param.extParam.tref        & 0x7F) << 1) | 0x01;
          }
        }

        for (; offset < pesExtensionFieldOff + param.extParam.pesExtensionFieldLength; offset++) {
          /* [v8 reserved] */
          packetData[offset] = 0xFF;
        }
      }
    }

    for (; offset < pesHeaderDataOff + param.pesHeaderDataLen; offset++) {
      /* [v8 stuffing_byte] */
      packetData[offset] = 0xFF;
    }
  }
  else if ((param.packetStartCode & 0xFF) == 0xBE) { /* padding_stream */
    for (i = 0; i < param.pesPacketLength; i++) {
      /* [v8 padding_byte] */
      packetData[offset++] = 0xFF;
    }
  }

  return offset;
}