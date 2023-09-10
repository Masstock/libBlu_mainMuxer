#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <assert.h>

#include "h264_patcher.h"

int buildH264RbspTrailingBits(
  H264NalByteArrayHandlerPtr nal
)
{
  /* [v1 rbsp_stop_one_bit] // 0b1 */
  if (writeH264NalByteArrayBits(nal, 0x1, 1) < 0)
    return -1;

  while (!isByteAlignedH264NalByteArray(nal)) {
    /* [v1 rbsp_alignment_zero_bit] // 0b0 */
    if (writeH264NalByteArrayBits(nal, 0x0, 1) < 0)
      return -1;
  }

  return 0;
}

int buildH264ScalingList(
  H264NalByteArrayHandlerPtr nal,
  uint8_t * inputScalingList,
  unsigned scalingListLength
)
{
  unsigned i, scanIdx;
  int deltaScale, lastScale, nextScale;

  static const unsigned short ZZ_SCANNING_16_ORDER_MATRIX[16] = {
    0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15
  };
  static const unsigned short ZZ_SCANNING_64_ORDER_MATRIX[64] = {
     0,  1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
  };
  const unsigned short * zzScanningOrderMatrix;

  assert(NULL != inputScalingList);
  assert(scalingListLength == 16 || scalingListLength == 64);

  lastScale = 8, nextScale = 8;

  if (scalingListLength == 16)
    zzScanningOrderMatrix = ZZ_SCANNING_16_ORDER_MATRIX;
  else
    zzScanningOrderMatrix = ZZ_SCANNING_64_ORDER_MATRIX;

  for (i = 0; i < scalingListLength; i++) {
    /* Get scale index in ScalingList using Zig-Zag parsing : */
    scanIdx = zzScanningOrderMatrix[i];

    if (nextScale != 0) {
      /* Compute delta between lastScale and current scale in ScalingList : */
      deltaScale = inputScalingList[scanIdx] - lastScale;

      /* Apply modulo : */
      if (127 < deltaScale)
        deltaScale -= 256;
      if (deltaScale < -128)
        deltaScale += 256;

      /* [se delta_scale] */
      if (writeH264NalByteArraySignedExpGolombCode(nal, deltaScale) < 0)
        return -1;

      nextScale = inputScalingList[scanIdx]; /* Update nextScale */
    }

    lastScale = inputScalingList[scanIdx]; /* Update lastScale */
  }

  return 0;
}

int buildH264HrdParameters(
  H264NalByteArrayHandlerPtr nal,
  H264HrdParameters * param
)
{
  H264SchedSel * schedSel; /* Alias for readability */
  unsigned SchedSelIdx;

  assert(NULL != nal);
  assert(NULL != param);

  /* [ue cpb_cnt_minus1] */
  if (writeH264NalByteArrayExpGolombCode(nal, param->cpb_cnt_minus1) < 0)
    return -1;

  /* [u4 bit_rate_scale] */
  if (writeH264NalByteArrayBits(nal, param->bit_rate_scale, 4) < 0)
    return -1;

  /* [u4 cpb_size_scale] */
  if (writeH264NalByteArrayBits(nal, param->cpb_size_scale, 4) < 0)
    return -1;

  for (SchedSelIdx = 0; SchedSelIdx <= param->cpb_cnt_minus1; SchedSelIdx++) {
    schedSel = &param->schedSel[SchedSelIdx];

    /* [ue bit_rate_value_minus1[SchedSelIdx]] */
    if (writeH264NalByteArrayExpGolombCode(nal, schedSel->bit_rate_value_minus1) < 0)
      return -1;

    /* [ue cpb_size_value_minus1[SchedSelIdx]] */
    if (writeH264NalByteArrayExpGolombCode(nal, schedSel->cpb_size_value_minus1) < 0)
      return -1;

    /* [b1 cbr_flag[SchedSelIdx]] */
    if (writeH264NalByteArrayBits(nal, schedSel->cbr_flag, 1) < 0)
      return -1;
  }

  /* [u5 initial_cpb_removal_delay_length_minus1] */
  if (writeH264NalByteArrayBits(nal, param->initial_cpb_removal_delay_length_minus1, 5) < 0)
    return -1;

  /* [u5 cpb_removal_delay_length_minus1] */
  if (writeH264NalByteArrayBits(nal, param->cpb_removal_delay_length_minus1, 5) < 0)
    return -1;

  /* [u5 dpb_output_delay_length_minus1] */
  if (writeH264NalByteArrayBits(nal, param->dpb_output_delay_length_minus1, 5) < 0)
    return -1;

  /* [u5 time_offset_length] */
  if (writeH264NalByteArrayBits(nal, param->time_offset_length, 5) < 0)
    return -1;

  return 0;
}

