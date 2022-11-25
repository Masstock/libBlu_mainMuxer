#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include <errno.h>

#include "h264_checks.h"

int checkH264AccessUnitDelimiterCompliance(
  const H264AccessUnitDelimiterParameters param
)
{
  LIBBLU_H264_DEBUG_AUD(
    "  Primary coded picture slices types (primary_pic_type): 0x%02" PRIX8 ".\n",
    param.primary_pic_type
  );

  LIBBLU_H264_DEBUG_AUD(
    "   -> slice_type authorized: %s.\n",
    h264PrimaryPictureTypeStr(param.primary_pic_type)
  );

  if (H264_PRIM_PIC_TYPE_ALL < param.primary_pic_type)
    LIBBLU_H264_ERROR_RETURN(
      "Reserved 'primary_pic_type' == %u in use.\n",
      param.primary_pic_type
    );

  return 0; /* OK */
}

int checkH264ProfileIdcCompliance(
  const H264ProfileIdcValue profile_idc,
  const H264ContraintFlags constraints
)
{
  int i;
  bool set;

  bool compliantProfile;

  LIBBLU_H264_DEBUG_SPS(
    "  Profile (profile_idc): %d.\n",
    profile_idc
  );

  LIBBLU_H264_DEBUG_SPS(
    "   => %s profile.\n",
    h264ProfileIdcValueStr (
      profile_idc,
      constraints
    )
  );

  LIBBLU_H264_DEBUG_SPS(
    "  Constraints flags:\n"
  );
  for (i = 0; i <= 5; i++) {
    set = *(&constraints.set0 + i);
    LIBBLU_H264_DEBUG_SPS(
      "   -> Set %d (constraint_set%d_flag): %s (0b%d).\n",
      i, i, BOOL_STR(set), set
    );
  }
  LIBBLU_H264_DEBUG_SPS(
    "   -> Reserved flags (reserved_zero_2bits): 0x%02" PRIX8 ".\n",
    constraints.reservedFlags
  );

  compliantProfile = false;
  switch (profile_idc) {
    case H264_PROFILE_MAIN:
      compliantProfile = true;
      break;

    case H264_PROFILE_HIGH:
      compliantProfile = (!constraints.set4);
      break;

    default:
      break;
  }

  if (!compliantProfile) {
    LIBBLU_H264_DEBUG_SPS(
      "    => Non BD-compliant.\n"
    );

    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "%s profile is not part of the profiles allowed by BD-specs "
      "('profile_idc' == %d).\n",
      h264ProfileIdcValueStr (
        profile_idc,
        constraints
      ),
      profile_idc
    );
  }

  return 0; /* OK */
}

int checkH264LevelIdcCompliance(
  const uint8_t level_idc
)
{
  unsigned i;
  bool unknownLevel;

  static const uint8_t levels[] = {
    10, H264_LEVEL_1B, 11, 12, 13,
    20, 21, 22,
    30, 31, 32,
    40, 41, 42,
    50, 51, 52,
    60, 61, 62
  };

  LIBBLU_H264_DEBUG_SPS(
    "  Level (level_idc): %" PRIu8 ".%" PRIu8 " (0x%02" PRIX8 ").\n",
    level_idc / 10,
    level_idc % 10,
    level_idc
  );

  unknownLevel = true;
  for (i = 0; i < ARRAY_SIZE(levels); i++) {
    if (levels[i] == level_idc) {
      unknownLevel = false;
      break;
    }
  }

  if (unknownLevel) {
    LIBBLU_H264_DEBUG_SPS(
      "   => Unknown/Reserved value.\n"
    );

    LIBBLU_H264_ERROR_RETURN(
      "Unknown/Reserved 'level_idc' == %d in use.\n",
      level_idc
    );
  }

  if (level_idc < 30 || 41 < level_idc) {
    /* BDAV unallowed level_idc. */
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "Level %" PRIu8 ".%" PRIu8 " is not part of the "
      "levels allowed by BD-specs ('level_idc' == %d).\n",
      level_idc / 10,
      level_idc % 10,
      level_idc
    );
  }

  return 0;
}

int checkH264HrdParametersCompliance(
  const H264HrdParameters param
)
{
  unsigned ShedSelIdx;

  LIBBLU_H264_DEBUG_VUI(
    "    Number of CPB configurations "
    "(cpb_cnt_minus1): %u (0x%X).\n",
    param.cpb_cnt_minus1 + 1,
    param.cpb_cnt_minus1
  );

  if (H264_BDAV_ALLOWED_CPB_CNT <= param.cpb_cnt_minus1) {
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "%u CPB configurations defined, "
      "expect no more than %u configurations "
      "('cpb_cnt_minus1' == %u).\n",
      param.cpb_cnt_minus1 + 1,
      H264_BDAV_ALLOWED_CPB_CNT,
      param.cpb_cnt_minus1
    );
  }

  if (H264_MAX_CPB_CONFIGURATIONS <= param.cpb_cnt_minus1) {
    LIBBLU_H264_ERROR_RETURN(
      "Overflow, the number of defined CPB configurations exceed %u "
      "('cpb_cnt_minus1' == 0x%X).\n",
      H264_MAX_CPB_CONFIGURATIONS,
      param.cpb_cnt_minus1
    );
  }

  if (0 < param.cpb_cnt_minus1) {
    LIBBLU_H264_DEBUG_VUI(
      "     -> WARNING: Only the cpb_cnt_minus1-SchedSelIdx "
      "will be verified.\n"
    );
  }

  LIBBLU_H264_DEBUG_VUI(
    "    Bit-rate Scaling value "
    "(bit_rate_scale): %" PRIu8 " (0x%02" PRIX8 ").\n",
    param.bit_rate_scale,
    param.bit_rate_scale
  );

  LIBBLU_H264_DEBUG_VUI(
    "    CPB Size Scaling value "
    "(cpb_size_scale): %" PRIu8 " (0x%02" PRIX8 ").\n",
    param.cpb_size_scale,
    param.cpb_size_scale
  );

  LIBBLU_H264_DEBUG_VUI("    CPB configurations:\n");
  for (ShedSelIdx = 0; ShedSelIdx <= param.cpb_cnt_minus1; ShedSelIdx++) {
    LIBBLU_H264_DEBUG_VUI("     - ShedSelIdx %u:\n", ShedSelIdx);

    LIBBLU_H264_DEBUG_VUI(
      "      Bit rate value "
      "(bit_rate_value_minus1): %" PRIu32 " (0x%" PRIX32 ").\n",
      param.schedSel[ShedSelIdx].bit_rate_value_minus1,
      param.schedSel[ShedSelIdx].bit_rate_value_minus1
    );

    if (0 < ShedSelIdx) {
      if (
        param.schedSel[ShedSelIdx].bit_rate_value_minus1
        <= param.schedSel[ShedSelIdx - 1].bit_rate_value_minus1
      ) {
        LIBBLU_H264_ERROR_RETURN(
          "Invalid HRD 'bit_rate_value_minus1[SchedSelIdx]' value for "
          "Schedule Selection Index %u, "
          "'bit_rate_value_minus1[SchedSelIdx-1]' == %" PRIu32 " shall be "
          "lower than 'bit_rate_value_minus1[SchedSelIdx]' == %" PRIu32 ".\n",
          ShedSelIdx,
          param.schedSel[ShedSelIdx-1].bit_rate_value_minus1,
          param.schedSel[ShedSelIdx].bit_rate_value_minus1
        );
      }
    }

    LIBBLU_H264_DEBUG_VUI(
      "       => Bit rate (BitRate): %" PRIu64 " bits/s.\n",
      param.schedSel[ShedSelIdx].BitRate
    );

    LIBBLU_H264_DEBUG_VUI(
      "      Coded Picture Buffer size value "
      "(cpb_size_value_minus1): %" PRIu32 " (0x%" PRIX32 ").\n",
      param.schedSel[ShedSelIdx].cpb_size_value_minus1,
      param.schedSel[ShedSelIdx].cpb_size_value_minus1
    );

    if (0 < ShedSelIdx) {
      if (
        param.schedSel[ShedSelIdx].cpb_size_value_minus1
        > param.schedSel[ShedSelIdx - 1].cpb_size_value_minus1
      ) {
        LIBBLU_H264_ERROR_RETURN(
          "Invalid HRD 'cpb_size_value_minus1[SchedSelIdx]' value for "
          "Schedule Selection Index %u, "
          "'cpb_size_value_minus1[SchedSelIdx-1]' == %" PRIu32 " shall be "
          "greater or equal to 'cpb_size_value_minus1[SchedSelIdx]' "
          "== %" PRIu32 ".\n",
          ShedSelIdx,
          param.schedSel[ShedSelIdx-1].cpb_size_value_minus1,
          param.schedSel[ShedSelIdx].cpb_size_value_minus1
        );
      }
    }

    LIBBLU_H264_DEBUG_VUI(
      "       => CPB size (CpbSize): %" PRIu64 " bits.\n",
      param.schedSel[ShedSelIdx].CpbSize
    );

    LIBBLU_H264_DEBUG_VUI(
      "      Constant Bit-Rate mode (cbr_flag): %s (0x%X).\n",
      BOOL_STR(param.schedSel[ShedSelIdx].cbr_flag),
      param.schedSel[ShedSelIdx].cbr_flag
    );
  }

  LIBBLU_H264_DEBUG_VUI(
    "    Fields sizes:\n"
  );
  LIBBLU_H264_DEBUG_VUI(
    "     -> Initial CPB Delay "
    "(initial_cpb_removal_delay_length_minus1): "
    "%" PRIu8 " bits (0x%02" PRIX8 ").\n",
    param.initial_cpb_removal_delay_length_minus1 + 1,
    param.initial_cpb_removal_delay_length_minus1
  );
  LIBBLU_H264_DEBUG_VUI(
    "     -> CPB Removal Delay "
    "(cpb_removal_delay_length_minus1): "
    "%" PRIu8 " bits (0x%02" PRIX8 ").\n",
    param.cpb_removal_delay_length_minus1 + 1,
    param.cpb_removal_delay_length_minus1
  );
  LIBBLU_H264_DEBUG_VUI(
    "     -> DPB Output Delay "
    "(dpb_output_delay_length_minus1): "
    "%" PRIu8 " bits (0x%02" PRIX8 ").\n",
    param.dpb_output_delay_length_minus1 + 1,
    param.dpb_output_delay_length_minus1
  );
  LIBBLU_H264_DEBUG_VUI(
    "     -> Time Offset "
    "(time_offset_length): "
    "%" PRIu8 " bits (0x%02" PRIX8 ").\n",
    param.time_offset_length,
    param.time_offset_length
  );

  return 0;
}

