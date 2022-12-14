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
  const lbc * filepath
);

/* ### ES Format Utilities : ############################################### */

typedef struct {
  bool initialized;
  LibbluStreamCodingType codingType;

  int (*analyze) (LibbluESParsingSettings *);
  LibbluESPesPacketHeaderPrepFun preparePesHeader;
} LibbluESFormatUtilities;

static inline void cleanLibbluESFormatUtilities(
  LibbluESFormatUtilities * dst
)
{
  *dst = (LibbluESFormatUtilities) {
    .initialized = false
  };
}

static inline bool isInitializedLibbluESFormatUtilities(
  LibbluESFormatUtilities utilities
)
{
  return utilities.initialized;
}

int initLibbluESFormatUtilities(
  LibbluESFormatUtilities * dst,
  LibbluStreamCodingType codingType
);

int generateScriptES(
  LibbluESFormatUtilities utilities,
  const lbc * esFilepath,
  const lbc * outputScriptFilepath,
  LibbluESSettingsOptions options
);

#if 0
static inline int preparePesHeaderParam(
  LibbluESFormatUtilities utilities,
  PesPacketHeaderParam * dst,
  LibbluESPesPacketProperties pesPacketProp,
  LibbluStreamCodingType codingType
)
{
  return utilities.preparePesHeader(dst, pesPacketProp, codingType);
}
#endif

#endif