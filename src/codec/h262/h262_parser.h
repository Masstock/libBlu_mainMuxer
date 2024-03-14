/** \~english
 * \file h262_parser.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief H.262 video (MPEG-2 Part 2) bitstreams handling module.
 *
 * \todo Split module in sub-modules to increase readability.
 * \todo Require a COMPLETE rewritting, more respecting standard.
 *
 * \xrefitem references "References" "References list"
 *  [1] ITU-T Rec. H.262 (02/2000);\n
 *  [2] ITU-T Rec. H.262 Amd1, Amd2, Amd3 and Amd4;\n
 *  [3] ISO/IEC 11172-2, MPEG-1 Part 2 Video.
 *
 * \note To avoid confusions with MPEG-2 TS, H.262 name is used instead
 * of MPEG-2 Part 2.
 */

#ifndef __LIBBLU_MUXER__CODECS__H262__PARSER_H__
#define __LIBBLU_MUXER__CODECS__H262__PARSER_H__

#include "h262_data.h"
#include "h262_error.h"

#include "../../esms/scriptCreation.h"
#include "../../util.h"
#include "../common/constantCheckFunctionsMacros.h"
#include "../common/esParsingSettings.h"

unsigned getH262MaxBr(
  uint8_t profileIDC,
  uint8_t levelIDC
);

/* Sections Headers codes : */
#define START_CODE_PREFIX                            0x000001
#define PICTURE_START_CODE                           0x00000100
#define USER_DATA_START_CODE                         0x000001B2
#define SEQUENCE_HEADER_CODE                         0x000001B3
#define SEQUENCE_ERROR_CODE                          0x000001B4
#define EXTENSION_START_CODE                         0x000001B5
#define SEQUENCE_END_CODE                            0x000001B7
#define GROUP_START_CODE                             0x000001B8
#define SLICE_START_CODE                             0x00000101

/* Video format values : */
#define FORMAT_COMPONENT                                    0x0
#define FORMAT_PAL                                          0x1
#define FORMAT_NTSC                                         0x2
#define FORMAT_SECAM                                        0x3
#define FORMAT_MAC                                          0x4
#define FORMAT_UNSPECIFIED                                  0x5

/* Colour Primaries values : */
#define COLOR_PRIM_BT709                                    0x1
#define COLOR_PRIM_UNSPECIFIED                              0x2
#define COLOR_PRIM_BT470M                                   0x4
#define COLOR_PRIM_BT470BG                                  0x5
#define COLOR_PRIM_SMPTE170                                 0x6
#define COLOR_PRIM_SMPTE240                                 0x7

/* Transfer Characteristics values : */
#define TRANS_CHAR_BT709                                    0x1
#define TRANS_CHAR_UNSPECIFIED                              0x2
#define TRANS_CHAR_BT470M                                   0x4
#define TRANS_CHAR_BT470BG                                  0x5
#define TRANS_CHAR_SMPTE170                                 0x6
#define TRANS_CHAR_SMPTE240                                 0x7
#define TRANS_CHAR_LINEAR                                   0x8

/* Matrix Coefficients values : */
#define MATRX_COEF_BT709                                    0x1
#define MATRX_COEF_UNSPECIFIED                              0x2
#define MATRX_COEF_FCC                                      0x4
#define MATRX_COEF_BT470BG                                  0x5
#define MATRX_COEF_SMPTE170                                 0x6
#define MATRX_COEF_SMPTE240                                 0x7

/* HDMV MPEG-2 Constraints */
/* Interlaced streams expressed in fields : */
#define MAXGOPLEN_25                                         50
#define MAXGOPLEN_2997                                       60

/* HDMV MPEG-2 Constraints */
/* Progressive streams expressed in pictures : */
#define MAXGOPLEN_23976                                      24
#define MAXGOPLEN_24                                         24
#define MAXGOPLEN_50                                         50
#define MAXGOPLEN_5994                                       60

#define MAX_CONSECUTIVE_B_FRAMES                              2

/* Picture coding types values : */
#if 0
#define PICTURE_I                                           0x1
#define PICTURE_P                                           0x2
#define PICTURE_B                                           0x3
#define PICTURE_D                                           0x4 /* Not used */
#endif