int checkH264VuiParametersCompliance(
  const H264VuiParameters param,
  const H264SPSDataParameters spsParam,
  const H264ParametersHandlerPtr handle
)
{
  bool valid;

  H264BdavExpectedAspectRatioRet expectedSar;
  H264VideoFormatValue expectedVideoFormat;
  H264ColourPrimariesValue expectedColourPrimaries;
  H264TransferCharacteristicsValue expectedTransferCharact;
  H264MatrixCoefficientsValue expectedMatrixCoeff;

  const H264VuiColourDescriptionParameters * colourParam;

  LIBBLU_H264_DEBUG_VUI(
    "   Aspect ratio information "
    "(aspect_ratio_info_present_flag): %s (0b%X).\n",
    BOOL_PRESENCE(param.aspect_ratio_info_present_flag),
    param.aspect_ratio_info_present_flag
  );

  if (param.aspect_ratio_info_present_flag) {
    LIBBLU_H264_DEBUG_VUI(
      "    -> Sample Aspect Ratio, SAR "
      "(aspect_ratio_idc): %s (0x%X).\n",
      h264AspectRatioIdcValueStr(param.aspect_ratio_idc),
      param.aspect_ratio_idc
    );

    if (
      H264_ASPECT_RATIO_IDC_2_BY_1 < param.aspect_ratio_idc
      && param.aspect_ratio_idc != H264_ASPECT_RATIO_IDC_EXTENDED_SAR
    ) {
      LIBBLU_H264_ERROR_RETURN(
        "Reserved 'aspect_ratio_idc' == %d in use in VUI parameters.\n",
        param.aspect_ratio_idc
      );
    }

    if (param.aspect_ratio_idc == H264_ASPECT_RATIO_IDC_EXTENDED_SAR) {
      LIBBLU_H264_DEBUG_VUI(
        "    -> Sample aspect ratio horizontal size (sar_width): %u.\n",
        param.sar_width
      );
      LIBBLU_H264_DEBUG_VUI(
        "    -> Sample aspect ratio vertical size (sar_height): %u.\n",
        param.sar_height
      );
      LIBBLU_H264_DEBUG_VUI(
        "     => Extended SAR ratio: %u:%u.\n",
        param.sar_width,
        param.sar_height
      );
    }
  }
  else {
    LIBBLU_H264_DEBUG_VUI(
      "    -> Sample Aspect Ratio, SAR "
      "(aspect_ratio_idc): %s (Inferred).\n",
      h264AspectRatioIdcValueStr(param.aspect_ratio_idc),
      param.aspect_ratio_idc
    );
  }

  if (!param.aspect_ratio_info_present_flag) {
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "Missing required 'aspect_ratio_idc' information in SPS VUI parameters "
      "('aspect_ratio_info_present_flag' == 0b0).\n"
    );
  }

  expectedSar = getH264BdavExpectedAspectRatioIdc(
    spsParam.FrameWidth,
    spsParam.FrameHeight
  );
  valid = CHECK_H264_BDAV_EXPECTED_ASPECT_RATIO(
    expectedSar, param.aspect_ratio_idc
  );

  if (!valid) {
    if (expectedSar.a != expectedSar.b) {
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Invalid SAR '%s' in SPS VUI parameters for %ux%u, "
        "BD-specs require %s or %s for this "
        "resolution ('aspect_ratio_idc' == %d).\n",
        h264AspectRatioIdcValueStr(param.aspect_ratio_idc),
        spsParam.FrameWidth,
        spsParam.FrameHeight,
        h264AspectRatioIdcValueStr(expectedSar.a),
        h264AspectRatioIdcValueStr(expectedSar.b),
        param.aspect_ratio_idc
      );
    }

    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "Invalid SAR '%s' for %ux%u, BD-specs require %s for this "
      "resolution ('aspect_ratio_idc' == %d).\n",
      h264AspectRatioIdcValueStr(param.aspect_ratio_idc),
      spsParam.FrameWidth,
      spsParam.FrameHeight,
      h264AspectRatioIdcValueStr(expectedSar.a),
      param.aspect_ratio_idc
    );
  }

  LIBBLU_H264_DEBUG_VUI(
    "   Over-scan preference information "
    "(overscan_info_present_flag): %s (0b%X).\n",
    BOOL_PRESENCE(param.overscan_info_present_flag),
    param.overscan_info_present_flag
  );

  if (param.overscan_info_present_flag) {
    LIBBLU_H264_DEBUG_VUI(
      "    -> Over-scan preference "
      "(overscan_appropriate_flag): %s (0b%X).\n",
      (param.overscan_appropriate_flag) ? "Crop decoded pictures" : "Add margins",
      param.overscan_appropriate_flag
    );

    if (
      CHECK_H264_WARNING_FLAG(handle, vuiInvalidOverscan)
      && param.overscan_appropriate_flag != 0x0
    ) {
      LIBBLU_WARNING(
        "Over-scan preference in VUI parameters shouldn't allow cropping "
        "of decoded pictures ('overscan_appropriate_flag' == 0b0).\n"
      );
      handle->curProgParam.wrongVuiParameters = true;
      MARK_H264_WARNING_FLAG(handle, vuiInvalidOverscan);
    }
  }
  else {
    LIBBLU_H264_DEBUG_VUI(
      "    -> Over-scan preference: Unspecified (Inferred).\n"
    );
  }

  LIBBLU_H264_DEBUG_VUI(
    "   Video signal type parameters "
    "(video_signal_type_present_flag): %s (0b%X).\n",
    BOOL_PRESENCE(param.video_signal_type_present_flag),
    param.video_signal_type_present_flag
  );

  if (param.video_signal_type_present_flag) {
    handle->curProgParam.missingVSTVuiParameters = false;

    LIBBLU_H264_DEBUG_VUI(
      "    -> Video format (video_format): %s (0x%X).\n",
      h264VideoFormatValueStr(param.video_format),
      param.video_format
    );

    if (H264_VIDEO_FORMAT_UNSPECIFIED < param.video_format) {
      LIBBLU_H264_ERROR_RETURN(
        "Reserved 'video_format' == %d in use.\n",
        param.video_format
      );
    }

    expectedVideoFormat = getH264BdavExpectedVideoFormat(
      param.FrameRate
    );

    if (
      CHECK_H264_WARNING_FLAG(handle, vuiUnexpectedVideoFormat)
      && expectedVideoFormat != param.video_format
    ) {
      LIBBLU_WARNING(
        "Unexpected Video Format '%s' in SPS VUI parameters "
        "for %ux%u, should be '%s' for this "
        "resolution ('video_format' == %d).\n",
        h264VideoFormatValueStr(param.video_format),
        spsParam.FrameWidth,
        spsParam.FrameHeight,
        h264VideoFormatValueStr(expectedVideoFormat),
        param.video_format
      );

      handle->curProgParam.wrongVuiParameters = true;
      MARK_H264_WARNING_FLAG(handle, vuiUnexpectedVideoFormat);
    }

    LIBBLU_H264_DEBUG_VUI(
      "    -> Full-range luma and chroma samples values "
      "(video_full_range_flag): %s (0b%X).\n",
      BOOL_STR(param.video_full_range_flag),
      param.video_full_range_flag
    );

    if (
      CHECK_H264_WARNING_FLAG(handle, vuiUnexpectedVideoFullRange)
      && param.video_full_range_flag
    ) {
      LIBBLU_WARNING(
        "Video color range shall be set to 'limited' in SPS VUI parameters "
        "according to BD-specs ('video_full_range_flag' == 0b1).\n"
      );
      handle->curProgParam.wrongVuiParameters = true;
      MARK_H264_WARNING_FLAG(handle, vuiUnexpectedVideoFullRange);
    }

    LIBBLU_H264_DEBUG_VUI(
      "    -> Color description parameters "
      "(colour_description_present_flag): %s (0b%X).\n",
      BOOL_PRESENCE(param.colour_description_present_flag),
      param.colour_description_present_flag
    );

    if (param.colour_description_present_flag) {
      colourParam = &param.colour_description;

      LIBBLU_H264_DEBUG_VUI(
        "     -> Colour primaries "
        "(colour_primaries): %s (0x%X).\n",
        h264ColorPrimariesValueStr(colourParam->colour_primaries),
        colourParam->colour_primaries
      );

      expectedColourPrimaries = getH264BdavExpectedColorPrimaries(
        spsParam.FrameHeight
      );

      if (
        CHECK_H264_WARNING_FLAG(handle, vuiUnexpectedColourPrimaries)
        && expectedColourPrimaries != colourParam->colour_primaries
      ) {
        LIBBLU_WARNING(
          "Color Primaries in SPS VUI parameters should be defined to '%s', "
          "not '%s' ('colour_primaries' == 0x%X).\n",
          h264ColorPrimariesValueStr(expectedColourPrimaries),
          h264ColorPrimariesValueStr(colourParam->colour_primaries),
          colourParam->colour_primaries
        );
        handle->curProgParam.wrongVuiParameters = true;
        MARK_H264_WARNING_FLAG(handle, vuiUnexpectedColourPrimaries);
      }

      LIBBLU_H264_DEBUG_VUI(
        "     -> Transfer characteristics "
        "(transfer_characteristics): %s (0x%X).\n",
        h264TransferCharacteristicsValueStr(colourParam->transfer_characteristics),
        colourParam->transfer_characteristics
      );

      expectedTransferCharact = getH264BdavExpectedTransferCharacteritics(
        spsParam.FrameHeight
      );

      if (
        CHECK_H264_WARNING_FLAG(handle, vuiUnexpectedTranferCharact)
        && expectedTransferCharact != colourParam->transfer_characteristics
      ) {
        LIBBLU_WARNING(
          "Transfer Characteristics in SPS VUI parameters should "
          "be defined to '%s', not '%s' "
          "('transfer_characteristics' == 0x%X).\n",
          h264TransferCharacteristicsValueStr(expectedTransferCharact),
          h264TransferCharacteristicsValueStr(colourParam->transfer_characteristics),
          colourParam->transfer_characteristics
        );
        handle->curProgParam.wrongVuiParameters = true;
        MARK_H264_WARNING_FLAG(handle, vuiUnexpectedTranferCharact);
      }

      LIBBLU_H264_DEBUG_VUI(
        "     -> Matrix coefficients "
        "(matrix_coefficients): %s (0x%X).\n",
        h264MatrixCoefficientsValueStr(colourParam->matrix_coefficients),
        colourParam->matrix_coefficients
      );

      expectedMatrixCoeff = getH264BdavExpectedMatrixCoefficients(
        spsParam.FrameHeight
      );

      if (
        CHECK_H264_WARNING_FLAG(handle, vuiUnexpectedMatrixCoeff)
        && expectedMatrixCoeff != colourParam->matrix_coefficients
      ) {
        LIBBLU_WARNING(
          "Matrix coefficients in SPS VUI parameters should "
          "be defined to '%s', not '%s' ('matrix_coefficients' == 0x%X).\n",
          h264MatrixCoefficientsValueStr(expectedMatrixCoeff),
          h264MatrixCoefficientsValueStr(colourParam->matrix_coefficients),
          colourParam->matrix_coefficients
        );
        handle->curProgParam.wrongVuiParameters = true;
        MARK_H264_WARNING_FLAG(handle, vuiUnexpectedMatrixCoeff);
      }
    }
  }
  else {
    handle->curProgParam.missingVSTVuiParameters = true;

    LIBBLU_H264_DEBUG_VUI(
      "    -> Video format (video_format): %s (Inferred).\n",
      h264VideoFormatValueStr(param.video_format)
    );
    LIBBLU_H264_DEBUG_VUI(
      "    -> Full-range luma and chroma samples values "
      "(video_full_range_flag): %s (Inferred).\n",
      BOOL_STR(param.video_full_range_flag)
    );
    LIBBLU_H264_DEBUG_VUI(
      "    -> Color description parameters "
      "(colour_description_present_flag): %s (Inferred).\n",
      BOOL_PRESENCE(param.colour_description_present_flag)
    );
  }

  if (
    CHECK_H264_WARNING_FLAG(handle, vuiMissingVideoSignalType)
    && !param.video_signal_type_present_flag
  ) {
    LIBBLU_WARNING(
      "Video Signal Type parameters shall be present in SPS VUI parameters "
      "according to BD-specs.\n"
    );
    handle->curProgParam.wrongVuiParameters = true;
    MARK_H264_WARNING_FLAG(handle, vuiMissingVideoSignalType);
  }

  if (
    CHECK_H264_WARNING_FLAG(handle, vuiMissingColourDesc)
    && !param.colour_description_present_flag
  ) {
    LIBBLU_WARNING(
      "Color Description parameters shall be present in SPS VUI parameters "
      "according to BD-specs.\n"
    );
    handle->curProgParam.wrongVuiParameters = true;
    MARK_H264_WARNING_FLAG(handle, vuiMissingColourDesc);
  }

  LIBBLU_H264_DEBUG_VUI(
    "   Chroma location information "
    "(chroma_loc_info_present_flag): %s (0b%X).\n",
    BOOL_PRESENCE(param.chroma_loc_info_present_flag),
    param.chroma_loc_info_present_flag
  );

  if (param.chroma_loc_info_present_flag) {
    LIBBLU_H264_DEBUG_VUI(
      "    -> Top-field "
      "(chroma_sample_loc_type_top_field): %u (0x%X).\n",
      param.chroma_sample_loc_type_top_field,
      param.chroma_sample_loc_type_top_field
    );
    LIBBLU_H264_DEBUG_VUI(
      "    -> Bottom-field "
      "(chroma_sample_loc_type_bottom_field): %u (0x%X).\n",
      param.chroma_sample_loc_type_bottom_field,
      param.chroma_sample_loc_type_bottom_field
    );

    if (
      (
        param.chroma_sample_loc_type_top_field != 0
        && param.chroma_sample_loc_type_top_field != 2
      )
      || (
        param.chroma_sample_loc_type_bottom_field != 0
        && param.chroma_sample_loc_type_bottom_field != 2
      )
    ) {
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Chroma sample location if present shall be set to 0 or 2 "
        "according to BD-specs ('chroma_sample_loc_type_top_field' == %d / "
        "'chroma_sample_loc_type_bottom_field' == %d).\n",
        param.chroma_sample_loc_type_top_field,
        param.chroma_sample_loc_type_bottom_field
      );
    }
  }
  else {
    LIBBLU_H264_DEBUG_VUI(
      "    -> Top-field "
      "(chroma_sample_loc_type_top_field): %u (Inferred).\n",
      param.chroma_sample_loc_type_top_field
    );
    LIBBLU_H264_DEBUG_VUI(
      "    -> Bottom-field "
      "(chroma_sample_loc_type_bottom_field): %u (Inferred).\n",
      param.chroma_sample_loc_type_bottom_field
    );
  }

  LIBBLU_H264_DEBUG_VUI(
    "   Timing information (timing_info_present_flag): %s (0b%X).\n",
    BOOL_PRESENCE(param.timing_info_present_flag),
    param.timing_info_present_flag
  );

  if (!param.timing_info_present_flag)
    LIBBLU_H264_ERROR_RETURN(
      "Timing information shall be present in SPS VUI parameters "
      "('timing_info_present_flag' == 0b0).\n"
    );

  if (param.timing_info_present_flag) {
    LIBBLU_H264_DEBUG_VUI(
      "    -> Number of time units (num_units_in_tick): "
      "%" PRIu32 " (0x%" PRIx32 ").\n",
      param.num_units_in_tick,
      param.num_units_in_tick
    );

    LIBBLU_H264_DEBUG_VUI(
      "    -> Time scale clock (time_scale): "
      "%" PRIu32 " Hz (0x%" PRIx32 ").\n",
      param.time_scale,
      param.time_scale
    );

    LIBBLU_H264_DEBUG_VUI(
      "    -> Fixed frame rate "
      "(fixed_frame_rate_flag): %s (0x%X).\n",
      BOOL_STR(param.fixed_frame_rate_flag),
      param.fixed_frame_rate_flag
    );

    if (!param.fixed_frame_rate_flag) {
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Forbidden use of a variable frame-rate mode "
        "('fixed_frame_rate_flag' == 0b0).\n"
      );
    }

    /* Equation D-2 */
    LIBBLU_H264_DEBUG_VUI(
      "    => Frame-rate: %.3f.\n",
      param.FrameRate
    );
  }

  LIBBLU_H264_DEBUG_VUI(
    "   NAL HRD parameters "
    "(nal_hrd_parameters_present_flag): %s (0b%X).\n",
    BOOL_PRESENCE(param.nal_hrd_parameters_present_flag),
    param.nal_hrd_parameters_present_flag
  );

  if (!param.nal_hrd_parameters_present_flag) {
    LIBBLU_H264_DEBUG_VUI(
      "    -> Warning: Absence of NAL HRD parameters is only tolerated "
      "for still-picture steams.\n"
    );
    handle->curProgParam.stillPictureTolerance = true;
  }

  if (param.nal_hrd_parameters_present_flag) {
    if (checkH264HrdParametersCompliance(param.nal_hrd_parameters) < 0)
      return -1;
  }

  LIBBLU_H264_DEBUG_VUI(
    "   VCL HRD parameters "
    "(vcl_hrd_parameters_present_flag): %s (0b%X).\n",
    BOOL_PRESENCE(param.vcl_hrd_parameters_present_flag),
    param.vcl_hrd_parameters_present_flag
  );

  if (param.vcl_hrd_parameters_present_flag) {
    if (checkH264HrdParametersCompliance(param.vcl_hrd_parameters) < 0)
      return -1;
  }

  if (param.nal_hrd_parameters_present_flag || param.vcl_hrd_parameters_present_flag) {
    LIBBLU_H264_DEBUG_VUI(
      "   Low delay (low_delay_hrd_flag): %s (0b%X).\n",
      BOOL_STR(param.low_delay_hrd_flag),
      param.low_delay_hrd_flag
    );

    if (param.low_delay_hrd_flag) {
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Forbidden use of low delay HRD "
        "('low_delay_hrd_flag' == 0b1).\n"
      );
    }
  }

#if 0
  if (param.nal_hrd_parameters_present_flag && param.vcl_hrd_parameters_present_flag) {
    /* Check fields lengths */
    if (
      param.nal_hrd_parameters.initial_cpb_removal_delay_length_minus1
        != param.vcl_hrd_parameters.initial_cpb_removal_delay_length_minus1
      || param.nal_hrd_parameters.cpb_removal_delay_length_minus1
        != param.vcl_hrd_parameters.cpb_removal_delay_length_minus1
      || param.nal_hrd_parameters.dpb_output_delay_length_minus1
        != param.vcl_hrd_parameters.dpb_output_delay_length_minus1
      || param.nal_hrd_parameters.time_offset_length
        != param.vcl_hrd_parameters.time_offset_length
    ) {
      LIBBLU_H264_ERROR_RETURN(
        "Variable CPB/DPB HRD fields lengths.\n"
      );
    }
  }
#endif

  LIBBLU_H264_DEBUG_VUI(
    "   Picture Structure in SEI picture timing messages "
    "(pic_struct_present_flag): %s (0b%X).\n",
    BOOL_PRESENCE(param.pic_struct_present_flag),
    param.pic_struct_present_flag
  );
  /**
   * NOTE: Compliance of 'pic_struct_present_flag' is verified
   * in SPS verifier (calling function).
   */

  LIBBLU_H264_DEBUG_VUI(
    "   Video Sequence Bitstream Restriction parameters "
    "(bitstream_restriction_flag): %s (0b%X).\n",
    BOOL_PRESENCE(param.bitstream_restriction_flag),
    param.bitstream_restriction_flag
  );

  if (param.bitstream_restriction_flag) {
    const H264VuiVideoSeqBitstreamRestrictionsParameters * restrParam;
    restrParam = &param.bistream_restrictions;

    LIBBLU_H264_DEBUG_VUI(
      "    Samples outside picture boundaries in Motion Vectors "
      "(motion_vectors_over_pic_boundaries_flag): %s (0b%X).\n",
      BOOL_PRESENCE(restrParam->motion_vectors_over_pic_boundaries_flag),
      restrParam->motion_vectors_over_pic_boundaries_flag
    );

    LIBBLU_H264_DEBUG_VUI(
      "    Maximum picture size "
      "(max_bytes_per_pic_denom): "
    );
    if (restrParam->max_bytes_per_pic_denom != 0) {
      LIBBLU_H264_DEBUG_VUI_NH(
        "%" PRIu8 " bytes",
        restrParam->max_bytes_per_pic_denom
      );
    }
    else
      LIBBLU_H264_DEBUG_VUI_NH("Unlimited");
    LIBBLU_H264_DEBUG_VUI_NH(
      " (0x%02" PRIX8 ").\n",
      restrParam->max_bytes_per_pic_denom
    );

    LIBBLU_H264_DEBUG_VUI(
      "    Maximum macro-block size (max_bits_per_mb_denom): "
    );
    if (restrParam->max_bits_per_mb_denom != 0) {
      LIBBLU_H264_DEBUG_VUI_NH(
        "%" PRIu8 " bits",
        restrParam->max_bits_per_mb_denom
      );
    }
    else
      LIBBLU_H264_DEBUG_VUI_NH("Unlimited");
    LIBBLU_H264_DEBUG_VUI_NH(
      " (0x%02" PRIX8 ").\n",
      restrParam->max_bits_per_mb_denom
    );

    LIBBLU_H264_DEBUG_VUI(
      "    Maximum horizontal motion vector length "
      "(log2_max_mv_length_horizontal): "
      "%.2f luma sample(s) (0x%02" PRIX8 ").\n",
      0.25 * restrParam->log2_max_mv_length_horizontal,
      restrParam->log2_max_mv_length_horizontal
    );

    LIBBLU_H264_DEBUG_VUI(
      "    Maximum vertical motion vector length "
      "(log2_max_mv_length_vertical): "
      "%.2f luma sample(s) (0x%02" PRIX8 ").\n",
      0.25 * restrParam->log2_max_mv_length_vertical,
      restrParam->log2_max_mv_length_vertical
    );

    LIBBLU_H264_DEBUG_VUI(
      "    Maximum number of frames in DPB "
      "(max_num_reorder_frames): "
      "%" PRIu16 " frame buffers (0x%04" PRIX8 ").\n",
      restrParam->max_num_reorder_frames,
      restrParam->max_num_reorder_frames
    );

    LIBBLU_H264_DEBUG_VUI(
      "    HRD-DPB required size "
      "(max_dec_frame_buffering): "
      "%" PRIu16 " frame buffers (0x%04" PRIX8 ").\n",
      restrParam->max_dec_frame_buffering,
      restrParam->max_dec_frame_buffering
    );
  }

  return 0;
}

