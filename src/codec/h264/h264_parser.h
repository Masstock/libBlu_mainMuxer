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
 *  [1] ITU-T Rec. H.264 (08/2021).
 *
 * Used abbreviations (with chapters numbers in [1]):
 *  - NAL : Network Abstraction Layer (4, 6.1);
 *  - NALU : NAL Unit (3.97);
 *  - SPS : Sequence Parameter Set (7.3.2.1, 7.4.2.1);
 *  - PPS : Picture Paramter Set (7.3.2.2, 7.4.2.2);
 *  - SEI : Supplemental Enhancement Information (7.3.2.3, 7.4.2.3).
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

/* access_unit_delimiter_rbsp() NAL */
int decodeH264AccessUnitDelimiter(
  H264ParametersHandlerPtr handle
);

/* seq_parameter_set_rbsp() NAL */
int decodeH264SequenceParametersSet(
  H264ParametersHandlerPtr handle,
  LibbluESSettingsOptions options
);

/* pic_parameter_set_rbsp() NAL */
int decodeH264PicParametersSet(
  H264ParametersHandlerPtr handle
);

/* sei_rbsp() NAL */
int decodeH264SupplementalEnhancementInformation(
  H264ParametersHandlerPtr handle
);

/* slice_layer_without_partitioning_rbsp() NAL */
int decodeH264SliceLayerWithoutPartitioning(
  H264ParametersHandlerPtr handle,
  LibbluESSettingsOptions options
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