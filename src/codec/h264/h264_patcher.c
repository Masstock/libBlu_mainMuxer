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
  if (writeH264NalByteArrayExpGolombCode(nal, param->cpbCntMinus1) < 0)
    return -1;

  /* [u4 bit_rate_scale] */
  if (writeH264NalByteArrayBits(nal, param->bitRateScale, 4) < 0)
    return -1;

  /* [u4 cpb_size_scale] */
  if (writeH264NalByteArrayBits(nal, param->cpbSizeScale, 4) < 0)
    return -1;

  for (SchedSelIdx = 0; SchedSelIdx <= param->cpbCntMinus1; SchedSelIdx++) {
    schedSel = &param->schedSel[SchedSelIdx];

    /* [ue bit_rate_value_minus1[SchedSelIdx]] */
    if (writeH264NalByteArrayExpGolombCode(nal, schedSel->bitRateValueMinus1) < 0)
      return -1;

    /* [ue cpb_size_value_minus1[SchedSelIdx]] */
    if (writeH264NalByteArrayExpGolombCode(nal, schedSel->cpbSizeValueMinus1) < 0)
      return -1;

    /* [b1 cbr_flag[SchedSelIdx]] */
    if (writeH264NalByteArrayBits(nal, schedSel->cbrFlag, 1) < 0)
      return -1;
  }

  /* [u5 initial_cpb_removal_delay_length_minus1] */
  if (writeH264NalByteArrayBits(nal, param->initialCpbRemovalDelayLengthMinus1, 5) < 0)
    return -1;

  /* [u5 cpb_removal_delay_length_minus1] */
  if (writeH264NalByteArrayBits(nal, param->cpbRemovalDelayLengthMinus1, 5) < 0)
    return -1;

  /* [u5 dpb_output_delay_length_minus1] */
  if (writeH264NalByteArrayBits(nal, param->dpbOutputDelayLengthMinus1, 5) < 0)
    return -1;

  /* [u5 time_offset_length] */
  if (writeH264NalByteArrayBits(nal, param->timeOffsetLength, 5) < 0)
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
  H264ModifiedNalUnit * modNalUnit;

  bool constantSps;

  assert(NULL != handle);
  assert(NULL != param);
  assert(insertingOffset <= 0xFFFFFFFF);

  for (i = 0; i < handle->modNalLst.nbSequenceParametersSet; i++) {
    modNalUnit = handle->modNalLst.sequenceParametersSets + i;

    /* Checking if an identical pre-builded SPS Nal is available. */
    constantSps = constantH264SequenceParametersSetCheck(
      *((H264SPSDataParameters *) modNalUnit->linkedParam),
      *param
    );

    if (constantSps) {
      /* Compatible, write pre-builded Nal. */
      ret = appendAddDataBlockCommand(
        handle->esms,
        insertingOffset,
        INSERTION_MODE_ERASE,
        modNalUnit->dataSectionIdx
      );
      if (ret < 0)
        return 0; /* Error */
      return modNalUnit->length;
    }
  }

  /* Build new SPS NAL : */
  spsNalParam.nalRefIdc = NAL_SEQUENCE_PARAMETERS_SET_REF_IDC;
  spsNalParam.nalUnitType = NAL_UNIT_TYPE_SEQUENCE_PARAMETERS_SET;

  if (NULL == (spsNal = createH264NalByteArrayHandler(spsNalParam)))
    return 0; /* Error */

  /* seq_parameter_set_rbsp() */ {
    /* seq_parameter_set_data() */ {
      /* [u8 profile_idc] */
      if (writeH264NalByteArrayBits(spsNal, param->profileIdc, 8) < 0)
        return 0;

      /* [b1 constraint_set0_flag] */
      if (writeH264NalByteArrayBits(spsNal, param->constraintFlags.set0, 1) < 0)
        return 0;
      /* [b1 constraint_set1_flag] */
      if (writeH264NalByteArrayBits(spsNal, param->constraintFlags.set1, 1) < 0)
        return 0;
      /* [b1 constraint_set2_flag] */
      if (writeH264NalByteArrayBits(spsNal, param->constraintFlags.set2, 1) < 0)
        return 0;
      /* [b1 constraint_set3_flag] */
      if (writeH264NalByteArrayBits(spsNal, param->constraintFlags.set3, 1) < 0)
        return 0;
      /* [b1 constraint_set4_flag] */
      if (writeH264NalByteArrayBits(spsNal, param->constraintFlags.set4, 1) < 0)
        return 0;
      /* [b1 constraint_set5_flag] */
      if (writeH264NalByteArrayBits(spsNal, param->constraintFlags.set5, 1) < 0)
        return 0;
      /* [v2 reserved_zero_2bits] */
      if (writeH264NalByteArrayBits(spsNal, param->constraintFlags.reservedFlags, 2) < 0)
        return 0;

      /* [u8 level_idc] */
      if (writeH264NalByteArrayBits(spsNal, param->levelIdc, 8) < 0)
        return 0;

      /* [ue seq_parameter_set_id] */
      if (writeH264NalByteArrayExpGolombCode(spsNal, param->seqParametersSetId) < 0)
        return 0;

      if (H264_PROFILE_IS_HIGH(param->profileIdc)) {
        /* [ue chroma_format_idc] */
        if (writeH264NalByteArrayExpGolombCode(spsNal, param->chromaFormat) < 0)
          return 0;

        if (param->chromaFormat == H264_CHROMA_FORMAT_444) {
          /* [b1 separate_colour_plane_flag] */
          if (writeH264NalByteArrayBits(spsNal, param->separateColourPlaneFlag444, 1) < 0)
            return 0;
        }

        /* [ue bit_depth_luma_minus8] */
        if (writeH264NalByteArrayExpGolombCode(spsNal, param->bitDepthLumaMinus8) < 0)
          return 0;

        /* [ue bit_depth_chroma_minus8] */
        if (writeH264NalByteArrayExpGolombCode(spsNal, param->bitDepthChromaMinus8) < 0)
          return 0;

        /* [b1 qpprime_y_zero_transform_bypass_flag] */
        if (writeH264NalByteArrayBits(spsNal, param->qpprimeYZeroTransformBypass, 1) < 0)
          return 0;

        /* [b1 seq_scaling_matrix_present_flag] */
        if (writeH264NalByteArrayBits(spsNal, param->seqScalingMatrixPresent, 1) < 0)
          return 0;

        if (param->seqScalingMatrixPresent) {
          for (i = 0; i < ((param->chromaFormat != H264_CHROMA_FORMAT_444) ? 8 : 12); i++) {
            /* [b1 seq_scaling_list_present_flag[i]] */
            if (writeH264NalByteArrayBits(spsNal, param->seqScalingMatrix.seqScalingListPresent[i], 1) < 0)
              return 0;

            if (param->seqScalingMatrix.seqScalingListPresent[i]) {
              /* [vn scaling_list()] */
              if (i < 6) {
                ret = buildH264ScalingList(
                  spsNal,
                  param->seqScalingMatrix.scalingList4x4[i],
                  16
                );
              }
              else {
                ret = buildH264ScalingList(
                  spsNal,
                  param->seqScalingMatrix.scalingList8x8[i-6],
                  64
                );
              }

              if (ret < 0)
                return 0;
            }
          }
        }
      }

      /* [ue log2_max_frame_num_minus4] */
      if (writeH264NalByteArrayExpGolombCode(spsNal, param->log2MaxFrameNumMinus4) < 0)
        return 0;

      /* [ue pic_order_cnt_type] */
      if (writeH264NalByteArrayExpGolombCode(spsNal, param->picOrderCntType) < 0)
        return 0;

      if (param->picOrderCntType == 0) {
        /* [ue log2_max_pic_order_cnt_lsb_minus4] */
        if (writeH264NalByteArrayExpGolombCode(spsNal, param->log2MaxPicOrderCntLsbMinus4) < 0)
          return 0;
      }
      else if (param->picOrderCntType == 1) {
        /* [b1 delta_pic_order_always_zero_flag] */
        if (writeH264NalByteArrayBits(spsNal, param->deltaPicOrderAlwaysZero, 1) < 0)
          return 0;

        /* [se offset_for_non_ref_pic] */
        if (writeH264NalByteArraySignedExpGolombCode(spsNal, param->offsetForNonRefPic) < 0)
          return 0;

        /* [se offset_for_top_to_bottom_field] */
        if (writeH264NalByteArraySignedExpGolombCode(spsNal, param->offsetForTopToBottomField) < 0)
          return 0;

        /* [ue num_ref_frames_in_pic_order_cnt_cycle] */
        if (writeH264NalByteArrayExpGolombCode(spsNal, param->numRefFramesInPicOrderCntCycle) < 0)
          return 0;

        for (i = 0; i < param->numRefFramesInPicOrderCntCycle; i++) {
          /* [se offset_for_ref_frame] */
          if (writeH264NalByteArraySignedExpGolombCode(spsNal, param->offsetForRefFrame[i]) < 0)
            return 0;
        }
      }

      /* [ue max_num_ref_frames] */
      if (writeH264NalByteArrayExpGolombCode(spsNal, param->maxNumRefFrames) < 0)
        return 0;

      /* [b1 gaps_in_frame_num_value_allowed_flag] */
      if (writeH264NalByteArrayBits(spsNal, param->gapsInFrameNumValueAllowed, 1) < 0)
        return 0;

      /* [ue pic_width_in_mbs_minus1] */
      if (writeH264NalByteArrayExpGolombCode(spsNal, param->picWidthInMbsMinus1) < 0)
        return 0;

      /* [ue pic_height_in_map_units_minus1] */
      if (writeH264NalByteArrayExpGolombCode(spsNal, param->picHeightInMapUnitsMinus1) < 0)
        return 0;

      /* [b1 frame_mbs_only_flag] */
      if (writeH264NalByteArrayBits(spsNal, param->frameMbsOnly, 1) < 0)
        return 0;

      if (!param->frameMbsOnly) {
        /* [b1 mb_adaptive_frame_field_flag] */
        if (writeH264NalByteArrayBits(spsNal, param->mbAdaptativeFrameField, 1) < 0)
          return 0;
      }

      /* [b1 direct_8x8_inference_flag] */
      if (writeH264NalByteArrayBits(spsNal, param->direct8x8Interference, 1) < 0)
        return 0;

      /* [b1 frame_cropping_flag] */
      if (writeH264NalByteArrayBits(spsNal, param->frameCropping, 1) < 0)
        return 0;

      if (param->frameCropping) {
        /* [ue frame_crop_left_offset] */
        if (writeH264NalByteArrayExpGolombCode(spsNal, param->frameCropOffset.left) < 0)
          return 0;

        /* [ue frame_crop_right_offset] */
        if (writeH264NalByteArrayExpGolombCode(spsNal, param->frameCropOffset.right) < 0)
          return 0;

        /* [ue frame_crop_top_offset] */
        if (writeH264NalByteArrayExpGolombCode(spsNal, param->frameCropOffset.top) < 0)
          return 0;

        /* [ue frame_crop_bottom_offset] */
        if (writeH264NalByteArrayExpGolombCode(spsNal, param->frameCropOffset.bottom) < 0)
          return 0;
      }

      /* [b1 vui_parameters_present_flag] */
      if (writeH264NalByteArrayBits(spsNal, param->vuiParametersPresent, 1) < 0)
        return 0;

      if (param->vuiParametersPresent) {
        /* vui_parameters() */ {
          vuiParam = &param->vuiParameters;
          colourDesc = &vuiParam->videoSignalType.colourDescription;

          /* [b1 vui_parameters_present_flag] */
          if (writeH264NalByteArrayBits(spsNal, vuiParam->aspectRatioInfoPresent, 1) < 0)
            return 0;

          if (param->vuiParameters.aspectRatioInfoPresent) {
            /* [u8 aspect_ratio_idc] */
            if (writeH264NalByteArrayBits(spsNal, vuiParam->aspectRatioIdc, 8) < 0)
              return 0;

            if (vuiParam->aspectRatioIdc == H264_ASPECT_RATIO_IDC_EXTENDED_SAR) {
              /* [u16 sar_width] */
              if (writeH264NalByteArrayBits(spsNal, vuiParam->extendedSAR.width, 16) < 0)
                return 0;

              /* [u16 sar_height] */
              if (writeH264NalByteArrayBits(spsNal, vuiParam->extendedSAR.height, 16) < 0)
                return 0;
            }
          }

          /* [b1 overscan_info_present_flag] */
          if (writeH264NalByteArrayBits(spsNal, vuiParam->overscanInfoPresent, 1) < 0)
            return 0;

          if (vuiParam->overscanInfoPresent) {
            /* [b1 overscan_appropriate_flag] */
            if (writeH264NalByteArrayBits(spsNal, vuiParam->overscanAppropriate, 1) < 0)
              return 0;
          }

          /* [b1 video_signal_type_present_flag] */
          if (writeH264NalByteArrayBits(spsNal, vuiParam->videoSignalTypePresent, 1) < 0)
            return 0;

          if (vuiParam->videoSignalTypePresent) {
            /* [u3 video_format] */
            if (writeH264NalByteArrayBits(spsNal, vuiParam->videoSignalType.videoFormat, 3) < 0)
              return 0;

            /* [b1 video_full_range_flag] */
            if (writeH264NalByteArrayBits(spsNal, vuiParam->videoSignalType.videoFullRange, 1) < 0)
              return 0;

            /* [b1 colour_description_present_flag] */
            if (writeH264NalByteArrayBits(spsNal, vuiParam->videoSignalType.colourDescPresent, 1) < 0)
              return 0;

            if (vuiParam->videoSignalType.colourDescPresent) {
              /* [u8 colour_primaries] */
              if (writeH264NalByteArrayBits(spsNal, colourDesc->colourPrimaries, 8) < 0)
                return 0;

              /* [u8 transfer_characteristics] */
              if (writeH264NalByteArrayBits(spsNal, colourDesc->transferCharact, 8) < 0)
                return 0;

              /* [u8 matrix_coefficients] */
              if (writeH264NalByteArrayBits(spsNal, colourDesc->matrixCoeff, 8) < 0)
                return 0;
            }
          }

          /* [b1 chroma_loc_info_present_flag] */
          if (writeH264NalByteArrayBits(spsNal, vuiParam->chromaLocInfoPresent, 1) < 0)
            return 0;

          if (vuiParam->chromaLocInfoPresent) {
            /* [ue chroma_sample_loc_type_top_field] */
            if (writeH264NalByteArrayExpGolombCode(spsNal, vuiParam->ChromaLocInfo.sampleLocTypeTopField) < 0)
              return 0;

            /* [ue chroma_sample_loc_type_bottom_field] */
            if (writeH264NalByteArrayExpGolombCode(spsNal, vuiParam->ChromaLocInfo.sampleLocTypeBottomField) < 0)
              return 0;
          }

          /* [b1 timing_info_present_flag] */
          if (writeH264NalByteArrayBits(spsNal, vuiParam->timingInfoPresent, 1) < 0)
            return 0;

          if (vuiParam->timingInfoPresent) {
            /* [u32 num_units_in_tick] */
            if (writeH264NalByteArrayBits(spsNal, vuiParam->timingInfo.numUnitsInTick, 32) < 0)
              return 0;

            /* [u32 time_scale] */
            if (writeH264NalByteArrayBits(spsNal, vuiParam->timingInfo.timeScale, 32) < 0)
              return 0;

            /* [b1 fixed_frame_rate_flag] */
            if (writeH264NalByteArrayBits(spsNal, vuiParam->timingInfo.fixedFrameRateFlag, 1) < 0)
              return 0;
          }

          /* [b1 nal_hrd_parameters_present_flag] */
          if (writeH264NalByteArrayBits(spsNal, vuiParam->nalHrdParamPresent, 1) < 0)
            return 0;

          if (vuiParam->nalHrdParamPresent) {
            /* hrd_parameters() */
            if (buildH264HrdParameters(spsNal, &vuiParam->nalHrdParam) < 0)
              return 0;
          }

          /* [b1 vcl_hrd_parameters_present_flag] */
          if (writeH264NalByteArrayBits(spsNal, vuiParam->vclHrdParamPresent, 1) < 0)
            return 0;

          if (vuiParam->vclHrdParamPresent) {
            /* hrd_parameters() */
            if (buildH264HrdParameters(spsNal, &vuiParam->vclHrdParam) < 0)
              return 0;
          }

          if (vuiParam->nalHrdParamPresent || vuiParam->vclHrdParamPresent) {
            /* [b1 low_delay_hrd_flag] */
            if (writeH264NalByteArrayBits(spsNal, vuiParam->lowDelayHrd, 1) < 0)
              return 0;
          }

          /* [b1 pic_struct_present_flag] */
          if (writeH264NalByteArrayBits(spsNal, vuiParam->picStructPresent, 1) < 0)
            return 0;

          /* [b1 bitstream_restriction_flag] */
          if (writeH264NalByteArrayBits(spsNal, vuiParam->bitstreamRestrictionsPresent, 1) < 0)
            return 0;

          if (vuiParam->bitstreamRestrictionsPresent) {
            /* [b1 motion_vectors_over_pic_boundaries_flag] */
            if (writeH264NalByteArrayBits(spsNal, vuiParam->bistreamRestrictions.motionVectorsOverPicBoundaries, 1) < 0)
              return 0;

            /* [ue max_bytes_per_pic_denom] */
            if (writeH264NalByteArrayExpGolombCode(spsNal, vuiParam->bistreamRestrictions.maxBytesPerPicDenom) < 0)
              return 0;

            /* [ue max_bits_per_mb_denom] */
            if (writeH264NalByteArrayExpGolombCode(spsNal, vuiParam->bistreamRestrictions.maxBitsPerPicDenom) < 0)
              return 0;

            /* [ue log2_max_mv_length_horizontal] */
            if (writeH264NalByteArrayExpGolombCode(spsNal, vuiParam->bistreamRestrictions.log2MaxMvLengthHorizontal) < 0)
              return 0;

            /* [ue log2_max_mv_length_vertical] */
            if (writeH264NalByteArrayExpGolombCode(spsNal, vuiParam->bistreamRestrictions.log2MaxMvLengthVertical) < 0)
              return 0;

            /* [ue max_num_reorder_frames] */
            if (writeH264NalByteArrayExpGolombCode(spsNal, vuiParam->bistreamRestrictions.maxNumReorderFrames) < 0)
              return 0;

            /* [ue max_dec_frame_buffering] */
            if (writeH264NalByteArrayExpGolombCode(spsNal, vuiParam->bistreamRestrictions.maxDecFrameBuffering) < 0)
              return 0;
          }
        }
      }
    }
  }

  /* rbsp_trailing_bits() */
  if (buildH264RbspTrailingBits(spsNal) < 0)
    return 0;

  /* lb_print_data(spsNal->array, spsNal->writtenBytes); */
  /* lbc_printf("Written bytes: %" PRIu64 " byte(s).\n", spsNal->writtenBytes); */

  /**
   * Save created unit
   * (or directly inject it if no more data blocks are available).
   */

