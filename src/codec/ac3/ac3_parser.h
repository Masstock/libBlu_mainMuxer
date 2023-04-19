/** \~english
 * \file ac3_parser.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Dolby audio (AC3, E-AC3, TrueHD...) bitstreams handling module.
 *
 * \todo Split module in sub-modules to increase readability.
 * \todo E-AC3 Parameters checking and secondary track support.
 * \todo Audio block parsing for extended checking (and Dolby Atmos audio
 * block verifications).
 * \todo Issues with CRC checks.
 *
 * \xrefitem references "References" "References list"
 *  [1] AC-3/E-AC3 - ETSI TS 102 366 V1.4.1;\n
 *  [2] AC-3/E-AC3 - ATSC Standard A52-2018;\n
 *  [3] TrueHD - Dolby TrueHD (MLP) - High-level bitstream description;\n
 *  [4] Object Audio E-AC3 - ETSI TS 103 420 V1.2.1.
 */

#ifndef __LIBBLU_MUXER__CODECS__AC3__PARSER_H__
#define __LIBBLU_MUXER__CODECS__AC3__PARSER_H__

#include "../../esms/scriptCreation.h"
#include "../common/esParsingSettings.h"

#include "ac3_error.h"
#include "ac3_data.h"

/** \~english
 * \brief AC-3 syncframe CRC Checking parameters ([2] 7.10.1 CRC Checking).
 *
 * 16-bit CRC word, generator polynomial: 0x18005, little-endian.
 */
#define AC3_CRC_PARAMS  (CrcParam) {16, 0x18005, AC3_CRC_TABLE, false}

/** \~english
 * \brief TrueHD Major Sync CRC parameters ([3] 4.2.10 major_sync_info_CRC).
 *
 * 16-bit CRC word, generator polynomial: 0x1002D, big-endian.
 *
 */
#define TRUE_HD_MS_CRC_PARAMS  (CrcParam) {16, 0x1002D, NO_CRC_TABLE, true}

typedef struct {
  bool checkMode; /* Indicates if function need to fill the struct or control if values are the same */

  Ac3SyncInfoParameters syncInfo;
  Ac3BitStreamInfoParameters bitstreamInfo;

  bool eac3Present;
  Eac3BitStreamInfoParameters eac3BitstreamInfo;
} Ac3SyncFrameParameters;

#define MLP_MAX_EXT_SS_NB  4

typedef struct {
  bool checkMode;

  uint8_t checkNibble;
  uint16_t accessUnitLength;
  uint16_t inputTiming;

  bool majorSyncExists;
  MlpMajorSyncParameters majorSync;

  bool containAtmos;

  MlpExtSubstream extSubstreams[MLP_MAX_EXT_SS_NB];
} MlpAccessUnitParameters;

int analyzeAc3(
  LibbluESParsingSettings * settings
);

#endif
