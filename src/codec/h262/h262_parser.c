#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>
#include <assert.h>

#include "h262_parser.h"

unsigned getH262MaxBr(
  uint8_t profileIDC,
  uint8_t levelIDC
)
{
  /* Values from [1] ITU-T H.262 02/2000 - Table 8-13 */
  switch (profileIDC) {
    case H262_PROFILE_ID_SIMPLE:
      if (levelIDC == H262_LEVEL_ID_MAIN)
        return    15000000;
      break;

    case H262_PROFILE_ID_MAIN:
      switch (levelIDC) {
        case H262_LEVEL_ID_LOW:
          return   4000000;
        case H262_LEVEL_ID_MAIN:
          return  15000000;
        case H262_LEVEL_ID_HIGH_1440:
          return  60000000;
        case H262_LEVEL_ID_HIGH:
          return  80000000;
      }
      break;

    case H262_PROFILE_ID_SNR_SCALABLE:
      switch (levelIDC) {
        case H262_LEVEL_ID_LOW:
          return   4000000;
        case H262_LEVEL_ID_MAIN:
          return  15000000;
      }
      break;

    case H262_PROFILE_ID_SPATIALLY_SCALABLE:
      switch (levelIDC) {
        case H262_LEVEL_ID_HIGH_1440:
          return  60000000;
      }
      break;

    case H262_PROFILE_ID_HIGH:
      switch (levelIDC) {
        case H262_LEVEL_ID_MAIN:
          return  20000000;
        case H262_LEVEL_ID_HIGH_1440:
          return  80000000;
        case H262_LEVEL_ID_HIGH:
          return 100000000;
      }
  }

  return 0;
}

/* Decoding functions : */

int decodeSequenceHeader(
  BitstreamReaderPtr m2vInput,
  H262SequenceHeaderParameters * param
)
{
  uint32_t value;

  LIBBLU_H262_DEBUG(
    "0x%08" PRIX64 " === sequence_header() ===\n",
    tellPos(m2vInput)
  );

  /* [v32 sequence_header_code] // 0x000001B3 */
  if (readValueBigEndian(m2vInput, 4, &value) < 0)
    return -1;

  if (value != SEQUENCE_HEADER_CODE)
    LIBBLU_H262_ERROR_RETURN(
      "Unexpected start code for a sequence header "
      "(expect 0x%08X, got %0x%08" PRIX32 ").\n",
      SEQUENCE_HEADER_CODE,
      value
    );

  /* [u12 horizontal_size_value] */
  if (readBits(m2vInput, &value, 12) < 0)
    return -1;
  param->horizontal_size_value = value;

  /* [u12 vertical_size_value] */
  if (readBits(m2vInput, &value, 12) < 0)
    return -1;
  param->vertical_size_value = value;

  /* [u4 aspect_ratio_information] */
  if (readBits(m2vInput, &value, 4) < 0)
    return -1;
  param->aspect_ratio_information = value;

  /* [u4 frame_rate_code] */
  if (readBits(m2vInput, &value, 4) < 0)
    return -1;
  param->frame_rate_code = value;

  /* [u18 bit_rate_value_lower] */
  if (readBits(m2vInput, &value, 18) < 0)
    return -1;
  param->bit_rate_value = value;

  /* [u1 marker_bit] */
  if (readBits(m2vInput, &value, 1) < 0)
    return -1;

  if (!value)
    LIBBLU_H262_ERROR_RETURN(
      "Missing expected marker bit in sequence header "
      "(marker_bit == 0b0).\n"
    );

  /* [u10 vbv_buffer_size_value_lower] */
  if (readBits(m2vInput, &value, 10) < 0)
    return -1;
  param->vbv_buffer_size_value = value;

  /* [b1 constrained_parameters_flag] */
  if (readBits(m2vInput, &value, 1) < 0)
    return -1;
  param->constrained_parameters_flag = value;

  /* [b1 load_intra_quantiser_matrix_flag] */
  if (readBits(m2vInput, &value, 1) < 0)
    return -1;
  param->load_intra_quantiser_matrix = value;

  if (param->load_intra_quantiser_matrix) {
    /* intra_quantiser_matrix present. */
    unsigned i;

    /* [u8 intra_quantiser_matrix[0]] */
    if (readBits(m2vInput, &value, 8) < 0)
      return -1;
    param->intra_quantiser_matrix[0] = value;

    if (param->intra_quantiser_matrix[0] != 8)
      LIBBLU_H262_ERROR_RETURN(
        "Forbidden Intra quantizer matrix coefficient 0, "
        "the value shall be always 8 "
        "(intra_quantiser_matrix[0] == %" PRIu8 ").\n",
        param->intra_quantiser_matrix[0]
      );

    /* [u8*63 intra_quantiser_matrix[1..63]] */
    for (i = 1; i < 64; i++) {
      if (readBits(m2vInput, &value, 8) < 0)
        return -1;
      param->intra_quantiser_matrix[i] = value;

      if (!param->intra_quantiser_matrix[i])
        LIBBLU_H262_ERROR_RETURN(
          "Forbidden Intra quantizer matrix coefficient %u, "
          "the value zero is forbidden "
          "(intra_quantiser_matrix[%u] == 0).\n",
          i, i
        );
    }
  }

  /* [b1 load_non_intra_quantiser_matrix_flag] */
  if (readBits(m2vInput, &value, 1) < 0)
    return -1;
  param->load_non_intra_quantiser_matrix = value;

  if (param->load_non_intra_quantiser_matrix) {
    /* non_intra_quantiser_matrix present. */
    unsigned i;

    /* [u8*64 non_intra_quantiser_matrix] */
    for (i = 0; i < 64; i++) {
      if (readBits(m2vInput, &value, 8) < 0)
        return -1;
      param->non_intra_quantiser_matrix[i] = value;

      if (!param->non_intra_quantiser_matrix[i])
        LIBBLU_H262_ERROR_RETURN(
          "Forbidden non-Intra quantizer matrix coefficient %u, "
          "the value zero is forbidden "
          "(intra_quantiser_matrix[%u] == 0).\n",
          i, i
        );
    }
  }

  return 0;
}

int checkSequenceHeaderCompliance(
  H262SequenceHeaderParameters * param
)
{

  LIBBLU_H262_DEBUG(
    " sequence_header_code: 0x000001B3.\n"
  );

  LIBBLU_H262_DEBUG(
    " Horizontal screen size LSB (horizontal_size_value): %u.\n",
    param->horizontal_size_value
  );

  if (!param->horizontal_size_value)
    LIBBLU_H262_ERROR_RETURN(
      "Sequence header horizontal screen size value cannot be zero "
      "(horizontal_size_value == 0x0).\n"
    );

  LIBBLU_H262_DEBUG(
    " Vertical screen size LSB (vertical_size_value): %u.\n",
    param->vertical_size_value
  );

  if (!param->vertical_size_value)
    LIBBLU_H262_ERROR_RETURN(
      "Sequence header vertical screen size value cannot be zero "
      "(vertical_size_value == 0x0).\n"
    );

  /* NOTE: Compliance is checked in sequence_display_extension() */

#if 0
  /* TODO: Move to sequence extension */
  if (options.secondaryStream && param->vertical_size_value <= 720)
    LIBBLU_H262_WARNING(
      "HD video format may be not supported by all BD-players "
      "as secondary video track.\n"
    );
#endif

  LIBBLU_H262_DEBUG(
    " Aspect Ratio code (aspect_ratio_information): 0x%02" PRIX8 ".\n",
    param->aspect_ratio_information
  );
  LIBBLU_H262_DEBUG(
    "  -> %s.\n",
    H262AspectRatioInformationStr(param->aspect_ratio_information)
  );

  if (H262_ASPECT_RATIO_INF_FORBIDDEN == param->aspect_ratio_information)
    LIBBLU_H262_ERROR_RETURN(
      "Forbidden aspect ratio information in sequence header "
      "(aspect_ratio_information == 0x00).\n"
    );

  if (H262_ASPECT_RATIO_INF_RES <= param->aspect_ratio_information)
    LIBBLU_H262_ERROR_RETURN(
      "Reserved value in use (aspect_ratio_information == 0x%02X).\n",
      param->aspect_ratio_information
    );

  LIBBLU_H262_DEBUG(
    " Frame rate code (frame_rate_code): 0x%02" PRIX8 ".\n",
    param->frame_rate_code
  );
  LIBBLU_H262_DEBUG(
    "  -> Frame-rate (without modifiers): %s.\n",
    H262FrameRateCodeStr(param->frame_rate_code)
  );

  if (H262_FRAME_RATE_COD_FORBIDDEN == param->frame_rate_code)
    LIBBLU_H262_ERROR_RETURN(
      "Forbidden frame rate code in sequence header "
      "(frame_rate_code == 0x00).\n"
    );

  if (H262_FRAME_RATE_COD_RES <= param->frame_rate_code)
    LIBBLU_H262_ERROR_RETURN(
      "Reserved value in use (frame_rate_code == 0x%02X).\n",
      param->frame_rate_code
    );

  /* TODO: Move this check to sequence extension ? */
  if (isPalH262FrameRateCode(param->frame_rate_code))
    LIBBLU_H262_WARNING(
      "Frame-rate value is from PAL region standard, "
      "support of these frame-rate values is not mandatory and may cause "
      "playback issues on extra-european players.\n"
    );

  LIBBLU_H262_DEBUG(
    " Bit-rate LSB (bit_rate_value): %" PRIu32 ".\n",
    param->bit_rate_value
  );

  /* [1] ITU-T Rec. H.262 6.3.3 / [3] MPEG-1 Part 2 2.4.3.2 */
  if (!param->bit_rate_value)
    LIBBLU_H262_ERROR_RETURN(
      "Forbidden zero value for bit-rate LSB (bit_rate_value == 0x0).\n"
    );

  LIBBLU_H262_DEBUG(
    " Video Buffer Verifier buffer required size (vbv_buffer_size_value): "
    "%" PRIu32 ".\n",
    param->vbv_buffer_size_value
  );

  LIBBLU_H262_DEBUG(
    " MPEG-1 Video constrained mode (constrained_parameters_flag): "
    "%s (0b%x).\n",
    BOOL_INFO(param->constrained_parameters_flag),
    param->constrained_parameters_flag
  );

  if (param->constrained_parameters_flag) {
    /* MPEG-1 Contraint parameters check */
    unsigned result;

    LIBBLU_H262_DEBUG(
      "  Checking constraint horizontal_size_value <= 768 pels.\n"
    );

    if (768 < param->horizontal_size_value)
      LIBBLU_H262_ERROR_RETURN(
        "Horizontal size value exceed 768 pels limit for "
        "MPEG-1 contrained paramereters (constrained_parameters_flag == 0b1 "
        "&& 768 < horizontal_size_value (= %u)).\n",
        param->horizontal_size_value
      );

    LIBBLU_H262_DEBUG(
      "  Checking constraint vertical_size_value <= 576 pels.\n"
    );

    if (576 < param->vertical_size_value)
      LIBBLU_H262_ERROR_RETURN(
        "Vertical size value exceed 576 pels limit for "
        "MPEG-1 contrained paramereters (constrained_parameters_flag == 0b1 "
        "&& 576 < vertical_size_value (= %u)).\n",
        param->vertical_size_value
      );

    LIBBLU_H262_DEBUG(
      "  Checking constraint "
      "((horizontal_size + 15) / 16) * ((vertical_size + 15) / 16) <= 396.\n"
    );

    result =
      ((param->horizontal_size_value + 15) / 16)
      * ((param->vertical_size_value + 15) / 16)
    ;

    if (396 < result)
      LIBBLU_H262_ERROR_RETURN(
        "The number of luma macroblocks exceed the 396 upper limit for "
        "MPEG-1 contrained paramereters (constrained_parameters_flag == 0b1 "
        "&& 396 < ((horizontal_size + 15) / 16) * ((vertical_size + 15) / 16) "
        "(= %u)).\n",
        result
      );

    /* NOTE: picture_rate is rounded up */
    LIBBLU_H262_DEBUG(
      "  Checking constraint "
      "((horizontal_size + 15) / 16) * ((vertical_size + 15) / 16) "
      "* picture_rate <= 396 * 25.\n"
    );

    result =
      ((param->horizontal_size_value + 15) / 16)
      * ((param->vertical_size_value + 15) / 16)
      * upperBoundFrameRateH262FrameRateCode(param->frame_rate_code)
    ;

    if (396 * 25 < result)
      LIBBLU_H262_ERROR_RETURN(
        "The number of luma macroblocks per second exceed the 396*25 upper "
        "limit for MPEG-1 contrained paramereters "
        "(constrained_parameters_flag == 0b1 "
        "&& 396 * 25 < ((horizontal_size + 15) / 16) "
        "* ((vertical_size + 15) / 16) * picture_rate "
        "(= %u, with picture_rate == %u)).\n",
        result,
        upperBoundFrameRateH262FrameRateCode(param->frame_rate_code)
      );

    LIBBLU_H262_DEBUG(
      "  Checking constraint picture_rate <= 30.\n"
    );

    if (H262_FRAME_RATE_COD_30 < param->frame_rate_code)
      LIBBLU_H262_ERROR_RETURN(
        "Too high frame rate for MPEG-1 contrained paramereters "
        "(constrained_parameters_flag == 0b1 "
        "&& 30 < picture_rate (= %.3f)).\n",
        frameRateH262FrameRateCode(param->frame_rate_code)
      );

    LIBBLU_H262_DEBUG(
      "  Checking constraint vbv_buffer_size < 20 (20 * 1024 * 16 bits).\n"
    );

    if (20 <= param->vbv_buffer_size_value)
      LIBBLU_H262_ERROR_RETURN(
        "Too high VBV Buffer Size for MPEG-1 contrained paramereters "
        "(constrained_parameters_flag == 0b1 "
        "&& 20 <= vbv_buffer_size (= %u, = %u bits)).\n",
        param->vbv_buffer_size_value,
        param->vbv_buffer_size_value << 14
      );

    LIBBLU_H262_DEBUG(
      "  Checking constraint bit_rate < 4640 * 400 bps.\n"
    );

    if (4640 <= param->bit_rate_value)
      LIBBLU_H262_ERROR_RETURN(
        "Too high bit rate for MPEG-1 contrained paramereters "
        "(constrained_parameters_flag == 0b1 "
        "&& 4640 <= bit_rate (= %u, = %u bps)).\n",
        param->bit_rate_value,
        param->bit_rate_value * 400
      );
  }

  LIBBLU_H262_DEBUG(
    " User-defined Intra quantizer matrix "
    "(load_intra_quantiser_matrix): %s (0b%x).\n",
    BOOL_INFO(param->load_intra_quantiser_matrix),
    param->load_intra_quantiser_matrix
  );

  if (param->load_intra_quantiser_matrix) {
    LIBBLU_H262_DEBUG(
      "  User-defined Intra quantizer matrix (intra_quantiser_matrix[64]): "
      "Not displayed.\n"
    );
  }

  LIBBLU_H262_DEBUG(
    " User-defined non-Intra quantizer matrix "
    "(load_non_intra_quantiser_matrix): %s (0b%x).\n",
    BOOL_INFO(param->load_non_intra_quantiser_matrix),
    param->load_non_intra_quantiser_matrix
  );

  if (param->load_non_intra_quantiser_matrix) {
    LIBBLU_H262_DEBUG(
      "  User-defined Intra quantizer matrix (intra_quantiser_matrix[64]): "
      "Not displayed.\n"
    );
  }

  return 0; /* Everything is fine. */
}

