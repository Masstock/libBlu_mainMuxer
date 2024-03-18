#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include "elementaryStreamPesProperties.h"

/* ### ES Pes Packet Properties : ########################################## */

int prepareLibbluESPesPacketProperties(
  LibbluESPesPacketProperties *dst,
  EsmsPesPacket *esms_pes,
  uint64_t ref_STC_timestamp,
  uint64_t PTS_reference
)
{
  assert(NULL != dst);
  assert(NULL != esms_pes);

  uint64_t zero = 300ull *PTS_reference;

  dst->extension_frame = false;
  if (ES_AUDIO == esms_pes->prop.type)
    dst->extension_frame = esms_pes->prop.prefix.audio.extension_frame;

  dst->dts_present = esms_pes->prop.dts_present;
  dst->extension_data_pres = esms_pes->prop.ext_data_present;

  dst->pts = (300ull * esms_pes->pts) + ref_STC_timestamp;
  if (dst->pts < zero)
    LIBBLU_ERROR_RETURN("Negative Presentation Time Stamp on a PES header.\n");
  dst->pts -= zero;

  dst->dts = (300ull * esms_pes->dts) + ref_STC_timestamp;
  if (dst->dts < zero)
    LIBBLU_ERROR_RETURN("Negative Decoding Time Stamp on a PES header.\n");
  dst->dts -= zero;

  dst->extension_data = esms_pes->ext_data;
  dst->payload_size = esms_pes->size;

  return 0;
}