/* Picture structure values : */
#define PICTURE_STRUCT_TOP_FIELD                            0x1
#define PICTURE_STRUCT_BOTTOM_FIELD                         0x2
#define PICTURE_STRUCT_FRAME_PICTURE                        0x3

/* Interlacing Order : */
#define INTERLACING_BOTTOM_FIRST                            0x0
#define INTERLACING_TOP_FIRST                               0x1

typedef enum {
  EXT_LOC_SEQUENCE_EXTENSION,
  EXT_LOC_GROUP_OF_PICTURE_HEADER,
  EXT_LOC_PICTURE_CODING_EXTENSION
} ExtensionDataLocation;

/** \~english
 * \brief
 *
 * Values define DAR (Display Aspect Ratio), SAR (Sample Aspect Ratio) can be
 * derivated as following:
 *
 * If sequence_display_extension() is not present:
 *
 * \f$
 *   SAR = DAR \times \frac{
 *     horizontal\_size
 *   }{
 *     vertical\_size
 *   }
 * \f$
 *
 * Otherwise (if sequence_display_extension() is present):
 *
 * \f$
 *   SAR = DAR \times \frac{
 *     display\_horizontal\_size
 *   }{
 *     display\_vertical\_size
 *   }
 * \f$
 *
 * See [1] ITU-T Rec. H.262 (02/2000) 6.3.3.
 */
typedef enum {
 H262_ASPECT_RATIO_INF_FORBIDDEN  = 0x0,  /**< Forbidden value.              */
 H262_ASPECT_RATIO_INF_SQUARE     = 0x1,  /**< Square pixels.                */
 H262_ASPECT_RATIO_INF_3_BY_4     = 0x2,  /**< 3:4  */
 H262_ASPECT_RATIO_INF_9_BY_16    = 0x3,  /**< 9:16 */
 H262_ASPECT_RATIO_INF_1_BY_2_21  = 0x4,
 H262_ASPECT_RATIO_INF_RES
} H262AspectRatioInformation;

static inline const char *H262AspectRatioInformationStr(
  H262AspectRatioInformation aspect_ratio_information
)
{
  static const char *strings[] = {
    "Forbidden",
    "Square",
    "3:4 DAR",
    "9:16 DAR"
  };

  if (aspect_ratio_information < ARRAY_SIZE(strings))
    return strings[aspect_ratio_information];
  return "Reserved";
}

typedef enum {
  H262_FRAME_RATE_COD_FORBIDDEN  = 0x0,
  H262_FRAME_RATE_COD_23_976     = 0x1,
  H262_FRAME_RATE_COD_24         = 0x2,
  H262_FRAME_RATE_COD_25         = 0x3,
  H262_FRAME_RATE_COD_29_970     = 0x4,
  H262_FRAME_RATE_COD_30         = 0x5,
  H262_FRAME_RATE_COD_50         = 0x6,
  H262_FRAME_RATE_COD_59_94      = 0x7,
  H262_FRAME_RATE_COD_60         = 0x8,
  H262_FRAME_RATE_COD_RES
} H262FrameRateCode;

static inline bool isPalH262FrameRateCode(
  H262FrameRateCode frame_rate_code
)
{
  return
    H262_FRAME_RATE_COD_25 == frame_rate_code
    || H262_FRAME_RATE_COD_50 == frame_rate_code
  ;
}

static inline float frameRateH262FrameRateCode(
  H262FrameRateCode frame_rate_code
)
{
  static const float values[] = {
    -1.0f,
    (24000.0f / 1001.0f),
    24.0f,
    25.0f,
    (30000.0f / 1001.0f),
    30.0f,
    50.0f,
    (60000.0f / 1001.0f),
    60.0f
  };

  if (frame_rate_code < ARRAY_SIZE(values))
    return values[frame_rate_code];
  return -1.0f;
}

static inline unsigned upperBoundFrameRateH262FrameRateCode(
  H262FrameRateCode frame_rate_code
)
{
  static const unsigned values[] = {
    0, 24, 24, 25, 30, 30, 50, 60, 60
  };

  if (frame_rate_code < ARRAY_SIZE(values))
    return values[frame_rate_code];
  return 0;
}