int constantSequenceHeaderCheck(
  const H262SequenceHeaderParameters * first,
  const H262SequenceHeaderParameters * second
)
{
  return START_CHECK
    EQUAL(->horizontal_size_value)
    EQUAL(->vertical_size_value)
    EQUAL(->aspect_ratio_information)
    EQUAL(->frame_rate_code)
    EQUAL(->bit_rate_value)
    EQUAL(->vbv_buffer_size_value)
    EQUAL(->constrained_parameters_flag)
  END_CHECK;
}

int decodeSequenceExtension(
  BitstreamReaderPtr m2vInput,
  H262SequenceExtensionParameters * param
)
{
  uint32_t value;

  LIBBLU_H262_DEBUG(
    "0x%08" PRIX64 " === sequence_extension() ===\n",
    tellPos(m2vInput)
  );

  /* [v32 extension_start_code] // 0x000001B5 */
  if (readValueBigEndian(m2vInput, 4, &value) < 0)
    return -1;

  if (value != EXTENSION_START_CODE)
    LIBBLU_H262_ERROR_RETURN(
      "Unexpected start code for a sequence header "
      "(expect 0x%08X, got %0x%08" PRIX32 ").\n",
      SEQUENCE_HEADER_CODE,
      value
    );

  /* [u4 extension_start_code_identifier] */
  if (readBits(m2vInput, &value, 4) < 0)
    return -1;
  if (value != H262_EXT_STARTCODEEXT_SEQUENCE)
    LIBBLU_H262_ERROR_RETURN(
      "Unexpected extension identifier for a sequence extension "
      "(expect 0x%01X, got %0x%01" PRIX32 ").\n",
      H262_EXT_STARTCODEEXT_SEQUENCE,
      value
    );

  /** [u8 profile_and_level_identifier]
   *  -> v1 Excape bit
   *  -> u3 Profile identification
   *  -> u4 Level identification
   */
  if (readBits(m2vInput, &value, 1) < 0)
    return -1;
  param->profile_and_level_identifier.escapeBit = value;

  if (readBits(m2vInput, &value, 3) < 0)
    return -1;
  param->profile_and_level_identifier.profileIdentification = value;

  if (readBits(m2vInput, &value, 4) < 0)
    return -1;
  param->profile_and_level_identifier.levelIdentification = value;

  /* [b1 progressive_sequence flag] */
  if (readBits(m2vInput, &value, 1) < 0)
    return -1;
  param->progressive_sequence = value;

  /* [u2 chroma_format] */
  if (readBits(m2vInput, &value, 2) < 0)
    return -1;
  param->chroma_format = value;

  /* [u2 horizontal_size_extension] */
  if (readBits(m2vInput, &value, 2) < 0)
    return -1;
  param->horizontal_size_extension = value;

  /* [u2 vertical_size_extension] */
  if (readBits(m2vInput, &value, 2) < 0)
    return -1;
  param->vertical_size_extension = value;

  /* [u12 bit_rate_extension] */
  if (readBits(m2vInput, &value, 12) < 0)
    return -1;
  param->bit_rate_extension = value;

  /* [v1 marker_bit] */
  if (readBits(m2vInput, &value, 1) < 0)
    return -1;

  if (!value)
    LIBBLU_H262_ERROR_RETURN(
      "Missing expected marker bit in sequence header "
      "(marker_bit == 0b0).\n"
    );

  /* [u8 vbv_buffer_size_extension] */
  if (readBits(m2vInput, &value, 8) < 0)
    return -1;
  param->vbv_buffer_size_extension = value;

  /* [b1 low_delay] */
  if (readBits(m2vInput, &value, 1) < 0)
    return -1;
  param->low_delay = value;

  /* [u2 frame_rate_extension_n] */
  if (readBits(m2vInput, &value, 2) < 0)
    return -1;
  param->frame_rate_extension_n = value;

  /* [u5 frame_rate_extension_d] */
  if (readBits(m2vInput, &value, 5) < 0)
    return -1;
  param->frame_rate_extension_d = value;

  return 0;
}

