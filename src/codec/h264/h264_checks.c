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
    "  Primary coded picture slices types (primary_pic_type): 0x%" PRIx8 ".\n",
    param.primaryPictType
  );

  LIBBLU_H264_DEBUG_AUD(
    "   -> slice_type authorized: %s.\n",
    h264PrimaryPictureTypeStr(param.primaryPictType)
  );

  if (H264_PRIM_PIC_TYPE_ALL < param.primaryPictType)
    LIBBLU_H264_ERROR_RETURN(
      "Reserved 'primary_pic_type' == %u in use.\n",
      param.primaryPictType
    );

  return 0; /* OK */
}

bool constantH264AccessUnitDelimiterCheck(
  const H264AccessUnitDelimiterParameters first,
  const H264AccessUnitDelimiterParameters second
)
{
  return
    first.primaryPictType
      == second.primaryPictType
  ;
}

int checkH264ProfileIdcCompliance(
  const H264ProfileIdcValue profileIdc,
  const H264ContraintFlags constraints
)
{
  int i;
  bool set;

  bool compliantProfile;

  LIBBLU_H264_DEBUG_SPS(
    "  Profile (profile_idc): %d.\n",
    profileIdc
  );

  LIBBLU_H264_DEBUG_SPS(
    "   => %s profile.\n",
    h264ProfileIdcValueStr (
      profileIdc,
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
    "   -> Reserved flags (reserved_zero_2bits): 0x%" PRIx8 ".\n",
    constraints.reservedFlags
  );

  compliantProfile = false;
  switch (profileIdc) {
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
        profileIdc,
        constraints
      ),
      profileIdc
    );
  }

  return 0; /* OK */
}

int checkH264LevelIdcCompliance(
  const uint8_t levelIdc
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
    "  Level (level_idc): %" PRIu8 ".%" PRIu8 " (0x%" PRIx8 ").\n",
    levelIdc / 10,
    levelIdc % 10,
    levelIdc
  );

  unknownLevel = true;
  for (i = 0; i < ARRAY_SIZE(levels); i++) {
    if (levels[i] == levelIdc) {
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
      levelIdc
    );
  }

  if (levelIdc < 30 || 41 < levelIdc) {
    /* BDAV unallowed levelIdc. */
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "Level %" PRIu8 ".%" PRIu8 " is not part of the "
      "levels allowed by BD-specs ('level_idc' == %d).\n",
      levelIdc / 10,
      levelIdc % 10,
      levelIdc
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
    "(cpb_cnt_minus1): %u (0x%x).\n",
    param.cpbCntMinus1 + 1,
    param.cpbCntMinus1
  );

  if (H264_BDAV_ALLOWED_CPB_CNT <= param.cpbCntMinus1) {
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "%u CPB configurations defined, "
      "expect no more than %u configurations "
      "('cpb_cnt_minus1' == %u).\n",
      param.cpbCntMinus1 + 1,
      H264_BDAV_ALLOWED_CPB_CNT,
      param.cpbCntMinus1
    );
  }

  if (H264_MAX_CPB_CONFIGURATIONS <= param.cpbCntMinus1) {
    LIBBLU_ERROR_RETURN(
      "Overflow, the number of defined CPB configurations exceed %u "
      "('cpb_cnt_minus1' == 0x%x).\n",
      H264_MAX_CPB_CONFIGURATIONS,
      param.cpbCntMinus1
    );
  }

  if (0 < param.cpbCntMinus1) {
    LIBBLU_H264_DEBUG_VUI(
      "     -> WARNING: Only the cpb_cnt_minus1-SchedSelIdx "
      "will be verified.\n"
    );
  }

  LIBBLU_H264_DEBUG_VUI(
    "    Bit-rate Scaling value "
    "(bit_rate_scale): %" PRIu8 " (0x%" PRIx8 ").\n",
    param.bitRateScale,
    param.bitRateScale
  );

  LIBBLU_H264_DEBUG_VUI(
    "    CPB Size Scaling value "
    "(cpb_size_scale): %" PRIu8 " (0x%" PRIx8 ").\n",
    param.cpbSizeScale,
    param.cpbSizeScale
  );

  LIBBLU_H264_DEBUG_VUI("    CPB configurations:\n");
  for (ShedSelIdx = 0; ShedSelIdx <= param.cpbCntMinus1; ShedSelIdx++) {
    LIBBLU_H264_DEBUG_VUI("     - ShedSelIdx %u:\n", ShedSelIdx);

    LIBBLU_H264_DEBUG_VUI(
      "      Bit rate value "
      "(bit_rate_value_minus1): %" PRIu32 " (0x%" PRIX32 ").\n",
      param.schedSel[ShedSelIdx].bitRateValueMinus1,
      param.schedSel[ShedSelIdx].bitRateValueMinus1
    );

    if (0 < ShedSelIdx) {
      if (
        param.schedSel[ShedSelIdx].bitRateValueMinus1
        <= param.schedSel[ShedSelIdx - 1].bitRateValueMinus1
      ) {
        LIBBLU_ERROR_RETURN(
          "Invalid HRD 'bit_rate_value_minus1[SchedSelIdx]' value for "
          "Schedule Selection Index %u, "
          "'bit_rate_value_minus1[SchedSelIdx-1]' == %" PRIu32 " shall be "
          "lower than 'bit_rate_value_minus1[SchedSelIdx]' == %" PRIu32 ".\n",
          ShedSelIdx,
          param.schedSel[ShedSelIdx-1].bitRateValueMinus1,
          param.schedSel[ShedSelIdx].bitRateValueMinus1
        );
      }
    }

    LIBBLU_H264_DEBUG_VUI(
      "       => Bit rate (BitRate): %" PRIu64 " bits/s.\n",
      param.schedSel[ShedSelIdx].bitRate
    );

    LIBBLU_H264_DEBUG_VUI(
      "      Coded Picture Buffer size value "
      "(cpb_size_value_minus1): %" PRIu32 " (0x%" PRIX32 ").\n",
      param.schedSel[ShedSelIdx].cpbSizeValueMinus1,
      param.schedSel[ShedSelIdx].cpbSizeValueMinus1
    );

    if (0 < ShedSelIdx) {
      if (
        param.schedSel[ShedSelIdx].cpbSizeValueMinus1
        > param.schedSel[ShedSelIdx - 1].cpbSizeValueMinus1
      ) {
        LIBBLU_ERROR_RETURN(
          "Invalid HRD 'cpb_size_value_minus1[SchedSelIdx]' value for "
          "Schedule Selection Index %u, "
          "'cpb_size_value_minus1[SchedSelIdx-1]' == %" PRIu32 " shall be "
          "greater or equal to 'cpb_size_value_minus1[SchedSelIdx]' "
          "== %" PRIu32 ".\n",
          ShedSelIdx,
          param.schedSel[ShedSelIdx-1].cpbSizeValueMinus1,
          param.schedSel[ShedSelIdx].cpbSizeValueMinus1
        );
      }
    }

    LIBBLU_H264_DEBUG_VUI(
      "       => CPB size (CpbSize): %" PRIu64 " bits.\n",
      param.schedSel[ShedSelIdx].cpbSize
    );

    LIBBLU_H264_DEBUG_VUI(
      "      Constant Bit-Rate mode (cbr_flag): %s (0x%x).\n",
      BOOL_STR(param.schedSel[ShedSelIdx].cbrFlag),
      param.schedSel[ShedSelIdx].cbrFlag
    );
  }

  LIBBLU_H264_DEBUG_VUI(
    "    Fields sizes:\n"
  );
  LIBBLU_H264_DEBUG_VUI(
    "     -> Initial CPB Delay "
    "(initial_cpb_removal_delay_length_minus1): "
    "%" PRIu8 " bits (0x%" PRIx8 ").\n",
    param.initialCpbRemovalDelayLengthMinus1 + 1,
    param.initialCpbRemovalDelayLengthMinus1
  );
  LIBBLU_H264_DEBUG_VUI(
    "     -> CPB Removal Delay "
    "(cpb_removal_delay_length_minus1): "
    "%" PRIu8 " bits (0x%" PRIx8 ").\n",
    param.cpbRemovalDelayLengthMinus1 + 1,
    param.cpbRemovalDelayLengthMinus1
  );
  LIBBLU_H264_DEBUG_VUI(
    "     -> DPB Output Delay "
    "(dpb_output_delay_length_minus1): "
    "%" PRIu8 " bits (0x%" PRIx8 ").\n",
    param.dpbOutputDelayLengthMinus1 + 1,
    param.dpbOutputDelayLengthMinus1
  );
  LIBBLU_H264_DEBUG_VUI(
    "     -> Time Offset "
    "(time_offset_length): "
    "%" PRIu8 " bits (0x%" PRIx8 ").\n",
    param.timeOffsetLength,
    param.timeOffsetLength
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
    "(aspect_ratio_info_present_flag): %s (0b%x).\n",
    BOOL_PRESENCE(param.aspectRatioInfoPresent),
    param.aspectRatioInfoPresent
  );

  if (param.aspectRatioInfoPresent) {
    LIBBLU_H264_DEBUG_VUI(
      "    -> Sample Aspect Ratio, SAR "
      "(aspect_ratio_idc): %s (0x%x).\n",
      h264AspectRatioIdcValueStr(param.aspectRatioIdc),
      param.aspectRatioIdc
    );

    if (
      H264_ASPECT_RATIO_IDC_2_BY_1 < param.aspectRatioIdc
      && param.aspectRatioIdc != H264_ASPECT_RATIO_IDC_EXTENDED_SAR
    ) {
      LIBBLU_H264_ERROR_RETURN(
        "Reserved 'aspect_ratio_idc' == %d in use in VUI parameters.\n",
        param.aspectRatioIdc
      );
    }

    if (param.aspectRatioIdc == H264_ASPECT_RATIO_IDC_EXTENDED_SAR) {
      LIBBLU_H264_DEBUG_VUI(
        "    -> Sample aspect ratio horizontal size (sar_width): %u.\n",
        param.extendedSAR.width
      );
      LIBBLU_H264_DEBUG_VUI(
        "    -> Sample aspect ratio vertical size (sar_height): %u.\n",
        param.extendedSAR.height
      );
      LIBBLU_H264_DEBUG_VUI(
        "     => Extended SAR ratio: %u:%u.\n",
        param.extendedSAR.width,
        param.extendedSAR.height
      );
    }
  }
  else {
    LIBBLU_H264_DEBUG_VUI(
      "    -> Sample Aspect Ratio, SAR "
      "(aspect_ratio_idc): %s (Inferred).\n",
      h264AspectRatioIdcValueStr(param.aspectRatioIdc),
      param.aspectRatioIdc
    );
  }

  if (!param.aspectRatioInfoPresent) {
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
    expectedSar, param.aspectRatioIdc
  );

  if (!valid) {
    if (expectedSar.a != expectedSar.b) {
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Invalid SAR %s in SPS VUI parameters for %ux%u, "
        "BD-specs require %s or %s for this "
        "resolution ('aspect_ratio_idc' == %d).\n",
        h264AspectRatioIdcValueStr(param.aspectRatioIdc),
        spsParam.FrameWidth,
        spsParam.FrameHeight,
        h264AspectRatioIdcValueStr(expectedSar.a),
        h264AspectRatioIdcValueStr(expectedSar.b),
        param.aspectRatioIdc
      );
    }

    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "Invalid SAR %s for %ux%u, BD-specs require %s for this "
      "resolution ('aspect_ratio_idc' == %d).\n",
      h264AspectRatioIdcValueStr(param.aspectRatioIdc),
      spsParam.FrameWidth,
      spsParam.FrameHeight,
      h264AspectRatioIdcValueStr(expectedSar.a),
      param.aspectRatioIdc
    );
  }

  LIBBLU_H264_DEBUG_VUI(
    "   Over-scan preference information "
    "(overscan_info_present_flag): %s (0b%x).\n",
    BOOL_PRESENCE(param.overscanInfoPresent),
    param.overscanInfoPresent
  );

  if (param.overscanInfoPresent) {
    LIBBLU_H264_DEBUG_VUI(
      "    -> Over-scan preference "
      "(overscan_appropriate_flag): %s (0b%x).\n",
      (param.overscanAppropriate) ? "Crop decoded pictures" : "Add margins",
      param.overscanAppropriate
    );

    if (
      CHECK_H264_WARNING_FLAG(handle, vuiInvalidOverscan)
      && param.overscanAppropriate != 0x0
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
    "(video_signal_type_present_flag): %s (0b%x).\n",
    BOOL_PRESENCE(param.videoSignalTypePresent),
    param.videoSignalTypePresent
  );

  if (param.videoSignalTypePresent) {
    handle->curProgParam.missingVSTVuiParameters = false;

    LIBBLU_H264_DEBUG_VUI(
      "    -> Video format (video_format): %s (0x%x).\n",
      h264VideoFormatValueStr(param.videoSignalType.videoFormat),
      param.videoSignalType.videoFormat
    );

    if (H264_VIDEO_FORMAT_UNSPECIFIED < param.videoSignalType.videoFormat) {
      LIBBLU_H264_ERROR_RETURN(
        "Reserved 'video_format' == %d in use.\n",
        param.videoSignalType.videoFormat
      );
    }

    expectedVideoFormat = getH264BdavExpectedVideoFormat(
      param.timingInfo.frameRate
    );

    if (
      CHECK_H264_WARNING_FLAG(handle, vuiUnexpectedVideoFormat)
      && expectedVideoFormat != param.videoSignalType.videoFormat
    ) {
      LIBBLU_WARNING(
        "Unexpected Video Format %s in SPS VUI parameters "
        "for %ux%u, should be %s for this "
        "resolution ('video_format' == %d).\n",
        h264VideoFormatValueStr(param.videoSignalType.videoFormat),
        spsParam.FrameWidth,
        spsParam.FrameHeight,
        h264VideoFormatValueStr(expectedVideoFormat),
        param.videoSignalType.videoFormat
      );

      handle->curProgParam.wrongVuiParameters = true;
      MARK_H264_WARNING_FLAG(handle, vuiUnexpectedVideoFormat);
    }

    LIBBLU_H264_DEBUG_VUI(
      "    -> Full-range luma and chroma samples values "
      "(video_full_range_flag): %s (0b%x).\n",
      BOOL_STR(param.videoSignalType.videoFullRange),
      param.videoSignalType.videoFullRange
    );

    if (
      CHECK_H264_WARNING_FLAG(handle, vuiUnexpectedVideoFullRange)
      && param.videoSignalType.videoFullRange
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
      "(colour_description_present_flag): %s (0b%x).\n",
      BOOL_PRESENCE(param.videoSignalType.colourDescPresent),
      param.videoSignalType.colourDescPresent
    );

    if (param.videoSignalType.colourDescPresent) {
      colourParam = &param.videoSignalType.colourDescription;

      LIBBLU_H264_DEBUG_VUI(
        "     -> Colour primaries "
        "(colour_primaries): %s (0x%x).\n",
        h264ColorPrimariesValueStr(colourParam->colourPrimaries),
        colourParam->colourPrimaries
      );

      expectedColourPrimaries = getH264BdavExpectedColorPrimaries(
        spsParam.FrameHeight
      );

      if (
        CHECK_H264_WARNING_FLAG(handle, vuiUnexpectedColourPrimaries)
        && expectedColourPrimaries != colourParam->colourPrimaries
      ) {
        LIBBLU_WARNING(
          "Color Primaries in SPS VUI parameters should be defined to %s, "
          "not %s ('colour_primaries' == 0x%x).\n",
          h264ColorPrimariesValueStr(expectedColourPrimaries),
          h264ColorPrimariesValueStr(colourParam->colourPrimaries),
          colourParam->colourPrimaries
        );
        handle->curProgParam.wrongVuiParameters = true;
        MARK_H264_WARNING_FLAG(handle, vuiUnexpectedColourPrimaries);
      }

      LIBBLU_H264_DEBUG_VUI(
        "     -> Transfer characteristics "
        "(transfer_characteristics): %s (0x%x).\n",
        h264TransferCharacteristicsValueStr(colourParam->transferCharact),
        colourParam->transferCharact
      );

      expectedTransferCharact = getH264BdavExpectedTransferCharacteritics(
        spsParam.FrameHeight
      );

      if (
        CHECK_H264_WARNING_FLAG(handle, vuiUnexpectedTranferCharact)
        && expectedTransferCharact != colourParam->transferCharact
      ) {
        LIBBLU_WARNING(
          "Transfer Characteristics in SPS VUI parameters should "
          "be defined to %s, not %s ('transfer_characteristics' == 0x%x).\n",
          h264TransferCharacteristicsValueStr(expectedTransferCharact),
          h264TransferCharacteristicsValueStr(colourParam->transferCharact),
          colourParam->transferCharact
        );
        handle->curProgParam.wrongVuiParameters = true;
        MARK_H264_WARNING_FLAG(handle, vuiUnexpectedTranferCharact);
      }

      LIBBLU_H264_DEBUG_VUI(
        "     -> Matrix coefficients "
        "(matrix_coefficients): %s (0x%x).\n",
        h264MatrixCoefficientsValueStr(colourParam->matrixCoeff),
        colourParam->matrixCoeff
      );

      expectedMatrixCoeff = getH264BdavExpectedMatrixCoefficients(
        spsParam.FrameHeight
      );

      if (
        CHECK_H264_WARNING_FLAG(handle, vuiUnexpectedMatrixCoeff)
        && expectedMatrixCoeff != colourParam->matrixCoeff
      ) {
        LIBBLU_WARNING(
          "Matrix coefficients in SPS VUI parameters should "
          "be defined to %s, not %s ('matrix_coefficients' == 0x%x).\n",
          h264MatrixCoefficientsValueStr(expectedMatrixCoeff),
          h264MatrixCoefficientsValueStr(colourParam->matrixCoeff),
          colourParam->matrixCoeff
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
      h264VideoFormatValueStr(param.videoSignalType.videoFormat)
    );
    LIBBLU_H264_DEBUG_VUI(
      "    -> Full-range luma and chroma samples values "
      "(video_full_range_flag): %s (Inferred).\n",
      BOOL_STR(param.videoSignalType.videoFullRange)
    );
    LIBBLU_H264_DEBUG_VUI(
      "    -> Color description parameters "
      "(colour_description_present_flag): %s (Inferred).\n",
      BOOL_PRESENCE(param.videoSignalType.colourDescPresent)
    );
  }

  if (
    CHECK_H264_WARNING_FLAG(handle, vuiMissingVideoSignalType)
    && !param.videoSignalTypePresent
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
    && !param.videoSignalType.colourDescPresent
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
    "(chroma_loc_info_present_flag): %s (0b%x).\n",
    BOOL_PRESENCE(param.chromaLocInfoPresent),
    param.chromaLocInfoPresent
  );

  if (param.chromaLocInfoPresent) {
    LIBBLU_H264_DEBUG_VUI(
      "    -> Top-field "
      "(chroma_sample_loc_type_top_field): %u (0x%x).\n",
      param.ChromaLocInfo.sampleLocTypeTopField,
      param.ChromaLocInfo.sampleLocTypeTopField
    );
    LIBBLU_H264_DEBUG_VUI(
      "    -> Bottom-field "
      "(chroma_sample_loc_type_bottom_field): %u (0x%x).\n",
      param.ChromaLocInfo.sampleLocTypeBottomField,
      param.ChromaLocInfo.sampleLocTypeBottomField
    );

    if (
      (
        param.ChromaLocInfo.sampleLocTypeTopField != 0
        && param.ChromaLocInfo.sampleLocTypeTopField != 2
      )
      || (
        param.ChromaLocInfo.sampleLocTypeBottomField != 0
        && param.ChromaLocInfo.sampleLocTypeBottomField != 2
      )
    ) {
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Chroma sample location if present shall be set to 0 or 2 "
        "according to BD-specs ('chroma_sample_loc_type_top_field' == %d / "
        "'chroma_sample_loc_type_bottom_field' == %d).\n",
        param.ChromaLocInfo.sampleLocTypeTopField,
        param.ChromaLocInfo.sampleLocTypeBottomField
      );
    }
  }
  else {
    LIBBLU_H264_DEBUG_VUI(
      "    -> Top-field "
      "(chroma_sample_loc_type_top_field): %u (Inferred).\n",
      param.ChromaLocInfo.sampleLocTypeTopField
    );
    LIBBLU_H264_DEBUG_VUI(
      "    -> Bottom-field "
      "(chroma_sample_loc_type_bottom_field): %u (Inferred).\n",
      param.ChromaLocInfo.sampleLocTypeBottomField
    );
  }

  LIBBLU_H264_DEBUG_VUI(
    "   Timing information (timing_info_present_flag): %s (0b%x).\n",
    BOOL_PRESENCE(param.timingInfoPresent),
    param.timingInfoPresent
  );

  if (!param.timingInfoPresent)
    LIBBLU_H264_ERROR_RETURN(
      "Timing information shall be present in SPS VUI parameters "
      "('timing_info_present_flag' == 0b0).\n"
    );

  if (param.timingInfoPresent) {
    LIBBLU_H264_DEBUG_VUI(
      "    -> Number of time units (num_units_in_tick): "
      "%" PRIu32 " (0x%" PRIx32 ").\n",
      param.timingInfo.numUnitsInTick,
      param.timingInfo.numUnitsInTick
    );

    LIBBLU_H264_DEBUG_VUI(
      "    -> Time scale clock (time_scale): "
      "%" PRIu32 " Hz (0x%" PRIx32 ").\n",
      param.timingInfo.timeScale,
      param.timingInfo.timeScale
    );

    LIBBLU_H264_DEBUG_VUI(
      "    -> Fixed frame rate "
      "(fixed_frame_rate_flag): %s (0x%x).\n",
      BOOL_STR(param.timingInfo.fixedFrameRateFlag),
      param.timingInfo.fixedFrameRateFlag
    );

    if (!param.timingInfo.fixedFrameRateFlag) {
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Forbidden use of a variable frame-rate mode "
        "('fixed_frame_rate_flag' == 0b0).\n"
      );
    }

    /* Equation D-2 */
    LIBBLU_H264_DEBUG_VUI(
      "    => Frame-rate: %.3f.\n",
      param.timingInfo.frameRate
    );
  }

  LIBBLU_H264_DEBUG_VUI(
    "   NAL HRD parameters "
    "(nal_hrd_parameters_present_flag): %s (0b%x).\n",
    BOOL_PRESENCE(param.nalHrdParamPresent),
    param.nalHrdParamPresent
  );

  if (!param.nalHrdParamPresent) {
    LIBBLU_H264_DEBUG_VUI(
      "    -> Warning: Absence of NAL HRD parameters is only tolerated "
      "for still-picture steams.\n"
    );
    handle->curProgParam.stillPictureTolerance = true;
  }

  if (param.nalHrdParamPresent) {
    if (checkH264HrdParametersCompliance(param.nalHrdParam) < 0)
      return -1;
  }

  LIBBLU_H264_DEBUG_VUI(
    "   VCL HRD parameters "
    "(vcl_hrd_parameters_present_flag): %s (0b%x).\n",
    BOOL_PRESENCE(param.vclHrdParamPresent),
    param.vclHrdParamPresent
  );

  if (param.vclHrdParamPresent) {
    if (checkH264HrdParametersCompliance(param.vclHrdParam) < 0)
      return -1;
  }

  if (param.nalHrdParamPresent || param.vclHrdParamPresent) {
    LIBBLU_H264_DEBUG_VUI(
      "   Low delay (low_delay_hrd_flag): %s (0b%x).\n",
      BOOL_STR(param.lowDelayHrd),
      param.lowDelayHrd
    );

    if (param.lowDelayHrd) {
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Forbidden use of low delay HRD "
        "('low_delay_hrd_flag' == 0b1).\n"
      );
    }
  }