size_t appendH264SequenceParametersSet(
  H264ParametersHandlerPtr handle,
  size_t insertingOffset,
  H264SPSDataParameters * param
)
{
  int ret;

  size_t writtenBytes;
  unsigned i;

  H264NalHeaderParameters spsNalParam;
  H264NalByteArrayHandlerPtr spsNal;

  H264VuiParameters * vuiParam;
  H264VuiColourDescriptionParameters * colourDesc;

  bool constantSps;

  assert(NULL != handle);
  assert(NULL != param);
  assert(insertingOffset <= 0xFFFFFFFF);

  for (i = 0; i < handle->modNalLst.nbSequenceParametersSet; i++) {
    H264ModifiedNalUnit modNalUnit =
      handle->modNalLst.sequenceParametersSets[i]
    ;

    /* Checking if an identical pre-builded SPS Nal is available. */
    constantSps = constantH264SequenceParametersSetCheck(
      spsH264ModifiedNalUnit(modNalUnit),
      *param
    );

    if (constantSps) {
      /* Compatible, write pre-builded Nal. */
      ret = appendAddDataBlockCommandEsmsHandler(
        handle->esms,
        insertingOffset,
        INSERTION_MODE_OVERWRITE,
        modNalUnit.dataSectionIdx
      );
      if (ret < 0)
        return 0; /* Error */

      return modNalUnit.dataSectionSize;
    }
  }

  /* Build new SPS NAL : */
  spsNalParam.nal_ref_idc = NAL_SPS_REF_IDC;
  spsNalParam.nal_unit_type = NAL_UNIT_TYPE_SEQUENCE_PARAMETER_SET;

  if (NULL == (spsNal = createH264NalByteArrayHandler(spsNalParam)))
    return 0; /* Error */

  /* seq_parameter_set_rbsp() */ {
    /* seq_parameter_set_data() */ {
      /* [u8 profile_idc] */
      if (writeH264NalByteArrayBits(spsNal, param->profile_idc, 8) < 0)
        goto free_return;

      /* [b1 constraint_set0_flag] */
      if (writeH264NalByteArrayBits(spsNal, param->constraint_set_flags.set0, 1) < 0)
        goto free_return;
      /* [b1 constraint_set1_flag] */
      if (writeH264NalByteArrayBits(spsNal, param->constraint_set_flags.set1, 1) < 0)
        goto free_return;
      /* [b1 constraint_set2_flag] */
      if (writeH264NalByteArrayBits(spsNal, param->constraint_set_flags.set2, 1) < 0)
        goto free_return;
      /* [b1 constraint_set3_flag] */
      if (writeH264NalByteArrayBits(spsNal, param->constraint_set_flags.set3, 1) < 0)
        goto free_return;
      /* [b1 constraint_set4_flag] */
      if (writeH264NalByteArrayBits(spsNal, param->constraint_set_flags.set4, 1) < 0)
        goto free_return;
      /* [b1 constraint_set5_flag] */
      if (writeH264NalByteArrayBits(spsNal, param->constraint_set_flags.set5, 1) < 0)
        goto free_return;
      /* [v2 reserved_zero_2bits] */
      if (writeH264NalByteArrayBits(spsNal, param->constraint_set_flags.reservedFlags, 2) < 0)
        goto free_return;

      /* [u8 level_idc] */
      if (writeH264NalByteArrayBits(spsNal, param->level_idc, 8) < 0)
        goto free_return;

      /* [ue seq_parameter_set_id] */
      if (writeH264NalByteArrayExpGolombCode(spsNal, param->seq_parameter_set_id) < 0)
        goto free_return;

      if (isHighScalableMultiviewH264ProfileIdcValue(param->profile_idc)) {
        /* [ue chroma_format_idc] */
        if (writeH264NalByteArrayExpGolombCode(spsNal, param->chroma_format_idc) < 0)
          goto free_return;

        if (param->chroma_format_idc == H264_CHROMA_FORMAT_444) {
          /* [b1 separate_colour_plane_flag] */
          if (writeH264NalByteArrayBits(spsNal, param->separate_colour_plane_flag, 1) < 0)
            goto free_return;
        }

        /* [ue bit_depth_luma_minus8] */
        if (writeH264NalByteArrayExpGolombCode(spsNal, param->bit_depth_luma_minus8) < 0)
          goto free_return;

        /* [ue bit_depth_chroma_minus8] */
        if (writeH264NalByteArrayExpGolombCode(spsNal, param->bit_depth_chroma_minus8) < 0)
          goto free_return;

        /* [b1 qpprime_y_zero_transform_bypass_flag] */
        if (writeH264NalByteArrayBits(spsNal, param->qpprime_y_zero_transform_bypass_flag, 1) < 0)
          goto free_return;

        /* [b1 seq_scaling_matrix_present_flag] */
        if (writeH264NalByteArrayBits(spsNal, param->seq_scaling_matrix_present_flag, 1) < 0)
          goto free_return;

        if (param->seq_scaling_matrix_present_flag) {
          for (i = 0; i < ((param->chroma_format_idc != H264_CHROMA_FORMAT_444) ? 8 : 12); i++) {
            /* [b1 seq_scaling_list_present_flag[i]] */
            if (writeH264NalByteArrayBits(spsNal, param->seq_scaling_matrix.seq_scaling_list_present_flag[i], 1) < 0)
              goto free_return;

            if (param->seq_scaling_matrix.seq_scaling_list_present_flag[i]) {
              /* [vn scaling_list()] */
              if (i < 6) {
                ret = buildH264ScalingList(
                  spsNal,
                  param->seq_scaling_matrix.ScalingList4x4[i],
                  16
                );
              }
              else {
                ret = buildH264ScalingList(
                  spsNal,
                  param->seq_scaling_matrix.ScalingList8x8[i-6],
                  64
                );
              }

              if (ret < 0)
                goto free_return;
            }
          }
        }
      }

      /* [ue log2_max_frame_num_minus4] */
      if (writeH264NalByteArrayExpGolombCode(spsNal, param->log2_max_frame_num_minus4) < 0)
        goto free_return;

      /* [ue pic_order_cnt_type] */
      if (writeH264NalByteArrayExpGolombCode(spsNal, param->pic_order_cnt_type) < 0)
        goto free_return;

      if (param->pic_order_cnt_type == 0) {
        /* [ue log2_max_pic_order_cnt_lsb_minus4] */
        if (writeH264NalByteArrayExpGolombCode(spsNal, param->log2_max_pic_order_cnt_lsb_minus4) < 0)
          goto free_return;
      }
      else if (param->pic_order_cnt_type == 1) {
        /* [b1 delta_pic_order_always_zero_flag] */
        if (writeH264NalByteArrayBits(spsNal, param->delta_pic_order_always_zero_flag, 1) < 0)
          goto free_return;

        /* [se offset_for_non_ref_pic] */
        if (writeH264NalByteArraySignedExpGolombCode(spsNal, param->offset_for_non_ref_pic) < 0)
          goto free_return;

        /* [se offset_for_top_to_bottom_field] */
        if (writeH264NalByteArraySignedExpGolombCode(spsNal, param->offset_for_top_to_bottom_field) < 0)
          goto free_return;

        /* [ue num_ref_frames_in_pic_order_cnt_cycle] */
        if (writeH264NalByteArrayExpGolombCode(spsNal, param->num_ref_frames_in_pic_order_cnt_cycle) < 0)
          goto free_return;

        for (i = 0; i < param->num_ref_frames_in_pic_order_cnt_cycle; i++) {
          /* [se offset_for_ref_frame] */
          if (writeH264NalByteArraySignedExpGolombCode(spsNal, param->offset_for_ref_frame[i]) < 0)
            goto free_return;
        }
      }

      /* [ue max_num_ref_frames] */
      if (writeH264NalByteArrayExpGolombCode(spsNal, param->max_num_ref_frames) < 0)
        goto free_return;

      /* [b1 gaps_in_frame_num_value_allowed_flag] */
      if (writeH264NalByteArrayBits(spsNal, param->gaps_in_frame_num_value_allowed_flag, 1) < 0)
        goto free_return;

      /* [ue pic_width_in_mbs_minus1] */
      if (writeH264NalByteArrayExpGolombCode(spsNal, param->pic_width_in_mbs_minus1) < 0)
        goto free_return;

      /* [ue pic_height_in_map_units_minus1] */
      if (writeH264NalByteArrayExpGolombCode(spsNal, param->pic_height_in_map_units_minus1) < 0)
        goto free_return;

      /* [b1 frame_mbs_only_flag] */
      if (writeH264NalByteArrayBits(spsNal, param->frame_mbs_only_flag, 1) < 0)
        goto free_return;

      if (!param->frame_mbs_only_flag) {
        /* [b1 mb_adaptive_frame_field_flag] */
        if (writeH264NalByteArrayBits(spsNal, param->mb_adaptive_frame_field_flag, 1) < 0)
          goto free_return;
      }

      /* [b1 direct_8x8_inference_flag] */
      if (writeH264NalByteArrayBits(spsNal, param->direct_8x8_inference_flag, 1) < 0)
        goto free_return;

      /* [b1 frame_cropping_flag] */
      if (writeH264NalByteArrayBits(spsNal, param->frame_cropping_flag, 1) < 0)
        goto free_return;

      if (param->frame_cropping_flag) {
        /* [ue frame_crop_left_offset] */
        if (writeH264NalByteArrayExpGolombCode(spsNal, param->frame_crop_offset.left) < 0)
          goto free_return;

        /* [ue frame_crop_right_offset] */
        if (writeH264NalByteArrayExpGolombCode(spsNal, param->frame_crop_offset.right) < 0)
          goto free_return;

        /* [ue frame_crop_top_offset] */
        if (writeH264NalByteArrayExpGolombCode(spsNal, param->frame_crop_offset.top) < 0)
          goto free_return;

        /* [ue frame_crop_bottom_offset] */
        if (writeH264NalByteArrayExpGolombCode(spsNal, param->frame_crop_offset.bottom) < 0)
          goto free_return;
      }

      /* [b1 vui_parameters_present_flag] */
      if (writeH264NalByteArrayBits(spsNal, param->vui_parameters_present_flag, 1) < 0)
        goto free_return;

      if (param->vui_parameters_present_flag) {
        /* vui_parameters() */ {
          vuiParam = &param->vui_parameters;
          colourDesc = &vuiParam->colour_description;

          /* [b1 vui_parameters_present_flag] */
          if (writeH264NalByteArrayBits(spsNal, vuiParam->aspect_ratio_info_present_flag, 1) < 0)
            goto free_return;

          if (param->vui_parameters.aspect_ratio_info_present_flag) {
            /* [u8 aspect_ratio_idc] */
            if (writeH264NalByteArrayBits(spsNal, vuiParam->aspect_ratio_idc, 8) < 0)
              goto free_return;

            if (vuiParam->aspect_ratio_idc == H264_ASPECT_RATIO_IDC_EXTENDED_SAR) {
              /* [u16 sar_width] */
              if (writeH264NalByteArrayBits(spsNal, vuiParam->sar_width, 16) < 0)
                goto free_return;

              /* [u16 sar_height] */
              if (writeH264NalByteArrayBits(spsNal, vuiParam->sar_height, 16) < 0)
                goto free_return;
            }
          }

          /* [b1 overscan_info_present_flag] */
          if (writeH264NalByteArrayBits(spsNal, vuiParam->overscan_info_present_flag, 1) < 0)
            goto free_return;

          if (vuiParam->overscan_info_present_flag) {
            /* [b1 overscan_appropriate_flag] */
            if (writeH264NalByteArrayBits(spsNal, vuiParam->overscan_appropriate_flag, 1) < 0)
              goto free_return;
          }

          /* [b1 video_signal_type_present_flag] */
          if (writeH264NalByteArrayBits(spsNal, vuiParam->video_signal_type_present_flag, 1) < 0)
            goto free_return;

          if (vuiParam->video_signal_type_present_flag) {
            /* [u3 video_format] */
            if (writeH264NalByteArrayBits(spsNal, vuiParam->video_format, 3) < 0)
              goto free_return;

            /* [b1 video_full_range_flag] */
            if (writeH264NalByteArrayBits(spsNal, vuiParam->video_full_range_flag, 1) < 0)
              goto free_return;

            /* [b1 colour_description_present_flag] */
            if (writeH264NalByteArrayBits(spsNal, vuiParam->colour_description_present_flag, 1) < 0)
              goto free_return;

            if (vuiParam->colour_description_present_flag) {
              /* [u8 colour_primaries] */
              if (writeH264NalByteArrayBits(spsNal, colourDesc->colour_primaries, 8) < 0)
                goto free_return;

              /* [u8 transfer_characteristics] */
              if (writeH264NalByteArrayBits(spsNal, colourDesc->transfer_characteristics, 8) < 0)
                goto free_return;

              /* [u8 matrix_coefficients] */
              if (writeH264NalByteArrayBits(spsNal, colourDesc->matrix_coefficients, 8) < 0)
                goto free_return;
            }
          }

          /* [b1 chroma_loc_info_present_flag] */
          if (writeH264NalByteArrayBits(spsNal, vuiParam->chroma_loc_info_present_flag, 1) < 0)
            goto free_return;

          if (vuiParam->chroma_loc_info_present_flag) {
            /* [ue chroma_sample_loc_type_top_field] */
            if (writeH264NalByteArrayExpGolombCode(spsNal, vuiParam->chroma_sample_loc_type_top_field) < 0)
              goto free_return;

            /* [ue chroma_sample_loc_type_bottom_field] */
            if (writeH264NalByteArrayExpGolombCode(spsNal, vuiParam->chroma_sample_loc_type_bottom_field) < 0)
              goto free_return;
          }

          /* [b1 timing_info_present_flag] */
          if (writeH264NalByteArrayBits(spsNal, vuiParam->timing_info_present_flag, 1) < 0)
            goto free_return;

          if (vuiParam->timing_info_present_flag) {
            /* [u32 num_units_in_tick] */
            if (writeH264NalByteArrayBits(spsNal, vuiParam->num_units_in_tick, 32) < 0)
              goto free_return;

            /* [u32 time_scale] */
            if (writeH264NalByteArrayBits(spsNal, vuiParam->time_scale, 32) < 0)
              goto free_return;

            /* [b1 fixed_frame_rate_flag] */
            if (writeH264NalByteArrayBits(spsNal, vuiParam->fixed_frame_rate_flag, 1) < 0)
              goto free_return;
          }

          /* [b1 nal_hrd_parameters_present_flag] */
          if (writeH264NalByteArrayBits(spsNal, vuiParam->nal_hrd_parameters_present_flag, 1) < 0)
            goto free_return;

          if (vuiParam->nal_hrd_parameters_present_flag) {
            /* hrd_parameters() */
            if (buildH264HrdParameters(spsNal, &vuiParam->nal_hrd_parameters) < 0)
              goto free_return;
          }

          /* [b1 vcl_hrd_parameters_present_flag] */
          if (writeH264NalByteArrayBits(spsNal, vuiParam->vcl_hrd_parameters_present_flag, 1) < 0)
            goto free_return;

          if (vuiParam->vcl_hrd_parameters_present_flag) {
            /* hrd_parameters() */
            if (buildH264HrdParameters(spsNal, &vuiParam->vcl_hrd_parameters) < 0)
              goto free_return;
          }

          if (vuiParam->nal_hrd_parameters_present_flag || vuiParam->vcl_hrd_parameters_present_flag) {
            /* [b1 low_delay_hrd_flag] */
            if (writeH264NalByteArrayBits(spsNal, vuiParam->low_delay_hrd_flag, 1) < 0)
              goto free_return;
          }

          /* [b1 pic_struct_present_flag] */
          if (writeH264NalByteArrayBits(spsNal, vuiParam->pic_struct_present_flag, 1) < 0)
            goto free_return;

          /* [b1 bitstream_restriction_flag] */
          if (writeH264NalByteArrayBits(spsNal, vuiParam->bitstream_restriction_flag, 1) < 0)
            goto free_return;

          if (vuiParam->bitstream_restriction_flag) {
            /* [b1 motion_vectors_over_pic_boundaries_flag] */
            if (writeH264NalByteArrayBits(spsNal, vuiParam->bistream_restrictions.motion_vectors_over_pic_boundaries_flag, 1) < 0)
              goto free_return;

            /* [ue max_bytes_per_pic_denom] */
            if (writeH264NalByteArrayExpGolombCode(spsNal, vuiParam->bistream_restrictions.max_bytes_per_pic_denom) < 0)
              goto free_return;

            /* [ue max_bits_per_mb_denom] */
            if (writeH264NalByteArrayExpGolombCode(spsNal, vuiParam->bistream_restrictions.max_bits_per_mb_denom) < 0)
              goto free_return;

            /* [ue log2_max_mv_length_horizontal] */
            if (writeH264NalByteArrayExpGolombCode(spsNal, vuiParam->bistream_restrictions.log2_max_mv_length_horizontal) < 0)
              goto free_return;

            /* [ue log2_max_mv_length_vertical] */
            if (writeH264NalByteArrayExpGolombCode(spsNal, vuiParam->bistream_restrictions.log2_max_mv_length_vertical) < 0)
              goto free_return;

            /* [ue max_num_reorder_frames] */
            if (writeH264NalByteArrayExpGolombCode(spsNal, vuiParam->bistream_restrictions.max_num_reorder_frames) < 0)
              goto free_return;

            /* [ue max_dec_frame_buffering] */
            if (writeH264NalByteArrayExpGolombCode(spsNal, vuiParam->bistream_restrictions.max_dec_frame_buffering) < 0)
              goto free_return;
          }
        }
      }
    }
  }

  /* rbsp_trailing_bits() */
  if (buildH264RbspTrailingBits(spsNal) < 0)
    goto free_return;

  /* lb_print_data(spsNal->array, spsNal->writtenBytes); */
  /* lbc_printf("Written bytes: %" PRIu64 " byte(s).\n", spsNal->writtenBytes); */

  /**
   * Save created unit
   * (or directly inject it if no more data blocks are available).
   */

#if !DISABLE_NAL_REPLACEMENT_DATA_OPTIMIZATION
  if (!isDataBlocksNbLimitReachedEsmsHandler(handle->esms)) {
    H264ModifiedNalUnit * modNalUnit;

    handle->modNalLst.sequenceParametersSets =
      (H264ModifiedNalUnit *) realloc(
        handle->modNalLst.sequenceParametersSets,
        (++handle->modNalLst.nbSequenceParametersSet)
        * sizeof(H264ModifiedNalUnit)
      )
    ;
    if (NULL == handle->modNalLst.sequenceParametersSets)
      LIBBLU_H264_ERROR_RETURN("Memory allocation error.\n");

    modNalUnit = &handle->modNalLst.sequenceParametersSets[
      handle->modNalLst.nbSequenceParametersSet-1
    ];

#if 0
    modNalUnit->linkedParam = (H264SPSDataParameters *) malloc(
      sizeof(H264SPSDataParameters)
    );
    if (NULL == modNalUnit->linkedParam)
      LIBBLU_H264_ERROR_FRETURN("Memory allocation error.\n");

    memcpy(
      modNalUnit->linkedParam,
      param,
      sizeof(H264SPSDataParameters)
    );
#endif

    modNalUnit->linkedParam = ofSpsH264AUNalUnitReplacementData(*param);
    modNalUnit->dataSectionSize = nbBytesH264NalByteArrayHandler(spsNal);

    ret = appendDataBlockEsmsHandler(
      handle->esms,
      spsNal->array,
      nbBytesH264NalByteArrayHandler(spsNal),
      &modNalUnit->dataSectionIdx
    );
    if (ret < 0)
      goto free_return;

    ret = appendAddDataBlockCommandEsmsHandler(
      handle->esms,
      insertingOffset, INSERTION_MODE_OVERWRITE,
      modNalUnit->dataSectionIdx
    );
  }
  else {
    /* Direct NAL injection : */
    ret = appendAddDataCommandEsmsHandler(
      handle->esms,
      insertingOffset,
      INSERTION_MODE_OVERWRITE,
      spsNal->array,
      nbBytesH264NalByteArrayHandler(spsNal)
    );
  }
#else
  /* Direct NAL injection : */
  ret = appendAddDataCommandEsmsHandler(
    handle->esms,
    insertingOffset,
    INSERTION_MODE_OVERWRITE,
    spsNal->array,
    nbBytesH264NalByteArrayHandler(spsNal)
  );
#endif

  if (ret < 0)
    goto free_return;

  writtenBytes = nbBytesH264NalByteArrayHandler(spsNal);
  destroyH264NalByteArrayHandler(spsNal);

  return writtenBytes;

free_return:
  destroyH264NalByteArrayHandler(spsNal);
  return 0;
}