int checkSequenceExtensionCompliance(
  const H262SequenceExtensionParameters * param,
  const H262SequenceHeaderParameters * header,
  LibbluESSettingsOptions options
)
{
  H262ProfileIdentification profileId;
  H262LevelIdentification levelId;

  unsigned horizontal_size;
  unsigned vertical_size;
  unsigned mb_width;
  unsigned mb_height;

  uint32_t bit_rate;
  unsigned vbv_buffer_size;
  uint64_t bitRate, vbvBufferSize;

  float frameRate;
  HdmvFrameRateCode frameRateCode;

  CheckVideoConfigurationRet checkRet;

  LIBBLU_H262_DEBUG(
    " sequence_header_code: 0x000001B5.\n"
  );

  LIBBLU_H262_DEBUG(
    " extension_start_code_identifier: 0x1.\n"
  );
  LIBBLU_H262_DEBUG(
    "  -> Sequence Extension ID.\n"
  );

  /* [1] ITU-T Rec. H.262 6.1.1.6 */
  if (header->constrained_parameters_flag)
    LIBBLU_H262_ERROR_RETURN(
      "Unexpected MPEG-1 Video contrained parameters flag, "
      "presence of sequence extension which indicate a MPEG-2 Video "
      "bitstream, constrained_parameters_flag is reserved and shall be set "
      "to 0b0 (not 0b1).\n"
    );

  LIBBLU_H262_DEBUG(
    " Profile and Level information (profile_and_level_indication):\n"
  );
  LIBBLU_H262_DEBUG(
    "  Escape bit: %s (0b%x).\n",
    BOOL_STR(param->profile_and_level_identifier.escapeBit),
    param->profile_and_level_identifier.escapeBit
  );

  if (param->profile_and_level_identifier.escapeBit)
    LIBBLU_H262_ERROR_RETURN(
      "Unsupported escape bit value 0b1 in profile_and_level_indication of "
      "sequence extension, meaning use of non-hierarchical extension "
      "profile/levels (not allowed by BDAV anyway).\n"
    );

  profileId = param->profile_and_level_identifier.profileIdentification;

  LIBBLU_H262_DEBUG(
    "  Profile: %s (0b%x).\n",
    H262ProfileIdentificationStr(profileId),
    profileId
  );

  if (isReservedH262ProfileIdentification(profileId))
    LIBBLU_H262_ERROR_RETURN(
      "Reserved value in use "
      "(profile_and_level_indication[6:4], "
      "Profile identification == 0x%01X).\n",
      profileId
    );

  if (!isBdavAllowedH262ProfileIdentification(profileId))
    LIBBLU_H262_ERROR_RETURN(
      "Illegal Profile, BDAV specifications allow use of Main Profile only "
      "(got %s, 0x%X).\n",
      H262ProfileIdentificationStr(profileId),
      profileId
    );

  levelId = param->profile_and_level_identifier.levelIdentification;

  LIBBLU_H262_DEBUG(
    "  Level: %s (0b%x).\n",
    H262LevelIdentificationStr(levelId),
    levelId
  );

  if (isReservedH262LevelIdentification(levelId))
    LIBBLU_H262_ERROR_RETURN(
      "Reserved value in use "
      "(profile_and_level_indication[3:0], Level identification == 0x%01X).\n",
      levelId
    );

  if (isBdavAllowedH262LevelIdentification(levelId))
    LIBBLU_H262_ERROR_RETURN(
      "Illegal Level, BDAV specifications allow use of "
      "Main and High levels only (got %s, 0x%X).\n",
      H262LevelIdentificationStr(levelId),
      levelId
    );

  LIBBLU_H262_DEBUG(
    " Progressive only frames sequence (progressive_sequence): %s (0b%x).\n",
    BOOL_STR(param->progressive_sequence),
    param->progressive_sequence
  );

  LIBBLU_H262_DEBUG(
    " Chrominance sub-sampling mode (chroma_format): %s (0x%X).\n",
    H262ChromaFormatStr(param->chroma_format),
    param->chroma_format
  );

  if (isReservedH262ChromaFormat(param->chroma_format))
    LIBBLU_H262_ERROR_RETURN(
      "Reserved value in use (chroma_format == 0x%01X).\n",
      param->chroma_format
    );

  if (H262_CHROMA_FORMAT_420 != param->chroma_format)
    LIBBLU_H262_ERROR_RETURN(
      "Illegal Chroma sub-sampling mode, BDAV specifications allow use of "
      "4:2:0 mode only (got chroma_format == 0x%X, %s).\n",
      param->chroma_format,
      H262ChromaFormatStr(param->chroma_format)
    );

  LIBBLU_H262_DEBUG(
    " Horizontal screen size MSB (horizontal_size_extension): %u.\n",
    param->horizontal_size_extension
  );

  LIBBLU_H262_DEBUG(
    " Vertical screen size MSB (vertical_size_extension): %u px.\n",
    param->vertical_size_extension
  );

  horizontal_size = computeHorizontalSizeH262SequenceComputedValues(header, param);
  mb_width = (horizontal_size + 15) / 16;
  vertical_size = computeVerticalSizeH262SequenceComputedValues(header, param);
  mb_height = (vertical_size + 15) / 16;

  LIBBLU_H262_DEBUG(
    "  => Displayable screen size: %ux%u pixels.\n",
    horizontal_size,
    vertical_size
  );
  LIBBLU_H262_DEBUG(
    "  => Encoded video dimensions: %ux%u MBs.\n",
    mb_width,
    mb_height
  );

  LIBBLU_H262_DEBUG(
    " Bit-rate MSB (bit_rate_extension): %" PRIu32 ".\n",
    param->bit_rate_extension
  );

  bit_rate = computeBitrateH262SequenceComputedValues(header, param);
  bitRate = (uint64_t) bit_rate * 400;

  LIBBLU_H262_DEBUG(
    "  => Bit-rate: %" PRIu32 " * 400 bps, %" PRIu64 " bps.\n",
    bit_rate,
    bitRate
  );

  if (options.dvdMedia && (BDAV_VIDEO_MAX_BITRATE_DVD_OUT / 400) < bit_rate)
    LIBBLU_H262_WARNING(
      "Bitrate value exceed maximum recommanded for a DVD media mux "
      "(%u bps < %" PRIu64 " bps).\n",
      BDAV_VIDEO_MAX_BITRATE_DVD_OUT,
      bitRate
    );

  if (options.secondaryStream && (BDAV_VIDEO_MAX_BITRATE_SEC_V / 400) < bit_rate)
    LIBBLU_H262_ERROR_RETURN(
      "Bitrate value exceed maximum allowed for a secondary video track "
      "(%u bps < %" PRIu64 " bps).\n",
      BDAV_VIDEO_MAX_BITRATE_SEC_V,
      bitRate
    );

  if ((BDAV_VIDEO_MAX_BITRATE / 400) < bit_rate)
    LIBBLU_H262_ERROR_RETURN(
      "Bitrate value exceed maximum allowed for a primary video track "
      "(%u bps < %" PRIu64 " bps).\n",
      BDAV_VIDEO_MAX_BITRATE,
      bitRate
    );

  LIBBLU_H262_DEBUG(
    " Video Buffer Verifier buffer required size MSB "
    "(vbv_buffer_size_extension): %" PRIu32 ".\n",
    param->vbv_buffer_size_extension
  );

  vbv_buffer_size = computeVbvBufSizeH262SequenceComputedValues(header, param);
  vbvBufferSize = (uint64_t) vbv_buffer_size * 16 * 1024;

  LIBBLU_H262_DEBUG(
    "  => VBV buffer required size: "
    "%" PRIu32 " * 16 * 1024 bits, %" PRIu64 " bits.\n",
    vbv_buffer_size,
    vbvBufferSize
  );

  if (options.dvdMedia && BDAV_VIDEO_MAX_BUFFER_SIZE_DVD_OUT < vbvBufferSize)
    LIBBLU_H262_WARNING(
      "VBV required size exceed maximum recommanded for a DVD media mux "
      "(%u bits < %" PRIu64 " bits).\n",
      BDAV_VIDEO_MAX_BUFFER_SIZE_DVD_OUT,
      vbvBufferSize
    );

  if (BDAV_VIDEO_MAX_BUFFER_SIZE < vbvBufferSize)
    LIBBLU_H262_ERROR_RETURN(
      "VBV required size exceed maximum allowed for a video track "
      "(%u bits < %" PRIu64 " bits).\n",
      BDAV_VIDEO_MAX_BUFFER_SIZE_DVD_OUT,
      vbvBufferSize
    );

  if (bitRate < vbvBufferSize)
    LIBBLU_H262_ERROR_RETURN(
      "BDAV specifications violation, VBV required size exceed bit-rate value "
      "leading to potential decoding delays exceeding one second "
      "(VBV req. size (= %" PRIu64 ") < bit-rate (= %" PRIu64 ")).",
      vbvBufferSize,
      bitRate
    );

  LIBBLU_H262_DEBUG(
    " Low-delay encoding mode (low_delay): %s (0b%x).\n",
    BOOL_INFO(param->low_delay),
    param->low_delay
  );

  if (param->low_delay)
    LIBBLU_H262_ERROR_RETURN(
      "Low-delay encoding mode is forbidden by BDAV specifications.\n"
    );

  LIBBLU_H262_DEBUG(
    " Frame-rate value modifier numerator (frame_rate_extension_n): %u.\n",
    param->frame_rate_extension_n
  );

  LIBBLU_H262_DEBUG(
    " Frame-rate value modifier denominator (frame_rate_extension_d): %u.\n",
    param->frame_rate_extension_d
  );

  frameRate = computeFrameRateH262SequenceComputedValues(header, param);
  frameRateCode = getHdmvFrameRateCode(frameRate);

  LIBBLU_H262_DEBUG(
    "  => Frame-rate: %.3f FPS.\n",
    frameRate
  );

  if (!options.secondaryStream) {
    /* Check as a primary stream */
    checkRet = checkPrimVideoConfiguration(
      horizontal_size,
      vertical_size,
      frameRateCode,
      !param->progressive_sequence
    );
  }
  else {
    checkRet = checkSecVideoConfiguration(
      horizontal_size,
      vertical_size,
      frameRateCode,
      !param->progressive_sequence
    );
  }

  switch (checkRet) {
    case CHK_VIDEO_CONF_RET_OK:
      break;

    case CHK_VIDEO_CONF_RET_ILL_VIDEO_SIZE:
      /**
       * NOTE: No need to check for UHD, UHD dimensions are rejected by
       * profile/level tests
       */
      LIBBLU_H262_ERROR_RETURN(
        "Video size %ux%u px is not allowed by BDAV specifications.\n",
        horizontal_size,
        vertical_size
      );

    case CHK_VIDEO_CONF_RET_ILL_FRAME_RATE:
      LIBBLU_H262_ERROR_RETURN(
        "Frame-rate value %.3f is not allowed in combination with video size "
        "%ux%u px according to BDAV specifications.\n",
        frameRate,
        horizontal_size,
        vertical_size
      );

    case CHK_VIDEO_CONF_RET_ILL_DISP_MODE:
      LIBBLU_H262_ERROR_RETURN(
        "%s display mode is not allowed in %uux%u@%.3f FPS configuration "
        "according to BDAV specifications.\n",
        (param->progressive_sequence) ? "Progressive" : "Interlaced",
        horizontal_size,
        vertical_size,
        frameRate
      );
  }

  return 0;
}

int constantSequenceExtensionCheck(
  const H262SequenceExtensionParameters * first,
  const H262SequenceExtensionParameters * second
)
{
  return START_CHECK
    EQUAL(->profile_and_level_identifier.escapeBit)
    EQUAL(->profile_and_level_identifier.profileIdentification)
    EQUAL(->profile_and_level_identifier.levelIdentification)
    EQUAL(->progressive_sequence)
    EQUAL(->chroma_format)
    EQUAL(->horizontal_size_extension)
    EQUAL(->vertical_size_extension)
    EQUAL(->bit_rate_extension)
    EQUAL(->vbv_buffer_size_extension)
    EQUAL(->low_delay)
    EQUAL(->frame_rate_extension_n)
    EQUAL(->frame_rate_extension_d)
  END_CHECK;
}

int decodeSequenceDisplayExtension(
  BitstreamReaderPtr m2vInput,
  H262SequenceDisplayExtensionParameters * param
)
{
  uint32_t value;

  /* [u4 extension_start_code_identifier] */
  if (readBits(m2vInput, &value, 4) < 0)
    return -1;

  if (value != H262_EXT_STARTCODEEXT_SEQUENCE_DISPLAY)
    LIBBLU_H262_ERROR_RETURN(
      "Unexpected extension identifier for a sequence display extension "
      "(expect 0x%01X, got %0x%01" PRIX32 ").\n",
      H262_EXT_STARTCODEEXT_SEQUENCE_DISPLAY,
      value
    );

  /* [u3 video_format] */
  if (readBits(m2vInput, &value, 3) < 0)
    return -1;
  param->videoFormat = value;

  /* [u1 colour_description flag] */
  if (readBits(m2vInput, &value, 1) < 0)
    return -1;
  param->colourDesc = value;

  /* videoFormat & colourDesc parsed outside funct call */

  if (param->colourDesc) {
    /* [u8 colour_primaries] */
    if (readBits(m2vInput, &value, 8) < 0)
      return -1;
    param->colourPrimaries = value;

    /* [u8 transfer_characteristics] */
    if (readBits(m2vInput, &value, 8) < 0)
      return -1;
    param->transferCharact = value;

    /* [u8 matrix_coefficients] */
    if (readBits(m2vInput, &value, 8) < 0)
      return -1;
    param->matrixCoeff = value;
  }
  else {
    param->colourPrimaries = COLOR_PRIM_UNSPECIFIED;
    param->transferCharact = TRANS_CHAR_UNSPECIFIED;
    param->matrixCoeff     = MATRX_COEF_UNSPECIFIED;
  }

  /* [u14 display_horizontal_size] */
  if (readBits(m2vInput, &value, 14) < 0)
    return -1;
  param->dispHorizontalSize = value;

  /* [v1 marker_bit] */
  if (readBits(m2vInput, &value, 1) < 0)
    return -1;

  if (!value)
    LIBBLU_H262_ERROR_RETURN(
      "Missing expected marker bit in sequence header "
      "(marker_bit == 0b0).\n"
    );

  /* [u14 display_vertical_size]  */
  if (readBits(m2vInput, &value, 14) < 0)
    return -1;
  param->dispVerticalSize = value;

  /* [v3 reserved] */
  if (skipBits(m2vInput, 3) < 0)
    return -1;

  return 0;
}