int preparePesHeaderCommon(
  PesPacketHeaderParam *dst,
  LibbluESPesPacketProperties prop,
  LibbluStreamCodingType codingType
)
{
  dst->packetStartCode = PES_PACKET_START_CODE_PREFIX;

  switch (codingType) {
  case STREAM_CODING_TYPE_H262: /* MPEG-2 */
  case STREAM_CODING_TYPE_AVC: /* H.264/AVC */
    dst->packetStartCode |= 0xE0; break;
  case STREAM_CODING_TYPE_LPCM: /* LPCM */
  case STREAM_CODING_TYPE_IG: /* IGS */
  case STREAM_CODING_TYPE_PG: /* PGS */
    /* private-stream-1 */
    dst->packetStartCode |= 0xBD; break;
  case STREAM_CODING_TYPE_AC3: /* AC-3 */
  case STREAM_CODING_TYPE_DTS: /* DTS Coherent Acoustics */
  case STREAM_CODING_TYPE_TRUEHD: /* Dolby TrueHD */
  case STREAM_CODING_TYPE_EAC3: /* E-AC-3 */
  case STREAM_CODING_TYPE_HDHR: /* DTS-HDHR */
  case STREAM_CODING_TYPE_HDMA: /* DTS-HDMA */
  case STREAM_CODING_TYPE_DTSE_SEC: /* DTS-Express Secondary */
    /* extended_stream_id */
    dst->packetStartCode |= 0xFD; break;
  default: /* Unsupported streamCodingType */
    LIBBLU_ERROR_RETURN(
      "Unable to prepare PES packet header, unknown parameters for stream "
      "coding type 0x%02X (%s).\n",
      (unsigned) codingType,
      LibbluStreamCodingTypeStr(codingType)
    );
  }

  if (!IS_SHORT_PES_HEADER(dst->packetStartCode & 0xFF)) {
#if 0
    (dst->packetStartCode & 0xFF) != 0xBC && /* program_stream_map               */
    (dst->packetStartCode & 0xFF) != 0xBE && /* padding_stream                   */
    (dst->packetStartCode & 0xFF) != 0xBF && /* private_stream_2                 */
    (dst->packetStartCode & 0xFF) != 0xF0 && /* ECM                              */
    (dst->packetStartCode & 0xFF) != 0xF1 && /* EMM                              */
    (dst->packetStartCode & 0xFF) != 0xF2 && /* DSMCC_stream                     */
    (dst->packetStartCode & 0xFF) != 0xF8 && /* ITU-T Rec. H.222.1 type E stream */
    (dst->packetStartCode & 0xFF) != 0xFF    /* program_stream_directory         */
#endif

    dst->pesScramblingControl = 0x00; /* Not Scrambled */
    dst->pesPriority = 0x0;
    dst->dataAlignmentIndicator = 0x1;
    dst->copyright = 0x0;
    dst->originalOrCopy = 0x0;

    dst->ptsFlag = 0x1;
    if (dst->ptsFlag) {
      switch (codingType) {
      case STREAM_CODING_TYPE_H262: /* MPEG-2 */
      case STREAM_CODING_TYPE_AVC: /* H.264/AVC */
      case STREAM_CODING_TYPE_IG: /* IGS */
      case STREAM_CODING_TYPE_PG: /* IGS */
        dst->dtsFlag = prop.dts_present;
        break;
      default:
        dst->dtsFlag = 0x0;
      }
    }

    dst->escrFlag = 0x0;
    dst->esRateFlag = 0x0;
    dst->dsmTrickModeFlag = 0x0;
    dst->additionalCopyInfoFlag = 0x0;
    dst->pesCrcFlag = 0x0;

    switch (codingType) {
    case STREAM_CODING_TYPE_AC3: /* AC-3 (Dolby Digital) */
    case STREAM_CODING_TYPE_DTS: /* DTS */
    case STREAM_CODING_TYPE_TRUEHD: /* MLP (Dolby TrueHD) */
    case STREAM_CODING_TYPE_EAC3: /* E-AC-3 (Dolby Digital Plus) */
    case STREAM_CODING_TYPE_HDHR: /* DTS-HDHR (High Resolution) */
    case STREAM_CODING_TYPE_HDMA: /* DTS-HDMA (Master Audio) */
    case STREAM_CODING_TYPE_EAC3_SEC: /* E-AC-3 (Dolby Digital Plus) (Secondary Audio) */
    case STREAM_CODING_TYPE_DTSE_SEC: /* DTS-HD Express (Secondary Audio) */
    case STREAM_CODING_TYPE_VC1: /* VC-1 */
      dst->pesExtFlag = 0x1;
      break;
    default:
      dst->pesExtFlag = 0x0;
    }

    dst->pesHeaderDataLen = 0x00; /* Updated in the continuation of the function. */

    if (dst->ptsFlag) {
      dst->pts = prop.pts;
      dst->pesHeaderDataLen += 5;

      if (dst->dtsFlag) {
        dst->dts = prop.dts;
        dst->pesHeaderDataLen += 5;
      }
    }

    if (dst->escrFlag) {
      dst->pesHeaderDataLen += 6;
      /* Not currently used */
      dst->escr = 0;
      LIBBLU_ERROR_RETURN(
        "Missing functionnality from PES header: ESCR flag.\n"
      );
    }

    if (dst->esRateFlag) {
      dst->pesHeaderDataLen += 3;
      dst->esRate = 0; /* Not currently used */
      LIBBLU_ERROR_RETURN(
        "Missing functionnality from PES header: ES rate.\n"
      );
    }

    if (dst->dsmTrickModeFlag) {
      dst->pesHeaderDataLen += 1;
      /* Not currently used */
      LIBBLU_ERROR_RETURN(
        "Missing functionnality from PES header: DSM Trick Mode flag.\n"
      );
    }

    if (dst->additionalCopyInfoFlag) {
      dst->pesHeaderDataLen += 1;
      /* Not currently used */
      LIBBLU_ERROR_RETURN(
        "Additional Copy Info flag.\n"
      );
    }

    if (dst->pesCrcFlag) {
      dst->pesHeaderDataLen += 2;
      /* Not currently used */
      LIBBLU_ERROR_RETURN(
        "Missing functionnality from PES header: PES CRC flag.\n"
      );
    }

    if (dst->pesExtFlag) {
      dst->extParam.pesPrivateDataFlag = 0x0;
      dst->extParam.packHeaderFieldFlag = 0x0;
      dst->extParam.programPacketSequenceCounterFlag = 0x0;
      dst->extParam.pStdBufferFlag = 0x0;
      dst->extParam.pesExtensionFlag2 = 0x1;
      dst->pesHeaderDataLen++;

      if (dst->extParam.pesPrivateDataFlag) {
        /* Not currently used */
        LIBBLU_ERROR_RETURN(
          "Missing functionnality from PES header: PES Private Data flag.\n"
        );
      }

      if (dst->extParam.packHeaderFieldFlag) {
        /* Not currently used */
        LIBBLU_ERROR_RETURN(
          "Missing functionnality from PES header: Pack Header Field flag.\n"
        );
      }

      if (dst->extParam.programPacketSequenceCounterFlag) {
        /* Not currently used */
        LIBBLU_ERROR_RETURN(
          "Missing functionnality from PES header: "
          "Program Packet Sequence Counter flag.\n"
        );
      }

      if (dst->extParam.pStdBufferFlag) {
        /* Not currently used */
        LIBBLU_ERROR_RETURN(
          "Missing functionnality from PES header: P-STD Buffer flag.\n"
        );
      }

      if (dst->extParam.pesExtensionFlag2) {
        dst->extParam.pesExtensionFieldLength = 0x1;
        dst->extParam.streamIdExtensionFlag = 0x0;
        dst->pesHeaderDataLen += 1 + dst->extParam.pesExtensionFieldLength;

        if (0x0 == dst->extParam.streamIdExtensionFlag) {
          switch (codingType) {
          case STREAM_CODING_TYPE_AC3: /* AC-3 (Dolby Digital) */
          case STREAM_CODING_TYPE_DTS: /* DTS */
            dst->extParam.streamIdExtension = 0x71; break;
          case STREAM_CODING_TYPE_TRUEHD: /* MLP (Dolby TrueHD) */
            if (prop.extension_frame)
              dst->extParam.streamIdExtension = 0x72;
            else
              dst->extParam.streamIdExtension = 0x76;
            break;
          case STREAM_CODING_TYPE_EAC3: /* E-AC-3 (Dolby Digital Plus) */
          case STREAM_CODING_TYPE_HDHR: /* DTS-HDHR (High Resolution) */
          case STREAM_CODING_TYPE_HDMA: /* DTS-HDMA (Master Audio) */
            if (prop.extension_frame)
              dst->extParam.streamIdExtension = 0x72;
            else
              dst->extParam.streamIdExtension = 0x71;
            break;
          case STREAM_CODING_TYPE_EAC3_SEC: /* E-AC-3 (Dolby Digital Plus) (Secondary Audio) */
          case STREAM_CODING_TYPE_DTSE_SEC: /* DTS-HD Express (Secondary Audio) */
            dst->extParam.streamIdExtension = 0x72; break;
          case STREAM_CODING_TYPE_VC1: /* VC-1 */
            /* Values from 0x55 to 0x5F are reserved for VC-1 */
            dst->extParam.streamIdExtension = 0x55; break;
          default:
            LIBBLU_ERROR_RETURN(
              "Unable to prepare PES packet header, "
              "unknown PES Extension parameters for stream "
              "coding type 0x%02X (%s).\n",
              (unsigned) codingType,
              LibbluStreamCodingTypeStr(codingType)
            );
          }
        }
        else /* Not currently used */
          LIBBLU_ERROR_RETURN(
            "Missing functionnality from PES header: "
            "Stream Extension ID flag.\n"
          );
      }
    }
  }
  else if ((dst->packetStartCode & 0xFF) == 0xBE) { /* padding_stream */
    dst->pesHeaderDataLen = 0;

    /* Not currently used */
    LIBBLU_ERROR_RETURN(
      "Missing functionnality from PES header: "
      "padding_stream.\n"
    );
  }
  else
    dst->pesHeaderDataLen = 0;

  return 0;
}

/* ### ES Pes Packet Data : ################################################ */

int allocateLibbluESPesPacketData(
  LibbluESPesPacketData *dst,
  uint32_t size
)
{
  bool perfm_alloc = (dst->alloc_size < size);

#if USE_OPTIMIZED_REALLOCATION
  perfm_alloc |= (size + DIFF_TRIGGER_REALLOC_OPTI < dst->alloc_size);
#endif

  if (perfm_alloc) {
    /* Look-up next power of 2 */
    uint32_t new_size = lb_ceil_pow2_32(1u + size);
    if (!new_size)
      LIBBLU_ERROR_RETURN("PES packet allocation size overflow.\n");

    uint8_t *new_data = realloc(dst->data, new_size);
    if (NULL == new_data)
      LIBBLU_ERROR_RETURN("Memory allocation error.\n");

    dst->data = new_data;
    dst->alloc_size = new_size;
  }

  return 0;
}