#if 0
  if (param.nalHrdParamPresent && param.vclHrdParamPresent) {
    /* Check fields lengths */
    if (
      param.nalHrdParam.initialCpbRemovalDelayLengthMinus1
        != param.vclHrdParam.initialCpbRemovalDelayLengthMinus1
      || param.nalHrdParam.cpbRemovalDelayLengthMinus1
        != param.vclHrdParam.cpbRemovalDelayLengthMinus1
      || param.nalHrdParam.dpbOutputDelayLengthMinus1
        != param.vclHrdParam.dpbOutputDelayLengthMinus1
      || param.nalHrdParam.timeOffsetLength
        != param.vclHrdParam.timeOffsetLength
    ) {
      LIBBLU_H264_ERROR_RETURN(
        "Variable CPB/DPB HRD fields lengths.\n"
      );
    }
  }
#endif

  LIBBLU_H264_DEBUG_VUI(
    "   Picture Structure in SEI picture timing messages "
    "(pic_struct_present_flag): %s (0b%x).\n",
    BOOL_PRESENCE(param.picStructPresent),
    param.picStructPresent
  );
  /**
   * NOTE: Compliance of 'pic_struct_present_flag' is verified
   * in SPS verifier (calling function).
   */

  LIBBLU_H264_DEBUG_VUI(
    "   Video Sequence Bitstream Restriction parameters "
    "(bitstream_restriction_flag): %s (0b%x).\n",
    BOOL_PRESENCE(param.bitstreamRestrictionsPresent),
    param.bitstreamRestrictionsPresent
  );

  if (param.bitstreamRestrictionsPresent) {
    const H264VuiVideoSeqBitstreamRestrictionsParameters * restrParam;
    restrParam = &param.bistreamRestrictions;

    LIBBLU_H264_DEBUG_VUI(
      "    Samples outside picture boundaries in Motion Vectors "
      "(motion_vectors_over_pic_boundaries_flag): %s (0b%x).\n",
      BOOL_PRESENCE(restrParam->motionVectorsOverPicBoundaries),
      restrParam->motionVectorsOverPicBoundaries
    );

    LIBBLU_H264_DEBUG_VUI(
      "    Maximum picture size "
      "(max_bytes_per_pic_denom): "
    );
    if (restrParam->maxBytesPerPicDenom != 0) {
      LIBBLU_H264_DEBUG_VUI_NH(
        "%" PRIu8 " bytes",
        restrParam->maxBytesPerPic
      );
    }
    else
      LIBBLU_H264_DEBUG_VUI_NH("Unlimited");
    LIBBLU_H264_DEBUG_VUI_NH(
      " (0x%" PRIx8 ").\n",
      restrParam->maxBytesPerPicDenom
    );

    LIBBLU_H264_DEBUG_VUI(
      "    Maximum macro-block size (max_bits_per_mb_denom): "
    );
    if (restrParam->maxBitsPerPicDenom != 0) {
      LIBBLU_H264_DEBUG_VUI_NH(
        "%" PRIu8 " bits",
        restrParam->maxBitsPerMb
      );
    }
    else
      LIBBLU_H264_DEBUG_VUI_NH("Unlimited");
    LIBBLU_H264_DEBUG_VUI_NH(
      " (0x%" PRIx8 ").\n",
      restrParam->maxBitsPerPicDenom
    );

    LIBBLU_H264_DEBUG_VUI(
      "    Maximum horizontal motion vector length "
      "(log2_max_mv_length_horizontal): "
      "%.2f luma sample(s) (0x%" PRIx8 ").\n",
      0.25 * restrParam->log2MaxMvLengthHorizontal,
      restrParam->log2MaxMvLengthHorizontal
    );

    LIBBLU_H264_DEBUG_VUI(
      "    Maximum vertical motion vector length "
      "(log2_max_mv_length_vertical): "
      "%.2f luma sample(s) (0x%" PRIx8 ").\n",
      0.25 * restrParam->log2MaxMvLengthVertical,
      restrParam->log2MaxMvLengthVertical
    );

    LIBBLU_H264_DEBUG_VUI(
      "    Maximum number of frames in DPB "
      "(max_num_reorder_frames): "
      "%" PRIu16 " frame buffers (0x%" PRIx16 ").\n",
      restrParam->maxNumReorderFrames,
      restrParam->maxNumReorderFrames
    );

    LIBBLU_H264_DEBUG_VUI(
      "    HRD-DPB required size "
      "(max_dec_frame_buffering): "
      "%" PRIu16 " frame buffers (0x%" PRIx16 ").\n",
      restrParam->maxDecFrameBuffering,
      restrParam->maxDecFrameBuffering
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
    param.profileIdc,
    param.constraintFlags
  );
  if (ret < 0)
    return -1;
  updateH264ProfileLimits(
    handle,
    param.profileIdc,
    param.constraintFlags,
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

  if (checkH264LevelIdcCompliance(param.levelIdc) < 0)
    return -1;
  updateH264LevelLimits(handle, param.levelIdc);

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
      "Unable to a find a MaxFS value for 'level_idc' == 0x%" PRIx8 ".\n",
      param.levelIdc
    );
  }

  handle->constraints.brNal = getH264BrNal(
    handle->constraints,
    param.profileIdc
  );

  LIBBLU_H264_DEBUG_SPS(
    "   => BitRate "
    "(Maximum allowed NAL HRD Bit Rate in bits/sec): %u.\n",
    handle->constraints.brNal
  );

  LIBBLU_H264_DEBUG_SPS(
    "  Sequence Parameter Set ID (seq_parameter_set_id): %u (0x%x).\n",
    param.seqParametersSetId,
    param.seqParametersSetId
  );

  /**
   * TODO: Enable better check of non multiple SPS ID.
   */
  if (param.seqParametersSetId != 0)
    LIBBLU_H264_ERROR_RETURN(
      "Unsupported multiple Sequence Parameters Set definitions, "
      "only 'seq_parameter_set_id' == 0x0 supported.\n"
    );

  if (H264_PROFILE_IS_HIGH(param.profileIdc)) {
    LIBBLU_H264_DEBUG_SPS(
      "  Chroma sampling format (chroma_format_idc): %s (0x%x).\n",
      h264ChromaFormatIdcValueStr(param.chromaFormat),
      param.chromaFormat
    );

    if (H264_CHROMA_FORMAT_444 < param.chromaFormat)
      LIBBLU_H264_ERROR_RETURN(
        "Reserved 'chroma_format_idc' == %d in use.\n",
        param.chromaFormat
      );

    if (param.chromaFormat == H264_CHROMA_FORMAT_444)
      LIBBLU_H264_DEBUG_SPS(
        "  Separate color channels planes (separate_colour_plane_flag): "
        "%s (0x).\n",
        BOOL_STR(param.separateColourPlaneFlag444),
        param.separateColourPlaneFlag444
      );

    if (
      handle->constraints.restrictedChromaFormatIdc
      != H264_UNRESTRICTED_CHROMA_FORMAT_IDC
    ) {
      /* Check chroma_format_idc */

      if (
        !(
          (1 << param.chromaFormat)
          & handle->constraints.restrictedChromaFormatIdc
        )
      ) {
        LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
          "Unexpected Chroma sampling format %s "
          "('chroma_format_idc' == %d).\n",
          h264ChromaFormatIdcValueStr(param.chromaFormat),
          param.chromaFormat
        );
      }
    }

    LIBBLU_H264_DEBUG_SPS(
      "  Luma samples Bit Depth value (bit_depth_luma_minus8): "
      "%" PRIu8 " (0x%" PRIx8 ").\n",
      param.bitDepthLumaMinus8,
      param.bitDepthLumaMinus8
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
      "%" PRIu8 " (0x%" PRIx8 ").\n",
      param.bitDepthChromaMinus8,
      param.bitDepthChromaMinus8
    );

    LIBBLU_H264_DEBUG_SPS(
      "   => BitDepth_C (Chroma samples Bit Depth): %u bits.\n",
      param.BitDepthChroma
    );

    LIBBLU_H264_DEBUG_SPS(
      "   => QpBdOffset_C (Chroma quantization parameter range): %u.\n",
      param.QpBdOffsetChroma
    );

    if (param.chromaFormat == H264_CHROMA_FORMAT_400) {
      LIBBLU_H264_DEBUG_SPS(
        "  -> Not used, only luma is present in monochrome mode.\n"
      );
    }
    else if (
      param.chromaFormat == H264_CHROMA_FORMAT_444
      && param.separateColourPlaneFlag444
    )
      LIBBLU_H264_DEBUG_SPS(
        "  -> Not used, separated color planes are coded according to Luma "
        "Bit-depth value.\n"
      );

    if (
      handle->constraints.maxAllowedBitDepthMinus8
        < param.bitDepthLumaMinus8
      || handle->constraints.maxAllowedBitDepthMinus8
        < param.bitDepthChromaMinus8
    ) {
      LIBBLU_ERROR(
        "Luma and/or chroma bit-depth used are not allowed for "
        "bitstream profile conformance.\n"
      );

      if (
        handle->constraints.maxAllowedBitDepthMinus8
        < param.bitDepthLumaMinus8
      )
        LIBBLU_ERROR(
          " -> Luma bit-depth: %u bits over %u bits maximal "
          "allowed value.\n",
          param.BitDepthLuma,
          handle->constraints.maxAllowedBitDepthMinus8 + 8
        );

      if (
        handle->constraints.maxAllowedBitDepthMinus8
        < param.bitDepthChromaMinus8
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
      "(qpprime_y_zero_transform_bypass_flag): %s (0x%x).\n",
      BOOL_STR(param.qpprimeYZeroTransformBypass),
      param.qpprimeYZeroTransformBypass
    );

    if (
      param.qpprimeYZeroTransformBypass
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
      "(seq_scaling_matrix_present_flag): %s (0x%x).\n",
      BOOL_PRESENCE(param.seqScalingMatrixPresent),
      param.seqScalingMatrixPresent
    );

    if (!param.seqScalingMatrixPresent)
      LIBBLU_H264_DEBUG_SPS(
        "   -> Use Flat_4x4_16 and Flat_8x8_16 scaling lists.\n"
      );

    if (param.seqScalingMatrixPresent) {
      unsigned nbLists;

      LIBBLU_H264_DEBUG_SPS("  Sequence Scaling Lists:\n");

      nbLists = (param.chromaFormat != H264_CHROMA_FORMAT_444) ? 8 : 12;
      for (i = 0; i < nbLists; i++) {
        LIBBLU_H264_DEBUG_SPS(
          "   -> %s (seq_scaling_list_present_flag[%x]): %s (0b%x).\n",
          getSequenceScalingListName(i), i,
          BOOL_PRESENCE(param.seqScalingMatrix.seqScalingListPresent[i]),
          param.seqScalingMatrix.seqScalingListPresent[i]
        );
      }
    }
  }