int rebuildH264SPSNalVuiParameters(
  H264SPSDataParameters * spsParam,
  H264ParametersHandlerPtr handle,
  const LibbluESSettingsOptions options
)
{
  H264VuiParameters * vuiParam;
  H264VuiColourDescriptionParameters * colourDesc;

  assert(NULL != spsParam);
  assert(NULL != handle);

  assert(spsParam->vui_parameters_present_flag);
  assert(handle->sequenceParametersSetPresent);

  vuiParam = &spsParam->vui_parameters;
  colourDesc = &vuiParam->colour_description;

  LIBBLU_DEBUG_COM("Applying SPS Nal VUI parameters updating.\n");

  if (handle->curProgParam.useVuiRebuilding) {
    vuiParam->video_signal_type_present_flag = true;
    vuiParam->video_format = H264_VIDEO_FORMAT_NTSC;
    vuiParam->video_full_range_flag = false;
    vuiParam->colour_description_present_flag = true;

    /* Set VUI Color Description parameters : */
    switch (handle->sequenceParametersSet.data.FrameHeight) {
      case 576: /* SD PAL */
        colourDesc->colour_primaries = H264_COLOR_PRIM_BT470BG;
        colourDesc->transfer_characteristics = H264_TRANS_CHAR_BT470BG;
        colourDesc->matrix_coefficients = H264_MATRX_COEF_BT470BG;
        break;

      case 480: /* SD NTSC */
        colourDesc->colour_primaries = H264_COLOR_PRIM_SMPTE170M;
        colourDesc->transfer_characteristics = H264_TRANS_CHAR_SMPTE170M;
        colourDesc->matrix_coefficients = H264_MATRX_COEF_SMPTE170M;
        break;

      default: /* HD */
        colourDesc->colour_primaries = H264_COLOR_PRIM_BT709;
        colourDesc->transfer_characteristics = H264_TRANS_CHAR_BT709;
        colourDesc->matrix_coefficients = H264_MATRX_COEF_BT709;
    }
  }

  if (handle->curProgParam.useVuiUpdate) {
    switch (options.fpsChange) {
      case 0x0: /* No change */
        break;

      case FRAME_RATE_CODE_23976: /* 23.976 */
        vuiParam->num_units_in_tick = 1001;
        vuiParam->time_scale = 48000;
        break;

      case FRAME_RATE_CODE_24: /* 24 */
        vuiParam->num_units_in_tick = 1000;
        vuiParam->time_scale = 48000;
        break;

      case FRAME_RATE_CODE_25: /* 25 */
        vuiParam->num_units_in_tick = 1000;
        vuiParam->time_scale = 50000;
        break;

      case FRAME_RATE_CODE_29970: /* 29.970 */
        vuiParam->num_units_in_tick = 1001;
        vuiParam->time_scale = 60000;
        break;

      case FRAME_RATE_CODE_50: /* 50 */
        vuiParam->num_units_in_tick = 1000;
        vuiParam->time_scale = 100000;
        break;

      case FRAME_RATE_CODE_59940: /* 59.940 */
        vuiParam->num_units_in_tick = 1001;
        vuiParam->time_scale = 120000;
        break;

      default:
        LIBBLU_H264_ERROR_RETURN(
          "Patcher is unable to get FPS destination value.\n"
        );
    }

    if (isUsedLibbluAspectRatioMod(options.arChange)) {
      /* vuiParam->aspect_ratio_info_present_flag = true; */
      vuiParam->aspect_ratio_idc = options.arChange.idc;
      if (options.arChange.idc == H264_ASPECT_RATIO_IDC_EXTENDED_SAR) {
        vuiParam->sar_width = options.arChange.x;
        vuiParam->sar_height = options.arChange.y;
      }
    }

    if (0x00 != options.levelChange) {
      if (
        options.levelChange < spsParam->level_idc
        && !handle->curProgParam.usageOfLowerLevel
      ) {
        LIBBLU_WARNING(
          "Usage of a lower level than initial one, stream may not respect "
          "new level-related constraints.\n"
        );

        /* updateH264LevelLimits(handle, spsParam->level_idc); */
        handle->curProgParam.usageOfLowerLevel = true;
      }

      spsParam->level_idc = options.levelChange;
    }

  } /* if (handle->curProgParam.useVuiUpdate) */

  return 0; /* OK */
}

