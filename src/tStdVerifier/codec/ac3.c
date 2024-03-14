#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include "ac3.h"

int createAc3BufferingChainBdavStd(
  BufModelNode *root,
  LibbluES *stream,
  uint64_t initialTimestamp,
  BufModelBuffersListPtr buffersList
)
{
  int ret;

  BufModelNode tb, b;
  BufModelBufferParameters tbParam, bParam;

  (void) stream;

  tbParam = (BufModelBufferParameters) {
    .name = TRANSPORT_BUFFER,
    .instant_filling = false,
    .dont_overflow_output = false,
    .buffer_size = BDAV_STD_AC3_TBS * 8
  };
  bParam = (BufModelBufferParameters) {
    .name = MAIN_BUFFER,
    .instant_filling = true,
    .dont_overflow_output = false,
    .buffer_size = BDAV_STD_AC3_BS * 8
  };

  /* Create TB */
  ret = createLeakingBufferNode(
    buffersList, &tb,
    tbParam,
    initialTimestamp,
    BDAV_STD_AC3_RX
  );
  if (ret < 0)
    return -1;

  /* Create B */
  ret = createRemovalBufferNode(
    buffersList, &b,
    bParam,
    initialTimestamp
  );
  if (ret < 0)
    return -1;

  ret = setBufferOutput(tb, b);
  if (ret < 0)
    LIBBLU_ERROR_RETURN("Broken construction of system buffering chain.\n");

  *root = tb;
  return 0;
}