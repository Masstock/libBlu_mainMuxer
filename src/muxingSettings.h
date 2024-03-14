/** \~english
 * \file muxingSettings.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Muxing settings module.
 */

#ifndef __LIBBLU_MUXER__MUXING_SETTINGS_H__
#define __LIBBLU_MUXER__MUXING_SETTINGS_H__

#include "util.h"
#include "elementaryStream.h"
#include "dtcpSettings.h"
#include "ini/iniHandler.h"

typedef struct {
  bool force_script_generation;
  bool CBR_mux;
  bool write_TP_extra_headers;
  bool PCR_on_ES_packets;
  bool disable_buffering_model;  /**< Disable T-STD Buffer model use. */

  LibbluESSettingsOptions es_shared_options;
  BufModelOptions buffering_model_options;

  uint16_t PCR_PID;  /**< PCR carriage ES PID value.                         */
} LibbluMuxingOptions;

static inline void defaultLibbluMuxingOptions(
  LibbluMuxingOptions *dst,
  IniFileContext conf_hdl
)
{
  *dst = (LibbluMuxingOptions) {
    .write_TP_extra_headers = true,
    .es_shared_options = {
      .conf_hdl = conf_hdl
    },
  };
}

int initLibbluMuxingOptions(
  LibbluMuxingOptions *dst,
  IniFileContext conf_hdl
);

typedef struct {
  lbc *output_ts_filename;

  LibbluESSettings *es_settings;
  unsigned nb_allocated_es;
  unsigned nb_used_es;

  uint64_t TS_recording_rate;
  uint64_t presentation_start_time;
  float initial_STD_STC_delay;       /**< Initial System Target Decoder
    System Time Clock delay in seconds. This value must be greater than 0 and
    lesser than 1 second. */

  LibbluDtcpSettings DTCP_parameters;

  LibbluMuxingOptions options;
} LibbluMuxingSettings;

#define LIBBLU_DEFAULT_OUTPUT_TS_FILENAME  "out.m2ts"

#define MUX_DEFAULT_INITIAL_STD_STC_DELAY  0.9

/** \~english
 * \brief Initiate #LibbluMuxingSettings structure.
 *
 * \param dst
 * \param output_ts_filename
 * \param conf_hdl
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int initLibbluMuxingSettings(
  LibbluMuxingSettings *dst,
  const lbc *output_ts_filename,
  const LibbluMuxingOptions mux_options
);

/** \~english
 * \brief Release memory allocation used by #LibbluMuxingSettings.
 *
 * \param settings Object to clean.
 */
static inline void cleanLibbluMuxingSettings(
  LibbluMuxingSettings settings
)
{
  free(settings.output_ts_filename);
  for (unsigned i = 0; i < settings.nb_used_es; i++)
    cleanLibbluESSettings(settings.es_settings[i]);
  free(settings.es_settings);
}

/** \~english
 * \brief Declare a new Elementary Stream in supplied muxing settings and
 * return a handle to set its settings.
 *
 * \param settings Destination settings.
 * \return LibbluESSettings* Upon success, a pointer to the new ES initialized
 * settings is returned. Otherwise, if the limit of ES as been reached, a NULL
 * pointer is returned.
 */
static inline LibbluESSettings *getNewESLibbluMuxingSettings(
  LibbluMuxingSettings *settings
)
{
  assert(NULL != settings);

  if (settings->nb_allocated_es <= settings->nb_used_es) {
    unsigned new_size = GROW_ALLOCATION(settings->nb_allocated_es, 8);
    if (!new_size)
      LIBBLU_ERROR_NRETURN("ES settings overflow.\n");

    LibbluESSettings *new_array = realloc(
      settings->es_settings,
      new_size *sizeof(LibbluESSettings)
    );

    settings->es_settings     = new_array;
    settings->nb_allocated_es = new_size;
  }

  LibbluESSettings *es = &settings->es_settings[
    settings->nb_used_es++
  ];

  return initLibbluESSettings(es);
}

#define MUX_MIN_TS_RECORDING_RATE  500000

#define MUX_MAX_TS_RECORDING_RATE  120000000

#define MUX_DEFAULT_TS_RECORDING_RATE  48000000

/** \~english
 * \brief Set the muxing target multiplex recording rate value.
 *
 * \param dst Destination muxing settings structure.
 * \param value Multiplex bit rate in bits per second (bps).
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned, indicating supplied value exceed parameter limits.
 */
static inline int setTargetMuxRateLibbluMuxingSettings(
  LibbluMuxingSettings *dst,
  uint64_t TS_recording_rate
)
{
  if (TS_recording_rate < MUX_MIN_TS_RECORDING_RATE)
    return -1; // Below min
  if (MUX_MAX_TS_RECORDING_RATE < TS_recording_rate)
    return -1; // Above max

  dst->TS_recording_rate = TS_recording_rate;
  return 0;
}

#define MUX_MIN_PRES_START_TIME  SUB_CLOCK_90KHZ

#define MUX_MAX_PRES_START_TIME  ((uint64_t) SUB_CLOCK_90KHZ * 60 * 60 * 5000)

#define MUX_DEFAULT_PRES_START_TIME  ((uint64_t) 54000000 * 300)

/** \~english
 * \brief Set the muxing initial presentation time value.
 *
 * \param dst Destination muxing settings structure.
 * \param value Initial presentation time value in #SUB_CLOCK_90KHZ ticks.
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned, indicating supplied value exceed parameter limits.
 */
static inline int setInitPresTimeLibbluMuxingSettings(
  LibbluMuxingSettings *dst,
  uint64_t value
)
{
  if (value < MUX_MIN_PRES_START_TIME)
    return -1;
  if (MUX_MAX_PRES_START_TIME < value)
    return -1;

  dst->presentation_start_time = value * 300;
  return 0;
}

static inline int setFpsChangeLibbluMuxingSettings(
  LibbluMuxingSettings *dst,
  const lbc *expr
)
{
  HdmvFrameRateCode idc;
  if (parseFpsChange(expr, &idc) < 0)
    return -1;

  dst->options.es_shared_options.fps_mod = idc;
  return 0;
}

static inline int setArChangeLibbluMuxingSettings(
  LibbluMuxingSettings *dst,
  const lbc *expr
)
{
  LibbluAspectRatioMod aspect_ratio;
  if (parseArChange(expr, &aspect_ratio) < 0)
    return -1;

  dst->options.es_shared_options.ar_mod = aspect_ratio;
  return 0;
}

static inline int setLevelChangeLibbluMuxingSettings(
  LibbluMuxingSettings *dst,
  const lbc *expr
)
{
  uint8_t level;
  if (parseLevelChange(expr, &level) < 0)
    return -1;

  dst->options.es_shared_options.level_mod = level;
  return 0;
}

static inline int setHdmvInitialTimestampLibbluMuxingSettings(
  LibbluMuxingSettings *dst,
  uint64_t timestamp
)
{
#if 0 < LIBBLU_MIN_HDMV_INIT_TIMESTAMP
  if (timestamp < LIBBLU_MIN_HDMV_INIT_TIMESTAMP)
    return -1;
#endif
  if (LIBBLU_MAX_HDMV_INIT_TIMESTAMP < timestamp)
    return -1;

  dst->options.es_shared_options.hdmv.initial_timestamp = timestamp;
  return 0;
}

#endif