int patchH264HRDParameters_cpbRemovalDelayLength(
  H264HrdParameters * param,
  unsigned newCpbRemovalDelayLength
)
{
  assert(NULL != param);

  if (!newCpbRemovalDelayLength)
    LIBBLU_H264_ERROR_RETURN(
      "Internal error, forbidden zero-length "
      "'cpb_removal_delay_length' parameter.\n"
    );

  if (32 < newCpbRemovalDelayLength)
    LIBBLU_H264_ERROR_RETURN(
      "Internal error, forbidden 32 bits superior "
      "'cpb_removal_delay_length' parameter.\n"
    );

  param->cpb_removal_delay_length_minus1 = newCpbRemovalDelayLength - 1;

  return 0;
}

int rebuildH264SPSNalVuiHRDParameters(
  H264SPSDataParameters * spsParam
)
{
  int ret;

  assert(NULL != spsParam);
  assert(spsParam->vui_parameters_present_flag);

  LIBBLU_DEBUG_COM("Applying SPS Nal VUI HRD parameters updating.\n");

  if (spsParam->vui_parameters.nal_hrd_parameters_present_flag) {
    ret = patchH264HRDParameters_cpbRemovalDelayLength(
      &spsParam->vui_parameters.nal_hrd_parameters,
      H264_TARGET_INIT_CPB_REM_DELAY_LENGTH
    );
    if (ret < 0)
      return -1;
  }

  if (spsParam->vui_parameters.vcl_hrd_parameters_present_flag) {
    ret = patchH264HRDParameters_cpbRemovalDelayLength(
      &spsParam->vui_parameters.vcl_hrd_parameters,
      H264_TARGET_INIT_CPB_REM_DELAY_LENGTH
    );
    if (ret < 0)
      return -1;
  }

  return 0;
}