static inline const char *H262FrameRateCodeStr(
  H262FrameRateCode frame_rate_code
)
{
  static const char *strings[] = {
    "Forbidden",
    "23.976 FPS",
    "24 FPS",
    "25 FPS",
    "29.970 FPS",
    "30 FPS",
    "50 FPS",
    "59.940 FPS",
    "60 FPS"
  };

  if (frame_rate_code < ARRAY_SIZE(strings))
    return strings[frame_rate_code];
  return "Reserved";
}

typedef struct {
  bool checkMode; /* Indicates if function need to fill the struct or control if values are the same */

  unsigned horizontal_size_value;
  unsigned vertical_size_value;
  H262AspectRatioInformation aspect_ratio_information;
  H262FrameRateCode frame_rate_code;
  uint32_t bit_rate_value;
  unsigned vbv_buffer_size_value;

  bool constrained_parameters_flag;

  bool load_intra_quantiser_matrix;
  uint8_t intra_quantiser_matrix[64];
  bool load_non_intra_quantiser_matrix;
  uint8_t non_intra_quantiser_matrix[64];
} H262SequenceHeaderParameters;

typedef enum {
  H262_PROFILE_ID_RES                 = 0x0,
  H262_PROFILE_ID_HIGH                = 0x1,
  H262_PROFILE_ID_SPATIALLY_SCALABLE  = 0x2,
  H262_PROFILE_ID_SNR_SCALABLE        = 0x3,
  H262_PROFILE_ID_MAIN                = 0x4,
  H262_PROFILE_ID_SIMPLE              = 0x5
} H262ProfileIdentification;

static inline bool isBdavAllowedH262ProfileIdentification(
  H262ProfileIdentification profileId
)
{
  return H262_PROFILE_ID_MAIN == profileId;
}

static inline bool isReservedH262ProfileIdentification(
  H262ProfileIdentification profileId
)
{
  return
    profileId <= H262_PROFILE_ID_RES
    || H262_PROFILE_ID_SIMPLE < profileId
  ;
}

static inline const char *H262ProfileIdentificationStr(
  H262ProfileIdentification profileId
)
{
  static const char *strings[] = {
    "Reserved",
    "High",
    "Spatially Scalable",
    "SNR Scalable",
    "Main",
    "Simple"
  };

  if (profileId < ARRAY_SIZE(strings))
    return strings[profileId];
  return "Reserved";
}

typedef enum {
  H262_LEVEL_ID_HIGH       = 0x4,
  H262_LEVEL_ID_HIGH_1440  = 0x6,
  H262_LEVEL_ID_MAIN       = 0x8,
  H262_LEVEL_ID_LOW        = 0xA
} H262LevelIdentification;

static inline bool isReservedH262LevelIdentification(
  H262LevelIdentification levelId
)
{
  return !(0 == (levelId & 1) && 2 <= (levelId >> 1) && (levelId >> 1) <= 5);
}

static inline bool isBdavAllowedH262LevelIdentification(
  H262LevelIdentification levelId
)
{
  return
    H262_LEVEL_ID_HIGH == levelId
    && H262_LEVEL_ID_MAIN == levelId
  ;
}

static inline const char *H262LevelIdentificationStr(
  H262LevelIdentification levelId
)
{
  switch (levelId) {
  case H262_LEVEL_ID_HIGH:
    return "High";
  case H262_LEVEL_ID_HIGH_1440:
    return "High 1440";
  case H262_LEVEL_ID_MAIN:
    return "Main";
  case H262_LEVEL_ID_LOW:
    return "Low";
  default:
    break;
  }

  return "Reserved";
}

typedef enum {
  H262_CHROMA_FORMAT_RES  = 0x0,
  H262_CHROMA_FORMAT_420  = 0x1,
  H262_CHROMA_FORMAT_422  = 0x2,
  H262_CHROMA_FORMAT_444  = 0x3
} H262ChromaFormat;

static inline bool isReservedH262ChromaFormat(
  H262ChromaFormat chroma_format
)
{
  return
    chroma_format <= H262_CHROMA_FORMAT_RES
    || H262_CHROMA_FORMAT_444 < chroma_format
  ;
}

