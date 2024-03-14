/** \~english
 * \file codecsUtilities.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Muxer integrated codecs interface module.
 */

#ifndef __LIBBLU_MUXER__CODECS_UTILITIES_H__
#define __LIBBLU_MUXER__CODECS_UTILITIES_H__

#include "codec/common/esParsingSettings.h"
#include "pesPackets.h"
#include "streamCodingType.h"

#include "codec/lpcm/lpcm_parser.h"
#include "codec/h262/h262_parser.h"
#include "codec/ac3/ac3_parser.h"
#include "codec/dts/dts_parser.h"
#include "codec/h264/h264_parser.h"
#include "codec/hdmv/igs_parser.h"
#include "codec/hdmv/pgs_parser.h"

LibbluStreamCodingType guessESStreamCodingType(
  const lbc *filepath
);

/* ### ES Format Utilities : ############################################### */

typedef int (*LibbluAnalyze_fun) (LibbluESParsingSettings *);

typedef struct {
  bool initialized;
  LibbluStreamCodingType codingType;

  LibbluAnalyze_fun analyze;
  LibbluPesPacketHeaderPrep_fun preparePesHeader;
} LibbluESFormatUtilities;

int initLibbluESFormatUtilities(
  LibbluESFormatUtilities *dst,
  LibbluStreamCodingType codingType
);

int generateScriptES(
  LibbluESFormatUtilities utilities,
  const lbc *esFilepath,
  const lbc *outputScriptFilepath,
  LibbluESSettingsOptions options
);

#endif