int decodeGopHeader(
  BitstreamReaderPtr m2vInput,
  H262GopHeaderParameters * param
)
{
  uint32_t value;

  /* [v32 group_start_code] // 0x000001B8 */
  if (readValueBigEndian(m2vInput, 4, &value) < 0)
    return -1;

  if (value != GROUP_START_CODE)
    LIBBLU_H262_ERROR_RETURN(
      "Unexpected start code for a Group of Pictures header "
      "(expect 0x%08X, got %0x%08" PRIX32 ").\n",
      GROUP_START_CODE,
      value
    );

  /** [u25 time_code] :
   *  - u1: drop_frame_flag;
   *  - u5: time_code_hours;
   *  - u6: time_code_minutes;
   *  - v1: marker_bit;
   *  - u6: time_code_seconds;
   *  - u6: time_code_pictures;
   */
  if (readBits(m2vInput, &value, 25) < 0)
    return -1;

  param->dropFrame        =  value >> 24;
  param->timeCodeHours    = (value >> 19) & 0x1F;
  param->timeCodeMinutes  = (value >> 13) & 0x3F;
  param->timeCodeSeconds  = (value >>  6) & 0x3F;
  param->timeCodePictures = (value      ) & 0x3F;

  /* Check marker_bit */
  if (0 == ((value >> 12) & 0x1))
    LIBBLU_H262_ERROR_RETURN(
      "Invalid marker_bit in Group of Pictures header, "
      "value shall be 0b1, not 0b0.\n"
    );

  /* [u1 closed_gop flag] */
  if (readBits(m2vInput, &value, 1) < 0)
    return -1;
  param->closedGop = value;

  /* [b1 broken_link] */
  if (readBits(m2vInput, &value, 1) < 0)
    return -1;
  param->brokenLink = value;

  /* [u5 reserved] */
  if (skipBits(m2vInput, 5) < 0)
    return -1;

  return 0;
}

int decodePictureHeader(
  BitstreamReaderPtr m2vInput,
  H262PictureHeaderParameters * param
)
{
  uint32_t value;

  LIBBLU_H262_DEBUG(
    "0x%08" PRIX64 " === picture_header() ===\n",
    tellPos(m2vInput)
  );

  /* [v32 picture_start_code] // 0x00000100 */
  if (readValueBigEndian(m2vInput, 4, &value) < 0)
    return -1;

  if (value != PICTURE_START_CODE)
    LIBBLU_H262_ERROR_RETURN(
      "Unexpected start code for a Picture "
      "(expect 0x%08X, got %0x%08" PRIX32 ").\n",
      PICTURE_START_CODE,
      value
    );

  /* [u10 temporal_reference] */
  if (readBits(m2vInput, &value, 10) < 0)
    return -1;
  param->temporal_reference = value;

  /* [u3 picture_coding_type] */
  if (readBits(m2vInput, &value, 3) < 0)
    return -1;
  param->picture_coding_type = value;

  /* [u16 vbv_delay] */
  if (readBits(m2vInput, &value, 16) < 0)
    return -1;
  param->vbv_delay = value;

  /* [3] ISO/IEC 11172-2 reserved fields */
  if (
    param->picture_coding_type == H262_PIC_CODING_TYPE_P
    || param->picture_coding_type == H262_PIC_CODING_TYPE_B
  ) {
    /* [v1 full_pel_forward_vector] */
    if (readBits(m2vInput, &value, 1) < 0)
      return -1;
    param->full_pel_forward_vector = value;

    /* [v3 forward_vector] */
    if (readBits(m2vInput, &value, 3) < 0)
      return -1;
    param->forward_f_code = value;
  }

  if (param->picture_coding_type == H262_PIC_CODING_TYPE_B) {
    /* [v1 full_pel_backward_vector] */
    if (readBits(m2vInput, &value, 1) < 0)
      return -1;
    param->full_pel_backward_vector = value;

    /* [v3 backward_f_code] */
    if (readBits(m2vInput, &value, 3) < 0)
      return -1;
    param->backward_f_code = value;
  }

  /* Extra picture informations section */
  /* extra_bit_picture */
  do {
    /* [b1 extra_bit_picture] */
    if (readBits(m2vInput, &value, 1) < 0)
      return -1;

    /* extra_bit_picture == 0b1 */
    if (value) {
      /* [u8 extra_information_picture] */
      if (readBits(m2vInput, NULL, 8) < 0)
        return -1;
    }
  } while (value);
  /* extra_bit_picture == 0b0 */

  if (paddingByte(m2vInput) < 0)
    return -1;

  return 0;
}

int checkPictureHeaderCompliance(
  const H262PictureHeaderParameters * param,
  unsigned gopIndex
)
{
  static short bPictDist = 0;
  static int vbvBufInUse = -1;

  LIBBLU_H262_DEBUG(
    " picture_start_code: 0x00000100.\n"
  );

  LIBBLU_H262_DEBUG(
    " Coded picture temporal reference "
    "(temporal_reference): %u.\n",
    param->temporal_reference
  );

  LIBBLU_H262_DEBUG(
    " Picture type (picture_coding_type): %s (0x%X).\n",
    H262PictureCodingTypeStr(param->picture_coding_type),
    param->picture_coding_type
  );

  if (H262_PIC_CODING_TYPE_D == param->picture_coding_type)
    LIBBLU_H262_ERROR_RETURN(
      "Unexpected D-picture in a MPEG-2 Video bitstream, "
      "these are only allowed in MPEG-1 Video.\n"
    );

  if (isReservedH262PictureCodingType(param->picture_coding_type))
    LIBBLU_H262_ERROR_RETURN(
      "Reserved value in use (picture_coding_type == 0x%01X).\n",
      param->picture_coding_type
    );

  if (gopIndex == 0 && param->picture_coding_type != H262_PIC_CODING_TYPE_I)
    LIBBLU_H262_ERROR_RETURN(
      "First coded picture in GOP shall be a I-picture (not a %s).\n",
      H262PictureCodingTypeStr(param->picture_coding_type)
    );

  if (param->picture_coding_type == H262_PIC_CODING_TYPE_B)
    bPictDist++;
  else
    bPictDist = 0;

  if (MAX_CONSECUTIVE_B_FRAMES < bPictDist)
    LIBBLU_H262_ERROR_RETURN(
      "Too many consecutive B-pictures, BDAV restricts GOP structure to "
      "maximum %u consecutive B-pictures.\n",
      MAX_CONSECUTIVE_B_FRAMES
    );

  LIBBLU_H262_DEBUG(
    " Video Buffer Verifier delay (vbv_delay): 0x%04" PRIX16 ".\n",
    param->vbv_delay
  );

  if (vbvBufInUse < 0)
    vbvBufInUse = (param->vbv_delay != 0xFFFF);

  if (vbvBufInUse ^ (param->vbv_delay != 0xFFFF))
    LIBBLU_H262_ERROR_RETURN(
      "VBV delay use inconsistency.\n"
    );

  if (
    param->picture_coding_type == H262_PIC_CODING_TYPE_P
    || param->picture_coding_type == H262_PIC_CODING_TYPE_B
  ) {
    LIBBLU_H262_DEBUG(
      " MPEG-1 full pixel forward vectors steps (full_pel_forward_vector): "
      "%s (0b%x).\n",
      BOOL_INFO(param->full_pel_forward_vector),
      param->full_pel_forward_vector
    );

    if (param->full_pel_forward_vector)
      LIBBLU_H262_ERROR_RETURN(
        "MPEG-1 Video reserved field in use, forward vectors full pixel steps "
        "mode is no longer used in MPEG-2 Video "
        "(full_pel_forward_vector == 0b1).\n"
      );

    LIBBLU_H262_DEBUG(
      " MPEG-1 forward vectors code (forward_f_code): 0x%X.\n",
      param->forward_f_code
    );

    if (0x7 != param->forward_f_code)
      LIBBLU_H262_ERROR_RETURN(
        "MPEG-1 Video reserved field in use, forward vectors code "
        "is no longer used in MPEG-2 Video "
        "(forward_f_code == 0x%" PRIX8 ", expect 0x7).\n",
        param->forward_f_code
      );
  }

  if (param->picture_coding_type == H262_PIC_CODING_TYPE_B) {
    LIBBLU_H262_DEBUG(
      " MPEG-1 full pixel backward vectors steps (full_pel_backward_vector): "
      "%s (0b%x).\n",
      BOOL_INFO(param->full_pel_backward_vector),
      param->full_pel_backward_vector
    );

    if (param->full_pel_backward_vector)
      LIBBLU_H262_ERROR_RETURN(
        "MPEG-1 Video reserved field in use, backward vectors full pixel steps "
        "mode is no longer used in MPEG-2 Video "
        "(full_pel_backward_vector == 0b1).\n"
      );

    LIBBLU_H262_DEBUG(
      " MPEG-1 backward vectors code (backward_f_code): 0x%X.\n",
      param->backward_f_code
    );

    if (0x7 != param->backward_f_code)
      LIBBLU_H262_ERROR_RETURN(
        "MPEG-1 Video reserved field in use, backward vectors code "
        "is no longer used in MPEG-2 Video "
        "(backward_f_code == 0x%" PRIX8 ", expect 0x7).\n",
        param->backward_f_code
      );
  }

  return 0;
}