static inline const char *H262ChromaFormatStr(
  H262ChromaFormat chroma_format
)
{
  static const char *strings[] = {
    "Reserved",
    "4:2:0",
    "4:2:2",
    "4:4:4"
  };

  if (chroma_format < ARRAY_SIZE(strings))
    return strings[chroma_format];
  return "Reserved";
}

typedef struct {
  bool checkMode;

  struct {
    bool escapeBit;
    H262ProfileIdentification profileIdentification;
    H262LevelIdentification levelIdentification;
  } profile_and_level_identifier;

  bool progressive_sequence;
  H262ChromaFormat chroma_format;

  unsigned horizontal_size_extension;
  unsigned vertical_size_extension;
  uint32_t bit_rate_extension;
  uint32_t vbv_buffer_size_extension;

  bool low_delay;

  unsigned frame_rate_extension_n;
  unsigned frame_rate_extension_d;
} H262SequenceExtensionParameters;

typedef struct {
  unsigned horizontal_size;
  unsigned vertical_size;
  uint32_t bit_rate;
  uint32_t vbv_buffer_size;
  float frame_rate;

  HdmvVideoFormat videoFormat;
  HdmvFrameRateCode frameRateCode;
  H262ProfileIdentification profile;
  H262LevelIdentification level;
} H262SequenceComputedValues;

static inline unsigned computeHorizontalSizeH262SequenceComputedValues(
  const H262SequenceHeaderParameters *header,
  const H262SequenceExtensionParameters *extension
)
{
  return
    (extension->horizontal_size_extension << 12)
    | header->horizontal_size_value
  ;
}

static inline unsigned computeVerticalSizeH262SequenceComputedValues(
  const H262SequenceHeaderParameters *header,
  const H262SequenceExtensionParameters *extension
)
{
  return
    (extension->vertical_size_extension << 12)
    | header->vertical_size_value
  ;
}

static inline uint32_t computeBitrateH262SequenceComputedValues(
  const H262SequenceHeaderParameters *header,
  const H262SequenceExtensionParameters *extension
)
{
  return
    (extension->bit_rate_extension << 18)
    | header->bit_rate_value
  ;
}

static inline uint32_t computeVbvBufSizeH262SequenceComputedValues(
  const H262SequenceHeaderParameters *header,
  const H262SequenceExtensionParameters *extension
)
{
  return
    (extension->vbv_buffer_size_extension << 10)
    | header->vbv_buffer_size_value
  ;
}

static inline float computeFrameRateH262SequenceComputedValues(
  const H262SequenceHeaderParameters *hdr,
  const H262SequenceExtensionParameters *ext
)
{
  return
    (float) frameRateH262FrameRateCode(hdr->frame_rate_code)
    * (ext->frame_rate_extension_n + 1)
    * (ext->frame_rate_extension_d + 1)
  ;
}

static inline void setH262SequenceComputedValues(
  H262SequenceComputedValues *dst,
  const H262SequenceHeaderParameters *hdr,
  const H262SequenceExtensionParameters *ext
)
{
  dst->horizontal_size = computeHorizontalSizeH262SequenceComputedValues(hdr, ext);
  dst->vertical_size = computeVerticalSizeH262SequenceComputedValues(hdr, ext);
  dst->bit_rate = computeBitrateH262SequenceComputedValues(hdr, ext);
  dst->vbv_buffer_size = computeVbvBufSizeH262SequenceComputedValues(hdr, ext);
  dst->frame_rate = computeFrameRateH262SequenceComputedValues(hdr, ext);

  dst->videoFormat = getHdmvVideoFormat(
    dst->horizontal_size,
    dst->vertical_size,
    !ext->progressive_sequence
  );
  dst->frameRateCode = getHdmvFrameRateCode(dst->frame_rate);
  dst->profile = ext->profile_and_level_identifier.profileIdentification;
  dst->level = ext->profile_and_level_identifier.levelIdentification;
}

