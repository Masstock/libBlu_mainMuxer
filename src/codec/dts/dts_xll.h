/** \~english
 * \file dts_xll.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief DTS audio bitstreams Extension Substream LossLess parsing module
 * (DTS XLL).
 */

#ifndef __LIBBLU_MUXER__CODECS__DTS__XLL_H__
#define __LIBBLU_MUXER__CODECS__DTS__XLL_H__

#include "dts_xll_util.h"
#include "dts_xll_checks.h"

#include "dts_data.h"

#define DTS_XLL_FORCE_UNPACKING false
#define DTS_XLL_FORCE_REBUILD_SEIING false

int decodeDtsXllCommonHeader(
  DtsXllFrameContextPtr ctx,
  DtsXllCommonHeader * param
);

int decodeDtsXllFrame(
  DtsXllFrameContextPtr ctx,
  DcaXllFrameSFPosition * pbrFramePosition
);

int parseDtsXllFrame(
  BitstreamReaderPtr dtsInput,
  DtsXllFrameContextPtr ctx,
  DcaAudioAssetDescriptorParameters asset,
  size_t assetLength
);

#endif