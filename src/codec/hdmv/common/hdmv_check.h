

#ifndef __LIBBLU_MUXER__CODECS__HDMV__CHECK_H__
#define __LIBBLU_MUXER__CODECS__HDMV__CHECK_H__

#include "hdmv_common.h"

int checkAndBuildDisplaySetHdmvEpochState(
  HdmvParsingOptions options,
  HdmvStreamType type,
  HdmvEpochState * epoch,
  unsigned dsIdx
);

int checkObjectsBufferingHdmvEpochState(
  HdmvEpochState * epoch,
  HdmvStreamType type
);

int checkDuplicatedDSHdmvEpochState(
  HdmvEpochState * ds,
  unsigned lastDSIdx
);

#endif