#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "muxingSettings.h"

static int _readOptionsFromConfigHandle(
  LibbluMuxingOptions *dst,
  const IniFileContext conf_hdl
)
{
  lbc *string;

  string = lookupIniFile(conf_hdl, "BUFFERINGMODEL.CRASHONERROR");
  if (NULL != string) {
    bool value;

    if (lbc_atob(&value, string) < 0)
      LIBBLU_ERROR_RETURN(
        "Invalid boolean value setting 'crashOnError' in section "
        "[BufferingModel] of INI file.\n"
      );
    dst->buffering_model_options.abort_on_underflow = value;
  }

  string = lookupIniFile(conf_hdl, "BUFFERINGMODEL.TIMEOUT");
  if (NULL != string) {
    uint64_t value;

    if (!lbc_sscanf(string, lbc_str("%" PRIu64), &value))
      LIBBLU_ERROR_RETURN(
        "Invalid integer value setting 'timeout' in section "
        "[BufferingModel] of INI file.\n"
      );
    dst->buffering_model_options.underflow_warn_timeout = value;
  }

  return 0;
}

int initLibbluMuxingOptions(
  LibbluMuxingOptions *dst,
  IniFileContext conf_hdl
)
{
  defaultLibbluMuxingOptions(dst, conf_hdl);
  if (_readOptionsFromConfigHandle(dst, conf_hdl) < 0)
    return -1;
  return 0;
}

int initLibbluMuxingSettings(
  LibbluMuxingSettings *dst,
  const lbc *output_ts_filename,
  const LibbluMuxingOptions mux_options
)
{
  assert(NULL != dst);

  if (NULL == output_ts_filename)
    output_ts_filename = lbc_str(LIBBLU_DEFAULT_OUTPUT_TS_FILENAME);

  lbc *output_ts_filename_copy = lbc_strdup(output_ts_filename);
  if (NULL == output_ts_filename_copy)
    LIBBLU_ERROR_RETURN("Memory allocation error.\n");

  *dst = (LibbluMuxingSettings) {
    .output_ts_filename = output_ts_filename_copy,

    .TS_recording_rate       = MUX_DEFAULT_TS_RECORDING_RATE,
    .presentation_start_time = MUX_DEFAULT_PRES_START_TIME,
    .initial_STD_STC_delay   = MUX_DEFAULT_INITIAL_STD_STC_DELAY,

    .options = mux_options
  };

  setHdmvDefaultUnencryptedLibbluDtcpSettings(&dst->DTCP_parameters);

  return 0;
}