#if !DISABLE_NAL_REPLACEMENT_DATA_OPTIMIZATION
  if (!isDataBlocksNbLimitReachedEsms(handle->esms)) {
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

    modNalUnit->linkedParam = (H264SPSDataParameters *) malloc(
      sizeof(H264SPSDataParameters)
    );
    if (NULL == modNalUnit->linkedParam)
      LIBBLU_H264_ERROR_RETURN("Memory allocation error.\n");

    memcpy(
      modNalUnit->linkedParam,
      param,
      sizeof(H264SPSDataParameters)
    );

    modNalUnit->length = H264_NAL_BA_HDLR_NB_WRITTEN_BYTES(spsNal);
    ret = appendDataBlockEsms(
      handle->esms,
      spsNal->array,
      H264_NAL_BA_HDLR_NB_WRITTEN_BYTES(spsNal),
      &modNalUnit->dataSectionIdx
    );
    if (ret < 0)
      return 0;

    ret = appendAddDataBlockCommand(
      handle->esms,
      insertingOffset, INSERTION_MODE_ERASE,
      modNalUnit->dataSectionIdx
    );
  }
  else {
    /* Direct NAL injection : */
    ret = appendAddDataCommand(
      handle->esms,
      insertingOffset, INSERTION_MODE_ERASE,
      H264_NAL_BA_HDLR_NB_WRITTEN_BYTES(spsNal),
      spsNal->array
    );
  }
