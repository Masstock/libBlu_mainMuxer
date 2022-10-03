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

int checkDcaCoreSSFrameHeaderCompliance(
  const DcaCoreSSFrameHeaderParameters param,
  DtsDcaCoreSSWarningFlags * warningFlags
);

bool constantDcaCoreSSFrameHeader(
  DcaCoreSSFrameHeaderParameters first,
  DcaCoreSSFrameHeaderParameters second
);

#define IS_ENTRY_POINT_DCA_CORE_SS(syncFrame)                                 \
  (!(syncFrame).header.predHist)

int checkDcaCoreSSFrameHeaderChangeCompliance(
  const DcaCoreSSFrameHeaderParameters old,
  const DcaCoreSSFrameHeaderParameters cur
);

int checkDcaCoreSSCompliance(
  const DcaCoreSSFrameParameters frame,
  DtsDcaCoreSSWarningFlags * warningFlags
);

bool constantDcaCoreSS(
  const DcaCoreSSFrameParameters first,
  const DcaCoreSSFrameParameters second
);

/** \~english
 * \brief
 *
 * \param old
 * \param cur
 * \param warningFlags
 * \return int
 *
 * Apply rules from ETSI TS 102 114 V.1.6.1 E.4
 */
int checkDcaCoreSSChangeCompliance(
  const DcaCoreSSFrameParameters old,
  const DcaCoreSSFrameParameters cur,
  DtsDcaCoreSSWarningFlags * warningFlags
);

int checkDcaExtSSHeaderMixMetadataCompliance(
  DcaExtSSHeaderMixMetadataParameters param
);

int checkDcaExtSSHeaderStaticFieldsCompliance(
  const DcaExtSSHeaderStaticFieldsParameters param,
  bool isSecondaryStream,
  unsigned nExtSSIndex,
  DtsDcaExtSSWarningFlags * warningFlags
);

int checkDcaAudioAssetDescriptorStaticFieldsCompliance(
  const DcaAudioAssetDescriptorStaticFieldsParameters param,
  bool isSecondaryStream,
  DtsDcaExtSSWarningFlags * warningFlags
);

int checkDcaAudioAssetDescriptorDynamicMetadataCompliance(
  const DcaAudioAssetDescriptorDynamicMetadataParameters param,
  const DcaAudioAssetDescriptorStaticFieldsParameters staticFields
);

int checkDcaAudioAssetDescriptorDecoderNavDataCompliance(
  const DcaAudioAssetDescriptorDecoderNavDataParameters param,
  const DcaExtSSHeaderMixMetadataParameters mixMetadata,
  bool isSecondaryStream
);

int checkDcaAudioAssetDescriptorCompliance(
  const DcaAudioAssetDescriptorParameters param,
  bool isSecondaryStream,
  const DcaExtSSHeaderMixMetadataParameters mixMetadata,
  DtsDcaExtSSWarningFlags * warningFlags
);

int checkDcaExtSSHeaderCompliance(
  const DcaExtSSHeaderParameters param,
  bool isSecondaryStream,
  DtsDcaExtSSWarningFlags * warningFlags
);

#endif