static const char * getSequenceScalingListName(
  int indice
)
{
  switch (indice) {
    case 0:
      return "Sl_4x4_Intra_Y";
    case 1:
      return "Sl_4x4_Intra_Cb";
    case 2:
      return "Sl_4x4_Intra_Cr";
    case 3:
      return "Sl_4x4_Inter_Y";
    case 4:
      return "Sl_4x4_Inter_Cb";
    case 5:
      return "Sl_4x4_Inter_Cr";
    case 6:
      return "Sl_8x8_Intra_Y";
    case 7:
      return "Sl_8x8_Inter_Y";
    case 8:
      return "Sl_8x8_Intra_Cb";
    case 9:
      return "Sl_8x8_Inter_Cb";
    case 10:
      return "Sl_8x8_Intra_Cr";
    case 11:
      return "Sl_8x8_Inter_Cr";
  }

  return "UNKNOWN";
}

int checkH264SequenceParametersSetCompliance(
  const H264SPSDataParameters param,
  H264ParametersHandlerPtr handle
)
{
  int ret;

  unsigned i;
  unsigned MaxFS, MaxDpbMbs, MaxDpbFrames;

  assert(NULL != handle);

  ret = checkH264ProfileIdcCompliance(
    param.profile_idc,
    param.constraint_set_flags
  );
  if (ret < 0)
    return -1;
  updateH264ProfileLimits(
    handle,
    param.profile_idc,
    param.constraint_set_flags,
    true
  );

  LIBBLU_H264_DEBUG_SPS(
    "  Computed profile limits:\n"
  );

  LIBBLU_H264_DEBUG_SPS(
    "   => cpbBrVclFactor: %u.\n",
    handle->constraints.cpbBrVclFactor
  );
  LIBBLU_H264_DEBUG_SPS(
    "   => cpbBrNalFactor: %u.\n",
    handle->constraints.cpbBrNalFactor
  );

  if (
    !handle->constraints.cpbBrVclFactor
    || !handle->constraints.cpbBrNalFactor
  ) {
    LIBBLU_H264_DEBUG_SPS(
      "   NOTE: 0 value means 'non-concerned' or 'unsupported profile' "
      "for this specific parameter.\n"
    );
  }

  if (checkH264LevelIdcCompliance(param.level_idc) < 0)
    return -1;
  updateH264LevelLimits(handle, param.level_idc);

  LIBBLU_H264_DEBUG_SPS(
    "  Computed level limits:\n"
  );

  LIBBLU_H264_DEBUG_SPS(
    "   => MaxMBPS (Max macroblock processing rate in MB/s): %u.\n",
    handle->constraints.MaxMBPS
  );
  LIBBLU_H264_DEBUG_SPS(
    "   => MaxFS (Max frame size in MBs): %u.\n",
    handle->constraints.MaxFS
  );
  LIBBLU_H264_DEBUG_SPS(
    "   => MaxDpbMbs (Max decoded picture buffer size in MBs): %u.\n",
    handle->constraints.MaxDpbMbs
  );
  LIBBLU_H264_DEBUG_SPS(
    "   => MaxBR (Max video bitrate in *factor* bits/s): %u.\n",
    handle->constraints.MaxBR
  );
  LIBBLU_H264_DEBUG_SPS(
    "   => MaxCPB (Max CPB size in *factor* bits): %u.\n",
    handle->constraints.MaxCPB
  );
  LIBBLU_H264_DEBUG_SPS(
    "    -> *factor* value varies according to the context of use.\n"
  );
  LIBBLU_H264_DEBUG_SPS(
    "   => MaxVmvR (Vertical MV component limit in luma frame samples): %u.\n",
    handle->constraints.MaxVmvR
  );
  LIBBLU_H264_DEBUG_SPS(
    "   => MinCR (Minimal compression ratio): %u.\n",
    handle->constraints.MinCR
  );
  LIBBLU_H264_DEBUG_SPS(
    "   => MaxMvsPer2Mb (Max number of motion vectors per two consecutive MBs): %u.\n",
    handle->constraints.MaxMvsPer2Mb
  );

  if (
    !handle->constraints.MaxMBPS
    || !handle->constraints.MaxFS
    || !handle->constraints.MaxDpbMbs
    || !handle->constraints.MaxBR
    || !handle->constraints.MaxCPB
    || !handle->constraints.MaxVmvR
    || !handle->constraints.MinCR
    || !handle->constraints.MaxMvsPer2Mb
  ) {
    LIBBLU_H264_DEBUG_SPS(
      "    NOTE: 0 values means 'non-concerned' or 'unsupported level' "
      "for this specific parameter.\n"
    );
  }

  if (0 == (MaxFS = handle->constraints.MaxFS)) {
    LIBBLU_H264_ERROR_RETURN(
      "Unable to a find a MaxFS value for 'level_idc' == 0x%02" PRIX8 ".\n",
      param.level_idc
    );
  }

  handle->constraints.brNal = getH264BrNal(
    handle->constraints,
    param.profile_idc
  );

  LIBBLU_H264_DEBUG_SPS(
    "   => BitRate "
    "(Maximum allowed NAL HRD Bit Rate in bits/sec): %u.\n",
    handle->constraints.brNal
  );

  LIBBLU_H264_DEBUG_SPS(
    "  Sequence Parameter Set ID (seq_parameter_set_id): %u (0x%X).\n",
    param.seq_parameter_set_id,
    param.seq_parameter_set_id
  );

  /**
   * TODO: Enable better check of non multiple SPS ID.
   */
  if (param.seq_parameter_set_id != 0)
    LIBBLU_H264_ERROR_RETURN(
      "Unsupported multiple Sequence Parameters Set definitions, "
      "only 'seq_parameter_set_id' == 0x0 supported.\n"
    );

  if (H264_PROFILE_IS_HIGH(param.profile_idc)) {
    LIBBLU_H264_DEBUG_SPS(
      "  Chroma sampling format (chroma_format_idc): %s (0x%X).\n",
      h264ChromaFormatIdcValueStr(param.chroma_format_idc),
      param.chroma_format_idc
    );

    if (H264_CHROMA_FORMAT_444 < param.chroma_format_idc)
      LIBBLU_H264_ERROR_RETURN(
        "Reserved 'chroma_format_idc' == %d in use.\n",
        param.chroma_format_idc
      );

    if (param.chroma_format_idc == H264_CHROMA_FORMAT_444)
      LIBBLU_H264_DEBUG_SPS(
        "  Separate color channels planes (separate_colour_plane_flag): "
        "%s (0x).\n",
        BOOL_STR(param.separate_colour_plane_flag),
        param.separate_colour_plane_flag
      );

    if (
      handle->constraints.restrictedChromaFormatIdc
      != H264_UNRESTRICTED_CHROMA_FORMAT_IDC
    ) {
      /* Check chroma_format_idc */

      if (
        !(
          (1 << param.chroma_format_idc)
          & handle->constraints.restrictedChromaFormatIdc
        )
      ) {
        LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
          "Unexpected Chroma sampling format %s "
          "('chroma_format_idc' == %d).\n",
          h264ChromaFormatIdcValueStr(param.chroma_format_idc),
          param.chroma_format_idc
        );
      }
    }

    LIBBLU_H264_DEBUG_SPS(
      "  Luma samples Bit Depth value (bit_depth_luma_minus8): "
      "%" PRIu8 " (0x%02" PRIX8 ").\n",
      param.bit_depth_luma_minus8,
      param.bit_depth_luma_minus8
    );

    LIBBLU_H264_DEBUG_SPS(
      "   => BitDepth_Y (Luma samples Bit Depth): %u bits.\n",
      param.BitDepthLuma
    );

    LIBBLU_H264_DEBUG_SPS(
      "   => QpBdOffset_Y (Luma quantization parameter range): %u.\n",
      param.QpBdOffsetLuma
    );

    LIBBLU_H264_DEBUG_SPS(
      "  Chroma samples Bit Depth value (bit_depth_chroma_minus8): "
      "%" PRIu8 " (0x%02" PRIX8 ").\n",
      param.bit_depth_chroma_minus8,
      param.bit_depth_chroma_minus8
    );

    LIBBLU_H264_DEBUG_SPS(
      "   => BitDepth_C (Chroma samples Bit Depth): %u bits.\n",
      param.BitDepthChroma
    );

    LIBBLU_H264_DEBUG_SPS(
      "   => QpBdOffset_C (Chroma quantization parameter range): %u.\n",
      param.QpBdOffsetChroma
    );

    if (param.chroma_format_idc == H264_CHROMA_FORMAT_400) {
      LIBBLU_H264_DEBUG_SPS(
        "  -> Not used, only luma is present in monochrome mode.\n"
      );
    }
    else if (
      param.chroma_format_idc == H264_CHROMA_FORMAT_444
      && param.separate_colour_plane_flag
    )
      LIBBLU_H264_DEBUG_SPS(
        "  -> Not used, separated color planes are coded according to Luma "
        "Bit-depth value.\n"
      );

    if (
      handle->constraints.maxAllowedBitDepthMinus8
        < param.bit_depth_luma_minus8
      || handle->constraints.maxAllowedBitDepthMinus8
        < param.bit_depth_chroma_minus8
    ) {
      LIBBLU_ERROR(
        "Luma and/or chroma bit-depth used are not allowed for "
        "bitstream profile conformance.\n"
      );

      if (
        handle->constraints.maxAllowedBitDepthMinus8
        < param.bit_depth_luma_minus8
      )
        LIBBLU_ERROR(
          " -> Luma bit-depth: %u bits over %u bits maximal "
          "allowed value.\n",
          param.BitDepthLuma,
          handle->constraints.maxAllowedBitDepthMinus8 + 8
        );

      if (
        handle->constraints.maxAllowedBitDepthMinus8
        < param.bit_depth_chroma_minus8
      )
        LIBBLU_ERROR(
          " -> Chroma bit-depth: %u bits over %u bits maximal "
          "allowed value.\n",
          param.BitDepthChroma,
          handle->constraints.maxAllowedBitDepthMinus8 + 8
        );

      return -1;
    }

    LIBBLU_H264_DEBUG_SPS(
      "   => RawMbBits (Raw Macroblock size in bits): %" PRIu64 " bits.\n",
      param.RawMbBits
    );

    LIBBLU_H264_DEBUG_SPS(
      "  Transform bypass when QP'Y equal 0 "
      "(qpprime_y_zero_transform_bypass_flag): %s (0x%X).\n",
      BOOL_STR(param.qpprime_y_zero_transform_bypass_flag),
      param.qpprime_y_zero_transform_bypass_flag
    );

    if (
      param.qpprime_y_zero_transform_bypass_flag
      && handle->constraints.forbiddenQpprimeYZeroTransformBypass
    ) {
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Transform bypass operation for QP'_Y coefficients equal to "
        "zero is not allowed for bitstream profile conformance "
        "('qpprime_y_zero_transform_bypass_flag' == 0b1).\n"
      );
    }

    LIBBLU_H264_DEBUG_SPS(
      "  Sequence Scaling Matrix "
      "(seq_scaling_matrix_present_flag): %s (0x%X).\n",
      BOOL_PRESENCE(param.seq_scaling_matrix_present_flag),
      param.seq_scaling_matrix_present_flag
    );

    if (!param.seq_scaling_matrix_present_flag)
      LIBBLU_H264_DEBUG_SPS(
        "   -> Use Flat_4x4_16 and Flat_8x8_16 scaling lists.\n"
      );

    if (param.seq_scaling_matrix_present_flag) {
      unsigned nbLists;

      LIBBLU_H264_DEBUG_SPS("  Sequence Scaling Lists:\n");

      nbLists = (param.chroma_format_idc != H264_CHROMA_FORMAT_444) ? 8 : 12;
      for (i = 0; i < nbLists; i++) {
        LIBBLU_H264_DEBUG_SPS(
          "   -> %s (seq_scaling_list_present_flag[%X]): %s (0b%X).\n",
          getSequenceScalingListName(i), i,
          BOOL_PRESENCE(param.seq_scaling_matrix.seq_scaling_list_present_flag[i]),
          param.seq_scaling_matrix.seq_scaling_list_present_flag[i]
        );
      }
    }
  }
#if 1
  else {
    LIBBLU_H264_DEBUG_SPS(
      "  Chroma sampling format (chroma_format_idc): %s (Inferred).\n",
      h264ChromaFormatIdcValueStr(param.chroma_format_idc)
    );
    LIBBLU_H264_DEBUG_SPS(
      "  Luma samples Bit Depth value (bit_depth_luma_minus8): "
      "%" PRIu8 " (Inferred)",
      param.bit_depth_luma_minus8
    );
    LIBBLU_H264_DEBUG_SPS(
      "   => BitDepth_Y (Luma samples Bit Depth): %u bits.\n",
      param.BitDepthLuma
    );
    LIBBLU_H264_DEBUG_SPS(
      "   => QpBdOffset_Y (Luma quantization parameter range): %u.\n",
      param.QpBdOffsetLuma
    );
    LIBBLU_H264_DEBUG_SPS(
      "  Chroma samples Bit Depth value (bit_depth_chroma_minus8): "
      "%" PRIu8 " (Inferred).\n",
      param.bit_depth_chroma_minus8
    );
    LIBBLU_H264_DEBUG_SPS(
      "   => BitDepth_C (Chroma samples Bit Depth): %u bits.\n",
      param.BitDepthChroma
    );
    LIBBLU_H264_DEBUG_SPS(
      "   => QpBdOffset_C (Chroma quantization parameter range): %u.\n",
      param.QpBdOffsetChroma
    );
    LIBBLU_H264_DEBUG_SPS(
      "   => Transform bypass when QP'Y equal 0 "
      "(qpprime_y_zero_transform_bypass_flag): %s (Inferred).\n",
      BOOL_STR(param.qpprime_y_zero_transform_bypass_flag)
    );
    LIBBLU_H264_DEBUG_SPS(
      "  Sequence Scaling Matrix "
      "(seq_scaling_matrix_present_flag): %s (Inferred).\n",
      BOOL_PRESENCE(param.seq_scaling_matrix_present_flag)
    );
    if (!param.seq_scaling_matrix_present_flag)
      LIBBLU_H264_DEBUG_SPS(
        "   -> Use Flat_4x4_16 and Flat_8x8_16 scaling lists.\n"
      );
  }
