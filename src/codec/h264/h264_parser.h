/** \~english
 * \file h264_parser.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief H.264 video (AVC) bitstreams parsing module.
 */

/** \~english
 * \dir h264
 *
 * \brief H.264 video (AVC) bitstreams handling modules.
 *
 * \todo Add support for H.264 MVC bitstreams and enable Blu-ray 3D support.
 * \todo Delete requirement of Buffering period SEI and Picture timing
SEI for the verification of the HRD conformance.
 *
 * \xrefitem references "References" "References list"
 *  [1] ITU-T Rec. H.264 (06/2019).
 */

#ifndef __LIBBLU_MUXER__CODECS__H264__PARSER_H__
#define __LIBBLU_MUXER__CODECS__H264__PARSER_H__

#include "../../util.h"
#include "../../esms/scriptCreation.h"
#include "../../pesPackets.h"
#include "../common/esParsingSettings.h"

#include "h264_util.h"
#include "h264_patcher.h"
#include "h264_checks.h"
#include "h264_hrdVerifier.h"
#include "h264_data.h"

int parseH264RbspTrailingBits(
  H264ParametersHandlerPtr handle
);

int processAccessUnit(
  H264ParametersHandlerPtr handle,
  LibbluESParsingSettings * settings
);

int processCompleteAccessUnit(
  H264ParametersHandlerPtr handle,
  const LibbluESSettingsOptions options
);

/* access_unit_delimiter_rbsp() NAL */
int decodeH264AccessUnitDelimiter(
  H264ParametersHandlerPtr handle
);

/* seq_parameter_set_rbsp() NAL */
int buildScalingList(
  H264ParametersHandlerPtr handle,
  uint8_t * scalingList,
  const unsigned sizeOfList,
  bool * useDefaultScalingMatrix
);

int parseH264HrdParameters(
  H264ParametersHandlerPtr handle,
  H264HrdParameters * param
);

int parseH264VuiParameters(
  H264ParametersHandlerPtr handle,
  H264VuiParameters * param
);

int parseH264SequenceParametersSetData(
  H264ParametersHandlerPtr handle,
  H264SPSDataParameters * param
);

int decodeH264SequenceParametersSet(
  H264ParametersHandlerPtr handle,
  const LibbluESSettingsOptions options
);

/* pic_parameter_set_rbsp() NAL */
int decodeH264PicParametersSet(
  H264ParametersHandlerPtr handle
);

/* sei_rbsp() NAL */
int parseH264SeiBufferingPeriod(
  H264ParametersHandlerPtr handle,
  H264SeiBufferingPeriod * param
);

int parseH264SeiPicTiming(
  H264ParametersHandlerPtr handle,
  H264SeiPicTiming * param
);

int parseH264SeiUserDataUnregistered(
  H264ParametersHandlerPtr handle,
  H264SeiUserDataUnregistered * param,
  const size_t payloadSize
);

int parseH264SeiRecoveryPoint(
  H264ParametersHandlerPtr handle,
  H264SeiRecoveryPoint * param
);

int parseH264SeiReservedSeiMessage(
  H264ParametersHandlerPtr handle,
  const size_t payloadSize
);

int parseH264SupplementalEnhancementInformationMessage(
  H264ParametersHandlerPtr handle,
  H264SeiMessageParameters * param
);

int decodeH264SupplementalEnhancementInformation(
  H264ParametersHandlerPtr handle
);

/* slice_layer_without_partitioning_rbsp() NAL */
int parseH264RefPicListModification(
  H264ParametersHandlerPtr handle,
  H264RefPicListModification * param,
  H264SliceTypeValue slice_type
);

int parseH264PredWeightTable(
  H264ParametersHandlerPtr handle,
  H264PredWeightTable * param,
  H264SliceHeaderParameters * sliceHeaderParam
);

int parseH264DecRefPicMarking(
  H264ParametersHandlerPtr handle,
  H264DecRefPicMarking * param,
  bool IdrPicFlag
);

int parseH264SliceHeader(
  H264ParametersHandlerPtr handle,
  H264SliceHeaderParameters * param
);

typedef enum {
  H264_FRST_VCL_NALU_PCP_RET_ERROR  = -1,  /**< Fatal error happen.          */
  H264_FRST_VCL_NALU_PCP_RET_FALSE  =  0,  /**< VCL NALU is not the start of
    a new primary coded picture.                                             */
  H264_FRST_VCL_NALU_PCP_RET_TRUE   =  1   /**< Successful detection of the
    first VCL NALU of a primary coded picture.                               */
} H264FirstVclNaluPCPRet;

H264FirstVclNaluPCPRet detectFirstVclNalUnitOfPrimCodedPicture(
  H264SliceHeaderParameters last,
  H264SliceHeaderParameters cur
);

int decodeH264SliceLayerWithoutPartitioning(
  H264ParametersHandlerPtr handle,
  const LibbluESSettingsOptions options
);

/* filler_data_rbsp() NAL */
int decodeH264FillerData(
  H264ParametersHandlerPtr handle
);

/* end_of_seq_rbsp() NAL */
int decodeH264EndOfSequence(
  H264ParametersHandlerPtr handle
);

int analyzeH264(
  LibbluESParsingSettings * settings
);

#endif