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
#include "ini/iniData.h"

#define LIBBLU_MAX_NB_STREAMS  32

typedef struct {
  bool forcedScriptBuilding;
  bool cbrMuxing;
  bool writeTPExtraHeaders;
  bool pcrOnESPackets;
  bool disableTStdBufVerifier;

  LibbluESSettingsOptions globalSharedOptions;

  uint16_t pcrPID;          /**< PCR carriage ES PID value.                  */
} LibbluMuxingOptions;

static inline void defaultLibbluMuxingOptions(
  LibbluMuxingOptions * dst,
  IniFileContextPtr confHandle
)
{
  *dst = (LibbluMuxingOptions) {
    .writeTPExtraHeaders = true,
    .disableTStdBufVerifier = DISABLE_T_STD_BUFFER_VER,
    .globalSharedOptions = {
      .confHandle = confHandle
    }
  };
}

#define LIBBLU_DEFAULT_OUTPUT_TS_FILENAME  lbc_str("out.m2ts")

#define LIBBLU_MIN_MUXING_RATE  500000
#define LIBBLU_MAX_MUXING_RATE  120000000
#define LIBBLU_DEFAULT_MUXING_RATE  48000000

#define LIBBLU_MIN_INIT_PRES_TIME  SUB_CLOCK_90KHZ
#define LIBBLU_MAX_INIT_PRES_TIME  ((uint64_t) SUB_CLOCK_90KHZ * 60 * 60 * 5000)
#define LIBBLU_DEFAULT_INIT_PRES_TIME  ((uint64_t) 54000000 * 300)

#define LIBBLU_DEFAULT_INIT_TSTD_DUR  0.9

typedef struct {
  lbc * outputTsFilename;

  LibbluESSettings inputStreams[LIBBLU_MAX_NB_STREAMS];
  unsigned nbInputStreams;

  uint64_t targetMuxingRate;
  uint64_t initialPresentationTime;
  float initialTStdBufDuration;

  LibbluDtcpSettings dtcpParameters;

  /* Options : */
  LibbluMuxingOptions options;
} LibbluMuxingSettings;

#define LIBBLU_MUX_SETTINGS_SET_OPTION(set, opname, val)                      \
  ((set)->options.opname = (val))

#define LIBBLU_MUX_SETTINGS_OPTION(set, opname)                               \
  ((set)->options.opname)

#define LIBBLU_MUX_SETTINGS_SET_GLB_OPTION(set, opname, val)                  \
  ((set)->options.globalSharedOptions.opname = (val))

#define LIBBLU_MUX_SETTINGS_GLB_OPTION(set, opname)                           \
  ((set)->options.globalSharedOptions.opname)

/** \~english
 * \brief Initiate #LibbluMuxingSettings structure.
 *
 * \param dst
 * \param outputTsFilename
 * \param confHandle
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int initLibbluMuxingSettings(
  LibbluMuxingSettings * dst,
  const lbc * outputTsFilename,
  IniFileContextPtr confHandle
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
  unsigned i;

  free(settings.outputTsFilename);

  for (i = 0; i < settings.nbInputStreams; i++)
    cleanLibbluESSettings(settings.inputStreams[i]);
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
static inline LibbluESSettings * getNewESLibbluMuxingSettings(
  LibbluMuxingSettings * settings
)
{
  LibbluESSettings * es;

  assert(NULL != settings);

  if (LIBBLU_MAX_NB_STREAMS <= settings->nbInputStreams)
    return NULL; /* No more, too much ES used */

  es = settings->inputStreams + settings->nbInputStreams++;
  return initLibbluESSettings(es);
}

/** \~english
 * \brief Set the muxing target multiplex rate value.
 *
 * \param dst Destination muxing settings structure.
 * \param value Multiplex bit rate in bits per second (bps).
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned, indicating supplied value exceed parameter limits.
 */
static inline int setTargetMuxRateLibbluMuxingSettings(
  LibbluMuxingSettings * dst,
  uint64_t value
)
{
  if (value < LIBBLU_MIN_MUXING_RATE)
    return -1;
  if (LIBBLU_MAX_MUXING_RATE < value)
    return -1;

  dst->targetMuxingRate = value;
  return 0;
}

/** \~english
 * \brief Set the muxing initial presentation time value.
 *
 * \param dst Destination muxing settings structure.
 * \param value Initial presentation time value in #SUB_CLOCK_90KHZ ticks.
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned, indicating supplied value exceed parameter limits.
 */
static inline int setInitPresTimeLibbluMuxingSettings(
  LibbluMuxingSettings * dst,
  uint64_t value
)
{
  if (value < LIBBLU_MIN_INIT_PRES_TIME)
    return -1;
  if (LIBBLU_MAX_INIT_PRES_TIME < value)
    return -1;

  dst->initialPresentationTime = value * 300;
  return 0;
}

static inline int setFpsChangeLibbluMuxingSettings(
  LibbluMuxingSettings * dst,
  const lbc * expr
)
{
  HdmvFrameRateCode idc;

  if (parseFpsChange(expr, &idc) < 0)
    return -1;

  LIBBLU_MUX_SETTINGS_SET_GLB_OPTION(dst, fpsChange, idc);
  return 0;
}

static inline int setArChangeLibbluMuxingSettings(
  LibbluMuxingSettings * dst,
  const lbc * expr
)
{
  LibbluAspectRatioMod values;

  if (parseArChange(expr, &values) < 0)
    return -1;

  LIBBLU_MUX_SETTINGS_SET_GLB_OPTION(dst, arChange, values);
  return 0;
}

static inline int setLevelChangeLibbluMuxingSettings(
  LibbluMuxingSettings * dst,
  const lbc * expr
)
{
  uint8_t idc;

  if (parseLevelChange(expr, &idc) < 0)
    return -1;

  LIBBLU_MUX_SETTINGS_SET_GLB_OPTION(dst, levelChange, idc);
  return 0;
}

#endif