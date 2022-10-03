/** \~english
 * \file h264_checks.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief H.264 video (AVC) bitstreams conformance and compliance checking module.
 */

#ifndef __LIBBLU_MUXER_CODECS_H264_CHECKS_H__
#define __LIBBLU_MUXER_CODECS_H264_CHECKS_H__

#include "../../esms/scriptData.h"
#include "h264_util.h"
#include "h264_data.h"

int checkH264AccessUnitDelimiterCompliance(
  const H264AccessUnitDelimiterParameters param
);
bool constantH264AccessUnitDelimiterCheck(
  const H264AccessUnitDelimiterParameters first,
  const H264AccessUnitDelimiterParameters second
);

/* seq_parameter_set_rbsp() NAL */
int checkH264ProfileIdcCompliance(
  const H264ProfileIdcValue profileIdc,
  const H264ContraintFlags constraints
);
int checkH264LevelIdcCompliance(
  const uint8_t levelIdc
);
int checkH264HrdParametersCompliance(
  const H264HrdParameters param
);
int checkH264VuiParametersCompliance(
  const H264VuiParameters param,
  const H264SPSDataParameters spsParam,
  const H264ParametersHandlerPtr handle
);
int checkH264SequenceParametersSetCompliance(
  const H264SPSDataParameters param,
  const H264ParametersHandlerPtr handle
);
int checkH264SpsBitstreamCompliance(
  const H264SPSDataParameters param,
  const LibbluESSettingsOptions options
);
bool constantH264ConstraintFlagsCheck(
  const H264SPSDataParameters first,
  const H264SPSDataParameters second
);
bool constantH264HrdParametersCheck(
  const H264HrdParameters first,
  const H264HrdParameters second
);
bool constantH264VuiParametersCheck(
  const H264VuiParameters first,
  const H264VuiParameters second
);
bool constantH264SequenceParametersSetCheck(
  const H264SPSDataParameters first,
  const H264SPSDataParameters second
);
int checkH264SequenceParametersSetChangeCompliance(
  const H264SPSDataParameters first,
  const H264SPSDataParameters second
);

/* pic_parameter_set_rbsp() NAL */
int checkH264PicParametersSetCompliance(
  const H264PicParametersSetParameters param,
  const H264ParametersHandlerPtr handle
);
bool constantH264PicParametersSetCheck(
  const H264PicParametersSetParameters first,
  const H264PicParametersSetParameters second
);
int checkH264PicParametersSetChangeCompliance(
  const H264PicParametersSetParameters first,
  const H264PicParametersSetParameters second,
  const H264ParametersHandlerPtr handle
);

/* sei_rbsp() NAL */
char * H264SEIMessagePayloadTypeStr(
  const H264SeiPayloadTypeValue payloadType
);
int checkH264SeiBufferingPeriodCompliance(
  const H264SeiBufferingPeriod param,
  const H264ParametersHandlerPtr handle
);
bool constantH264SeiBufferingPeriodCheck(
  const H264ParametersHandlerPtr handle,
  const H264SeiBufferingPeriod first,
  const H264SeiBufferingPeriod second
);
int checkH264SeiBufferingPeriodChangeCompliance(
  const H264ParametersHandlerPtr handle,
  const H264SeiBufferingPeriod first,
  const H264SeiBufferingPeriod second
);
int checkH264SeiPicTimingCompliance(
  const H264SeiPicTiming param,
  const H264ParametersHandlerPtr handle
);
int checkH264SeiRecoveryPointCompliance(
  const H264SeiRecoveryPoint param,
  const H264ParametersHandlerPtr handle
);
bool constantH264SeiRecoveryPointCheck(
  const H264SeiRecoveryPoint first,
  const H264SeiRecoveryPoint second
);
int checkH264SeiRecoveryPointChangeCompliance(
  const H264ParametersHandlerPtr handle,
  const H264SeiRecoveryPoint second
);

/* slice_layer_without_partitioning_rbsp() NAL */
int checkH264RefPicListModificationCompliance(
  const H264RefPicListModification param,
  const H264SliceHeaderParameters sliceHeaderParam
);
int checkH264PredWeightTableCompliance(
  const H264PredWeightTable param
);
int checkH264DecRefPicMarkingCompliance(
  const H264DecRefPicMarking param
);
int checkH264SliceHeaderCompliance(
  const H264SliceHeaderParameters param,
  const H264ParametersHandlerPtr handle,
  const LibbluESSettingsOptions options
);

int checkH264SliceLayerWithoutPartitioningCompliance(
  const H264SliceLayerWithoutPartitioningParameters param,
  const H264ParametersHandlerPtr handle,
  const LibbluESSettingsOptions options
);
int checkH264SliceHeaderChangeCompliance(
  const H264ParametersHandlerPtr handle,
  const H264SliceHeaderParameters first,
  const H264SliceHeaderParameters second,
  const LibbluESSettingsOptions options
);
int checkH264SliceLayerWithoutPartitioningChangeCompliance(
  const H264ParametersHandlerPtr handle,
  const H264SliceLayerWithoutPartitioningParameters first,
  const H264SliceLayerWithoutPartitioningParameters second,
  const LibbluESSettingsOptions options
);

#endif