typedef struct {
  bool checkMode;

  uint8_t videoFormat;

  bool colourDesc;
  uint8_t colourPrimaries;
  uint8_t transferCharact;
  uint8_t matrixCoeff;

  uint16_t dispHorizontalSize;
  uint16_t dispVerticalSize;
} H262SequenceDisplayExtensionParameters;

typedef struct {
  bool dropFrame;
  uint8_t timeCodeHours;
  uint8_t timeCodeMinutes;
  uint8_t timeCodeSeconds;
  uint8_t timeCodePictures;

  bool closedGop;
  bool brokenLink;
} H262GopHeaderParameters;

typedef enum {
  H262_PIC_CODING_TYPE_FORBIDDEN  = 0x0,
  H262_PIC_CODING_TYPE_I          = 0x1,
  H262_PIC_CODING_TYPE_P          = 0x2,
  H262_PIC_CODING_TYPE_B          = 0x3,
  H262_PIC_CODING_TYPE_D          = 0x4
} H262PictureCodingType;

static inline const char *H262PictureCodingTypeStr(
  H262PictureCodingType picture_coding_type
)
{
  static const char *strings[] = {
    "Forbidden",
    "Intra-coded I-Picture",
    "Predictive-coded P-Picture",
    "Bidirectionally-predictive-coded B-Picture",
    "DC Intra-coded D-Picture"
  };

  if (picture_coding_type < ARRAY_SIZE(strings))
    return strings[picture_coding_type];
  return "Reserved";
}

static inline bool isReservedH262PictureCodingType(
  H262PictureCodingType picture_coding_type
)
{
  return
    H262_PIC_CODING_TYPE_FORBIDDEN == picture_coding_type
    || H262_PIC_CODING_TYPE_D < picture_coding_type
  ;
}

typedef struct {
  unsigned temporal_reference;
  H262PictureCodingType picture_coding_type;
  uint16_t vbv_delay;

  bool full_pel_forward_vector;
  uint8_t forward_f_code;

  bool full_pel_backward_vector;
  uint8_t backward_f_code;
} H262PictureHeaderParameters;

typedef struct {
  uint8_t f_codes[2][2];
  uint8_t intraDcPrec;
  uint8_t pictStruct;

  bool topFieldFirst;
  bool framePredFrameDct;
  bool concealmentMotionVectors;
  bool qScaleType;
  bool intraVlcFormat;
  bool alternateScan;
  bool repeatFirstField;
  bool chroma420Type;
  bool progressiveFrame;
  bool compositeDisplayFlag;
} H262PictureCodingExtensionParameters;

typedef struct {
  bool load_intra_quantiser_matrix;
  uint8_t intra_quantiser_matrix[64];

  bool load_non_intra_quantiser_matrix;
  uint8_t non_intra_quantiser_matrix[64];

  bool load_chroma_intra_quantiser_matrix;
  uint8_t chroma_intra_quantiser_matrix[64];

  bool load_chroma_non_intra_quantiser_matrix;
  uint8_t chroma_non_intra_quantiser_matrix[64];
} H262QuantMatrixExtensionParameters;

typedef struct {
  bool copyrightFlag;
  uint8_t copyrightIdentifier;
  bool originalOrCopy;
  uint64_t copyrightNumber;
} H262CopyrightExtensionParameters;

#if 0
typedef struct {
  uint8_t quantiserScaleCode;
  bool intraSlice;
  bool slicePictIdEnable;
  uint8_t slicePictureId;
} H262SliceParameters;
#endif

typedef struct {
  bool sequenceDisplayPresence;
  H262SequenceDisplayExtensionParameters sequenceDisplay;

  /* Place holder for sequence_scalable_extension() */

  bool quantMatrixPresence;
  H262QuantMatrixExtensionParameters quantMatrix;

  bool copyrightPresence;
  H262CopyrightExtensionParameters copyright;

  /* Place holder for picture_display_extension() */

  /* Place holder for picture_spatial_scalable_extension() */

  /* Place holder for picture_temporal_scalable_extension() */

  /* Place holder for camera_parameters_extension() */

  /* Place holder for ITU-T_extension() */
} H262ExtensionParameters;

int analyzeH262(
  LibbluESParsingSettings *settings
);

#endif
