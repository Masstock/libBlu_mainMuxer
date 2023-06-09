#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include "lpcm.h"

int createLpcmBufferingChainBdavStd(
  BufModelNode * root,
  LibbluESPtr stream,
  uint64_t initialTimestamp,
  BufModelBuffersListPtr buffersList
)
{
  BufModelNode tb, b;
  BufModelBufferParameters tbParam, bParam;

  tbParam = (BufModelBufferParameters) {
    .name = TRANSPORT_BUFFER,
    .instantFilling = false,
    .dontOverflowOutput = false,
    .bufferSize = BDAV_STD_LPCM_TBS * 8
  };
  bParam = (BufModelBufferParameters) {
    .name = MAIN_BUFFER,
    .instantFilling = true,
    .dontOverflowOutput = false,
    .bufferSize = (
      (stream->prop.sample_rate != SAMPLE_RATE_CODE_192000) ?
        BDAV_STD_LPCM_BS_48_96
      :
        BDAV_STD_LPCM_BS_192
    ) * 8
  };

  /* Create TB */
  if (
    createLeakingBufferNode(
      buffersList, &tb,
      tbParam,
      initialTimestamp,
      (
        (stream->prop.sample_rate != SAMPLE_RATE_CODE_192000) ?
          BDAV_STD_LPCM_RX_192
        :
          BDAV_STD_LPCM_RX_48_96
      )
    ) < 0
  )
    return -1;

  /* Create B */
  if (
    createRemovalBufferNode(
      buffersList, &b,
      bParam,
      initialTimestamp
    ) < 0
  )
    return -1;

  if (setBufferOutput(tb, b) < 0)
    LIBBLU_ERROR_RETURN("Broken construction of system buffering chain.\n");

  *root = tb;
  return 0;
}