#if 1
  else {
    LIBBLU_H264_DEBUG_SPS(
      "  Chroma sampling format (chroma_format_idc): %s (Inferred).\n",
      h264ChromaFormatIdcValueStr(param.chromaFormat)
    );
    LIBBLU_H264_DEBUG_SPS(
      "  Luma samples Bit Depth value (bit_depth_luma_minus8): "
      "%" PRIu8 " (Inferred)",
      param.bitDepthLumaMinus8
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
      param.bitDepthChromaMinus8
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
      BOOL_STR(param.qpprimeYZeroTransformBypass)
    );
    LIBBLU_H264_DEBUG_SPS(
      "  Sequence Scaling Matrix "
      "(seq_scaling_matrix_present_flag): %s (Inferred).\n",
      BOOL_PRESENCE(param.seqScalingMatrixPresent)
    );
    if (!param.seqScalingMatrixPresent)
      LIBBLU_H264_DEBUG_SPS(
        "   -> Use Flat_4x4_16 and Flat_8x8_16 scaling lists.\n"
      );
  }
#endif

  LIBBLU_H264_DEBUG_SPS(
    "  Max Frame number value "
    "(log2_max_frame_num_minus4): %u (0x%" PRIx8 ").\n",
    param.log2MaxFrameNumMinus4,
    param.log2MaxFrameNumMinus4
  );

  LIBBLU_H264_DEBUG_SPS(
    "   => MaxFrameNum: %u.\n",
    param.MaxFrameNum
  );

  if (12 < param.log2MaxFrameNumMinus4) {
    LIBBLU_H264_ERROR_RETURN(
      "Out of range 'log2_max_frame_num_minus4' == %u, "
      "shall not exceed 12 (Rec. ITU-T H.264 7.4.2.1.1).\n",
      param.log2MaxFrameNumMinus4
    );
  }

  LIBBLU_H264_DEBUG_SPS(
    "  Picture order count type "
    "(pic_order_cnt_type): Type %" PRIu8 " (0x%" PRIx8 ").\n",
    param.picOrderCntType,
    param.picOrderCntType
  );

  switch (param.picOrderCntType) {
    case 0:
      LIBBLU_H264_DEBUG_SPS(
        "   -> Max picture order count LSB value "
        "(log2_max_pic_order_cnt_lsb_minus4): %d (0x%" PRIx8 ").\n",
        param.log2MaxPicOrderCntLsbMinus4,
        param.log2MaxPicOrderCntLsbMinus4
      );
      LIBBLU_H264_DEBUG_SPS(
        "    => MaxPicOrderCntLsb: %u.\n",
        param.MaxPicOrderCntLsb
      );

      if (12 < param.log2MaxPicOrderCntLsbMinus4) {
        LIBBLU_H264_ERROR_RETURN(
          "Out of range 'log2_max_pic_order_cnt_lsb_minus4' == %u, "
          "shall not exceed 12 (Rec. ITU-T H.264 7.4.2.1.1).\n",
          param.log2MaxPicOrderCntLsbMinus4
        );
      }
      break;

    case 1:
      LIBBLU_H264_DEBUG_SPS(
        "   -> Delta picture order counter "
        "(delta_pic_order_always_zero_flag): %s (0x%x).\n",
        BOOL_PRESENCE(!param.deltaPicOrderAlwaysZero),
        param.deltaPicOrderAlwaysZero
      );

      if (param.deltaPicOrderAlwaysZero) {
        LIBBLU_H264_DEBUG_SPS(
          "    -> 'delta_pic_order_cnt[0]' and 'delta_pic_order_cnt[1]' "
          "not present and inferred to be equal to 0.\n"
        );
      }

      LIBBLU_H264_DEBUG_SPS(
        "   -> Offset for non-referential picture "
        "(offset_for_non_ref_pic): %d (0x%x).\n",
        param.offsetForNonRefPic,
        param.offsetForNonRefPic
      );

      LIBBLU_H264_DEBUG_SPS(
        "   -> Offset for bottom-field "
        "(offset_for_top_to_bottom_field): %d (0x%x).\n",
        param.offsetForTopToBottomField,
        param.offsetForTopToBottomField
      );

      LIBBLU_H264_DEBUG_SPS(
        "   -> Number of referential frames in picture order counter cycle "
        "(num_ref_frames_in_pic_order_cnt_cycle): %u (0x%x)",
        param.numRefFramesInPicOrderCntCycle,
        param.numRefFramesInPicOrderCntCycle
      );

      for (i = 0; i < param.numRefFramesInPicOrderCntCycle; i++) {
        LIBBLU_H264_DEBUG_SPS(
          "    -> Offset for referential frame %u "
          "(offset_for_ref_frame[%u]): %d (0x%x).\n",
          i, i,
          param.offsetForRefFrame[i],
          param.offsetForRefFrame[i]
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
        param.picOrderCntType
      );
  }

  LIBBLU_H264_DEBUG_SPS(
    "  Max number of referential frames "
    "(max_num_ref_frames): %u (0x%x).\n",
    param.maxNumRefFrames,
    param.maxNumRefFrames
  );

  if (0 == (MaxDpbMbs = handle->constraints.MaxDpbMbs)) {
    LIBBLU_H264_ERROR_RETURN(
      "Unable to a find a MaxDpbMbs value for 'level_idc' == 0x%" PRIx8 ".\n",
      param.levelIdc
    );
  }

  /* Equation A.3.1.h) */
  MaxDpbFrames = MIN(
    MaxDpbMbs / (param.PicWidthInMbs * param.FrameHeightInMbs),
    16
  );

  if (MaxDpbFrames < param.maxNumRefFrames) {
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "Out of range 'max_num_ref_frames' == %u in SPS, shall not exceed "
      "'MaxDpbFrames' == %u for bitstream level conformance.\n",
      param.maxNumRefFrames, MaxDpbFrames
    );
  }

  LIBBLU_H264_DEBUG_SPS(
    "  Gaps allowed in frame number values "
    "(gaps_in_frame_num_value_allowed_flag): %s (0x%x).\n",
    BOOL_STR(param.gapsInFrameNumValueAllowed),
    param.gapsInFrameNumValueAllowed
  );

  if (param.gapsInFrameNumValueAllowed) {
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "Unexpected allowed presence of gaps in frame number value "
      "('gaps_in_frame_num_value_allowed_flag' == 0b1).\n"
    );
  }

  LIBBLU_H264_DEBUG_SPS(
    "  Decoded Picture Width in MacroBlocks value "
    "(pic_width_in_mbs_minus1): %u (0x%x).\n",
    param.picWidthInMbsMinus1,
    param.picWidthInMbsMinus1
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
      H264_PROFILE_IS_HIGH(param.profileIdc) ? "A.3.2.d)" : "A.3.1.f)",
      (unsigned) sqrt(MaxFS * 8),
      param.PicWidthInMbs,
      param.levelIdc / 10, param.levelIdc % 10
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
    "(pic_height_in_map_units_minus1): %u (0x%x).\n",
    param.picHeightInMapUnitsMinus1,
    param.picHeightInMapUnitsMinus1
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
    "(frame_mbs_only_flag): %s (0x%x).\n",
    BOOL_STR(param.frameMbsOnly),
    param.frameMbsOnly
  );

  if (!param.frameMbsOnly) {
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
        param.profileIdc == H264_PROFILE_MAIN
        || param.profileIdc == H264_PROFILE_EXTENDED
        || param.profileIdc == H264_PROFILE_HIGH
        || param.profileIdc == H264_PROFILE_HIGH_10
        || param.profileIdc == H264_PROFILE_HIGH_422
        || param.profileIdc == H264_PROFILE_HIGH_444_PREDICTIVE
        || param.profileIdc == H264_PROFILE_CAVLC_444_INTRA
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
      H264_PROFILE_IS_HIGH(param.profileIdc) ? "A.3.2.e)" : "A.3.1.g)",
      (unsigned) sqrt(MaxFS * 8),
      param.FrameHeightInMbs,
      param.levelIdc / 10, param.levelIdc % 10
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
      H264_PROFILE_IS_HIGH(param.profileIdc) ? "A.3.2.c)" : "A.3.1.e)",
      MaxFS,
      param.PicWidthInMbs * param.FrameHeightInMbs,
      param.levelIdc / 10, param.levelIdc % 10
    );
  }

  if (!param.frameMbsOnly) {
    LIBBLU_H264_DEBUG_SPS(
      "   -> Adaptive frame/field macroblocks switching "
      "(mb_adaptive_frame_field_flag): %s (0x%x).\n",
      (param.mbAdaptativeFrameField) ? "Allowed" : "Forbidden",
      param.mbAdaptativeFrameField
    );

    if (param.mbAdaptativeFrameField) {
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
    "(direct_8x8_inference_flag): %s blocks level (0x%x).\n",
    (param.direct8x8Interference) ? "8x8" : "4x4",
    param.direct8x8Interference
  );

  if (!param.direct8x8Interference) {
    if (!param.frameMbsOnly) {
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
        param.profileIdc == H264_PROFILE_MAIN
        || param.profileIdc == H264_PROFILE_HIGH
        || param.profileIdc == H264_PROFILE_HIGH_10
        || param.profileIdc == H264_PROFILE_HIGH_422
        || param.profileIdc == H264_PROFILE_HIGH_444_PREDICTIVE
      )
    ) {
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Derivation process mode flag 'direct_8x8_inference_flag' == 0b0 is "
        "not allowed for bitstream level conformance.\n"
      );
    }
  }

  LIBBLU_H264_DEBUG_SPS(
    "  Frame cropping (frame_cropping_flag): %s (0x%x).\n",
    BOOL_STR(param.frameCropping),
    param.frameCropping
  );

  if (param.frameCropping) {
    LIBBLU_H264_DEBUG_SPS(
      "   -> Left (frame_crop_left_offset): %u pixels (0x%x).\n",
      param.frameCropOffset.left * param.CropUnitX,
      param.frameCropOffset.left
    );

    LIBBLU_H264_DEBUG_SPS(
      "   -> Right (frame_crop_right_offset): %u pixels (0x%x).\n",
      param.frameCropOffset.right * param.CropUnitX,
      param.frameCropOffset.right
    );

    LIBBLU_H264_DEBUG_SPS(
      "   -> Top (frame_crop_top_offset): %u pixels (0x%x).\n",
      param.frameCropOffset.top * param.CropUnitY,
      param.frameCropOffset.top
    );

    LIBBLU_H264_DEBUG_SPS(
      "   -> Bottom (frame_crop_bottom_offset): %u pixels (0x%x).\n",
      param.frameCropOffset.bottom * param.CropUnitY,
      param.frameCropOffset.bottom
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
    "(vui_parameters_present_flag): %s (0x%x).\n",
    BOOL_PRESENCE(param.vuiParametersPresent),
    param.vuiParametersPresent
  );

  if (!param.vuiParametersPresent) {
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "According to BD-specs, VUI parameters shall be present in SPS "
      "('vui_parameters_present_flag' == 0b0).\n"
    );
  }

  if (param.vuiParametersPresent) {
    ret = checkH264VuiParametersCompliance(
      param.vuiParameters,
      param,
      handle
    );
    if (ret < 0)
      return -1;

    if (!param.frameMbsOnly && !param.vuiParameters.picStructPresent) {
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "According to BD-specs 'pic_struct' shall be present in "
        "Picture Timing SEI for interlaced video "
        "('pic_struct_present_flag' == 0b0).\n"
      );
    }
  }

  LIBBLU_H264_DEBUG_SPS("  # Target parameters:\n");

  if (FLOAT_COMPARE(param.vuiParameters.timingInfo.frameRate, 23.976))
    handle->constraints.gopMaxLength = 24;
  else if (param.vuiParameters.timingInfo.frameRate == 24)
    handle->constraints.gopMaxLength = 24;
  else if (FLOAT_COMPARE(param.vuiParameters.timingInfo.frameRate, 29.970))
    handle->constraints.gopMaxLength = 30;
  else if (param.vuiParameters.timingInfo.frameRate == 25)
    handle->constraints.gopMaxLength = 25;
  else if (FLOAT_COMPARE(param.vuiParameters.timingInfo.frameRate, 59.940))
    handle->constraints.gopMaxLength = 60;
  else /* 50 */
    handle->constraints.gopMaxLength = 50;

  LIBBLU_H264_DEBUG_SPS(
    "   -> GOP maximal length "
    "(interval between two I pictures): %u pictures.\n",
    handle->constraints.gopMaxLength
  );

  if (param.levelIdc == 41)
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

  assert(param.vuiParametersPresent);

  if (options.secondaryStream)
    ret = checkSecVideoConfiguration(
      param.FrameWidth,
      param.FrameHeight,
      param.vuiParameters.timingInfo.frameRateCode,
      !param.frameMbsOnly
    );
  else
    ret = checkPrimVideoConfiguration(
      param.FrameWidth,
      param.FrameHeight,
      param.vuiParameters.timingInfo.frameRateCode,
      !param.frameMbsOnly
    );

  switch (ret) {
    case CHK_VIDEO_CONF_RET_OK:
      break;

    case CHK_VIDEO_CONF_RET_ILL_VIDEO_SIZE:
      LIBBLU_H264_ERROR_RETURN(
        "Unallowed video definition %ux%u, do not form part of BD-specs.\n",
        param.FrameWidth,
        param.FrameHeight
      );

    case CHK_VIDEO_CONF_RET_ILL_FRAME_RATE:
      LIBBLU_H264_ERROR_RETURN(
        "Frame-rate value %.3f is not allowed for video definition %ux%u "
        "according to BD-specs.\n",
        param.vuiParameters.timingInfo.frameRate,
        param.FrameWidth,
        param.FrameHeight
      );

    case CHK_VIDEO_CONF_RET_ILL_DISP_MODE:
      LIBBLU_H264_ERROR_RETURN(
        "%s display mode is forbidden in the configuration of "
        "video definition %ux%u at frame-rate %.3f "
        "according to BD-specs.\n",
        (param.frameMbsOnly) ? "Progressive" : "Interlaced",
        param.FrameWidth,
        param.FrameHeight,
        param.vuiParameters.timingInfo.frameRate
      );
  }

  return 0;
}

