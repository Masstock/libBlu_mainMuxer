

#ifndef __LIBBLU_MUXER__CODECS__AC3__EAC3_PARSER_H__
#define __LIBBLU_MUXER__CODECS__AC3__EAC3_PARSER_H__

#include "ac3_data.h"
#include "ac3_error.h"
#include "eac3_util.h"

#include "ac3_core_parser.h"


int parseEac3SyncInfo(
  LibbluBitReaderPtr br
);

int parseEac3BitStreamInfo(
  LibbluBitReaderPtr br,
  Eac3BitStreamInfoParameters * bsi
);

#endif