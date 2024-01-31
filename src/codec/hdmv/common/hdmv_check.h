

#ifndef __LIBBLU_MUXER__CODECS__HDMV__CHECK_H__
#define __LIBBLU_MUXER__CODECS__HDMV__CHECK_H__

#include "hdmv_common.h"

int checkAndBuildDisplaySetHdmvDSState(
  HdmvParsingOptions options,
  HdmvStreamType type,
  HdmvDSState * epoch,
  unsigned dsIdx
);

int checkObjectsBufferingHdmvDSState(
  HdmvDSState * epoch,
  HdmvStreamType type
);

int checkDuplicatedDSHdmvDSState(
  HdmvDSState * ds,
  unsigned lastDSIdx
);

#endif