bool constantH264ConstraintFlagsCheck(
  const H264SPSDataParameters first,
  const H264SPSDataParameters second
)
{
  return
    first.constraintFlags.set0
      == second.constraintFlags.set0
    && first.constraintFlags.set1
      == second.constraintFlags.set1
    && first.constraintFlags.set2
      == second.constraintFlags.set2
    && first.constraintFlags.set3
      == second.constraintFlags.set3
    && first.constraintFlags.set4
      == second.constraintFlags.set4
    && first.constraintFlags.set5
      == second.constraintFlags.set5
    && first.constraintFlags.reservedFlags
      == second.constraintFlags.reservedFlags;
  ;
}

bool constantH264HrdParametersCheck(
  const H264HrdParameters first,
  const H264HrdParameters second
)
{
  return
    first.cpbCntMinus1
      == second.cpbCntMinus1
    && first.bitRateScale
      == second.bitRateScale
    && first.cpbSizeScale
      == second.cpbSizeScale
    && first.initialCpbRemovalDelayLengthMinus1
      == second.initialCpbRemovalDelayLengthMinus1
    && first.cpbRemovalDelayLengthMinus1
      == second.cpbRemovalDelayLengthMinus1
    && first.dpbOutputDelayLengthMinus1
      == second.dpbOutputDelayLengthMinus1
    && first.timeOffsetLength
      == second.timeOffsetLength
    && 0 == memcmp(
      first.schedSel,
      second.schedSel,
      (first.cpbCntMinus1 + 1) * sizeof(H264SchedSel)
    )
  ;
}

bool constantH264VuiParametersCheck(
  const H264VuiParameters first,
  const H264VuiParameters second
)
{
  return
    first.aspectRatioInfoPresent
      == second.aspectRatioInfoPresent
    && first.aspectRatioIdc
      == second.aspectRatioIdc
    && first.extendedSAR.width
      == second.extendedSAR.width
    && first.extendedSAR.height
      == second.extendedSAR.height
    && first.overscanInfoPresent
      == second.overscanInfoPresent
    && first.overscanAppropriate
      == second.overscanAppropriate
    && first.videoSignalTypePresent
      == second.videoSignalTypePresent
    && first.videoSignalType.videoFormat
      == second.videoSignalType.videoFormat
    && first.videoSignalType.videoFullRange
      == second.videoSignalType.videoFullRange
    && first.videoSignalType.colourDescPresent
      == second.videoSignalType.colourDescPresent
    && first.videoSignalType.colourDescription.colourPrimaries
      == second.videoSignalType.colourDescription.colourPrimaries
    && first.videoSignalType.colourDescription.transferCharact
      == second.videoSignalType.colourDescription.transferCharact
    && first.videoSignalType.colourDescription.matrixCoeff
      == second.videoSignalType.colourDescription.matrixCoeff
    && first.chromaLocInfoPresent
      == second.chromaLocInfoPresent
    && first.ChromaLocInfo.sampleLocTypeTopField
      == second.ChromaLocInfo.sampleLocTypeTopField
    && first.ChromaLocInfo.sampleLocTypeBottomField
      == second.ChromaLocInfo.sampleLocTypeBottomField
    && first.timingInfoPresent == second.timingInfoPresent
    && (
      !first.timingInfoPresent
      || (
        first.timingInfo.numUnitsInTick
          == second.timingInfo.numUnitsInTick
        && first.timingInfo.timeScale
          == second.timingInfo.timeScale
        && first.timingInfo.fixedFrameRateFlag
          == second.timingInfo.fixedFrameRateFlag
      )
    )
    && first.nalHrdParamPresent
      == second.nalHrdParamPresent
    && (
      !first.nalHrdParamPresent
      || constantH264HrdParametersCheck(
        first.nalHrdParam,
        second.nalHrdParam
      )
    )
    && first.vclHrdParamPresent
      == second.vclHrdParamPresent
    && (
      !first.vclHrdParamPresent
      || constantH264HrdParametersCheck(
        first.vclHrdParam,
        second.vclHrdParam
      )
    )
    && first.lowDelayHrd
      == second.lowDelayHrd
    && first.picStructPresent
      == second.picStructPresent
    && first.bitstreamRestrictionsPresent
      == second.bitstreamRestrictionsPresent
    && (
      !first.bitstreamRestrictionsPresent
      || (
        first.bistreamRestrictions.motionVectorsOverPicBoundaries
          == second.bistreamRestrictions.motionVectorsOverPicBoundaries
        && first.bistreamRestrictions.maxBytesPerPicDenom
          == second.bistreamRestrictions.maxBytesPerPicDenom
        && first.bistreamRestrictions.maxBitsPerPicDenom
          == second.bistreamRestrictions.maxBitsPerPicDenom
        && first.bistreamRestrictions.log2MaxMvLengthHorizontal
          == second.bistreamRestrictions.log2MaxMvLengthHorizontal
        && first.bistreamRestrictions.log2MaxMvLengthVertical
          == second.bistreamRestrictions.log2MaxMvLengthVertical
        && first.bistreamRestrictions.maxNumReorderFrames
          == second.bistreamRestrictions.maxNumReorderFrames
        && first.bistreamRestrictions.maxDecFrameBuffering
          == second.bistreamRestrictions.maxDecFrameBuffering
      )
    )
  ;
}

bool constantH264SequenceParametersSetCheck(
  const H264SPSDataParameters first,
  const H264SPSDataParameters second
)
{
  return
    first.profileIdc
      == second.profileIdc
    && constantH264ConstraintFlagsCheck(
      first,
      second
    )
    && first.levelIdc
      == second.levelIdc
    /* Only one seq_parameter_set_id allowed : */
    && first.seqParametersSetId
      == second.seqParametersSetId

    && first.chromaFormat
      == second.chromaFormat
    && first.separateColourPlaneFlag444
      == second.separateColourPlaneFlag444

    && first.bitDepthLumaMinus8
      == second.bitDepthLumaMinus8
    && first.bitDepthChromaMinus8
      == second.bitDepthChromaMinus8
    && first.qpprimeYZeroTransformBypass
      == second.qpprimeYZeroTransformBypass

    && first.seqScalingMatrixPresent
      == second.seqScalingMatrixPresent
    && (
      !first.seqScalingMatrixPresent
      || (
        0 == memcmp(
          &first.seqScalingMatrix,
          &second.seqScalingMatrix,
          sizeof(H264SeqScalingMatrix)
        )
      )
    )

    && first.log2MaxFrameNumMinus4
      == second.log2MaxFrameNumMinus4

    && first.picOrderCntType
      == second.picOrderCntType
    && (
      /* Check pic_order_cnt_type == 0 */
      first.picOrderCntType != 0
      || (
        first.log2MaxPicOrderCntLsbMinus4
          == second.log2MaxPicOrderCntLsbMinus4
      )
    )
    && (
      /* Check pic_order_cnt_type == 1 */
      first.picOrderCntType != 1
      || (
        first.deltaPicOrderAlwaysZero
          == second.deltaPicOrderAlwaysZero
        && first.offsetForNonRefPic
          == second.offsetForNonRefPic
        && first.offsetForTopToBottomField
          == second.offsetForTopToBottomField
        && first.numRefFramesInPicOrderCntCycle
          == second.numRefFramesInPicOrderCntCycle
        && (
          first.numRefFramesInPicOrderCntCycle == 0
          || (
            0 == memcmp(
              first.offsetForRefFrame,
              second.offsetForRefFrame,
              first.numRefFramesInPicOrderCntCycle * sizeof(long)
            )
          )
        )
      )
    )

    && first.maxNumRefFrames
      == second.maxNumRefFrames
    && first.gapsInFrameNumValueAllowed
      == second.gapsInFrameNumValueAllowed

    && first.picWidthInMbsMinus1
      == second.picWidthInMbsMinus1
    && first.picHeightInMapUnitsMinus1
      == second.picHeightInMapUnitsMinus1
    && first.frameMbsOnly
      == second.frameMbsOnly

    && first.mbAdaptativeFrameField
      == second.mbAdaptativeFrameField
    && first.direct8x8Interference
      == second.direct8x8Interference

    && first.frameCropping
      == second.frameCropping
    && first.frameCropOffset.left
      == second.frameCropOffset.left
    && first.frameCropOffset.right
      == second.frameCropOffset.right
    && first.frameCropOffset.top
      == second.frameCropOffset.top
    && first.frameCropOffset.bottom
      == second.frameCropOffset.bottom

    && first.vuiParametersPresent == second.vuiParametersPresent
    && (
      !first.vuiParametersPresent
      || constantH264VuiParametersCheck(
        first.vuiParameters,
        second.vuiParameters
      )
    )
  ;
}

