#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include "streamCodingType.h"

const lbc * LibbluStreamCodingTypeStr(
  LibbluStreamCodingType sct
)
{
  switch (sct) {
    case STREAM_CODING_TYPE_MPEG1:
      return lbc_str("MPEG-1 Video");
    case STREAM_CODING_TYPE_H262:
      return lbc_str("MPEG-2 Video");
    case STREAM_CODING_TYPE_AVC:
      return lbc_str("H.264/AVC");
    case STREAM_CODING_TYPE_MVC:
      return lbc_str("H.264/MVC");
    case STREAM_CODING_TYPE_HEVC:
      return lbc_str("H.265/HEVC");
    case STREAM_CODING_TYPE_VC1:
      return lbc_str("SMPTE 421 VC-1");
    case STREAM_CODING_TYPE_LPCM:
      return lbc_str("Linear PCM audio");
    case STREAM_CODING_TYPE_AC3:
      return lbc_str("Dolby AC-3");
    case STREAM_CODING_TYPE_DTS:
      return lbc_str("DTS Coherent Acoustics");
    case STREAM_CODING_TYPE_TRUEHD:
      return lbc_str("Dolby TrueHD");
    case STREAM_CODING_TYPE_EAC3:
      return lbc_str("Dolby AC-3+");
    case STREAM_CODING_TYPE_HDHR:
      return lbc_str("DTS-HD High Resolution");
    case STREAM_CODING_TYPE_HDMA:
      return lbc_str("DTS-HD Master Audio");
    case STREAM_CODING_TYPE_PG:
      return lbc_str("HDMV Presentation Graphics");
    case STREAM_CODING_TYPE_IG:
      return lbc_str("HDMV Interactive Graphics");
    case STREAM_CODING_TYPE_TEXT:
      return lbc_str("HDMV Text Subtitles");
    case STREAM_CODING_TYPE_EAC3_SEC:
      return lbc_str("Dolby AC-3+ as Secondary Track");
    case STREAM_CODING_TYPE_DTSE_SEC:
      return lbc_str("DTS Express as Secondary Track");

    default:
      break;
  }

  return lbc_str("Unknown");
}