/** \~english
 * \file dts_parser.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief DTS audio bitstreams parsing module.
 */

/** \~english
 * \dir dts
 *
 * \brief DTS audio bitstreams handling modules.
 *
 * \xrefitem references "References" "References list"
 *  [1] ETSI TS 102 114 v1.6.1 (2019-08);\n
 *  [2] ETSI TS 103 491 v1.2.1 (2019-05);\n
 *  [3] ATSC A/103:2014, "Annex E: DTS-HD File Structure";\n
 *  [4] U.S. Patent No. 5,978,762;\n
 *  [5] ffmpeg library source code.
 */

#ifndef __LIBBLU_MUXER__CODECS__DTS__PARSER_H__
#define __LIBBLU_MUXER__CODECS__DTS__PARSER_H__

#include "../common/esParsingSettings.h"
#include "../../esms/scriptCreation.h"

#include "dts_error.h"
#include "dts_util.h"
#include "dts_checks.h"
#include "dts_dtshd_file.h"
#include "dts_pbr_file.h"
#include "dts_xll.h"
#include "dts_patcher.h"
#include "dts_data.h"

int parseDtshdChunk(
  DtsContext * ctx
);

int decodeDcaCoreBitStreamHeader(
  BitstreamReaderPtr file,
  DcaCoreBSHeaderParameters * param
);

int decodeDcaCoreSS(
  DtsContext * ctx
);

int decodeDcaExtSSHeader(
  BitstreamReaderPtr file,
  DcaExtSSHeaderParameters * param
);

int decodeDcaExtSubAsset(
  BitstreamReaderPtr dtsInput,
  DtsXllFrameContextPtr * xllCtx,
  DcaAudioAssetDescriptorParameters assetParam,
  DcaExtSSHeaderParameters extSSParam,
  size_t assetLength
);

int patchDcaExtSSHeader(
  DtsContext * ctx,
  DcaExtSSHeaderParameters param,
  const unsigned xllAssetId,
  DcaXllFrameSFPosition * assetContentOffsets
);

int decodeDcaExtSS(
  DtsContext * ctx
);

int analyzeDts(
  LibbluESParsingSettings * settings
);

#endif