#else
  /* Direct NAL injection : */
  ret = appendAddDataCommand(
    handle->esms,
    insertingOffset, INSERTION_MODE_ERASE,
    H264_NAL_BA_HDLR_NB_WRITTEN_BYTES(spsNal),
    spsNal->array
  );
#endif

  if (ret < 0)
    return 0;

  writtenBytes = H264_NAL_BA_HDLR_NB_WRITTEN_BYTES(spsNal);
  destroyH264NalByteArrayHandler(spsNal);

  return writtenBytes;
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

  assert(spsParam->vuiParametersPresent);
  assert(handle->sequenceParametersSetPresent);

  vuiParam = &spsParam->vuiParameters;
  colourDesc = &vuiParam->videoSignalType.colourDescription;

  LIBBLU_DEBUG_COM("Applying SPS Nal VUI parameters updating.\n");

  if (handle->curProgParam.useVuiRebuilding) {
    vuiParam->videoSignalTypePresent = true;
    vuiParam->videoSignalType.videoFormat = H264_VIDEO_FORMAT_NTSC;
    vuiParam->videoSignalType.videoFullRange = false;
    vuiParam->videoSignalType.colourDescPresent = true;

    /* Set VUI Color Description parameters : */
    switch (handle->sequenceParametersSet.data.FrameHeight) {
      case 576: /* SD PAL */
        colourDesc->colourPrimaries = H264_COLOR_PRIM_BT470BG;
        colourDesc->transferCharact = H264_TRANS_CHAR_BT470BG;
        colourDesc->matrixCoeff = H264_MATRX_COEF_BT470BG;
        break;

      case 480: /* SD NTSC */
        colourDesc->colourPrimaries = H264_COLOR_PRIM_SMPTE170M;
        colourDesc->transferCharact = H264_TRANS_CHAR_SMPTE170M;
        colourDesc->matrixCoeff = H264_MATRX_COEF_SMPTE170M;
        break;

      default: /* HD */
        colourDesc->colourPrimaries = H264_COLOR_PRIM_BT709;
        colourDesc->transferCharact = H264_TRANS_CHAR_BT709;
        colourDesc->matrixCoeff = H264_MATRX_COEF_BT709;
    }
  }

  if (handle->curProgParam.useVuiUpdate) {
    switch (options.fpsChange) {
      case 0x0: /* No change */
        break;

      case FRAME_RATE_CODE_23976: /* 23.976 */
        vuiParam->timingInfo.numUnitsInTick = 1001;
        vuiParam->timingInfo.timeScale = 48000;
        break;

      case FRAME_RATE_CODE_24: /* 24 */
        vuiParam->timingInfo.numUnitsInTick = 1000;
        vuiParam->timingInfo.timeScale = 48000;
        break;

      case FRAME_RATE_CODE_25: /* 25 */
        vuiParam->timingInfo.numUnitsInTick = 1000;
        vuiParam->timingInfo.timeScale = 50000;
        break;

      case FRAME_RATE_CODE_29970: /* 29.970 */
        vuiParam->timingInfo.numUnitsInTick = 1001;
        vuiParam->timingInfo.timeScale = 60000;
        break;

      case FRAME_RATE_CODE_50: /* 50 */
        vuiParam->timingInfo.numUnitsInTick = 1000;
        vuiParam->timingInfo.timeScale = 100000;
        break;

      case FRAME_RATE_CODE_59940: /* 59.940 */
        vuiParam->timingInfo.numUnitsInTick = 1001;
        vuiParam->timingInfo.timeScale = 120000;
        break;

      default:
        LIBBLU_H264_ERROR_RETURN(
          "Patcher is unable to get FPS destination value.\n"
        );
    }

    if (isUsedLibbluAspectRatioMod(options.arChange)) {
      /* vuiParam->aspectRatioInfoPresent = true; */
      vuiParam->aspectRatioIdc = options.arChange.idc;
      if (options.arChange.idc == H264_ASPECT_RATIO_IDC_EXTENDED_SAR) {
        vuiParam->extendedSAR.width = options.arChange.x;
        vuiParam->extendedSAR.height = options.arChange.y;
      }
    }

    if (0x00 != options.levelChange) {
      if (
        options.levelChange < spsParam->levelIdc
        && !handle->curProgParam.usageOfLowerLevel
      ) {
        LIBBLU_WARNING(
          "Usage of a lower level than initial one, stream may not respect "
          "new level-related constraints.\n"
        );

        /* updateH264LevelLimits(handle, spsParam->levelIdc); */
        handle->curProgParam.usageOfLowerLevel = true;
      }

      spsParam->levelIdc = options.levelChange;
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

  param->cpbRemovalDelayLengthMinus1 = newCpbRemovalDelayLength - 1;

  return 0;
}

int rebuildH264SPSNalVuiHRDParameters(
  H264SPSDataParameters * spsParam
)
{
  int ret;

  assert(NULL != spsParam);
  assert(spsParam->vuiParametersPresent);

  LIBBLU_DEBUG_COM("Applying SPS Nal VUI HRD parameters updating.\n");

  if (spsParam->vuiParameters.nalHrdParamPresent) {
    ret = patchH264HRDParameters_cpbRemovalDelayLength(
      &spsParam->vuiParameters.nalHrdParam,
      H264_TARGET_INIT_CPB_REM_DELAY_LENGTH
    );
    if (ret < 0)
      return -1;
  }

  if (spsParam->vuiParameters.vclHrdParamPresent) {
    ret = patchH264HRDParameters_cpbRemovalDelayLength(
      &spsParam->vuiParameters.vclHrdParam,
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

    if (!updatedSpsDataParam.vuiParametersPresent)
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

  if (update)
    return replaceCurNalCell(
      handle,
      &updatedSpsDataParam,
      sizeof(H264SPSDataParameters)
    );
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
  H264HrdBufferingPeriodParameters * hrdParam;

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

  if (!handle->sequenceParametersSet.data.vuiParametersPresent)
    LIBBLU_H264_ERROR_RETURN(
      "Unable to patch SEI Buffering Period message, "
      "missing VUI parameters in SPS.\n"
    );

  vuiParam = &handle->sequenceParametersSet.data.vuiParameters;

  /**
   * Check for NalHrdBpPresentFlag and VclHrdBpPresentFlag equal to 0b0,
   * if so, Buffering Period SEI message shouldn't be present.
   */

  if (!vuiParam->nalHrdParamPresent && !vuiParam->vclHrdParamPresent)
    LIBBLU_H264_ERROR_RETURN(
      "Unable to patch SEI Buffering Period message, "
      "missing HRD settings in VUI parameters.\n"
    );

  /* [ue seq_parameter_set_id] */
  if (writeH264NalByteArrayExpGolombCode(seiNal, param->seqParametersSetId) < 0)
    return -1;

  if (vuiParam->nalHrdParamPresent) {
    maxSchedSelIdx = vuiParam->nalHrdParam.cpbCntMinus1 + 1;
    fieldLength = vuiParam->nalHrdParam.initialCpbRemovalDelayLengthMinus1 + 1;

    for (SchedSelIdx = 0; SchedSelIdx < maxSchedSelIdx; SchedSelIdx++) {
      hrdParam = param->nalHrdParam + SchedSelIdx;

      /* [u<initial_cpb_removal_delay_length_minus1 + 1> initial_cpb_removal_delay[SchedSelIdx]] */
      if (writeH264NalByteArrayBits(seiNal, hrdParam->initialCpbRemovalDelay, fieldLength) < 0)
        return -1;

      /* [u<initial_cpb_removal_delay_length_minus1 + 1> initial_cpb_removal_delay_offset[SchedSelIdx]] */
      if (writeH264NalByteArrayBits(seiNal, hrdParam->initialCpbRemovalDelayOff, fieldLength) < 0)
        return -1;
    }
  }

  if (vuiParam->vclHrdParamPresent) {
    maxSchedSelIdx = vuiParam->vclHrdParam.cpbCntMinus1 + 1;
    fieldLength = vuiParam->vclHrdParam.initialCpbRemovalDelayLengthMinus1 + 1;

    for (SchedSelIdx = 0; SchedSelIdx < maxSchedSelIdx; SchedSelIdx++) {
      hrdParam = param->vclHrdParam + SchedSelIdx;

      /* [u<initial_cpb_removal_delay_length_minus1 + 1> initial_cpb_removal_delay[SchedSelIdx]] */
      if (writeH264NalByteArrayBits(seiNal, hrdParam->initialCpbRemovalDelay, fieldLength) < 0)
        return -1;

      /* [u<initial_cpb_removal_delay_length_minus1 + 1> initial_cpb_removal_delay_offset[SchedSelIdx]] */
      if (writeH264NalByteArrayBits(seiNal, hrdParam->initialCpbRemovalDelayOff, fieldLength) < 0)
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
  EsmsFileHeaderPtr handle->esms,
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
    .nalRefIdc = NAL_SEI_REF_IDC,
    .nalUnitType = NAL_UNIT_TYPE_SUPPLEMENTAL_ENHANCEMENT_INFORMATION
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
  ret = appendAddDataCommand(
    handle->esms,
    insertingOffset, INSERTION_MODE_ERASE,
    H264_NAL_BA_HDLR_NB_WRITTEN_BYTES(seiNal),
    seiNal->array
  );
  if (ret < 0)
    return 0;

  /* exit(0); */
  writtenBytes = H264_NAL_BA_HDLR_NB_WRITTEN_BYTES(seiNal);
  destroyH264NalByteArrayHandler(seiNal);

  return writtenBytes;
}

size_t appendH264SeiBufferingPeriodPlaceHolder(
  H264ParametersHandlerPtr handle,
  EsmsFileHeaderPtr handle->esms,
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
      .nalRefIdc = NAL_SEI_REF_IDC,
      .nalUnitType = NAL_UNIT_TYPE_SUPPLEMENTAL_ENHANCEMENT_INFORMATION
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

    seiModNalUnit->length = H264_NAL_BA_HDLR_NB_WRITTEN_BYTES(seiNal);

    ret = appendDataBlockEsms(
      handle->esms,
      seiNal->array,
      H264_NAL_BA_HDLR_NB_WRITTEN_BYTES(seiNal),
      &seiModNalUnit->dataSectionIdx
    );
    if (ret < 0)
      return 0;

    *seiModNalUnitPres = true;
    destroyH264NalByteArrayHandler(seiNal);
  }

  assert(NULL != seiModNalUnit->linkedParam);

  ret = appendAddDataBlockCommand(
    handle->esms,
    insertingOffset,
    INSERTION_MODE_ERASE,
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
  const unsigned seqParametersSetId,
  const H264HrdBufferingPeriodParameters * hrdParam,
  const H264HrdBufferingPeriodParameters * vclParam
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

  if (!handle->sequenceParametersSet.data.vuiParametersPresent)
    LIBBLU_H264_ERROR_RETURN(
      "Unable to patch SEI Buffering Period message, "
      "missing VUI parameters in SPS.\n"
    );

  vuiParam = &handle->sequenceParametersSet.data.vuiParameters;

  /**
   * Check for NalHrdBpPresentFlag and VclHrdBpPresentFlag equal to 0b0,
   * if so, Buffering Period SEI message shouldn't be present.
   */

  if (!vuiParam->nalHrdParamPresent && !vuiParam->vclHrdParamPresent)
    LIBBLU_H264_ERROR_RETURN(
      "Unable to patch SEI Buffering Period message, "
      "missing HRD settings in VUI parameters.\n"
    );

  seiMessage->payloadType = H264_SEI_TYPE_BUFFERING_PERIOD;
  param = &seiMessage->bufferingPeriod;

  messageLen = 0;

  param->seqParametersSetId = seqParametersSetId;
  messageLen += calcH264ExpGolombCodeLength(seqParametersSetId, false);

  nbCpbConfigs = vuiParam->nalHrdParam.cpbCntMinus1 + 1;
  for (cpbConfig = 0; cpbConfig < nbCpbConfigs; cpbConfig++) {
    param->nalHrdParam[cpbConfig] = hrdParam[cpbConfig];
    messageLen += (vuiParam->nalHrdParam.initialCpbRemovalDelayLengthMinus1 + 1) * 2;
  }

  nbCpbConfigs = vuiParam->vclHrdParam.cpbCntMinus1 + 1;
  for (cpbConfig = 0; cpbConfig < nbCpbConfigs; cpbConfig++) {
    param->vclHrdParam[cpbConfig] = vclParam[cpbConfig];
    messageLen += (vuiParam->nalHrdParam.initialCpbRemovalDelayLengthMinus1 + 1) * 2;
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

  H264HrdBufferingPeriodParameters nalHrd[1];
  H264HrdBufferingPeriodParameters vclHrd[1];

  H264AUNalUnitPtr nalUnit;

  assert(NULL != handle);
  assert(handle->sequenceParametersSetPresent);

  /* Setting default empty SEI parameters : */

  nalHrd[0] = (H264HrdBufferingPeriodParameters) {
    .initialCpbRemovalDelay = 0,
    .initialCpbRemovalDelayOff = 0,
  };
  vclHrd[0] = (H264HrdBufferingPeriodParameters) {
    .initialCpbRemovalDelay = 0,
    .initialCpbRemovalDelayOff = 0,
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
  EsmsFileHeaderPtr handle->esms
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

  H264HrdBufferingPeriodParameters nalHrd[1];
  H264HrdBufferingPeriodParameters vclHrd[1];

  /* Pointer to saved parameter in H264ParametersHandlerPtr struct : */
  H264ModifiedNalUnit * seiModNalUnit;

  H264NalHeaderParameters seiNalParam;
  H264NalByteArrayHandlerPtr seiNal;

  assert(NULL != handle);

  if (!handle->modNalLst.patchBufferingPeriodSeiPresent)
    return 0; /* Nothing to do. */

  /* All AU in stream cannot be zero-size... */
  assert(0 != handle->curProgParam.largestFrameSize);
  assert(0 != handle->curProgParam.largestIFrameSize);

  /* Usually, required parameters presence has been checked before. */
  assert(handle->sequenceParametersSetPresent);
  assert(handle->sequenceParametersSet.data.vuiParametersPresent);

  vuiParam = &handle->sequenceParametersSet.data.vuiParameters;
  nalHrdPresent = vuiParam->nalHrdParamPresent;
  vclHrdPresent = vuiParam->vclHrdParamPresent;

  assert(nalHrdPresent || vclHrdPresent);

  /* NAL-HRD computation : */
  if (nalHrdPresent) {
    cpbRemovalDelayFieldLength = vuiParam->nalHrdParam.initialCpbRemovalDelayLengthMinus1 + 1;
    maxAllowedCpbRemovalDelayValue = (2 << (cpbRemovalDelayFieldLength - 1)) - 1; /* Quick pow(2, cpbRemovalDelayFieldLength) - 1 */

    nalBitRate = vuiParam->nalHrdParam.schedSel[0].bitRate;

    nalMaxCpbBufferingDelay = handle->curProgParam.largestFrameSize * 8l / nalBitRate;
    lbc_printf("Max CPB Buffering delay: %f.\n", nalMaxCpbBufferingDelay);

    nalResultInitCpbRemovalDelay = ceil(nalMaxCpbBufferingDelay * H264_90KHZ_CLOCK);
    lbc_printf("Result computed initial CPB removal delay: %f.\n", nalResultInitCpbRemovalDelay);

    nalCpbSize = vuiParam->nalHrdParam.schedSel[0].cpbSize;

    if (nalCpbSize < nalResultInitCpbRemovalDelay * nalBitRate / H264_90KHZ_CLOCK)
      LIBBLU_ERROR_RETURN("Global CPB overflow occurs during NAL-HRD initial_cpb_removal_delay computation.\n");

    if (H264_90KHZ_CLOCK < nalResultInitCpbRemovalDelay) {
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
      cpbRemovalDelayFieldLength = vuiParam->vclHrdParam.initialCpbRemovalDelayLengthMinus1 + 1;
      maxAllowedCpbRemovalDelayValue = (2 << (cpbRemovalDelayFieldLength - 1)) - 1; /* Quick pow(2, cpbRemovalDelayFieldLength) - 1 */
    }
    else
      assert(cpbRemovalDelayFieldLength == vuiParam->vclHrdParam.initialCpbRemovalDelayLengthMinus1 + 1);

    vclBitRate = vuiParam->vclHrdParam.schedSel[0].bitRate;

    vclMaxCpbBufferingDelay = handle->curProgParam.largestFrameSize * 8l / vclBitRate;
    lbc_printf("Max CPB Buffering delay: %f.\n", vclMaxCpbBufferingDelay);

    vclResultInitCpbRemovalDelay = ceil(vclMaxCpbBufferingDelay * H264_90KHZ_CLOCK);
    lbc_printf("Result computed initial CPB removal delay: %f.\n", vclResultInitCpbRemovalDelay);

    vclCpbSize = vuiParam->vclHrdParam.schedSel[0].cpbSize;

    if (vclCpbSize < vclResultInitCpbRemovalDelay * vclBitRate / H264_90KHZ_CLOCK)
      LIBBLU_ERROR_RETURN("Global CPB overflow occurs during NAL-HRD initial_cpb_removal_delay computation.\n");

    if (H264_90KHZ_CLOCK < vclResultInitCpbRemovalDelay)
      /* Considering initial CPB buffering cannot be superior to one second. */
      LIBBLU_H264_ERROR_RETURN(
        "Computed 'initial_cpb_removal_delay' exceed one second "
        "(%.2f second(s), %f ticks).\n",
        vclResultInitCpbRemovalDelay / H264_90KHZ_CLOCK,
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

  /* Pack computed replacement parameters : */
  nalHrd[0].initialCpbRemovalDelay = nalResultInitCpbRemovalDelay;
  nalHrd[0].initialCpbRemovalDelayOff = 0;
  vclHrd[0].initialCpbRemovalDelay = vclResultInitCpbRemovalDelay;
  vclHrd[0].initialCpbRemovalDelayOff = 0;
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
  seiNalParam.nalRefIdc = NAL_SEI_REF_IDC;
  seiNalParam.nalUnitType = NAL_UNIT_TYPE_SUPPLEMENTAL_ENHANCEMENT_INFORMATION;

  if (NULL == (seiNal = createH264NalByteArrayHandler(seiNalParam)))
    return -1; /* Error */

  /* sei_rbsp() */
  if (buildH264SupplementalEnhancementInformation(handle, seiNal, &newSeiNalParam) < 0)
    return -1;

  seiModNalUnit = &handle->modNalLst.bufferingPeriodSeiMsg;

  if (seiModNalUnit->length < H264_NAL_BA_HDLR_NB_WRITTEN_BYTES(seiNal))
    LIBBLU_H264_ERROR_RETURN(
      "Final Buffering Period SEI NALU is bigger than place holder.\n"
    );

  /* Replace place-holder NALU with updated one. */
  ret = updateDataBlockEsms(
    handle->esms,
    seiNal->array,
    H264_NAL_BA_HDLR_NB_WRITTEN_BYTES(seiNal),
    seiModNalUnit->dataSectionIdx
  );
  if (ret < 0)
    return -1;

  destroyH264NalByteArrayHandler(seiNal);

  return 0; /* OK */
}
#endif
