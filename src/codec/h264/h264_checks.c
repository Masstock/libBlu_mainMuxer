#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "h264_checks.h"

int checkH264AccessUnitDelimiterCompliance(
  H264WarningFlags *warn,
  H264AccessUnitDelimiterParameters param
)
{
  LIBBLU_H264_DEBUG_AUD(
    "  Primary coded picture slice types (primary_pic_type): "
    "0x%02" PRIX8 ".\n",
    param.primary_pic_type
  );

  LIBBLU_H264_DEBUG_AUD(
    "   -> Allowed 'slice_type' values: %s.\n",
    H264PrimaryPictureTypeStr(param.primary_pic_type)
  );

  LIBBLU_H264_DEBUG_AUD(
    "   -> BD allowed 'slice_type' values: %s.\n",
    bdAllowedH264PrimaryPictureTypeStr(param.primary_pic_type)
  );

  if (H264_PRIM_PIC_TYPE_ALL < param.primary_pic_type)
    LIBBLU_H264_CK_ERROR_RETURN(
      "Reserved 'primary_pic_type' == %u in use.\n",
      param.primary_pic_type
    );

  if (H264_PRIM_PIC_TYPE_IPB < param.primary_pic_type)
    LIBBLU_H264_CK_BD_FAIL_WCOND_RETURN(
      LIBBLU_WARN_COUNT_CHECK_INC(warn, AUD_wrong_primary_pic_type),
      "Primary coded picture slice types shall be equal to 0, 1 or 2 "
      "in Access Unit Delimiter ('primary_pic_type' == %u).\n",
      param.primary_pic_type
    );

  return 0; /* OK */
}

static int _checkH264ProfileIdcCompliance(
  H264ProfileIdcValue profile_idc,
  H264ContraintFlags constraints
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
    H264ProfileIdcValueStr (
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

    LIBBLU_H264_CK_FAIL_RETURN(
      "%s profile is not part of the profiles allowed by BD-specs "
      "('profile_idc' == %d).\n",
      H264ProfileIdcValueStr (
        profile_idc,
        constraints
      ),
      profile_idc
    );
  }

  return 0; /* OK */
}

static int _checkH264LevelIdcCompliance(
  uint8_t level_idc
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

    LIBBLU_H264_CK_ERROR_RETURN(
      "Unknown/Reserved 'level_idc' == %d in use.\n",
      level_idc
    );
  }

  if (level_idc < 30 || 41 < level_idc) {
    /* BDAV unallowed level_idc. */
    LIBBLU_H264_CK_FAIL_RETURN(
      "Level %" PRIu8 ".%" PRIu8 " is not part of the "
      "levels allowed by BD-specs ('level_idc' == %d).\n",
      level_idc / 10,
      level_idc % 10,
      level_idc
    );
  }

  return 0;
}

