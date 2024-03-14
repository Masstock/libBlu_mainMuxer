/** \~english
 * \file esParsingSettings.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Parsing settings structure module.
 */

#ifndef __LIBBLU_MUXER__CODECS__COMMON__ES_PARSING_SETTINGS_H__
#define __LIBBLU_MUXER__CODECS__COMMON__ES_PARSING_SETTINGS_H__

#include "../../elementaryStreamOptions.h"
#include "../../elementaryStreamPesProperties.h"
#include "../../util.h"

typedef struct {
  const lbc *esFilepath;
  const lbc *scriptFilepath;

  /* uint64_t flags; */
  bool askForRestart;

  LibbluESSettingsOptions options;
} LibbluESParsingSettings;

static inline void initLibbluESParsingSettings(
  LibbluESParsingSettings *dst,
  const lbc *esFilepath,
  const lbc *scriptFilepath,
  LibbluESSettingsOptions options
)
{
  /* uint64_t flags = computeFlagsLibbluESSettingsOptions(options); */

  *dst = (LibbluESParsingSettings) {
    .esFilepath = esFilepath,
    .scriptFilepath = scriptFilepath,
    .options = options
  };
}

static inline bool doRestartLibbluESParsingSettings(
  LibbluESParsingSettings *settings
)
{
  bool doRestart;

  doRestart = settings->askForRestart;
  settings->askForRestart = false;
  return doRestart;
}

#endif