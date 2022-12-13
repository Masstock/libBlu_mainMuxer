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

int checkH264AccessUnitDelimiterCompliance(
  H264AccessUnitDelimiterParameters param
);

static inline bool constantH264AccessUnitDelimiterCheck(
  H264AccessUnitDelimiterParameters first,
  H264AccessUnitDelimiterParameters second
)
{
  return START_CHECK
    EQUAL(.primary_pic_type)
  END_CHECK;
}

/* seq_parameter_set_rbsp() NAL */
int checkH264ProfileIdcCompliance(
  H264ProfileIdcValue profile_idc,
  H264ContraintFlags constraints
);
int checkH264LevelIdcCompliance(
  uint8_t level_idc
);
int checkH264HrdParametersCompliance(
  H264HrdParameters param
);
int checkH264VuiParametersCompliance(
  const H264ParametersHandlerPtr handle,
  H264VuiParameters param,
  H264SPSDataParameters spsParam
);
int checkH264SequenceParametersSetCompliance(
  const H264ParametersHandlerPtr handle,
  H264SPSDataParameters param
);
int checkH264SpsBitstreamCompliance(
  LibbluESSettingsOptions options,
  H264SPSDataParameters param
);

static inline bool constantH264ConstraintFlagsCheck(
  H264ContraintFlags first,
  H264ContraintFlags second
)
{
  return START_CHECK
    EQUAL(.set0)
    EQUAL(.set1)
    EQUAL(.set2)
    EQUAL(.set3)
    EQUAL(.set4)
    EQUAL(.set5)
    EQUAL(.reservedFlags)
  END_CHECK;
}

static inline bool constantH264HrdParametersCheck(
  H264HrdParameters first,
  H264HrdParameters second
)
{
  return START_CHECK
    EQUAL(.cpb_cnt_minus1)
    EQUAL(.bit_rate_scale)
    EQUAL(.cpb_size_scale)
    EQUAL(.initial_cpb_removal_delay_length_minus1)
    EQUAL(.cpb_removal_delay_length_minus1)
    EQUAL(.dpb_output_delay_length_minus1)
    EQUAL(.time_offset_length)
    EXPR(
      0 == memcmp(
        first.schedSel,
        second.schedSel,
        (first.cpb_cnt_minus1 + 1) * sizeof(H264SchedSel)
      )
    )
  END_CHECK;
}

static inline bool constantH264VuiColourDescriptionParametersCheck(
  H264VuiColourDescriptionParameters first,
  H264VuiColourDescriptionParameters second
)
{
  return START_CHECK
    EQUAL(.colour_primaries)
    EQUAL(.transfer_characteristics)
    EQUAL(.matrix_coefficients)
  END_CHECK;
}

static inline bool constantH264VuiVideoSeqBitstreamRestrictionsParametersCheck(
  H264VuiVideoSeqBitstreamRestrictionsParameters first,
  H264VuiVideoSeqBitstreamRestrictionsParameters second
)
{
  return START_CHECK
    EQUAL(.motion_vectors_over_pic_boundaries_flag)
    EQUAL(.max_bytes_per_pic_denom)
    EQUAL(.max_bits_per_mb_denom)
    EQUAL(.log2_max_mv_length_horizontal)
    EQUAL(.log2_max_mv_length_vertical)
    EQUAL(.max_num_reorder_frames)
    EQUAL(.max_dec_frame_buffering)
  END_CHECK;
}

