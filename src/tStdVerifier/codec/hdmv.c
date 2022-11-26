#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include "hdmv.h"

int createHdmvBufferingChainBdavStd(
  BufModelNode * root,
  LibbluESPtr stream,
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
    .instantFilling = false,
    .dontOverflowOutput = false,
    .bufferSize = BDAV_STD_HDMV_TBS * 8
  };
  bParam = (BufModelBufferParameters) {
    .name = MAIN_BUFFER,
    .instantFilling = true,
    .dontOverflowOutput = false,
    .bufferSize = BDAV_STD_HDMV_CDBS * 8
  };

  /* Create TB */
  ret = createLeakingBufferNode(
    buffersList, &tb,
    tbParam,
    initialTimestamp,
    BDAV_STD_HDMV_RX
  );
  if (ret < 0)
    return -1;

  /* Create CDB (Coded Data Buffer) */
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