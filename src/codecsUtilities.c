#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include "codecsUtilities.h"

LibbluStreamCodingType guessESStreamCodingType(
  const lbc * filepath
)
{
  /* TODO */
  (void) filepath;

  return -1;
}

int initLibbluESFormatUtilities(
  LibbluESFormatUtilities * dst,
  LibbluStreamCodingType codingType
)
{
  assert(NULL != dst);

  LibbluAnalyze_fun analyze_fun;
  switch (codingType) {
    case STREAM_CODING_TYPE_MPEG1:
    case STREAM_CODING_TYPE_H262:
      analyze_fun = analyzeH262;
      break;

    case STREAM_CODING_TYPE_AVC:
      analyze_fun = analyzeH264;
      break;

    case STREAM_CODING_TYPE_LPCM:
      analyze_fun = analyzeLpcm;
      break;

    case STREAM_CODING_TYPE_AC3:
    case STREAM_CODING_TYPE_TRUEHD:
    case STREAM_CODING_TYPE_EAC3:
    case STREAM_CODING_TYPE_EAC3_SEC:
      analyze_fun = analyzeAc3;
      break;

    case STREAM_CODING_TYPE_DTS:
    case STREAM_CODING_TYPE_HDHR:
    case STREAM_CODING_TYPE_HDMA:
    case STREAM_CODING_TYPE_DTSE_SEC:
      analyze_fun = analyzeDts;
      break;

    case STREAM_CODING_TYPE_PG:
      analyze_fun = analyzePgs;
      break;

    case STREAM_CODING_TYPE_IG:
      analyze_fun = analyzeIgs;
      break;

#if 0
    case STREAM_CODING_TYPE_MVC:
    case STREAM_CODING_TYPE_HEVC:
    case STREAM_CODING_TYPE_VC1:
    case STREAM_CODING_TYPE_TEXT:
#endif
    default:
      LIBBLU_ERROR_RETURN(
        "Currently unsupported stream coding type '%s' (0x%02X).\n",
        LibbluStreamCodingTypeStr(codingType),
        codingType
      );
  }

  *dst = (LibbluESFormatUtilities) {
    .initialized = true,
    .codingType = codingType,
    .analyze = analyze_fun,
    .preparePesHeader = preparePesHeaderCommon
  };

  return 0;
}

int generateScriptES(
  LibbluESFormatUtilities utilities,
  const lbc * esFilepath,
  const lbc * outputScriptFilepath,
  LibbluESSettingsOptions options
)
{
  LibbluESParsingSettings parsingSettings;

  initLibbluESParsingSettings(
    &parsingSettings,
    esFilepath,
    outputScriptFilepath,
    options
  );

  do {
    if (utilities.analyze(&parsingSettings) < 0)
      return -1;
#if 0
    switch (codingType) {
      case STREAM_CODING_TYPE_MPEG1:
      case STREAM_CODING_TYPE_H262:
        ret = analyzeH262(esFilepath, outputScriptFilepath, flags);
        break;

      case STREAM_CODING_TYPE_AVC:
        ret = analyzeH264(
          esFilepath,
          outputScriptFilepath,
          &flags,
          &restartParsing
        );
        break;

      case STREAM_CODING_TYPE_LPCM:
        ret = analyzeLpcm(esFilepath, outputScriptFilepath);
        break;

      case STREAM_CODING_TYPE_AC3:
      case STREAM_CODING_TYPE_TRUEHD:
      case STREAM_CODING_TYPE_EAC3:
      case STREAM_CODING_TYPE_EAC3_SEC:
        ret = analyzeAc3(esFilepath, outputScriptFilepath, flags);
        break;

      case STREAM_CODING_TYPE_DTS:
      case STREAM_CODING_TYPE_HDHR:
      case STREAM_CODING_TYPE_HDMA:
      case STREAM_CODING_TYPE_DTSE_SEC:
        ret = analyzeDts(esFilepath, outputScriptFilepath, flags);
        break;

      case STREAM_CODING_TYPE_PG:
        ret = analyzePgs(esFilepath, outputScriptFilepath, flags);
        break;

      case STREAM_CODING_TYPE_IG:
        ret = analyzeIgs(esFilepath, outputScriptFilepath, flags);
        break;

      case STREAM_CODING_TYPE_MVC:
      case STREAM_CODING_TYPE_HEVC:
      case STREAM_CODING_TYPE_VC1:
      case STREAM_CODING_TYPE_TEXT:
        LIBBLU_ERROR_RETURN(
          "Unable to generate script for ES type '%s' (0x%02X).\n",
          LibbluStreamCodingTypeStr(codingType),
          codingType
        );
    }
#endif
  } while (doRestartLibbluESParsingSettings(&parsingSettings));

  return 0;
}