int checkH264SequenceParametersSetChangeCompliance(
  const H264SPSDataParameters first,
  const H264SPSDataParameters second
)
{
  if (
    first.profileIdc
      == second.profileIdc
    && constantH264ConstraintFlagsCheck(
      first,
      second
    )
    && first.levelIdc
      == second.levelIdc
    /* Only one seq_parameter_set_id allowed : */
    && first.seqParametersSetId
      == second.seqParametersSetId

    && first.chromaFormat
      == second.chromaFormat
    && first.separateColourPlaneFlag444
      == second.separateColourPlaneFlag444

    && first.bitDepthLumaMinus8
      == second.bitDepthLumaMinus8
    && first.bitDepthChromaMinus8
      == second.bitDepthChromaMinus8

    && first.log2MaxFrameNumMinus4
      == second.log2MaxFrameNumMinus4

    && first.picOrderCntType
      == second.picOrderCntType
    && (
      /* Check pic_order_cnt_type == 0 */
      first.picOrderCntType != 0
      || (
        first.log2MaxPicOrderCntLsbMinus4
          == second.log2MaxPicOrderCntLsbMinus4
      )
    )
    && (
      /* Check pic_order_cnt_type == 1 */
      first.picOrderCntType != 1
      || (
        first.deltaPicOrderAlwaysZero
          == second.deltaPicOrderAlwaysZero
        && first.offsetForNonRefPic
          == second.offsetForNonRefPic
        && first.offsetForTopToBottomField
          == second.offsetForTopToBottomField
        && first.numRefFramesInPicOrderCntCycle
          == second.numRefFramesInPicOrderCntCycle
        && (
          first.numRefFramesInPicOrderCntCycle == 0
          || (
            0 == memcmp(
              first.offsetForRefFrame,
              second.offsetForRefFrame,
              first.numRefFramesInPicOrderCntCycle * sizeof(long)
            )
          )
        )
      )
    )

    && first.maxNumRefFrames
      == second.maxNumRefFrames
    && first.gapsInFrameNumValueAllowed
      == second.gapsInFrameNumValueAllowed

    && first.picWidthInMbsMinus1
      == second.picWidthInMbsMinus1
    && first.picHeightInMapUnitsMinus1
      == second.picHeightInMapUnitsMinus1
    && first.frameMbsOnly
      == second.frameMbsOnly

    && first.mbAdaptativeFrameField
      == second.mbAdaptativeFrameField
    && first.direct8x8Interference
      == second.direct8x8Interference

    && first.frameCropping
      == second.frameCropping
    && first.frameCropOffset.left
      == second.frameCropOffset.left
    && first.frameCropOffset.right
      == second.frameCropOffset.right
    && first.frameCropOffset.top
      == second.frameCropOffset.top
    && first.frameCropOffset.bottom
      == second.frameCropOffset.bottom

    && first.vuiParametersPresent
      == second.vuiParametersPresent
    && (
      !first.vuiParametersPresent
      || constantH264VuiParametersCheck(
        first.vuiParameters,
        second.vuiParameters
      )
    )
  )
    return 0;
  return -1;
}

int checkH264PicParametersSetCompliance(
  const H264PicParametersSetParameters param,
  const H264ParametersHandlerPtr handle
)
{
  unsigned i, j;

  bool validEntropyCodingMode;

  LIBBLU_H264_DEBUG_PPS(
    "  Picture Parameter Set id (pic_parameter_set_id): %u (0x%x).\n",
    param.picParametersSetId,
    param.picParametersSetId
  );

  if (H264_MAX_PPS <= param.picParametersSetId)
    LIBBLU_H264_ERROR_RETURN(
      "PPS pic_parameter_set_id overflow ('pic_parameter_set_id' == %u).\n",
      param.picParametersSetId
    );

  if (!handle->picParametersSetIdsPresent[param.picParametersSetId]) {
    /* New PPS entry, check for maximum of allowed PPS */
    if (H264_MAX_ALLOWED_PPS <= handle->picParametersSetIdsPresentNb)
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Too many different PPS present (shall not exceed %d).\n",
        H264_MAX_ALLOWED_PPS
      );
  }

  LIBBLU_H264_DEBUG_PPS(
    "  Active Sequence Parameter Set id (seq_parameter_set_id): %u (0x%x).\n",
    param.seqParametersSetId, param.seqParametersSetId
  );

  if (param.seqParametersSetId != 0x0)
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "Unallowed multiple-PPS definition.\n"
    );

  LIBBLU_H264_DEBUG_PPS(
    "  Entropy coding mode (entropy_coding_mode_flag): %s (0b%x).\n",
    (param.entropyCodingMode) ? "CABAC" : "CAVLC",
    param.entropyCodingMode
  );

  if (
    CHECK_H264_WARNING_FLAG(handle, useOfLowEfficientCavlc)
    && !param.entropyCodingMode
  ) {
    LIBBLU_WARNING(
      "Usage of a lower efficient entropy coding (CAVLC), "
      "CABAC is more suitable for Blu-ray Disc authoring.\n"
    );
    MARK_H264_WARNING_FLAG(handle, useOfLowEfficientCavlc);
  }

  validEntropyCodingMode = H264_IS_VALID_ENTROPY_CODING_MODE_RESTR(
    handle->constraints.restrictedEntropyCodingMode,
    param.entropyCodingMode
  );

  if (!validEntropyCodingMode) {
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "%s entropy coding mode is not allowed for bitstream profile "
      "conformance, 'entropy_coding_mode_flag' shall be fixed to 0b%x.\n",
      (param.entropyCodingMode) ? "CABAC" : "CAVLC",
      param.entropyCodingMode
    );
  }

  LIBBLU_H264_DEBUG_PPS(
    "  Picture field order syntax present in slice headers "
    "(bottom_field_pic_order_in_frame_present_flag): %s (0x%x).\n",
    BOOL_INFO(param.bottomFieldPicOrderInFramePresent),
    param.bottomFieldPicOrderInFramePresent
  );

  LIBBLU_H264_DEBUG_PPS(
    "  Number of slice groups (num_slice_groups_minus1): %u (0x%x).\n",
    param.nbSliceGroups,
    param.nbSliceGroups - 1
  );

  if (
    handle->constraints.maxAllowedNumSliceGroups
    < param.nbSliceGroups - 1
  )
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "bitstream profile does not allow more than %u slice groups "
      "('num_slice_groups_minus1' == %u).\n",
      handle->constraints.maxAllowedNumSliceGroups,
      param.nbSliceGroups - 1
    );

  LIBBLU_H264_DEBUG_PPS(
    "  Default maximum number of reference index for reference picture list 0 "
    "(num_ref_idx_l0_default_active_minus1): %u index(es) (0x%x).\n",
    param.numRefIdxl0DefaultActiveMinus1,
    param.numRefIdxl0DefaultActiveMinus1 - 1
  );

  /* Already checked at parsing */

  LIBBLU_H264_DEBUG_PPS(
    "  Default maximum number of reference index for reference picture list 1 "
    "(num_ref_idx_l1_default_active_minus1): %u index(es) (0x%x).\n",
    param.numRefIdxl1DefaultActiveMinus1,
    param.numRefIdxl1DefaultActiveMinus1 - 1
  );

  /* Already checked at parsing */

  LIBBLU_H264_DEBUG_PPS(
    "  Weighted prediction mode for P slices "
    "(weighted_pred_flag): %s (0x%x).\n",
    (param.weightedPred) ? "Explicit" : "Default",
    param.weightedPred
  );

  if (
    param.weightedPred
    && handle->constraints.forbiddenWeightedPredModesUse
  )
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "Weighted prediction mode is not allowed for bitstream "
      "profile conformance.\n"
    );

  LIBBLU_H264_DEBUG_PPS(
    "  Weighted prediction mode for B slices "
    "(weighted_bipred_idc): %s (0x%x).\n",
    h264WeightedBipredIdcStr(param.weightedBipredIdc),
    param.weightedBipredIdc
  );

  if (H264_WEIGHTED_PRED_B_SLICES_RESERVED <= param.weightedBipredIdc) {
    LIBBLU_H264_ERROR_RETURN(
      "Reserved PPS 'weighted_bipred_idc' == %u in use.\n",
      param.weightedBipredIdc
    );
  }

  if (
    param.weightedBipredIdc != H264_WEIGHTED_PRED_B_SLICES_DEFAULT
    && handle->constraints.forbiddenWeightedPredModesUse
  )
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "Weighted prediction mode is not allowed for bitstream profile "
      "conformance.\n"
    );


  LIBBLU_H264_DEBUG_PPS(
    "  Picture initial SliceQPy for slices "
    "(pic_init_qp_minus26): %d (0x%x).\n",
    param.picInitQpMinus26 + 26,
    param.picInitQpMinus26
  );

  if (param.picInitQpMinus26 < -26 || 25 < param.picInitQpMinus26)
    LIBBLU_ERROR_RETURN(
      "Reserved 'pic_init_qp_minus26' == %d in use.\n",
      param.picInitQpMinus26
    );

  LIBBLU_H264_DEBUG_PPS(
    "  Picture initial SliceQSy for SP and SI slices "
    "(pic_init_qs_minus26): %d (0x%x).\n",
    param.picInitQsMinus26 + 26,
    param.picInitQsMinus26
  );

  if (param.picInitQsMinus26 < -26 || 25 < param.picInitQsMinus26)
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "Reserved 'pic_init_qs_minus26' == %d in use.\n",
      param.picInitQsMinus26
    );

  LIBBLU_H264_DEBUG_PPS(
    "  Chroma QPy / QSy offset for Cb chroma component "
    "(chroma_qp_index_offset): %d (0x%x).\n",
    param.chromaQpIndexOff,
    param.chromaQpIndexOff
  );

  if (param.chromaQpIndexOff < -12 || 12 < param.chromaQpIndexOff)
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "Reserved 'chroma_qp_index_offset' == %d in use.\n",
      param.chromaQpIndexOff
    );


  LIBBLU_H264_DEBUG_PPS(
    "  Deblocking filter parameters "
    "(deblocking_filter_control_present_flag): %s (0b%x).\n",
    BOOL_PRESENCE(param.deblockingFilterControlPresent),
    param.deblockingFilterControlPresent
  );

  LIBBLU_H264_DEBUG_PPS(
    "  Constrained intra prediction "
    "(constrained_intra_pred_flag): %s (0b%x).\n",
    (param.contraintIntraPred) ?
      "Constrained to I or SI macroblocks references"
    :
      "Unconstrained",
    param.contraintIntraPred
  );

  if (
    CHECK_H264_WARNING_FLAG(handle, useOfLowEfficientConstaintIntraPred)
    && param.contraintIntraPred
  ) {
    LIBBLU_WARNING(
      "Usage of lower efficient constraint macroblock intra prediction mode, "
      "which may reduce compression efficiency.\n"
    );
    MARK_H264_WARNING_FLAG(handle, useOfLowEfficientConstaintIntraPred);
  }

  LIBBLU_H264_DEBUG_PPS(
    "  Redundant picture counter syntax in B slices "
    "(redundant_pic_cnt_present_flag): %s (0b%x).\n",
    BOOL_PRESENCE(param.redundantPicCntPresent),
    param.redundantPicCntPresent
  );

  if (
    param.redundantPicCntPresent
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
      "(transform_8x8_mode_flag): %s (0b%x).\n",
      BOOL_INFO(param.transform8x8Mode),
      param.transform8x8Mode
    );

    LIBBLU_H264_DEBUG_PPS(
      "   Picture scaling matrix "
      "(pic_scaling_matrix_present_flag): %s (0b%x).\n",
      BOOL_PRESENCE(param.picScalingMatrixPresent),
      param.picScalingMatrixPresent
    );

    if (param.picScalingMatrixPresent) {
      for (i = 0; i < param.nbScalingMatrix; i++) {
        LIBBLU_H264_DEBUG_PPS(
          "    -> Picture scaling list %u "
          "(pic_scaling_list_present_flag[%u]): %s (0b%x).\n",
          i, i,
          BOOL_PRESENCE(param.picScalingListPresent[i]),
          param.picScalingListPresent[i]
        );

        if (param.picScalingListPresent[i]) {
          bool useDefScalingMatrix;
          bool isMatrix4x4;
          unsigned nbCoeffs;

          isMatrix4x4 = (i < 6);
          useDefScalingMatrix =
            (isMatrix4x4) ?
              param.useDefaultScalingMatrix4x4[i]
            :
              param.useDefaultScalingMatrix8x8[i-6]
          ;

          LIBBLU_H264_DEBUG_PPS(
            "     -> Use default scaling matrix: %s (0b%x).\n",
            BOOL_INFO(useDefScalingMatrix),
            useDefScalingMatrix
          );

          nbCoeffs = ((isMatrix4x4) ? 16 : 64);

          for (j = 0; j < nbCoeffs; j++)
            LIBBLU_H264_DEBUG_PPS(
              "     -> Scaling list %s %u, index %u: %" PRIu8 ".\n",
              (isMatrix4x4) ? "4x4" : "8x8", i, j,
              (isMatrix4x4) ?
                param.scalingList4x4[i][j]
              :
                param.scalingList8x8[i-6][j]
            );
        }
      }
    }

    LIBBLU_H264_DEBUG_PPS(
      "   Chroma QPy / QSy offset for Cr chroma component "
      "(second_chroma_qp_index_offset): %d (0x%x).\n",
      param.secondChromaQpIndexOff,
      param.secondChromaQpIndexOff
    );

    if (
      param.secondChromaQpIndexOff < -12
      || 12 < param.secondChromaQpIndexOff
    )
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Reserved 'second_chroma_qp_index_offset' == %d in use.\n",
        param.secondChromaQpIndexOff
      );
  }

  return 0; /* OK */
}