#endif

  LIBBLU_H264_DEBUG_SPS(
    "  Max Frame number value "
    "(log2_max_frame_num_minus4): %u (0x%02" PRIX8 ").\n",
    param.log2_max_frame_num_minus4,
    param.log2_max_frame_num_minus4
  );

  LIBBLU_H264_DEBUG_SPS(
    "   => MaxFrameNum: %u.\n",
    param.MaxFrameNum
  );

  if (12 < param.log2_max_frame_num_minus4) {
    LIBBLU_H264_ERROR_RETURN(
      "Out of range 'log2_max_frame_num_minus4' == %u, "
      "shall not exceed 12 (Rec. ITU-T H.264 7.4.2.1.1).\n",
      param.log2_max_frame_num_minus4
    );
  }

  LIBBLU_H264_DEBUG_SPS(
    "  Picture order count type "
    "(pic_order_cnt_type): Type %" PRIu8 " (0x%02" PRIX8 ").\n",
    param.pic_order_cnt_type,
    param.pic_order_cnt_type
  );

  switch (param.pic_order_cnt_type) {
    case 0:
      LIBBLU_H264_DEBUG_SPS(
        "   -> Max picture order count LSB value "
        "(log2_max_pic_order_cnt_lsb_minus4): %d (0x%02" PRIX8 ").\n",
        param.log2_max_pic_order_cnt_lsb_minus4,
        param.log2_max_pic_order_cnt_lsb_minus4
      );
      LIBBLU_H264_DEBUG_SPS(
        "    => MaxPicOrderCntLsb: %u.\n",
        param.MaxPicOrderCntLsb
      );

      if (12 < param.log2_max_pic_order_cnt_lsb_minus4) {
        LIBBLU_H264_ERROR_RETURN(
          "Out of range 'log2_max_pic_order_cnt_lsb_minus4' == %u, "
          "shall not exceed 12 (Rec. ITU-T H.264 7.4.2.1.1).\n",
          param.log2_max_pic_order_cnt_lsb_minus4
        );
      }
      break;

    case 1:
      LIBBLU_H264_DEBUG_SPS(
        "   -> Delta picture order counter "
        "(delta_pic_order_always_zero_flag): %s (0x%X).\n",
        BOOL_PRESENCE(!param.delta_pic_order_always_zero_flag),
        param.delta_pic_order_always_zero_flag
      );

      if (param.delta_pic_order_always_zero_flag) {
        LIBBLU_H264_DEBUG_SPS(
          "    -> 'delta_pic_order_cnt[0]' and 'delta_pic_order_cnt[1]' "
          "not present and inferred to be equal to 0.\n"
        );
      }

      LIBBLU_H264_DEBUG_SPS(
        "   -> Offset for non-referential picture "
        "(offset_for_non_ref_pic): %d (0x%X).\n",
        param.offset_for_non_ref_pic,
        param.offset_for_non_ref_pic
      );

      LIBBLU_H264_DEBUG_SPS(
        "   -> Offset for bottom-field "
        "(offset_for_top_to_bottom_field): %d (0x%X).\n",
        param.offset_for_top_to_bottom_field,
        param.offset_for_top_to_bottom_field
      );

      LIBBLU_H264_DEBUG_SPS(
        "   -> Number of referential frames in picture order counter cycle "
        "(num_ref_frames_in_pic_order_cnt_cycle): %u (0x%X)",
        param.num_ref_frames_in_pic_order_cnt_cycle,
        param.num_ref_frames_in_pic_order_cnt_cycle
      );

      for (i = 0; i < param.num_ref_frames_in_pic_order_cnt_cycle; i++) {
        LIBBLU_H264_DEBUG_SPS(
          "    -> Offset for referential frame %u "
          "(offset_for_ref_frame[%u]): %d (0x%X).\n",
          i, i,
          param.offset_for_ref_frame[i],
          param.offset_for_ref_frame[i]
        );
      }

      LIBBLU_H264_DEBUG_SPS(
        "     => ExpectedDeltaPerPicOrderCntCycle: %d.\n",
        param.ExpectedDeltaPerPicOrderCntCycle
      );
      break;

    case 2:
      /**
       * No extra parameters needed.
       * Forbids the presence of two consecutive non-referential
       * frames/non-complementary fields.
       */
      handle->curProgParam.picOrderCntType2use = true;
      break;

    default:
      LIBBLU_H264_DEBUG_SPS(
        "   -> Reserved value (Unknown type).\n"
      );

      LIBBLU_H264_ERROR_RETURN(
        "Reserved 'pic_order_cnt_type' == %" PRIu8 " in use, "
        "unknown picture order count type (expect values between 0 and 2).\n",
        param.pic_order_cnt_type
      );
  }

  LIBBLU_H264_DEBUG_SPS(
    "  Max number of referential frames "
    "(max_num_ref_frames): %u (0x%X).\n",
    param.max_num_ref_frames,
    param.max_num_ref_frames
  );

  if (0 == (MaxDpbMbs = handle->constraints.MaxDpbMbs)) {
    LIBBLU_H264_ERROR_RETURN(
      "Unable to a find a MaxDpbMbs value for 'level_idc' == 0x%02" PRIX8 ".\n",
      param.level_idc
    );
  }

  /* Equation A.3.1.h) */
  MaxDpbFrames = MIN(
    MaxDpbMbs / (param.PicWidthInMbs * param.FrameHeightInMbs),
    16
  );

  if (MaxDpbFrames < param.max_num_ref_frames) {
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "Out of range 'max_num_ref_frames' == %u in SPS, shall not exceed "
      "'MaxDpbFrames' == %u for bitstream level conformance.\n",
      param.max_num_ref_frames, MaxDpbFrames
    );
  }

  LIBBLU_H264_DEBUG_SPS(
    "  Gaps allowed in frame number values "
    "(gaps_in_frame_num_value_allowed_flag): %s (0x%X).\n",
    BOOL_STR(param.gaps_in_frame_num_value_allowed_flag),
    param.gaps_in_frame_num_value_allowed_flag
  );

  if (param.gaps_in_frame_num_value_allowed_flag) {
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "Unexpected allowed presence of gaps in frame number value "
      "('gaps_in_frame_num_value_allowed_flag' == 0b1).\n"
    );
  }

  LIBBLU_H264_DEBUG_SPS(
    "  Decoded Picture Width in MacroBlocks value "
    "(pic_width_in_mbs_minus1): %u (0x%X).\n",
    param.pic_width_in_mbs_minus1,
    param.pic_width_in_mbs_minus1
  );

  LIBBLU_H264_DEBUG_SPS(
    "   => PicWidthInMbs (Picture width in MacroBlocks): %u MBs.\n",
    param.PicWidthInMbs
  );

  /* Constraint H.264 A.3.1.f) / A.3.2.d) */
  if (MaxFS * 8 < SQUARE(param.PicWidthInMbs)) {
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "Rec. ITU-T H.264 %s constraint is not satisfied "
      "(Sqrt(MaxFS * 8) = %u < PicWidthInMbs = %u "
      "for level %" PRIu8 ".%" PRIu8 "), "
      "leading a violation of bitstream profile/level conformance.\n",
      H264_PROFILE_IS_HIGH(param.profile_idc) ? "A.3.2.d)" : "A.3.1.f)",
      (unsigned) sqrt(MaxFS * 8),
      param.PicWidthInMbs,
      param.level_idc / 10, param.level_idc % 10
    );
  }

  LIBBLU_H264_DEBUG_SPS(
    "   => PicWidthInSamplesL "
    "(Raw Picture width in luma samples): %u samples.\n",
    param.PicWidthInSamplesL
  );

  LIBBLU_H264_DEBUG_SPS(
    "   => PicWidthInSamplesC "
    "(Raw Picture width in chroma samples): %u samples.\n",
    param.PicWidthInSamplesC
  );

  LIBBLU_H264_DEBUG_SPS(
    "   => FrameWidth "
    "(decoded frame width after applying cropping): %u pixels.\n",
    param.FrameWidth
  );

  LIBBLU_H264_DEBUG_SPS(
    "  Decoded Frame/Field Height in slice group map units value "
    "(pic_height_in_map_units_minus1): %u (0x%X).\n",
    param.pic_height_in_map_units_minus1,
    param.pic_height_in_map_units_minus1
  );

  LIBBLU_H264_DEBUG_SPS(
    "   => PicHeightInMapUnits "
    "(Picture width in slice group map units): %u map units.\n",
    param.PicHeightInMapUnits
  );

  LIBBLU_H264_DEBUG_SPS(
    "   => PicSizeInMapUnits "
    "(Picture size in slice group map units): %u map units.\n",
    param.PicWidthInMbs * param.PicHeightInMapUnits
  );

  LIBBLU_H264_DEBUG_SPS(
    "  Progressive stream, only using frame-typed macroblocks "
    "(frame_mbs_only_flag): %s (0x%X).\n",
    BOOL_STR(param.frame_mbs_only_flag),
    param.frame_mbs_only_flag
  );

  if (!param.frame_mbs_only_flag) {
    if (handle->constraints.restrictedFrameMbsOnlyFlag) {
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Possible presence of interlaced pictures is not allowed "
        "for bitstream profile conformance ('frame_mbs_only_flag' == 0b0).\n"
      );
    }

    /* Constraint Rec. ITU-T H.264 A.3.3.c) */
    if (
      handle->constraints.frame_mbs_only_flag &&
      (
        param.profile_idc == H264_PROFILE_MAIN
        || param.profile_idc == H264_PROFILE_EXTENDED
        || param.profile_idc == H264_PROFILE_HIGH
        || param.profile_idc == H264_PROFILE_HIGH_10
        || param.profile_idc == H264_PROFILE_HIGH_422
        || param.profile_idc == H264_PROFILE_HIGH_444_PREDICTIVE
        || param.profile_idc == H264_PROFILE_CAVLC_444_INTRA
      )
    ) {
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Possible presence of interlaced pictures is not allowed "
        "for bitstream level conformance ('frame_mbs_only_flag' == 0b0).\n"
      );
    }
  }

  LIBBLU_H264_DEBUG_SPS(
    "   => FrameHeightInMbs (Frame height in macroblocks): %u MBs.\n",
    param.FrameHeightInMbs
  );

  /* Constraint Rec. ITU-T H.264 A.3.1.g) / A.3.2.e) */
  if (MaxFS * 8 < SQUARE(param.FrameHeightInMbs)) {
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "Rec. ITU-T H.264 %s constraint is not satisfied "
      "(Sqrt(MaxFS * 8) = %u < FrameHeightInMbs = %u "
      "for level %" PRIu8 ".%" PRIu8 ") "
      "leading a violation of bitstream profile/level conformance.\n",
      H264_PROFILE_IS_HIGH(param.profile_idc) ? "A.3.2.e)" : "A.3.1.g)",
      (unsigned) sqrt(MaxFS * 8),
      param.FrameHeightInMbs,
      param.level_idc / 10, param.level_idc % 10
    );
  }

  LIBBLU_H264_DEBUG_SPS(
    "   => FrameHeight "
    "(decoded frame height after applying cropping): %u pixels.\n",
    param.FrameHeight
  );

  /* Constraint Rec. ITU-T H.264 A.3.1.e) / A.3.2.c) */
  if (MaxFS < param.PicWidthInMbs * param.FrameHeightInMbs) {
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "Rec. ITU-T H.264 %s constraint is not satisfied "
      "(MaxFS = %u < PicWidthInMbs * FrameHeightInMbs = %u "
      "for level %" PRIu8 ".%" PRIu8 ") "
      "leading a violation of bitstream profile/level conformance.\n",
      H264_PROFILE_IS_HIGH(param.profile_idc) ? "A.3.2.c)" : "A.3.1.e)",
      MaxFS,
      param.PicWidthInMbs * param.FrameHeightInMbs,
      param.level_idc / 10, param.level_idc % 10
    );
  }

  if (!param.frame_mbs_only_flag) {
    LIBBLU_H264_DEBUG_SPS(
      "   -> Adaptive frame/field macroblocks switching "
      "(mb_adaptive_frame_field_flag): %s (0x%X).\n",
      (param.mb_adaptive_frame_field_flag) ? "Allowed" : "Forbidden",
      param.mb_adaptive_frame_field_flag
    );

    if (param.mb_adaptive_frame_field_flag) {
      LIBBLU_H264_DEBUG_SPS(
        "    => MacroBlock-Adaptive Frame-Field coding - MBAFF.\n"
      );
    }
    else {
      LIBBLU_H264_DEBUG_SPS(
        "    => Picture-Adaptive Frame-Field coding - PicAFF.\n"
      );
    }
  }

  LIBBLU_H264_DEBUG_SPS(
    "  Motion vectors computing level "
    "(direct_8x8_inference_flag): %s blocks level (0x%X).\n",
    (param.direct_8x8_inference_flag) ? "8x8" : "4x4",
    param.direct_8x8_inference_flag
  );

  if (!param.direct_8x8_inference_flag) {
    if (!param.frame_mbs_only_flag) {
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Derivation process mode flag 'direct_8x8_inference_flag' should be "
        "equal to 0b1 since 'frame_mbs_only_flag' == 0b0.\n"
      );
    }

    if (handle->constraints.forcedDirect8x8InferenceFlag) {
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Derivation process mode flag 'direct_8x8_inference_flag' == 0b0 is "
        "not allowed for bitstream profile conformance.\n"
      );
    }

    /* Constraint Rec. ITU-T H.264 A.3.3.c) */
    if (
      handle->constraints.direct_8x8_inference_flag &&
      (
        param.profile_idc == H264_PROFILE_MAIN
        || param.profile_idc == H264_PROFILE_HIGH
        || param.profile_idc == H264_PROFILE_HIGH_10
        || param.profile_idc == H264_PROFILE_HIGH_422
        || param.profile_idc == H264_PROFILE_HIGH_444_PREDICTIVE
      )
    ) {
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Derivation process mode flag 'direct_8x8_inference_flag' == 0b0 is "
        "not allowed for bitstream level conformance.\n"
      );
    }
  }

  LIBBLU_H264_DEBUG_SPS(
    "  Frame cropping (frame_cropping_flag): %s (0x%X).\n",
    BOOL_STR(param.frame_cropping_flag),
    param.frame_cropping_flag
  );

  if (param.frame_cropping_flag) {
    LIBBLU_H264_DEBUG_SPS(
      "   -> Left (frame_crop_left_offset): %u pixels (0x%X).\n",
      param.frame_crop_offset.left * param.CropUnitX,
      param.frame_crop_offset.left
    );

    LIBBLU_H264_DEBUG_SPS(
      "   -> Right (frame_crop_right_offset): %u pixels (0x%X).\n",
      param.frame_crop_offset.right * param.CropUnitX,
      param.frame_crop_offset.right
    );

    LIBBLU_H264_DEBUG_SPS(
      "   -> Top (frame_crop_top_offset): %u pixels (0x%X).\n",
      param.frame_crop_offset.top * param.CropUnitY,
      param.frame_crop_offset.top
    );

    LIBBLU_H264_DEBUG_SPS(
      "   -> Bottom (frame_crop_bottom_offset): %u pixels (0x%X).\n",
      param.frame_crop_offset.bottom * param.CropUnitY,
      param.frame_crop_offset.bottom
    );

    LIBBLU_H264_DEBUG_SPS(
      "   => ChromaArrayType: %u.\n",
      param.ChromaArrayType
    );

    LIBBLU_H264_DEBUG_SPS(
      "   => CropUnitX: %u.\n",
      param.CropUnitX
    );

    LIBBLU_H264_DEBUG_SPS(
      "   => CropUnitY: %u.\n",
      param.CropUnitY
    );
  }

  LIBBLU_H264_DEBUG_SPS(
    "  Video Usability Information, VUI parameters "
    "(vui_parameters_present_flag): %s (0x%X).\n",
    BOOL_PRESENCE(param.vui_parameters_present_flag),
    param.vui_parameters_present_flag
  );

  if (!param.vui_parameters_present_flag) {
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "According to BD-specs, VUI parameters shall be present in SPS "
      "('vui_parameters_present_flag' == 0b0).\n"
    );
  }

  if (param.vui_parameters_present_flag) {
    ret = checkH264VuiParametersCompliance(
      param.vui_parameters,
      param,
      handle
    );
    if (ret < 0)
      return -1;

    if (!param.frame_mbs_only_flag && !param.vui_parameters.pic_struct_present_flag) {
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "According to BD-specs 'pic_struct' shall be present in "
        "Picture Timing SEI for interlaced video "
        "('pic_struct_present_flag' == 0b0).\n"
      );
    }
  }

  LIBBLU_H264_DEBUG_SPS("  # Target parameters:\n");

  if (FLOAT_COMPARE(param.vui_parameters.FrameRate, 23.976))
    handle->constraints.gopMaxLength = 24;
  else if (param.vui_parameters.FrameRate == 24)
    handle->constraints.gopMaxLength = 24;
  else if (FLOAT_COMPARE(param.vui_parameters.FrameRate, 29.970))
    handle->constraints.gopMaxLength = 30;
  else if (param.vui_parameters.FrameRate == 25)
    handle->constraints.gopMaxLength = 25;
  else if (FLOAT_COMPARE(param.vui_parameters.FrameRate, 59.940))
    handle->constraints.gopMaxLength = 60;
  else /* 50 */
    handle->constraints.gopMaxLength = 50;

  LIBBLU_H264_DEBUG_SPS(
    "   -> GOP maximal length "
    "(interval between two I pictures): %u pictures.\n",
    handle->constraints.gopMaxLength
  );

  if (param.level_idc == 41)
    handle->constraints.sliceNb = 4;
  else
    handle->constraints.sliceNb = 1;

  LIBBLU_H264_DEBUG_SPS(
    "   -> Number of slices: %u slice(s).\n",
    handle->constraints.sliceNb
  );

  handle->constraints.consecutiveBPicNb = H264_BDAV_MAX_CONSECUTIVE_B_PICTURES;

  LIBBLU_H264_DEBUG_SPS(
    "   -> Max number of consecutive B pictures: %u picture(s).\n",
    handle->constraints.consecutiveBPicNb
  );

  return 0;
}

int checkH264SpsBitstreamCompliance(
  const H264SPSDataParameters param,
  const LibbluESSettingsOptions options
)
{
  CheckVideoConfigurationRet ret;
  HdmvFrameRateCode frCode;

  assert(param.vui_parameters_present_flag);

  frCode = getHdmvFrameRateCode(param.vui_parameters.FrameRate);
  if (!frCode)
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "Unallowed frame-rate value %.3f, do not form part of BD-specs.\n",
      param.vui_parameters.FrameRate
    );

  if (options.secondaryStream)
    ret = checkSecVideoConfiguration(
      param.FrameWidth,
      param.FrameHeight,
      frCode,
      !param.frame_mbs_only_flag
    );
  else
    ret = checkPrimVideoConfiguration(
      param.FrameWidth,
      param.FrameHeight,
      frCode,
      !param.frame_mbs_only_flag
    );

  switch (ret) {
    case CHK_VIDEO_CONF_RET_OK:
      break;

    case CHK_VIDEO_CONF_RET_ILL_VIDEO_SIZE:
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Unallowed video definition %ux%u, do not form part of BD-specs.\n",
        param.FrameWidth,
        param.FrameHeight
      );

    case CHK_VIDEO_CONF_RET_ILL_FRAME_RATE:
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Frame-rate value %.3f is not allowed for video definition %ux%u "
        "according to BD-specs.\n",
        param.vui_parameters.FrameRate,
        param.FrameWidth,
        param.FrameHeight
      );

    case CHK_VIDEO_CONF_RET_ILL_DISP_MODE:
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "%s display mode is forbidden in the configuration of "
        "video definition %ux%u at frame-rate %.3f "
        "according to BD-specs.\n",
        (param.frame_mbs_only_flag) ? "Progressive" : "Interlaced",
        param.FrameWidth,
        param.FrameHeight,
        param.vui_parameters.FrameRate
      );
  }

  return 0;
}