static int _checkH264HrdParametersCompliance(
  const H264ParametersHandlerPtr handle,
  H264HrdParameters param
)
{
  unsigned ShedSelIdx;

  LIBBLU_H264_DEBUG_VUI(
    "    Number of CPB configurations "
    "(cpb_cnt_minus1): %u (0x%X).\n",
    param.cpb_cnt_minus1 + 1,
    param.cpb_cnt_minus1
  );

  if (H264_BD_ALLOWED_CPB_CNT <= param.cpb_cnt_minus1) {
    LIBBLU_H264_CK_FAIL_RETURN(
      "%u CPB configurations defined, "
      "expect no more than %u configurations "
      "('cpb_cnt_minus1' == %u).\n",
      param.cpb_cnt_minus1 + 1,
      H264_BD_ALLOWED_CPB_CNT,
      param.cpb_cnt_minus1
    );
  }

  if (H264_MAX_CPB_CONFIGURATIONS <= param.cpb_cnt_minus1) {
    LIBBLU_H264_CK_ERROR_RETURN(
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
      param.SchedSel[ShedSelIdx].bit_rate_value_minus1,
      param.SchedSel[ShedSelIdx].bit_rate_value_minus1
    );

    if (0 < ShedSelIdx) {
      if (
        param.SchedSel[ShedSelIdx].bit_rate_value_minus1
        <= param.SchedSel[ShedSelIdx - 1].bit_rate_value_minus1
      ) {
        LIBBLU_H264_CK_ERROR_RETURN(
          "Invalid HRD 'bit_rate_value_minus1[SchedSelIdx]' value for "
          "Schedule Selection Index %u, "
          "'bit_rate_value_minus1[SchedSelIdx-1]' == %" PRIu32 " shall be "
          "lower than 'bit_rate_value_minus1[SchedSelIdx]' == %" PRIu32 ".\n",
          ShedSelIdx,
          param.SchedSel[ShedSelIdx-1].bit_rate_value_minus1,
          param.SchedSel[ShedSelIdx].bit_rate_value_minus1
        );
      }
    }

    LIBBLU_H264_DEBUG_VUI(
      "       => Bit rate (BitRate): %" PRIu64 " bits/s.\n",
      param.SchedSel[ShedSelIdx].BitRate
    );

    if (handle->constraints.max_bit_rate < param.SchedSel[ShedSelIdx].BitRate)
      LIBBLU_H264_CK_FAIL_RETURN(
        "Bit rate value exceed maximum value of %u bps for ShedSelIdx %u "
        "('BitRate' == %u).\n",
        handle->constraints.max_bit_rate,
        ShedSelIdx,
        param.SchedSel[ShedSelIdx].BitRate
      );

    LIBBLU_H264_DEBUG_VUI(
      "      Coded Picture Buffer size value "
      "(cpb_size_value_minus1): %" PRIu32 " (0x%" PRIX32 ").\n",
      param.SchedSel[ShedSelIdx].cpb_size_value_minus1,
      param.SchedSel[ShedSelIdx].cpb_size_value_minus1
    );

    if (0 < ShedSelIdx) {
      if (
        param.SchedSel[ShedSelIdx].cpb_size_value_minus1
        > param.SchedSel[ShedSelIdx - 1].cpb_size_value_minus1
      ) {
        LIBBLU_H264_CK_ERROR_RETURN(
          "Invalid HRD 'cpb_size_value_minus1[SchedSelIdx]' value for "
          "Schedule Selection Index %u, "
          "'cpb_size_value_minus1[SchedSelIdx-1]' == %" PRIu32 " shall be "
          "greater or equal to 'cpb_size_value_minus1[SchedSelIdx]' "
          "== %" PRIu32 ".\n",
          ShedSelIdx,
          param.SchedSel[ShedSelIdx-1].cpb_size_value_minus1,
          param.SchedSel[ShedSelIdx].cpb_size_value_minus1
        );
      }
    }

    LIBBLU_H264_DEBUG_VUI(
      "       => CPB size (CpbSize): %" PRIu64 " bits.\n",
      param.SchedSel[ShedSelIdx].CpbSize
    );

    LIBBLU_H264_DEBUG_VUI(
      "      Constant Bit-Rate mode (cbr_flag): %s (0x%X).\n",
      BOOL_STR(param.SchedSel[ShedSelIdx].cbr_flag),
      param.SchedSel[ShedSelIdx].cbr_flag
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

static int _checkAspectRatioInfoH264VuiParametersCompliance(
  H264WarningFlags *warn,
  H264SPSDataParameters sps,
  H264VuiParameters vui
)
{
  H264BdavExpectedAspectRatioRet expected;

  expected = getH264BDExpectedAspectRatioIdc(
    sps.FrameWidth,
    sps.FrameHeight
  );

  if (!isIncludedH264BdavExpectedAspectRatio(expected, vui.aspect_ratio_idc)) {
    if (expected.a != expected.b) {
      LIBBLU_H264_CK_BD_FAIL_WCOND_RETURN(
        LIBBLU_WARN_COUNT_CHECK_INC(warn, VUI_wrong_SAR),
        "Sample Aspect Ratio in SPS VUI parameters for %ux%u, "
        "shall be %s or %s for this resolution, not %s "
        "('aspect_ratio_idc' == %d).\n",
        sps.FrameWidth,
        sps.FrameHeight,
        H264AspectRatioIdcValueStr(expected.a),
        H264AspectRatioIdcValueStr(expected.b),
        H264AspectRatioIdcValueStr(vui.aspect_ratio_idc),
        vui.aspect_ratio_idc
      );
    }
    else {
      LIBBLU_H264_CK_BD_FAIL_WCOND_RETURN(
        LIBBLU_WARN_COUNT_CHECK_INC(warn, VUI_wrong_SAR),
        "Sample Aspect Ratio in SPS VUI parameters for %ux%u, "
        "shall be %s for this resolution, not %s "
        "('aspect_ratio_idc' == %d).\n",
        sps.FrameWidth,
        sps.FrameHeight,
        H264AspectRatioIdcValueStr(expected.a),
        H264AspectRatioIdcValueStr(vui.aspect_ratio_idc),
        vui.aspect_ratio_idc
      );
    }
  }

  return 0;
}

static int _checkVideoFormatH264VuiParametersCompliance(
  const H264ParametersHandlerPtr handle,
  H264VuiParameters vui
)
{
  H264VideoFormatValue expected = getH264BDExpectedVideoFormat(
    vui.FrameRate
  );

  if (expected != vui.video_format) {
    LIBBLU_H264_CK_BD_WCOND_WARNING(
      LIBBLU_WARN_COUNT_CHECK_INC(&handle->warning_flags, VUI_wrong_video_format),
      "Unexpected video format '%s' with frame rate of %.3f, shall be '%s'"
      "('video_format' == %u).\n",
      H264VideoFormatValueStr(vui.video_format),
      vui.FrameRate,
      H264VideoFormatValueStr(expected),
      vui.video_format
    );

    handle->cur_prog_param.wrong_VUI = true;
  }

  return 0;
}

static int _checkColPrimH264VuiParametersCompliance(
  const H264ParametersHandlerPtr handle,
  H264SPSDataParameters sps,
  H264VuiColourDescriptionParameters colour_desc
)
{
  H264ColourPrimariesValue expected = getH264BDExpectedColorPrimaries(
    sps.FrameHeight
  );

  if (expected != colour_desc.colour_primaries) {
    LIBBLU_H264_CK_BD_WCOND_WARNING(
      LIBBLU_WARN_COUNT_CHECK_INC(&handle->warning_flags, VUI_wrong_color_primaries),
      "Color Primaries in SPS VUI parameters should be defined to '%s', "
      "not '%s' ('colour_primaries' == 0x%X).\n",
      H264ColorPrimariesValueStr(expected),
      H264ColorPrimariesValueStr(colour_desc.colour_primaries),
      colour_desc.colour_primaries
    );
    handle->cur_prog_param.wrong_VUI = true;
  }

  return 0;
}

static int _checkTransfCharactH264VuiParametersCompliance(
  const H264ParametersHandlerPtr handle,
  H264SPSDataParameters sps,
  H264VuiColourDescriptionParameters colour_desc
)
{
  H264TransferCharacteristicsValue expected = getH264BDExpectedTransferCharacteritics(
    sps.FrameHeight
  );

  if (expected != colour_desc.transfer_characteristics) {
    LIBBLU_H264_CK_BD_WCOND_WARNING(
      LIBBLU_WARN_COUNT_CHECK_INC(&handle->warning_flags, VUI_wrong_transfer_characteristics),
      "Transfer Characteristics in SPS VUI parameters should "
      "be defined to '%s', not '%s' "
      "('transfer_characteristics' == 0x%X).\n",
      H264TransferCharacteristicsValueStr(expected),
      H264TransferCharacteristicsValueStr(colour_desc.transfer_characteristics),
      colour_desc.transfer_characteristics
    );
    handle->cur_prog_param.wrong_VUI = true;
  }

  return 0;
}

static int _checkMatrixCoeffH264VuiParametersCompliance(
  const H264ParametersHandlerPtr handle,
  H264SPSDataParameters sps,
  H264VuiColourDescriptionParameters colour_desc
)
{
  H264MatrixCoefficientsValue expected = getH264BDExpectedMatrixCoefficients(
    sps.FrameHeight
  );

  if (expected != colour_desc.matrix_coefficients) {
    LIBBLU_H264_CK_BD_WCOND_WARNING(
      LIBBLU_WARN_COUNT_CHECK_INC(&handle->warning_flags, VUI_wrong_matrix_coefficients),
      "Matrix coefficients in SPS VUI parameters should "
      "be defined to '%s', not '%s' ('matrix_coefficients' == 0x%X).\n",
      H264MatrixCoefficientsValueStr(expected),
      H264MatrixCoefficientsValueStr(colour_desc.matrix_coefficients),
      colour_desc.matrix_coefficients
    );
    handle->cur_prog_param.wrong_VUI = true;
  }

  return 0;
}

static int _checkH264VuiColDescParametersCompliance(
  const H264ParametersHandlerPtr handle,
  H264SPSDataParameters sps,
  H264VuiColourDescriptionParameters colour_desc
)
{

  LIBBLU_H264_DEBUG_VUI(
    "     -> Colour primaries "
    "(colour_primaries): %s (0x%X).\n",
    H264ColorPrimariesValueStr(colour_desc.colour_primaries),
    colour_desc.colour_primaries
  );

  if (_checkColPrimH264VuiParametersCompliance(handle, sps, colour_desc) < 0)
    return -1;

  LIBBLU_H264_DEBUG_VUI(
    "     -> Transfer characteristics "
    "(transfer_characteristics): %s (0x%X).\n",
    H264TransferCharacteristicsValueStr(colour_desc.transfer_characteristics),
    colour_desc.transfer_characteristics
  );

  if (_checkTransfCharactH264VuiParametersCompliance(handle, sps, colour_desc) < 0)
    return -1;

  LIBBLU_H264_DEBUG_VUI(
    "     -> Matrix coefficients "
    "(matrix_coefficients): %s (0x%X).\n",
    H264MatrixCoefficientsValueStr(colour_desc.matrix_coefficients),
    colour_desc.matrix_coefficients
  );

  if (_checkMatrixCoeffH264VuiParametersCompliance(handle, sps, colour_desc) < 0)
    return -1;

  return 0;
}

static int _checkH264VuiParametersCompliance(
  const H264ParametersHandlerPtr handle,
  H264SPSDataParameters sps
)
{
  H264VuiParameters vui = sps.vui_parameters;

  LIBBLU_H264_DEBUG_VUI(
    "   Aspect ratio information "
    "(aspect_ratio_info_present_flag): %s (0b%X).\n",
    BOOL_PRESENCE(vui.aspect_ratio_info_present_flag),
    vui.aspect_ratio_info_present_flag
  );

  if (vui.aspect_ratio_info_present_flag) {
    LIBBLU_H264_DEBUG_VUI(
      "    -> Sample Aspect Ratio, SAR "
      "(aspect_ratio_idc): %s (0x%X).\n",
      H264AspectRatioIdcValueStr(vui.aspect_ratio_idc),
      vui.aspect_ratio_idc
    );

    if (
      H264_ASPECT_RATIO_IDC_2_BY_1 < vui.aspect_ratio_idc
      && vui.aspect_ratio_idc != H264_ASPECT_RATIO_IDC_EXTENDED_SAR
    ) {
      LIBBLU_H264_CK_ERROR_RETURN(
        "Reserved 'aspect_ratio_idc' == %d in use in VUI parameters.\n",
        vui.aspect_ratio_idc
      );
    }

    if (vui.aspect_ratio_idc == H264_ASPECT_RATIO_IDC_EXTENDED_SAR) {
      LIBBLU_H264_DEBUG_VUI(
        "    -> Sample aspect ratio horizontal size (sar_width): %u.\n",
        vui.sar_width
      );
      LIBBLU_H264_DEBUG_VUI(
        "    -> Sample aspect ratio vertical size (sar_height): %u.\n",
        vui.sar_height
      );
      LIBBLU_H264_DEBUG_VUI(
        "     => Extended SAR ratio: %u:%u.\n",
        vui.sar_width,
        vui.sar_height
      );
    }

    /* Check aspect_ratio_idc values */
    if (_checkAspectRatioInfoH264VuiParametersCompliance(&handle->warning_flags, sps, vui) < 0)
      return -1;
  }
  else {
    LIBBLU_H264_DEBUG_VUI(
      "    -> Sample Aspect Ratio, SAR "
      "(aspect_ratio_idc): %s (Inferred).\n",
      H264AspectRatioIdcValueStr(vui.aspect_ratio_idc),
      vui.aspect_ratio_idc
    );

    LIBBLU_H264_CK_FAIL_RETURN(
      "Missing required 'aspect_ratio_idc' information in SPS VUI parameters "
      "('aspect_ratio_info_present_flag' == 0b0).\n"
    );
  }

  LIBBLU_H264_DEBUG_VUI(
    "   Over-scan preference information "
    "(overscan_info_present_flag): %s (0b%X).\n",
    BOOL_PRESENCE(vui.overscan_info_present_flag),
    vui.overscan_info_present_flag
  );

  if (vui.overscan_info_present_flag) {
    LIBBLU_H264_DEBUG_VUI(
      "    -> Over-scan preference (overscan_appropriate_flag): %s (0b%X).\n",
      H264OverscanAppropriateValueStr(vui.overscan_appropriate_flag),
      vui.overscan_appropriate_flag
    );
  }
  else {
    LIBBLU_H264_DEBUG_VUI(
      "    -> Over-scan preference: %s (Inferred).\n",
      H264OverscanAppropriateValueStr(vui.overscan_appropriate_flag)
    );
  }

  if (H264_OVERSCAN_APPROP_CROP == vui.overscan_appropriate_flag) {
    LIBBLU_H264_CK_BD_WCOND_WARNING(
      LIBBLU_WARN_COUNT_CHECK_INC(&handle->warning_flags, VUI_wrong_overscan_appropriate_flag),
      "Over-scan preference in VUI parameters shouldn't allow cropping "
      "of decoded pictures ('overscan_appropriate_flag' == 0b0).\n"
    );
    handle->cur_prog_param.wrong_VUI = true;
  }

  LIBBLU_H264_DEBUG_VUI(
    "   Video signal type parameters "
    "(video_signal_type_present_flag): %s (0b%X).\n",
    BOOL_PRESENCE(vui.video_signal_type_present_flag),
    vui.video_signal_type_present_flag
  );

  if (vui.video_signal_type_present_flag) {
    handle->cur_prog_param.missing_VUI_video_signal_type = false;

    LIBBLU_H264_DEBUG_VUI(
      "    -> Video format (video_format): %s (0x%X).\n",
      H264VideoFormatValueStr(vui.video_format),
      vui.video_format
    );

    if (H264_VIDEO_FORMAT_UNSPECIFIED < vui.video_format)
      LIBBLU_H264_CK_ERROR_RETURN(
        "Reserved 'video_format' == %d in use.\n",
        vui.video_format
      );

    if (_checkVideoFormatH264VuiParametersCompliance(handle, vui) < 0)
      return -1;

    LIBBLU_H264_DEBUG_VUI(
      "    -> Full-range luma and chroma samples values "
      "(video_full_range_flag): %s (0b%X).\n",
      BOOL_STR(vui.video_full_range_flag),
      vui.video_full_range_flag
    );

    if (vui.video_full_range_flag) {
      LIBBLU_H264_CK_BD_WCOND_WARNING(
        LIBBLU_WARN_COUNT_CHECK_INC(&handle->warning_flags, VUI_wrong_video_full_range_flag),
        "Video color range shall be set to 'limited' in SPS VUI parameters "
        "according to BD-specs ('video_full_range_flag' == 0b1).\n"
      );
      handle->cur_prog_param.wrong_VUI = true;
    }

    LIBBLU_H264_DEBUG_VUI(
      "    -> Color description parameters "
      "(colour_description_present_flag): %s (0b%X).\n",
      BOOL_PRESENCE(vui.colour_description_present_flag),
      vui.colour_description_present_flag
    );

    if (vui.colour_description_present_flag) {
      H264VuiColourDescriptionParameters col_desc = vui.colour_description;

      if (_checkH264VuiColDescParametersCompliance(handle, sps, col_desc) < 0)
        return -1;
    }
  }
  else {
    handle->cur_prog_param.missing_VUI_video_signal_type = true;

    LIBBLU_H264_DEBUG_VUI(
      "    -> Video format (video_format): %s (Inferred).\n",
      H264VideoFormatValueStr(vui.video_format)
    );
    LIBBLU_H264_DEBUG_VUI(
      "    -> Full-range luma and chroma samples values "
      "(video_full_range_flag): %s (Inferred).\n",
      BOOL_STR(vui.video_full_range_flag)
    );
    LIBBLU_H264_DEBUG_VUI(
      "    -> Color description parameters "
      "(colour_description_present_flag): %s (Inferred).\n",
      BOOL_PRESENCE(vui.colour_description_present_flag)
    );

    LIBBLU_H264_CK_BD_WCOND_WARNING(
      LIBBLU_WARN_COUNT_CHECK_INC(&handle->warning_flags, VUI_missing_video_signal_type),
      "Video Signal Type parameters shall be present in SPS VUI.\n"
    );

    handle->cur_prog_param.wrong_VUI = true;
  }

  if (!vui.colour_description_present_flag) {
    H264VuiColourDescriptionParameters col_desc = vui.colour_description;

    LIBBLU_H264_DEBUG_VUI(
      "     -> Colour primaries "
      "(colour_primaries): %s (Inferred).\n",
      H264ColorPrimariesValueStr(col_desc.colour_primaries),
      col_desc.colour_primaries
    );

    LIBBLU_H264_DEBUG_VUI(
      "     -> Transfer characteristics "
      "(transfer_characteristics): %s (Inferred).\n",
      H264TransferCharacteristicsValueStr(col_desc.transfer_characteristics),
      col_desc.transfer_characteristics
    );

    LIBBLU_H264_DEBUG_VUI(
      "     -> Matrix coefficients "
      "(matrix_coefficients): %s (Inferred).\n",
      H264MatrixCoefficientsValueStr(col_desc.matrix_coefficients),
      col_desc.matrix_coefficients
    );
  }

  if (!vui.colour_description_present_flag) {
    LIBBLU_H264_CK_BD_WCOND_WARNING(
      LIBBLU_WARN_COUNT_CHECK_INC(&handle->warning_flags, VUI_missing_colour_description),
      "Color Description parameters shall be present in SPS VUI.\n"
    );

    handle->cur_prog_param.wrong_VUI = true;
  }

  LIBBLU_H264_DEBUG_VUI(
    "   Chroma location information "
    "(chroma_loc_info_present_flag): %s (0b%X).\n",
    BOOL_PRESENCE(vui.chroma_loc_info_present_flag),
    vui.chroma_loc_info_present_flag
  );

  if (vui.chroma_loc_info_present_flag) {
    LIBBLU_H264_DEBUG_VUI(
      "    -> Top-field "
      "(chroma_sample_loc_type_top_field): %u (0x%X).\n",
      vui.chroma_sample_loc_type_top_field,
      vui.chroma_sample_loc_type_top_field
    );
    LIBBLU_H264_DEBUG_VUI(
      "    -> Bottom-field "
      "(chroma_sample_loc_type_bottom_field): %u (0x%X).\n",
      vui.chroma_sample_loc_type_bottom_field,
      vui.chroma_sample_loc_type_bottom_field
    );

    if (
      (
        vui.chroma_sample_loc_type_top_field != 0
        && vui.chroma_sample_loc_type_top_field != 2
      )
      || (
        vui.chroma_sample_loc_type_bottom_field != 0
        && vui.chroma_sample_loc_type_bottom_field != 2
      )
    ) {
      LIBBLU_H264_CK_FAIL_RETURN(
        "Chroma sample location if present shall be set to 0 or 2 "
        "according to BD-specs ('chroma_sample_loc_type_top_field' == %d / "
        "'chroma_sample_loc_type_bottom_field' == %d).\n",
        vui.chroma_sample_loc_type_top_field,
        vui.chroma_sample_loc_type_bottom_field
      );
    }
  }
  else {
    LIBBLU_H264_DEBUG_VUI(
      "    -> Top-field "
      "(chroma_sample_loc_type_top_field): %u (Inferred).\n",
      vui.chroma_sample_loc_type_top_field
    );
    LIBBLU_H264_DEBUG_VUI(
      "    -> Bottom-field "
      "(chroma_sample_loc_type_bottom_field): %u (Inferred).\n",
      vui.chroma_sample_loc_type_bottom_field
    );
  }

  LIBBLU_H264_DEBUG_VUI(
    "   Timing information (timing_info_present_flag): %s (0b%X).\n",
    BOOL_PRESENCE(vui.timing_info_present_flag),
    vui.timing_info_present_flag
  );

  if (!vui.timing_info_present_flag)
    LIBBLU_H264_CK_ERROR_RETURN(
      "Timing information shall be present in SPS VUI parameters "
      "('timing_info_present_flag' == 0b0).\n"
    );

  if (vui.timing_info_present_flag) {
    LIBBLU_H264_DEBUG_VUI(
      "    -> Number of time units (num_units_in_tick): "
      "%" PRIu32 " (0x%" PRIx32 ").\n",
      vui.num_units_in_tick,
      vui.num_units_in_tick
    );

    LIBBLU_H264_DEBUG_VUI(
      "    -> Time scale clock (time_scale): "
      "%" PRIu32 " Hz (0x%" PRIx32 ").\n",
      vui.time_scale,
      vui.time_scale
    );

    LIBBLU_H264_DEBUG_VUI(
      "    -> Fixed frame rate "
      "(fixed_frame_rate_flag): %s (0x%X).\n",
      BOOL_STR(vui.fixed_frame_rate_flag),
      vui.fixed_frame_rate_flag
    );

    if (!vui.fixed_frame_rate_flag) {
      LIBBLU_H264_CK_FAIL_RETURN(
        "Forbidden use of a variable frame-rate mode "
        "('fixed_frame_rate_flag' == 0b0).\n"
      );
    }

    /* Equation D-2 */
    LIBBLU_H264_DEBUG_VUI(
      "    => Frame-rate: %.3f.\n",
      vui.FrameRate
    );
  }

  LIBBLU_H264_DEBUG_VUI(
    "   NAL HRD parameters "
    "(nal_hrd_parameters_present_flag): %s (0b%X).\n",
    BOOL_PRESENCE(vui.nal_hrd_parameters_present_flag),
    vui.nal_hrd_parameters_present_flag
  );

  if (vui.nal_hrd_parameters_present_flag) {
    if (_checkH264HrdParametersCompliance(handle, vui.nal_hrd_parameters) < 0)
      return -1;
  }

  LIBBLU_H264_DEBUG_VUI(
    "   VCL HRD parameters "
    "(vcl_hrd_parameters_present_flag): %s (0b%X).\n",
    BOOL_PRESENCE(vui.vcl_hrd_parameters_present_flag),
    vui.vcl_hrd_parameters_present_flag
  );

  if (vui.vcl_hrd_parameters_present_flag) {
    if (_checkH264HrdParametersCompliance(handle, vui.vcl_hrd_parameters) < 0)
      return -1;
  }

  if (vui.nal_hrd_parameters_present_flag || vui.vcl_hrd_parameters_present_flag) {
    LIBBLU_H264_DEBUG_VUI(
      "   Low delay (low_delay_hrd_flag): %s (0b%X).\n",
      BOOL_STR(vui.low_delay_hrd_flag),
      vui.low_delay_hrd_flag
    );

    if (vui.low_delay_hrd_flag)
      LIBBLU_H264_CK_BD_FAIL_RETURN(
        "Forbidden use of low delay HRD ('low_delay_hrd_flag' == 0b1).\n"
      );
  }

  if (vui.nal_hrd_parameters_present_flag && vui.vcl_hrd_parameters_present_flag) {
    /* Check length fields being identical between NAL and VCL HRD */
    bool same = true;

    char field_names[40 * 4] = {'\0'};
    char *fn_ptr = field_names;

#define NOT_SAME(fn)  (vui.nal_hrd_parameters.fn != vui.vcl_hrd_parameters.fn)

    if (NOT_SAME(initial_cpb_removal_delay_length_minus1)) {
      lb_str_cat_comma(&fn_ptr, "initial_cpb_removal_delay_length_minus1", !same);
      same = false;
    }

    if (NOT_SAME(cpb_removal_delay_length_minus1)) {
      lb_str_cat_comma(&fn_ptr, "cpb_removal_delay_length_minus1", !same);
      same = false;
    }

    if (NOT_SAME(dpb_output_delay_length_minus1)) {
      lb_str_cat_comma(&fn_ptr, "dpb_output_delay_length_minus1", !same);
      same = false;
    }

    if (NOT_SAME(time_offset_length)) {
      lb_str_cat_comma(&fn_ptr, "time_offset_length", !same);
      same = false;
    }

    if (!same) {
      LIBBLU_H264_CK_ERROR_RETURN(
        "NAL HRD and VCL HRD when both present shall share identical length "
        "fields, got mismatch on '%s'.\n",
        field_names
      );
    }

#undef NOT_SAME
  }

  LIBBLU_H264_DEBUG_VUI(
    "   Picture Structure in SEI picture timing messages "
    "(pic_struct_present_flag): %s (0b%X).\n",
    BOOL_PRESENCE(vui.pic_struct_present_flag),
    vui.pic_struct_present_flag
  );
  /**
   * NOTE: Compliance of 'pic_struct_present_flag' is verified
   * in SPS verifier (calling function).
   */

  LIBBLU_H264_DEBUG_VUI(
    "   Video Sequence Bitstream Restriction parameters "
    "(bitstream_restriction_flag): %s (0b%X).\n",
    BOOL_PRESENCE(vui.bitstream_restriction_flag),
    vui.bitstream_restriction_flag
  );

  if (vui.bitstream_restriction_flag) {
    const H264VuiVideoSeqBsRestrParameters *restrParam =
      &vui.bistream_restrictions
    ;

    LIBBLU_H264_DEBUG_VUI(
      "    Samples outside picture boundaries in Motion Vectors "
      "(motion_vectors_over_pic_boundaries_flag): %s (0b%X).\n",
      BOOL_PRESENCE(restrParam->motion_vectors_over_pic_boundaries_flag),
      restrParam->motion_vectors_over_pic_boundaries_flag
    );

    LIBBLU_H264_DEBUG_VUI(
      "    Maximum picture size (max_bytes_per_pic_denom): "
      "%u bytes (0x%02X).\n",
      restrParam->max_bytes_per_pic_denom,
      restrParam->max_bytes_per_pic_denom
    );
    if (!restrParam->max_bytes_per_pic_denom)
      LIBBLU_H264_DEBUG_VUI(
        "     -> Unlimited.\n"
      );

    LIBBLU_H264_DEBUG_VUI(
      "    Maximum macro-block size (max_bits_per_mb_denom): "
      "%u bits (0x%02X).\n",
      restrParam->max_bits_per_mb_denom,
      restrParam->max_bits_per_mb_denom
    );
    if (!restrParam->max_bits_per_mb_denom)
      LIBBLU_H264_DEBUG_VUI(
        "     -> Unlimited.\n"
      );

    LIBBLU_H264_DEBUG_VUI(
      "    Maximum horizontal motion vector length "
      "(log2_max_mv_length_horizontal): "
      "%.2f luma sample(s) (0x%02X).\n",
      0.25 * restrParam->log2_max_mv_length_horizontal,
      restrParam->log2_max_mv_length_horizontal
    );

    LIBBLU_H264_DEBUG_VUI(
      "    Maximum vertical motion vector length "
      "(log2_max_mv_length_vertical): "
      "%.2f luma sample(s) (0x%02X).\n",
      0.25 * restrParam->log2_max_mv_length_vertical,
      restrParam->log2_max_mv_length_vertical
    );

    LIBBLU_H264_DEBUG_VUI(
      "    Maximum number of frames in DPB "
      "(max_num_reorder_frames): "
      "%" PRIu16 " frame buffers (0x%04X).\n",
      restrParam->max_num_reorder_frames,
      restrParam->max_num_reorder_frames
    );

    LIBBLU_H264_DEBUG_VUI(
      "    HRD-DPB required size "
      "(max_dec_frame_buffering): "
      "%" PRIu16 " frame buffers (0x%04X).\n",
      restrParam->max_dec_frame_buffering,
      restrParam->max_dec_frame_buffering
    );
  }

  return 0;
}

static const char * _getSequenceScalingListName(
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
  const H264ParametersHandlerPtr handle,
  H264SPSDataParameters param
)
{
  assert(NULL != handle);

  if (_checkH264ProfileIdcCompliance(param.profile_idc, param.constraint_set_flags) < 0)
    return -1;
  updateH264ProfileLimits(handle, param.profile_idc, param.constraint_set_flags);

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

  if (_checkH264LevelIdcCompliance(param.level_idc) < 0)
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

  if (0 == handle->constraints.MaxFS)
    LIBBLU_H264_CK_ERROR_RETURN(
      "Unable to a find a MaxFS value for 'level_idc' == 0x%02" PRIX8 ".\n",
      param.level_idc
    );

  handle->constraints.max_bit_rate = getH264BrNal(
    handle->constraints,
    param.profile_idc
  );

  LIBBLU_H264_DEBUG_SPS(
    "   => BitRate "
    "(Maximum allowed NAL HRD Bit Rate in bits/sec): %u.\n",
    handle->constraints.max_bit_rate
  );

  LIBBLU_H264_DEBUG_SPS(
    "  Sequence Parameter Set ID (seq_parameter_set_id): %u (0x%X).\n",
    param.seq_parameter_set_id,
    param.seq_parameter_set_id
  );

  if (0x00 != param.seq_parameter_set_id)
    LIBBLU_H264_CK_BD_FAIL_RETURN(
      "Invalid Sequence Parameter Set ID, "
      "only one SPS with ID 0 shall be present in bitstream "
      "('seq_parameter_set_id' == %u).\n",
      param.seq_parameter_set_id
    );

  /**
   * TODO: Enable better check of non multiple SPS ID.
   */
  if (0x00 != param.seq_parameter_set_id)
    LIBBLU_TODO_MSG(
      "Unsupported multiple Sequence Parameters Set definitions, "
      "only 'seq_parameter_set_id' == 0x0 supported.\n"
    );

  if (handle->sequence_parameter_set_valid)
    LIBBLU_H264_CK_BD_FAIL_RETURN(
      "Unexpected multiple Sequence Parameter Set, "
      "there should only be one SPS present in the first access unit of the GOP "
      "('seq_parameter_set_id' == %u).\n",
      param.seq_parameter_set_id
    );

  if (isHighScalableMultiviewH264ProfileIdcValue(param.profile_idc)) {
    LIBBLU_H264_DEBUG_SPS(
      "  Chroma sampling format (chroma_format_idc): %s (0x%X).\n",
      H264ChromaFormatIdcValueStr(param.chroma_format_idc),
      param.chroma_format_idc
    );

    if (H264_CHROMA_FORMAT_444 < param.chroma_format_idc)
      LIBBLU_H264_CK_ERROR_RETURN(
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
        LIBBLU_H264_CK_FAIL_RETURN(
          "Unexpected Chroma sampling format %s "
          "('chroma_format_idc' == %d).\n",
          H264ChromaFormatIdcValueStr(param.chroma_format_idc),
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
      LIBBLU_H264_CK_ERROR(
        "Luma and/or chroma bit-depth used are not allowed for "
        "bitstream profile conformance.\n"
      );

      if (
        handle->constraints.maxAllowedBitDepthMinus8
        < param.bit_depth_luma_minus8
      )
        LIBBLU_H264_CK_ERROR(
          " -> Luma bit-depth: %u bits over %u bits maximal "
          "allowed value.\n",
          param.BitDepthLuma,
          handle->constraints.maxAllowedBitDepthMinus8 + 8
        );

      if (
        handle->constraints.maxAllowedBitDepthMinus8
        < param.bit_depth_chroma_minus8
      )
        LIBBLU_H264_CK_ERROR(
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
      LIBBLU_H264_CK_FAIL_RETURN(
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
      LIBBLU_H264_DEBUG_SPS("  Sequence Scaling Lists:\n");

      unsigned nb_lists = (param.chroma_format_idc != H264_CHROMA_FORMAT_444) ? 8 : 12;
      for (unsigned i = 0; i < nb_lists; i++) {
        LIBBLU_H264_DEBUG_SPS(
          "   -> %s (seq_scaling_list_present_flag[%X]): %s (0b%X).\n",
          _getSequenceScalingListName(i), i,
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
      H264ChromaFormatIdcValueStr(param.chroma_format_idc)
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
    LIBBLU_H264_CK_ERROR_RETURN(
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
      LIBBLU_H264_CK_ERROR_RETURN(
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

    for (unsigned i = 0; i < param.num_ref_frames_in_pic_order_cnt_cycle; i++) {
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
    break;

  default:
    LIBBLU_H264_DEBUG_SPS(
      "   -> Reserved value (Unknown type).\n"
    );

    LIBBLU_H264_CK_ERROR_RETURN(
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

  if (0 == handle->constraints.MaxDpbMbs)
    LIBBLU_H264_CK_ERROR_RETURN(
      "Unable to a find a MaxDpbMbs value for 'level_idc' == 0x%02" PRIX8 ".\n",
      param.level_idc
    );

  /* Equation A.3.1.h) */
  unsigned MaxDpbFrames = MIN(
    handle->constraints.MaxDpbMbs / (param.PicWidthInMbs * param.FrameHeightInMbs),
    16
  );

  if (MaxDpbFrames < param.max_num_ref_frames)
    LIBBLU_H264_CK_FAIL_RETURN(
      "Out of range 'max_num_ref_frames' == %u in SPS, shall not exceed "
      "'MaxDpbFrames' == %u for bitstream level conformance.\n",
      param.max_num_ref_frames, MaxDpbFrames
    );

  LIBBLU_H264_DEBUG_SPS(
    "  Gaps allowed in frame number values "
    "(gaps_in_frame_num_value_allowed_flag): %s (0x%X).\n",
    BOOL_STR(param.gaps_in_frame_num_value_allowed_flag),
    param.gaps_in_frame_num_value_allowed_flag
  );

  if (param.gaps_in_frame_num_value_allowed_flag)
    LIBBLU_H264_CK_FAIL_RETURN(
      "Unexpected allowed presence of gaps in frame number value "
      "('gaps_in_frame_num_value_allowed_flag' == 0b1).\n"
    );

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
  if (handle->constraints.MaxFS * 8 < SQUARE(param.PicWidthInMbs))
    LIBBLU_H264_CK_FAIL_RETURN(
      "Rec. ITU-T H.264 %s constraint is not satisfied "
      "((MaxFS * 8) = %u < (PicWidthInMbs ** 2) = %u "
      "for level %" PRIu8 ".%" PRIu8 "), "
      "leading a violation of bitstream profile/level conformance.\n",
      isHighScalableMultiviewH264ProfileIdcValue(param.profile_idc) ? "A.3.2.d)" : "A.3.1.f)",
      handle->constraints.MaxFS * 8,
      SQUARE(param.PicWidthInMbs),
      param.level_idc / 10, param.level_idc % 10
    );

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
    param.PicWidthInMbs *param.PicHeightInMapUnits
  );

  LIBBLU_H264_DEBUG_SPS(
    "  Progressive stream, only using frame-typed macroblocks "
    "(frame_mbs_only_flag): %s (0x%X).\n",
    BOOL_STR(param.frame_mbs_only_flag),
    param.frame_mbs_only_flag
  );

  if (!param.frame_mbs_only_flag) {
    if (handle->constraints.restrictedFrameMbsOnlyFlag) {
      LIBBLU_H264_CK_FAIL_RETURN(
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
      LIBBLU_H264_CK_FAIL_RETURN(
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
  if (handle->constraints.MaxFS * 8u < SQUARE(param.FrameHeightInMbs)) {
    LIBBLU_H264_CK_FAIL_RETURN(
      "Rec. ITU-T H.264 %s constraint is not satisfied "
      "((MaxFS * 8) = %u < (FrameHeightInMbs ** 2) = %u "
      "for level %" PRIu8 ".%" PRIu8 ") "
      "leading a violation of bitstream profile/level conformance.\n",
      isHighScalableMultiviewH264ProfileIdcValue(param.profile_idc) ? "A.3.2.e)" : "A.3.1.g)",
      handle->constraints.MaxFS * 8u,
      SQUARE(param.FrameHeightInMbs),
      param.level_idc / 10, param.level_idc % 10
    );
  }

  LIBBLU_H264_DEBUG_SPS(
    "   => FrameHeight "
    "(decoded frame height after applying cropping): %u pixels.\n",
    param.FrameHeight
  );

  /* Constraint Rec. ITU-T H.264 A.3.1.e) / A.3.2.c) */
  if (handle->constraints.MaxFS < param.PicWidthInMbs *param.FrameHeightInMbs) {
    LIBBLU_H264_CK_FAIL_RETURN(
      "Rec. ITU-T H.264 %s constraint is not satisfied "
      "(MaxFS = %u < PicWidthInMbs *FrameHeightInMbs = %u "
      "for level %" PRIu8 ".%" PRIu8 ") "
      "leading a violation of bitstream profile/level conformance.\n",
      isHighScalableMultiviewH264ProfileIdcValue(param.profile_idc) ? "A.3.2.c)" : "A.3.1.e)",
      handle->constraints.MaxFS,
      param.PicWidthInMbs *param.FrameHeightInMbs,
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
      LIBBLU_H264_CK_FAIL_RETURN(
        "Derivation process mode flag 'direct_8x8_inference_flag' should be "
        "equal to 0b1 since 'frame_mbs_only_flag' == 0b0.\n"
      );
    }

    if (handle->constraints.forcedDirect8x8InferenceFlag) {
      LIBBLU_H264_CK_FAIL_RETURN(
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
      LIBBLU_H264_CK_FAIL_RETURN(
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
      param.frame_crop_offset.left *param.CropUnitX,
      param.frame_crop_offset.left
    );

    LIBBLU_H264_DEBUG_SPS(
      "   -> Right (frame_crop_right_offset): %u pixels (0x%X).\n",
      param.frame_crop_offset.right *param.CropUnitX,
      param.frame_crop_offset.right
    );

    LIBBLU_H264_DEBUG_SPS(
      "   -> Top (frame_crop_top_offset): %u pixels (0x%X).\n",
      param.frame_crop_offset.top *param.CropUnitY,
      param.frame_crop_offset.top
    );

    LIBBLU_H264_DEBUG_SPS(
      "   -> Bottom (frame_crop_bottom_offset): %u pixels (0x%X).\n",
      param.frame_crop_offset.bottom *param.CropUnitY,
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

  if (!param.vui_parameters_present_flag)
    LIBBLU_H264_CK_BD_FAIL_RETURN(
      "VUI parameters shall be present in Sequence Parameter Set "
      "('vui_parameters_present_flag' == 0b0).\n"
    );

  if (param.vui_parameters_present_flag) {
    if (_checkH264VuiParametersCompliance(handle, param) < 0)
      return -1;

    if (!param.frame_mbs_only_flag && !param.vui_parameters.pic_struct_present_flag)
      LIBBLU_H264_CK_BD_FAIL_RETURN(
        "'pic_struct' shall be present in Picture Timing SEI for interlaced video "
        "('frame_mbs_only_flag' == 0b0 in SPS, 'pic_struct_present_flag' == 0b0).\n"
      );
  }

  if (param.vui_parameters_present_flag) {
    LIBBLU_H264_DEBUG_SPS("  BD Target constraints:\n");

    bool max_BR_15_mbps = (
      BitRateH264HrdParameters(
        param.vui_parameters.nal_hrd_parameters,
        param.vui_parameters.nal_hrd_parameters.cpb_cnt_minus1
      ) <= 15000000
    );

    updateH264BDConstraints(handle, param.level_idc, &param.vui_parameters, max_BR_15_mbps);

    LIBBLU_H264_DEBUG_SPS(
      "   -> GOP maximal length "
      "(interval between two I pictures): %u pictures.\n",
      handle->bd_constraints.max_GOP_length
    );

    LIBBLU_H264_DEBUG_SPS(
      "   -> Minimum number of slices: %u slice(s).\n",
      handle->bd_constraints.min_nb_slices
    );

    LIBBLU_H264_DEBUG_SPS(
      "   -> Max number of consecutive B pictures: %u picture(s).\n",
      handle->bd_constraints.max_nb_consecutive_B_frames
    );
  }

  return 0;
}

int checkH264SpsBitstreamCompliance(
  LibbluESSettingsOptions options,
  H264SPSDataParameters param
)
{
  assert(param.vui_parameters_present_flag);

  HdmvFrameRateCode frame_rate_code = getHdmvFrameRateCode(
    param.vui_parameters.FrameRate
  );
  if (!frame_rate_code)
    LIBBLU_H264_CK_BD_FAIL_RETURN(
      "Frame-rate %.3f is not allowed.\n",
      param.vui_parameters.FrameRate
    );

  CheckVideoConfigurationRet check_ret = (
    (options.secondary)
    ? checkSecVideoConfiguration(
      param.FrameWidth, param.FrameHeight, frame_rate_code, !param.frame_mbs_only_flag
    )
    : checkPrimVideoConfiguration(
      param.FrameWidth, param.FrameHeight, frame_rate_code, !param.frame_mbs_only_flag
    )
  );

  switch (check_ret) {
  case CHK_VIDEO_CONF_RET_OK:
    break;

  case CHK_VIDEO_CONF_RET_ILL_VIDEO_SIZE:
    LIBBLU_H264_CK_BD_FAIL_RETURN(
      "Frame size %ux%u is not allowed.\n",
      param.FrameWidth,
      param.FrameHeight
    );
    break;

  case CHK_VIDEO_CONF_RET_ILL_FRAME_RATE:
    LIBBLU_H264_CK_BD_FAIL_RETURN(
      "Frame-rate %.3f is not allowed in combination with frame size %ux%u.\n",
      param.vui_parameters.FrameRate,
      param.FrameWidth,
      param.FrameHeight
    );
    break;

  case CHK_VIDEO_CONF_RET_ILL_DISP_MODE:
    LIBBLU_H264_CK_BD_FAIL_RETURN(
      "%s is not allowed in the combination "
      "with frame size %ux%u and frame-rate %.3f.\n",
      (param.frame_mbs_only_flag) ? "Progressive" : "Interlace",
      param.FrameWidth,
      param.FrameHeight,
      param.vui_parameters.FrameRate
    );
    break;
  }

  return 0;
}

static bool _constantH264ConstraintFlagsCheck(
  H264ContraintFlags first,
  H264ContraintFlags second
)
{
  return CHECK(
    EQUAL(.set0)
    EQUAL(.set1)
    EQUAL(.set2)
    EQUAL(.set3)
    EQUAL(.set4)
    EQUAL(.set5)
    EQUAL(.reservedFlags)
  );
}

static bool _constantH264HrdParametersCheck(
  H264HrdParameters first,
  H264HrdParameters second
)
{
  return CHECK(
    EQUAL(.cpb_cnt_minus1)
    EQUAL(.bit_rate_scale)
    EQUAL(.cpb_size_scale)
    EQUAL(.initial_cpb_removal_delay_length_minus1)
    EQUAL(.cpb_removal_delay_length_minus1)
    EQUAL(.dpb_output_delay_length_minus1)
    EQUAL(.time_offset_length)
    EXPR(
      0 == memcmp(
        first.SchedSel,
        second.SchedSel,
        (first.cpb_cnt_minus1 + 1) * sizeof(H264SchedSel)
      )
    )
  );
}

static bool _constantH264VuiColourDescriptionParametersCheck(
  H264VuiColourDescriptionParameters first,
  H264VuiColourDescriptionParameters second
)
{
  return CHECK(
    EQUAL(.colour_primaries)
    EQUAL(.transfer_characteristics)
    EQUAL(.matrix_coefficients)
  );
}

static bool _constantH264VuiVideoSeqBitstreamRestrictionsParametersCheck(
  H264VuiVideoSeqBsRestrParameters first,
  H264VuiVideoSeqBsRestrParameters second
)
{
  return CHECK(
    EQUAL(.motion_vectors_over_pic_boundaries_flag)
    EQUAL(.max_bytes_per_pic_denom)
    EQUAL(.max_bits_per_mb_denom)
    EQUAL(.log2_max_mv_length_horizontal)
    EQUAL(.log2_max_mv_length_vertical)
    EQUAL(.max_num_reorder_frames)
    EQUAL(.max_dec_frame_buffering)
  );
}

static bool _constantH264VuiParametersCheck(
  H264VuiParameters first,
  H264VuiParameters second
)
{
  return CHECK(
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
      _constantH264VuiColourDescriptionParametersCheck
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
      SUB_FUN(.nal_hrd_parameters, _constantH264HrdParametersCheck)
    END_COND
    EQUAL(.vcl_hrd_parameters_present_flag)
    START_COND(.vcl_hrd_parameters_present_flag, true)
      SUB_FUN(.vcl_hrd_parameters, _constantH264HrdParametersCheck)
    END_COND
    EQUAL(.low_delay_hrd_flag)
    EQUAL(.pic_struct_present_flag)
    EQUAL(.bitstream_restriction_flag)
    START_COND(.bitstream_restriction_flag, true)
      SUB_FUN(
        .bistream_restrictions,
        _constantH264VuiVideoSeqBitstreamRestrictionsParametersCheck
      )
    END_COND
  );
}

bool constantH264SequenceParametersSetCheck(
  H264SPSDataParameters first,
  H264SPSDataParameters second
)
{
  return CHECK(
    EQUAL(.profile_idc)
    SUB_FUN(.constraint_set_flags, _constantH264ConstraintFlagsCheck)
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
          first.num_ref_frames_in_pic_order_cnt_cycle *sizeof(int)
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
      SUB_FUN(.vui_parameters, _constantH264VuiParametersCheck)
    END_COND
  );
}

int checkH264SPSChangeBDCompliance(
  H264WarningFlags *warn,
  H264SPSDataParameters first,
  H264SPSDataParameters second
)
{
  bool constant_fields = true;

  char non_constant_fields[40 * 13] = {'\0'};
  char *ncf_ptr = non_constant_fields;

#define BOTH_TRUE(name)  (first name && second name)

#define MARK_NON_CONSTANT(fn)                                                 \
  do {                                                                        \
    lb_str_cat_comma(&ncf_ptr, fn, !constant_fields);                         \
    constant_fields = false;                                                  \
  } while (0)

  if (!CHECK(EQUAL(.profile_idc)))
    MARK_NON_CONSTANT("profile_idc");
  if (!CHECK(EQUAL(.level_idc)))
    MARK_NON_CONSTANT("level_idc");
  if (!CHECK(EQUAL(.pic_width_in_mbs_minus1)))
    MARK_NON_CONSTANT("pic_width_in_mbs_minus1");
  if (!CHECK(EQUAL(.pic_height_in_map_units_minus1)))
    MARK_NON_CONSTANT("pic_height_in_map_units_minus1");
  if (!CHECK(EQUAL(.frame_mbs_only_flag)))
    MARK_NON_CONSTANT("frame_mbs_only_flag");

  /* vui_parameters() */
  if (BOTH_TRUE(.vui_parameters_present_flag)) {
    if (BOTH_TRUE(.vui_parameters.aspect_ratio_info_present_flag)) {
      if (!CHECK(EQUAL(.vui_parameters.aspect_ratio_idc)))
        MARK_NON_CONSTANT("aspect_ratio_idc");
    }

    if (BOTH_TRUE(.vui_parameters.timing_info_present_flag)) {
      if (!CHECK(EQUAL(.vui_parameters.time_scale)))
        MARK_NON_CONSTANT("time_scale");
      if (!CHECK(EQUAL(.vui_parameters.num_units_in_tick)))
        MARK_NON_CONSTANT("num_units_in_tick");
    }

    if (BOTH_TRUE(.vui_parameters.nal_hrd_parameters_present_flag)) {
      if (!CHECK(EQUAL(.vui_parameters.nal_hrd_parameters.cpb_cnt_minus1)))
        MARK_NON_CONSTANT("cpb_cnt_minus1");
      if (!CHECK(EQUAL(.vui_parameters.nal_hrd_parameters.bit_rate_scale)))
        MARK_NON_CONSTANT("bit_rate_scale");
      if (!CHECK(EQUAL(.vui_parameters.nal_hrd_parameters.cpb_size_scale)))
        MARK_NON_CONSTANT("cpb_size_scale");

      if (constant_fields) {
        for (
          unsigned i = 0;
          constant_fields
          && i <= first.vui_parameters.nal_hrd_parameters.cpb_cnt_minus1;
          i++
        ) {
          if (!CHECK(EQUAL(.vui_parameters.nal_hrd_parameters.SchedSel[i].bit_rate_value_minus1)))
            MARK_NON_CONSTANT("bit_rate_value_minus1");
          if (!CHECK(EQUAL(.vui_parameters.nal_hrd_parameters.SchedSel[i].cpb_size_value_minus1)))
            MARK_NON_CONSTANT("cpb_size_value_minus1");
        }
      }
    }
  }

  if (!constant_fields)
    LIBBLU_H264_CK_BD_FAIL_WCOND_RETURN(
      LIBBLU_WARN_COUNT_CHECK_INC(warn, SPS_value_change),
      "The following values from the Sequence Parameter Set vary although "
      "they should remain constant: %s.\n",
      non_constant_fields
    );

  return 0;
}

int checkH264PicParametersSetCompliance(
  const H264ParametersHandlerPtr handle,
  H264PicParametersSetParameters param
)
{
  LIBBLU_H264_DEBUG_PPS(
    "  Picture Parameter Set id (pic_parameter_set_id): %u (0x%X).\n",
    param.pic_parameter_set_id,
    param.pic_parameter_set_id
  );

  if (H264_MAX_PPS <= param.pic_parameter_set_id)
    LIBBLU_H264_CK_ERROR_RETURN(
      "PPS pic_parameter_set_id overflow ('pic_parameter_set_id' == %u).\n",
      param.pic_parameter_set_id
    );

  if (handle->picture_parameter_set_valid[param.pic_parameter_set_id])
    LIBBLU_H264_CK_BD_FAIL_RETURN(
      "Unexpected duplicated Picture Parameter Set ID, "
      "ID shall be unique among other PPS in the same access unit "
      "('pic_parameter_set_id' == %u).\n",
      param.pic_parameter_set_id
    );

  LIBBLU_H264_DEBUG_PPS(
    "  Active Sequence Parameter Set id (seq_parameter_set_id): %u (0x%X).\n",
    param.seq_parameter_set_id,
    param.seq_parameter_set_id
  );

  /* Checking SPS id */ // TODO: Support more than one SPS
  if (0x00 != param.seq_parameter_set_id || !handle->sequence_parameter_set_GOP_valid)
    LIBBLU_H264_ERROR_RETURN(
      "The Picture Parameter Set references an invalid SPS or one "
      "that has not been communicated since the last IDR access unit "
      "('seq_parameter_set_id' == %u).\n",
      param.seq_parameter_set_id
    );

  LIBBLU_H264_DEBUG_PPS(
    "  Entropy coding mode (entropy_coding_mode_flag): %s (0b%X).\n",
    (param.entropy_coding_mode_flag) ? "CABAC" : "CAVLC",
    param.entropy_coding_mode_flag
  );

  bool validEntropyCodingMode = isRespectedH264EntropyCodingModeRestriction(
    handle->constraints.restrEntropyCodingMode,
    param.entropy_coding_mode_flag
  );

  if (!validEntropyCodingMode)
    LIBBLU_H264_CK_FAIL_RETURN(
      "%s entropy coding mode is not allowed for bitstream profile "
      "conformance, 'entropy_coding_mode_flag' shall be fixed to 0b%X.\n",
      (param.entropy_coding_mode_flag) ? "CABAC" : "CAVLC",
      !param.entropy_coding_mode_flag
    );

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
    LIBBLU_H264_CK_FAIL_RETURN(
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
    LIBBLU_H264_CK_FAIL_RETURN(
      "Weighted prediction mode is not allowed for bitstream "
      "profile conformance.\n"
    );

  LIBBLU_H264_DEBUG_PPS(
    "  Weighted prediction mode for B slices "
    "(weighted_bipred_idc): %s (0x%X).\n",
    H264WeightedBipredIdcStr(param.weighted_bipred_idc),
    param.weighted_bipred_idc
  );

  if (H264_WEIGHTED_PRED_B_SLICES_RESERVED <= param.weighted_bipred_idc) {
    LIBBLU_H264_CK_ERROR_RETURN(
      "Reserved PPS 'weighted_bipred_idc' == %u in use.\n",
      param.weighted_bipred_idc
    );
  }

  if (
    param.weighted_bipred_idc != H264_WEIGHTED_PRED_B_SLICES_DEFAULT
    && handle->constraints.forbiddenWeightedPredModesUse
  )
    LIBBLU_H264_CK_FAIL_RETURN(
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
    LIBBLU_H264_CK_ERROR_RETURN(
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
    LIBBLU_H264_CK_FAIL_RETURN(
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
    LIBBLU_H264_CK_FAIL_RETURN(
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
    LIBBLU_H264_CK_FAIL_RETURN(
      "Redundant pictures usage is not allowed for bitstream's "
      "profile conformance.\n"
    );

  LIBBLU_H264_DEBUG_PPS(
    "  Presence of Fidelity Range Extensions: %s.\n",
    BOOL_INFO(param.picParamExtensionPresent)
  );

  if (param.picParamExtensionPresent) {
    if (handle->constraints.forbiddenPPSExtensionParameters)
      LIBBLU_H264_CK_FAIL_RETURN(
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
      for (unsigned i = 0; i < param.nbScalingMatrix; i++) {
        LIBBLU_H264_DEBUG_PPS(
          "    -> Picture scaling list %u "
          "(pic_scaling_list_present_flag[%u]): %s (0b%X).\n",
          i, i,
          BOOL_PRESENCE(param.pic_scaling_list_present_flag[i]),
          param.pic_scaling_list_present_flag[i]
        );

        if (param.pic_scaling_list_present_flag[i]) {
          bool is_mat_4x4 = (i < 6);
          bool use_default_scaling_mat = (
            (is_mat_4x4) ?
              param.UseDefaultScalingMatrix4x4Flag[i]
            :
              param.UseDefaultScalingMatrix8x8Flag[i-6]
          );

          LIBBLU_H264_DEBUG_PPS(
            "     -> Use default scaling matrix: %s (0b%X).\n",
            BOOL_INFO(use_default_scaling_mat),
            use_default_scaling_mat
          );

          unsigned nb_coeffs = ((is_mat_4x4) ? 16u : 64u);
          for (unsigned j = 0; j < nb_coeffs; j++)
            LIBBLU_H264_DEBUG_PPS(
              "     -> Scaling list %s %u, index %u: %" PRIu8 ".\n",
              (is_mat_4x4) ? "4x4" : "8x8", i, j,
              (is_mat_4x4) ?
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
      LIBBLU_H264_CK_FAIL_RETURN(
        "Reserved 'second_chroma_qp_index_offset' == %d in use.\n",
        param.second_chroma_qp_index_offset
      );
  }

  return 0; /* OK */
}

int checkH264PicParametersSetChangeCompliance(
  const H264ParametersHandlerPtr handle,
  H264PicParametersSetParameters first,
  H264PicParametersSetParameters second
)
{
  LIBBLU_H264_DEBUG_PPS(
    "  ### Change detected in PPS parameters, check changes compliance.\n"
  );

  /* Check second parameters compliance : */
  if (checkH264PicParametersSetCompliance(handle, second) < 0)
    return -1;

  /* Check second parameters mandatory parameters changeless : */
  if (first.entropy_coding_mode_flag != second.entropy_coding_mode_flag)
    LIBBLU_H264_CK_BD_FAIL_WCOND_RETURN(
      LIBBLU_WARN_COUNT_CHECK_INC(&handle->warning_flags, PPS_entropy_coding_mode_flag_change),
      "Entropy coding mode ('entropy_coding_mode_flag') in Picture Parameter "
      "Set shall remain the same.\n"
    );

  return 0; /* OK */
}

int checkH264SeiBufferingPeriodCompliance(
  const H264ParametersHandlerPtr handle,
  H264SeiBufferingPeriod param
)
{
  assert(NULL != handle);

  LIBBLU_H264_DEBUG_SEI(
    "   Linked sequence parameter set (seq_parameter_set_id): %u (0x%X).\n",
    param.seq_parameter_set_id,
    param.seq_parameter_set_id
  );

  /* Checking SPS id (already done at parsing) */ // TODO: Support more than one SPS
  if (0x00 != param.seq_parameter_set_id || !handle->sequence_parameter_set_GOP_valid)
    LIBBLU_H264_CK_ERROR_RETURN(
      "The Buffering Period SEI message references an invalid SPS or one "
      "that has not been communicated since the last IDR access unit "
      "('seq_parameter_set_id' == %u).\n",
      param.seq_parameter_set_id
    );
  const H264SPSDataParameters *sps = &handle->sequence_parameter_set.data;

  if (!sps->NalHrdBpPresentFlag && !sps->VclHrdBpPresentFlag)
    LIBBLU_H264_CK_FAIL_RETURN(
      "HRD parameters are not present in Sequence Parameter Set, "
      "Buffering Period SEI message shall not be present "
      "('NalHrdBpPresentFlag' == 0b0 and 'VclHrdBpPresentFlag' == 0b0).\n"
    );

  for (int type = 0; type < 2; type++) {
    if (!((type) ? sps->NalHrdBpPresentFlag : sps->VclHrdBpPresentFlag))
      continue;
    const H264HrdParameters *vui_HRD_param = (
      (type)
      ? &sps->vui_parameters.vcl_hrd_parameters
      : &sps->vui_parameters.nal_hrd_parameters
    );

    const H264SeiBufferingPeriodSchedSel *HRD_param = (
      (type)
      ? param.vcl_hrd_parameters
      : param.nal_hrd_parameters
    );

    LIBBLU_H264_DEBUG_SEI(
      "   %s HRD - Coded Picture Buffer Parameters:\n",
      (type) ? "VCL" : "NAL"
    );

    for (
      unsigned SchedSelIdx = 0;
      SchedSelIdx <= vui_HRD_param->cpb_cnt_minus1;
      SchedSelIdx++
    ) {
      LIBBLU_H264_DEBUG_SEI("    - SchedSelIdx %u:\n", SchedSelIdx);

      LIBBLU_H264_DEBUG_SEI(
        "     -> Initial CPB removal delay "
        "(initial_cpb_removal_delay[%u]): "
        "%" PRIu32 " (%" PRIu32 " ms, 0x%" PRIx32 ").\n",
        SchedSelIdx,
        HRD_param[SchedSelIdx].initial_cpb_removal_delay,
        HRD_param[SchedSelIdx].initial_cpb_removal_delay / 90,
        HRD_param[SchedSelIdx].initial_cpb_removal_delay
      );

      if (!HRD_param[SchedSelIdx].initial_cpb_removal_delay)
        LIBBLU_H264_CK_FAIL_WCOND_RETURN(
          LIBBLU_WARN_COUNT_CHECK_INC(
            &handle->warning_flags,
            SEI_buf_period_initial_cpb_removal_delay_is_zero
          ),
          "Forbidden zero initial CPB removal delay "
          "(initial_cpb_removal_delay).\n"
        );

      double max_HRD_delay = (
        90000.0
        * vui_HRD_param->SchedSel[SchedSelIdx].CpbSize
        / vui_HRD_param->SchedSel[SchedSelIdx].BitRate
      );

      LIBBLU_H264_DEBUG_SEI(
        "      -> Max allowed delay: %.0f ms.\n",
        max_HRD_delay
      );

      if (max_HRD_delay < HRD_param[SchedSelIdx].initial_cpb_removal_delay / 90.0)
        LIBBLU_H264_CK_FAIL_WCOND_RETURN(
          LIBBLU_WARN_COUNT_CHECK_INC(
            &handle->warning_flags,
            SEI_buf_period_initial_cpb_removal_delay_exceeds
          ),
          "NAL HRD CPB Removal delay exceed maximal delay (%.2f < %.2f).\n",
          max_HRD_delay,
          HRD_param[SchedSelIdx].initial_cpb_removal_delay / 90.0
        );

      LIBBLU_H264_DEBUG_SEI(
        "     -> Initial CPB removal delay offset "
        "(initial_cpb_removal_delay_offset[%u]): "
        "%" PRIu32 " (%" PRIu32 " ms, 0x%" PRIx32 ").\n",
        SchedSelIdx,
        HRD_param[SchedSelIdx].initial_cpb_removal_delay_offset,
        HRD_param[SchedSelIdx].initial_cpb_removal_delay_offset / 90,
        HRD_param[SchedSelIdx].initial_cpb_removal_delay_offset
      );
    }
  }

  return 0;
}

bool constantH264SeiBufferingPeriodCheck(
  const H264ParametersHandlerPtr handle,
  H264SeiBufferingPeriod first,
  H264SeiBufferingPeriod second
)
{
  assert(NULL != handle);
  assert(
    handle->sequence_parameter_set_present
    && handle->sequence_parameter_set.data.vui_parameters_present_flag
  );

  const H264VuiParameters *VUI_param =
    &handle->sequence_parameter_set.data.vui_parameters
  ;

  if (VUI_param->nal_hrd_parameters_present_flag) {
    int ret = memcmp(
      first.nal_hrd_parameters,
      second.nal_hrd_parameters,
      (VUI_param->nal_hrd_parameters.cpb_cnt_minus1 + 1)
        * sizeof(H264SeiBufferingPeriodSchedSel)
    );
    if (ret != 0)
      return false;
  }

  if (VUI_param->vcl_hrd_parameters_present_flag) {
    int ret = memcmp(
      first.vcl_hrd_parameters,
      second.vcl_hrd_parameters,
      (VUI_param->vcl_hrd_parameters.cpb_cnt_minus1 + 1)
        * sizeof(H264SeiBufferingPeriodSchedSel)
    );
    if (ret != 0)
      return false;
  }

  return true;
}

int checkH264SeiBufferingPeriodChangeCompliance(
  const H264ParametersHandlerPtr handle,
  H264SeiBufferingPeriod first,
  H264SeiBufferingPeriod second
)
{
  unsigned SchedSelIdx;

  H264VuiParameters *vuiParam;

  LIBBLU_H264_DEBUG_SEI(
    "  ### Change detected in Buffering Period SEI message parameters, "
    "check changes compliance.\n"
  );

  /* Check second parameters compliance : */
  if (checkH264SeiBufferingPeriodCompliance(handle, second) < 0)
    return -1;

  vuiParam = &handle->sequence_parameter_set.data.vui_parameters;

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
        LIBBLU_H264_CK_ERROR_RETURN(
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
        LIBBLU_H264_CK_ERROR_RETURN(
          "Fluctuating VCL HRD initial CPB removal delay "
          "(initial_cpb_removal_delay/initial_cpb_removal_delay_offset).\n"
        );
    }
  }

  return 0; /* OK */
}

int checkH264SeiPicTimingCompliance(
  const H264ParametersHandlerPtr handle,
  H264SeiPicTiming param
)
{
  unsigned i;

  H264VuiParameters *vuiParam;

  assert(NULL != handle);

  if (
    !handle->sequence_parameter_set_present
    || !handle->sequence_parameter_set.data.vui_parameters_present_flag
  )
    LIBBLU_H264_CK_ERROR_RETURN(
      "Missing mandatory VUI parameters, Pic Timing SEI message "
      "shouldn't be present.\n"
    );

  vuiParam = &handle->sequence_parameter_set.data.vui_parameters;

  if (!vuiParam->CpbDpbDelaysPresentFlag && !vuiParam->pic_struct_present_flag)
    LIBBLU_H264_CK_ERROR_RETURN(
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
      H264PicStructValueStr(param.pic_struct),
      param.pic_struct
    );

    if (H264_PIC_STRUCT_TRIPLED_FRAME < param.pic_struct)
      LIBBLU_H264_CK_ERROR_RETURN(
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
          H264CtTypeValueStr(param.clock_timestamp[i].ct_type),
          param.clock_timestamp[i].ct_type
        );

        if (H264_CT_TYPE_RESERVED <= param.clock_timestamp[i].ct_type)
          LIBBLU_H264_CK_ERROR_RETURN(
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
          H264CountingTypeValueStr(param.clock_timestamp[i].counting_type),
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
            LIBBLU_H264_CK_ERROR_RETURN(
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

int checkH264SeiRecoveryPointCompliance(
  const H264ParametersHandlerPtr handle,
  H264SeiRecoveryPoint param
)
{
  if (!handle->sequence_parameter_set_present)
    LIBBLU_H264_CK_ERROR_RETURN(
      "Missing mandatory SPS NALU, Recovery Point SEI message "
      "shouldn't be present."
    );

  if (!handle->nb_picture_parameter_set_present)
    LIBBLU_H264_CK_ERROR_RETURN(
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
    handle->sequence_parameter_set.data.MaxFrameNum
  );

  if (handle->sequence_parameter_set.data.MaxFrameNum <= param.recovery_frame_cnt)
    LIBBLU_H264_CK_ERROR_RETURN(
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
    LIBBLU_H264_CK_ERROR_RETURN(
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

    LIBBLU_H264_CK_ERROR_RETURN(
      "Reserved 'changing_slice_group_idc' == %" PRIu8 " in use.\n",
      param.changing_slice_group_idc
    );
  }

  if (0x0 != param.changing_slice_group_idc) {
    if (!handle->constraints.maxAllowedNumSliceGroups) {
      LIBBLU_H264_CK_ERROR_RETURN(
        "'changing_slice_group_idc' == %" PRIu8 " should not indicate "
        "presence of multiple slice groups.\n",
        param.changing_slice_group_idc
      );
    }
    else {
      /* TODO */
      LIBBLU_TODO_MSG("Multiple slice groups are not supported.\n");
    }

    return -1;
  }

  return 0;
}

int checkH264SeiRecoveryPointChangeCompliance(
  const H264ParametersHandlerPtr handle,
  H264SeiRecoveryPoint second
)
{
  LIBBLU_H264_DEBUG_SEI(
    "  ### Change detected in Recovery point SEI message parameters, "
    "check changes compliance.\n"
  );

  /* Check second parameters compliance : */
  if (checkH264SeiRecoveryPointCompliance(handle, second) < 0)
    return -1;

  return 0; /* OK */
}

static int _checkH264RefPicListModificationCompliance(
  H264RefPicListModification param,
  H264SliceHeaderParameters sliceHeaderParam
)
{
  unsigned i;

  if (
    !is_I_H264SliceTypeValue(sliceHeaderParam.slice_type)
    && !is_SI_H264SliceTypeValue(sliceHeaderParam.slice_type)
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
          H264ModificationOfPicNumsIdcValueStr(
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

  if (is_B_H264SliceTypeValue(sliceHeaderParam.slice_type)) {
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
          H264ModificationOfPicNumsIdcValueStr(
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

static int _checkH264PredWeightTableCompliance(
  H264PredWeightTable param
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
    LIBBLU_H264_CK_ERROR_RETURN(
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
    LIBBLU_H264_CK_ERROR_RETURN(
      "Out of range 'chroma_log2_weight_denom' == %u in use.\n",
      param.chroma_log2_weight_denom
    );

  LIBBLU_H264_DEBUG_SLICE(
    "   Other parameters not checked.\n"
  );

  return 0;
}

static int _checkH264DecRefPicMarkingCompliance(
  H264ParametersHandlerPtr handle,
  H264DecRefPicMarking param
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

    if (param.no_output_of_prior_pics_flag)
      LIBBLU_H264_CK_BD_FAIL_RETURN(
        "Field 'no_output_of_prior_pics_flag' in IDR pictures shall be set "
        "to 0b0.\n"
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
      H264MemoryManagementControlOperationValue operation;
      unsigned i;

      assert(0 < param.nbMemMngmntCtrlOp); // At least 'End of list' op.

      operation = H264_MEM_MGMNT_CTRL_OP_END;
        // Suppress compiler non-initalization warning.

      for (i = 0; i < param.nbMemMngmntCtrlOp; i++) {
        const H264MemMngmntCtrlOpBlk *opBlk = &param.MemMngmntCtrlOp[i];

        LIBBLU_H264_DEBUG_SLICE("    - Instruction %u:\n");

        operation = opBlk->operation;
        LIBBLU_H264_DEBUG_SLICE(
          "     Operation (memory_management_control_operation): %s (0x%X).\n",
          H264MemoryManagementControlOperationValueStr(operation),
          operation
        );

        if (
          H264_MEM_MGMNT_CTRL_OP_SHORT_TERM_UNUSED == operation
          || H264_MEM_MGMNT_CTRL_OP_SHORT_TERM_USED_LONG_TERM == operation
        ) {
          LIBBLU_H264_DEBUG_SLICE(
            "      -> Difference of pictures "
            "(difference_of_pic_nums_minus1): %u (0x%X).\n",
            opBlk->difference_of_pic_nums_minus1 + 1,
            opBlk->difference_of_pic_nums_minus1
          );
        }

        if (H264_MEM_MGMNT_CTRL_OP_LONG_TERM_UNUSED == operation) {
          LIBBLU_H264_DEBUG_SLICE(
            "      -> Long-term picture number "
            "(long_term_pic_num): %u (0x%X).\n",
            opBlk->long_term_pic_num,
            opBlk->long_term_pic_num
          );
        }

        if (
          H264_MEM_MGMNT_CTRL_OP_SHORT_TERM_USED_LONG_TERM == operation
          || H264_MEM_MGMNT_CTRL_OP_USED_LONG_TERM == operation
        ) {
          LIBBLU_H264_DEBUG_SLICE(
            "      -> Long-term frame index "
            "(long_term_frame_idx): %u (0x%X).\n",
            opBlk->long_term_frame_idx,
            opBlk->long_term_frame_idx
          );
        }

        if (H264_MEM_MGMNT_CTRL_OP_MAX_LONG_TERM == operation) {
          LIBBLU_H264_DEBUG_SLICE(
            "      -> Maximum long-term frame index "
            "(max_long_term_frame_idx_plus1): %u (0x%X).\n",
            opBlk->max_long_term_frame_idx_plus1 - 1,
            opBlk->max_long_term_frame_idx_plus1
          );
        }
      }

      if (H264_MEM_MGMNT_CTRL_OP_END != operation)
        LIBBLU_H264_CK_WCOND_WARNING(
          LIBBLU_WARN_COUNT_CHECK_INC(
            &handle->warning_flags,
            slice_too_many_memory_management_control_operation
          ),
          "More 'memory_management_control_operation' fields present in slice "
          "header than supported, HRD Verifier operations might be "
          "incorrect.\n"
        );
    }
  }

  return 0;
}

static int _checkH264SliceHeaderCompliance(
  const H264ParametersHandlerPtr handle,
  H264SliceHeaderParameters param,
  bool is_first_VCL_NALU
)
{
  assert(NULL != handle);
  assert(handle->access_unit_delimiter_present);
  assert(handle->sequence_parameter_set_present);
  assert(handle->sequence_parameter_set.data.vui_parameters_present_flag);
  assert(0 < handle->nb_picture_parameter_set_present);

  const H264SPSDataParameters *sps = &handle->sequence_parameter_set.data;
  H264CurrentProgressParam *cur = &handle->cur_prog_param;

  if (handle->constraints.idrPicturesOnly && !param.is_IDR_pic)
    LIBBLU_H264_CK_FAIL_RETURN(
      "Presence of non-IDR picture is not allowed for "
      "bitstream's profile conformance.\n"
    );

  LIBBLU_H264_DEBUG_SLICE(
    "   Address of the first macroblock in slice "
    "(first_mb_in_slice): %" PRIu32 " (0x%" PRIx32 ").\n",
    param.first_mb_in_slice,
    param.first_mb_in_slice
  );

  unsigned last_mb_in_slice = (param.PicSizeInMbs >> param.MbaffFrameFlag) - 1u;
  if (last_mb_in_slice < param.first_mb_in_slice)
    LIBBLU_H264_CK_ERROR_RETURN(
      "First macroblock address in slice is over %u "
      "('first_mb_in_slice' == %" PRIu32 ", "
      "'PicSizeInMbs' == %u, 'MbaffFrameFlag' == 0b%x).\n",
      last_mb_in_slice,
      param.first_mb_in_slice,
      param.PicSizeInMbs,
      param.MbaffFrameFlag
    );

  if (is_first_VCL_NALU) {
    /* First VCL NAL unit of a primary coded picture */
    cur->nb_pictures++;
    cur->nb_slices_in_picture = 1;

    /* Check presence of both fields (or a complete frame) before completing AU. */
    bool complete_frame;
    if (param.field_pic_flag) {
      /* Field */
      if (param.bottom_field_flag) {
        /* Bottom field */
        if (cur->au_content.bottom_field_present)
          LIBBLU_H264_CK_BD_FAIL_RETURN(
            "Unexpected presence of two consecutive bottom fields, "
            "non-paired fields are not allowed and fields shall be coded as "
            "field of complementary field pair.\n"
          );
        cur->au_content.bottom_field_present = true;
      }
      else {
        /* Top field */
        if (cur->au_content.top_field_present)
          LIBBLU_H264_CK_BD_FAIL_RETURN(
            "Unexpected presence of two consecutive top fields, "
            "non-paired fields are not allowed and fields shall be coded as "
            "field of complementary field pair.\n"
          );
        cur->au_content.top_field_present = true;
      }

      complete_frame = (
        cur->au_content.bottom_field_present
        && cur->au_content.top_field_present
      ); // Complementary field pair
    }
    else {
      if (cur->au_content.bottom_field_present)
        LIBBLU_H264_CK_BD_FAIL_RETURN(
          "Unexpected presence of a non-paired bottom field, "
          "non-paired fields are not allowed and fields shall be coded as "
          "field of complementary field pair.\n"
        );
      if (cur->au_content.top_field_present)
        LIBBLU_H264_CK_BD_FAIL_RETURN(
          "Unexpected presence of a non-paired top field, "
          "non-paired fields are not allowed and fields shall be coded as "
          "field of complementary field pair.\n"
        );

      cur->au_content.frame_present = true;
      complete_frame = true;
    }

    if (complete_frame) {
      cur->nb_frames++;
      cur->nb_GOP_frames++;

      if (is_I_H264SliceTypeValue(param.slice_type))
        cur->nb_GOP_frames = 1;

      if (is_B_H264SliceTypeValue(param.slice_type))
        cur->nb_consecutive_B_frames++;
      else
        cur->nb_consecutive_B_frames = 0;
    }

    if (!handle->sequence_parameter_set_GOP_valid)
      LIBBLU_H264_CK_ERROR_RETURN(
        "Missing a valid SPS NALU before slice header.\n"
      );
  }
  else
    cur->nb_slices_in_picture++;

  LIBBLU_H264_DEBUG_SLICE(
    "   Slice type (slice_type): %s (0x%X).\n",
    H264SliceTypeValueStr(param.slice_type),
    param.slice_type
  );

  if (H264_SLICE_TYPE_SI < param.slice_type)
    LIBBLU_H264_CK_ERROR_RETURN(
      "Reserved 'slice_type' == %u in use.\n",
      param.slice_type
    );

  if (param.is_IDR_pic && !is_I_H264SliceTypeValue(param.slice_type))
    LIBBLU_H264_CK_ERROR_RETURN(
      "Non I-slice type value 'slice_type' == %u in use in an IDR picture "
      "('slice_type' == %u).\n",
      param.slice_type
    );

  if (0 == sps->max_num_ref_frames && !is_I_H264SliceTypeValue(param.slice_type))
    LIBBLU_H264_CK_ERROR_RETURN(
      "Unexpected non I-slice type whereas the maximum number of reference "
      "pictures in Sequence Parameter Set is set to 0 "
      "('max_num_ref_frames' == 0b0, 'slice_type' == %u).\n",
      param.slice_type
    );

  // Check if slice type is allowed according to Profile/Level constraints
  const H264PrimaryPictureType constraints_allowed_slice_types =
    handle->constraints.allowed_slice_types
  ;

  if (!isAllowedH264SliceTypeValue(param.slice_type, constraints_allowed_slice_types))
    LIBBLU_H264_CK_FAIL_RETURN(
      "Unallowed 'slice_type' == %u in use for bitstream profile "
      "conformance.\n",
      param.slice_type
    );

  /* Check if slice type is allowed according AUD primary_pic_type */
  const H264PrimaryPictureType AUD_allowed_slice_types =
    handle->access_unit_delimiter.primary_pic_type
  ;

  if (!isAllowedH264SliceTypeValue(param.slice_type, AUD_allowed_slice_types))
    LIBBLU_H264_CK_ERROR_RETURN(
      "Mismatch between slice type and Access Unit Delimiter indicator "
      "('primary_pic_type' == %u while picture 'slice_type' == %u).\n",
      handle->access_unit_delimiter.primary_pic_type,
      param.slice_type
    );

  if (!isBDAllowedH264SliceTypeValue(param.slice_type, AUD_allowed_slice_types))
    LIBBLU_H264_CK_BD_FAIL_WCOND_RETURN(
      LIBBLU_WARN_COUNT_CHECK_INC(&handle->warning_flags, slice_wrong_slice_type),
      "Mismatch between slice type and Access Unit Delimiter indicator "
      "('primary_pic_type' == %u while picture 'slice_type' == %u).\n",
      handle->access_unit_delimiter.primary_pic_type,
      param.slice_type
    );

  LIBBLU_H264_DEBUG_SLICE(
    "   Active Picture Parameter Set id "
    "(pic_parameter_set_id): %" PRIu8 " (0x%02" PRIX8 ").\n",
    param.pic_parameter_set_id,
    param.pic_parameter_set_id
  );

  /* Checking PPS id (already done in slice header parsing) */
  if (!handle->picture_parameter_set_GOP_valid[param.pic_parameter_set_id])
    LIBBLU_H264_ERROR_RETURN(
      "The slice header references an invalid PPS or one that has not been "
      "communicated since the last IDR access unit "
      "('pic_parameter_set_id' == %u).\n",
      param.pic_parameter_set_id
    );
  const H264PicParametersSetParameters *pps = handle->picture_parameter_set[
    param.pic_parameter_set_id
  ];

  if (sps->separate_colour_plane_flag) {
    LIBBLU_H264_DEBUG_SLICE(
      "   Colour plane (colour_plane_id): %s (0x%X).\n",
      H264ColourPlaneIdValueStr(param.colour_plane_id),
      param.colour_plane_id
    );

    if (H264_COLOUR_PLANE_ID_CR < param.colour_plane_id)
      LIBBLU_H264_CK_ERROR_RETURN(
        "Reserved 'colour_plane_id' == %u in use.\n",
        param.colour_plane_id
      );
  }

  LIBBLU_H264_DEBUG_SLICE(
    "   Frame number (frame_num): %" PRIu16 " (0x%04" PRIX8 ").\n",
    param.frame_num,
    param.frame_num
  );

  LIBBLU_H264_DEBUG_SLICE(
    "    => PrevRefFrameNum (Previous reference frame number): %u.\n",
    param.PrevRefFrameNum
  );

  if (sps->MaxFrameNum <= param.frame_num)
    LIBBLU_H264_CK_ERROR_RETURN(
      "Unexpected too high frame number 'frame_num' == %" PRIu16 ", "
      "according to SPS, this value shall not exceed %u.\n",
      param.frame_num,
      sps->MaxFrameNum
    );

  if (is_first_VCL_NALU) {
    if (param.is_IDR_pic) {
      /* IDR picture */
      LIBBLU_H264_DEBUG_SLICE("    -> IDR Picture: 'frame_num' shall be 0.\n");

      if (0 != param.frame_num)
        LIBBLU_H264_CK_FAIL_RETURN(
          "Broken picture number, unexpected value 'frame_num' == %u for "
          "an IDR picture, shall be equal to 0.\n"
        );
    }
    else {
      /* Non-IDR picture */
      if (param.PrevRefFrameNum == param.frame_num) {
        LIBBLU_H264_DEBUG_SLICE(
          "    -> Non-IDR Picture: 'frame_num' equal to 'PrevRefFrameNum'.\n"
        );

        // TODO
      }
    }
  }

  // if (cur->gaps_in_frame_num) {
  //   if (!sps->gaps_in_frame_num_value_allowed_flag) {
  //     /* Unintentional lost of pictures. */
  //     LIBBLU_H264_CK_ERROR_RETURN("Unexpected gaps in 'frame_num' values.\n");
  //   }
  //   else {
  //     /* TODO: Inform presence of gaps in frame_num. */
  //     cur->gaps_in_frame_num = false; /* Reset */
  //   }
  // }

  if (param.is_IDR_pic) {
    /* IDR picture */
    if (param.frame_num != 0x0) {
      LIBBLU_H264_CK_ERROR_RETURN(
        "Unexpected 'frame_num' == %" PRIu16 " for IDR picture, "
        "shall be equal to 0.\n",
        param.frame_num
      );
    }
  }

  if (!sps->frame_mbs_only_flag) {
    LIBBLU_H264_DEBUG_SLICE(
      "   Not macroblocks-only frame linked parameters "
      "('frame_mbs_only_flag' in SPS == 0b0):\n"
    );

    LIBBLU_H264_DEBUG_SLICE(
      "    -> Field picture (field_pic_flag): %s (0b%X).\n",
      BOOL_INFO(param.field_pic_flag),
      param.field_pic_flag
    );

    if (param.field_pic_flag) {
      LIBBLU_H264_DEBUG_SLICE(
        "    -> Field type (bottom_field_flag): %s (0b%X).\n",
        (param.bottom_field_flag) ? "Bottom field" : "Top field",
        param.bottom_field_flag
      );
    }
  }
  else {
    LIBBLU_H264_DEBUG_SLICE(
      "   Macroblocks-only frame linked parameters "
      "('frame_mbs_only_flag' in SPS == 0b1):\n"
    );
  }

  LIBBLU_H264_DEBUG_SLICE(
    "    => MbaffFrameFlag "
    "(Macroblock-Adaptative Frame-Field Coding): %s (0b%d).\n",
    BOOL_STR(param.MbaffFrameFlag),
    param.MbaffFrameFlag
  );
  LIBBLU_H264_DEBUG_SLICE(
    "    => PicHeightInMbs "
    "(Picture height in macroblocks): %u MBs.\n",
    param.PicHeightInMbs
  );
  LIBBLU_H264_DEBUG_SLICE(
    "    => PicHeightInSamplesL "
    "(Picture height in luma samples): %u samples.\n",
    param.PicHeightInSamplesL
  );
  LIBBLU_H264_DEBUG_SLICE(
    "    => PicHeightInSamplesC "
    "(Picture height in chroma samples): %u samples.\n",
    param.PicHeightInSamplesC
  );

  if (param.is_IDR_pic) {
    LIBBLU_H264_DEBUG_SLICE(
      "   IDR picture linked parameters (IdrPicFlag == true):\n"
    );

    LIBBLU_H264_DEBUG_SLICE(
      "    -> IDR picture identifier "
      "(idr_pic_id): %" PRIu16 " (0x%04" PRIX8 ").\n",
      param.idr_pic_id,
      param.idr_pic_id
    );
  }

  if (0x0 == sps->pic_order_cnt_type) {
    LIBBLU_H264_DEBUG_SLICE(
      "   Picture order count type linked parameters "
      "(pic_order_cnt_type == 0):\n"
    );

    LIBBLU_H264_DEBUG_SLICE(
      "    -> LSB value "
      "(pic_order_cnt_lsb): %" PRIu16 " (0x%04" PRIX8 ").\n",
      param.pic_order_cnt_lsb,
      param.pic_order_cnt_lsb
    );

    if (pps->bottom_field_pic_order_in_frame_present_flag && !param.field_pic_flag) {
      LIBBLU_H264_DEBUG_SLICE(
        "    -> Value difference between bottom and top fields "
        "(delta_pic_order_cnt_bottom): %" PRIu16 " (0x%04" PRIX8 ").\n",
        param.delta_pic_order_cnt_bottom,
        param.delta_pic_order_cnt_bottom
      );
    }
  }

  if (0x1 == sps->pic_order_cnt_type && !sps->delta_pic_order_always_zero_flag) {
    LIBBLU_H264_DEBUG_SLICE(
      "   Picture order count type linked parameters "
      "(pic_order_cnt_type == 1):\n"
    );

    LIBBLU_H264_DEBUG_SLICE(
      "    -> %s order count difference from expected order count "
      "(delta_pic_order_cnt[0]): %" PRIu16 " (0x%04" PRIX8 ").\n",
      (pps->bottom_field_pic_order_in_frame_present_flag) ?
        "Top field"
      :
        "Coded field",
      param.delta_pic_order_cnt[0],
      param.delta_pic_order_cnt[0]
    );

    if (
      pps->bottom_field_pic_order_in_frame_present_flag
      && !param.field_pic_flag
    ) {
      LIBBLU_H264_DEBUG_SLICE(
        "    -> Bottom field order count difference from expected order count "
        "(delta_pic_order_cnt[0]): %" PRIu16 " (0x%04" PRIX8 ").\n",
        param.delta_pic_order_cnt[1],
        param.delta_pic_order_cnt[1]
      );
    }
  }

  if (pps->redundant_pic_cnt_present_flag) {
    LIBBLU_H264_DEBUG_SLICE(
      "   Redundant picture count linked parameters "
      "(redundant_pic_cnt_present_flag == 0b1):\n"
    );

    LIBBLU_H264_DEBUG_SLICE(
      "    -> Redundant picture counter value "
      "(redundant_pic_cnt): %" PRIu8 " (0x%02" PRIX8 ").\n",
      param.redundant_pic_cnt,
      param.redundant_pic_cnt
    );
  }

  if (is_B_H264SliceTypeValue(param.slice_type)) {
    LIBBLU_H264_DEBUG_SLICE(
      "   Inter prediction decoding process "
      "(direct_spatial_mv_pred_flag): %s (0b%X).\n",
      (param.direct_spatial_mv_pred_flag) ?
        "Spatial direct prediction mode"
      :
        "Temporal direct prediction mode",
      param.direct_spatial_mv_pred_flag
    );
  }

  if (
    is_P_H264SliceTypeValue(param.slice_type)
    || is_SP_H264SliceTypeValue(param.slice_type)
    || is_B_H264SliceTypeValue(param.slice_type)
  ) {
    LIBBLU_H264_DEBUG_SLICE(
      "   Override default number of reference indexes "
      "(num_ref_idx_active_override_flag): %s (0b%X).\n",
      BOOL_INFO(param.num_ref_idx_active_override_flag),
      param.num_ref_idx_active_override_flag
    );

    /* Check mandatory cases where num_ref_idx_active_override_flag shall be equal to 0b1: */
    if (
      !param.field_pic_flag
      && 15 < pps->num_ref_idx_l0_default_active_minus1
      && !param.num_ref_idx_active_override_flag
    )
      LIBBLU_H264_CK_ERROR_RETURN(
        "'num_ref_idx_active_override_flag' shall be equal to 0b1.\n"
      );

    if (
      is_B_H264SliceTypeValue(param.slice_type)
      && !param.field_pic_flag
      && 15 < pps->num_ref_idx_l1_default_active_minus1
      && !param.num_ref_idx_active_override_flag
    )
      LIBBLU_H264_CK_ERROR_RETURN(
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
        "    -> Number of reference index for reference picture list 0 "
        "(num_ref_idx_l0_active_minus1): %u (0x%X).\n",
        param.num_ref_idx_l0_active_minus1 + 1,
        param.num_ref_idx_l0_active_minus1
      );

      /* Check value range: */
      if (maxRefIdx < param.num_ref_idx_l0_active_minus1)
        LIBBLU_H264_CK_ERROR_RETURN(
          "Too high 'num_ref_idx_l0_active_minus1' == %u in use.\n",
          param.num_ref_idx_l0_active_minus1
        );

      if (is_B_H264SliceTypeValue(param.slice_type)) {
        LIBBLU_H264_DEBUG_SLICE(
          "    -> Number of reference index for reference picture list 1 "
          "(num_ref_idx_l1_active_minus1): %u (0x%X).\n",
          param.num_ref_idx_l1_active_minus1 + 1,
          param.num_ref_idx_l1_active_minus1
        );

        /* Check value range: */
        if (maxRefIdx < param.num_ref_idx_l1_active_minus1)
          LIBBLU_H264_CK_ERROR_RETURN(
            "Too high 'num_ref_idx_l1_active_minus1' == %u in use.\n",
            param.num_ref_idx_l1_active_minus1
          );
      }
    }
  }

  if (param.is_coded_slice_ext) {
    /* ref_pic_list_mvc_modification() */
    /* Annex H */

    /* TODO: Add 3D MVC support. */
    LIBBLU_H264_CK_ERROR_RETURN("Unsupported 3D MVC format.\n");
  }
  else
    if (_checkH264RefPicListModificationCompliance(param.refPicListMod, param) < 0)
      return -1;

  if (
    (
      pps->weighted_pred_flag && (
        is_P_H264SliceTypeValue(param.slice_type)
        || is_SP_H264SliceTypeValue(param.slice_type)
      )
    )
    || (
      pps->weighted_bipred_idc == H264_WEIGHTED_PRED_B_SLICES_EXPLICIT &&
      is_B_H264SliceTypeValue(param.slice_type)
    )
  ) {
    /* pred_weight_table() */
    if (_checkH264PredWeightTableCompliance(param.pred_weight_table) < 0)
      return -1;
  }

  if (param.decRefPicMarkingPresent) {
    /* dec_ref_pic_marking() */
    if (_checkH264DecRefPicMarkingCompliance(handle, param.dec_ref_pic_marking) < 0)
      return -1;
  }

  if (
    pps->entropy_coding_mode_flag &&
    !is_I_H264SliceTypeValue(param.slice_type) &&
    !is_SI_H264SliceTypeValue(param.slice_type)
  ) {
    LIBBLU_H264_DEBUG_SLICE(
      "   CABAC initialization table index "
      "(cabac_init_idc): %" PRIu8 " (0x%02" PRIX8 ").\n",
      param.cabac_init_idc,
      param.cabac_init_idc
    );

    if (2 < param.cabac_init_idc)
      LIBBLU_H264_CK_ERROR_RETURN(
        "Out of range 'cabac_init_idc' == %" PRIu8 ".\n",
        param.cabac_init_idc
      );
  }

  LIBBLU_H264_DEBUG_SLICE(
    "   Initial QPy value used for macroblocks of slice: %d (0x%X).\n",
    param.slice_qp_delta,
    param.slice_qp_delta
  );

  if (is_SP_H264SliceTypeValue(param.slice_type) || is_SI_H264SliceTypeValue(param.slice_type)) {
    if (is_SP_H264SliceTypeValue(param.slice_type)) {
      LIBBLU_H264_DEBUG_SLICE(
        "  Decoding process used to decode P macroblocks in SP slice "
        "(sp_for_switch_flag): %s (0b%X).\n",
        (param.sp_for_switch_flag) ? "Switching pictures" : "Non-switching pictures",
        param.sp_for_switch_flag
      );
    }

    LIBBLU_H264_DEBUG_SLICE(
      "   Initial QSy value used for macroblocks of slice: %d (0x%X).\n",
      param.slice_qs_delta,
      param.slice_qs_delta
    );
  }

  if (pps->deblocking_filter_control_present_flag) {
    LIBBLU_H264_DEBUG_SLICE(
      "   Deblocking filter control linked parameters "
      "('deblocking_filter_control_present_flag' == 1):\n"
    );

    LIBBLU_H264_DEBUG_SLICE(
      "    -> Deblocking filter status "
      "(disable_deblocking_filter_idc): %s (0x%X).\n",
      H264DeblockingFilterIdcStr(param.disable_deblocking_filter_idc),
      param.disable_deblocking_filter_idc
    );

    if (param.disable_deblocking_filter_idc != H264_DEBLOCKING_FILTER_DISABLED) {
      LIBBLU_H264_DEBUG_SLICE(
        "    -> Deblocking filter alpha and t_c0 tables offset used "
        "(slice_alpha_c0_offset_div2): %d (0x%X).\n",
        param.slice_alpha_c0_offset_div2 << 1,
        param.slice_alpha_c0_offset_div2
      );

      LIBBLU_H264_DEBUG_SLICE(
        "    -> Deblocking filter beta table offset used "
        "(slice_beta_offset_div2): %d (0x%X).\n",
        param.slice_beta_offset_div2 << 1,
        param.slice_beta_offset_div2
      );
    }
  }

  if (
    0 < pps->num_slice_groups_minus1
    && isChanging123H264SliceGroupMapTypeValue(
      pps->slice_groups.slice_group_map_type
    )
  ) {
    LIBBLU_H264_DEBUG_SLICE(
      "   Modifier of number of slice group map units "
      "(slice_group_change_cycle): %" PRIu32 " (0x%" PRIx32 ").\n",
      param.slice_group_change_cycle,
      param.slice_group_change_cycle
    );
  }

  return 0;
}

static int _checkH264SliceHeaderChangeCompliance(
  const H264ParametersHandlerPtr handle,
  H264SliceHeaderParameters first,
  H264SliceHeaderParameters second
)
{
  assert(NULL != handle);

  if (_checkH264SliceHeaderCompliance(handle, second, false) < 0)
    return -1;

  /* 'second' slice is part of the same primary coded picture as 'first' */
  bool invalid = false;

  char different_fields[12 * 30] = {'\0'};
  char *df_ptr = different_fields;

#define NOT_SAME(fn)  (first.fn != second.fn)
#define CHECK_CONCAT(fn)                                                      \
  do {                                                                        \
    if (NOT_SAME(fn)) {                                                       \
      lb_str_cat_comma(&df_ptr, "pic_parameter_set_id", invalid);             \
      invalid = true;                                                         \
    }                                                                         \
  } while (0)

  if (NOT_SAME(slice_type)) {
    bool first_unrestr_slice_type  = isUnrestrictedH264SliceTypeValue(first.slice_type);
    bool second_unrestr_slice_type = isUnrestrictedH264SliceTypeValue(second.slice_type);

    if (!first_unrestr_slice_type)
      LIBBLU_H264_CK_FAIL("Slice type of previous slice is restrictive.\n");
    if (!second_unrestr_slice_type)
      LIBBLU_H264_CK_FAIL("Slice type of current slice is restrictive.\n");

    if (!first_unrestr_slice_type || !second_unrestr_slice_type) {
      lb_str_cat_comma(&df_ptr, "slice_type", invalid);
      invalid = true;
    }
  }

  CHECK_CONCAT(pic_parameter_set_id);
  CHECK_CONCAT(frame_num);
  CHECK_CONCAT(field_pic_flag);
  CHECK_CONCAT(bottom_field_flag);
  CHECK_CONCAT(idr_pic_id);
  CHECK_CONCAT(pic_order_cnt_lsb);
  CHECK_CONCAT(delta_pic_order_cnt_bottom);
  CHECK_CONCAT(delta_pic_order_cnt[0]);
  CHECK_CONCAT(delta_pic_order_cnt[1]);
  CHECK_CONCAT(sp_for_switch_flag);
  CHECK_CONCAT(slice_group_change_cycle);

#undef NOT_SAME
#undef CHECK_CONCAT

  if (invalid)
    LIBBLU_H264_CK_FAIL_RETURN(
      "The following fields should have remained identical between two "
      "slice headers of the same coded picture: %s.\n",
      different_fields
    );

  /* Field specific checks */
  const H264SPSDataParameters *sps = &handle->sequence_parameter_set.data;
  H264CurrentProgressParam *cur_prog = &handle->cur_prog_param;

  // slice_type
  if ((first.slice_type % 5) != (second.slice_type % 5))
    cur_prog->cur_pic_has_diff_slice_types = true;

  if (5 <= second.slice_type && second.slice_type <= 9) {
    // Restrictive slice_type
    if (cur_prog->cur_pic_has_diff_slice_types)
      LIBBLU_H264_CK_ERROR_RETURN(
        "Slice type is restrictive but other slices of the same coded picture "
        "have a different slice type "
        "('slice_type' == %u).\n",
        second.slice_type
      );
  }

  // frame_num
  if (handle->constraints.forbiddenArbitrarySliceOrder || !sps->separate_colour_plane_flag) {
    if (second.first_mb_in_slice <= first.first_mb_in_slice)
      LIBBLU_H264_CK_ERROR_RETURN(
        "Broken slices order according to macroblocks addresses, "
        "arbitrary slice order is forbidden for bitstream profile "
        "conformance.\n"
      );
  }
  else {
    /* TODO: Add support of arbitrary slice order. */
    LIBBLU_TODO_MSG(
      "Arbitrary ordered slices structure is not supported.\n"
    );
  }


  return 0;
}

/* ###### Slice layer without partitioning : ############################### */

int checkH264SliceLayerWithoutPartitioningCompliance(
  const H264ParametersHandlerPtr handle,
  H264SliceLayerWithoutPartitioningParameters param
)
{
  /* assert(NULL != handle); */

  return _checkH264SliceHeaderCompliance(
    handle,
    param.header,
    true
  );
}

int checkH264SliceLayerWithoutPartitioningChangeCompliance(
  const H264ParametersHandlerPtr handle,
  H264SliceLayerWithoutPartitioningParameters first,
  H264SliceLayerWithoutPartitioningParameters second
)
{
  /* assert(NULL != handle); */

  return _checkH264SliceHeaderChangeCompliance(
    handle,
    first.header,
    second.header
  );
}

/* ### Access Unit : ####################################################### */

int checkContentH264AccessUnit(
  const H264ParametersHandlerPtr handle
)
{
  bool invalid_content = false;

  char missing_NALUs_names[4 * 30] = {'\0'};
  char *mn_ptr = missing_NALUs_names;

  // if (!handle->access_unit_delimiter_valid) {
  //   lb_str_cat_comma(&mn_ptr, "Access Unit Delimiter", invalid_content);
  //   invalid_content = true;
  // }

  // if (!handle->sequence_parameter_set_GOP_valid) {
  //   lb_str_cat_comma(&mn_ptr, "Sequence Parameter Set", invalid_content);
  //   invalid_content = true;
  // }

  if (!handle->slice_valid) {
    lb_str_cat_comma(&mn_ptr, "Coded slice", invalid_content);
    invalid_content = true;
  }

  if (invalid_content)
    LIBBLU_H264_CK_BD_FAIL_RETURN(
      "Incomplete access unit, following NAL units are missing: %s.\n",
      missing_NALUs_names
    );

  return 0;
}

int checkBDConstraintsH264AccessUnit(
  const H264ParametersHandlerPtr handle
)
{
  const H264CurrentProgressParam *cur = &handle->cur_prog_param;

  /* Check access unit content */
  bool is_GOP_start = (
    is_I_H264SliceTypeValue(handle->slice.header.slice_type)
    && handle->sequence_parameter_set_valid
  );
  // bool is_closed_GOP_start = (
  //   is_GOP_start
  //   && handle->slice.header.is_IDR_pic
  // );

  // AUD
  if (!handle->access_unit_delimiter_valid)
    LIBBLU_H264_CK_BD_FAIL_RETURN(
      "An Access Unit Delimiter shall be present "
      "as first access unit NAL unit.\n"
    );

  // SPS
  if (!is_GOP_start && handle->sequence_parameter_set_valid)
    LIBBLU_H264_CK_BD_FAIL_RETURN(
      "Illegal presence of Sequence Parameter Set in a subsequent "
      "access unit of a GOP.\n"
    );

  if (!handle->sequence_parameter_set.data.vui_parameters_present_flag)
    LIBBLU_H264_CK_BD_FAIL_RETURN(
      "VUI parameters shall be present in Sequence Parameter Set.\n"
    );

  // PPS
  if (is_GOP_start) {
    // First access unit
    if (H264_BD_MAX_ALLOWED_GOP_PPS < handle->nb_picture_parameter_set_valid)
      LIBBLU_H264_CK_BD_FAIL_RETURN(
        "The number of Picture Parameter Set present in the first access unit "
        "of a GOP exceeds the maximum of %u, got %u.\n",
        H264_BD_MAX_ALLOWED_GOP_PPS,
        handle->nb_picture_parameter_set_valid
      );
  }
  else {
    // Subsequent access unit
    if (H264_BD_MAX_ALLOWED_SUBSEQUENT_AU_PPS < handle->nb_picture_parameter_set_valid)
      LIBBLU_H264_CK_BD_FAIL_RETURN(
        "The number of Picture Parameter Set present in a subsequent access unit "
        "of a GOP exceeds the maximum of %u, got %u.\n",
        H264_BD_MAX_ALLOWED_GOP_PPS,
        handle->nb_picture_parameter_set_valid
      );
  }

  // SEI
  // Buffering period is not mandatory

  if (!handle->sequence_parameter_set.data.frame_mbs_only_flag) {
    // Picture timing
    if (!handle->sequence_parameter_set.data.vui_parameters.pic_struct_present_flag)
      LIBBLU_H264_CK_BD_FAIL_RETURN(
        "Picture structure shall be present in interlaced video sequence "
        "('frame_mbs_only_flag' == 0b1 in SPS).\n"
      );

    if (!handle->sei.pic_timing_valid)
      LIBBLU_H264_CK_BD_FAIL_RETURN(
        "Pic timing SEI message shall be present in interlaced "
        "video sequence ('frame_mbs_only_flag' == 0b1 in SPS).\n"
      );
  }

  /* Check constraints */
  const H264BDConstraintsParam *constraints = &handle->bd_constraints;
  assert(handle->bd_constraints_initialized);

  if (cur->nb_slices_in_picture < constraints->min_nb_slices)
    LIBBLU_H264_CK_BD_FAIL_RETURN(
      "Pending access unit contains %u slices (at least %u expected).\n",
      cur->nb_slices_in_picture,
      constraints->min_nb_slices
    );

  if (constraints->max_GOP_length < cur->nb_GOP_frames)
    LIBBLU_H264_CK_BD_FAIL_RETURN(
      "Pending Group Of Pictures is too long "
      "(at most %u P/B pictures expected).\n",
      constraints->max_GOP_length
    );

  if (constraints->max_nb_consecutive_B_frames < cur->nb_consecutive_B_frames)
    LIBBLU_H264_CK_BD_FAIL_RETURN(
      "Too many consecutive B frames "
      "(found %u frames, expected no more than %u).\n",
      cur->nb_consecutive_B_frames,
      constraints->max_nb_consecutive_B_frames
    );

  return 0;
}