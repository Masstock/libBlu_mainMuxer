/** \~english
 * \file dts_checks.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief DTS audio bitstreams compliance and integrity checks module.
 */

#ifndef __LIBBLU_MUXER__CODECS__DTS__CHECKS_H__
#define __LIBBLU_MUXER__CODECS__DTS__CHECKS_H__

#include "dts_util.h"
#include "dts_data.h"

#include "../common/constantCheckFunctionsMacros.h"

#define IS_ENTRY_POINT_DCA_CORE_SS(syncFrame)                                 \
  (!(syncFrame).header.predHist)

int checkDcaCoreSSCompliance(
  const DcaCoreSSFrameParameters * frame,
  DtsDcaCoreSSWarningFlags * warning_flags
);

bool constantDcaCoreSS(
  const DcaCoreSSFrameParameters * first,
  const DcaCoreSSFrameParameters * second
);

/** \~english
 * \brief
 *
 * \param old
 * \param cur
 * \param warning_flags
 * \return int
 *
 * Apply rules from ETSI TS 102 114 V.1.6.1 E.4
 */
int checkDcaCoreSSChangeCompliance(
  const DcaCoreSSFrameParameters * old,
  const DcaCoreSSFrameParameters * cur,
  DtsDcaCoreSSWarningFlags * warning_flags
);

int checkDcaExtSSHeaderCompliance(
  const DcaExtSSHeaderParameters * param,
  bool is_sec_stream,
  DtsDcaExtSSWarningFlags * warning_flags
);

#endif