static inline bool constantH264VuiParametersCheck(
  H264VuiParameters first,
  H264VuiParameters second
)
{
  return START_CHECK
    EQUAL(.aspect_ratio_info_present_flag)
    EQUAL(.aspect_ratio_idc)
    EQUAL(.sar_width)
    EQUAL(.sar_height)
    EQUAL(.overscan_info_present_flag)
    EQUAL(.overscan_appropriate_flag)
    EQUAL(.video_signal_type_present_flag)
    EQUAL(.video_format)
    EQUAL(.video_full_range_flag)
    EQUAL(.colour_description_present_flag)
    SUB_FUN(
      .colour_description,
      constantH264VuiColourDescriptionParametersCheck
    )
    EQUAL(.chroma_loc_info_present_flag)
    EQUAL(.chroma_sample_loc_type_top_field)
    EQUAL(.chroma_sample_loc_type_bottom_field)
    EQUAL(.timing_info_present_flag)
    START_COND(.timing_info_present_flag, true)
      EQUAL(.num_units_in_tick)
      EQUAL(.time_scale)
      EQUAL(.fixed_frame_rate_flag)
    END_COND
    EQUAL(.nal_hrd_parameters_present_flag)
    START_COND(.nal_hrd_parameters_present_flag, true)
      SUB_FUN(.nal_hrd_parameters, constantH264HrdParametersCheck)
    END_COND
    EQUAL(.vcl_hrd_parameters_present_flag)
    START_COND(.vcl_hrd_parameters_present_flag, true)
      SUB_FUN(.vcl_hrd_parameters, constantH264HrdParametersCheck)
    END_COND
    EQUAL(.low_delay_hrd_flag)
    EQUAL(.pic_struct_present_flag)
    EQUAL(.bitstream_restriction_flag)
    START_COND(.bitstream_restriction_flag, true)
      SUB_FUN(
        .bistream_restrictions,
        constantH264VuiVideoSeqBitstreamRestrictionsParametersCheck
      )
    END_COND
  END_CHECK;
}

static inline bool constantH264SequenceParametersSetCheck(
  H264SPSDataParameters first,
  H264SPSDataParameters second
)
{
  return START_CHECK
    EQUAL(.profile_idc)
    SUB_FUN(.constraint_set_flags, constantH264ConstraintFlagsCheck)
    EQUAL(.level_idc)
    EQUAL(.seq_parameter_set_id)
    EQUAL(.chroma_format_idc)
    EQUAL(.separate_colour_plane_flag)
    EQUAL(.bit_depth_luma_minus8)
    EQUAL(.bit_depth_chroma_minus8)
    EQUAL(.qpprime_y_zero_transform_bypass_flag)
    EQUAL(.seq_scaling_matrix_present_flag)
    START_COND(.seq_scaling_matrix_present_flag, true)
      EXPR(
        0 == memcmp(
          &first.seq_scaling_matrix,
          &second.seq_scaling_matrix,
          sizeof(H264SeqScalingMatrix)
        )
      )
    END_COND
    EQUAL(.log2_max_frame_num_minus4)
    EQUAL(.pic_order_cnt_type)
    START_COND(.pic_order_cnt_type, 0)
      EQUAL(.log2_max_pic_order_cnt_lsb_minus4)
    END_COND
    START_COND(.pic_order_cnt_type, 1)
      EQUAL(.delta_pic_order_always_zero_flag)
      EQUAL(.offset_for_non_ref_pic)
      EQUAL(.offset_for_top_to_bottom_field)
      EQUAL(.num_ref_frames_in_pic_order_cnt_cycle)
      EXPR(
        0 == memcmp(
          &first.offset_for_ref_frame,
          &second.offset_for_ref_frame,
          first.num_ref_frames_in_pic_order_cnt_cycle * sizeof(int)
        )
      )
    END_COND
    EQUAL(.max_num_ref_frames)
    EQUAL(.gaps_in_frame_num_value_allowed_flag)
    EQUAL(.pic_width_in_mbs_minus1)
    EQUAL(.pic_height_in_map_units_minus1)
    EQUAL(.frame_mbs_only_flag)
    EQUAL(.mb_adaptive_frame_field_flag)
    EQUAL(.direct_8x8_inference_flag)
    EQUAL(.frame_cropping_flag)
    EQUAL(.frame_crop_offset.left)
    EQUAL(.frame_crop_offset.right)
    EQUAL(.frame_crop_offset.top)
    EQUAL(.frame_crop_offset.bottom)
    EQUAL(.vui_parameters_present_flag)
    START_COND(.vui_parameters_present_flag, true)
      SUB_FUN(.vui_parameters, constantH264VuiParametersCheck)
    END_COND
  END_CHECK;
}

