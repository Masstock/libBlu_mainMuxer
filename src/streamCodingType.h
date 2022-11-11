/** \~english
 * \file streamCodingType.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief MPEG-TS Stream Coding Type values handling module.
 */

#ifndef __LIBBLU_MUXER__STREAM_CODING_TYPE_H__
#define __LIBBLU_MUXER__STREAM_CODING_TYPE_H__

#include "util.h"

/** \~english
 * \brief Stream coding types.
 *
 * Codec identifiers.
 */
typedef enum {
  STREAM_CODING_TYPE_NULL      =    -1,
  STREAM_CODING_TYPE_ANY       =  0xFF,

  STREAM_CODING_TYPE_MPEG1     =  0x01,  /**< MPEG-1 video.                  */
  STREAM_CODING_TYPE_H262      =  0x02,  /**< MPEG-2 video.                  */
  STREAM_CODING_TYPE_AVC       =  0x1B,  /**< H.264/AVC video.               */
  STREAM_CODING_TYPE_MVC       =  0x20,  /**< H.264/MVC video.               */
  STREAM_CODING_TYPE_HEVC      =  0x24,  /**< H.265/HEVC video.              */
  STREAM_CODING_TYPE_VC1       =  0xEA,  /**< SMPTE VC-1 video.              */

  STREAM_CODING_TYPE_LPCM      =  0x80,  /**< LPCM audio.                    */
  STREAM_CODING_TYPE_AC3       =  0x81,  /**< Dolby Digital audio.           */
  STREAM_CODING_TYPE_DTS       =  0x82,  /**< DTS Coherent Acoustics audio.  */
  STREAM_CODING_TYPE_TRUEHD    =  0x83,  /**< Dolby TrueHD audio.            */
  STREAM_CODING_TYPE_EAC3      =  0x84,  /**< Dolby Digital Plus audio.      */
  STREAM_CODING_TYPE_HDHR      =  0x85,  /**< DTS-HDHR audio.                */
  STREAM_CODING_TYPE_HDMA      =  0x86,  /**< DTS-HDMA audio.                */

  STREAM_CODING_TYPE_PG        =  0x90,  /**< Presentation Graphics.         */
  STREAM_CODING_TYPE_IG        =  0x91,  /**< Interactive Graphics.          */
  STREAM_CODING_TYPE_TEXT      =  0x92,  /**< Text subtitles.                */

  STREAM_CODING_TYPE_EAC3_SEC  =  0xA1,  /**< Dolby Digital Plus secondary
    audio.                                                                   */
  STREAM_CODING_TYPE_DTSE_SEC  =  0xA2   /**< DTS-Express secondary audio.   */
} LibbluStreamCodingType;

const lbc * streamCodingTypeStr(
  LibbluStreamCodingType sct
);

static inline int isSupportedStreamCodingType(
  LibbluStreamCodingType typ
)
{
  return
    typ == STREAM_CODING_TYPE_MPEG1
    || typ == STREAM_CODING_TYPE_H262
    || typ == STREAM_CODING_TYPE_AVC

    || typ == STREAM_CODING_TYPE_LPCM
    || typ == STREAM_CODING_TYPE_AC3
    || typ == STREAM_CODING_TYPE_DTS
    || typ == STREAM_CODING_TYPE_TRUEHD
    || typ == STREAM_CODING_TYPE_EAC3
    || typ == STREAM_CODING_TYPE_HDHR
    || typ == STREAM_CODING_TYPE_HDMA

    || typ == STREAM_CODING_TYPE_PG
    || typ == STREAM_CODING_TYPE_IG
    || typ == STREAM_CODING_TYPE_TEXT

    || typ == STREAM_CODING_TYPE_EAC3_SEC
    || typ == STREAM_CODING_TYPE_DTSE_SEC
  ;
}

static inline int isVideoStreamCodingType(
  LibbluStreamCodingType typ
)
{
  return
    typ == STREAM_CODING_TYPE_MPEG1
    || typ == STREAM_CODING_TYPE_H262
    || typ == STREAM_CODING_TYPE_AVC
    || typ == STREAM_CODING_TYPE_MVC
    || typ == STREAM_CODING_TYPE_HEVC
    || typ == STREAM_CODING_TYPE_VC1
  ;
}

