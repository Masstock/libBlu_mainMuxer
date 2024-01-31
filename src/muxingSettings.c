#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "muxingSettings.h"

static int _readSettingsFromConfigHandle(
  LibbluMuxingSettings * dst,
  const IniFileContext conf_hdl
)
{
  lbc * string;

  string = lookupIniFile(conf_hdl, "BUFFERINGMODEL.CRASHONERROR");
  if (NULL != string) {
    bool value;

    if (lbc_atob(&value, string) < 0)
      LIBBLU_ERROR_RETURN(
        "Invalid boolean value setting 'crashOnError' in section "
        "[BufferingModel] of INI file.\n"
      );
    dst->options.bufModelOptions.abortOnUnderflow = value;
  }

  string = lookupIniFile(conf_hdl, "BUFFERINGMODEL.TIMEOUT");
  if (NULL != string) {
    uint64_t value;

    if (!lbc_sscanf(string, "%" PRIu64, &value))
      LIBBLU_ERROR_RETURN(
        "Invalid integer value setting 'timeout' in section "
        "[BufferingModel] of INI file.\n"
      );
    dst->options.bufModelOptions.underflowWarnTimeout = value;
  }

  return 0;
}

int initLibbluMuxingSettings(
  LibbluMuxingSettings * dst,
  const lbc * outputTsFilename,
  IniFileContext conf_hdl
)
{
  assert(NULL != dst);

  *dst = (LibbluMuxingSettings) {
    .targetMuxingRate = LIBBLU_DEFAULT_MUXING_RATE,
    .initialPresentationTime = LIBBLU_DEFAULT_INIT_PRES_TIME,
    .initialTStdBufDuration = LIBBLU_DEFAULT_INIT_TSTD_DUR,
  };

  setHdmvDefaultUnencryptedLibbluDtcpSettings(&dst->dtcpParameters);
  defaultLibbluMuxingOptions(&dst->options, conf_hdl);
  if (_readSettingsFromConfigHandle(dst, conf_hdl) < 0)
    return -1;

  if (NULL == outputTsFilename)
    outputTsFilename = LIBBLU_DEFAULT_OUTPUT_TS_FILENAME;

  if (NULL == (dst->outputTsFilename = lbc_strdup(outputTsFilename)))
    LIBBLU_ERROR_RETURN("Memory allocation error.\n");

  return 0;
}