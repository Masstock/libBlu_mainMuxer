/** \~english
 * \file h264_patcher.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief H.264 video (AVC) bitstreams patching module.
 */

#ifndef __LIBBLU_MUXER__CODECS__H264__PATCHER_H__
#define __LIBBLU_MUXER__CODECS__H264__PATCHER_H__

#include "h264_util.h"
#include "h264_data.h"
#include "h264_checks.h"

#include "../../esms/scriptCreation.h"

/* Buffering period SEI message patcher related : */
#define H264_TARGET_INIT_CPB_REM_DELAY_LENGTH                18 /* bits */

int buildH264RbspTrailingBits(
  H264NalByteArrayHandlerPtr nal
);

int buildH264ScalingList(
  H264NalByteArrayHandlerPtr nal,
  uint8_t *inputScalingList,
  unsigned scalingListLength
);

int buildH264HrdParameters(
  H264NalByteArrayHandlerPtr nal,
  H264HrdParameters *param
);

size_t appendH264SequenceParametersSet(
  H264ParametersHandlerPtr handle,
  size_t insertingOffset,
  H264SPSDataParameters *param
);

int rebuildH264SPSNalVuiParameters(
  H264SPSDataParameters *spsParam,
  H264ParametersHandlerPtr handle,
  const LibbluESSettingsOptions options
);

int patchH264HRDParameters_cpbRemovalDelayLength(
  H264HrdParameters *param,
  unsigned newCpbRemovalDelayLength
);

int rebuildH264SPSNalVuiHRDParameters(
  H264SPSDataParameters *spsParam
);

int patchH264SequenceParametersSet(
  H264ParametersHandlerPtr handle,
  const LibbluESSettingsOptions options
);

#if 0
int buildH264SeiBufferingPeriodMessage(
  H264ParametersHandlerPtr handle,
  H264NalByteArrayHandlerPtr seiNal,
  H264SeiBufferingPeriod *param
);

int buildH264SeiMessage(
  H264ParametersHandlerPtr handle,
  H264NalByteArrayHandlerPtr seiNal,
  H264SeiMessageParameters *param
);

int buildH264SupplementalEnhancementInformation(
  H264ParametersHandlerPtr handle,
  H264NalByteArrayHandlerPtr seiNal,
  H264SeiRbspParameters *param
);

bool isH264SeiBufferingPeriodPatchMessage(
  const H264SeiRbspParameters *param
);

size_t appendH264Sei(
  H264ParametersHandlerPtr handle,
  EsmsHandlerPtr h264Infos,
  size_t insertingOffset,
  H264SeiRbspParameters *param
);

size_t appendH264SeiBufferingPeriodPlaceHolder(
  H264ParametersHandlerPtr handle,
  EsmsHandlerPtr h264Infos,
  size_t insertingOffset,
  H264SeiRbspParameters *param
);

int patchH264SeiBufferingPeriodMessageParameters(
  H264ParametersHandlerPtr handle,
  H264SeiMessageParameters *seiMessage,
  const unsigned seq_parameter_set_id,
  const H264SeiBufferingPeriodSchedSel *hrdParam,
  const H264SeiBufferingPeriodSchedSel *vclParam
);

int insertH264SeiBufferingPeriodPlaceHolder(
  H264ParametersHandlerPtr handle
);

int completeH264SeiBufferingPeriodComputation(
  H264ParametersHandlerPtr handle,
  EsmsHandlerPtr h264Infos
);
#endif

#endif