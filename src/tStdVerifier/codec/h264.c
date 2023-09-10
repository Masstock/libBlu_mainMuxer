#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include "h264.h"

/* If NULL == StreamInfos.codecSpecInfos.h264Param, not available */

int createH264BufferingChainBdavStd(
  BufModelNode * root,
  LibbluES * stream,
  uint64_t initialTimestamp,
  BufModelBuffersListPtr buffersList
)
{
  int ret;

  BufModelNode tb, mb, eb;
  BufModelBufferParameters tbParam, mbParam, ebParam;

  size_t maxCpb, maxBr, CpbSize, BitRate;
  size_t mbs, bsmux, bsoh;

  if (!(maxCpb = getH264MaxCPB(stream->prop.level_IDC)))
    LIBBLU_ERROR_RETURN(
      "Unable to define input stream buffering parameters, "
      "unknown level_idc %u for getH264MaxCPB().\n",
      stream->prop.level_IDC
    );
  if (!(maxBr = getH264MaxBR(stream->prop.level_IDC)))
    LIBBLU_ERROR_RETURN(
      "Unable to define input stream buffering parameters, "
      "unknown level_idc %u for getH264MaxCPB().\n",
      stream->prop.level_IDC
    );

  if (NULL == stream->fmt_prop.h264) {
    CpbSize = 1200 * maxCpb;
    BitRate = 1.2 * stream->prop.bitrate;
  }
  else {
    CpbSize = stream->fmt_prop.h264->CpbSize;
    BitRate = 1.2 * stream->fmt_prop.h264->BitRate;
  }

  /* Clipping values to BDAV constraints */
  CpbSize = MIN(CpbSize, 30000000);
  BitRate = MIN(BitRate, 48000000);

  tbParam = (BufModelBufferParameters) {
    .name = TRANSPORT_BUFFER,
    .instantFilling = false,
    .dontOverflowOutput = false,
    .bufferSize = BDAV_STD_H264_TBS * 8
  };

  bsmux = 0.004 * MAX(1200 * maxBr, 2000000);
  bsoh = (1.0 / 750.0) * MAX(1200 * maxBr, 2000000);
  mbs = MAX(bsmux + bsoh + 1200 * maxCpb, CpbSize) - CpbSize;

  mbParam = (BufModelBufferParameters) {
    .name = MULTIPLEX_BUFFER,
    .instantFilling = true,
    .dontOverflowOutput = true,
    .bufferSize = mbs
  };

  ebParam = (BufModelBufferParameters) {
    .name = ELEMENTARY_BUFFER,
    .instantFilling = true,
    .dontOverflowOutput = false,
    .bufferSize = CpbSize
  };

  /* Create TB */
  ret = createLeakingBufferNode(
    buffersList, &tb,
    tbParam,
    initialTimestamp,
    BitRate /* Rx */
  );
  if (ret < 0)
    return -1;

  /* Create MB */
  ret = createLeakingBufferNode(
    buffersList, &mb,
    mbParam,
    initialTimestamp,
    1200 * maxBr
  );
  if (ret < 0)
    return -1;

  /* Create EB */
  ret = createRemovalBufferNode(
    buffersList, &eb,
    ebParam,
    initialTimestamp
  );
  if (ret < 0)
    return -1;

  if (setBufferOutput(tb, mb) < 0)
    LIBBLU_ERROR_RETURN("Broken construction of system buffering chain.\n");
  if (setBufferOutput(mb, eb) < 0)
    LIBBLU_ERROR_RETURN("Broken construction of system buffering chain.\n");

  *root = tb;
  return 0;
}