int patchH264SequenceParametersSet(
  H264ParametersHandlerPtr handle,
  const LibbluESSettingsOptions options
)
{
  int ret;

  bool update;
  H264SPSDataParameters updatedSpsDataParam;

  assert(NULL != handle);
  assert(handle->sequenceParametersSetPresent);

  update = false;

  if (
    handle->curProgParam.useVuiRebuilding
    || handle->curProgParam.useVuiUpdate
  ) {
    /* SPS VUI parameters fix/updating */
    updatedSpsDataParam = handle->sequenceParametersSet.data;

    if (!updatedSpsDataParam.vui_parameters_present_flag)
      LIBBLU_ERROR_RETURN(
        "Unable to patch SPS, missing VUI parameters.\n"
      );

    ret = rebuildH264SPSNalVuiParameters(
      &updatedSpsDataParam, handle, options
    );
    if (ret < 0)
      return -1;

    update = true;
  }

  if (options.forceRebuildSei) {
    /* SPS HRD parameters check (updating fields lengths) */
    if (!update)
      updatedSpsDataParam = handle->sequenceParametersSet.data;

    if (rebuildH264SPSNalVuiHRDParameters(&updatedSpsDataParam) < 0)
      return -1;

    update = true;
  }

  if (update) {
    return replaceCurNalCell(
      handle,
      ofSpsH264AUNalUnitReplacementData(updatedSpsDataParam)
    );
  }

  return 0; /* OK, no patching required. */
}

#if 0
int buildH264SeiBufferingPeriodMessage(
  H264ParametersHandlerPtr handle,
  H264NalByteArrayHandlerPtr seiNal,
  H264SeiBufferingPeriod * param
)
{
  /* buffering_period() */
  unsigned SchedSelIdx, maxSchedSelIdx;
  unsigned fieldLength;

  H264VuiParameters * vuiParam;
  H264SeiBufferingPeriodSchedSel * hrdParam;

  assert(NULL != handle);
  assert(NULL != seiNal);
  assert(NULL != param);

  /**
   * Check for missing VUI parameters,
   * if so, Buffering Period SEI message shouldn't be present.
   */

  if (!handle->sequenceParametersSetPresent)
    LIBBLU_H264_ERROR_RETURN(
      "Unable to patch SEI Buffering Period message, missing SPS.\n"
    );

  if (!handle->sequenceParametersSet.data.vui_parameters_present_flag)
    LIBBLU_H264_ERROR_RETURN(
      "Unable to patch SEI Buffering Period message, "
      "missing VUI parameters in SPS.\n"
    );

  vuiParam = &handle->sequenceParametersSet.data.vui_parameters;

  /**
   * Check for NalHrdBpPresentFlag and VclHrdBpPresentFlag equal to 0b0,
   * if so, Buffering Period SEI message shouldn't be present.
   */

  if (!vuiParam->nal_hrd_parameters_present_flag && !vuiParam->vcl_hrd_parameters_present_flag)
    LIBBLU_H264_ERROR_RETURN(
      "Unable to patch SEI Buffering Period message, "
      "missing HRD settings in VUI parameters.\n"
    );

  /* [ue seq_parameter_set_id] */
  if (writeH264NalByteArrayExpGolombCode(seiNal, param->seq_parameter_set_id) < 0)
    return -1;

  if (vuiParam->nal_hrd_parameters_present_flag) {
    maxSchedSelIdx = vuiParam->nal_hrd_parameters.cpb_cnt_minus1 + 1;
    fieldLength = vuiParam->nal_hrd_parameters.initial_cpb_removal_delay_length_minus1 + 1;

    for (SchedSelIdx = 0; SchedSelIdx < maxSchedSelIdx; SchedSelIdx++) {
      hrdParam = param->nal_hrd_parameters + SchedSelIdx;

      /* [u<initial_cpb_removal_delay_length_minus1 + 1> initial_cpb_removal_delay[SchedSelIdx]] */
      if (writeH264NalByteArrayBits(seiNal, hrdParam->initial_cpb_removal_delay, fieldLength) < 0)
        return -1;

      /* [u<initial_cpb_removal_delay_length_minus1 + 1> initial_cpb_removal_delay_offset[SchedSelIdx]] */
      if (writeH264NalByteArrayBits(seiNal, hrdParam->initial_cpb_removal_delay_offset, fieldLength) < 0)
        return -1;
    }
  }

  if (vuiParam->vcl_hrd_parameters_present_flag) {
    maxSchedSelIdx = vuiParam->vcl_hrd_parameters.cpb_cnt_minus1 + 1;
    fieldLength = vuiParam->vcl_hrd_parameters.initial_cpb_removal_delay_length_minus1 + 1;

    for (SchedSelIdx = 0; SchedSelIdx < maxSchedSelIdx; SchedSelIdx++) {
      hrdParam = param->vcl_hrd_parameters + SchedSelIdx;

      /* [u<initial_cpb_removal_delay_length_minus1 + 1> initial_cpb_removal_delay[SchedSelIdx]] */
      if (writeH264NalByteArrayBits(seiNal, hrdParam->initial_cpb_removal_delay, fieldLength) < 0)
        return -1;

      /* [u<initial_cpb_removal_delay_length_minus1 + 1> initial_cpb_removal_delay_offset[SchedSelIdx]] */
      if (writeH264NalByteArrayBits(seiNal, hrdParam->initial_cpb_removal_delay_offset, fieldLength) < 0)
        return -1;
    }
  }

  return 0; /* OK */
}

