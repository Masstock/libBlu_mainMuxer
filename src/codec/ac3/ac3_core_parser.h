

#ifndef __LIBBLU_MUXER__CODECS__AC3__CORE_PARSER_H__
#define __LIBBLU_MUXER__CODECS__AC3__CORE_PARSER_H__

#include "ac3_error.h"
#include "ac3_data.h"


int parseAc3SyncInfo(
  BitstreamReaderPtr bs,
  Ac3SyncInfoParameters * syncinfo
);

int parseAc3Addbsi(
  LibbluBitReaderPtr br,
  Ac3Addbsi * addbsi
);

int parseAc3BitStreamInfo(
  LibbluBitReaderPtr br,
  Ac3BitStreamInfoParameters * bsi
);

#endif