int checkH264PicParametersSetCompliance(
  const H264PicParametersSetParameters param,
  const H264ParametersHandlerPtr handle
)
{
  unsigned i, j;

  bool validEntropyCodingMode;

  LIBBLU_H264_DEBUG_PPS(
    "  Picture Parameter Set id (pic_parameter_set_id): %u (0x%X).\n",
    param.pic_parameter_set_id,
    param.pic_parameter_set_id
  );

  if (H264_MAX_PPS <= param.pic_parameter_set_id)
    LIBBLU_H264_ERROR_RETURN(
      "PPS pic_parameter_set_id overflow ('pic_parameter_set_id' == %u).\n",
      param.pic_parameter_set_id
    );

  if (!handle->picParametersSetIdsPresent[param.pic_parameter_set_id]) {
    /* New PPS entry, check for maximum of allowed PPS */
    if (H264_MAX_ALLOWED_PPS <= handle->picParametersSetIdsPresentNb)
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Too many different PPS present (shall not exceed %d).\n",
        H264_MAX_ALLOWED_PPS
      );
  }

  LIBBLU_H264_DEBUG_PPS(
    "  Active Sequence Parameter Set id (seq_parameter_set_id): %u (0x%X).\n",
    param.seq_parameter_set_id, param.seq_parameter_set_id
  );

  if (param.seq_parameter_set_id != 0x0)
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "Unallowed multiple-PPS definition.\n"
    );

  LIBBLU_H264_DEBUG_PPS(
    "  Entropy coding mode (entropy_coding_mode_flag): %s (0b%X).\n",
    (param.entropy_coding_mode_flag) ? "CABAC" : "CAVLC",
    param.entropy_coding_mode_flag
  );

  if (
    CHECK_H264_WARNING_FLAG(handle, useOfLowEfficientCavlc)
    && !param.entropy_coding_mode_flag
  ) {
    LIBBLU_WARNING(
      "Usage of a lower efficient entropy coding (CAVLC), "
      "CABAC is more suitable for Blu-ray Disc authoring.\n"
    );
    MARK_H264_WARNING_FLAG(handle, useOfLowEfficientCavlc);
  }

  validEntropyCodingMode = H264_IS_VALID_ENTROPY_CODING_MODE_RESTR(
    handle->constraints.restrictedEntropyCodingMode,
    param.entropy_coding_mode_flag
  );

  if (!validEntropyCodingMode) {
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "%s entropy coding mode is not allowed for bitstream profile "
      "conformance, 'entropy_coding_mode_flag' shall be fixed to 0b%X.\n",
      (param.entropy_coding_mode_flag) ? "CABAC" : "CAVLC",
      param.entropy_coding_mode_flag
    );
  }

  LIBBLU_H264_DEBUG_PPS(
    "  Picture field order syntax present in slice headers "
    "(bottom_field_pic_order_in_frame_present_flag): %s (0x%X).\n",
    BOOL_INFO(param.bottom_field_pic_order_in_frame_present_flag),
    param.bottom_field_pic_order_in_frame_present_flag
  );

  LIBBLU_H264_DEBUG_PPS(
    "  Number of slice groups (num_slice_groups_minus1): %u (0x%X).\n",
    param.num_slice_groups_minus1 + 1,
    param.num_slice_groups_minus1
  );

  if (
    handle->constraints.maxAllowedNumSliceGroups
    <= param.num_slice_groups_minus1
  )
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "bitstream profile does not allow more than %u slice groups "
      "('num_slice_groups_minus1' == %u).\n",
      handle->constraints.maxAllowedNumSliceGroups,
      param.num_slice_groups_minus1
    );

  LIBBLU_H264_DEBUG_PPS(
    "  Default maximum number of reference index for reference picture list 0 "
    "(num_ref_idx_l0_default_active_minus1): %u index(es) (0x%X).\n",
    param.num_ref_idx_l0_default_active_minus1,
    param.num_ref_idx_l0_default_active_minus1 - 1
  );

  /* Already checked at parsing */

  LIBBLU_H264_DEBUG_PPS(
    "  Default maximum number of reference index for reference picture list 1 "
    "(num_ref_idx_l1_default_active_minus1): %u index(es) (0x%X).\n",
    param.num_ref_idx_l1_default_active_minus1,
    param.num_ref_idx_l1_default_active_minus1 - 1
  );

  /* Already checked at parsing */

  LIBBLU_H264_DEBUG_PPS(
    "  Weighted prediction mode for P slices "
    "(weighted_pred_flag): %s (0x%X).\n",
    (param.weighted_pred_flag) ? "Explicit" : "Default",
    param.weighted_pred_flag
  );

  if (
    param.weighted_pred_flag
    && handle->constraints.forbiddenWeightedPredModesUse
  )
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "Weighted prediction mode is not allowed for bitstream "
      "profile conformance.\n"
    );

  LIBBLU_H264_DEBUG_PPS(
    "  Weighted prediction mode for B slices "
    "(weighted_bipred_idc): %s (0x%X).\n",
    h264WeightedBipredIdcStr(param.weighted_bipred_idc),
    param.weighted_bipred_idc
  );

  if (H264_WEIGHTED_PRED_B_SLICES_RESERVED <= param.weighted_bipred_idc) {
    LIBBLU_H264_ERROR_RETURN(
      "Reserved PPS 'weighted_bipred_idc' == %u in use.\n",
      param.weighted_bipred_idc
    );
  }

  if (
    param.weighted_bipred_idc != H264_WEIGHTED_PRED_B_SLICES_DEFAULT
    && handle->constraints.forbiddenWeightedPredModesUse
  )
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "Weighted prediction mode is not allowed for bitstream profile "
      "conformance.\n"
    );


  LIBBLU_H264_DEBUG_PPS(
    "  Picture initial SliceQPy for slices "
    "(pic_init_qp_minus26): %d (0x%X).\n",
    param.pic_init_qp_minus26 + 26,
    param.pic_init_qp_minus26
  );

  if (param.pic_init_qp_minus26 < -26 || 25 < param.pic_init_qp_minus26)
    LIBBLU_H264_ERROR_RETURN(
      "Reserved 'pic_init_qp_minus26' == %d in use.\n",
      param.pic_init_qp_minus26
    );

  LIBBLU_H264_DEBUG_PPS(
    "  Picture initial SliceQSy for SP and SI slices "
    "(pic_init_qs_minus26): %d (0x%X).\n",
    param.pic_init_qs_minus26 + 26,
    param.pic_init_qs_minus26
  );

  if (param.pic_init_qs_minus26 < -26 || 25 < param.pic_init_qs_minus26)
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "Reserved 'pic_init_qs_minus26' == %d in use.\n",
      param.pic_init_qs_minus26
    );

  LIBBLU_H264_DEBUG_PPS(
    "  Chroma QPy / QSy offset for Cb chroma component "
    "(chroma_qp_index_offset): %d (0x%X).\n",
    param.chroma_qp_index_offset,
    param.chroma_qp_index_offset
  );

  if (param.chroma_qp_index_offset < -12 || 12 < param.chroma_qp_index_offset)
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "Reserved 'chroma_qp_index_offset' == %d in use.\n",
      param.chroma_qp_index_offset
    );


  LIBBLU_H264_DEBUG_PPS(
    "  Deblocking filter parameters "
    "(deblocking_filter_control_present_flag): %s (0b%X).\n",
    BOOL_PRESENCE(param.deblocking_filter_control_present_flag),
    param.deblocking_filter_control_present_flag
  );

  LIBBLU_H264_DEBUG_PPS(
    "  Constrained intra prediction "
    "(constrained_intra_pred_flag): %s (0b%X).\n",
    (param.constrained_intra_pred_flag) ?
      "Constrained to I or SI macroblocks references"
    :
      "Unconstrained",
    param.constrained_intra_pred_flag
  );

  if (
    CHECK_H264_WARNING_FLAG(handle, useOfLowEfficientConstaintIntraPred)
    && param.constrained_intra_pred_flag
  ) {
    LIBBLU_WARNING(
      "Usage of lower efficient constraint macroblock intra prediction mode, "
      "which may reduce compression efficiency.\n"
    );
    MARK_H264_WARNING_FLAG(handle, useOfLowEfficientConstaintIntraPred);
  }

  LIBBLU_H264_DEBUG_PPS(
    "  Redundant picture counter syntax in B slices "
    "(redundant_pic_cnt_present_flag): %s (0b%X).\n",
    BOOL_PRESENCE(param.redundant_pic_cnt_present_flag),
    param.redundant_pic_cnt_present_flag
  );

  if (
    param.redundant_pic_cnt_present_flag
    && handle->constraints.forbiddenRedundantPictures
  )
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "Redundant pictures usage is not allowed for bitstream's "
      "profile conformance.\n"
    );

  LIBBLU_H264_DEBUG_PPS(
    "  Presence of Fidelity Range Extensions: %s.\n",
    BOOL_INFO(param.picParamExtensionPresent)
  );

  if (param.picParamExtensionPresent) {
    if (handle->constraints.forbiddenPPSExtensionParameters)
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Presence of Fidelity Range Extensions in PPS is not allowed for "
        "bitstream's profile conformance.\n"
      );

    LIBBLU_H264_DEBUG_PPS(
      "   Usage of 8x8 transform decoding process "
      "(transform_8x8_mode_flag): %s (0b%X).\n",
      BOOL_INFO(param.transform_8x8_mode_flag),
      param.transform_8x8_mode_flag
    );

    LIBBLU_H264_DEBUG_PPS(
      "   Picture scaling matrix "
      "(pic_scaling_matrix_present_flag): %s (0b%X).\n",
      BOOL_PRESENCE(param.pic_scaling_matrix_present_flag),
      param.pic_scaling_matrix_present_flag
    );

    if (param.pic_scaling_matrix_present_flag) {
      for (i = 0; i < param.nbScalingMatrix; i++) {
        LIBBLU_H264_DEBUG_PPS(
          "    -> Picture scaling list %u "
          "(pic_scaling_list_present_flag[%u]): %s (0b%X).\n",
          i, i,
          BOOL_PRESENCE(param.pic_scaling_list_present_flag[i]),
          param.pic_scaling_list_present_flag[i]
        );

        if (param.pic_scaling_list_present_flag[i]) {
          bool useDefScalingMatrix;
          bool isMatrix4x4;
          unsigned nbCoeffs;

          isMatrix4x4 = (i < 6);
          useDefScalingMatrix =
            (isMatrix4x4) ?
              param.UseDefaultScalingMatrix4x4Flag[i]
            :
              param.UseDefaultScalingMatrix8x8Flag[i-6]
          ;

          LIBBLU_H264_DEBUG_PPS(
            "     -> Use default scaling matrix: %s (0b%X).\n",
            BOOL_INFO(useDefScalingMatrix),
            useDefScalingMatrix
          );

          nbCoeffs = ((isMatrix4x4) ? 16 : 64);

          for (j = 0; j < nbCoeffs; j++)
            LIBBLU_H264_DEBUG_PPS(
              "     -> Scaling list %s %u, index %u: %" PRIu8 ".\n",
              (isMatrix4x4) ? "4x4" : "8x8", i, j,
              (isMatrix4x4) ?
                param.ScalingList4x4[i][j]
              :
                param.ScalingList8x8[i-6][j]
            );
        }
      }
    }

    LIBBLU_H264_DEBUG_PPS(
      "   Chroma QPy / QSy offset for Cr chroma component "
      "(second_chroma_qp_index_offset): %d (0x%X).\n",
      param.second_chroma_qp_index_offset,
      param.second_chroma_qp_index_offset
    );

    if (
      param.second_chroma_qp_index_offset < -12
      || 12 < param.second_chroma_qp_index_offset
    )
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Reserved 'second_chroma_qp_index_offset' == %d in use.\n",
        param.second_chroma_qp_index_offset
      );
  }

  return 0; /* OK */
}

int checkH264PicParametersSetChangeCompliance(
  const H264PicParametersSetParameters first,
  const H264PicParametersSetParameters second,
  const H264ParametersHandlerPtr handle
)
{
  LIBBLU_H264_DEBUG_PPS(
    "  ### Change detected in PPS parameters, check changes compliance.\n"
  );

  /* Check second parameters compliance : */
  if (checkH264PicParametersSetCompliance(second, handle) < 0)
    return -1;

  /* Check second parameters mandatory parameters changeless : */
  if (first.entropy_coding_mode_flag != second.entropy_coding_mode_flag)
    LIBBLU_H264_ERROR_RETURN(
      "Entropy coding mode ('entropy_coding_mode_flag') change between PPS.\n"
    );

  return 0; /* OK */
}

char * H264SEIMessagePayloadTypeStr(const H264SeiPayloadTypeValue payloadType)
{
  switch (payloadType) {
    case H264_SEI_TYPE_BUFFERING_PERIOD:
      return "Buffering period";

    case H264_SEI_TYPE_PIC_TIMING:
      return "Picture timing";

    case H264_SEI_TYPE_USER_DATA_UNREGISTERED:
      return "User data unregistered";

    case H264_SEI_TYPE_RECOVERY_POINT:
      return "Recovery point";
  }

  return "Reserved/Unknown";
}

int checkH264SeiBufferingPeriodCompliance(
  const H264SeiBufferingPeriod param,
  const H264ParametersHandlerPtr handle
)
{
  double maxDelay;
  unsigned SchedSelIdx;

  const H264VuiParameters * vuiParam;

  int type;
  bool present;
  const H264HrdParameters * hrdParam;
  const H264HrdBufferingPeriodParameters * hrdBufPerd;

  assert(NULL != handle);

  if (
    !handle->sequenceParametersSetPresent
    || !handle->sequenceParametersSet.data.vui_parameters_present_flag
  )
    LIBBLU_H264_ERROR_RETURN(
      "Missing mandatory VUI parameters, Buffering Period SEI message "
      "shouldn't be present."
    );

  vuiParam = &handle->sequenceParametersSet.data.vui_parameters;

  if (
    !vuiParam->nal_hrd_parameters_present_flag
    && !vuiParam->vcl_hrd_parameters_present_flag
  ) {
    if (CHECK_H264_WARNING_FLAG(handle, unexpectedPresenceOfBufPeriodSei)) {
      LIBBLU_WARNING(
        "Unexpected presence of Buffering Period SEI message, "
        "'NalHrdBpPresentFlag' and 'VclHrdBpPresentFlag' are equal to 0b0.\n"
      );

      MARK_H264_WARNING_FLAG(handle, unexpectedPresenceOfBufPeriodSei);
    }
  }

  LIBBLU_H264_DEBUG_SEI(
    "   Linked sequence parameter set (seq_parameter_set_id): %u (0x%X).\n",
    param.seq_parameter_set_id,
    param.seq_parameter_set_id
  );

  if (param.seq_parameter_set_id != 0x0)
    LIBBLU_H264_ERROR_RETURN(
      "Unallowed multiple SPS definition use "
      "('seq_parameter_set_id' == %u in Buffering Period SEI msg).\n",
      param.seq_parameter_set_id
    );

  for (type = 0; type < 2; type++) {
    present = (type) ?
      vuiParam->vcl_hrd_parameters_present_flag
    :
      vuiParam->nal_hrd_parameters_present_flag
    ;
    if (!present)
      continue;

    hrdParam = (type) ? &vuiParam->vcl_hrd_parameters : &vuiParam->nal_hrd_parameters;
    hrdBufPerd = (type) ? param.vcl_hrd_parameters : param.nal_hrd_parameters;

    LIBBLU_H264_DEBUG_SEI(
      "   %s HRD - Coded Picture Buffer Parameters:\n",
      (type) ? "VCL" : "NAL"
    );

    for (
      SchedSelIdx = 0;
      SchedSelIdx <= hrdParam->cpb_cnt_minus1;
      SchedSelIdx++
    ) {
      LIBBLU_H264_DEBUG_SEI("    - SchedSelIdx %u:\n", SchedSelIdx);

      LIBBLU_H264_DEBUG_SEI(
        "     -> Initial CPB removal delay "
        "(initial_cpb_removal_delay[%u]): "
        "%" PRIu32 " (%" PRIu32 " ms, 0x%" PRIx32 ").\n",
        SchedSelIdx,
        hrdBufPerd[SchedSelIdx].initial_cpb_removal_delay,
        hrdBufPerd[SchedSelIdx].initial_cpb_removal_delay / 90,
        hrdBufPerd[SchedSelIdx].initial_cpb_removal_delay
      );

      if (!hrdBufPerd[SchedSelIdx].initial_cpb_removal_delay)
        LIBBLU_H264_ERROR_RETURN(
          "Forbidden zero initial CPB removal delay "
          "(initial_cpb_removal_delay).\n"
        );

      maxDelay = H264_90KHZ_CLOCK * (
        (double) hrdParam->schedSel[SchedSelIdx].CpbSize /
        (double) hrdParam->schedSel[SchedSelIdx].BitRate
      );

      LIBBLU_H264_DEBUG_SEI(
        "      -> Max allowed delay: %.0f ms.\n",
        maxDelay
      );

      if (
        maxDelay
        < hrdBufPerd[SchedSelIdx].initial_cpb_removal_delay / 90.0
      )
        LIBBLU_H264_ERROR_RETURN(
          "NAL HRD CPB Removal delay exceed maximal delay (%.2f < %.2f).\n",
          maxDelay,
          hrdBufPerd[SchedSelIdx].initial_cpb_removal_delay / 90.0
        );

      LIBBLU_H264_DEBUG_SEI(
        "     -> Initial CPB removal delay offset "
        "(initial_cpb_removal_delay_offset[%u]): "
        "%" PRIu32 " (%" PRIu32 " ms, 0x%" PRIx32 ").\n",
        SchedSelIdx,
        hrdBufPerd[SchedSelIdx].initial_cpb_removal_delay_offset,
        hrdBufPerd[SchedSelIdx].initial_cpb_removal_delay_offset / 90,
        hrdBufPerd[SchedSelIdx].initial_cpb_removal_delay_offset
      );
    }
  }

  return 0;
}

