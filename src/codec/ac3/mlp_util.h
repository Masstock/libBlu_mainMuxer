

#ifndef __LIBBLU_MUXER__CODECS__AC3__MLP_UTIL_H__
#define __LIBBLU_MUXER__CODECS__AC3__MLP_UTIL_H__

#include "ac3_error.h"
#include "ac3_data.h"

#define MLP_MAX_NB_SS  4

typedef struct {
  MlpSyncHeaderParameters mlp_sync_header;
  MlpSubstreamDirectoryEntry substream_directory[MLP_MAX_NB_SS];
  MlpSubstreamParameters substreams[MLP_MAX_NB_SS];
  MlpInformations info;

  unsigned nb_frames;
  uint64_t pts;
} MlpParsingContext;

#endif