static inline int checkH264SequenceParametersSetChangeCompliance(
  H264SPSDataParameters first,
  H264SPSDataParameters second
)
{
  bool constantFields = START_CHECK
    EQUAL(.profile_idc)
    SUB_FUN(.constraint_set_flags, constantH264ConstraintFlagsCheck)
    EQUAL(.level_idc)
    EQUAL(.seq_parameter_set_id) /* Only one seq_parameter_set_id allowed */
    EQUAL(.chroma_format_idc)
    EQUAL(.separate_colour_plane_flag)
    EQUAL(.bit_depth_luma_minus8)
    EQUAL(.bit_depth_chroma_minus8)
    EQUAL(.log2_max_frame_num_minus4)
    EQUAL(.pic_order_cnt_type)
    START_COND(.pic_order_cnt_type, 0)
      EQUAL(.log2_max_pic_order_cnt_lsb_minus4)
    END_COND
    START_COND(.pic_order_cnt_type, 1)
      EQUAL(.delta_pic_order_always_zero_flag)
      EQUAL(.offset_for_non_ref_pic)
      EQUAL(.offset_for_top_to_bottom_field)
      EQUAL(.num_ref_frames_in_pic_order_cnt_cycle)
      EXPR(
        0 == memcmp(
          &first.offset_for_ref_frame,
          &second.offset_for_ref_frame,
          first.num_ref_frames_in_pic_order_cnt_cycle * sizeof(int)
        )
      )
    END_COND
    EQUAL(.max_num_ref_frames)
    EQUAL(.gaps_in_frame_num_value_allowed_flag)
    EQUAL(.pic_width_in_mbs_minus1)
    EQUAL(.pic_height_in_map_units_minus1)
    EQUAL(.frame_mbs_only_flag)
    EQUAL(.mb_adaptive_frame_field_flag)
    EQUAL(.direct_8x8_inference_flag)
    EQUAL(.frame_cropping_flag)
    EQUAL(.frame_crop_offset.left)
    EQUAL(.frame_crop_offset.right)
    EQUAL(.frame_crop_offset.top)
    EQUAL(.frame_crop_offset.bottom)
    EQUAL(.vui_parameters_present_flag)
    START_COND(.vui_parameters_present_flag, true)
      SUB_FUN(.vui_parameters, constantH264VuiParametersCheck)
    END_COND
  END_CHECK;

  if (!constantFields)
    return -1;
  return 0;
}

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
  return START_CHECK
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
  END_CHECK;
}

int checkH264PicParametersSetChangeCompliance(
  const H264ParametersHandlerPtr handle,
  H264PicParametersSetParameters first,
  H264PicParametersSetParameters second
);

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
int checkH264SeiPicTimingCompliance(
  const H264ParametersHandlerPtr handle,
  H264SeiPicTiming param
);
int checkH264SeiRecoveryPointCompliance(
  const H264ParametersHandlerPtr handle,
  H264SeiRecoveryPoint param
);

static inline bool constantH264SeiRecoveryPointCheck(
  H264SeiRecoveryPoint first,
  H264SeiRecoveryPoint second
)
{
  return START_CHECK
    EQUAL(.recovery_frame_cnt)
    EQUAL(.exact_match_flag)
    EQUAL(.broken_link_flag)
    EQUAL(.changing_slice_group_idc)
  END_CHECK;
}

int checkH264SeiRecoveryPointChangeCompliance(
  const H264ParametersHandlerPtr handle,
  H264SeiRecoveryPoint second
);

/* slice_layer_without_partitioning_rbsp() NAL */
int checkH264RefPicListModificationCompliance(
  H264RefPicListModification param,
  H264SliceHeaderParameters sliceHeaderParam
);
int checkH264PredWeightTableCompliance(
  H264PredWeightTable param
);
int checkH264DecRefPicMarkingCompliance(
  H264DecRefPicMarking param
);
int checkH264SliceHeaderCompliance(
  const H264ParametersHandlerPtr handle,
  LibbluESSettingsOptions options,
  H264SliceHeaderParameters param
);

int checkH264SliceLayerWithoutPartitioningCompliance(
  const H264ParametersHandlerPtr handle,
  LibbluESSettingsOptions options,
  H264SliceLayerWithoutPartitioningParameters param
);
int checkH264SliceHeaderChangeCompliance(
  const H264ParametersHandlerPtr handle,
  LibbluESSettingsOptions options,
  H264SliceHeaderParameters first,
  H264SliceHeaderParameters second
);
int checkH264SliceLayerWithoutPartitioningChangeCompliance(
  const H264ParametersHandlerPtr handle,
  LibbluESSettingsOptions options,
  H264SliceLayerWithoutPartitioningParameters first,
  H264SliceLayerWithoutPartitioningParameters second
);

#endif