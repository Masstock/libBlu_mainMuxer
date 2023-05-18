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

#include "ac3_data.h"
#include "ac3_error.h"
#include "mlp_check.h"
#include "mlp_parser.h"

/** \~english
 * \brief AC-3 syncframe CRC Checking parameters ([2] 7.10.1 CRC Checking).
 *
 * 16-bit CRC word, generator polynomial: 0x18005, little-endian.
 */
#define AC3_CRC_PARAMS                                                        \
  (CrcParam) {.table = ac3CrcTable, .mask = 0xFFFF, .length = 16}

#if 0
typedef struct {
  // bool checkMode; /* Indicates if function need to fill the struct or control if values are the same */

  Ac3SyncInfoParameters syncInfo;
  Ac3BitStreamInfoParameters bitstreamInfo;

  bool eac3Present;
  Eac3BitStreamInfoParameters eac3BitstreamInfo;
} Ac3SyncFrameParameters;
#endif

#define MLP_MAX_NB_SS  4

#if 0
typedef struct {
  bool checkMode;

  uint8_t checkNibble;
  uint16_t accessUnitLength;
  uint16_t inputTiming;

  bool majorSyncExists;
  MlpMajorSyncParameters majorSync;

  bool containAtmos;

  MlpSubstreamDirectoryEntry extSubstreams[MLP_MAX_EXT_SS_NB];
} MlpAccessUnitParameters;
#endif

typedef struct {
  struct {
    Ac3SyncInfoParameters syncinfo;
    Ac3BitStreamInfoParameters bsi;

    unsigned nb_frames;
    uint64_t pts;
  } ac3;

  struct {
    MlpSyncHeaderParameters mlp_sync_header;
    MlpSubstreamDirectoryEntry substream_directory[MLP_MAX_NB_SS];
    MlpSubstreamParameters substreams[MLP_MAX_NB_SS];
    MlpInformations info;

    unsigned nb_frames;
    uint64_t pts;
  } mlp;

  struct {
    Eac3BitStreamInfoParameters bsi;

    unsigned nb_frames;
    uint64_t pts;
  } eac3;

  bool extract_core;
  bool contains_mlp;
} Ac3Context;

int analyzeAc3(
  LibbluESParsingSettings * settings
);

#endif