int buildH264SeiMessage(
  H264ParametersHandlerPtr handle,
  H264NalByteArrayHandlerPtr seiNal,
  H264SeiMessageParameters * param
)
{
  /* sei_message() */
  int ret;

  size_t bigValue;

  assert(NULL != handle);
  assert(NULL != seiNal);
  assert(NULL != param);

  for (bigValue = param->payloadType; 255 < bigValue; bigValue -= 255) {
    /* [v8 ff_byte] */
    if (writeH264NalByteArrayBits(seiNal, 0xFF, 8) < 0)
      return -1;
  }

  /* [u8 last_payload_type_byte] */
  if (writeH264NalByteArrayBits(seiNal, bigValue, 8) < 0)
    return -1;

  for (bigValue = param->payloadSize; 255 < bigValue; bigValue -= 255) {
    /* [v8 ff_byte] */
    if (writeH264NalByteArrayBits(seiNal, 0xFF, 8) < 0)
      return -1;
  }

  /* [u8 last_payload_size_byte] */
  if (writeH264NalByteArrayBits(seiNal, bigValue, 8) < 0)
    return -1;

  /* sei_payload() */
  switch (param->payloadType) {
    case H264_SEI_TYPE_BUFFERING_PERIOD:
      ret = buildH264SeiBufferingPeriodMessage(
        handle, seiNal, &param->bufferingPeriod
      );
      break;

    default:
      LIBBLU_ERROR_RETURN(
        "Internal error, missing building rules for SEI %s message type.\n",
        H264SEIMessagePayloadTypeStr(param->payloadType)
      );
  }

  return ret;
}

int buildH264SupplementalEnhancementInformation(
  H264ParametersHandlerPtr handle,
  H264NalByteArrayHandlerPtr seiNal,
  H264SeiRbspParameters * param
)
{
  /* sei_rbsp() */
  unsigned msgIdx;

  assert(NULL != handle);
  assert(NULL != seiNal);
  assert(NULL != param);

  for (msgIdx = 0; msgIdx < param->messagesNb; msgIdx++) {
    /* sei_message() */
    if (buildH264SeiMessage(handle, seiNal, &param->messages[msgIdx]) < 0)
      return -1;
  }

  /* rbsp_trailing_bits() */
  if (buildH264RbspTrailingBits(seiNal) < 0)
    return -1;

  return 0; /* OK */
}

bool isH264SeiBufferingPeriodPatchMessage(
  const H264SeiRbspParameters * param
)
{
  assert(NULL != param);

  return
    param->messagesNb == 1
    && param->messages[0].payloadType == H264_SEI_TYPE_BUFFERING_PERIOD
  ;
}

size_t appendH264Sei(
  H264ParametersHandlerPtr handle,
  EsmsHandlerPtr handle->esms,
  size_t insertingOffset,
  H264SeiRbspParameters * param
)
{
  int ret;

  size_t writtenBytes;

  H264NalHeaderParameters seiNalParam;
  H264NalByteArrayHandlerPtr seiNal;

  assert(NULL != handle);
  assert(NULL != param);
  assert(insertingOffset <= 0xFFFFFFFF);

  /* Build SEI NAL : */
  seiNalParam = (H264NalHeaderParameters) {
    .nal_ref_idc = NAL_SEI_REF_IDC,
    .nal_unit_type = NAL_UNIT_TYPE_SUPPLEMENTAL_ENHANCEMENT_INFORMATION
  };

  if (NULL == (seiNal = createH264NalByteArrayHandler(seiNalParam)))
    return 0; /* Error */

  /* sei_rbsp() */
  ret = buildH264SupplementalEnhancementInformation(
    handle, seiNal, param
  );
  if (ret < 0)
    return 0;

  /* lb_print_data(seiNal->array, seiNal->writtenBytes); */
  /* lbc_printf("Written bytes: %" PRIu64 " byte(s).\n", seiNal->writtenBytes); */

  /* Direct NAL injection : */
  ret = appendAddDataCommandEsmsHandler(
    handle->esms,
    insertingOffset,
    INSERTION_MODE_OVERWRITE,
    seiNal->array,
    nbBytesH264NalByteArrayHandler(seiNal)
  );
  if (ret < 0)
    return 0;

  /* exit(0); */
  writtenBytes = nbBytesH264NalByteArrayHandler(seiNal);
  destroyH264NalByteArrayHandler(seiNal);

  return writtenBytes;
}

size_t appendH264SeiBufferingPeriodPlaceHolder(
  H264ParametersHandlerPtr handle,
  EsmsHandlerPtr handle->esms,
  size_t insertingOffset,
  H264SeiRbspParameters * param
)
{
  /**
   * TODO: Merge appendH264Sei and appendH264SeiBufferingPeriodPlaceHolder
   * functions.
   */
  int ret;

  H264NalHeaderParameters seiNalParam;
  H264NalByteArrayHandlerPtr seiNal;

  /* Pointers to saved parameters in H264ParametersHandlerPtr struct : */
  H264ModifiedNalUnit * seiModNalUnit;
  bool * seiModNalUnitPres;

  assert(NULL != handle);
  assert(NULL != param);
  assert(insertingOffset <= 0xFFFFFFFF);

  seiModNalUnit = &handle->modNalLst.bufferingPeriodSeiMsg;
  seiModNalUnitPres = &handle->modNalLst.patchBufferingPeriodSeiPresent;

  if (!*seiModNalUnitPres) {
    assert(NULL == seiModNalUnit->linkedParam);

    /* Build SEI NAL : */
    seiNalParam = (H264NalHeaderParameters) {
      .nal_ref_idc = NAL_SEI_REF_IDC,
      .nal_unit_type = NAL_UNIT_TYPE_SUPPLEMENTAL_ENHANCEMENT_INFORMATION
    };

    if (NULL == (seiNal = createH264NalByteArrayHandler(seiNalParam)))
      return 0; /* Error */

    /* sei_rbsp() */
    ret = buildH264SupplementalEnhancementInformation(
      handle, seiNal, param
    );
    if (ret < 0)
      return 0;

    seiModNalUnit->linkedParam = (H264SeiRbspParameters *) malloc(
      sizeof(H264SeiRbspParameters)
    );
    if (NULL == seiModNalUnit->linkedParam)
      LIBBLU_H264_ERROR_RETURN("Memory allocation error.\n");
    memcpy(seiModNalUnit->linkedParam, param, sizeof(H264SeiRbspParameters));

    seiModNalUnit->length = nbBytesH264NalByteArrayHandler(seiNal);

    ret = appendDataBlockEsmsHandler(
      handle->esms,
      seiNal->array,
      nbBytesH264NalByteArrayHandler(seiNal),
      &seiModNalUnit->dataSectionIdx
    );
    if (ret < 0)
      return 0;

    *seiModNalUnitPres = true;
    destroyH264NalByteArrayHandler(seiNal);
  }

  assert(NULL != seiModNalUnit->linkedParam);

  ret = appendAddDataBlockCommandEsmsHandler(
    handle->esms,
    insertingOffset,
    INSERTION_MODE_OVERWRITE,
    seiModNalUnit->dataSectionIdx
  );
  if (ret < 0)
    return 0; /* Error */

  /* LIBBLU_ERROR("Write %zu bytes.\n", seiModNalUnit->length); */

  return seiModNalUnit->length;
}