bool constantH264PicParametersSetCheck(
  const H264PicParametersSetParameters first,
  const H264PicParametersSetParameters second
)
{
  return
    first.picParametersSetId
      == second.picParametersSetId
    && first.seqParametersSetId
      == second.seqParametersSetId

    && first.entropyCodingMode
      == second.entropyCodingMode
    && first.bottomFieldPicOrderInFramePresent
      == second.bottomFieldPicOrderInFramePresent

    && first.nbSliceGroups
      == second.nbSliceGroups

    && first.numRefIdxl0DefaultActiveMinus1
      == second.numRefIdxl0DefaultActiveMinus1
    && first.numRefIdxl1DefaultActiveMinus1
      == second.numRefIdxl1DefaultActiveMinus1

    && first.weightedPred
      == second.weightedPred
    && first.weightedBipredIdc
      == second.weightedBipredIdc

    && first.picInitQpMinus26
      == second.picInitQpMinus26
    && first.picInitQsMinus26
      == second.picInitQsMinus26

    && first.deblockingFilterControlPresent
      == second.deblockingFilterControlPresent
    && first.contraintIntraPred
      == second.contraintIntraPred
    && first.redundantPicCntPresent
      == second.redundantPicCntPresent
    && first.transform8x8Mode
      == second.transform8x8Mode
  ;
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
  if (first.entropyCodingMode != second.entropyCodingMode)
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
    || !handle->sequenceParametersSet.data.vuiParametersPresent
  )
    LIBBLU_H264_ERROR_RETURN(
      "Missing mandatory VUI parameters, Buffering Period SEI message "
      "shouldn't be present."
    );

  vuiParam = &handle->sequenceParametersSet.data.vuiParameters;

  if (
    !vuiParam->nalHrdParamPresent
    && !vuiParam->vclHrdParamPresent
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
    "   Linked sequence parameter set (seq_parameter_set_id): %u (0x%x).\n",
    param.seqParametersSetId,
    param.seqParametersSetId
  );

  if (param.seqParametersSetId != 0x0)
    LIBBLU_H264_ERROR_RETURN(
      "Unallowed multiple SPS definition use "
      "('seq_parameter_set_id' == %u in Buffering Period SEI msg).\n",
      param.seqParametersSetId
    );

  for (type = 0; type < 2; type++) {
    present = (type) ?
      vuiParam->vclHrdParamPresent
    :
      vuiParam->nalHrdParamPresent
    ;
    if (!present)
      continue;

    hrdParam = (type) ? &vuiParam->vclHrdParam : &vuiParam->nalHrdParam;
    hrdBufPerd = (type) ? param.vclHrdParam : param.nalHrdParam;

    LIBBLU_H264_DEBUG_SEI(
      "   %s HRD - Coded Picture Buffer Parameters:\n",
      (type) ? "VCL" : "NAL"
    );

    for (
      SchedSelIdx = 0;
      SchedSelIdx <= hrdParam->cpbCntMinus1;
      SchedSelIdx++
    ) {
      LIBBLU_H264_DEBUG_SEI("    - SchedSelIdx %u:\n", SchedSelIdx);

      LIBBLU_H264_DEBUG_SEI(
        "     -> Initial CPB removal delay "
        "(initial_cpb_removal_delay[%u]): "
        "%" PRIu32 " (%" PRIu32 " ms, 0x%" PRIx32 ").\n",
        SchedSelIdx,
        hrdBufPerd[SchedSelIdx].initialCpbRemovalDelay,
        hrdBufPerd[SchedSelIdx].initialCpbRemovalDelay / 90,
        hrdBufPerd[SchedSelIdx].initialCpbRemovalDelay
      );

      if (!hrdBufPerd[SchedSelIdx].initialCpbRemovalDelay)
        LIBBLU_H264_ERROR_RETURN(
          "Forbidden zero initial CPB removal delay "
          "(initial_cpb_removal_delay).\n"
        );

      maxDelay = H264_90KHZ_CLOCK * (
        (double) hrdParam->schedSel[SchedSelIdx].cpbSize /
        (double) hrdParam->schedSel[SchedSelIdx].bitRate
      );

      LIBBLU_H264_DEBUG_SEI(
        "      -> Max allowed delay: %.0f ms.\n",
        maxDelay
      );

      if (
        maxDelay
        < (double) hrdBufPerd[SchedSelIdx].initialCpbRemovalDelay
      )
        LIBBLU_H264_ERROR_RETURN(
          "NAL HRD CPB Removal delay exceed maximal delay (%.2f < %.2f).\n",
          maxDelay,
          (double) hrdBufPerd[SchedSelIdx].initialCpbRemovalDelay
        );

      LIBBLU_H264_DEBUG_SEI(
        "     -> Initial CPB removal delay offset "
        "(initial_cpb_removal_delay_offset[%u]): "
        "%" PRIu32 " (%" PRIu32 " ms, 0x%" PRIx32 ").\n",
        SchedSelIdx,
        hrdBufPerd[SchedSelIdx].initialCpbRemovalDelayOff,
        hrdBufPerd[SchedSelIdx].initialCpbRemovalDelayOff / 90,
        hrdBufPerd[SchedSelIdx].initialCpbRemovalDelayOff
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
    && handle->sequenceParametersSet.data.vuiParametersPresent
  );

  vuiParam = &handle->sequenceParametersSet.data.vuiParameters;

  if (vuiParam->nalHrdParamPresent) {
    ret = memcmp(
      first.nalHrdParam,
      second.nalHrdParam,
      (vuiParam->nalHrdParam.cpbCntMinus1 + 1)
        * sizeof(H264HrdBufferingPeriodParameters)
    );
    if (ret != 0)
      return false;
  }

  if (vuiParam->vclHrdParamPresent) {
    ret = memcmp(
      first.vclHrdParam,
      second.vclHrdParam,
      (vuiParam->vclHrdParam.cpbCntMinus1 + 1)
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

  vuiParam = &handle->sequenceParametersSet.data.vuiParameters;

  /* Check second parameters mandatory parameters changeless : */
  if (!ALLOW_BUFFERING_PERIOD_CHANGE && vuiParam->nalHrdParamPresent) {
    for (
      SchedSelIdx = 0;
      SchedSelIdx <= vuiParam->nalHrdParam.cpbCntMinus1;
      SchedSelIdx++
    ) {
      if (
        first.nalHrdParam[SchedSelIdx].initialCpbRemovalDelay
          + first.nalHrdParam[SchedSelIdx].initialCpbRemovalDelayOff
        != second.nalHrdParam[SchedSelIdx].initialCpbRemovalDelay
          + second.nalHrdParam[SchedSelIdx].initialCpbRemovalDelayOff
      )
        LIBBLU_ERROR_RETURN(
          "Fluctuating NAL HRD initial CPB removal delay "
          "(initial_cpb_removal_delay/initial_cpb_removal_delay_offset).\n"
        );
    }
  }

  if (!ALLOW_BUFFERING_PERIOD_CHANGE && vuiParam->vclHrdParamPresent) {
    for (
      SchedSelIdx = 0;
      SchedSelIdx <= vuiParam->vclHrdParam.cpbCntMinus1;
      SchedSelIdx++
    ) {
      if (
        first.vclHrdParam[SchedSelIdx].initialCpbRemovalDelay
          + first.vclHrdParam[SchedSelIdx].initialCpbRemovalDelayOff
        != second.vclHrdParam[SchedSelIdx].initialCpbRemovalDelay
          + second.vclHrdParam[SchedSelIdx].initialCpbRemovalDelayOff
      )
        LIBBLU_ERROR_RETURN(
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
    || !handle->sequenceParametersSet.data.vuiParametersPresent
  )
    LIBBLU_ERROR_RETURN(
      "Missing mandatory VUI parameters, Pic Timing SEI message "
      "shouldn't be present.\n"
    );

  vuiParam = &handle->sequenceParametersSet.data.vuiParameters;

  if (
    !vuiParam->cpbDpbDelaysPresent
    && !vuiParam->picStructPresent
  )
    LIBBLU_ERROR_RETURN(
      "'CpbDpbDelaysPresentFlag' and 'pic_struct_present_flag' are equal to "
      "0b0, so Pic Timing SEI message shouldn't be present.\n"
    );

  if (vuiParam->cpbDpbDelaysPresent) {
    LIBBLU_H264_DEBUG_SEI(
      "   Buffering parameters "
      "('CpbDpbDelaysPresentFlag' == 0b1 in SPS VUI):\n"
    );

    LIBBLU_H264_DEBUG_SEI(
      "    Coded Picture Buffer removal delay "
      "(cpb_removal_delay): %" PRIu32 " ticks (0x%" PRIx32 ").\n",
      param.cpbRemovalDelay,
      param.cpbRemovalDelay
    );

    LIBBLU_H264_DEBUG_SEI(
      "    Decoded Picture Buffer output delay (dpb_output_delay): "
      "%" PRIu32 " ticks (0x%" PRIx32 ").\n",
      param.dpbOutputDelay,
      param.dpbOutputDelay
    );
  }

  if (vuiParam->picStructPresent) {
    LIBBLU_H264_DEBUG_SEI(
      "   Picture structure parameters "
      "(pic_struct_present_flag == 0b1 in SPS VUI):\n"
    );

    LIBBLU_H264_DEBUG_SEI(
      "    Picture structure (pic_struct): %s (0x%x).\n",
      h264PicStructValueStr(param.picStruct),
      param.picStruct
    );

    if (H264_PIC_STRUCT_TRIPLED_FRAME < param.picStruct)
      LIBBLU_H264_ERROR_RETURN(
        "Reserved 'pic_struct' == %u in use.\n",
        param.picStruct
      );

    LIBBLU_H264_DEBUG_SEI(
      "    Pictures clock timestamps (clock_timestamp_flag):\n"
    );
    for (i = 0; i < param.numClockTS; i++) {
      LIBBLU_H264_DEBUG_SEI(
        "     - Picture %u: %s (0x%x).\n",
        i,
        BOOL_PRESENCE(param.clockTimestampPresent[i]),
        param.clockTimestampPresent[i]
      );

      if (param.clockTimestampPresent[i]) {

        LIBBLU_H264_DEBUG_SEI(
          "      -> Picture scan type (ct_type): %s (0x%x).\n",
          h264CtTypeValueStr(param.clockTimestampParam[i].ctType),
          param.clockTimestampParam[i].ctType
        );

        if (H264_CT_TYPE_RESERVED <= param.clockTimestampParam[i].ctType)
          LIBBLU_H264_ERROR_RETURN(
            "Reserved 'ct_type' == %d in use.\n",
            param.clockTimestampParam[i].ctType
          );

        LIBBLU_H264_DEBUG_SEI(
          "      -> Number of Units In Tick field based "
          "(nuit_field_based_flag): %s (0b%x).\n",
          BOOL_INFO(param.clockTimestampParam[i].nuitFieldBased),
          param.clockTimestampParam[i].nuitFieldBased
        );

        LIBBLU_H264_DEBUG_SEI(
          "      -> Timestamp counting type (counting_type): %s (0x%x).\n",
          h264CountingTypeValueStr(param.clockTimestampParam[i].countingType),
          param.clockTimestampParam[i].countingType
        );

        LIBBLU_H264_DEBUG_SEI(
          "      -> Timestamp format (full_timestamp_flag): %s (0b%x).\n",
          (param.ClockTimestamp[i].fullTimestamp) ?
            "Full timestamp"
          :
            "Compact timestamp",
          param.ClockTimestamp[i].fullTimestamp
        );

        LIBBLU_H264_DEBUG_SEI(
          "      -> Discontinuity marker (discontinuity_flag): %s (0b%x).\n",
          BOOL_INFO(param.ClockTimestamp[i].discontinuity),
          param.ClockTimestamp[i].discontinuity
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
          "to counting_type (cnt_dropped_flag): %s (0b%x).\n",
          BOOL_INFO(param.ClockTimestamp[i].cntDropped),
          param.ClockTimestamp[i].cntDropped
        );

        LIBBLU_H264_DEBUG_SEI(
          "      -> nFrames value (n_frames): %u frame(s) (0x%x).\n",
          param.ClockTimestamp[i].nFrames,
          param.ClockTimestamp[i].nFrames
        );

        if (vuiParam->timingInfoPresent) {
          LIBBLU_H264_DEBUG_SEI(
            "      -> MaxFPS expected (From SPS VUI): %u frames.\n",
            vuiParam->timingInfo.maxFPS
          );

          if (vuiParam->timingInfo.maxFPS <= param.ClockTimestamp[i].nFrames)
            LIBBLU_H264_ERROR_RETURN(
              "Forbidden 'n_frames' == %u in use, shall be less than value "
              "defined by equation D-2 (equal to %u) in Rec. ITU-T H.264.\n",
              vuiParam->timingInfo.maxFPS
            );
        }

        LIBBLU_H264_DEBUG_SEI(
          "      -> Timestamp: %02u:%02u:%02u.%03u.\n",
          param.ClockTimestamp[i].hoursValue,
          param.ClockTimestamp[i].minutesValue,
          param.ClockTimestamp[i].secondsValue,
          param.ClockTimestamp[i].nFrames
        );

        if (0 < param.ClockTimestamp[i].timeOffset)
          LIBBLU_H264_DEBUG_SEI(
            "      -> Time offset (time_offset): "
            "%" PRIu32 " (0x%" PRIx32 ").\n",
            param.ClockTimestamp[i].timeOffset,
            param.ClockTimestamp[i].timeOffset
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
    "(recovery_frame_cnt): %u frame(s) (0x%x).\n",
    param.recoveryFrameCnt,
    param.recoveryFrameCnt
  );

  LIBBLU_H264_DEBUG_SEI(
    "    -> Max possible frame number (MaxFrameNum from SPS): %u.\n",
    handle->sequenceParametersSet.data.MaxFrameNum
  );

  if (
    handle->sequenceParametersSet.data.MaxFrameNum
    <= param.recoveryFrameCnt
  )
    LIBBLU_H264_ERROR_RETURN(
      "Too high 'recovery_frame_cnt' == %u in Recovery Point SEI message, "
      "shall be less than SPS 'MaxFrameNum'.\n",
      param.recoveryFrameCnt
    );

  LIBBLU_H264_DEBUG_SEI(
    "   Exact matching recovery point "
    "(exact_match_flag): %s (0b%x).\n",
    BOOL_INFO(param.exactMatch),
    param.exactMatch
  );

  LIBBLU_H264_DEBUG_SEI(
    "   Presence of a broken reference link at recovery point "
    "(broken_link_flag): %s (0b%x).\n",
    BOOL_INFO(param.brokenLink),
    param.brokenLink
  );

  if (param.brokenLink)
    LIBBLU_ERROR_RETURN(
      "Unallowed broken link ('broken_link_flag' == 0b1) signaled by "
      "Recovery Point SEI message.\n"
    );

  LIBBLU_H264_DEBUG_SEI(
    "   Changing slice group indicator (changing_slice_group_idc): "
    "0x%" PRIx8 ".\n", param.changingSliceGroupIdc
  );
  switch (param.changingSliceGroupIdc) {
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

      LIBBLU_ERROR_RETURN(
        "Reserved 'changing_slice_group_idc' == %" PRIu8 " in use.\n",
        param.changingSliceGroupIdc
      );
  }

  if (param.changingSliceGroupIdc != 0x0) {
    if (!handle->constraints.maxAllowedNumSliceGroups) {
      LIBBLU_H264_ERROR_RETURN(
        "'changing_slice_group_idc' == %" PRIu8 " should not indicate "
        "presence of multiple slice groups.\n",
        param.changingSliceGroupIdc
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

bool constantH264SeiRecoveryPointCheck(
  const H264SeiRecoveryPoint first,
  const H264SeiRecoveryPoint second
)
{
  return
    first.recoveryFrameCnt
      == second.recoveryFrameCnt
    && first.exactMatch
      == second.exactMatch
    && first.brokenLink
      == second.brokenLink
    && first.changingSliceGroupIdc
      == second.changingSliceGroupIdc
  ;
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
    !H264_SLICE_IS_TYPE_I(sliceHeaderParam.sliceType)
    && !H264_SLICE_IS_TYPE_SI(sliceHeaderParam.sliceType)
  ) {
    LIBBLU_H264_DEBUG_SEI(
      "  ref_pic_list_modification() - Reference picture list modification:\n"
    );

    LIBBLU_H264_DEBUG_SEI(
      "   Modification of reference picture list 0 "
      "(ref_pic_list_modification_flag_l0): %s (0b%x).\n",
      BOOL_PRESENCE(param.refPicListModificationFlagl0),
      param.refPicListModificationFlagl0
    );

    if (param.refPicListModificationFlagl0) {
      for (i = 0; i < param.nbRefPicListModificationl0; i++) {
        LIBBLU_H264_DEBUG_SEI("    -> Picture number %u:\n", i);

        LIBBLU_H264_DEBUG_SEI(
          "     Modification indicator "
          "(modification_of_pic_nums_idc): %s (0x%x).\n",
          h264ModificationOfPicNumsIdcValueStr(
            param.refPicListModificationl0[i].modOfPicNumsIdc
          ),
          param.refPicListModificationl0[i].modOfPicNumsIdc
        );

        if (param.refPicListModificationl0[i].modOfPicNumsIdc <= 1)
          LIBBLU_H264_DEBUG_SEI(
            "     Difference value "
            "(abs_diff_pic_num_minus1): %" PRIu32 " (0x%" PRIx32 ").\n",
            param.refPicListModificationl0[i].absDiffPicNumMinus1,
            param.refPicListModificationl0[i].absDiffPicNumMinus1
          );

        if (param.refPicListModificationl0[i].modOfPicNumsIdc == 2)
          LIBBLU_H264_DEBUG_SEI(
            "     Reference long-term picture number "
            "(long_term_pic_num): %" PRIu32 " (0x%" PRIx32 ").\n",
            param.refPicListModificationl0[i].longTermPicNum,
            param.refPicListModificationl0[i].longTermPicNum
          );
      }
    }
  }

  if (H264_SLICE_IS_TYPE_B(sliceHeaderParam.sliceType)) {
    LIBBLU_H264_DEBUG_SEI(
      "   Modification of reference picture list 1 "
      "(ref_pic_list_modification_flag_l1): %s (0b%x).\n",
      BOOL_PRESENCE(param.refPicListModificationFlagl1),
      param.refPicListModificationFlagl1
    );

    if (param.refPicListModificationFlagl1) {
      for (i = 0; i < param.nbRefPicListModificationl1; i++) {
        LIBBLU_H264_DEBUG_SEI("    -> Picture number %u:\n", i);

        LIBBLU_H264_DEBUG_SEI(
          "     Modification indicator "
          "(modification_of_pic_nums_idc): %s (0x%x).\n",
          h264ModificationOfPicNumsIdcValueStr(
            param.refPicListModificationl1[i].modOfPicNumsIdc
          ),
          param.refPicListModificationl1[i].modOfPicNumsIdc
        );

        if (param.refPicListModificationl1[i].modOfPicNumsIdc <= 1)
          LIBBLU_H264_DEBUG_SEI(
            "     Difference value "
            "(abs_diff_pic_num_minus1): %" PRIu32 " (0x%" PRIx32 ").\n",
            param.refPicListModificationl1[i].absDiffPicNumMinus1,
            param.refPicListModificationl1[i].absDiffPicNumMinus1
          );

        if (param.refPicListModificationl1[i].modOfPicNumsIdc == 2)
          LIBBLU_H264_DEBUG_SEI(
            "     Reference long-term picture number "
            "(long_term_pic_num): %" PRIu32 " (0x%" PRIx32 ").\n",
            param.refPicListModificationl1[i].longTermPicNum,
            param.refPicListModificationl1[i].longTermPicNum
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


  LIBBLU_H264_DEBUG_SEI(
    "  pred_weight_table() - Prediction weight table:\n"
  );

  LIBBLU_H264_DEBUG_SEI(
    "   Luma weighting factors denominator "
    "(luma_log2_weight_denom): %u (0x%x).\n",
    1 << param.lumaLog2WeightDenom,
    param.lumaLog2WeightDenom
  );

  if (7 < param.lumaLog2WeightDenom)
    LIBBLU_ERROR_RETURN(
      "Out of range 'luma_log2_weight_denom' == %u in use.\n",
      param.lumaLog2WeightDenom
    );

  LIBBLU_H264_DEBUG_SEI(
    "   Chroma weighting factors denominator "
    "(chroma_log2_weight_denom): %u (0x%x).\n",
    1 << param.chromaLog2WeightDenom,
    param.chromaLog2WeightDenom
  );

  if (7 < param.chromaLog2WeightDenom)
    LIBBLU_ERROR_RETURN(
      "Out of range 'chroma_log2_weight_denom' == %u in use.\n",
      param.chromaLog2WeightDenom
    );

  LIBBLU_H264_DEBUG_SEI(
    "   Other parameters not checked.\n"
  );

  return 0;
}

int checkH264DecRefPicMarkingCompliance(
  const H264DecRefPicMarking param
)
{
  H264MemoryManagementControlOperationsBlockPtr opBlk;

  LIBBLU_H264_DEBUG_SEI(
    "  dec_ref_pic_marking() - Decoded reference picture marking:\n"
  );

  if (param.IdrPicFlag) {
    LIBBLU_H264_DEBUG_SEI(
      "   Clear all DPB frame buffers "
      "(no_output_of_prior_pics_flag): %s (0b%x).\n",
      BOOL_INFO(param.noOutputOfPriorPics),
      param.noOutputOfPriorPics
    );

    LIBBLU_H264_DEBUG_SEI(
      "   Usage duration of reference picture "
      "(long_term_reference_flag): %s (0b%x).\n",
      (param.longTermReference) ? "Long term" : "Short term",
      param.longTermReference
    );
  }
  else {
    LIBBLU_H264_DEBUG_SEI(
      "   Adaptive reference pictures marking mode "
      "(adaptive_ref_pic_marking_mode_flag): %s (0b%x).\n",
      BOOL_INFO(param.adaptativeRefPicMarkingMode),
      param.adaptativeRefPicMarkingMode
    );

    if (!param.adaptativeRefPicMarkingMode)
      LIBBLU_H264_DEBUG_SEI(
        "    -> Sliding window reference picture marking mode.\n"
      );

    if (param.adaptativeRefPicMarkingMode) {
      for (
        opBlk = param.MemMngmntCtrlOp;
        NULL != opBlk;
        opBlk = opBlk->nextOperation
      ) {
        LIBBLU_H264_DEBUG_SEI("    - Instruction %u:\n");

        LIBBLU_H264_DEBUG_SEI(
          "     Operation (memory_management_control_operation): %s (0x%x).\n",
          h264MemoryManagementControlOperationValueStr(opBlk->operation),
          opBlk->operation
        );

        if (
          opBlk->operation
            == H264_MEM_MGMNT_CTRL_OP_SHORT_TERM_UNUSED
          || opBlk->operation
            == H264_MEM_MGMNT_CTRL_OP_SHORT_TERM_USED_LONG_TERM
        ) {
          LIBBLU_H264_DEBUG_SEI(
            "      -> Difference of pictures "
            "(difference_of_pic_nums_minus1): %u (0x%x).\n",
            opBlk->diffOfPicNumsMinus1 + 1,
            opBlk->diffOfPicNumsMinus1
          );
        }

        if (
          opBlk->operation == H264_MEM_MGMNT_CTRL_OP_LONG_TERM_UNUSED
        ) {
          LIBBLU_H264_DEBUG_SEI(
            "      -> Long-term picture number "
            "(long_term_pic_num): %u (0x%x).\n",
            opBlk->longTermPicNum,
            opBlk->longTermPicNum
          );
        }

        if (
          opBlk->operation
            == H264_MEM_MGMNT_CTRL_OP_SHORT_TERM_USED_LONG_TERM
          || opBlk->operation
            == H264_MEM_MGMNT_CTRL_OP_USED_LONG_TERM
        ) {
          LIBBLU_H264_DEBUG_SEI(
            "      -> Long-term frame index "
            "(long_term_frame_idx): %u (0x%x).\n",
            opBlk->longTermFrameIdx,
            opBlk->longTermFrameIdx
          );
        }

        if (
          opBlk->operation == H264_MEM_MGMNT_CTRL_OP_MAX_LONG_TERM
        ) {
          LIBBLU_H264_DEBUG_SEI(
            "      -> Maximum long-term frame index "
            "(max_long_term_frame_idx_plus1): %u (0x%x).\n",
            opBlk->maxLongTermFrameIdxPlus1 - 1,
            opBlk->maxLongTermFrameIdxPlus1
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
  assert(handle->sequenceParametersSet.data.vuiParametersPresent);
  assert(0 < handle->picParametersSetIdsPresentNb);

  vuiParam = &handle->sequenceParametersSet.data.vuiParameters;

  if (handle->constraints.idrPicturesOnly && !param.IdrPicFlag)
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "Presence of non-IDR picture is not allowed for "
      "bitstream's profile conformance.\n"
    );

  if (param.IdrPicFlag)
    handle->curProgParam.idrPresent = true;

  LIBBLU_H264_DEBUG_SLICE(
    "   Address of the first macroblock in slice "
    "(first_mb_in_slice): %" PRIu32 " (0x%" PRIx32 ").\n",
    param.firstMbInSlice,
    param.firstMbInSlice
  );

  if (param.firstMbInSlice == 0x0) {
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

      if (!vuiParam->nalHrdParamPresent)
        LIBBLU_ERROR_RETURN(
          "Missing NAL HRD parameters in VUI parameters of SPS NALU.\n"
        );

      assert(0); /* Non-reported error. */
      return -1;
    }

    handle->curProgParam.curNbSlices = 1;

    /* Check presence of both fields (or a complete frame) before completing AU. */
    if (param.fieldPic) {
      if (param.bottomField)
        handle->curProgParam.bottomFieldPresent = true;
      else
        handle->curProgParam.topFieldPresent = true;
    }
    else {
      handle->curProgParam.bottomFieldPresent = true;
      handle->curProgParam.topFieldPresent = true;
    }

    if (param.IdrPicFlag) {
      handle->sequenceParametersSetGopValid = false;
      handle->sei.bufferingPeriodGopValid = false;
      handle->curProgParam.idxPicInGop = 0;
    }
    else
      handle->curProgParam.idxPicInGop++;

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
      if (param.firstMbInSlice <= handle->curProgParam.lastFirstMbInSlice)
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

    handle->curProgParam.curNbSlices++;
  }
  handle->curProgParam.lastFirstMbInSlice = param.firstMbInSlice;

  LIBBLU_H264_DEBUG_SLICE(
    "  Slice type (slice_type): %s (0x%x).\n",
    h264SliceTypeValueStr(param.sliceType),
    param.sliceType
  );

  if (H264_SLICE_TYPE_SI < param.sliceType)
    LIBBLU_ERROR_RETURN(
      "Reserved 'slice_type' == %u in use.\n",
      param.sliceType
    );

  /* Check if slice type is allowed: */
  allowedSliceTypes = handle->constraints.allowedSliceTypes;
  if (!H264_IS_VALID_SLICE_TYPE(allowedSliceTypes, param.sliceType))
    LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
      "Unallowed 'slice_type' == %u in use for bitstream profile "
      "conformance.\n",
      param.sliceType
    );

  if (param.IdrPicFlag && !H264_SLICE_IS_TYPE_I(param.sliceType))
    LIBBLU_ERROR_RETURN(
      "Non I-slice type value 'slice_type' == %u in use in an IDR picture.\n"
    );

  /* Check Access unit delimiter primary_pic_type */
  H264_CHECK_PRIMARY_PICTURE_TYPE(
    allowedSliceTypes,
    handle->accessUnitDelimiter.primaryPictType,
    param.sliceType
  );

  if (!allowedSliceTypes) {
    LIBBLU_ERROR_RETURN(
      "Wrong Access Unit Delimiter slice type indicator, "
      "'primary_pic_type' == %u while picture 'slice_type' == %u.\n",
      handle->accessUnitDelimiter.primaryPictType,
      param.sliceType
    );
  }

  LIBBLU_H264_DEBUG_SLICE(
    "  Active Picture Parameter Set id "
    "(pic_parameter_set_id): %" PRIu8 " (0x%" PRIx8 ").\n",
    param.picParametersSetId,
    param.picParametersSetId
  );

  linkedPPSId = param.picParametersSetId;
  if (
    H264_MAX_PPS <= linkedPPSId
    || !handle->picParametersSetIdsPresent[linkedPPSId]
  )
    LIBBLU_ERROR_RETURN(
      "Reserved/unknown linked PPS id 'pic_parameter_set_id' == %u "
      "in slice header.\n",
      linkedPPSId
    );

  ppsParam = handle->picParametersSet[linkedPPSId];

  if (handle->sequenceParametersSet.data.separateColourPlaneFlag444) {
    LIBBLU_H264_DEBUG_SLICE(
      "  Colour plane (colour_plane_id): %s (0x%x).\n",
      h264ColourPlaneIdValueStr(param.colourPlaneId),
      param.colourPlaneId
    );

    if (H264_COLOUR_PLANE_ID_CR < param.colourPlaneId)
      LIBBLU_ERROR_RETURN(
        "Reserved 'colour_plane_id' == %u in use.\n",
        param.colourPlaneId
      );
  }

  LIBBLU_H264_DEBUG_SLICE(
    "  Frame number (frame_num): %" PRIu16 " (0x%" PRIx16 ").\n",
    param.frameNum,
    param.frameNum
  );

  if (handle->sequenceParametersSet.data.MaxFrameNum <= param.frameNum)
    LIBBLU_ERROR_RETURN(
      "Unexpected too high frame number 'frame_num' == %" PRIu16 ", "
      "according to SPS, this value shall not exceed %u.\n",
      param.frameNum,
      handle->sequenceParametersSet.data.MaxFrameNum
    );

  if (handle->curProgParam.gapsInFrameNum) {
    if (!handle->sequenceParametersSet.data.gapsInFrameNumValueAllowed) {
      /* Unintentional lost of pictures. */
      LIBBLU_ERROR_RETURN(
        "Unexpected gaps in 'frame_num' values.\n"
      );
    }
    else {
      /* TODO: Inform presence of gaps in frame_num. */


      handle->curProgParam.gapsInFrameNum = false; /* Reset */
    }
  }

  if (param.IdrPicFlag) {
    /* IDR picture */
    if (param.frameNum != 0x0) {
      LIBBLU_ERROR_RETURN(
        "Unexpected 'frame_num' == %" PRIu16 " for IDR picture, "
        "shall be equal to 0.\n",
        param.frameNum
      );
    }
  }

  if (!handle->sequenceParametersSet.data.frameMbsOnly) {
    LIBBLU_H264_DEBUG_SLICE(
      "  Not macroblocks-only frame linked parameters "
      "('frame_mbs_only_flag' in SPS == 0b0):\n"
    );

    LIBBLU_H264_DEBUG_SLICE(
      "   -> Field picture (field_pic_flag): %s (0b%x).\n",
      BOOL_INFO(param.fieldPic),
      param.fieldPic
    );

    if (param.fieldPic) {
      LIBBLU_H264_DEBUG_SLICE(
        "   -> Field type (bottom_field_flag): %s (0b%x).\n",
        (param.bottomField) ? "Bottom field" : "Top field",
        param.bottomField
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
    BOOL_STR(param.mbaffFrameFlag),
    param.mbaffFrameFlag
  );
  LIBBLU_H264_DEBUG_SLICE(
    "   => PicHeightInMbs "
    "(Picture height in macroblocks): %u MBs.\n",
    param.picHeightInMbs
  );
  LIBBLU_H264_DEBUG_SLICE(
    "   => PicHeightInSamplesL "
    "(Picture height in luma samples): %u samples.\n",
    param.picHeightInSamplesL
  );
  LIBBLU_H264_DEBUG_SLICE(
    "   => PicHeightInSamplesC "
    "(Picture height in chroma samples): %u samples.\n",
    param.picHeightInSamplesC
  );

  if (param.IdrPicFlag) {
    LIBBLU_H264_DEBUG_SLICE(
      "  IDR picture linked parameters (IdrPicFlag == true):\n"
    );

    LIBBLU_H264_DEBUG_SLICE(
      "   -> IDR picture identifier "
      "(idr_pic_id): %" PRIu16 " (0x%" PRIx16 ").\n",
      param.idrPicId,
      param.idrPicId
    );
  }

  if (handle->sequenceParametersSet.data.picOrderCntType == 0x0) {
    LIBBLU_H264_DEBUG_SLICE(
      "  Picture order count type linked parameters "
      "(pic_order_cnt_type == 0):\n"
    );

    LIBBLU_H264_DEBUG_SLICE(
      "   -> LSB value "
      "(pic_order_cnt_lsb): %" PRIu16 " (0x%" PRIx16 ").\n",
      param.picOrderCntLsb,
      param.picOrderCntLsb
    );

    if (ppsParam->bottomFieldPicOrderInFramePresent && !param.fieldPic) {
      LIBBLU_H264_DEBUG_SLICE(
        "   -> Value difference between bottom and top fields "
        "(delta_pic_order_cnt_bottom): %" PRIu16 " (0x%" PRIx16 ").\n",
        param.deltaPicOrderCntBottom,
        param.deltaPicOrderCntBottom
      );
    }
  }

  if (
    handle->sequenceParametersSet.data.picOrderCntType == 0x1
    && !handle->sequenceParametersSet.data.deltaPicOrderAlwaysZero
  ) {
    LIBBLU_H264_DEBUG_SLICE(
      "  Picture order count type linked parameters "
      "(pic_order_cnt_type == 1):\n"
    );

    LIBBLU_H264_DEBUG_SLICE(
      "   -> %s order count difference from expected order count "
      "(delta_pic_order_cnt[0]): %" PRIu16 " (0x%" PRIx16 ").\n",
      (ppsParam->bottomFieldPicOrderInFramePresent) ?
        "Top field"
      :
        "Coded field",
      param.deltaPicOrderCnt[0],
      param.deltaPicOrderCnt[0]
    );

    if (
      ppsParam->bottomFieldPicOrderInFramePresent
      && !param.fieldPic
    ) {
      LIBBLU_H264_DEBUG_SLICE(
        "   -> Bottom field order count difference from expected order count "
        "(delta_pic_order_cnt[0]): %" PRIu16 " (0x%" PRIx16 ").\n",
        param.deltaPicOrderCnt[1],
        param.deltaPicOrderCnt[1]
      );
    }
  }

  if (ppsParam->redundantPicCntPresent) {
    LIBBLU_H264_DEBUG_SLICE(
      "  Redundant picture count linked parameters "
      "(redundant_pic_cnt_present_flag == 0b1):\n"
    );

    LIBBLU_H264_DEBUG_SLICE(
      "   -> Redundant picture counter value "
      "(redundant_pic_cnt): %" PRIu8 " (0x%" PRIx8 ").\n",
      param.redundantPicCnt,
      param.redundantPicCnt
    );
  }

  if (H264_SLICE_IS_TYPE_B(param.sliceType)) {
    LIBBLU_H264_DEBUG_SLICE(
      "  Inter prediction decoding process "
      "(direct_spatial_mv_pred_flag): %s (0b%x).\n",
      (param.directSpatialMvPred) ?
        "Spatial direct prediction mode"
      :
        "Temporal direct prediction mode",
      param.directSpatialMvPred
    );
  }

  if (
    H264_SLICE_IS_TYPE_P(param.sliceType)
    || H264_SLICE_IS_TYPE_SP(param.sliceType)
    || H264_SLICE_IS_TYPE_B(param.sliceType)
  ) {
    LIBBLU_H264_DEBUG_SLICE(
      "  Override default number of reference indexes "
      "(num_ref_idx_active_override_flag): %s (0b%x).\n",
      BOOL_INFO(param.numRefIdxActiveOverride),
      param.numRefIdxActiveOverride
    );

    /* Check mandatory cases where num_ref_idx_active_override_flag shall be equal to 0b1: */
    if (
      !param.fieldPic
      && 15 < ppsParam->numRefIdxl0DefaultActiveMinus1
      && !param.numRefIdxActiveOverride
    )
      LIBBLU_ERROR_RETURN(
        "'num_ref_idx_active_override_flag' shall be equal to 0b1.\n"
      );

    if (
      H264_SLICE_IS_TYPE_B(param.sliceType)
      && !param.fieldPic
      && 15 < ppsParam->numRefIdxl1DefaultActiveMinus1
      && !param.numRefIdxActiveOverride
    )
      LIBBLU_ERROR_RETURN(
        "'num_ref_idx_active_override_flag' shall be equal to 0b1.\n"
      );

    if (param.numRefIdxActiveOverride) {
      unsigned maxRefIdx;

      maxRefIdx =
        (param.fieldPic) ?
          H264_REF_PICT_LIST_MAX_NB_FIELD
        :
          H264_REF_PICT_LIST_MAX_NB
      ;

      LIBBLU_H264_DEBUG_SLICE(
        "   -> Number of reference index for reference picture list 0 "
        "(num_ref_idx_l0_active_minus1): %u (0x%x).\n",
        param.numRefIdxl0ActiveMinus1 + 1,
        param.numRefIdxl0ActiveMinus1
      );

      /* Check value range: */
      if (maxRefIdx < param.numRefIdxl0ActiveMinus1)
        LIBBLU_H264_ERROR_RETURN(
          "Too high 'num_ref_idx_l0_active_minus1' == %u in use.\n",
          param.numRefIdxl0ActiveMinus1
        );

      if (H264_SLICE_IS_TYPE_B(param.sliceType)) {
        LIBBLU_H264_DEBUG_SLICE(
          "   -> Number of reference index for reference picture list 1 "
          "(num_ref_idx_l1_active_minus1): %u (0x%x).\n",
          param.numRefIdxl1ActiveMinus1 + 1,
          param.numRefIdxl1ActiveMinus1
        );

        /* Check value range: */
        if (maxRefIdx < param.numRefIdxl1ActiveMinus1)
          LIBBLU_H264_ERROR_RETURN(
            "Too high 'num_ref_idx_l1_active_minus1' == %u in use.\n",
            param.numRefIdxl1ActiveMinus1
          );
      }
    }
  }

  if (param.isSliceExt) {
    /* ref_pic_list_mvc_modification() */
    /* Annex H */

    /* TODO: Add 3D MVC support. */
    LIBBLU_ERROR_RETURN("Unsupported 3D MVC format.\n");
  }
  else
    if (checkH264RefPicListModificationCompliance(param.refPicListMod, param) < 0)
      return -1;

  if (
    (
      ppsParam->weightedPred && (
        H264_SLICE_IS_TYPE_P(param.sliceType) ||
        H264_SLICE_IS_TYPE_SP(param.sliceType)
      )
    ) ||
    (
      ppsParam->weightedBipredIdc == H264_WEIGHTED_PRED_B_SLICES_EXPLICIT &&
      H264_SLICE_IS_TYPE_B(param.sliceType)
    )
  ) {
    /* pred_weight_table() */
    if (checkH264PredWeightTableCompliance(param.predWeightTable) < 0)
      return -1;
  }

  if (param.decRefPicMarkingPresent) {
    /* dec_ref_pic_marking() */
    if (checkH264DecRefPicMarkingCompliance(param.decRefPicMarking) < 0)
      return -1;
  }

  if (
    ppsParam->entropyCodingMode &&
    !H264_SLICE_IS_TYPE_I(param.sliceType) &&
    !H264_SLICE_IS_TYPE_SI(param.sliceType)
  ) {
    LIBBLU_H264_DEBUG_SLICE(
      "  CABAC initialization table index "
      "(cabac_init_idc): %" PRIu8 " (0x%" PRIx8 ").\n",
      param.cabacInitIdc,
      param.cabacInitIdc
    );

    if (2 < param.cabacInitIdc)
      LIBBLU_ERROR_RETURN(
        "Out of range 'cabac_init_idc' == %" PRIu8 ".\n",
        param.cabacInitIdc
      );
  }

  LIBBLU_H264_DEBUG_SLICE(
    "  Initial QPy value used for macroblocks of slice: %d (0x%x).\n",
    param.sliceQpDelta,
    param.sliceQpDelta
  );

  if (H264_SLICE_IS_TYPE_SP(param.sliceType) || H264_SLICE_IS_TYPE_SI(param.sliceType)) {
    if (H264_SLICE_IS_TYPE_SP(param.sliceType)) {
      LIBBLU_H264_DEBUG_SLICE(
        "  Decoding process used to decode P macroblocks in SP slice "
        "(sp_for_switch_flag): %s (0b%x).\n",
        (param.spForSwitch) ? "Switching pictures" : "Non-switching pictures",
        param.spForSwitch
      );
    }

    LIBBLU_H264_DEBUG_SLICE(
      "  Initial QSy value used for macroblocks of slice: %d (0x%x).\n",
      param.sliceQsDelta,
      param.sliceQsDelta
    );
  }

  if (ppsParam->deblockingFilterControlPresent) {
    LIBBLU_H264_DEBUG_SLICE(
      "  Deblocking filter control linked parameters "
      "('deblocking_filter_control_present_flag' == 1):\n"
    );

    LIBBLU_H264_DEBUG_SLICE(
      "   -> Deblocking filter status "
      "(disable_deblocking_filter_idc): %s (0x%x).\n",
      h264DeblockingFilterIdcStr(param.disableDeblockingFilterIdc),
      param.disableDeblockingFilterIdc
    );

    if (param.disableDeblockingFilterIdc != H264_DEBLOCKING_FILTER_DISABLED) {
      LIBBLU_H264_DEBUG_SLICE(
        "   -> Deblocking filter alpha and t_c0 tables offset used "
        "(slice_alpha_c0_offset_div2): %d (0x%x).\n",
        param.sliceAlphaC0OffsetDiv2 << 1,
        param.sliceAlphaC0OffsetDiv2
      );

      LIBBLU_H264_DEBUG_SLICE(
        "   -> Deblocking filter beta table offset used "
        "(slice_beta_offset_div2): %d (0x%x).\n",
        param.sliceBetaOffsetDiv2 << 1,
        param.sliceBetaOffsetDiv2
      );
    }
  }

  if (
    1 < ppsParam->nbSliceGroups &&
    (
      ppsParam->sliceGroups.mapType == H264_SLICE_GROUP_TYPE_CHANGING_1
      || ppsParam->sliceGroups.mapType == H264_SLICE_GROUP_TYPE_CHANGING_2
      || ppsParam->sliceGroups.mapType == H264_SLICE_GROUP_TYPE_CHANGING_3
    )
  ) {
    LIBBLU_H264_DEBUG_SLICE(
      "  Modifier of number of slice group map units "
      "(slice_group_change_cycle): %" PRIu32 " (0x%" PRIx32 ").\n",
      param.sliceGroupChangeCycle,
      param.sliceGroupChangeCycle
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
  if (second.firstMbInSlice != 0x0 && first.sliceType != second.sliceType) {
    if (
      !H264_SLICE_TYPE_IS_UNRESTRICTED(first.sliceType)
      || !H264_SLICE_TYPE_IS_UNRESTRICTED(second.sliceType)
    ) {
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Unallowed different type of slices in the same picture.\n"
      );
    }
  }
  if (second.firstMbInSlice != 0x0) {
    if (first.frameNum != second.frameNum)
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Unallowed different 'frame_num' in the same picture.\n"
      );

    if (first.fieldPic != second.fieldPic)
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Unallowed different 'field_pic_flag' in the same picture.\n"
      );

    if (first.bottomField != second.bottomField)
      LIBBLU_H264_COMPLIANCE_ERROR_RETURN(
        "Unallowed different 'bottom_field_flag' in the same picture.\n"
      );

    if (first.picOrderCntLsb != second.picOrderCntLsb)
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