bool constantH264SeiBufferingPeriodCheck(
  const H264ParametersHandlerPtr handle,
  const H264SeiBufferingPeriod first,
  const H264SeiBufferingPeriod second
)
{
  int ret;

  const H264VuiParameters * vuiParam;

  assert(NULL != handle);
  assert(
    handle->sequenceParametersSetPresent
    && handle->sequenceParametersSet.data.vui_parameters_present_flag
  );

  vuiParam = &handle->sequenceParametersSet.data.vui_parameters;

  if (vuiParam->nal_hrd_parameters_present_flag) {
    ret = memcmp(
      first.nal_hrd_parameters,
      second.nal_hrd_parameters,
      (vuiParam->nal_hrd_parameters.cpb_cnt_minus1 + 1)
        * sizeof(H264HrdBufferingPeriodParameters)
    );
    if (ret != 0)
      return false;
  }

  if (vuiParam->vcl_hrd_parameters_present_flag) {
    ret = memcmp(
      first.vcl_hrd_parameters,
      second.vcl_hrd_parameters,
      (vuiParam->vcl_hrd_parameters.cpb_cnt_minus1 + 1)
        * sizeof(H264HrdBufferingPeriodParameters)
    );
    if (ret != 0)
      return false;
  }

  return true;
}

int checkH264SeiBufferingPeriodChangeCompliance(
  const H264ParametersHandlerPtr handle,
  const H264SeiBufferingPeriod first,
  const H264SeiBufferingPeriod second
)
{
  unsigned SchedSelIdx;

  H264VuiParameters * vuiParam;

  LIBBLU_H264_DEBUG_SEI(
    "  ### Change detected in Buffering Period SEI message parameters, "
    "check changes compliance.\n"
  );

  /* Check second parameters compliance : */
  if (checkH264SeiBufferingPeriodCompliance(second, handle) < 0)
    return -1;

  vuiParam = &handle->sequenceParametersSet.data.vui_parameters;

  /* Check second parameters mandatory parameters changeless : */
  if (!ALLOW_BUFFERING_PERIOD_CHANGE && vuiParam->nal_hrd_parameters_present_flag) {
    for (
      SchedSelIdx = 0;
      SchedSelIdx <= vuiParam->nal_hrd_parameters.cpb_cnt_minus1;
      SchedSelIdx++
    ) {
      if (
        first.nal_hrd_parameters[SchedSelIdx].initial_cpb_removal_delay
          + first.nal_hrd_parameters[SchedSelIdx].initial_cpb_removal_delay_offset
        != second.nal_hrd_parameters[SchedSelIdx].initial_cpb_removal_delay
          + second.nal_hrd_parameters[SchedSelIdx].initial_cpb_removal_delay_offset
      )
        LIBBLU_H264_ERROR_RETURN(
          "Fluctuating NAL HRD initial CPB removal delay "
          "(initial_cpb_removal_delay/initial_cpb_removal_delay_offset).\n"
        );
    }
  }

  if (!ALLOW_BUFFERING_PERIOD_CHANGE && vuiParam->vcl_hrd_parameters_present_flag) {
    for (
      SchedSelIdx = 0;
      SchedSelIdx <= vuiParam->vcl_hrd_parameters.cpb_cnt_minus1;
      SchedSelIdx++
    ) {
      if (
        first.vcl_hrd_parameters[SchedSelIdx].initial_cpb_removal_delay
          + first.vcl_hrd_parameters[SchedSelIdx].initial_cpb_removal_delay_offset
        != second.vcl_hrd_parameters[SchedSelIdx].initial_cpb_removal_delay
          + second.vcl_hrd_parameters[SchedSelIdx].initial_cpb_removal_delay_offset
      )
        LIBBLU_H264_ERROR_RETURN(
          "Fluctuating VCL HRD initial CPB removal delay "
          "(initial_cpb_removal_delay/initial_cpb_removal_delay_offset).\n"
        );
    }
  }

  return 0; /* OK */
}

int checkH264SeiPicTimingCompliance(
  const H264SeiPicTiming param,
  const H264ParametersHandlerPtr handle
)
{
  unsigned i;

  H264VuiParameters * vuiParam;

  assert(NULL != handle);

  if (
    !handle->sequenceParametersSetPresent
    || !handle->sequenceParametersSet.data.vui_parameters_present_flag
  )
    LIBBLU_H264_ERROR_RETURN(
      "Missing mandatory VUI parameters, Pic Timing SEI message "
      "shouldn't be present.\n"
    );

  vuiParam = &handle->sequenceParametersSet.data.vui_parameters;

  if (
    !vuiParam->CpbDpbDelaysPresentFlag
    && !vuiParam->pic_struct_present_flag
  )
    LIBBLU_H264_ERROR_RETURN(
      "'CpbDpbDelaysPresentFlag' and 'pic_struct_present_flag' are equal to "
      "0b0, so Pic Timing SEI message shouldn't be present.\n"
    );

  if (vuiParam->CpbDpbDelaysPresentFlag) {
    LIBBLU_H264_DEBUG_SEI(
      "   Buffering parameters "
      "('CpbDpbDelaysPresentFlag' == 0b1 in SPS VUI):\n"
    );

    LIBBLU_H264_DEBUG_SEI(
      "    Coded Picture Buffer removal delay "
      "(cpb_removal_delay): %" PRIu32 " ticks (0x%" PRIx32 ").\n",
      param.cpb_removal_delay,
      param.cpb_removal_delay
    );

    LIBBLU_H264_DEBUG_SEI(
      "    Decoded Picture Buffer output delay (dpb_output_delay): "
      "%" PRIu32 " ticks (0x%" PRIx32 ").\n",
      param.dpb_output_delay,
      param.dpb_output_delay
    );
  }

  if (vuiParam->pic_struct_present_flag) {
    LIBBLU_H264_DEBUG_SEI(
      "   Picture structure parameters "
      "(pic_struct_present_flag == 0b1 in SPS VUI):\n"
    );

    LIBBLU_H264_DEBUG_SEI(
      "    Picture structure (pic_struct): %s (0x%X).\n",
      h264PicStructValueStr(param.pic_struct),
      param.pic_struct
    );

    if (H264_PIC_STRUCT_TRIPLED_FRAME < param.pic_struct)
      LIBBLU_H264_ERROR_RETURN(
        "Reserved 'pic_struct' == %u in use.\n",
        param.pic_struct
      );

    LIBBLU_H264_DEBUG_SEI(
      "    Pictures clock timestamps (clock_timestamp_flag):\n"
    );
    for (i = 0; i < param.NumClockTS; i++) {
      LIBBLU_H264_DEBUG_SEI(
        "     - Picture %u: %s (0x%X).\n",
        i,
        BOOL_PRESENCE(param.clock_timestamp_flag[i]),
        param.clock_timestamp_flag[i]
      );

      if (param.clock_timestamp_flag[i]) {

        LIBBLU_H264_DEBUG_SEI(
          "      -> Picture scan type (ct_type): %s (0x%X).\n",
          h264CtTypeValueStr(param.clock_timestamp[i].ct_type),
          param.clock_timestamp[i].ct_type
        );

        if (H264_CT_TYPE_RESERVED <= param.clock_timestamp[i].ct_type)
          LIBBLU_H264_ERROR_RETURN(
            "Reserved 'ct_type' == %d in use.\n",
            param.clock_timestamp[i].ct_type
          );

        LIBBLU_H264_DEBUG_SEI(
          "      -> Number of Units In Tick field based "
          "(nuit_field_based_flag): %s (0b%X).\n",
          BOOL_INFO(param.clock_timestamp[i].nuit_field_based_flag),
          param.clock_timestamp[i].nuit_field_based_flag
        );

        LIBBLU_H264_DEBUG_SEI(
          "      -> Timestamp counting type (counting_type): %s (0x%X).\n",
          h264CountingTypeValueStr(param.clock_timestamp[i].counting_type),
          param.clock_timestamp[i].counting_type
        );

        LIBBLU_H264_DEBUG_SEI(
          "      -> Timestamp format (full_timestamp_flag): %s (0b%X).\n",
          (param.ClockTimestamp[i].full_timestamp_flag) ?
            "Full timestamp"
          :
            "Compact timestamp",
          param.ClockTimestamp[i].full_timestamp_flag
        );

        LIBBLU_H264_DEBUG_SEI(
          "      -> Discontinuity marker (discontinuity_flag): %s (0b%X).\n",
          BOOL_INFO(param.ClockTimestamp[i].discontinuity_flag),
          param.ClockTimestamp[i].discontinuity_flag
        );

        /**
         * TODO: Add following check.
         * """ From Rec. ITU-T H.264
         * When discontinuity_flag is equal to 0, the value of clockTimestamp
         * shall be greater than or equal to all values of clockTimestamp
         * present for the preceding picture in DPB output order.
         * """
         */

        LIBBLU_H264_DEBUG_SEI(
          "      -> Apply n_frames values skipping according "
          "to counting_type (cnt_dropped_flag): %s (0b%X).\n",
          BOOL_INFO(param.ClockTimestamp[i].cnt_dropped_flag),
          param.ClockTimestamp[i].cnt_dropped_flag
        );

        LIBBLU_H264_DEBUG_SEI(
          "      -> n_frames value (n_frames): %u frame(s) (0x%X).\n",
          param.ClockTimestamp[i].n_frames,
          param.ClockTimestamp[i].n_frames
        );

        if (vuiParam->timing_info_present_flag) {
          LIBBLU_H264_DEBUG_SEI(
            "      -> MaxFPS expected (From SPS VUI): %u frames.\n",
            vuiParam->MaxFPS
          );

          if (vuiParam->MaxFPS <= param.ClockTimestamp[i].n_frames)
            LIBBLU_H264_ERROR_RETURN(
              "Forbidden 'n_frames' == %u in use, shall be less than value "
              "defined by equation D-2 (equal to %u) in Rec. ITU-T H.264.\n",
              vuiParam->MaxFPS
            );
        }

        LIBBLU_H264_DEBUG_SEI(
          "      -> Timestamp: %02u:%02u:%02u.%03u.\n",
          param.ClockTimestamp[i].hours_value,
          param.ClockTimestamp[i].minutes_value,
          param.ClockTimestamp[i].seconds_value,
          param.ClockTimestamp[i].n_frames
        );

        if (0 < param.ClockTimestamp[i].time_offset)
          LIBBLU_H264_DEBUG_SEI(
            "      -> Time offset (time_offset): "
            "%" PRIu32 " (0x%" PRIx32 ").\n",
            param.ClockTimestamp[i].time_offset,
            param.ClockTimestamp[i].time_offset
          );
      }
    }
  }

  return 0;
}

#if 0
int checkH264SeiPicTimingChangeCompliance(
  H264ParametersHandlerPtr handle,
  H264SeiPicTiming first,
  H264SeiPicTiming second
)
{
  LIBBLU_H264_DEBUG_SEI(
    "  ### Change detected in Picture timing SEI message parameters, "
    "check changes compliance.\n"
  );

  return 0; /* OK */
}
#endif

int checkH264SeiRecoveryPointCompliance(
  const H264SeiRecoveryPoint param,
  const H264ParametersHandlerPtr handle
)
{
  if (!handle->sequenceParametersSetPresent)
    LIBBLU_H264_ERROR_RETURN(
      "Missing mandatory SPS NALU, Recovery Point SEI message "
      "shouldn't be present."
    );

  if (!handle->picParametersSetIdsPresentNb)
    LIBBLU_H264_ERROR_RETURN(
      "Missing mandatory PPS NALU, Recovery Point SEI message "
      "shouldn't be present."
    );

  LIBBLU_H264_DEBUG_SEI(
    "   Recovery point frames countdown "
    "(recovery_frame_cnt): %u frame(s) (0x%X).\n",
    param.recovery_frame_cnt,
    param.recovery_frame_cnt
  );

  LIBBLU_H264_DEBUG_SEI(
    "    -> Max possible frame number (MaxFrameNum from SPS): %u.\n",
    handle->sequenceParametersSet.data.MaxFrameNum
  );

  if (
    handle->sequenceParametersSet.data.MaxFrameNum
    <= param.recovery_frame_cnt
  )
    LIBBLU_H264_ERROR_RETURN(
      "Too high 'recovery_frame_cnt' == %u in Recovery Point SEI message, "
      "shall be less than SPS 'MaxFrameNum'.\n",
      param.recovery_frame_cnt
    );

  LIBBLU_H264_DEBUG_SEI(
    "   Exact matching recovery point "
    "(exact_match_flag): %s (0b%X).\n",
    BOOL_INFO(param.exact_match_flag),
    param.exact_match_flag
  );

  LIBBLU_H264_DEBUG_SEI(
    "   Presence of a broken reference link at recovery point "
    "(broken_link_flag): %s (0b%X).\n",
    BOOL_INFO(param.broken_link_flag),
    param.broken_link_flag
  );

  if (param.broken_link_flag)
    LIBBLU_H264_ERROR_RETURN(
      "Unallowed broken link ('broken_link_flag' == 0b1) signaled by "
      "Recovery Point SEI message.\n"
    );

  LIBBLU_H264_DEBUG_SEI(
    "   Changing slice group indicator (changing_slice_group_idc): "
    "0x%02" PRIX8 ".\n", param.changing_slice_group_idc
  );
  switch (param.changing_slice_group_idc) {
    case 0x0:
      LIBBLU_H264_DEBUG_SEI(
        "    -> All decoded pictures between message and "
        "recovery point are correct.\n"
      );
      break;

    case 0x1:
      LIBBLU_H264_DEBUG_SEI(
        "    -> All macroblocks in slice group 0 between message and recovery "
        "point produce correct decoded pictures.\n"
      );
      LIBBLU_H264_DEBUG_SEI(
        "    -> Number of slice groups should be equal to 2.\n"
      );
      break;

    case 0x2:
      LIBBLU_H264_DEBUG_SEI(
        "    -> All macroblocks in slice group 1 between message and recovery "
        "point produce correct decoded pictures.\n"
      );
      LIBBLU_H264_DEBUG_SEI(
        "    -> Number of slice groups should be equal to 2.\n"
      );
      break;

    default:
      LIBBLU_H264_DEBUG_SEI("    -> Reserved value.\n");

      LIBBLU_H264_ERROR_RETURN(
        "Reserved 'changing_slice_group_idc' == %" PRIu8 " in use.\n",
        param.changing_slice_group_idc
      );
  }

  if (param.changing_slice_group_idc != 0x0) {
    if (!handle->constraints.maxAllowedNumSliceGroups) {
      LIBBLU_H264_ERROR_RETURN(
        "'changing_slice_group_idc' == %" PRIu8 " should not indicate "
        "presence of multiple slice groups.\n",
        param.changing_slice_group_idc
      );
    }
    else {
      /* TODO */
      LIBBLU_H264_ERROR_RETURN(
        "Multiple slice groups are not supported.\n"
      );
    }

    return -1;
  }

  return 0;
}