int patchH264SeiBufferingPeriodMessageParameters(
  H264ParametersHandlerPtr handle,
  H264SeiMessageParameters * seiMessage,
  const unsigned seq_parameter_set_id,
  const H264SeiBufferingPeriodSchedSel * hrdParam,
  const H264SeiBufferingPeriodSchedSel * vclParam
)
{
  size_t messageLen;
  unsigned cpbConfig, nbCpbConfigs;
  bool paddingRequired;

  H264VuiParameters * vuiParam;

  H264SeiBufferingPeriod * param;

  assert(NULL != handle);
  assert(NULL != seiMessage);

  LIBBLU_DEBUG_COM(
    "Applying SEI Nal buffering_period message parameters updating.\n"
  );

  /**
   * Check for missing VUI parameters,
   * if so, Buffering Period SEI message shouldn't be present.
   */

  if (!handle->sequenceParametersSetPresent)
    LIBBLU_H264_ERROR_RETURN(
      "Unable to patch SEI Buffering Period message, missing SPS.\n"
    );

  if (!handle->sequenceParametersSet.data.vui_parameters_present_flag)
    LIBBLU_H264_ERROR_RETURN(
      "Unable to patch SEI Buffering Period message, "
      "missing VUI parameters in SPS.\n"
    );

  vuiParam = &handle->sequenceParametersSet.data.vui_parameters;

  /**
   * Check for NalHrdBpPresentFlag and VclHrdBpPresentFlag equal to 0b0,
   * if so, Buffering Period SEI message shouldn't be present.
   */

  if (!vuiParam->nal_hrd_parameters_present_flag && !vuiParam->vcl_hrd_parameters_present_flag)
    LIBBLU_H264_ERROR_RETURN(
      "Unable to patch SEI Buffering Period message, "
      "missing HRD settings in VUI parameters.\n"
    );

  seiMessage->payloadType = H264_SEI_TYPE_BUFFERING_PERIOD;
  param = &seiMessage->bufferingPeriod;

  messageLen = 0;

  param->seq_parameter_set_id = seq_parameter_set_id;
  messageLen += calcH264ExpGolombCodeLength(seq_parameter_set_id, false);

  nbCpbConfigs = vuiParam->nal_hrd_parameters.cpb_cnt_minus1 + 1;
  for (cpbConfig = 0; cpbConfig < nbCpbConfigs; cpbConfig++) {
    param->nal_hrd_parameters[cpbConfig] = hrdParam[cpbConfig];
    messageLen += (vuiParam->nal_hrd_parameters.initial_cpb_removal_delay_length_minus1 + 1) * 2;
  }

  nbCpbConfigs = vuiParam->vcl_hrd_parameters.cpb_cnt_minus1 + 1;
  for (cpbConfig = 0; cpbConfig < nbCpbConfigs; cpbConfig++) {
    param->vcl_hrd_parameters[cpbConfig] = vclParam[cpbConfig];
    messageLen += (vuiParam->nal_hrd_parameters.initial_cpb_removal_delay_length_minus1 + 1) * 2;
  }

  paddingRequired = (messageLen & 0x7);
  messageLen >>= 3; /* Bits to bytes round down. */

  if (paddingRequired)
    messageLen++;

  seiMessage->payloadSize = messageLen;

  return 0;
}

int insertH264SeiBufferingPeriodPlaceHolder(
  H264ParametersHandlerPtr handle
)
{
  int ret;

  H264SeiRbspParameters newSeiNalParam;

  H264SeiBufferingPeriodSchedSel nalHrd[1];
  H264SeiBufferingPeriodSchedSel vclHrd[1];

  H264AUNalUnitPtr nalUnit;

  assert(NULL != handle);
  assert(handle->sequenceParametersSetPresent);

  /* Setting default empty SEI parameters : */

  nalHrd[0] = (H264SeiBufferingPeriodSchedSel) {
    .initial_cpb_removal_delay = 0,
    .initial_cpb_removal_delay_offset = 0,
  };
  vclHrd[0] = (H264SeiBufferingPeriodSchedSel) {
    .initial_cpb_removal_delay = 0,
    .initial_cpb_removal_delay_offset = 0,
  };
  newSeiNalParam.messagesNb = 1;

  /* Build H264SeiRbspParameters structure : */
  ret = patchH264SeiBufferingPeriodMessageParameters(
    handle,
    &newSeiNalParam.messages[0],
    0x0,
    nalHrd, vclHrd
  );
  if (ret < 0)
    return -1;

  /* Creating new NALU to receive created SEI. */
  nalUnit = createNewNalCell(
    handle,
    NAL_UNIT_TYPE_SUPPLEMENTAL_ENHANCEMENT_INFORMATION
  );
  if (NULL == nalUnit)
    return -1;

  /* Replace empty NALU parameters with created ones. */
  ret = replaceCurNalCell(
    handle,
    &newSeiNalParam,
    sizeof(H264SeiRbspParameters)
  );
  if (ret < 0)
    return -1;

  return addNalCellToAccessUnit(handle); /* Push created NALU in AU pile. */
}

