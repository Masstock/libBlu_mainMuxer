#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include "systemStreams.h"

int createSystemBufferingChainBdavStd(
  BufModelBuffersListPtr bufList,
  BufModelNode * root,
  uint64_t initialTimestamp,
  uint64_t transportRate
)
{
  BufModelNode tb, b;
  BufModelBufferParameters tbParam, bParam;

  assert(NULL != root);

  tbParam = (BufModelBufferParameters) {
    .name = TRANSPORT_BUFFER,
    .instantFilling = false,
    .dontOverflowOutput = false,
    .bufferSize = BDAV_STD_SYSTEM_TBS * 8
  };
  bParam = (BufModelBufferParameters) {
    .name = MAIN_BUFFER,
    .instantFilling = true,
    .dontOverflowOutput = false,
    .bufferSize = BDAV_STD_SYSTEM_BS * 8
  };

  /* Create TB */
  if (
    createLeakingBufferNode(
      bufList, &tb,
      tbParam,
      initialTimestamp,
      BDAV_STD_SYSTEM_RX
    ) < 0
  )
    return -1;

  /* Create B */
  if (
    createLeakingBufferNode(
      bufList, &b,
      bParam,
      initialTimestamp,
      MAX(BDAV_STD_SYSTEM_R_MIN, transportRate / 500)
    ) < 0
  )
    return -1;

  if (setBufferOutput(tb, b) < 0)
    LIBBLU_ERROR_RETURN("Broken construction of system buffering chain.\n");

  *root = tb;
  return 0;
}