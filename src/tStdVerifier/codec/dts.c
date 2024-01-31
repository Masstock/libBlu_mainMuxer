#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include "dts.h"

int createDtsBufferingChainBdavStd(
  BufModelNode * root,
  LibbluES * stream,
  uint64_t initialTimestamp,
  BufModelBuffersListPtr buffersList
)
{
  int ret;

  size_t rx, bs;
  BufModelNode tb, b;
  BufModelBufferParameters tbParam, bParam;

  switch (stream->prop.coding_type) {
  case STREAM_CODING_TYPE_DTS:
    rx = BDAV_STD_DTS_RX_DCA;
    bs = BDAV_STD_DTS_BS_DCA;
    break;

  case STREAM_CODING_TYPE_HDMA:
    rx = BDAV_STD_DTS_RX_DTS_HDMA;
    bs = BDAV_STD_DTS_BS_HDMA;
    break;

  case STREAM_CODING_TYPE_HDHR:
    rx = BDAV_STD_DTS_RX_DTS_HDHR;
    bs = BDAV_STD_DTS_BS_HDHR;
    break;

  default:
    /* Unmanaged DTS stream coding type */
    *root = newVoidBufModelNode();
    return 0;
  }

  tbParam = (BufModelBufferParameters) {
    .name = TRANSPORT_BUFFER,
    .instantFilling = false,
    .dontOverflowOutput = false,
    .bufferSize = BDAV_STD_DTS_TBS * 8
  };
  bParam = (BufModelBufferParameters) {
    .name = MAIN_BUFFER,
    .instantFilling = true,
    .dontOverflowOutput = false,
    .bufferSize = bs * 8
  };

  /* Create TB */
  ret = createLeakingBufferNode(
    buffersList, &tb,
    tbParam,
    initialTimestamp,
    rx
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

  if (setBufferOutput(tb, b) < 0)
    LIBBLU_ERROR_RETURN("Broken construction of system buffering chain.\n");

  *root = tb;
  return 0;
}