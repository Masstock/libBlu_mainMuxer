

#ifndef __LIBBLU_MUXER__CODECS__HDMV__CHECK_H__
#define __LIBBLU_MUXER__CODECS__HDMV__CHECK_H__

#include "hdmv_common.h"

int checkHdmvSequence(
  HdmvSequencePtr sequence
);

int checkHdmvDisplaySet(
  HdmvDisplaySet * ds,
  HdmvStreamType type
);

int checkDuplicatedHdmvDisplaySet(
  HdmvDisplaySet * ds,
  unsigned lastDSIdx
);

#endif