int checkH264SeiRecoveryPointChangeCompliance(
  const H264ParametersHandlerPtr handle,
  const H264SeiRecoveryPoint second
)
{
  LIBBLU_H264_DEBUG_SEI(
    "  ### Change detected in Recovery point SEI message parameters, "
    "check changes compliance.\n"
  );

  /* Check second parameters compliance : */
  if (checkH264SeiRecoveryPointCompliance(second, handle) < 0)
    return -1;

  return 0; /* OK */
}

int checkH264RefPicListModificationCompliance(
  const H264RefPicListModification param,
  const H264SliceHeaderParameters sliceHeaderParam
)
{
  unsigned i;

  if (
    !H264_SLICE_IS_TYPE_I(sliceHeaderParam.slice_type)
    && !H264_SLICE_IS_TYPE_SI(sliceHeaderParam.slice_type)
  ) {
    LIBBLU_H264_DEBUG_SLICE(
      "  ref_pic_list_modification() - Reference picture list modification:\n"
    );

    LIBBLU_H264_DEBUG_SLICE(
      "   Modification of reference picture list 0 "
      "(ref_pic_list_modification_flag_l0): %s (0b%X).\n",
      BOOL_PRESENCE(param.ref_pic_list_modification_flag_l0),
      param.ref_pic_list_modification_flag_l0
    );

    if (param.ref_pic_list_modification_flag_l0) {
      for (i = 0; i < param.nbRefPicListModificationl0; i++) {
        LIBBLU_H264_DEBUG_SLICE("    -> Picture number %u:\n", i);

        LIBBLU_H264_DEBUG_SLICE(
          "     Modification indicator "
          "(modification_of_pic_nums_idc): %s (0x%X).\n",
          h264ModificationOfPicNumsIdcValueStr(
            param.refPicListModificationl0[i].modification_of_pic_nums_idc
          ),
          param.refPicListModificationl0[i].modification_of_pic_nums_idc
        );

        if (param.refPicListModificationl0[i].modification_of_pic_nums_idc <= 1)
          LIBBLU_H264_DEBUG_SLICE(
            "     Difference value "
            "(abs_diff_pic_num_minus1): %" PRIu32 " (0x%" PRIx32 ").\n",
            param.refPicListModificationl0[i].abs_diff_pic_num_minus1,
            param.refPicListModificationl0[i].abs_diff_pic_num_minus1
          );

        if (param.refPicListModificationl0[i].modification_of_pic_nums_idc == 2)
          LIBBLU_H264_DEBUG_SLICE(
            "     Reference long-term picture number "
            "(long_term_pic_num): %" PRIu32 " (0x%" PRIx32 ").\n",
            param.refPicListModificationl0[i].long_term_pic_num,
            param.refPicListModificationl0[i].long_term_pic_num
          );
      }
    }
  }

  if (H264_SLICE_IS_TYPE_B(sliceHeaderParam.slice_type)) {
    LIBBLU_H264_DEBUG_SLICE(
      "   Modification of reference picture list 1 "
      "(ref_pic_list_modification_flag_l1): %s (0b%X).\n",
      BOOL_PRESENCE(param.refPicListModificationFlagl1),
      param.refPicListModificationFlagl1
    );

    if (param.refPicListModificationFlagl1) {
      for (i = 0; i < param.nbRefPicListModificationl1; i++) {
        LIBBLU_H264_DEBUG_SLICE("    -> Picture number %u:\n", i);

        LIBBLU_H264_DEBUG_SLICE(
          "     Modification indicator "
          "(modification_of_pic_nums_idc): %s (0x%X).\n",
          h264ModificationOfPicNumsIdcValueStr(
            param.refPicListModificationl1[i].modification_of_pic_nums_idc
          ),
          param.refPicListModificationl1[i].modification_of_pic_nums_idc
        );

        if (param.refPicListModificationl1[i].modification_of_pic_nums_idc <= 1)
          LIBBLU_H264_DEBUG_SLICE(
            "     Difference value "
            "(abs_diff_pic_num_minus1): %" PRIu32 " (0x%" PRIx32 ").\n",
            param.refPicListModificationl1[i].abs_diff_pic_num_minus1,
            param.refPicListModificationl1[i].abs_diff_pic_num_minus1
          );

        if (param.refPicListModificationl1[i].modification_of_pic_nums_idc == 2)
          LIBBLU_H264_DEBUG_SLICE(
            "     Reference long-term picture number "
            "(long_term_pic_num): %" PRIu32 " (0x%" PRIx32 ").\n",
            param.refPicListModificationl1[i].long_term_pic_num,
            param.refPicListModificationl1[i].long_term_pic_num
          );
      }
    }
  }

  return 0;
}

int checkH264PredWeightTableCompliance(
  const H264PredWeightTable param
)
{


  LIBBLU_H264_DEBUG_SLICE(
    "  pred_weight_table() - Prediction weight table:\n"
  );

  LIBBLU_H264_DEBUG_SLICE(
    "   Luma weighting factors denominator "
    "(luma_log2_weight_denom): %u (0x%X).\n",
    1 << param.luma_log2_weight_denom,
    param.luma_log2_weight_denom
  );

  if (7 < param.luma_log2_weight_denom)
    LIBBLU_H264_ERROR_RETURN(
      "Out of range 'luma_log2_weight_denom' == %u in use.\n",
      param.luma_log2_weight_denom
    );

  LIBBLU_H264_DEBUG_SLICE(
    "   Chroma weighting factors denominator "
    "(chroma_log2_weight_denom): %u (0x%X).\n",
    1 << param.chroma_log2_weight_denom,
    param.chroma_log2_weight_denom
  );

  if (7 < param.chroma_log2_weight_denom)
    LIBBLU_H264_ERROR_RETURN(
      "Out of range 'chroma_log2_weight_denom' == %u in use.\n",
      param.chroma_log2_weight_denom
    );

  LIBBLU_H264_DEBUG_SLICE(
    "   Other parameters not checked.\n"
  );

  return 0;
}

int checkH264DecRefPicMarkingCompliance(
  const H264DecRefPicMarking param
)
{

  LIBBLU_H264_DEBUG_SLICE(
    "  dec_ref_pic_marking() - Decoded reference picture marking:\n"
  );

  if (param.IdrPicFlag) {
    LIBBLU_H264_DEBUG_SLICE(
      "   Clear all DPB frame buffers "
      "(no_output_of_prior_pics_flag): %s (0b%X).\n",
      BOOL_INFO(param.no_output_of_prior_pics_flag),
      param.no_output_of_prior_pics_flag
    );

    LIBBLU_H264_DEBUG_SLICE(
      "   Usage duration of reference picture "
      "(long_term_reference_flag): %s (0b%X).\n",
      (param.long_term_reference_flag) ? "Long term" : "Short term",
      param.long_term_reference_flag
    );
  }
  else {
    LIBBLU_H264_DEBUG_SLICE(
      "   Adaptive reference pictures marking mode "
      "(adaptive_ref_pic_marking_mode_flag): %s (0b%X).\n",
      BOOL_INFO(param.adaptive_ref_pic_marking_mode_flag),
      param.adaptive_ref_pic_marking_mode_flag
    );

    if (!param.adaptive_ref_pic_marking_mode_flag)
      LIBBLU_H264_DEBUG_SLICE(
        "    -> Sliding window reference picture marking mode.\n"
      );

    if (param.adaptive_ref_pic_marking_mode_flag) {
      H264MemMngmntCtrlOpBlkPtr opBlk = param.MemMngmntCtrlOp;

      for (; NULL != opBlk; opBlk = opBlk->nextOperation) {
        LIBBLU_H264_DEBUG_SLICE("    - Instruction %u:\n");

        LIBBLU_H264_DEBUG_SLICE(
          "     Operation (memory_management_control_operation): %s (0x%X).\n",
          H264MemoryManagementControlOperationValueStr(opBlk->operation),
          opBlk->operation
        );

        if (opBlk->operation == H264_MEM_MGMNT_CTRL_OP_END) {
          if (NULL != opBlk->nextOperation) {
            /* This shall not happen */
            LIBBLU_H264_ERROR_RETURN(
              "Invalid 'end' memory management control operation, "
              "not the last in list.\n"
            );
          }
        }

        if (
          opBlk->operation
            == H264_MEM_MGMNT_CTRL_OP_SHORT_TERM_UNUSED
          || opBlk->operation
            == H264_MEM_MGMNT_CTRL_OP_SHORT_TERM_USED_LONG_TERM
        ) {
          LIBBLU_H264_DEBUG_SLICE(
            "      -> Difference of pictures "
            "(difference_of_pic_nums_minus1): %u (0x%X).\n",
            opBlk->difference_of_pic_nums_minus1 + 1,
            opBlk->difference_of_pic_nums_minus1
          );
        }

        if (
          opBlk->operation == H264_MEM_MGMNT_CTRL_OP_LONG_TERM_UNUSED
        ) {
          LIBBLU_H264_DEBUG_SLICE(
            "      -> Long-term picture number "
            "(long_term_pic_num): %u (0x%X).\n",
            opBlk->long_term_pic_num,
            opBlk->long_term_pic_num
          );
        }

        if (
          opBlk->operation
            == H264_MEM_MGMNT_CTRL_OP_SHORT_TERM_USED_LONG_TERM
          || opBlk->operation
            == H264_MEM_MGMNT_CTRL_OP_USED_LONG_TERM
        ) {
          LIBBLU_H264_DEBUG_SLICE(
            "      -> Long-term frame index "
            "(long_term_frame_idx): %u (0x%X).\n",
            opBlk->long_term_frame_idx,
            opBlk->long_term_frame_idx
          );
        }

        if (
          opBlk->operation == H264_MEM_MGMNT_CTRL_OP_MAX_LONG_TERM
        ) {
          LIBBLU_H264_DEBUG_SLICE(
            "      -> Maximum long-term frame index "
            "(max_long_term_frame_idx_plus1): %u (0x%X).\n",
            opBlk->max_long_term_frame_idx_plus1 - 1,
            opBlk->max_long_term_frame_idx_plus1
          );
        }
      }
    }
  }

  return 0;
}

