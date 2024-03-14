

#ifndef __LIBBLU_MUXER__CODECS__HDMV__CHECK_H__
#define __LIBBLU_MUXER__CODECS__HDMV__CHECK_H__

#include "hdmv_common.h"
#include "hdmv_rectangle.h"
#include "hdmv_collision_tree.h"
#include "hdmv_seq_indexer.h"

int checkAndUpdateHdmvDSState(
  HdmvDSState *ds,
  HdmvEpochState *epoch_state,
  int64_t pres_time
);

int checkObjectsBufferingHdmvEpochState(
  HdmvEpochState *epoch_state
);

int checkDuplicatedDSHdmvDSState(
  const HdmvDSState *prev_ds_state,
  HdmvDSState *cur_ds_state,
  HdmvEpochState *epoch_state
);

#endif