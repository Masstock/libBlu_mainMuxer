#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include "muxingSettings.h"

int initLibbluMuxingSettings(
  LibbluMuxingSettings * dst,
  const lbc * outputTsFilename,
  IniFileContextPtr confHandle
)
{
  assert(NULL != dst);

  if (NULL == outputTsFilename)
    outputTsFilename = LIBBLU_DEFAULT_OUTPUT_TS_FILENAME;

  if (NULL == (dst->outputTsFilename = lbc_strdup(outputTsFilename)))
    LIBBLU_ERROR_RETURN("Memory allocation error.\n");

  dst->nbInputStreams = 0;

  dst->targetMuxingRate = LIBBLU_DEFAULT_MUXING_RATE;
  dst->initialPresentationTime = LIBBLU_DEFAULT_INIT_PRES_TIME;
  dst->initialTStdBufDuration = LIBBLU_DEFAULT_INIT_TSTD_DUR;

  setHdmvDefaultUnencryptedLibbluDtcpSettings(&dst->dtcpParameters);

  defaultLibbluMuxingOptions(&dst->options, confHandle);

  return 0;
}