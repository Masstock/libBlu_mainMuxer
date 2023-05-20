

#ifndef __LIBBLU_MUXER__CODECS__AC3__MLP_PARSER_H__
#define __LIBBLU_MUXER__CODECS__AC3__MLP_PARSER_H__

#include "ac3_data.h"
#include "ac3_error.h"

int parseMlpSyncHeader(
  BitstreamReaderPtr bs,
  LibbluBitReaderPtr br,
  uint8_t ** buffer,
  size_t * buffer_size,
  MlpSyncHeaderParameters * sh
);

int parseMlpSubstreamDirectoryEntry(
  LibbluBitReaderPtr br,
  MlpSubstreamDirectoryEntry * entry
);

int decodeMlpSubstreamSegment(
  LibbluBitReaderPtr br,
  MlpSubstreamParameters * substream,
  const MlpSubstreamDirectoryEntry * entry,
  unsigned ss_idx
);

int decodeMlpExtraData(
  LibbluBitReaderPtr br
);

#endif