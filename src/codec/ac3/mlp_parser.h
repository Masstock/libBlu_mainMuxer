

#ifndef __LIBBLU_MUXER__CODECS__AC3__MLP_PARSER_H__
#define __LIBBLU_MUXER__CODECS__AC3__MLP_PARSER_H__

#include "ac3_data.h"
#include "ac3_error.h"
#include "mlp_check.h"
#include "mlp_util.h"

int parseMlpMinorSyncHeader(
  BitstreamReaderPtr bs,
  MlpSyncHeaderParameters * sh
);

int parseMlpMajorSyncInfo(
  LibbluBitReader * br,
  MlpMajorSyncInfoParameters * param
);

int parseMlpSyncHeader(
  BitstreamReaderPtr bs,
  LibbluBitReader * br,
  uint8_t ** buffer,
  size_t * buffer_size,
  MlpSyncHeaderParameters * sh
);

int decodeMlpSubstreamDirectory(
  LibbluBitReader * br,
  MlpParsingContext * ctx
);

int decodeMlpSubstreamSegments(
  LibbluBitReader * br,
  MlpParsingContext * ctx
);

int decodeMlpExtraData(
  LibbluBitReader * br
);

#endif