static inline int isAudioStreamCodingType(
  LibbluStreamCodingType typ
)
{
  return
    typ == STREAM_CODING_TYPE_LPCM
    || typ == STREAM_CODING_TYPE_AC3
    || typ == STREAM_CODING_TYPE_DTS
    || typ == STREAM_CODING_TYPE_TRUEHD
    || typ == STREAM_CODING_TYPE_EAC3
    || typ == STREAM_CODING_TYPE_HDHR
    || typ == STREAM_CODING_TYPE_HDMA
    || typ == STREAM_CODING_TYPE_EAC3_SEC
    || typ == STREAM_CODING_TYPE_DTSE_SEC
  ;
}

static inline int isAc3AudioStreamCodingType(
  LibbluStreamCodingType typ
)
{
  return
    typ == STREAM_CODING_TYPE_AC3
    || typ == STREAM_CODING_TYPE_TRUEHD
    || typ == STREAM_CODING_TYPE_EAC3
    || typ == STREAM_CODING_TYPE_EAC3_SEC
  ;
}

static inline int isSecondaryCompatibleStreamCodingType(
  LibbluStreamCodingType typ
)
{
  return
    typ == STREAM_CODING_TYPE_MPEG1
    || typ == STREAM_CODING_TYPE_H262
    || typ == STREAM_CODING_TYPE_AVC
    || typ == STREAM_CODING_TYPE_VC1
    || typ == STREAM_CODING_TYPE_EAC3
    || typ == STREAM_CODING_TYPE_EAC3_SEC
    || typ == STREAM_CODING_TYPE_DTSE_SEC
  ;
}

typedef enum {
  ES_VIDEO,
  ES_AUDIO,
  ES_HDMV
} LibbluESType;

static inline const char * libbluESTypeStr(
  LibbluESType type
)
{
  static const char * types[] = {
    "video",
    "audio",
    "HDMV"
  };

  if (0 <= type && type < ARRAY_SIZE(types))
    return types[type];
  return "unknown";
}

static inline LibbluESType determineLibbluESType(
  LibbluStreamCodingType codingType
)
{
  switch (codingType) {
    /* Video :                                               */
    case STREAM_CODING_TYPE_MPEG1:    /* MPEG-1 (deprecated) */
    case STREAM_CODING_TYPE_H262:    /* MPEG-2              */
    case STREAM_CODING_TYPE_AVC:      /* H264-AVC            */
    case STREAM_CODING_TYPE_VC1:      /* VC-1                */
    case STREAM_CODING_TYPE_MVC:      /* H264-MVC Dependency */
    case STREAM_CODING_TYPE_HEVC:     /* H265-HEVC           */
      return ES_VIDEO;

    /* Audio :                                               */
    case STREAM_CODING_TYPE_LPCM:     /* LPCM                */
    case STREAM_CODING_TYPE_AC3:      /* AC3 (Dolby Digital) */
    case STREAM_CODING_TYPE_DTS:      /* DTS                 */
    case STREAM_CODING_TYPE_TRUEHD:   /* Dolby-TrueHD        */
    case STREAM_CODING_TYPE_EAC3:     /* E-AC3 (DD Plus)     */
    case STREAM_CODING_TYPE_HDHR:     /* DTS-HDHR            */
    case STREAM_CODING_TYPE_HDMA:     /* DTS-HDMA            */
    /* Secondary Audio :                                     */
    case STREAM_CODING_TYPE_EAC3_SEC: /* E-AC3 (DD Plus)     */
    case STREAM_CODING_TYPE_DTSE_SEC: /* DTS-Express         */
      return ES_AUDIO;

    /* HDMV Streams :                                        */
    case STREAM_CODING_TYPE_PG:       /* Presentation Graph. */
    case STREAM_CODING_TYPE_IG:       /* Interactive Graph.  */
    case STREAM_CODING_TYPE_TEXT:     /* Text-Subtitles      */
      return ES_HDMV;

    default:
      break;
  }

  return -1; /* Error, unknow id                     */
}

#endif