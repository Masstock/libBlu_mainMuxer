#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include "pesPackets.h"

size_t writePesHeader(
  uint8_t * packetData,
  size_t offset,
  const PesPacketHeaderParam * param
)
{
  // uint64_t pts, dts;
  // uint64_t escrBase;
  // uint8_t escrExt, packetStartCodePrefix;
  // size_t pesHeaderDataOff, pesExtensionFieldOff, i;

  /* [u24 packet_start_code_prefix] [u8 stream_id] */
  WB_ARRAY(packetData, offset, param->packetStartCode >> 24);
  WB_ARRAY(packetData, offset, param->packetStartCode >> 16);
  WB_ARRAY(packetData, offset, param->packetStartCode >>  8);
  WB_ARRAY(packetData, offset, param->packetStartCode);

  /* [u16 PES_packet_length] */
  if (param->pesPacketLength < 0xFFFF) {
    WB_ARRAY(packetData, offset, param->pesPacketLength >> 8);
    WB_ARRAY(packetData, offset, param->pesPacketLength);
  }
  else {
    WB_ARRAY(packetData, offset, 0x00);
    WB_ARRAY(packetData, offset, 0x00);
  }

  uint8_t stream_id = param->packetStartCode & 0xFF;
  if (
    stream_id != 0xBC     /* program_stream_map               */
    && stream_id != 0xBE  /* padding_stream                   */
    && stream_id != 0xBF  /* private_stream_2                 */
    && stream_id != 0xF0  /* ECM                              */
    && stream_id != 0xF1  /* EMM                              */
    && stream_id != 0xF2  /* DSMCC_stream                     */
    && stream_id != 0xF8  /* ITU-T Rec. H.222.1 type E stream */
    && stream_id != 0xFF  /* program_stream_directory         */
  ) {

    /**
     * -> v2: '10'
     * -> u2: PES_scrambling_control
     * -> b1: PES_priority
     * -> b1: data_alignment_indicator
     * -> b1: copyright
     * -> b1: original_or_copy
     */
    uint8_t byte = (
      ((param->pesScramblingControl & 0x3) << 4)
      | (param->pesPriority                  << 3)
      | (param->dataAlignmentIndicator       << 2)
      | (param->copyright                    << 1)
      | param->originalOrCopy
      | 0x80
    );
    WB_ARRAY(packetData, offset, byte);

    /**
     * -> u2: PTS_DTS_flags
     * -> b1: ESCR_flag
     * -> b1: ES_rate_flag
     * -> b1: DSM_trick_mode_flag
     * -> b1: additional_copy_info_flag
     * -> b1: PES_CRC_flag
     * -> b1: PES_extension_flag
     */
    byte = (
      (param->ptsFlag                        << 7)
      | (param->dtsFlag                      << 6)
      | (param->escrFlag                     << 5)
      | (param->esRateFlag                   << 4)
      | (param->dsmTrickModeFlag             << 3)
      | (param->additionalCopyInfoFlag       << 2)
      | (param->pesCrcFlag                   << 1)
      | param->pesExtFlag
    );
    WB_ARRAY(packetData, offset, byte);

    /* [u8 PES_header_data_length] */
    size_t PES_header_data_length_offset = offset;
    WB_ARRAY(packetData, offset, param->pesHeaderDataLen);

    if (param->ptsFlag) {
      uint64_t pts = (param->pts / 300) & 0x1FFFFFFFF;

      /* [v4 '0010'/'0011'] [u3 PTS[32..30]] [v1 marker_bit] */
      WB_ARRAY(packetData, offset, (param->dtsFlag << 4) | (pts >> 29) | 0x21);

      /* [u15 PTS[29..15]] [v1 marker_bit] */
      WB_ARRAY(packetData, offset, (pts >> 22));
      WB_ARRAY(packetData, offset, (pts >> 14) | 0x1);

      /* [u15 PTS[14..0]] [v1 marker_bit] */
      WB_ARRAY(packetData, offset, (pts >> 7));
      WB_ARRAY(packetData, offset, (pts << 1) | 0x1);

      if (param->dtsFlag) {
        uint64_t dts = (param->dts / 300) & 0x1FFFFFFFF;

        /* [v4 '0001'] [u3 DTS[32..30]] [v1 marker_bit] */
        WB_ARRAY(packetData, offset, (dts >> 29) | 0x11);

        /* [u15 DTS[29..15]] [v1 marker_bit] */
        WB_ARRAY(packetData, offset, (dts >> 22));
        WB_ARRAY(packetData, offset, (dts >> 14) | 1);

        /* [u15 DTS[14..0]] [v1 marker_bit] */
        WB_ARRAY(packetData, offset, (dts >> 7));
        WB_ARRAY(packetData, offset, (dts << 1) | 0x1);
      }
    }

    if (param->escrFlag) {
      uint32_t ESCR_base = (param->escr / 300) & 0x1FFFFFFFF;
      uint32_t ESCR_ext  = (param->escr % 300) & 0x1FF;

      /**
       * -> v2: reserved
       * -> u3: ESCR_base[32..30]
       * -> v1: marker_bit
       * -> u2: ESCR_base[29..28]
       */
      byte = (
        ((ESCR_base   >> 30) & 0x38)
        | ((ESCR_base >> 28) & 0x03)
        | 0xC4
      );
      WB_ARRAY(packetData, offset, byte);

      /* [u8 ESCR_base[27..20]] */
      WB_ARRAY(packetData, offset, ESCR_base >> 20);

      /**
       * -> u5: ESCR_base[19..15]
       * -> v1: marker_bit
       * -> u2: ESCR_base[14..13]
       */
      byte = (
        ((ESCR_base   >> 11) & 0xF8)
        | ((ESCR_base >> 12) & 0x3)
      );
      WB_ARRAY(packetData, offset, byte);

      /* [u8 ESCR_base[12..5]] */
      WB_ARRAY(packetData, offset, ESCR_base >> 4);

      /**
       * -> u5: ESCR_base[4..0]
       * -> v1: marker_bit
       * -> u2: ESCR_extension[9..8]
       */
      WB_ARRAY(packetData, offset, (ESCR_base << 3) | (ESCR_ext >> 7) | 0x4);

      /* [u7 ESCR_extension[7..0]] [v1 marker_bit] */
      WB_ARRAY(packetData, offset, (ESCR_ext << 1) | 0x1);
    }

    if (param->esRateFlag) {
      /* [v1 marker_bit] [u22 ES_rate] [v1 marker_bit] */
      WB_ARRAY(packetData, offset, (param->esRate >> 15) | 0x80);
      WB_ARRAY(packetData, offset, (param->esRate >>  7));
      WB_ARRAY(packetData, offset, (param->esRate <<  1) | 0x1);
    }

    if (param->dsmTrickModeFlag) {
      /* [u3 trick_mode_control] */
      byte = param->dsmTrickModeParam.trickModeControl << 5;

      if (
        param->dsmTrickModeParam.trickModeControl == TRICKMODE_FAST_FORWARD
        || param->dsmTrickModeParam.trickModeControl == TRICKMODE_FAST_REVERSE
      ) {
        /* [u2 field_id] [b1 intra_slice_refresh] [u2 frequency_truncation] */
        byte |= (
          ((param->dsmTrickModeParam.fieldId << 3) & 0x18)
          | (param->dsmTrickModeParam.intraSlideRefresh << 2)
          | (param->dsmTrickModeParam.frequencyTruncation & 0x3)
        );
      }
      else if (
        param->dsmTrickModeParam.trickModeControl == TRICKMODE_SLOW_MOTION
        || param->dsmTrickModeParam.trickModeControl == TRICKMODE_SLOW_REVERSE
      ) {
        /* [u5 rep_cntrl] */
        byte |= param->dsmTrickModeParam.repCntrl & 0x1F;
      }
      else if (param->dsmTrickModeParam.trickModeControl == TRICKMODE_FREEZE_FRAME) {
        /* [u2 field_id] [v3 reserved] */
        byte |= ((param->dsmTrickModeParam.fieldId & 0x3) << 3) | 0x7;
      }
      else {
        /* [v5 reserved] */
        byte |= 0x1F;
      }

      WB_ARRAY(packetData, offset, byte);
    }

    if (param->additionalCopyInfoFlag) {
      /* [v1 reserved] [u7 additional_copy_info] */
      WB_ARRAY(packetData, offset, param->additionalCopyInfo | 0x80);
    }

    if (param->pesCrcFlag) {
      /* [u16 previous_PES_packet_CRC] */
      WB_ARRAY(packetData, offset, param->previousPesPacketCrc >> 8);
      WB_ARRAY(packetData, offset, param->previousPesPacketCrc);
    }

    if (param->pesExtFlag) {
      /**
       * -> b1: PES_private_data_flag
       * -> b1: pack_header_field_flag
       * -> b1: program_packet_sequence_counter_flag
       * -> b1: P-STD_buffer_flag
       * -> v3: reserved 0b000
       * -> b1: PES_extension_flag_2
       */
      byte = (
        (param->extParam.pesPrivateDataFlag << 7)
        | (param->extParam.packHeaderFieldFlag << 6)
        | (param->extParam.programPacketSequenceCounterFlag << 5)
        | (param->extParam.pStdBufferFlag << 4)
        | param->extParam.pesExtensionFlag2
      );
      WB_ARRAY(packetData, offset, byte);

      if (param->extParam.pesPrivateDataFlag) {
        /* [u128 PES_private_data] */
        memcpy(&packetData[offset], param->extParam.pesPrivateData, 16);
        offset += 16;
      }

      if (param->extParam.packHeaderFieldFlag) {
        /* [u8 pack_field_length] */
        WB_ARRAY(packetData, offset, param->extParam.packFieldLength);
        /* [vn pack_field_length] */
        memcpy(&packetData[offset], param->extParam.packHeader, param->extParam.packFieldLength);
        offset += param->extParam.packFieldLength;
      }

      if (param->extParam.programPacketSequenceCounterFlag) {
        /* [v1 marker_bit] [u7 program_packet_sequence_counter] */
        WB_ARRAY(packetData, offset, param->extParam.programPacketSequenceCounter | 0x80);

        /* [v1 marker_bit] [b1 MPEG1_H262_identifier] [u6 original_stuff_length] */
        byte = (
          (param->extParam.mpeg1H262Identifier << 6)
          | (param->extParam.originalStuffLength & 0x3F)
          | 0x80
        );
        WB_ARRAY(packetData, offset, byte);
      }

      if (param->extParam.pStdBufferFlag) {
        /* [v2 '01'] [b1 P-STD_buffer_scale] [u13 P-STD_buffer_size] */
        byte = (
          (param->extParam.pStdBufferScale << 5)
          | ((param->extParam.pStdBufferSize >> 8) & 0x1F)
          | 0x40
        );
        WB_ARRAY(packetData, offset, byte);
        WB_ARRAY(packetData, offset, param->extParam.pStdBufferSize);
      }

      if (param->extParam.pesExtensionFlag2) {
        /* [v1 marker_bit] [u7 PES_extension_field_length] */
        unsigned PES_extension_field_length_offset = offset;
        WB_ARRAY(packetData, offset, param->extParam.pesExtensionFieldLength | 0x80);

        /* [b1 stream_id_extension_flag] */
        byte = param->extParam.streamIdExtensionFlag << 7;

        if (!param->extParam.streamIdExtensionFlag) {
          /* [u7 stream_id_extension] */
          byte |= param->extParam.streamIdExtension & 0x7F;
        }
        else {
          /* [v6 reserved] [b1 tref_extension_flag] */
          WB_ARRAY(packetData, offset, byte | param->extParam.trefExtensionFlag | 0x7E);

          if (param->extParam.trefExtensionFlag) {
            /* [v4 reserved] [u3 TREF[32..30]] [v1 marker_bit] */
            WB_ARRAY(packetData, offset, (param->extParam.tref >> 29) | 0xF1);

            /* [u15 TREF[29..15]] [v1 marker_bit] */
            WB_ARRAY(packetData, offset, (param->extParam.tref >> 22));
            WB_ARRAY(packetData, offset, (param->extParam.tref >> 14) | 0x01);

            /* [u15 TREF[14..0]] [v1 marker_bit] */
            WB_ARRAY(packetData, offset, (param->extParam.tref >>  7));
            byte = ((param->extParam.tref & 0x7F) << 1) | 0x01;
          }
        }

        WB_ARRAY(packetData, offset, byte);

        unsigned ext_data_size = offset - PES_extension_field_length_offset;
        for (unsigned i = ext_data_size; i < param->extParam.pesExtensionFieldLength; i++) {
          /* [v8 reserved] */
          packetData[offset] = 0xFF;
        }
      }
    }

    unsigned header_data_size = offset - PES_header_data_length_offset;
    for (unsigned i = header_data_size; i < param->pesHeaderDataLen; i++) {
      /* [v8 stuffing_byte] */
      packetData[offset++] = 0xFF;
    }
  }
  else if ((param->packetStartCode & 0xFF) == 0xBE) {
    /* padding_stream */
    for (unsigned i = 0; i < param->pesPacketLength; i++) {
      /* [v8 padding_byte] */
      packetData[offset++] = 0xFF;
    }
  }

  return offset;
}