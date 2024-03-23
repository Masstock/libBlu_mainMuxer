/** \~english
 * \file h264_checks.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief H.264 video (AVC) bitstreams conformance and compliance checking module.
 */

#ifndef __LIBBLU_MUXER__CODECS__H264__CHECKS_H__
#define __LIBBLU_MUXER__CODECS__H264__CHECKS_H__

#include "../common/constantCheckFunctionsMacros.h"
#include "../../esms/scriptData.h"
#include "h264_util.h"
#include "h264_data.h"

/* ### Access Unit Delimiter (AUD) : ####################################### */

int checkH264AccessUnitDelimiterCompliance(
  H264WarningFlags *warn,
  H264AccessUnitDelimiterParameters param
);

static inline bool constantH264AccessUnitDelimiterCheck(
  H264AccessUnitDelimiterParameters first,
  H264AccessUnitDelimiterParameters second
)
{
  return CHECK(
    EQUAL(.primary_pic_type)
  );
}

/* ### Sequence Parameters Set (SPS) : ##################################### */

/* seq_parameter_set_rbsp() NAL */
int checkH264SequenceParametersSetCompliance(
  const H264ParametersHandlerPtr handle,
  H264SPSDataParameters param
);

int checkH264SpsBitstreamCompliance(
  LibbluESSettingsOptions options,
  H264SPSDataParameters param
);

bool constantH264SequenceParametersSetCheck(
  H264SPSDataParameters first,
  H264SPSDataParameters second
);

int checkH264SequenceParametersSetChangeCompliance(
  H264SPSDataParameters first,
  H264SPSDataParameters second
);

/* ### Picture Parameters Set (SPS) : ###################################### */

/* pic_parameter_set_rbsp() NAL */
int checkH264PicParametersSetCompliance(
  const H264ParametersHandlerPtr handle,
  H264PicParametersSetParameters param
);

static inline bool constantH264PicParametersSetCheck(
  H264PicParametersSetParameters first,
  H264PicParametersSetParameters second
)
{
  return CHECK(
    EQUAL(.pic_parameter_set_id)
    EQUAL(.seq_parameter_set_id)
    EQUAL(.entropy_coding_mode_flag)
    EQUAL(.bottom_field_pic_order_in_frame_present_flag)
    EQUAL(.num_slice_groups_minus1)
    EQUAL(.num_ref_idx_l0_default_active_minus1)
    EQUAL(.num_ref_idx_l1_default_active_minus1)
    EQUAL(.weighted_pred_flag)
    EQUAL(.weighted_bipred_idc)
    EQUAL(.pic_init_qp_minus26)
    EQUAL(.pic_init_qs_minus26)
    EQUAL(.deblocking_filter_control_present_flag)
    EQUAL(.constrained_intra_pred_flag)
    EQUAL(.redundant_pic_cnt_present_flag)
    EQUAL(.transform_8x8_mode_flag)
  );
}

int checkH264PicParametersSetChangeCompliance(
  const H264ParametersHandlerPtr handle,
  H264PicParametersSetParameters first,
  H264PicParametersSetParameters second
);

/* ### Supplemental Enhancement Information (SEI) : ######################## */

/* ###### Buffering Period SEI message : ################################### */

/* sei_rbsp() NAL */
int checkH264SeiBufferingPeriodCompliance(
  const H264ParametersHandlerPtr handle,
  H264SeiBufferingPeriod param
);

bool constantH264SeiBufferingPeriodCheck(
  const H264ParametersHandlerPtr handle,
  H264SeiBufferingPeriod first,
  H264SeiBufferingPeriod second
);

int checkH264SeiBufferingPeriodChangeCompliance(
  const H264ParametersHandlerPtr handle,
  H264SeiBufferingPeriod first,
  H264SeiBufferingPeriod second
);

/* ###### Picture Timing SEI message : ##################################### */

int checkH264SeiPicTimingCompliance(
  const H264ParametersHandlerPtr handle,
  H264SeiPicTiming param
);

/* ###### Recovery Point SEI message : ##################################### */

int checkH264SeiRecoveryPointCompliance(
  const H264ParametersHandlerPtr handle,
  H264SeiRecoveryPoint param
);

static inline bool constantH264SeiRecoveryPointCheck(
  H264SeiRecoveryPoint first,
  H264SeiRecoveryPoint second
)
{
  return CHECK(
    EQUAL(.recovery_frame_cnt)
    EQUAL(.exact_match_flag)
    EQUAL(.broken_link_flag)
    EQUAL(.changing_slice_group_idc)
  );
}

int checkH264SeiRecoveryPointChangeCompliance(
  const H264ParametersHandlerPtr handle,
  H264SeiRecoveryPoint second
);

/* ### Slices : ############################################################ */

/* ###### Slice header : ################################################### */

int checkH264SliceHeaderCompliance(
  const H264ParametersHandlerPtr handle,
  LibbluESSettingsOptions options,
  H264SliceHeaderParameters param
);

int checkH264SliceHeaderChangeCompliance(
  const H264ParametersHandlerPtr handle,
  LibbluESSettingsOptions options,
  H264SliceHeaderParameters first,
  H264SliceHeaderParameters second
);

/* ###### Slice layer without partitioning : ############################### */

/* slice_layer_without_partitioning_rbsp() NAL */
int checkH264SliceLayerWithoutPartitioningCompliance(
  const H264ParametersHandlerPtr handle,
  LibbluESSettingsOptions options,
  H264SliceLayerWithoutPartitioningParameters param
);

int checkH264SliceLayerWithoutPartitioningChangeCompliance(
  const H264ParametersHandlerPtr handle,
  LibbluESSettingsOptions options,
  H264SliceLayerWithoutPartitioningParameters first,
  H264SliceLayerWithoutPartitioningParameters second
);

#endif