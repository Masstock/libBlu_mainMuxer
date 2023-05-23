

#ifndef __LIBBLU_MUXER__CODECS__AC3__UTIL_H__
#define __LIBBLU_MUXER__CODECS__AC3__UTIL_H__

#include "../../util.h"
#include "../../esms/scriptCreation.h"
#include "../common/esParsingSettings.h"

#include "ac3_error.h"
#include "ac3_data.h"
#include "mlp_util.h"

typedef struct {
  Ac3SyncInfoParameters syncinfo;
  Ac3BitStreamInfoParameters bsi;

  unsigned nb_frames;
  uint64_t pts;
} Ac3CoreContext;

typedef struct {
  unsigned src_file_idx;
  BitstreamReaderPtr bs;
  BitstreamWriterPtr script_bs;
  EsmsFileHeaderPtr script;
  const lbc * script_fp;

  Ac3CoreContext core;

  MlpParsingContext mlp;

  bool skip_ext;
  // bool isSecondaryStream;
  // bool skipExtensionSubstreams;
  // unsigned skippedFramesControl;
  // bool skipCurrentPeriod;
} Ac3Context;

int initAc3Context(
  Ac3Context * ctx,
  LibbluESParsingSettings * settings
);

int completeAc3Context(
  Ac3Context * ctx
);

void cleanAc3Context(
  Ac3Context * ctx
);

#endif