int decodePictureCodingExtension(
  BitstreamReaderPtr m2vInput,
  H262PictureCodingExtensionParameters * param
) {
  uint32_t value;

  /* [v32 extension_start_code] // 0x000001B5 */
  if (readValueBigEndian(m2vInput, 4, &value) < 0)
    return -1;

  if (value != EXTENSION_START_CODE)
    LIBBLU_H262_ERROR_RETURN(
      "Unexpected start code for a Picture coding extension "
      "(expect 0x%08X, got %0x%08" PRIX32 ").\n",
      EXTENSION_START_CODE,
      value
    );

  /* [u4 extension_start_code_identifier] */
  if (readBits(m2vInput, &value, 4) < 0)
    return -1;

  if (value != H262_EXT_STARTCODEEXT_PICTURE_CODING)
    LIBBLU_H262_ERROR_RETURN(
      "Unexpected extension identifier for a picture coding extension "
      "(expect 0x%01X, got %0x%01" PRIX32 ").\n",
      H262_EXT_STARTCODEEXT_PICTURE_CODING,
      value
    );

  /* [u4 f_code[0][0]] */
  if (readBits(m2vInput, &value, 4) < 0)
    return -1;
  param->f_codes[0][0] = value;

  /* [u4 f_code[0][1]] */
  if (readBits(m2vInput, &value, 4) < 0)
    return -1;
  param->f_codes[0][1] = value;

  /* [u4 f_code[1][0]] */
  if (readBits(m2vInput, &value, 4) < 0)
    return -1;
  param->f_codes[1][0] = value;

  /* [u4 f_code[1][1]] */
  if (readBits(m2vInput, &value, 4) < 0)
    return -1;
  param->f_codes[1][1] = value;

  /* [u2 intra_dc_precision] */
  if (readBits(m2vInput, &value, 2) < 0)
    return -1;
  param->intraDcPrec = 8 + value; /* in bits */

  /* [u2 picture_structure] */
  if (readBits(m2vInput, &value, 2) < 0)
    return -1;
  param->pictStruct = value;

  /* [b1 top_field_first] */
  if (readBits(m2vInput, &value, 1) < 0)
    return -1;
  param->topFieldFirst = value;

  /* [b1 frame_pred_frame_dct] */
  if (readBits(m2vInput, &value, 1) < 0)
    return -1;
  param->framePredFrameDct = value;

  /* [b1 concealment_motion_vectors] // not used */
  if (readBits(m2vInput, &value, 1) < 0)
    return -1;
  param->concealmentMotionVectors = value;

  /* [b1 q_scale_type]               // not used */
  if (readBits(m2vInput, &value, 1) < 0)
    return -1;
  param->qScaleType = value;

  /* [b1 intra_vlc_format]           // not used */
  if (readBits(m2vInput, &value, 1) < 0)
    return -1;
  param->intraVlcFormat = value;

  /* [b1 alternate_scan]             // not used */
  if (readBits(m2vInput, &value, 1) < 0)
    return -1;
  param->alternateScan = value;

  /* [b1 repeat_first_field] */
  if (readBits(m2vInput, &value, 1) < 0)
    return -1;
  param->repeatFirstField = value;

  /* [b1 chroma_420_type] */
  if (readBits(m2vInput, &value, 1) < 0)
    return -1;
  param->chroma420Type = value;

  /* [b1 progressive_frame] */
  if (readBits(m2vInput, &value, 1) < 0)
    return -1;
  param->progressiveFrame = value;

  /* [b1 composite_display_flag] */
  if (readBits(m2vInput, &value, 1) < 0)
    return -1;
  param->compositeDisplayFlag = value;

  if (paddingByte(m2vInput) < 0)
    return -1;

  return 0;
}

int decodeQuantMatrixExtension(BitstreamReaderPtr m2vInput, H262QuantMatrixExtensionParameters * param)
{
  uint32_t value;

  /* [u4 extension_start_code_identifier] */
  if (readBits(m2vInput, &value, 4) < 0)
    return -1;

  if (value != H262_EXT_STARTCODEEXT_QUANT_MATRIX)
    LIBBLU_H262_ERROR_RETURN(
      "Unexpected extension identifier for a quant matrix extension "
      "(expect 0x%X, got %0x%" PRIX32 ").\n",
      H262_EXT_STARTCODEEXT_QUANT_MATRIX,
      value
    );

  /* [b1 load_intra_quantiser_matrix_flag] */
  if (readBits(m2vInput, &value, 1) < 0)
    return -1;
  param->load_intra_quantiser_matrix = value;

  if (param->load_intra_quantiser_matrix) {
    /* intra_quantiser_matrix present. */

    /* [u8 intra_quantiser_matrix[0]] */
    if (readBits(m2vInput, &value, 8) < 0)
      return -1;
    param->intra_quantiser_matrix[0] = value;

    if (param->intra_quantiser_matrix[0] != 8)
      LIBBLU_H262_ERROR_RETURN(
        "Forbidden Intra quantizer matrix coefficient 0, "
        "the value shall be always 8 "
        "(intra_quantiser_matrix[0] == %" PRIu8 ").\n",
        param->intra_quantiser_matrix[0]
      );

    /* [u8*63 intra_quantiser_matrix[1..63]] */
    for (unsigned i = 1; i < 64; i++) {
      if (readBits(m2vInput, &value, 8) < 0)
        return -1;
      param->intra_quantiser_matrix[i] = value;

      if (!param->intra_quantiser_matrix[i])
        LIBBLU_H262_ERROR_RETURN(
          "Forbidden Intra quantizer matrix coefficient %u, "
          "the value zero is forbidden "
          "(intra_quantiser_matrix[%u] == 0).\n",
          i, i
        );
    }
  }

  /* [b1 load_non_intra_quantiser_matrix_flag] */
  if (readBits(m2vInput, &value, 1) < 0)
    return -1;
  param->load_non_intra_quantiser_matrix = value;

  if (param->load_non_intra_quantiser_matrix) {
    /* non_intra_quantiser_matrix present. */

    /* [u8*64 non_intra_quantiser_matrix] */
    for (unsigned i = 0; i < 64; i++) {
      if (readBits(m2vInput, &value, 8) < 0)
        return -1;
      param->non_intra_quantiser_matrix[i] = value;

      if (!param->non_intra_quantiser_matrix[i])
        LIBBLU_H262_ERROR_RETURN(
          "Forbidden non-Intra quantizer matrix coefficient %u, "
          "the value zero is forbidden "
          "(intra_quantiser_matrix[%u] == 0).\n",
          i, i
        );
    }
  }

  /* [b1 load_chroma_intra_quantiser_matrix] */
  if (readBits(m2vInput, &value, 1) < 0)
    return -1;
  param->load_chroma_intra_quantiser_matrix = value;

  if (param->load_chroma_intra_quantiser_matrix) {
    /* chroma_intra_quantiser_matrix present. */

    /* [u8 chroma_intra_quantiser_matrix[0]] */
    if (readBits(m2vInput, &value, 8) < 0)
      return -1;
    param->chroma_intra_quantiser_matrix[0] = value;

    if (param->chroma_intra_quantiser_matrix[0] != 8)
      LIBBLU_H262_ERROR_RETURN(
        "Forbidden chroma Intra quantizer matrix coefficient 0, "
        "the value shall be always 8 "
        "(chroma_intra_quantiser_matrix[0] == %" PRIu8 ").\n",
        param->chroma_intra_quantiser_matrix[0]
      );

    /* [u8*63 chroma_intra_quantiser_matrix[1..63]] */
    for (unsigned i = 1; i < 64; i++) {
      if (readBits(m2vInput, &value, 8) < 0)
        return -1;
      param->chroma_intra_quantiser_matrix[i] = value;

      if (!param->chroma_intra_quantiser_matrix[i])
        LIBBLU_H262_ERROR_RETURN(
          "Forbidden chroma Intra quantizer matrix coefficient %u, "
          "the value zero is forbidden "
          "(chroma_intra_quantiser_matrix[%u] == 0).\n",
          i, i
        );
    }
  }

  /* [b1 load_chroma_non_intra_quantiser_matrix] */
  if (readBits(m2vInput, &value, 1) < 0)
    return -1;
  param->load_chroma_non_intra_quantiser_matrix = value;

  if (param->load_chroma_non_intra_quantiser_matrix) {
    /* non_intra_quantiser_matrix present. */

    /* [u8*64 chroma_non_intra_quantiser_matrix] */
    for (unsigned i = 0; i < 64; i++) {
      if (readBits(m2vInput, &value, 8) < 0)
        return -1;
      param->chroma_non_intra_quantiser_matrix[i] = value;

      if (!param->chroma_non_intra_quantiser_matrix[i])
        LIBBLU_H262_ERROR_RETURN(
          "Forbidden chroma non-Intra quantizer matrix coefficient %u, "
          "the value zero is forbidden "
          "(intra_quantiser_matrix[%u] == 0).\n",
          i, i
        );
    }
  }

  return 0;
}

int decodeCopyrightExtension(BitstreamReaderPtr m2vInput, H262CopyrightExtensionParameters * param)
{
  uint32_t value;

  /* [u4 extension_start_code_identifier] */
  if (readBits(m2vInput, &value, 4) < 0)
    return -1;

  if (value != H262_EXT_STARTCODEEXT_COPYRIGHT)
    LIBBLU_H262_ERROR_RETURN(
      "Unexpected extension identifier for a copyright extension "
      "(expect 0x%01X, got %0x%01" PRIX32 ").\n",
      H262_EXT_STARTCODEEXT_COPYRIGHT,
      value
    );

  /* [b1 copyright_flag] */
  if (readBits(m2vInput, &value, 1) < 0)
    return -1;
  param->copyrightFlag = value;

  /* [u8 copyright_identifier] */
  if (readBits(m2vInput, &value, 8) < 0)
    return -1;
  param->copyrightIdentifier = value;

  /* [b1 original_or_copy] */
  if (readBits(m2vInput, &value, 1) < 0)
    return -1;
  param->originalOrCopy = value;

  /* [v2 reserved] [v5 reserved] */
  if (skipBits(m2vInput, 7) < 0)
    return -1;

  /* [v1 marker_bit] */
  if (readBits(m2vInput, &value, 1) < 0)
    return -1;

  if (!value)
    LIBBLU_H262_ERROR_RETURN(
      "Invalid 'marker_bit' in Copyright extension, "
      "value shall be 0b1, not 0b0.\n"
    );

  /* [u20 copyright_number_1] */
  if (readBits(m2vInput, &value, 20) < 0)
    return -1;
  param->copyrightNumber = value;

  /* [u1 marker_bit] */
  if (readBits(m2vInput, &value, 1) < 0)
    return -1;

  if (!value)
    LIBBLU_H262_ERROR_RETURN(
      "Invalid 'marker_bit' in Copyright extension, "
      "value shall be 0b1, not 0b0.\n"
    );

  /* [u22 copyright_number_2] */
  if (readBits(m2vInput, &value, 22) < 0)
    return -1;
  param->copyrightNumber = (param->copyrightNumber << 22) + value;

  /* [u1 marker_bit] */
  if (readBits(m2vInput, &value, 1) < 0)
    return -1;

  if (!value)
    LIBBLU_H262_ERROR_RETURN(
      "Invalid 'marker_bit' in Copyright extension, "
      "value shall be 0b1, not 0b0.\n"
    );

  /* [u22 copyright_number_3] */
  if (readBits(m2vInput, &value, 22) < 0)
    return -1;
  param->copyrightNumber = (param->copyrightNumber << 22) + value;

  return 0;
}

int jumpToNextCode(BitstreamReaderPtr m2vInput)
{
  uint32_t value;

  while ((value = nextUint24(m2vInput)) != START_CODE_PREFIX && !isEof(m2vInput)) {
    if ((value & 0xFF) != 0x0) {
      if (skipBytes(m2vInput, 2) < 0)
        return -1;
    }

    if (skipBytes(m2vInput, 1) < 0)
      return -1;
  }

  return 0;
}

