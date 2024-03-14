#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include "streamCodingType.h"

const char * LibbluStreamCodingTypeStr(
  LibbluStreamCodingType sct
)
{
  switch (sct) {
  case STREAM_CODING_TYPE_MPEG1:
    return "MPEG-1 Video";
  case STREAM_CODING_TYPE_H262:
    return "MPEG-2 Video";
  case STREAM_CODING_TYPE_AVC:
    return "H.264/AVC";
  case STREAM_CODING_TYPE_MVC:
    return "H.264/MVC";
  case STREAM_CODING_TYPE_HEVC:
    return "H.265/HEVC";
  case STREAM_CODING_TYPE_VC1:
    return "SMPTE 421 VC-1";
  case STREAM_CODING_TYPE_LPCM:
    return "Linear PCM audio";
  case STREAM_CODING_TYPE_AC3:
    return "Dolby AC-3";
  case STREAM_CODING_TYPE_DTS:
    return "DTS Coherent Acoustics";
  case STREAM_CODING_TYPE_TRUEHD:
    return "Dolby TrueHD";
  case STREAM_CODING_TYPE_EAC3:
    return "Dolby AC-3+";
  case STREAM_CODING_TYPE_HDHR:
    return "DTS-HD High Resolution";
  case STREAM_CODING_TYPE_HDMA:
    return "DTS-HD Master Audio";
  case STREAM_CODING_TYPE_PG:
    return "HDMV Presentation Graphics";
  case STREAM_CODING_TYPE_IG:
    return "HDMV Interactive Graphics";
  case STREAM_CODING_TYPE_TEXT:
    return "HDMV Text Subtitles";
  case STREAM_CODING_TYPE_EAC3_SEC:
    return "Dolby AC-3+ as Secondary Track";
  case STREAM_CODING_TYPE_DTSE_SEC:
    return "DTS Express as Secondary Track";

  default:
    break;
  }

  return "Unknown";
}