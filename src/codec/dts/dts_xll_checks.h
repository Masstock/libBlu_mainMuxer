/** \~english
 * \file dts_xll_checks.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief DTS audio bitstreams XLL compliance and integrity checks module.
 */

#ifndef __LIBBLU_MUXER__CODECS__DTS__XLL_CHECKS_H__
#define __LIBBLU_MUXER__CODECS__DTS__XLL_CHECKS_H__

#include "dts_data.h"
#include "dts_util.h"

int checkDtsXllCommonHeader(
  const DtsXllCommonHeader param
);

int checkDtsXllChannelSetSubHeader(
  const DtsXllChannelSetSubHeader param
);

#endif