/* int decodeSlice(BitstreamReaderPtr m2vInput, H262SliceParameters * param) */
int decodeSlice(BitstreamReaderPtr m2vInput)
{
  uint32_t value;

  /* [v32 slice_start_code] // 0x000001xx */
  if (readValueBigEndian(m2vInput, 4, &value) < 0)
    return -1;

  if ((value >> 8) != (SLICE_START_CODE >> 8))
    LIBBLU_H262_ERROR_RETURN(
      "Unexpected start code for a Picture Slice "
      "(expect 0x%06XXX, got %0x%08" PRIX32 ").\n",
      SLICE_START_CODE >> 8,
      value
    );

  /* [u5 quantiser_scale_code] */
  if (readBits(m2vInput, &value, 5) < 0)
    return -1;

  /* param->quantiserScaleCode = value; */

  if (nextUint8(m2vInput) >> 7) {
    /* [v1 sliceExtensionFlag] */

    /* [b1 intra_slice] [b1 slice_picture_id_enable] */
    /* if (readBits(m2vInput, &value, 2) < 0) */
      /* return -1; */
    /* param->intraSlice        = value >> 1; */
    /* param->slicePictIdEnable = value & 0x1; */

    /* [u6 slice_picture_id] */
    /* if (readBits(m2vInput, &value, 6) < 0) */
      /* return -1; */
    /* param->slicePictureId = value; */

    if (skipBits(m2vInput, 9) < 0)
      return -1;

    while (nextUint8(m2vInput) >> 7) {
      /* [b1 extra_bit_slice] // 0b1 */
      /* [u8 extra_information_slice] */
      if (skipBits(m2vInput, 9) < 0)
        return -1;
    }
  }

  /* [v1 extra_bit_slice] // 0b0 */
  if (skipBits(m2vInput, 1) < 0)
    return -1;

  if (paddingByte(m2vInput) < 0)
    return -1;

  /* while (nextUint24(m2vInput) != START_CODE_PREFIX && !isEof(m2vInput)) { */
    /* if (skipBytes(m2vInput, 1) < 0) */
      /* return -1; */
  /* } */

  if (jumpToNextCode(m2vInput) < 0)
    return -1;
  return 0;
}

int browseUserData(BitstreamReaderPtr m2vInput)
{
  uint32_t value;

  /* [v32 user_data_start_code] // 0x000001B2 */
  if (readValueBigEndian(m2vInput, 4, &value) < 0)
    return -1;

  if (value != USER_DATA_START_CODE)
    LIBBLU_H262_ERROR_RETURN(
      "Unexpected start code for a used data section "
      "(expect 0x%08X, got %0x%08" PRIX32 ").\n",
      USER_DATA_START_CODE,
      value
    );

  while (nextUint32(m2vInput) != START_CODE_PREFIX) {
    if (skipBytes(m2vInput, 1) < 0)
      return -1;
  }

  return 0;
}

int decodeExtension(
  BitstreamReaderPtr m2vInput,
  H262ExtensionParameters * param,
  ExtensionDataLocation location
)
{
  uint32_t value;

  while (nextUint32(m2vInput) == EXTENSION_START_CODE) {
    /* [v32 extension_start_code] // 0x000001B5 */
    if (readValueBigEndian(m2vInput, 4, &value) < 0)
      return -1;

    if (value != EXTENSION_START_CODE)
      LIBBLU_H262_ERROR_RETURN(
        "Unexpected start code for a Extension section "
        "(expect 0x%08X, got %0x%08" PRIX32 ").\n",
        EXTENSION_START_CODE,
        value
      );

    switch (location) {
      case EXT_LOC_SEQUENCE_EXTENSION:
        switch (nextUint8(m2vInput) >> 4) {
          case H262_EXT_STARTCODEEXT_SEQUENCE_DISPLAY:
            param->sequenceDisplayPresence = true;
            if (decodeSequenceDisplayExtension(m2vInput, &(param->sequenceDisplay)) < 0)
              return -1;
            break;

          case H262_EXT_STARTCODEEXT_SEQUENCE_SCALABLE:
            LIBBLU_H262_ERROR_RETURN(
              "Unexpected scalable extension, scalable bitstream aren't "
              "allowed in BDAV specifications.\n"
            );

          default:
            LIBBLU_H262_ERROR_RETURN(
              "Unknown extension identifier 0x%X.\n",
              nextUint8(m2vInput) >> 4
            );
        }
        break;

      case EXT_LOC_GROUP_OF_PICTURE_HEADER:
        LIBBLU_ERROR_RETURN(
          "Unexpected extension section after GOP header.\n"
        );

      case EXT_LOC_PICTURE_CODING_EXTENSION:
        switch (nextUint8(m2vInput) >> 4) {
          case H262_EXT_STARTCODEEXT_QUANT_MATRIX:
            param->quantMatrixPresence = true;
            if (decodeQuantMatrixExtension(m2vInput, &(param->quantMatrix)) < 0)
              return -1;
            break;

          case H262_EXT_STARTCODEEXT_PICTURE_DISPLAY:
          case H262_EXT_STARTCODEEXT_SPATIAL_SCALABLE:
          case H262_EXT_STARTCODEEXT_TEMPORAL_SCALABLE:
          case H262_EXT_STARTCODEEXT_CAMERA_PARAMETERS:
          case H262_EXT_STARTCODEEXT_ITU_T:
            LIBBLU_ERROR_RETURN(
              "Unsupported extension id %X.\n",
              nextUint8(m2vInput) >> 4
            );

          case H262_EXT_STARTCODEEXT_COPYRIGHT:
            param->copyrightPresence = true;
            if (decodeCopyrightExtension(m2vInput, &(param->copyright)) < 0)
              return -1;
            break;

          default:
            LIBBLU_ERROR_RETURN(
              "Unknown/Unexpected extension id %X.\n",
              nextUint8(m2vInput) >> 4
            );
        }
        break;
    }
  }

  return 0;
}

int decodeExtensionUserData(
  BitstreamReaderPtr m2vInput,
  H262ExtensionParameters * param,
  ExtensionDataLocation location
)
{

  while (
    nextUint32(m2vInput) == EXTENSION_START_CODE
    || nextUint32(m2vInput) == USER_DATA_START_CODE
  ) {
    if (
      location != EXT_LOC_GROUP_OF_PICTURE_HEADER
      && nextUint32(m2vInput) == EXTENSION_START_CODE
    ) {
      if (decodeExtension(m2vInput, param, location) < 0)
        return -1;
      continue;
    }

    if (nextUint32(m2vInput) == USER_DATA_START_CODE) {
      if (browseUserData(m2vInput) < 0)
        return -1;
      continue;
    }

    LIBBLU_H262_ERROR_RETURN(
      "Unexpected extension/user data section.\n"
    );
  }

  return 0;
}

#if 0
int checkSequenceDisplayExtensionCompliance(
  H262SequenceDisplayExtensionParameters * param,
  H262SequenceHeaderParameters * headerParam
) {

  LIBBLU_DEBUG_COM("Video format: ");
  switch (param->videoFormat) {
    case FORMAT_COMPONENT:
      LIBBLU_DEBUG_COM_NO_HEADER("Component (Not recommanded)");
      break;
    case FORMAT_PAL:
      LIBBLU_DEBUG_COM_NO_HEADER("PAL (Uncommon)");
      break;
    case FORMAT_NTSC:
      LIBBLU_DEBUG_COM_NO_HEADER("NTSC");
      break;
    case FORMAT_SECAM:
      LIBBLU_DEBUG_COM_NO_HEADER("SECAM (Uncommon)");
      break;
    case FORMAT_MAC:
      LIBBLU_DEBUG_COM_NO_HEADER("MAC (Uncommon)");
      break;
    case FORMAT_UNSPECIFIED:
      LIBBLU_DEBUG_COM_NO_HEADER("Unspecified");
      break;
    default:
      LIBBLU_DEBUG_COM_NO_HEADER("Reserved value");
      return -1;
  }
  LIBBLU_DEBUG_COM_NO_HEADER(" (%02" PRIx8 ").\n", param->videoFormat);

  if (param->videoFormat != FORMAT_NTSC && param->videoFormat != FORMAT_UNSPECIFIED) {
    LIBBLU_DEBUG_COM("Warning: Uncommon Video Format specified in Sequence Display Extension.\n");
  }

  if (param->colourDesc) {
    LIBBLU_DEBUG_COM("Info: Color informations present.\n");

    LIBBLU_DEBUG_COM("Colour Primaries: ");
    switch (param->colourPrimaries) {
      case COLOR_PRIM_BT709:
        LIBBLU_DEBUG_COM_NO_HEADER("ITU-R BT.709"); break;
      case COLOR_PRIM_UNSPECIFIED:
        LIBBLU_DEBUG_COM_NO_HEADER("Unspecified Video"); break;
      case COLOR_PRIM_BT470M:
        LIBBLU_DEBUG_COM_NO_HEADER("ITU-R BT.470-2 System M"); break;
      case COLOR_PRIM_BT470BG:
        LIBBLU_DEBUG_COM_NO_HEADER("ITU-R BT.470-2 System B, G"); break;
      case COLOR_PRIM_SMPTE170:
        LIBBLU_DEBUG_COM_NO_HEADER("SMPTE 170M"); break;
      case COLOR_PRIM_SMPTE240:
        LIBBLU_DEBUG_COM_NO_HEADER("SMPTE 240M (1987)"); break;
      default:
        LIBBLU_DEBUG_COM_NO_HEADER("Reserved value");
        return -1;
    }
    LIBBLU_DEBUG_COM_NO_HEADER(" (%02" PRIx8 ").\n", param->colourPrimaries);

    LIBBLU_DEBUG_COM("Transfer Characteristics: ");
    switch (param->transferCharact) {
      case TRANS_CHAR_BT709:
        LIBBLU_DEBUG_COM_NO_HEADER("ITU-R BT.709"); break;
      case TRANS_CHAR_UNSPECIFIED:
        LIBBLU_DEBUG_COM_NO_HEADER("Unspecified Video"); break;
      case TRANS_CHAR_BT470M:
        LIBBLU_DEBUG_COM_NO_HEADER("ITU-R BT.470-2 System M");
        break;
      case TRANS_CHAR_BT470BG:
        LIBBLU_DEBUG_COM_NO_HEADER("ITU-R BT.470-2 System B, G");
        break;
      case TRANS_CHAR_SMPTE170:
        LIBBLU_DEBUG_COM_NO_HEADER("SMPTE 170M");
        break;
      case TRANS_CHAR_SMPTE240:
        LIBBLU_DEBUG_COM_NO_HEADER("SMPTE 240M (1987)");
        break;
      case TRANS_CHAR_LINEAR:
        LIBBLU_DEBUG_COM_NO_HEADER("Linear transfer characteristics");
        break;
      default:
        LIBBLU_DEBUG_COM_NO_HEADER("Reserved value");
        return -1;
    }
    LIBBLU_DEBUG_COM_NO_HEADER(" (%02" PRIx8 ").\n", param->transferCharact);

    LIBBLU_DEBUG_COM("Matrix Coefficients: ");
    switch (param->matrixCoeff) {
      case MATRX_COEF_BT709:
        LIBBLU_DEBUG_COM_NO_HEADER("ITU-R BT.709");
        break;
      case MATRX_COEF_UNSPECIFIED:
        LIBBLU_DEBUG_COM_NO_HEADER("Unspecified Video");
        break;
      case MATRX_COEF_FCC:
        LIBBLU_DEBUG_COM_NO_HEADER("ITU-R BT.470-2 System M");
        break;
      case MATRX_COEF_BT470BG:
        LIBBLU_DEBUG_COM_NO_HEADER("ITU-R BT.470-2 System B, G");
        break;
      case MATRX_COEF_SMPTE170:
        LIBBLU_DEBUG_COM_NO_HEADER("SMPTE 170M");
        break;
      case MATRX_COEF_SMPTE240:
        LIBBLU_DEBUG_COM_NO_HEADER("SMPTE 240M (1987)");
        break;
      default:
        LIBBLU_DEBUG_COM_NO_HEADER("Reserved value");
        return -1;
    }
    LIBBLU_DEBUG_COM_NO_HEADER(" (%02" PRIx8 ").\n", param->matrixCoeff);


    if (
      headerParam->vertical_size_value >= 720 && (
      param->colourPrimaries != COLOR_PRIM_BT709 ||
      param->transferCharact != TRANS_CHAR_BT709 ||
      param->matrixCoeff != MATRX_COEF_BT709
    )) {
      LIBBLU_DEBUG_COM("Warning: ITU-R BT.709 color parameters shall be used for HD video.\n");
    }

    if (
      headerParam->vertical_size_value == 480 && (
      param->colourPrimaries != COLOR_PRIM_BT470M ||
      param->transferCharact != TRANS_CHAR_BT470M ||
      param->matrixCoeff != MATRX_COEF_BT470BG
    )) {
      LIBBLU_DEBUG_COM("Warning: ITU-R BT.470-2 System B, G color parameters shall be used for SD NTSC.\n");
    }

    if (
      headerParam->vertical_size_value == 576 && (
      param->colourPrimaries != COLOR_PRIM_BT470BG ||
      param->transferCharact != TRANS_CHAR_BT470BG ||
      param->matrixCoeff != MATRX_COEF_BT470BG
    )) {
      LIBBLU_DEBUG_COM("Warning: ITU-R BT.470-2 System B, G color parameters shall be used for SD PAL.\n");
    }
  }

  LIBBLU_DEBUG_COM("Display size: %dx%d.\n", param->dispHorizontalSize, param->dispVerticalSize);

  if (
    headerParam->horizontal_size_value != param->dispHorizontalSize ||
    headerParam->vertical_size_value != param->dispVerticalSize
  ) {
    LIBBLU_DEBUG_COM(
      "Warning: Display screen over-scan specified is not equal to screen resolution (this may cause image cropping) and shall not used for BDAV."
    );
  }

  return 0;
}
#endif