int checkH264SliceHeaderCompliance(
  const H264SliceHeaderParameters param,
  const H264ParametersHandlerPtr handle,
  const LibbluESSettingsOptions options
)
{
  unsigned linkedPPSId;

  assert(NULL != handle);

  H264VuiParameters * vuiParam;
  H264PicParametersSetParameters * ppsParam;

  H264AllowedSliceTypes allowedSliceTypes;

  assert(handle->accessUnitDelimiterPresent);
  assert(handle->sequenceParametersSetPresent);
  assert(handle->sequenceParametersSet.data.vui_parameters_present_flag);
  assert(0 < handle->picParametersSetIdsPresentNb);

  vuiParam = &handle->sequenceParametersSet.data.vui_parameters;

  if (handle->constraints.idrPicturesOnly && !param.isIdrPic)
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "Presence of non-IDR picture is not allowed for "
      "bitstream's profile conformance.\n"
    );

  /* if (param.isIdrPic)
    handle->curProgParam.idrPresent = true; */

  LIBBLU_H264_DEBUG_SLICE(
    "   Address of the first macroblock in slice "
    "(first_mb_in_slice): %" PRIu32 " (0x%" PRIx32 ").\n",
    param.first_mb_in_slice,
    param.first_mb_in_slice
  );

  if (param.first_mb_in_slice == 0x0) {
    /* second picture */
    handle->curProgParam.nbPics++;

    if (
      1 < handle->curProgParam.nbPics
      && handle->curProgParam.stillPictureTolerance
    ) {
      /** Error trigger:
       * Decoders are more tolerant with still-picture streams
       * (only-one IDR picture streams). 'stillPictureTolerance' means an
       * error has been found in stream, which can pass if stream where a still
       * picture one. Since 1 < nbPics, we need to report that error.
       */

      if (!vuiParam->nal_hrd_parameters_present_flag)
        LIBBLU_H264_ERROR_RETURN(
          "Missing NAL HRD parameters in VUI parameters of SPS NALU.\n"
        );

      LIBBLU_H264_ERROR_RETURN(
        "Bit-stream contains more than one picture, "
        "still-picture related compliance tolerance cannot longer be used.\n"
      );
    }

    handle->curProgParam.nbSlicesInPic = 1;

    /* Check presence of both fields (or a complete frame) before completing AU. */
    if (param.field_pic_flag) {
      if (param.bottom_field_flag)
        handle->curProgParam.auContent.bottomFieldPresent = true;
      else
        handle->curProgParam.auContent.topFieldPresent = true;
    }
    else {
      handle->curProgParam.auContent.framePresent = true;
    }

    if (param.isIdrPic) {
      handle->sequenceParametersSetGopValid = false;
      handle->sei.bufferingPeriodGopValid = false;
    }

    if (!handle->sequenceParametersSetGopValid) {
      if (!handle->sequenceParametersSetValid)
        LIBBLU_H264_ERROR_RETURN(
          "Missing a valid SPS NALU before slice header.\n"
        );

      handle->sequenceParametersSetGopValid = true;
    }

    if (!handle->sei.bufferingPeriodGopValid) {
      if (
        !handle->sei.bufferingPeriodValid
        && !options.discardSei
        && !options.forceRebuildSei
      ) {
#if 0
        LIBBLU_ERROR(
          "Missing mandatory buffering_period SEI messages, "
          "add '" FLAG_FORCE_REBUILD_SEI_COM "' parameter.\n"
        );
#endif

        LIBBLU_H264_ERROR_RETURN(
          "Missing mandatory buffering_period SEI messages.\n"
        );
      }

      handle->sei.bufferingPeriodGopValid = true;
    }
  }
  else {
    if (handle->constraints.forbiddenArbitrarySliceOrder) {
      if (param.first_mb_in_slice <= handle->curProgParam.lstPic.first_mb_in_slice)
        LIBBLU_H264_ERROR_RETURN(
          "Broken slices order according to macroblocks addresses, "
          "arbitrary slice order is forbidden for bitstream profile "
          "conformance.\n"
        );
    }
    else {
      /* TODO: Add support of arbitrary slice order. */
      LIBBLU_H264_ERROR_RETURN(
        "Arbitrary ordered slices structure is not supported.\n"
      );
    }

    handle->curProgParam.nbSlicesInPic++;
  }
  handle->curProgParam.lstPic.first_mb_in_slice = param.first_mb_in_slice;

  LIBBLU_H264_DEBUG_SLICE(
    "  Slice type (slice_type): %s (0x%X).\n",
    h264SliceTypeValueStr(param.slice_type),
    param.slice_type
  );

  if (H264_SLICE_TYPE_SI < param.slice_type)
    LIBBLU_H264_ERROR_RETURN(
      "Reserved 'slice_type' == %u in use.\n",
      param.slice_type
    );

  /* Check if slice type is allowed: */
  allowedSliceTypes = handle->constraints.allowedSliceTypes;
  if (!H264_IS_VALID_SLICE_TYPE(allowedSliceTypes, param.slice_type))
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "Unallowed 'slice_type' == %u in use for bitstream profile "
      "conformance.\n",
      param.slice_type
    );

  if (param.isIdrPic && !H264_SLICE_IS_TYPE_I(param.slice_type))
    LIBBLU_H264_ERROR_RETURN(
      "Non I-slice type value 'slice_type' == %u in use in an IDR picture.\n"
    );

  /* Check Access unit delimiter primary_pic_type */
  H264_CHECK_PRIMARY_PICTURE_TYPE(
    allowedSliceTypes,
    handle->accessUnitDelimiter.primary_pic_type,
    param.slice_type
  );

  if (!allowedSliceTypes) {
    LIBBLU_H264_ERROR_RETURN(
      "Wrong Access Unit Delimiter slice type indicator, "
      "'primary_pic_type' == %u while picture 'slice_type' == %u.\n",
      handle->accessUnitDelimiter.primary_pic_type,
      param.slice_type
    );
  }

  LIBBLU_H264_DEBUG_SLICE(
    "  Active Picture Parameter Set id "
    "(pic_parameter_set_id): %" PRIu8 " (0x%02" PRIX8 ").\n",
    param.pic_parameter_set_id,
    param.pic_parameter_set_id
  );

  linkedPPSId = param.pic_parameter_set_id;
  if (
    H264_MAX_PPS <= linkedPPSId
    || !handle->picParametersSetIdsPresent[linkedPPSId]
  )
    LIBBLU_H264_ERROR_RETURN(
      "Reserved/unknown linked PPS id 'pic_parameter_set_id' == %u "
      "in slice header.\n",
      linkedPPSId
    );

  ppsParam = handle->picParametersSet[linkedPPSId];

  if (handle->sequenceParametersSet.data.separate_colour_plane_flag) {
    LIBBLU_H264_DEBUG_SLICE(
      "  Colour plane (colour_plane_id): %s (0x%X).\n",
      h264ColourPlaneIdValueStr(param.colour_plane_id),
      param.colour_plane_id
    );

    if (H264_COLOUR_PLANE_ID_CR < param.colour_plane_id)
      LIBBLU_H264_ERROR_RETURN(
        "Reserved 'colour_plane_id' == %u in use.\n",
        param.colour_plane_id
      );
  }

  LIBBLU_H264_DEBUG_SLICE(
    "  Frame number (frame_num): %" PRIu16 " (0x%04" PRIX8 ").\n",
    param.frameNum,
    param.frameNum
  );

  if (handle->sequenceParametersSet.data.MaxFrameNum <= param.frameNum)
    LIBBLU_H264_ERROR_RETURN(
      "Unexpected too high frame number 'frame_num' == %" PRIu16 ", "
      "according to SPS, this value shall not exceed %u.\n",
      param.frameNum,
      handle->sequenceParametersSet.data.MaxFrameNum
    );

  if (handle->curProgParam.gapsInFrameNum) {
    if (!handle->sequenceParametersSet.data.gaps_in_frame_num_value_allowed_flag) {
      /* Unintentional lost of pictures. */
      LIBBLU_H264_ERROR_RETURN(
        "Unexpected gaps in 'frame_num' values.\n"
      );
    }
    else {
      /* TODO: Inform presence of gaps in frame_num. */


      handle->curProgParam.gapsInFrameNum = false; /* Reset */
    }
  }

  if (param.isIdrPic) {
    /* IDR picture */
    if (param.frameNum != 0x0) {
      LIBBLU_H264_ERROR_RETURN(
        "Unexpected 'frame_num' == %" PRIu16 " for IDR picture, "
        "shall be equal to 0.\n",
        param.frameNum
      );
    }
  }

  if (!handle->sequenceParametersSet.data.frame_mbs_only_flag) {
    LIBBLU_H264_DEBUG_SLICE(
      "  Not macroblocks-only frame linked parameters "
      "('frame_mbs_only_flag' in SPS == 0b0):\n"
    );

    LIBBLU_H264_DEBUG_SLICE(
      "   -> Field picture (field_pic_flag): %s (0b%X).\n",
      BOOL_INFO(param.field_pic_flag),
      param.field_pic_flag
    );

    if (param.field_pic_flag) {
      LIBBLU_H264_DEBUG_SLICE(
        "   -> Field type (bottom_field_flag): %s (0b%X).\n",
        (param.bottom_field_flag) ? "Bottom field" : "Top field",
        param.bottom_field_flag
      );
    }
  }
  else {
    LIBBLU_H264_DEBUG_SLICE(
      "  Macroblocks-only frame linked parameters "
      "('frame_mbs_only_flag' in SPS == 0b1):\n"
    );
  }

  LIBBLU_H264_DEBUG_SLICE(
    "   => MbaffFrameFlag "
    "(Macroblock-Adaptative Frame-Field Coding): %s (0b%d).\n",
    BOOL_STR(param.MbaffFrameFlag),
    param.MbaffFrameFlag
  );
  LIBBLU_H264_DEBUG_SLICE(
    "   => PicHeightInMbs "
    "(Picture height in macroblocks): %u MBs.\n",
    param.PicHeightInMbs
  );
  LIBBLU_H264_DEBUG_SLICE(
    "   => PicHeightInSamplesL "
    "(Picture height in luma samples): %u samples.\n",
    param.PicHeightInSamplesL
  );
  LIBBLU_H264_DEBUG_SLICE(
    "   => PicHeightInSamplesC "
    "(Picture height in chroma samples): %u samples.\n",
    param.PicHeightInSamplesC
  );

  if (param.isIdrPic) {
    LIBBLU_H264_DEBUG_SLICE(
      "  IDR picture linked parameters (IdrPicFlag == true):\n"
    );

    LIBBLU_H264_DEBUG_SLICE(
      "   -> IDR picture identifier "
      "(idr_pic_id): %" PRIu16 " (0x%04" PRIX8 ").\n",
      param.idr_pic_id,
      param.idr_pic_id
    );
  }

  if (handle->sequenceParametersSet.data.pic_order_cnt_type == 0x0) {
    LIBBLU_H264_DEBUG_SLICE(
      "  Picture order count type linked parameters "
      "(pic_order_cnt_type == 0):\n"
    );

    LIBBLU_H264_DEBUG_SLICE(
      "   -> LSB value "
      "(pic_order_cnt_lsb): %" PRIu16 " (0x%04" PRIX8 ").\n",
      param.pic_order_cnt_lsb,
      param.pic_order_cnt_lsb
    );

    if (ppsParam->bottom_field_pic_order_in_frame_present_flag && !param.field_pic_flag) {
      LIBBLU_H264_DEBUG_SLICE(
        "   -> Value difference between bottom and top fields "
        "(delta_pic_order_cnt_bottom): %" PRIu16 " (0x%04" PRIX8 ").\n",
        param.delta_pic_order_cnt_bottom,
        param.delta_pic_order_cnt_bottom
      );
    }
  }

  if (
    handle->sequenceParametersSet.data.pic_order_cnt_type == 0x1
    && !handle->sequenceParametersSet.data.delta_pic_order_always_zero_flag
  ) {
    LIBBLU_H264_DEBUG_SLICE(
      "  Picture order count type linked parameters "
      "(pic_order_cnt_type == 1):\n"
    );

    LIBBLU_H264_DEBUG_SLICE(
      "   -> %s order count difference from expected order count "
      "(delta_pic_order_cnt[0]): %" PRIu16 " (0x%04" PRIX8 ").\n",
      (ppsParam->bottom_field_pic_order_in_frame_present_flag) ?
        "Top field"
      :
        "Coded field",
      param.delta_pic_order_cnt[0],
      param.delta_pic_order_cnt[0]
    );

    if (
      ppsParam->bottom_field_pic_order_in_frame_present_flag
      && !param.field_pic_flag
    ) {
      LIBBLU_H264_DEBUG_SLICE(
        "   -> Bottom field order count difference from expected order count "
        "(delta_pic_order_cnt[0]): %" PRIu16 " (0x%04" PRIX8 ").\n",
        param.delta_pic_order_cnt[1],
        param.delta_pic_order_cnt[1]
      );
    }
  }

  if (ppsParam->redundant_pic_cnt_present_flag) {
    LIBBLU_H264_DEBUG_SLICE(
      "  Redundant picture count linked parameters "
      "(redundant_pic_cnt_present_flag == 0b1):\n"
    );

    LIBBLU_H264_DEBUG_SLICE(
      "   -> Redundant picture counter value "
      "(redundant_pic_cnt): %" PRIu8 " (0x%02" PRIX8 ").\n",
      param.redundant_pic_cnt,
      param.redundant_pic_cnt
    );
  }

  if (H264_SLICE_IS_TYPE_B(param.slice_type)) {
    LIBBLU_H264_DEBUG_SLICE(
      "  Inter prediction decoding process "
      "(direct_spatial_mv_pred_flag): %s (0b%X).\n",
      (param.direct_spatial_mv_pred_flag) ?
        "Spatial direct prediction mode"
      :
        "Temporal direct prediction mode",
      param.direct_spatial_mv_pred_flag
    );
  }

  if (
    H264_SLICE_IS_TYPE_P(param.slice_type)
    || H264_SLICE_IS_TYPE_SP(param.slice_type)
    || H264_SLICE_IS_TYPE_B(param.slice_type)
  ) {
    LIBBLU_H264_DEBUG_SLICE(
      "  Override default number of reference indexes "
      "(num_ref_idx_active_override_flag): %s (0b%X).\n",
      BOOL_INFO(param.num_ref_idx_active_override_flag),
      param.num_ref_idx_active_override_flag
    );

    /* Check mandatory cases where num_ref_idx_active_override_flag shall be equal to 0b1: */
    if (
      !param.field_pic_flag
      && 15 < ppsParam->num_ref_idx_l0_default_active_minus1
      && !param.num_ref_idx_active_override_flag
    )
      LIBBLU_H264_ERROR_RETURN(
        "'num_ref_idx_active_override_flag' shall be equal to 0b1.\n"
      );

    if (
      H264_SLICE_IS_TYPE_B(param.slice_type)
      && !param.field_pic_flag
      && 15 < ppsParam->num_ref_idx_l1_default_active_minus1
      && !param.num_ref_idx_active_override_flag
    )
      LIBBLU_H264_ERROR_RETURN(
        "'num_ref_idx_active_override_flag' shall be equal to 0b1.\n"
      );

    if (param.num_ref_idx_active_override_flag) {
      unsigned maxRefIdx;

      maxRefIdx =
        (param.field_pic_flag) ?
          H264_REF_PICT_LIST_MAX_NB_FIELD
        :
          H264_REF_PICT_LIST_MAX_NB
      ;

      LIBBLU_H264_DEBUG_SLICE(
        "   -> Number of reference index for reference picture list 0 "
        "(num_ref_idx_l0_active_minus1): %u (0x%X).\n",
        param.num_ref_idx_l0_active_minus1 + 1,
        param.num_ref_idx_l0_active_minus1
      );

      /* Check value range: */
      if (maxRefIdx < param.num_ref_idx_l0_active_minus1)
        LIBBLU_H264_ERROR_RETURN(
          "Too high 'num_ref_idx_l0_active_minus1' == %u in use.\n",
          param.num_ref_idx_l0_active_minus1
        );

      if (H264_SLICE_IS_TYPE_B(param.slice_type)) {
        LIBBLU_H264_DEBUG_SLICE(
          "   -> Number of reference index for reference picture list 1 "
          "(num_ref_idx_l1_active_minus1): %u (0x%X).\n",
          param.num_ref_idx_l1_active_minus1 + 1,
          param.num_ref_idx_l1_active_minus1
        );

        /* Check value range: */
        if (maxRefIdx < param.num_ref_idx_l1_active_minus1)
          LIBBLU_H264_ERROR_RETURN(
            "Too high 'num_ref_idx_l1_active_minus1' == %u in use.\n",
            param.num_ref_idx_l1_active_minus1
          );
      }
    }
  }

  if (param.isCodedSliceExtension) {
    /* ref_pic_list_mvc_modification() */
    /* Annex H */

    /* TODO: Add 3D MVC support. */
    LIBBLU_H264_ERROR_RETURN("Unsupported 3D MVC format.\n");
  }
  else
    if (checkH264RefPicListModificationCompliance(param.refPicListMod, param) < 0)
      return -1;

  if (
    (
      ppsParam->weighted_pred_flag && (
        H264_SLICE_IS_TYPE_P(param.slice_type)
        || H264_SLICE_IS_TYPE_SP(param.slice_type)
      )
    )
    || (
      ppsParam->weighted_bipred_idc == H264_WEIGHTED_PRED_B_SLICES_EXPLICIT &&
      H264_SLICE_IS_TYPE_B(param.slice_type)
    )
  ) {
    /* pred_weight_table() */
    if (checkH264PredWeightTableCompliance(param.pred_weight_table) < 0)
      return -1;
  }

  if (param.decRefPicMarkingPresent) {
    /* dec_ref_pic_marking() */
    if (checkH264DecRefPicMarkingCompliance(param.dec_ref_pic_marking) < 0)
      return -1;
  }

  if (
    ppsParam->entropy_coding_mode_flag &&
    !H264_SLICE_IS_TYPE_I(param.slice_type) &&
    !H264_SLICE_IS_TYPE_SI(param.slice_type)
  ) {
    LIBBLU_H264_DEBUG_SLICE(
      "  CABAC initialization table index "
      "(cabac_init_idc): %" PRIu8 " (0x%02" PRIX8 ").\n",
      param.cabac_init_idc,
      param.cabac_init_idc
    );

    if (2 < param.cabac_init_idc)
      LIBBLU_H264_ERROR_RETURN(
        "Out of range 'cabac_init_idc' == %" PRIu8 ".\n",
        param.cabac_init_idc
      );
  }

  LIBBLU_H264_DEBUG_SLICE(
    "  Initial QPy value used for macroblocks of slice: %d (0x%X).\n",
    param.slice_qp_delta,
    param.slice_qp_delta
  );

  if (H264_SLICE_IS_TYPE_SP(param.slice_type) || H264_SLICE_IS_TYPE_SI(param.slice_type)) {
    if (H264_SLICE_IS_TYPE_SP(param.slice_type)) {
      LIBBLU_H264_DEBUG_SLICE(
        "  Decoding process used to decode P macroblocks in SP slice "
        "(sp_for_switch_flag): %s (0b%X).\n",
        (param.sp_for_switch_flag) ? "Switching pictures" : "Non-switching pictures",
        param.sp_for_switch_flag
      );
    }

    LIBBLU_H264_DEBUG_SLICE(
      "  Initial QSy value used for macroblocks of slice: %d (0x%X).\n",
      param.slice_qs_delta,
      param.slice_qs_delta
    );
  }

  if (ppsParam->deblocking_filter_control_present_flag) {
    LIBBLU_H264_DEBUG_SLICE(
      "  Deblocking filter control linked parameters "
      "('deblocking_filter_control_present_flag' == 1):\n"
    );

    LIBBLU_H264_DEBUG_SLICE(
      "   -> Deblocking filter status "
      "(disable_deblocking_filter_idc): %s (0x%X).\n",
      h264DeblockingFilterIdcStr(param.disable_deblocking_filter_idc),
      param.disable_deblocking_filter_idc
    );

    if (param.disable_deblocking_filter_idc != H264_DEBLOCKING_FILTER_DISABLED) {
      LIBBLU_H264_DEBUG_SLICE(
        "   -> Deblocking filter alpha and t_c0 tables offset used "
        "(slice_alpha_c0_offset_div2): %d (0x%X).\n",
        param.slice_alpha_c0_offset_div2 << 1,
        param.slice_alpha_c0_offset_div2
      );

      LIBBLU_H264_DEBUG_SLICE(
        "   -> Deblocking filter beta table offset used "
        "(slice_beta_offset_div2): %d (0x%X).\n",
        param.slice_beta_offset_div2 << 1,
        param.slice_beta_offset_div2
      );
    }
  }

  if (
    0 < ppsParam->num_slice_groups_minus1
    && isChanging123H264SliceGroupMapTypeValue(
      ppsParam->slice_groups.slice_group_map_type
    )
  ) {
    LIBBLU_H264_DEBUG_SLICE(
      "  Modifier of number of slice group map units "
      "(slice_group_change_cycle): %" PRIu32 " (0x%" PRIx32 ").\n",
      param.slice_group_change_cycle,
      param.slice_group_change_cycle
    );
  }

  return 0;
}

int checkH264SliceLayerWithoutPartitioningCompliance(
  const H264SliceLayerWithoutPartitioningParameters param,
  const H264ParametersHandlerPtr handle,
  const LibbluESSettingsOptions options
)
{
  /* assert(NULL != handle); */

  return checkH264SliceHeaderCompliance(param.header, handle, options);
}

int checkH264SliceHeaderChangeCompliance(
  const H264ParametersHandlerPtr handle,
  const H264SliceHeaderParameters first,
  const H264SliceHeaderParameters second,
  const LibbluESSettingsOptions options
)
{
  assert(NULL != handle);

  if (checkH264SliceHeaderCompliance(second, handle, options) < 0)
    return -1;

  /* Check slice types etc. */
  if (second.first_mb_in_slice != 0x0 && first.slice_type != second.slice_type) {
    if (
      !H264_SLICE_TYPE_IS_UNRESTRICTED(first.slice_type)
      || !H264_SLICE_TYPE_IS_UNRESTRICTED(second.slice_type)
    ) {
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Unallowed different type of slices in the same picture.\n"
      );
    }
  }
  if (second.first_mb_in_slice != 0x0) {
    if (first.frameNum != second.frameNum)
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Unallowed different 'frame_num' in the same picture.\n"
      );

    if (first.field_pic_flag != second.field_pic_flag)
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Unallowed different 'field_pic_flag' in the same picture.\n"
      );

    if (first.bottom_field_flag != second.bottom_field_flag)
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Unallowed different 'bottom_field_flag' in the same picture.\n"
      );

    if (first.pic_order_cnt_lsb != second.pic_order_cnt_lsb)
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Unallowed different 'pic_order_cnt_lsb' in the same picture.\n"
      );
  }

  return 0;
}

int checkH264SliceLayerWithoutPartitioningChangeCompliance(
  const H264ParametersHandlerPtr handle,
  const H264SliceLayerWithoutPartitioningParameters first,
  const H264SliceLayerWithoutPartitioningParameters second,
  const LibbluESSettingsOptions options
)
{
  /* assert(NULL != handle); */

  return checkH264SliceHeaderChangeCompliance(
    handle, first.header, second.header, options
  );
}