int completeH264SeiBufferingPeriodComputation(
  H264ParametersHandlerPtr handle,
  EsmsHandlerPtr handle->esms
)
{
  /* Only support first one schedSel. */
  /* BUG: Computed values are broken, wrong method. */
  int ret;

  H264VuiParameters * vuiParam; /* Shorter-pointer. */
  uint8_t cpbRemovalDelayFieldLength;
  uint32_t maxAllowedCpbRemovalDelayValue;

  /* Network Abstraction Layer Hypothetical Reference Decoder (NAL-HRD) */
  bool nalHrdPresent;
  double nalBitRate, nalCpbSize, nalMaxCpbBufferingDelay, nalResultInitCpbRemovalDelay;

  /* Video Coding Layer Hypothetical Reference Decoder (VCL-HRD) */
  bool vclHrdPresent;
  double vclBitRate, vclCpbSize, vclMaxCpbBufferingDelay, vclResultInitCpbRemovalDelay;

  H264SeiRbspParameters newSeiNalParam;

  H264SeiBufferingPeriodSchedSel nalHrd[1];
  H264SeiBufferingPeriodSchedSel vclHrd[1];

  /* Pointer to saved parameter in H264ParametersHandlerPtr struct : */
  H264ModifiedNalUnit * seiModNalUnit;

  H264NalHeaderParameters seiNalParam;
  H264NalByteArrayHandlerPtr seiNal;

  assert(NULL != handle);

  if (!handle->modNalLst.patchBufferingPeriodSeiPresent)
    return 0; /* Nothing to do. */

  /* All AU in stream cannot be zero-size... */
  assert(0 != handle->curProgParam.largestAUSize);
  assert(0 != handle->curProgParam.largestIPicAUSize);

  /* Usually, required parameters presence has been checked before. */
  assert(handle->sequenceParametersSetPresent);
  assert(handle->sequenceParametersSet.data.vui_parameters_present_flag);

  vuiParam = &handle->sequenceParametersSet.data.vui_parameters;
  nalHrdPresent = vuiParam->nal_hrd_parameters_present_flag;
  vclHrdPresent = vuiParam->vcl_hrd_parameters_present_flag;

  assert(nalHrdPresent || vclHrdPresent);

  /* NAL-HRD computation : */
  if (nalHrdPresent) {
    cpbRemovalDelayFieldLength = vuiParam->nal_hrd_parameters.initial_cpb_removal_delay_length_minus1 + 1;
    maxAllowedCpbRemovalDelayValue = (2 << (cpbRemovalDelayFieldLength - 1)) - 1; /* Quick pow(2, cpbRemovalDelayFieldLength) - 1 */

    nalBitRate = vuiParam->nal_hrd_parameters.schedSel[0].BitRate;

    nalMaxCpbBufferingDelay = handle->curProgParam.largestAUSize * 8l / nalBitRate;
    lbc_printf("Max CPB Buffering delay: %f.\n", nalMaxCpbBufferingDelay);

    nalResultInitCpbRemovalDelay = ceil(nalMaxCpbBufferingDelay * 90000);
    lbc_printf("Result computed initial CPB removal delay: %f.\n", nalResultInitCpbRemovalDelay);

    nalCpbSize = vuiParam->nal_hrd_parameters.schedSel[0].CpbSize;

    if (nalCpbSize < nalResultInitCpbRemovalDelay * nalBitRate / 90000)
      LIBBLU_ERROR_RETURN("Global CPB overflow occurs during NAL-HRD initial_cpb_removal_delay computation.\n");

    if (90000 < nalResultInitCpbRemovalDelay) {
      /* Considering initial CPB buffering cannot be superior to one second. */
      LIBBLU_ERROR_RETURN("Computed initial_cpb_removal_delay exceed one second.\n");
    }

    if (maxAllowedCpbRemovalDelayValue < nalResultInitCpbRemovalDelay)
      LIBBLU_ERROR_RETURN(
        "Final initial_cpb_removal_delay value cannot be stored in %" PRIu8 " bits.\n",
        cpbRemovalDelayFieldLength
      );
  }
  else
    nalResultInitCpbRemovalDelay = 0;

  if (vclHrdPresent) {
    if (!nalHrdPresent) {
      cpbRemovalDelayFieldLength = vuiParam->vcl_hrd_parameters.initial_cpb_removal_delay_length_minus1 + 1;
      maxAllowedCpbRemovalDelayValue = (2 << (cpbRemovalDelayFieldLength - 1)) - 1; /* Quick pow(2, cpbRemovalDelayFieldLength) - 1 */
    }
    else
      assert(cpbRemovalDelayFieldLength == vuiParam->vcl_hrd_parameters.initial_cpb_removal_delay_length_minus1 + 1);

    vclBitRate = vuiParam->vcl_hrd_parameters.schedSel[0].BitRate;

    vclMaxCpbBufferingDelay = handle->curProgParam.largestAUSize * 8l / vclBitRate;
    lbc_printf("Max CPB Buffering delay: %f.\n", vclMaxCpbBufferingDelay);

    vclResultInitCpbRemovalDelay = ceil(vclMaxCpbBufferingDelay * 90000);
    lbc_printf("Result computed initial CPB removal delay: %f.\n", vclResultInitCpbRemovalDelay);

    vclCpbSize = vuiParam->vcl_hrd_parameters.schedSel[0].CpbSize;

    if (vclCpbSize < vclResultInitCpbRemovalDelay * vclBitRate / 90000)
      LIBBLU_ERROR_RETURN("Global CPB overflow occurs during NAL-HRD initial_cpb_removal_delay computation.\n");

    if (90000 < vclResultInitCpbRemovalDelay)
      /* Considering initial CPB buffering cannot be superior to one second. */
      LIBBLU_H264_ERROR_RETURN(
        "Computed 'initial_cpb_removal_delay' exceed one second "
        "(%.2f second(s), %f ticks).\n",
        vclResultInitCpbRemovalDelay / 90000,
        vclResultInitCpbRemovalDelay
      );

    if (maxAllowedCpbRemovalDelayValue < vclResultInitCpbRemovalDelay)
      LIBBLU_H264_ERROR_RETURN(
        "Final initial_cpb_removal_delay value cannot be stored "
        "in %" PRIu8 " bits.\n",
        cpbRemovalDelayFieldLength
      );

    /* TODO: Check A.3.1.a) constraint. */
  }
  else
    vclResultInitCpbRemovalDelay = 0;

  /* Pack computed replacementParameters parameters : */
  nalHrd[0].initial_cpb_removal_delay = nalResultInitCpbRemovalDelay;
  nalHrd[0].initial_cpb_removal_delay_offset = 0;
  vclHrd[0].initial_cpb_removal_delay = vclResultInitCpbRemovalDelay;
  vclHrd[0].initial_cpb_removal_delay_offset = 0;
  newSeiNalParam.messagesNb = 1;

  if (
    patchH264SeiBufferingPeriodMessageParameters(
      handle,
      &newSeiNalParam.messages[0],
      0x0,
      nalHrd, vclHrd
    ) < 0
  )
    return -1;

  /* Build updated SEI NAL : */
  seiNalParam.nal_ref_idc = NAL_SEI_REF_IDC;
  seiNalParam.nal_unit_type = NAL_UNIT_TYPE_SUPPLEMENTAL_ENHANCEMENT_INFORMATION;

  if (NULL == (seiNal = createH264NalByteArrayHandler(seiNalParam)))
    return -1; /* Error */

  /* sei_rbsp() */
  if (buildH264SupplementalEnhancementInformation(handle, seiNal, &newSeiNalParam) < 0)
    return -1;

  seiModNalUnit = &handle->modNalLst.bufferingPeriodSeiMsg;

  if (seiModNalUnit->length < nbBytesH264NalByteArrayHandler(seiNal))
    LIBBLU_H264_ERROR_RETURN(
      "Final Buffering Period SEI NALU is bigger than place holder.\n"
    );

  /* Replace place-holder NALU with updated one. */
  ret = updateDataBlockEsmsHandler(
    handle->esms,
    seiNal->array,
    nbBytesH264NalByteArrayHandler(seiNal),
    seiModNalUnit->dataSectionIdx
  );
  if (ret < 0)
    return -1;

  destroyH264NalByteArrayHandler(seiNal);

  return 0; /* OK */
}
#endif