int constantSequenceDisplayExtensionCheck(H262SequenceDisplayExtensionParameters * first, H262SequenceDisplayExtensionParameters * second)
{
  return
    first->videoFormat == second->videoFormat &&
    first->colourDesc == second->colourDesc &&
    first->colourPrimaries == second->colourPrimaries &&
    first->transferCharact == second->transferCharact &&
    first->matrixCoeff == second->matrixCoeff &&
    first->dispHorizontalSize == second->dispHorizontalSize &&
    first->dispVerticalSize == second->dispVerticalSize
  ;
}

#if 0
void setDefaultSequenceDisplayExtension(
  H262SequenceDisplayExtensionParameters * param,
  H262SequenceHeaderParameters * headerParam
) {
  if (NULL == param || NULL == headerParam)
    return;

  param->videoFormat        = FORMAT_UNSPECIFIED;
  param->colourDesc         = false;
  param->colourPrimaries    = COLOR_PRIM_UNSPECIFIED;
  param->transferCharact    = TRANS_CHAR_UNSPECIFIED;
  param->matrixCoeff        = MATRX_COEF_UNSPECIFIED;
  param->dispHorizontalSize = headerParam->horizontal_size_value;
  param->dispVerticalSize   = headerParam->vertical_size_value;
}
#endif

int checkGopHeaderCompliance(
  const H262GopHeaderParameters * param,
  unsigned gopIndex
)
{
  LIBBLU_DEBUG_COM(
    "GOP timing: %02d:%02d:%02d.%03d.\n",
    param->timeCodeHours, param->timeCodeMinutes, param->timeCodeSeconds, param->timeCodePictures
  );

  if (gopIndex == 0 && (param->timeCodeHours || param->timeCodeMinutes || param->timeCodeSeconds || param->timeCodePictures))
    LIBBLU_DEBUG_COM("Warning: GOP start time-code is not 0.\n");

  if (param->closedGop)
    LIBBLU_DEBUG_COM("Closed GOP.\n");
  else
    LIBBLU_DEBUG_COM("Open GOP.\n");

  if (gopIndex == 0 && !param->closedGop)
    LIBBLU_DEBUG_COM("Warning: It's recommended to use closed GOP for the first one of the stream.\n");

  LIBBLU_DEBUG_COM("Broken link: %s (0x%d).\n", (param->brokenLink) ? "Yes" : "No", param->brokenLink);

  if (param->brokenLink)
    LIBBLU_H262_ERROR_RETURN(
      "Unexpected broken-link signaled in GOP header.\n"
    );

  return 0;
}

int constantGopHeaderCheck(H262GopHeaderParameters * first, H262GopHeaderParameters * second)
{
  return first->dropFrame == second->dropFrame;
}

int checkPictureCodingExtensionCompliance(
  H262PictureCodingExtensionParameters * param,
  H262SequenceExtensionParameters * seqExtParam,
  H262PictureHeaderParameters * pictHeaderParam
)
{
  unsigned i, value;

  /* LIBBLU_DEBUG_COM("  f_codes:\n"); */
  for (i = 0; i < 4; i++) {
    value = param->f_codes[(i >= 2) ? 1 : 0][(i == 1 || i == 3) ? 1 : 0];

    /* LIBBLU_DEBUG_COM( */
      /* "   - f_code[%d][%d] : 0x%X.\n", */
      /* (i >= 2) ? 1 : 0, (i == 1 || i == 3) ? 1 : 0, */
      /* value */
    /* ); */

    /* First check: Values conformity (1-9, 15). */
    if (9 < value && value != 15)
      LIBBLU_H262_ERROR_RETURN(
        "Reserved f_code[%u] value (0x%X).\n",
        i, value
      );

    /* Second check: Forward movments FORBIDDEN in I and P pictures. */
    if (
      (
        H262_PIC_CODING_TYPE_I == pictHeaderParam->picture_coding_type
        || H262_PIC_CODING_TYPE_P == pictHeaderParam->picture_coding_type
      )
      && 2 < i && value != 15
    )
      LIBBLU_H262_ERROR_RETURN(
        "Forbidden f_code[%u] value for a non-B picture (0x%X).\n",
        i, value
      );
  }

  /* LIBBLU_DEBUG_COM("  Intra DC precision: 0x%" PRIx8 ".\n", param->intraDcPrec); */

  /* LIBBLU_DEBUG_COM("  Picture structure type: "); */
  switch (param->pictStruct) {
    case PICTURE_STRUCT_TOP_FIELD:
      /* LIBBLU_DEBUG_COM_NO_HEADER("Top Field (0b01).\n"); */
      break;
    case PICTURE_STRUCT_BOTTOM_FIELD:
      /* LIBBLU_DEBUG_COM_NO_HEADER("Bottom Field (0b10).\n"); */
      break;
    case PICTURE_STRUCT_FRAME_PICTURE:
      /* LIBBLU_DEBUG_COM_NO_HEADER("Frame Picture (0b11).\n"); */
      break;
    default:
      /* LIBBLU_DEBUG_COM_NO_HEADER("Reserved value (0b00).\n"); */
      LIBBLU_H262_ERROR_RETURN(
        "Reserved value in use (picture_structure == 0x%01X).\n",
        param->pictStruct
      );
  }

  if (seqExtParam->progressive_sequence && param->pictStruct != PICTURE_STRUCT_FRAME_PICTURE)
    LIBBLU_H262_ERROR_RETURN(
      "Unexpected interlacing in a progressive sequence.\n"
    );

  /* LIBBLU_DEBUG_COM("  Picture coding flags:\n"); */
  /* LIBBLU_DEBUG_COM("   Top field first:            %s (0x%" PRIx8 ").\n", (param->topFieldFirst) ? "Yes" : "No ", param->topFieldFirst); */
  /* LIBBLU_DEBUG_COM("   Frame pred frame DCT:       %s (0x%" PRIx8 ").\n", (param->framePredFrameDct) ? "Yes" : "No ", param->framePredFrameDct); */
  /* LIBBLU_DEBUG_COM("   Concealment motion vectors: %s (0x%" PRIx8 ").\n", (param->concealmentMotionVectors) ? "Yes" : "No ", param->concealmentMotionVectors); */
  /* LIBBLU_DEBUG_COM("   Q scale type:               %s (0x%" PRIx8 ").\n", (param->qScaleType) ? "Yes" : "No ", param->qScaleType); */
  /* LIBBLU_DEBUG_COM("   Intra vlc format:           %s (0x%" PRIx8 ").\n", (param->intraVlcFormat) ? "Yes" : "No ", param->intraVlcFormat); */
  /* LIBBLU_DEBUG_COM("   Alternate scan:             %s (0x%" PRIx8 ").\n", (param->alternateScan) ? "Yes" : "No ", param->alternateScan); */
  /* LIBBLU_DEBUG_COM("   Repeat first field:         %s (0x%" PRIx8 ").\n", (param->repeatFirstField) ? "Yes" : "No ", param->repeatFirstField); */
  /* LIBBLU_DEBUG_COM("   4:2:0 chroma type:          %s (0x%" PRIx8 ").\n", (param->chroma420Type) ? "Yes" : "No ", param->chroma420Type); */
  /* LIBBLU_DEBUG_COM("   Progressive frame:          %s (0x%" PRIx8 ").\n", (param->progressiveFrame) ? "Yes" : "No ", param->progressiveFrame); */
  /* LIBBLU_DEBUG_COM("   Composite display:          %s (0x%" PRIx8 ").\n", (param->compositeDisplayFlag) ? "Yes" : "No ", param->compositeDisplayFlag); */

  if (param->framePredFrameDct && !seqExtParam->progressive_sequence)
    LIBBLU_H262_ERROR_RETURN(
      "Unexpected interlacing in a progressive sequence.\n"
    );

  #if 0
  if (param->progressiveFrame) {
    if (param->pictStruct != PICTURE_STRUCT_FRAME_PICTURE && param->framePredFrameDct)
      LIBBLU_H262_ERROR_RETURN(
        "Unexpect "
      );
  }
  else if (param->repeatFirstField) {
    return -1;
  }

  if (seqExtParam->chroma_format == H262_CHROMA_FORMAT_420 && ((param->chroma420Type != param->progressiveFrame))) {
    return -1;
  }

  if (param->compositeDisplayFlag) {
    return -1;
  }
  #endif

  return 0;
}

int analyzeH262(
  LibbluESParsingSettings * settings
)
{
  EsmsFileHeaderPtr h262Infos = NULL;
  unsigned h262SourceFileIdx;

  BitstreamReaderPtr m2vInput = NULL;
  BitstreamWriterPtr essOutput = NULL;

  size_t frameOff, ignoredBytes;
  uint64_t gopPts = 0, startPts = 0, h262Pts = 0, h262Dts = 0;
  float frameDuration = 0;
  unsigned long duration;

  unsigned maxPictPerGop;
  unsigned pictNo, gopNo;

  H262SequenceHeaderParameters sequenceHeader = {0};
  H262SequenceExtensionParameters sequenceExtension = {0};
  /* H262GopHeaderParameters gopHeader = {0}; */
  H262PictureHeaderParameters pictureHeader = {0};
  H262PictureCodingExtensionParameters pictureCodingExt = {0};
  H262ExtensionParameters optionalExtensions = {0};
  /* H262SliceParameters slice; */

  /* Used as comparison : */
  H262SequenceHeaderParameters sequenceHeaderParam = {0};
  H262SequenceExtensionParameters sequenceExtensionParam = {0};
  H262SequenceComputedValues sequenceValues;
  H262GopHeaderParameters gopHeaderParam = {0};


  h262Infos = createEsmsFileHandler(ES_VIDEO, settings->options, FMT_SPEC_INFOS_NONE);
  if (NULL == h262Infos)
    return -1;

  /* Pre-gen CRC-32 */
  if (appendSourceFileEsms(h262Infos, settings->esFilepath, &h262SourceFileIdx) < 0)
    return -1;

  /* Open input file : */
  if (NULL == (m2vInput = createBitstreamReader(settings->esFilepath, (size_t) READ_BUFFER_LEN)))
    return -1;

  if (NULL == (essOutput = createBitstreamWriter(settings->scriptFilepath, (size_t) WRITE_BUFFER_LEN)))
    return -1;

  if (writeEsmsHeader(essOutput) < 0)
    return -1;

  maxPictPerGop = pictNo = gopNo = 0;

  while (!isEof(m2vInput)) {
    unsigned gopPictIdx;

    /* Progress bar : */
    printFileParsingProgressionBar(m2vInput);

    switch (nextUint32(m2vInput)) {
      case SEQUENCE_HEADER_CODE:
        /* [v32 sequence_header_code] // 0x000001B3*/
        break;

      case SEQUENCE_END_CODE:
        /* [v32 sequence_end_code] // 0x000001B7 */
        if (skipBytes(m2vInput, 4) < 0)
          return -1;
        continue;

      case GROUP_START_CODE:
        /* [v32 group_start_code] // 0x000001B8 */
        LIBBLU_H262_ERROR_RETURN(
          "Expect a sequence header before GOP header, each GOP shall be "
          "preceded by a sequence header.\n"
        );

      default:
        LIBBLU_H262_ERROR_RETURN(
          "Unknown section start code 0x%08" PRIX32 ".\n",
          nextUint32(m2vInput)
        );
    }

    frameOff = tellPos(m2vInput);

    if (decodeSequenceHeader(m2vInput, &sequenceHeaderParam) < 0)
      return -1;

    if (!sequenceHeader.checkMode) {
      /* Compliance checks */
      if (checkSequenceHeaderCompliance(&sequenceHeaderParam) < 0)
        return -1;
      sequenceHeader = sequenceHeaderParam;

      sequenceHeader.checkMode = true;
      maxPictPerGop =
        upperBoundFrameRateH262FrameRateCode(
          sequenceHeader.frame_rate_code
        ) + 1
      ;
    }
    else {
      /* Consistency check ([1] ITU-T Rec. H.262 6.1.1.6) */
      if (!constantSequenceHeaderCheck(&sequenceHeader, &sequenceHeaderParam))
        LIBBLU_H262_ERROR_RETURN(
          "Sequence header parameters are not constant. Except quantization "
          "matrices, all fields shall remain the same values during the whole "
          "bitstream.\n"
        );
    }

    /* TODO: Support MPEG-1 (without sequence extension) */
    /* If sequence extension present, apply MPEG-2 rules ([1] ITU-T Rec. H.262 6.1.1.6) */

    /* [v32 extension_start_code] // 0x000001B7*/
    if (nextUint32(m2vInput) != EXTENSION_START_CODE)
      LIBBLU_H262_ERROR_RETURN(
        "Expect a sequence extension after sequence header "
        "(MPEG-1 not currently supported).\n"
      );

    if (decodeSequenceExtension(m2vInput, &sequenceExtensionParam) < 0)
      return -1;

    if (!sequenceExtension.checkMode) {
      /* Compliance checks */
      if (checkSequenceExtensionCompliance(&sequenceExtensionParam, &sequenceHeader, settings->options) < 0)
        return -1;

      sequenceExtension = sequenceExtensionParam;
      sequenceExtension.checkMode = true;

      /* Setting H262 Output parameters : */
      setH262SequenceComputedValues(
        &sequenceValues, &sequenceHeader, &sequenceExtension
      );

      h262Infos->prop.codingType = STREAM_CODING_TYPE_H262; /* MPEG-2 */
      h262Infos->prop.videoFormat = sequenceValues.videoFormat;
      h262Infos->prop.frameRate = sequenceValues.frameRateCode;
      h262Infos->prop.profileIDC = sequenceValues.profile;
      h262Infos->prop.levelIDC = sequenceValues.level;

      frameDuration = MAIN_CLOCK_27MHZ / frameRateCodeToFloat(h262Infos->prop.frameRate);
      startPts = (uint64_t) ceil(frameDuration);

      h262Infos->ptsRef = startPts;
      h262Infos->bitrate = sequenceValues.bit_rate * 400;
    }
    else {
      /* Consistency check ([1] ITU-T Rec. H.262 6.1.1.6) */
      if (!constantSequenceExtensionCheck(&sequenceExtension, &sequenceExtensionParam))
        LIBBLU_H262_ERROR_RETURN(
          "Sequence extension parameters are not constant. All fields shall "
          "remain the same values during the whole bitstream.\n"
        );
    }

    /* Optional User_data/Extensions section */
    if (decodeExtensionUserData(m2vInput, &optionalExtensions, EXT_LOC_SEQUENCE_EXTENSION) < 0)
      return -1;

    /* GOP Header : */
    /* [v32 group_start_code] // 0x000001B8 */
    if (nextUint32(m2vInput) != GROUP_START_CODE)
      LIBBLU_H262_ERROR_RETURN(
        "Expect Group of Pictures header or an extension after "
        "sequence header (got 0x%08X).\n",
        nextUint32(m2vInput)
      );

    if (decodeGopHeader(m2vInput, &gopHeaderParam) < 0)
      return -1;

    if (checkGopHeaderCompliance(&gopHeaderParam, gopNo) < 0)
      return -1;
    /* gopHeader = gopHeaderParam; */

    /* Optional User_data/Extensions section */
    if (decodeExtensionUserData(m2vInput, &optionalExtensions, EXT_LOC_GROUP_OF_PICTURE_HEADER) < 0)
      return -1;

    gopPts = startPts + pictNo * frameDuration;
    gopPictIdx = 0;

    while (nextUint32(m2vInput) == PICTURE_START_CODE) {
      unsigned sliceNb;

      /* Picture Header */
      if (decodePictureHeader(m2vInput, &pictureHeader) < 0)
        return -1;

      /* Compliance checks */
      if (checkPictureHeaderCompliance(&pictureHeader, gopPictIdx) < 0)
        return -1;

      /* Picture Coding Extension */
      if (decodePictureCodingExtension(m2vInput, &pictureCodingExt) < 0)
        return -1;

      /* Compliance checks */
      if (checkPictureCodingExtensionCompliance(&pictureCodingExt, &sequenceExtension, &pictureHeader) < 0)
        return -1;

      /* Optional User_data/Extensions section */
      if (decodeExtensionUserData(m2vInput, &optionalExtensions, EXT_LOC_PICTURE_CODING_EXTENSION) < 0)
        return -1;

      /* Slices : */
      sliceNb = 0;
      while (nextUint32(m2vInput) == ((uint32_t) SLICE_START_CODE + (sliceNb & 0xFF))) {
        /* if (decodeSlice(m2vInput, &slice) < 0) */
        if (decodeSlice(m2vInput) < 0)
          return -1;

        if (isEof(m2vInput))
          LIBBLU_H262_ERROR_RETURN(
            "Expect a sequence end code at bitstream end.\n"
          );

        sliceNb++;
      }

      if (sliceNb == 0)
        LIBBLU_H262_ERROR_RETURN(
          "Empty picture, containing no slice.\n"
        );

      /* Writing PES frames cutting commands : */

      ignoredBytes = 0;

      if (nextUint32(m2vInput) == SEQUENCE_END_CODE) {
        if (skipBytes(m2vInput, 4) < 0)
          return -1;

        if (!isEof(m2vInput))
          ignoredBytes = 4; /* If not end of file, ignore useless SEQUENCE_END_CODE. */
      }

      h262Pts = gopPts + pictureHeader.temporal_reference * frameDuration;

      /* LIBBLU_WARNING("#PTS: %" PRIu64 "\n", h262Pts); */

      if (
        H262_PIC_CODING_TYPE_I == pictureHeader.picture_coding_type
        || H262_PIC_CODING_TYPE_P == pictureHeader.picture_coding_type
      ) {
        if (!gopPictIdx)
          h262Dts = gopPts - frameDuration;
        else
          h262Dts = gopPts + (gopPictIdx - 1) * frameDuration;

        /* LIBBLU_WARNING("#DTS: %" PRIu64 "\n", h262Dts); */
      }
      else
        h262Dts = 0;

      if (
        initEsmsVideoPesFrame(
          h262Infos,
          pictureHeader.picture_coding_type,
          H262_PIC_CODING_TYPE_I == pictureHeader.picture_coding_type
          || H262_PIC_CODING_TYPE_P == pictureHeader.picture_coding_type,
          h262Pts, h262Dts
        ) < 0

        || appendAddPesPayloadCommand(
          h262Infos, h262SourceFileIdx, 0x0, frameOff,
          tellPos(m2vInput) - frameOff - ignoredBytes
        ) < 0
      )
        return -1;

      if (writeEsmsPesPacket(essOutput, h262Infos) < 0)
        return -1;

      frameOff = tellPos(m2vInput);
      gopPictIdx++;
    }

    if (gopPictIdx == 0)
      LIBBLU_H262_ERROR_RETURN(
        "Unexpected empty GOP.\n"
      );

    if (maxPictPerGop < gopPictIdx)
      LIBBLU_H262_ERROR_RETURN(
        "Too many pictures in GOP (%u < %u).\n",
        maxPictPerGop,
        gopPictIdx
      );

    pictNo += gopPictIdx;
    gopNo++;
  }

  if (pictNo == 0)
    LIBBLU_H262_ERROR_RETURN(
      "Unexpected empty stream, no picture parsed.\n"
    );

  if (pictNo == 1) /* Only one I picture in signal: Still picture. */
    h262Infos->prop.stillPicture = true;

  closeBitstreamReader(m2vInput);

  /* [u8 endMarker] */
  if (writeByte(essOutput, ESMS_SCRIPT_END_MARKER) < 0)
    return -1;

  /* Display infos : */
  duration = ((gopPts + pictureHeader.temporal_reference * frameDuration) - h262Infos->ptsRef) / MAIN_CLOCK_27MHZ;

  lbc_printf("== Stream Infos =======================================================================\n");
  lbc_printf(
    "Codec: Video/MPEG-2, %dx%d%c, %.3f Im/s.\n",
    sequenceValues.horizontal_size,
    sequenceValues.vertical_size,
    (sequenceExtension.progressive_sequence ? 'p' : 'i'),
    sequenceValues.frame_rate
  );
  lbc_printf(
    "Stream Duration: %02lu:%02lu:%02lu\n",
    (duration / 60) / 60,
    (duration / 60) % 60,
    duration % 60
  );
  lbc_printf("=======================================================================================\n");

  h262Infos->endPts = gopPts + pictureHeader.temporal_reference * frameDuration;

  if (addEsmsFileEnd(essOutput, h262Infos) < 0)
    return -1;
  closeBitstreamWriter(essOutput);

  if (updateEsmsHeader(settings->scriptFilepath, h262Infos) < 0)
    return -1;
  destroyEsmsFileHandler(h262Infos);
  return 0;
}
