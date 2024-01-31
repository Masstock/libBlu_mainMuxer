/** \~english
 * \file h264_data.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief H.264 video (AVC) bitstreams data structures module.
 */

#ifndef __LIBBLU_MUXER__CODECS__H264__DATA_H__
#define __LIBBLU_MUXER__CODECS__H264__DATA_H__

#include "../../elementaryStreamProperties.h"

#define H264_AR_CHANGE_EXPECTED_SEPARATOR  ':'
#define H264_LEVEL_CHANGE_EXPECTED_SEPARATOR  '.'

/* Switches : */
#define DISABLE_NAL_REPLACEMENT_DATA_OPTIMIZATION  false
#define ALLOW_BUFFERING_PERIOD_CHANGE  true

#define H264_MAX_CHROMA_CHANNELS_NB  2

/** \~english
 * \brief Maximum supported number of Picture Parameters Sets.
 *
 * This value is defined according to the Rec. ITU-T H.264 7.4.2.2 syntax.
 */
#define H264_MAX_PPS  256

/** \~english
 * \brief Maximum allowed number of Picture Parameters Sets.
 *
 * This value may be restrained if required.
 */
#define H264_MAX_ALLOWED_PPS  H264_MAX_PPS

/** \~english
 * \brief Maximum supported number of slice groups in PPS NALUs.
 *
 * This value is defined according to maximum values described in
 * Rec. ITU-T H.264 Annex A.
 */
#define H264_MAX_ALLOWED_SLICE_GROUPS  8

/** \~english
 * \brief Maximum allowed number of consecutive B pictures according to BDAV
 * specifications.
 *
 */
#define H264_BDAV_MAX_CONSECUTIVE_B_PICTURES  3

/** \~english
 * \brief Maximum allowed bit-rate value according to BDAV specifications in
 * bits per second.
 *
 * \note This value is the same for BD-UHD.
 *
 * Defined to 40 Mb/s.
 */
#define H264_BDAV_MAX_BITRATE  40000000

/** \~english
 * \brief Maximum allowed CPB size according to BDAV specifications in bits.
 *
 * \note This value is the same for BD-UHD.
 *
 * Defined to 30 Mb.
 */
#define H264_BDAV_MAX_CPB_SIZE  30000000

/** \~english
 * \brief NAL Unit nal_ref_idc, Reference Indicator value.
 *
 */
typedef enum {
  NAL_REF_IDC_NONE   = 0,
  NAL_REF_IDC_LOW    = 1,
  NAL_REF_IDC_MEDIUM = 2,
  NAL_REF_IDC_HIGH   = 3
} H264NalRefIdcValue;

static inline const char * H264NalRefIdcValueStr(
  H264NalRefIdcValue nal_ref_idc
)
{
  const char * strings[] = {
    "None",
    "Low",
    "Medium",
    "High"
  };

  if (nal_ref_idc < ARRAY_SIZE(strings))
    return strings[nal_ref_idc];
  return "Unknown";
}

/** \~english
 * \brief Return true if NALU contains a SPS, a SPS Extension, a subset SPS,
 * a PPS, a slice of a reference picture, a slice partition of a reference
 * picture, or a prefix NALU preceding a slice of a reference picture.
 *
 * \param nal_ref_idc NALU header nal_ref_idc field value.
 * \return true The NALU might be used as reference.
 * \return false The NALU is not used as reference.
 *
 * Definition of concerned NALUs is from [1] 7.4.1.
 */
static inline bool isReferencePictureH264NalRefIdcValue(
  H264NalRefIdcValue nal_ref_idc
)
{
  return (NAL_REF_IDC_NONE != nal_ref_idc);
}

#define NAL_SPS_REF_IDC  NAL_REF_IDC_HIGH
#define NAL_SEI_REF_IDC  NAL_REF_IDC_LOW

/** \~english
 * \brief NAL Unit nal_unit_type, Type value.
 *
 */
typedef enum {
  NAL_UNIT_TYPE_UNSPECIFIED                           =  0,
  NAL_UNIT_TYPE_NON_IDR_SLICE                         =  1,
  NAL_UNIT_TYPE_SLICE_PART_A_LAYER                    =  2,
  NAL_UNIT_TYPE_SLICE_PART_B_LAYER                    =  3,
  NAL_UNIT_TYPE_SLICE_PART_C_LAYER                    =  4,
  NAL_UNIT_TYPE_IDR_SLICE                             =  5,
  NAL_UNIT_TYPE_SUPPLEMENTAL_ENHANCEMENT_INFORMATION  =  6,
  NAL_UNIT_TYPE_SEQUENCE_PARAMETER_SET                =  7,
  NAL_UNIT_TYPE_PIC_PARAMETER_SET                     =  8,
  NAL_UNIT_TYPE_ACCESS_UNIT_DELIMITER                 =  9,
  NAL_UNIT_TYPE_END_OF_SEQUENCE                       = 10,
  NAL_UNIT_TYPE_END_OF_STREAM                         = 11,
  NAL_UNIT_TYPE_FILLER_DATA                           = 12,
  NAL_UNIT_TYPE_SEQUENCE_PARAMETER_SET_EXTENSION      = 13,
  NAL_UNIT_TYPE_PREFIX_NAL_UNIT                       = 14,
  NAL_UNIT_TYPE_SUBSET_SEQUENCE_PARAMETER_SET         = 15,
  NAL_UNIT_TYPE_DEPTH_PARAMETER_SET                   = 16,
  NAL_UNIT_TYPE_RESERVED_17                           = 17,
  NAL_UNIT_TYPE_RESERVED_18                           = 18,
  NAL_UNIT_TYPE_CODED_SLICE_AUX_CODED_PIC_WO_PART     = 19,
  NAL_UNIT_TYPE_CODED_SLICE_EXT                       = 20,
  NAL_UNIT_TYPE_CODED_SLICE_DEPTH_EXT                 = 21,
  NAL_UNIT_TYPE_RESERVED_22                           = 22,
  NAL_UNIT_TYPE_RESERVED_23                           = 23
} H264NalUnitTypeValue;

/** \~english
 * \brief Return true if coded slice NALU nal_unit_type value correspond
 * to an IRD picture NALU.
 *
 * \param nal_unit_type Slice NALU header nal_unit_type field value.
 * \return true NALU is a coded slice of an IDR picture NALU.
 * \return false NALU is a coded slice of an non-IDR picture NALU.
 */
static inline bool isIdrPictureH264NalUnitTypeValue(
  H264NalUnitTypeValue nal_unit_type
)
{
  return (NAL_UNIT_TYPE_IDR_SLICE == nal_unit_type);
}

/** \~english
 * \brief Return true if coded slice NALU nal_unit_type value is equal to 20
 * or 21.
 *
 * \param nal_unit_type NALU header nal_unit_type field value.
 * \return true NALU is a coded slice extension.
 * \return false NALU is not a coded slice extension.
 *
 * Detected values corresponds to #NAL_UNIT_TYPE_CODED_SLICE_EXT and
 * #NAL_UNIT_TYPE_CODED_SLICE_DEPTH_EXT.
 */
static inline bool isCodedSliceExtensionH264NalUnitTypeValue(
  H264NalUnitTypeValue nal_unit_type
)
{
  return
    NAL_UNIT_TYPE_CODED_SLICE_EXT == nal_unit_type
    || NAL_UNIT_TYPE_CODED_SLICE_DEPTH_EXT == nal_unit_type
  ;
}

/** \~english
 * \brief Return a constant string representation of a nal_unit_type value.
 *
 * \param nal_unit_type Value to represent.
 * \return const char* Constant string representation.
 */
static inline const char * H264NalUnitTypeStr(
  H264NalUnitTypeValue nal_unit_type
)
{
  static const char * strings[] = {
    "Unspecified",
    "Coded Slice (non-IDR picture)",
    "Coded Slice data partition A",
    "Coded Slice data partition B",
    "Coded Slice data partition C",
    "Coded Slice (IDR picture)",
    "Supplemental Enhancement Information (SEI)",
    "Sequence Parameters Set (SPS)",
    "Picture Parameters Set (PPS)",
    "Access Unit Delimiter (AUD)",
    "End of sequence",
    "End of stream",
    "Filler data",
    "Sequence Parameter Set Extension",
    "Prefix NAL Unit",
    "Subset Sequence Parameter Set",
    "Depth Parameter Set",
    "Reserved",
    "Reserved",
    "Coded Slice (Auxiliary coded picture w/o partitioning)",
    "Coded Slice Extension",
    "Coded Slice Extension (Depth View/3D-AVC Texture view component)",
    "Reserved",
    "Reserved"
  };

  if (nal_unit_type < ARRAY_SIZE(strings))
    return strings[nal_unit_type];
  return "Unspecified";
}

typedef struct {
  H264NalRefIdcValue nal_ref_idc;
  H264NalUnitTypeValue nal_unit_type;

  /* bool firstNalInAU; */

  /* TODO: Add avc_3d_extension_flag and 3D extensions. */
} H264NalHeaderParameters;
/* Network Abstraction Layer Units */

typedef enum {
  H264_PRIM_PIC_TYPE_I       = 0,
  H264_PRIM_PIC_TYPE_IP      = 1,
  H264_PRIM_PIC_TYPE_IPB     = 2,
  H264_PRIM_PIC_TYPE_SI      = 3,
  H264_PRIM_PIC_TYPE_SISP    = 4,
  H264_PRIM_PIC_TYPE_ISI     = 5,
  H264_PRIM_PIC_TYPE_IPSISP  = 6,
  H264_PRIM_PIC_TYPE_ALL     = 7
} H264PrimaryPictureType;

static inline const char * H264PrimaryPictureTypeStr(
  H264PrimaryPictureType val
)
{
  static const char * strings[] = {
    "I",
    "I, P",
    "I, P, B",
    "SI",
    "SI, SP",
    "I, SI",
    "I, P, SI, SP",
    "I, P, B, SI, SP"
  };

  if (val < ARRAY_SIZE(strings))
    return strings[val];
  return "Unknown";
}

typedef struct {
  H264PrimaryPictureType primary_pic_type;
} H264AccessUnitDelimiterParameters;

/* Profile codes                                             */
/* profile_idc values :                                      */
typedef enum {
  H264_PROFILE_UNKNOWN                       = 0,

  /* Annex A Profiles */
  H264_PROFILE_BASELINE                      = 66,
  H264_PROFILE_MAIN                          = 77,
  H264_PROFILE_EXTENDED                      = 88,
  H264_PROFILE_HIGH                          = 100,
  H264_PROFILE_HIGH_10                       = 110,
  H264_PROFILE_HIGH_422                      = 122,
  H264_PROFILE_HIGH_444_PREDICTIVE           = 244,
  H264_PROFILE_CAVLC_444_INTRA               = 44,

  H264_PROFILE_SCALABLE_BASELINE             = 83,
  H264_PROFILE_SCALABLE_HIGH                 = 86,
  H264_PROFILE_MULTIVIEW_HIGH                = 118,
  H264_PROFILE_STEREO_HIGH                   = 128,
  H264_PROFILE_MFC_HIGH                      = 134,
  H264_PROFILE_DEPTH_MFC_HIGH                = 135,
  H264_PROFILE_MULTIVIEW_DEPTH_HIGH          = 138,
  H264_PROFILE_ENHANCED_MULTIVIEW_DEPTH_HIGH = 139,
} H264ProfileIdcValue;

static inline bool isBaselineMainExtendedH264ProfileIdcValue(
  H264ProfileIdcValue profile_idc
)
{
  return (
    profile_idc == H264_PROFILE_BASELINE
    || profile_idc == H264_PROFILE_MAIN
    || profile_idc == H264_PROFILE_EXTENDED

    || profile_idc == H264_PROFILE_SCALABLE_BASELINE
  );
}

static inline bool isHighH264ProfileIdcValue(
  H264ProfileIdcValue profile_idc
)
{
  return (
    profile_idc == H264_PROFILE_HIGH
    || profile_idc == H264_PROFILE_HIGH_10
    || profile_idc == H264_PROFILE_HIGH_422
    || profile_idc == H264_PROFILE_HIGH_444_PREDICTIVE
    || profile_idc == H264_PROFILE_CAVLC_444_INTRA
  );
}

static inline bool isHighScalableMultiviewH264ProfileIdcValue(
  H264ProfileIdcValue profile_idc
)
{
  return (
    isHighH264ProfileIdcValue(profile_idc)
    || profile_idc == H264_PROFILE_SCALABLE_HIGH
    || profile_idc == H264_PROFILE_MULTIVIEW_HIGH
    || profile_idc == H264_PROFILE_STEREO_HIGH
    || profile_idc == H264_PROFILE_MFC_HIGH
    || profile_idc == H264_PROFILE_DEPTH_MFC_HIGH
    || profile_idc == H264_PROFILE_MULTIVIEW_DEPTH_HIGH
    || profile_idc == H264_PROFILE_ENHANCED_MULTIVIEW_DEPTH_HIGH
  );
}

#define H264_LEVEL_1B 9 /* BUG: This value is not fixed. */

typedef struct {
  bool set0;
  bool set1;
  bool set2;
  bool set3;
  bool set4;
  bool set5;
  uint8_t reservedFlags;
} H264ContraintFlags;

static inline uint8_t valueH264ContraintFlags(
  H264ContraintFlags flags
)
{
  return
    (flags.set0   << 7)
    | (flags.set1 << 6)
    | (flags.set2 << 5)
    | (flags.set3 << 4)
    | (flags.set4 << 3)
    | (flags.set5 << 2)
    | (flags.reservedFlags & 0x3)
  ;
}

const char * H264ProfileIdcValueStr(
  H264ProfileIdcValue val,
  H264ContraintFlags constraints
);

/* Chroma format codes                                       */
/* chroma_format_idc values :                                */
typedef enum {
  H264_CHROMA_FORMAT_400 = 0,
  H264_CHROMA_FORMAT_420 = 1,
  H264_CHROMA_FORMAT_422 = 2,
  H264_CHROMA_FORMAT_444 = 3
} H264ChromaFormatIdcValue;

static inline const char * H264ChromaFormatIdcValueStr(
  H264ChromaFormatIdcValue val
)
{
  static const char * strings[] = {
    "4:0:0",
    "4:2:0",
    "4:2:2",
    "4:4:4"
  };

  if (val < ARRAY_SIZE(strings))
    return strings[val];
  return "Unknown";
}

typedef struct {
  bool seq_scaling_list_present_flag[12];
  uint8_t ScalingList4x4[6][16];
  uint8_t ScalingList8x8[6][64];
  bool UseDefaultScalingMatrix4x4Flag[6];
  bool UseDefaultScalingMatrix8x8Flag[6];
} H264SeqScalingMatrix;

typedef struct {
  int64_t BitRate;
  int64_t CpbSize;
  bool cbr_flag;

  uint32_t bit_rate_value_minus1;
  uint32_t cpb_size_value_minus1;
} H264SchedSel;

#define H264_MAX_CPB_CONFIGURATIONS  32

typedef struct {
  unsigned cpb_cnt_minus1;
  uint8_t bit_rate_scale;
  uint8_t cpb_size_scale;

  H264SchedSel schedSel[H264_MAX_CPB_CONFIGURATIONS];

  uint8_t initial_cpb_removal_delay_length_minus1;
  uint8_t cpb_removal_delay_length_minus1;
  uint8_t dpb_output_delay_length_minus1;
  uint8_t time_offset_length;
} H264HrdParameters;

static inline int64_t BitRateH264HrdParameters(
  H264HrdParameters hrd_parameters,
  unsigned SchedSelIdx
)
{
  assert(SchedSelIdx <= hrd_parameters.cpb_cnt_minus1);

  return hrd_parameters.schedSel[SchedSelIdx].BitRate;
}

static inline int64_t CpbSizeH264HrdParameters(
  H264HrdParameters hrd_parameters,
  unsigned SchedSelIdx
)
{
  assert(SchedSelIdx <= hrd_parameters.cpb_cnt_minus1);

  return hrd_parameters.schedSel[SchedSelIdx].CpbSize;
}

static inline bool cbr_flagH264HrdParameters(
  H264HrdParameters hrd_parameters,
  unsigned SchedSelIdx
)
{
  assert(SchedSelIdx <= hrd_parameters.cpb_cnt_minus1);

  return hrd_parameters.schedSel[SchedSelIdx].cbr_flag;
}

typedef enum {
  H264_ASPECT_RATIO_IDC_ERROR        = -1,

  H264_ASPECT_RATIO_IDC_UNSPECIFIED  = 0,
  H264_ASPECT_RATIO_IDC_1_BY_1       = 1,
  H264_ASPECT_RATIO_IDC_12_BY_11     = 2,
  H264_ASPECT_RATIO_IDC_10_BY_11     = 3,
  H264_ASPECT_RATIO_IDC_16_BY_11     = 4,
  H264_ASPECT_RATIO_IDC_40_BY_33     = 5,
  H264_ASPECT_RATIO_IDC_24_BY_11     = 6,
  H264_ASPECT_RATIO_IDC_20_BY_11     = 7,
  H264_ASPECT_RATIO_IDC_32_BY_11     = 8,
  H264_ASPECT_RATIO_IDC_80_BY_33     = 9,
  H264_ASPECT_RATIO_IDC_18_BY_11     = 10,
  H264_ASPECT_RATIO_IDC_15_BY_11     = 11,
  H264_ASPECT_RATIO_IDC_64_BY_33     = 12,
  H264_ASPECT_RATIO_IDC_160_BY_99    = 13,
  H264_ASPECT_RATIO_IDC_4_BY_3       = 14,
  H264_ASPECT_RATIO_IDC_3_BY_2       = 15,
  H264_ASPECT_RATIO_IDC_2_BY_1       = 16,
  H264_ASPECT_RATIO_IDC_EXTENDED_SAR = 255
} H264AspectRatioIdcValue;

static inline const char * H264AspectRatioIdcValueStr(
  const H264AspectRatioIdcValue val
)
{
  static const char * strings[] = {
    "Unspecified",
    "1:1",
    "12:11",
    "10:11",
    "16:11",
    "40:33",
    "24:11",
    "20:11",
    "32:11",
    "80:33",
    "18:11",
    "15:11",
    "64:33",
    "160:99",
    "4:3",
    "3:2",
    "2:1"
  };

  if ((unsigned) val < ARRAY_SIZE(strings))
    return strings[val];
  if (val == H264_ASPECT_RATIO_IDC_EXTENDED_SAR)
    return "Extended_SAR";
  return "Unknown";
}

typedef enum {
  H264_OVERSCAN_APPROP_ADD_MARGIN   = 0,
  H264_OVERSCAN_APPROP_CROP         = 1,
  H264_OVERSCAN_APPROP_UNSPECIFIED  = 3
} H264OverscanAppropriateValue;

static inline const char * H264OverscanAppropriateValueStr(
  H264OverscanAppropriateValue val
)
{
  static const char * strings[] = {
    "Avoid overscan",
    "Use overscan",
    "Unspecified"
  };

  if (val < ARRAY_SIZE(strings))
    return strings[val];
  return "Unknown";
}

typedef enum {
  H264_VIDEO_FORMAT_COMPONENT    = 0,
  H264_VIDEO_FORMAT_PAL          = 1,
  H264_VIDEO_FORMAT_NTSC         = 2,
  H264_VIDEO_FORMAT_SECAM        = 3,
  H264_VIDEO_FORMAT_MAC          = 4,
  H264_VIDEO_FORMAT_UNSPECIFIED  = 5,
} H264VideoFormatValue;

static inline const char * H264VideoFormatValueStr(
  H264VideoFormatValue val
)
{
  static const char * strings[] = {
    "Component",
    "PAL",
    "NTSC",
    "SECAM",
    "MAC",
    "Unspecified"
  };

  if (val < ARRAY_SIZE(strings))
    return strings[val];
  return "Unknown";
}

typedef enum {
  H264_COLOR_PRIM_BT709        = 1,  /* BT.709     */
  H264_COLOR_PRIM_UNSPECIFIED  = 2,
  H264_COLOR_PRIM_BT470M       = 4,  /* BT.470M    */
  H264_COLOR_PRIM_BT470BG      = 5,  /* BT.470BG   */
  H264_COLOR_PRIM_SMPTE170M    = 6,  /* SMPTE 170M */
  H264_COLOR_PRIM_SMPTE240M    = 7,  /* SMPTE 240M */
  H264_COLOR_PRIM_GENERIC      = 8,
  H264_COLOR_PRIM_BT2020       = 9,  /* BT.2020    */
  H264_COLOR_PRIM_CIEXYZ       = 10, /* CIE XYZ    */
  H264_COLOR_PRIM_DCIP3        = 11, /* DCI-P3     */
  H264_COLOR_PRIM_P3D65        = 12, /* Display P3 */
  H264_COLOR_PRIM_EBU3213      = 22, /* EBU 3213-E */
} H264ColourPrimariesValue;

static inline const char * H264ColorPrimariesValueStr(
  H264ColourPrimariesValue val
)
{
  static const char * strings[] = {
    "Unknown",
    "BT.709",
    "Unspecified",
    "Unknown",
    "BT.470M",
    "BR.470BG",
    "SMPTE 170M",
    "SMPTE 240M",
    "Generic",
    "BT.2020",
    "CIE XYZ",
    "DCI-P3",
    "Display P3"
  };

  if (val < ARRAY_SIZE(strings))
    return strings[val];
  if (val == H264_COLOR_PRIM_EBU3213)
    return "EBU 3213-E";
  return "Unknown";
}

typedef enum {
  H264_TRANS_CHAR_BT709        = 1,  /* BT.709     */
  H264_TRANS_CHAR_UNSPECIFIED  = 2,
  H264_TRANS_CHAR_BT470M       = 4,  /* BT.470M    */
  H264_TRANS_CHAR_BT470BG      = 5,  /* BT.470BG   */
  H264_TRANS_CHAR_SMPTE170M    = 6,  /* SMPTE 170M */
  H264_TRANS_CHAR_SMPTE240M    = 7,  /* SMPTE 240M */
  H264_TRANS_CHAR_LINEAR       = 8,
  H264_TRANS_CHAR_LOG          = 9,  /* Log 100:1  */
  H264_TRANS_CHAR_LOG_10       = 10, /* Log 100*Sqrt(10):1 */
  H264_TRANS_CHAR_XVYCC        = 11, /* xvYCC      */
  H264_TRANS_CHAR_BT1361       = 12, /* BT.1361-0  */
  H264_TRANS_CHAR_SRGB         = 13, /* sRGB       */
  H264_TRANS_CHAR_BT2020_10    = 14, /* BT.2020 10bits     */
  H264_TRANS_CHAR_BT2020_12    = 15, /* BT.2020 12bits     */
  H264_TRANS_CHAR_SMPTE2084PQ  = 16, /* SMPTE 2084PQ       */
  H264_TRANS_CHAR_SMPTEST428   = 17, /* SMPTE ST428-1      */
  H264_TRANS_CHAR_BT2100       = 18, /* BT.2100-2 HLG      */
} H264TransferCharacteristicsValue;

static inline const char * H264TransferCharacteristicsValueStr(
  H264TransferCharacteristicsValue val
)
{
  static const char * strings[] = {
    "Unknown",
    "BT.709",
    "Unspecified",
    "Unknown",
    "BT.470M",
    "BR.470BG",
    "SMPTE 170M",
    "SMPTE 240M",
    "Linear",
    "Log 100:1",
    "Log 100 * Sqrt(10):1",
    "xvYCC",
    "BT.1361-0",
    "sRGB",
    "BT.2020 10 bits",
    "BT.2020 12 bits",
    "SMPTE 2084PQ",
    "SMPTE ST428-1",
    "BT.2100-2 HLG"
  };

  if (val < ARRAY_SIZE(strings))
    return strings[val];
  return "Unknown";
}

typedef enum {
  H264_MATRX_COEF_SGRB         = 0,  /* sRGB       */
  H264_MATRX_COEF_BT709        = 1,  /* BT.709     */
  H264_MATRX_COEF_UNSPECIFIED  = 2,
  H264_MATRX_COEF_BT470M       = 4,  /* BT.470M    */
  H264_MATRX_COEF_BT470BG      = 5,  /* BT.470BG   */
  H264_MATRX_COEF_SMPTE170M    = 6,  /* SMPTE 170M */
  H264_MATRX_COEF_SMPTE240M    = 7,  /* SMPTE 240M */
  H264_MATRX_COEF_YCGCO        = 8,  /* YCoCg      */
  H264_MATRX_COEF_BT2100       = 9,  /* BT.2100-2 Y′CbCr   */
  H264_MATRX_COEF_BT2020       = 10, /* BT.2020    */
  H264_MATRX_COEF_SMPTEST2085  = 11, /* SMPTE ST 2085      */
  H264_MATRX_COEF_CHRM_NCONST  = 12, /* Chromaticity-derived non-constant luminance system */
  H264_MATRX_COEF_CHRM_CONST   = 13, /* Chromaticity-derived constant luminance system */
  H264_MATRX_COEF_ICTCP        = 14, /* BT.2100-2 ICtCp    */
} H264MatrixCoefficientsValue;

static inline const char * H264MatrixCoefficientsValueStr(
  H264MatrixCoefficientsValue val
)
{
  static const char * strings[] = {
    "sRGB/XYZ",
    "BT.709",
    "Unspecified",
    "Unknown",
    "BT.470M",
    "BR.470BG",
    "SMPTE 170M",
    "SMPTE 240M",
    "YCoCg",
    "BT.2100-2 Y'CbCr",
    "BT.2020",
    "SMPTE ST 2085",
    "Chromaticity-derived non-constant luminance system",
    "Chromaticity-derived constant luminance system",
    "BT.2100-2 ICtCp"
  };

  if (val < ARRAY_SIZE(strings))
    return strings[val];
  return "Unknown";
}

typedef struct {
  H264ColourPrimariesValue colour_primaries;
  H264TransferCharacteristicsValue transfer_characteristics;
  H264MatrixCoefficientsValue matrix_coefficients;
} H264VuiColourDescriptionParameters;

/** \~english
 * \brief H.264 VUI coded video sequence bitstream restrictions parameters.
 *
 * Described in [1] E.2.1 VUI parameters semantics.
 */
typedef struct {
  bool motion_vectors_over_pic_boundaries_flag;
  unsigned max_bytes_per_pic_denom;
  unsigned max_bits_per_mb_denom;
  unsigned log2_max_mv_length_horizontal;
  unsigned log2_max_mv_length_vertical;
  unsigned max_num_reorder_frames;
  unsigned max_dec_frame_buffering;
} H264VuiVideoSeqBsRestrParameters;

typedef struct {
  bool aspect_ratio_info_present_flag;
  H264AspectRatioIdcValue aspect_ratio_idc;
  struct {
    unsigned sar_width;
    unsigned sar_height;
  };  /**< aspect_ratio_idc == #H264_ASPECT_RATIO_IDC_EXTENDED_SAR related
    parameters.                                                              */

  bool overscan_info_present_flag;
  H264OverscanAppropriateValue overscan_appropriate_flag;

  bool video_signal_type_present_flag;
  struct {
    H264VideoFormatValue video_format;
    bool video_full_range_flag;
    bool colour_description_present_flag;
    H264VuiColourDescriptionParameters colour_description;
  };

  bool chroma_loc_info_present_flag;
  struct {
    unsigned chroma_sample_loc_type_top_field;
    unsigned chroma_sample_loc_type_bottom_field;
  };

  bool timing_info_present_flag;
  struct {
    uint32_t num_units_in_tick;
    uint32_t time_scale;

    bool fixed_frame_rate_flag;

    double FrameRate;  /**< Computed frame rate value, this variable is not
      described in the H.264 specs. Equal to (time_scale /
      (num_units_in_tick * 2)).                                              */
    unsigned MaxFPS;
  };

  bool nal_hrd_parameters_present_flag;
  H264HrdParameters nal_hrd_parameters;
  bool vcl_hrd_parameters_present_flag;
  H264HrdParameters vcl_hrd_parameters;
  bool CpbDpbDelaysPresentFlag;

  bool low_delay_hrd_flag;

  bool pic_struct_present_flag;
  bool bitstream_restriction_flag;
  H264VuiVideoSeqBsRestrParameters bistream_restrictions;
} H264VuiParameters;

typedef struct {
  H264ProfileIdcValue profile_idc;
  H264ContraintFlags constraint_set_flags;
  uint8_t level_idc;

  unsigned seq_parameter_set_id;

  struct {
    H264ChromaFormatIdcValue chroma_format_idc;
    bool separate_colour_plane_flag;

    uint8_t bit_depth_luma_minus8;
    uint8_t bit_depth_chroma_minus8;
    bool qpprime_y_zero_transform_bypass_flag;

    bool seq_scaling_matrix_present_flag;
    H264SeqScalingMatrix seq_scaling_matrix;
  };  /**< 'High' profile_idc related parameters. */

  uint8_t log2_max_frame_num_minus4;

  uint8_t pic_order_cnt_type;

  uint8_t log2_max_pic_order_cnt_lsb_minus4;

  bool delta_pic_order_always_zero_flag;
  int offset_for_non_ref_pic;
  int offset_for_top_to_bottom_field;
  unsigned num_ref_frames_in_pic_order_cnt_cycle;
  int offset_for_ref_frame[255];

  unsigned max_num_ref_frames;
  bool gaps_in_frame_num_value_allowed_flag;

  unsigned pic_width_in_mbs_minus1;
  unsigned pic_height_in_map_units_minus1;
  bool frame_mbs_only_flag;

  bool mb_adaptive_frame_field_flag;
  bool direct_8x8_inference_flag;

  bool frame_cropping_flag;
  struct {
    unsigned left;
    unsigned right;
    unsigned top;
    unsigned bottom;
  } frame_crop_offset;

  bool vui_parameters_present_flag;
  H264VuiParameters vui_parameters;

  /* Computed parameters: */
  unsigned BitDepthLuma;
  unsigned QpBdOffsetLuma;
  unsigned BitDepthChroma;
  unsigned QpBdOffsetChroma;
  unsigned ChromaArrayType;

  unsigned MaxFrameNum;

  uint64_t RawMbBits;

  unsigned MaxPicOrderCntLsb;

  unsigned ExpectedDeltaPerPicOrderCntCycle;

  unsigned SubWidthC;
  unsigned MbWidthC;
  unsigned PicWidthInMbs;
  unsigned PicWidthInSamplesL;
  unsigned PicWidthInSamplesC;

  unsigned SubHeightC;
  unsigned MbHeightC;
  unsigned PicHeightInMapUnits;
  unsigned PicSizeInMapUnits;
  unsigned FrameHeightInMbs;

  unsigned CropUnitX;
  unsigned CropUnitY;

  unsigned FrameWidth;
  unsigned FrameHeight;
} H264SequenceParametersSetDataParameters, H264SPSDataParameters;

typedef struct {
  H264SequenceParametersSetDataParameters data;
} H264SequenceParametersSetParameters;

typedef enum {
  H264_SLICE_GROUP_TYPE_INTERLEAVED         = 0,
  H264_SLICE_GROUP_TYPE_DISPERSED           = 1,
  H264_SLICE_GROUP_TYPE_FOREGROUND_LEFTOVER = 2,
  H264_SLICE_GROUP_TYPE_CHANGING_1          = 3,
  H264_SLICE_GROUP_TYPE_CHANGING_2          = 4,
  H264_SLICE_GROUP_TYPE_CHANGING_3          = 5,
  H264_SLICE_GROUP_TYPE_EXPLICIT            = 6
} H264SliceGroupMapTypeValue;

static inline bool isChanging123H264SliceGroupMapTypeValue(
  H264SliceGroupMapTypeValue slice_group_map_type
)
{
  return
    H264_SLICE_GROUP_TYPE_CHANGING_1 == slice_group_map_type
    || H264_SLICE_GROUP_TYPE_CHANGING_2 == slice_group_map_type
    || H264_SLICE_GROUP_TYPE_CHANGING_3 == slice_group_map_type
  ;
}

typedef enum {
  H264_WEIGHTED_PRED_B_SLICES_DEFAULT  = 0,
  H264_WEIGHTED_PRED_B_SLICES_EXPLICIT = 1,
  H264_WEIGHTED_PRED_B_SLICES_IMPLICIT = 2,
  H264_WEIGHTED_PRED_B_SLICES_RESERVED = 3
} H264WeightedBipredIdc;

static inline const char * H264WeightedBipredIdcStr(
  H264WeightedBipredIdc val
)
{
  static const char * strings[] = {
    "Default",
    "Explicit",
    "Implicit"
  };

  if (val < ARRAY_SIZE(strings))
    return strings[val];
  return "Unknown";
}

typedef struct {
  H264SliceGroupMapTypeValue slice_group_map_type;

  union {
    /* H264_SLICE_GROUP_TYPE_INTERLEAVED : */
    unsigned run_length_minus1[H264_MAX_ALLOWED_SLICE_GROUPS];

    /* H264_SLICE_GROUP_TYPE_FOREGROUND_LEFTOVER : */
    struct {
      unsigned top_left[H264_MAX_ALLOWED_SLICE_GROUPS];
      unsigned bottom_right[H264_MAX_ALLOWED_SLICE_GROUPS];
    };

    /* H264_SLICE_GROUP_TYPE_CHANGING_123 : */
    struct {
      bool slice_group_change_direction_flag;
      unsigned slice_group_change_rate_minus1;
    };

    /* H264_SLICE_GROUP_TYPE_EXPLICIT : */
    unsigned pic_size_in_map_units_minus1;
    /* slice_group_id not parsed. */
  } data;
} H264PicParametersSetSliceGroupsParameters;

typedef struct {
  unsigned pic_parameter_set_id;
  unsigned seq_parameter_set_id;

  bool entropy_coding_mode_flag;
  bool bottom_field_pic_order_in_frame_present_flag;

  unsigned num_slice_groups_minus1;
  H264PicParametersSetSliceGroupsParameters slice_groups;

  unsigned num_ref_idx_l0_default_active_minus1;
  unsigned num_ref_idx_l1_default_active_minus1;

  bool weighted_pred_flag;
  H264WeightedBipredIdc weighted_bipred_idc;

  int pic_init_qp_minus26;
  int pic_init_qs_minus26;
  int chroma_qp_index_offset;

  bool deblocking_filter_control_present_flag;
  bool constrained_intra_pred_flag;
  bool redundant_pic_cnt_present_flag;

  bool picParamExtensionPresent;
  bool transform_8x8_mode_flag;

  bool pic_scaling_matrix_present_flag;
  unsigned nbScalingMatrix;
  bool pic_scaling_list_present_flag[12];
  uint8_t ScalingList4x4[6][16];
  uint8_t ScalingList8x8[6][64];
  bool UseDefaultScalingMatrix4x4Flag[6];
  bool UseDefaultScalingMatrix8x8Flag[6];

  int second_chroma_qp_index_offset;
} H264PicParametersSetParameters;

typedef struct {
  uint32_t initial_cpb_removal_delay;
  uint32_t initial_cpb_removal_delay_offset;
} H264SeiBufferingPeriodSchedSel;

typedef struct {
  unsigned seq_parameter_set_id;

  H264SeiBufferingPeriodSchedSel nal_hrd_parameters[H264_MAX_CPB_CONFIGURATIONS];
  H264SeiBufferingPeriodSchedSel vcl_hrd_parameters[H264_MAX_CPB_CONFIGURATIONS];
} H264SeiBufferingPeriod;

typedef enum {
  H264_PIC_STRUCT_FRAME            =  0, /* Progressive frame */
  H264_PIC_STRUCT_TOP_FIELD        =  1, /* Interlaced top field */
  H264_PIC_STRUCT_BOTTOM_FIELD     =  2, /* Interlaced bottom field */
  H264_PIC_STRUCT_TOP_FLWD_BOTTOM  =  3, /* Interlaced top field followed by bottom field */
  H264_PIC_STRUCT_BOTTOM_FLWD_TOP  =  4, /* Interlaced bottom field followed by top field */
  H264_PIC_STRUCT_TOP_FLWD_BOT_TOP =  5, /* Interlaced top, bottom and bottom repeated fields */
  H264_PIC_STRUCT_BOT_FLWD_TOP_BOT =  6, /* Interlaced bottom, top, bottom repeated fields */
  H264_PIC_STRUCT_DOUBLED_FRAME    =  7, /* Progressive frame displayed twice */
  H264_PIC_STRUCT_TRIPLED_FRAME    =  8  /* Progressive frame displayed three times */
} H264PicStructValue;

static inline const char * H264PicStructValueStr(
  H264PicStructValue val
)
{
  static const char * strings[] = {
    "(Progressive) Frame",
    "Top field",
    "Bottom field",
    "Top field, Bottom field",
    "Bottom field, Top field",
    "Top field, Bottom field, Top field repeated",
    "Bottom field, Top field, Bottom field repeated",
    "Frame doubling",
    "Frame tripling"
  };

  if (0 <= val && val < ARRAY_SIZE(strings))
    return strings[val];
  return "Unknown";
}

#define H264_PIC_STRUCT_MAX_NUM_CLOCK_TS  3

typedef enum {
  H264_CT_TYPE_PROGRESSIVE = 0,
  H264_CT_TYPE_INTERLACED  = 1,
  H264_CT_TYPE_UNKNOWN     = 2,
  H264_CT_TYPE_RESERVED    = 3
} H264CtTypeValue;

static inline const char * H264CtTypeValueStr(
  H264CtTypeValue val
)
{
  static const char * strings[] = {
    "Progressive",
    "Interlaced",
    "Unknown"
  };

  if (0 <= val && val < ARRAY_SIZE(strings))
    return strings[val];
  return "Unknown";
}

typedef enum {
  H264_COUNTING_T_NO_DROP_UNUSED_TIME_OFF  = 0,
  H264_COUNTING_T_NO_DROP                  = 1,
  H264_COUNTING_T_DROP_ZEROS               = 2,
  H264_COUNTING_T_DROP_MAXFPS              = 3,
  H264_COUNTING_T_DROP_ZEROS_AND_ONES      = 4,
  H264_COUNTING_T_DROP_UNSPECIFIED_INDIV   = 5,
  H264_COUNTING_T_DROP_UNSPECIFIED_NB      = 6
} H264CountingTypeValue;

static inline const char * H264CountingTypeValueStr(
  H264CountingTypeValue val
)
{
  static const char * strings[] = {
    "No dropping of n_frames count values and no use of time_offset",
    "No dropping of n_frames count values",
    "Dropping of individual zero values of n_frames count",
    "Dropping of individual MaxFPS - 1 values of n_frames count",
    "dropping of the two lowest (value 0 and 1) n_frames counts when "
    "seconds_value is equal to 0 and minutes_value is not an integer "
    "multiple of 10",
    "dropping of unspecified individual n_frames count values",
    "dropping of unspecified numbers of unspecified n_frames count values"
  };

  if (0 <= val && val < ARRAY_SIZE(strings))
    return strings[val];
  return "Unknown";
}

typedef struct {
  H264CtTypeValue ct_type;
  bool nuit_field_based_flag;
  H264CountingTypeValue counting_type;
} H264ClockTimestampParam;

typedef struct {
  /* if CpbDpbDelaysPresentFlag*/
  uint32_t cpb_removal_delay;
  uint32_t dpb_output_delay;

  /* if pic_struct_present_flag */
  H264PicStructValue pic_struct;
  unsigned NumClockTS;

  bool clock_timestamp_flag[H264_PIC_STRUCT_MAX_NUM_CLOCK_TS];
  H264ClockTimestampParam clock_timestamp[
    H264_PIC_STRUCT_MAX_NUM_CLOCK_TS
  ];
  struct {
    bool full_timestamp_flag;
    bool discontinuity_flag;
    bool cnt_dropped_flag;
    unsigned n_frames;

    bool seconds_flag;
    bool minutes_flag;
    bool hours_flag;

    unsigned seconds_value;
    unsigned minutes_value;
    unsigned hours_value;

    uint32_t time_offset;
  } ClockTimestamp[H264_PIC_STRUCT_MAX_NUM_CLOCK_TS];
} H264SeiPicTiming;

#define H264_MAX_USER_DATA_PAYLOAD_SIZE 1024

typedef struct {
  uint8_t uuid_iso_iec_11578[16];

  bool userDataPayloadOverflow;
  size_t userDataPayloadLength;
  uint8_t userDataPayload[H264_MAX_USER_DATA_PAYLOAD_SIZE];
} H264SeiUserDataUnregistered;

typedef struct {
  unsigned recovery_frame_cnt;
  bool exact_match_flag;
  bool broken_link_flag;
  uint8_t changing_slice_group_idc;
} H264SeiRecoveryPoint;

typedef enum {
  H264_SEI_TYPE_BUFFERING_PERIOD       = 0,
  H264_SEI_TYPE_PIC_TIMING             = 1,
  H264_SEI_TYPE_USER_DATA_UNREGISTERED = 5,
  H264_SEI_TYPE_RECOVERY_POINT         = 6
} H264SeiPayloadTypeValue;

static inline const char * H264SEIMessagePayloadTypeStr(
  H264SeiPayloadTypeValue payloadType
)
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

typedef struct {
  H264SeiPayloadTypeValue payloadType;
  size_t payloadSize;
  bool tooEarlyEnd; /* x264 related error (0 bytes length message). */

  H264SeiBufferingPeriod bufferingPeriod;
  H264SeiPicTiming picTiming;
  H264SeiUserDataUnregistered userDataUnregistered;
  H264SeiRecoveryPoint recoveryPoint;
} H264SeiMessageParameters;

#define H264_MAX_SUPPORTED_RBSP_SEI_MESSAGES 3

typedef struct {
  H264SeiMessageParameters messages[H264_MAX_SUPPORTED_RBSP_SEI_MESSAGES];
  unsigned messagesNb;
} H264SeiRbspParameters;

typedef struct {
  bool bufferingPeriodPresent;
  bool bufferingPeriodValid;
  bool bufferingPeriodGopValid;
  H264SeiBufferingPeriod bufferingPeriod;

  bool picTimingPresent;
  bool picTimingValid;
  H264SeiPicTiming picTiming;

  bool recoveryPointPresent;
  bool recoveryPointValid;
  H264SeiRecoveryPoint recoveryPoint;
} H264SupplementalEnhancementInformationParameters;

typedef enum {
  H264_SLICE_TYPE_P_UNRESCTRICTED   = 0,
  H264_SLICE_TYPE_B_UNRESCTRICTED   = 1,
  H264_SLICE_TYPE_I_UNRESCTRICTED   = 2,
  H264_SLICE_TYPE_SP_UNRESCTRICTED  = 3,
  H264_SLICE_TYPE_SI_UNRESCTRICTED  = 4,
  H264_SLICE_TYPE_P                 = 5,
  H264_SLICE_TYPE_B                 = 6,
  H264_SLICE_TYPE_I                 = 7,
  H264_SLICE_TYPE_SP                = 8,
  H264_SLICE_TYPE_SI                = 9
} H264SliceTypeValue;

static inline bool is_P_H264SliceTypeValue(
  H264SliceTypeValue slice_type
)
{
  return (1 << slice_type) & 0x21;
}

static inline bool is_B_H264SliceTypeValue(
  H264SliceTypeValue slice_type
)
{
  return (1 << slice_type) & 0x42;
}

static inline bool is_I_H264SliceTypeValue(
  H264SliceTypeValue slice_type
)
{
  return (1 << slice_type) & 0x84;
}

static inline bool is_SP_H264SliceTypeValue(
  H264SliceTypeValue slice_type
)
{
  return (1 << slice_type) & 0x108;
}

static inline bool is_SI_H264SliceTypeValue(
  H264SliceTypeValue slice_type
)
{
  return (1 << slice_type) & 0x210;
}

static inline bool isUnrestrictedH264SliceTypeValue(
  H264SliceTypeValue slice_type
)
{
  return slice_type <= 4;
}

/** \~english
 * \brief Check consistency between AUD primary_pic_type and Slice slice_type.
 *
 * \param primary_pic_type
 * \return true
 * \return false
 */
static inline bool isAllowedH264SliceTypeValue(
  H264SliceTypeValue slice_type,
  H264PrimaryPictureType primary_pic_type
)
{
  static const unsigned primPicTypMasks[] = {
    0x84,
    0xA5,
    0xE7,
    0x10,
    0x18,
    0x94,
    0xBD,
    0xFF
  };

  assert(primary_pic_type < ARRAY_SIZE(primPicTypMasks));

  return primPicTypMasks[primary_pic_type] & (1 << slice_type);
}

static inline const char * H264SliceTypeValueStr(
  H264SliceTypeValue val
)
{
  static const char * strings[] = {
    "P Slice Unrestricted",
    "B Slice Unrestricted",
    "I Slice Unrestricted",
    "SP Slice Unrestricted",
    "SI Slice Unrestricted",
    "P Slice",
    "B Slice",
    "I Slice",
    "SP Slice",
    "SI Slice"
  };

  if (0 <= val && val < ARRAY_SIZE(strings))
    return strings[val];
  return "Unknown";
}

#if 0

/** binary mask:
 * 0 - I slice type
 * 1 - P slice type
 * 2 - B slice type
 */
typedef enum {
  H264_RESTRICTED_P_SLICE_TYPE       = 1 << 0,
  H264_RESTRICTED_B_SLICE_TYPE       = 1 << 1,
  H264_RESTRICTED_I_SLICE_TYPE       = 1 << 2,
  H264_RESTRICTED_SP_SLICE_TYPE      = 1 << 3,
  H264_RESTRICTED_SI_SLICE_TYPE      = 1 << 4,

  H264_RESTRICTED_IP_SLICE_TYPES     =
    H264_RESTRICTED_I_SLICE_TYPE
    | H264_RESTRICTED_P_SLICE_TYPE,

  H264_RESTRICTED_IPB_SLICE_TYPES    =
    H264_RESTRICTED_I_SLICE_TYPE
    | H264_RESTRICTED_P_SLICE_TYPE
    | H264_RESTRICTED_B_SLICE_TYPE,

  H264_UNRESTRICTED_SLICE_TYPE       = 0x1F
} H264AllowedSliceTypes;

static inline bool isAllowedH264SliceTypeValue(
  H264SliceTypeValue slice_type,
  H264AllowedSliceTypes allowedSliceTypes
)
{
  return (allowedSliceTypes | allowedSliceTypes << 5) & (1 << slice_type);
}

#endif

typedef enum {
  H264_COLOUR_PLANE_ID_Y   = 0,
  H264_COLOUR_PLANE_ID_CB  = 1,
  H264_COLOUR_PLANE_ID_CR  = 2
} H264ColourPlaneIdValue;

static inline const char * H264ColourPlaneIdValueStr(
  H264ColourPlaneIdValue val
)
{
  static const char * strings[] = {
    "Y (luma)",
    "Cb (blue differential chroma)",
    "Cr (red differential chroma)"
  };

  if (0 <= val && val < ARRAY_SIZE(strings))
    return strings[val];
  return "Unknown";
}

#define H264_MAX_ALLOWED_MOD_OF_PIC_NUMS_IDC  32

typedef enum {
  H264_MOD_OF_PIC_IDC_SUBSTRACT_ABS_DIFF = 0,
  H264_MOD_OF_PIC_IDC_ADD_ABS_DIFF       = 1,
  H264_MOD_OF_PIC_IDC_LONG_TERM          = 2,
  H264_MOD_OF_PIC_IDC_END_LOOP           = 3
} H264ModificationOfPicNumsIdcValue;

static inline const char * H264ModificationOfPicNumsIdcValueStr(
  H264ModificationOfPicNumsIdcValue val
)
{
  static const char * strings[] = {
    "Difference to subtract from picture number prediction value",
    "Difference to add from picture number prediction value",
    "Long-term picture used as reference picture",
    "End of loop indicator"
  };

  if (0 <= val && val < ARRAY_SIZE(strings))
    return strings[val];
  return "Unknown";
}

typedef struct {
  H264ModificationOfPicNumsIdcValue modification_of_pic_nums_idc;
  uint32_t abs_diff_pic_num_minus1;
  uint32_t long_term_pic_num;
} H264RefPicListModificationPictureIndex;

typedef struct {
  bool ref_pic_list_modification_flag_l0;
  H264RefPicListModificationPictureIndex refPicListModificationl0[
    H264_MAX_ALLOWED_MOD_OF_PIC_NUMS_IDC
  ];
  unsigned nbRefPicListModificationl0;

  bool refPicListModificationFlagl1;
  H264RefPicListModificationPictureIndex refPicListModificationl1[
    H264_MAX_ALLOWED_MOD_OF_PIC_NUMS_IDC
  ];
  unsigned nbRefPicListModificationl1;
} H264RefPicListModification;

#define H264_REF_PICT_LIST_MAX_NB 32
#define H264_REF_PICT_LIST_MAX_NB_FIELD 16

typedef struct {
  unsigned luma_log2_weight_denom;
  unsigned chroma_log2_weight_denom;

  struct {
    bool luma_weight_l0_flag;
    int luma_weight_l0;
    int luma_offset_l0;

    bool chroma_weight_l0_flag;
    int chroma_weight_l0[H264_MAX_CHROMA_CHANNELS_NB];
    int chroma_offset_l0[H264_MAX_CHROMA_CHANNELS_NB];
  } weightL0[H264_REF_PICT_LIST_MAX_NB];

  struct {
    bool luma_weight_l1_flag;
    int luma_weight_l1;
    int luma_offset_l1;

    bool chroma_weight_l1_flag;
    int chroma_weight_l1[H264_MAX_CHROMA_CHANNELS_NB];
    int chroma_offset_l1[H264_MAX_CHROMA_CHANNELS_NB];
  } weightL1[H264_REF_PICT_LIST_MAX_NB];
} H264PredWeightTable;

typedef enum {
  H264_MEM_MGMNT_CTRL_OP_END                        = 0,
  H264_MEM_MGMNT_CTRL_OP_SHORT_TERM_UNUSED          = 1,
  H264_MEM_MGMNT_CTRL_OP_LONG_TERM_UNUSED           = 2,
  H264_MEM_MGMNT_CTRL_OP_SHORT_TERM_USED_LONG_TERM  = 3,
  H264_MEM_MGMNT_CTRL_OP_MAX_LONG_TERM              = 4,
  H264_MEM_MGMNT_CTRL_OP_ALL_UNUSED                 = 5,
  H264_MEM_MGMNT_CTRL_OP_USED_LONG_TERM             = 6
} H264MemoryManagementControlOperationValue;

static inline const char * H264MemoryManagementControlOperationValueStr(
  H264MemoryManagementControlOperationValue val
)
{
  static const char * strings[] = {
    "End of instructions marker",
    "Mark a short-term reference picture as 'unused for reference'",
    "Mark a long-term reference picture as 'unused for reference'",
    "Mark a short-term reference picture as 'used for long-term reference'",
    "Set max long-term frame index",
    "Mark all reference pictures as 'unused for reference'",
    "Mark current picture as 'used for long-term reference'"
  };

  if (val < ARRAY_SIZE(strings))
    return strings[val];
  return "Unknown";
}

typedef struct H264MemoryManagementControlOperationsBlock {
  // struct H264MemoryManagementControlOperationsBlock * nextOperation;

  H264MemoryManagementControlOperationValue operation;
  unsigned difference_of_pic_nums_minus1;
  unsigned long_term_pic_num;
  unsigned long_term_frame_idx;
  unsigned max_long_term_frame_idx_plus1;
} H264MemMngmntCtrlOpBlk, *H264MemMngmntCtrlOpBlkPtr;

#if 0
H264MemMngmntCtrlOpBlkPtr createH264MemoryManagementControlOperations(
  void
);
void closeH264MemoryManagementControlOperations(
  H264MemMngmntCtrlOpBlkPtr op
);
H264MemMngmntCtrlOpBlkPtr copyH264MemoryManagementControlOperations(
  H264MemMngmntCtrlOpBlkPtr op
);
#endif

#define H264_MAX_SUPPORTED_MEM_MGMNT_CTRL_OPS  16

typedef struct {
  bool IdrPicFlag; /* For quick access */

  union {
    /* if IdrPicFlag */
    struct {
      bool no_output_of_prior_pics_flag;
      bool long_term_reference_flag;
    };

    /* else */
    struct {
#if 0
      H264MemMngmntCtrlOpBlkPtr MemMngmntCtrlOp; /**< TODO: Clean memory management */
#endif
      H264MemMngmntCtrlOpBlk MemMngmntCtrlOp[H264_MAX_SUPPORTED_MEM_MGMNT_CTRL_OPS];
      unsigned nbMemMngmntCtrlOp;

      bool adaptive_ref_pic_marking_mode_flag;
    };
  };

  bool presenceOfMemManCtrlOp4;
  bool presenceOfMemManCtrlOp5;
  bool presenceOfMemManCtrlOp6;
} H264DecRefPicMarking;

typedef enum {
  H264_DEBLOCKING_FILTER_ENABLED               = 0,
  H264_DEBLOCKING_FILTER_DISABLED              = 1,
  H264_DEBLOCKING_FILTER_DISABLED_INTER_SLICES = 2
} H264DeblockingFilterIdc; /* disable_deblocking_filter_idc  */

static inline const char * H264DeblockingFilterIdcStr(
  H264DeblockingFilterIdc val
)
{
  static const char * strings[] = {
    "Enable",
    "Disable",
    "Disabled at slice boundaries"
  };

  if (0 <= val && val < ARRAY_SIZE(strings))
    return strings[val];
  return "Unknown";
}

typedef struct {
  uint32_t first_mb_in_slice;

  H264SliceTypeValue slice_type;
  uint8_t pic_parameter_set_id;
  H264ColourPlaneIdValue colour_plane_id;
  uint16_t frame_num;

  bool field_pic_flag;
  bool bottom_field_flag;  /**< bottom_field_flag field value. If true,
    specifies that the slice is part of a coded bottom field picture.
    Otherwise, if false, specifies that the slice is part of a coded
    top field picture or a frame picture. If this field is not present,
    its value is false.                                                      */

  uint16_t idr_pic_id;
  int32_t pic_order_cnt_lsb;  /**< pic_order_cnt_lsb field value. This value
    is at most an unsigned integer of 16 bits. So signed integer of 32 bits
    is large enought and avoid unsigned/signed conversions in computation of
    PicOrderCnt.                                                             */
  int delta_pic_order_cnt_bottom;  /**< delta_pic_order_cnt field value.
    This value specifies the order of bottom and top fields as a delta of
    picture order count between both fields. Equal to  */

  int delta_pic_order_cnt[2];

  uint8_t redundant_pic_cnt;

  /* if (slice_type == B) */
  bool direct_spatial_mv_pred_flag;

  /* if (slice_type == P || SP || B) */
  bool num_ref_idx_active_override_flag;
  unsigned num_ref_idx_l0_active_minus1;
  unsigned num_ref_idx_l1_active_minus1;

  H264RefPicListModification refPicListMod;

  H264PredWeightTable pred_weight_table;

  bool decRefPicMarkingPresent;
  H264DecRefPicMarking dec_ref_pic_marking;

  uint8_t cabac_init_idc;
  int slice_qp_delta;

  bool sp_for_switch_flag;
  int slice_qs_delta;

  H264DeblockingFilterIdc disable_deblocking_filter_idc;
  int slice_alpha_c0_offset_div2;
  int slice_beta_offset_div2;

  uint32_t slice_group_change_cycle;

  /* Computed parameters: */
  bool isRefPic; /**< Obtained using #isReferencePictureH264NalRefIdcValue()
    with slice header NALU nal_ref_idc value.                                */
  bool isIdrPic; /**< Obtained using #isIdrPictureH264NalUnitTypeValue()
    with slice header NALU nal_unit_type value. Equal to IdrPicFlag.         */
  bool isCodedSliceExtension; /**< Obtained using
    #isCodedSliceExtensionH264NalUnitTypeValue() with slice header NALU
    nal_unit_type value.                                                     */
  unsigned sps_pic_order_cnt_type;  /**< Copy of the active SPS
    pic_order_cnt_type field.                                                */
  bool presenceOfMemManCtrlOp5;  /**< Presence of
    memory_management_control_operation equal to 5 in dec_ref_pic_marking.
    If the section dec_ref_pic_marking is not present, this value is false.  */

  // bool refPic;
  // bool IdrPicFlag;
  // bool isSliceExt;

  // unsigned pic_order_cnt_type; /* Copy from active SPS */

  bool MbaffFrameFlag;
  unsigned PicHeightInMbs;
  unsigned PicHeightInSamplesL;
  unsigned PicHeightInSamplesC;

  unsigned PicSizeInMbs;
} H264SliceHeaderParameters;

typedef struct {
  H264SliceHeaderParameters header;
  /* H264SliceDataParameters sliceData; */
} H264SliceLayerWithoutPartitioningParameters;

typedef union {
  H264SPSDataParameters sps;
} H264AUNalUnitReplacementData;

static inline H264AUNalUnitReplacementData ofSpsH264AUNalUnitReplacementData(
  H264SPSDataParameters sps
)
{
  return (H264AUNalUnitReplacementData) {.sps = sps};
}

typedef struct {
  H264NalUnitTypeValue nal_unit_type;

  int64_t startOffset;
  int64_t length;

  H264AUNalUnitReplacementData replacementParameters;
  bool replace;
} H264AUNalUnit, *H264AUNalUnitPtr;

typedef struct {
  BitstreamReaderPtr inputFile;

  bool packetInitialized;
  uint16_t currentRbspCellBits;
  size_t remainingRbspCellBits;

  H264NalRefIdcValue nal_ref_idc;
  H264NalUnitTypeValue nal_unit_type;
} H264NalDeserializerContext;

int initH264NalDeserializerContext(
  H264NalDeserializerContext * dst,
  const lbc * filepath
);

static inline void cleanH264NalDeserializerContext(
  H264NalDeserializerContext ctx
)
{
  closeBitstreamReader(ctx.inputFile);
}

typedef enum {
  H264_UNRESTRICTED_CHROMA_FORMAT_IDC = 0,

  H264_MONOCHROME_CHROMA_FORMAT_IDC   = 1 << H264_CHROMA_FORMAT_400,
  H264_420_CHROMA_FORMAT_IDC          = 1 << H264_CHROMA_FORMAT_420,
  H264_422_CHROMA_FORMAT_IDC          = 1 << H264_CHROMA_FORMAT_422,
  H264_444_CHROMA_FORMAT_IDC          = 1 << H264_CHROMA_FORMAT_444,

  H264_MONO_420_CHROMA_FORMAT_IDC     =
    H264_MONOCHROME_CHROMA_FORMAT_IDC |
    H264_420_CHROMA_FORMAT_IDC,

  H264_MONO_TO_422_CHROMA_FORMAT_IDC  =
    H264_MONOCHROME_CHROMA_FORMAT_IDC |
    H264_420_CHROMA_FORMAT_IDC        |
    H264_422_CHROMA_FORMAT_IDC
} H264ChromaFormatIdcRestriction;

typedef enum {
  H264_ENTROPY_CODING_MODE_CAVLC_ONLY   = 0x1,
  H264_ENTROPY_CODING_MODE_CABAC_ONLY   = 0x2,
  H264_ENTROPY_CODING_MODE_UNRESTRICTED = 0x3
} H264EntropyCodingModeRestriction;

/** \~english
 * \brief Check if given 'entropy_coding_mode_flag' comply with entropy coding
 * mode restr.
 *
 * \param restr H264EntropyCodingModeRestriction Restriction condition.
 * \param entropy_coding_mode_flag bool PPS 'entropy_coding_mode_flag' flag.
 * \return true PPS entropy coding mode respect restr condition.
 * \return false PPS entropy coding mode violate restr condition.
 */
static inline bool isRespectedH264EntropyCodingModeRestriction(
  H264EntropyCodingModeRestriction restr,
  bool entropy_coding_mode_flag
)
{
  return (restr & (1 << entropy_coding_mode_flag));
}

typedef struct {
  /* Table A-1 - Level limits */
  unsigned MaxMBPS;
  unsigned MaxFS;
  unsigned MaxDpbMbs;
  unsigned MaxBR;
  unsigned MaxCPB;
  unsigned MaxVmvR;
  unsigned MinCR;
  unsigned MaxMvsPer2Mb;

  /* Table A-2 - High profiles related factors */
  unsigned cpbBrVclFactor;
  unsigned cpbBrNalFactor;

  /* Tables A-3 and A-4 - Profiles level related limits */
  unsigned SliceRate;
  unsigned MinLumaBiPredSize;
  bool direct_8x8_inference_flag;
  bool frame_mbs_only_flag;
  unsigned MaxSubMbRectSize;

  unsigned brNal;  /**< Computed maximum NAL HRD BitRate value.              */

  /* Profile related restricted settings */
  bool idrPicturesOnly; /* Implies max_num_ref_frames, max_num_reorder_frames, max_dec_frame_buffering and dpb_output_delay == 0 */
  H264PrimaryPictureType allowedSliceTypes;
  bool forbiddenSliceDataPartitionLayersNal;
  bool forbiddenArbitrarySliceOrder;
  H264ChromaFormatIdcRestriction restrictedChromaFormatIdc;
  unsigned maxAllowedBitDepthMinus8;
  bool forcedDirect8x8InferenceFlag; /* forcedDirect8x8InferenceFlag || direct_8x8_inference_flag */
  bool restrictedFrameMbsOnlyFlag; /* restrictedFrameMbsOnlyFlag || frame_mbs_only_flag */
  bool forbiddenWeightedPredModesUse;
  bool forbiddenQpprimeYZeroTransformBypass;
  H264EntropyCodingModeRestriction restrEntropyCodingMode;
  unsigned maxAllowedNumSliceGroups;
  bool forbiddenPPSExtensionParameters;
  bool forbiddenRedundantPictures;
  unsigned maxAllowedLevelPrefix; /* TODO */
  bool restrictedPcmSampleValues; /* TODO */

  /* VUI bitstream restrictions */
  bool vuiRestrictionsPresent;
  bool constrainedMotionVectors;
  size_t maxBitsPerPic; /* Warning: In bits. */
  size_t maxBitsPerMb;

  unsigned sliceNb;

  unsigned gopMaxLength;
  unsigned consecutiveBPicNb;
} H264ConstraintsParam;

unsigned getH264BrNal(
  H264ConstraintsParam constraints,
  H264ProfileIdcValue profile_idc
);

/** \~english
 * \brief Last picture properties.
 *
 */
typedef struct {
  bool presenceOfMemManCtrlOp5;  /**< Presence of a
    memory_management_control_operation equal to 5 in slice header.          */
  bool isBottomField;            /**< Is a bottom field.                     */

  uint32_t first_mb_in_slice;    /**< Slice header first_mb_in_slice field.  */

  int32_t PicOrderCnt;           /**< Computed PicOrderCnt value.            */

  struct {
    int32_t TopFieldOrderCnt;   /**< Computed TopFieldOrderCnt value.        */
    int32_t PicOrderCntMsb;     /**< Computed PicOrderCntMsb value.          */
    int32_t pic_order_cnt_lsb;  /**< Slice header pic_order_cnt_lsb field
      value.                                                                 */
  }; /**< _computePicOrderCntType* functions dependant.                      */

  int64_t dts;  /**< Decoding Time Stamp value in #MAIN_CLOCK_27MHZ ticks. Used with duration value. */
  int64_t duration;  /**< Picture duration in #MAIN_CLOCK_27MHZ ticks. Next picture DTS value is set equal to last picture DTS plus this duration value. This computation manner is inspired from tsMuxer. */
} H264LastPictureProperties;

#define H264_PICTURES_BEFORE_RESTART  20
#define H264_AU_DEFAULT_NB_NALUS  8

typedef struct {
  bool initializedParam;  /**< Are bit-stream parameters initialized. */
  bool restartRequired; /**< Some parsing parameters must be modified,
    requiring restart of parsing. A threshold fixed by
    #H264_PICTURES_BEFORE_RESTART defines the number of parsed pictures to
    reach before cancelling parsing. This reduce the number of restarts when
    adjusting DTS-PTS initial delay according to pictures references
    structure.                                                               */

  struct {
    bool bottomFieldPresent;
    bool topFieldPresent;
    bool framePresent;
  } auContent;  /**< Current Access content. Used to merge complementary field pair pictures. */

  unsigned nbPics;  /**< Current number of parsed complete pictures. */
  unsigned nbConsecutiveBPics;  /**< Current number of consecutive B-pictures. */

  unsigned nbSlicesInPic;  /**< Current number of slices in current parsed picture. */

  double frameRate;  /**< Video frame-rate. */
  int64_t frameDuration;  /**< Duration of one video frame in #MAIN_CLOCK_27MHZ ticks. */
  int64_t fieldDuration;  /**< Duration of one video field in #MAIN_CLOCK_27MHZ ticks. */

  bool reachVclNaluOfaPrimCodedPic;  /**< Is the first VCL NALU of the current Access Unit reached. Used to determine start of a new access unit and completion of the previous one. */

  int64_t decPicNbCnt;  /**< "Decoding PicOrderCnt", picture number in decoding order modulo active SPS MaxPicOrderCntLsb value. */

  bool halfPicOrderCnt;  /**< Divide by two picture PicOrderCnt values. */
  int64_t initDecPicNbCntShift;  /**< Initial picture decoding delay in pictures units. */

  int64_t PicOrderCnt; /**< PicOrderCnt of current picture. */

  int64_t LastMaxStreamPicOrderCnt;
  int64_t MaxStreamPicOrderCnt;

  H264NalUnitTypeValue lstNaluType;  /**< Last parsed NALU nal_unit_type. */
  H264LastPictureProperties lstPic;  /**< Last picture values.           */

  /* Frames sizes parsing : */
  size_t largestAUSize;  /**< Largest parsed Access Unit in bytes. */
  size_t largestIPicAUSize;  /**< Largest parsed Access Unit containing an I picture in bytes. */

  /* Buffering informations : */
  uint64_t auCpbRemovalTime; /* Current AU CPB removal time in #MAIN_CLOCK_27MHZ ticks */
  uint64_t auDpbOutputTime; /* Current AU DPB output time in #MAIN_CLOCK_27MHZ ticks */

  /* Specific informations spotted in stream : */
  bool gapsInFrameNum; /* Presence of gaps in frame numbers */
  bool missingVSTVuiParameters;  /**< Missing of Video Signal Type from SPS VUI parameters. */
  bool wrongVuiParameters;
  bool spsPresentInSubAU; /* Presence of SPS in subsequent access units in GOP */
  bool usageOfLowerLevel;
  bool presenceOfUselessAccessUnitDelimiter;
  bool presenceOfUselessSequenceParameterSet;

  bool picOrderCntType2use; /**< TODO: Forbiddens presence of two consecutive non-referential frames/non-complementary fields. */

  /* Triggered options : */
  bool useVuiRebuilding;
  bool useVuiUpdate;
  bool stillPictureTolerance;
  bool discardUselessAccessUnitDelimiter;
  bool discardUselessSequenceParameterSet;

  struct {
    H264AUNalUnit * nalus;
    unsigned nbAllocatedNalus;
    unsigned nbUsedNalus;
    bool inProcessNalu;

    int64_t size; // In bytes.
  } curAccessUnit;
} H264CurrentProgressParam;

typedef struct {
#if 0
  void * linkedParam; /* Type defined by list name */

  size_t length;
  unsigned dataSectionIdx;
#else
  H264AUNalUnitReplacementData linkedParam;
  unsigned dataSectionIdx;
  size_t dataSectionSize;
#endif
} H264ModifiedNalUnit;

static inline H264SPSDataParameters spsH264ModifiedNalUnit(
  H264ModifiedNalUnit unit
)
{
  return unit.linkedParam.sps;
}

typedef struct {
  H264ModifiedNalUnit * sequenceParametersSets;
  unsigned nbSequenceParametersSet;

  bool patchBufferingPeriodSeiPresent;
  H264ModifiedNalUnit bufferingPeriodSeiMsg;
} H264ModifiedNalUnitsList;

/* Spec data functions : */
unsigned getH264MaxMBPS(
  uint8_t level_idc
);
unsigned getH264MaxFS(
  uint8_t level_idc
);
unsigned getH264MaxDpbMbs(
  uint8_t level_idc
);
unsigned getH264MaxBR(
  uint8_t level_idc
);
unsigned getH264MaxCPB(
  uint8_t level_idc
);
unsigned getH264MaxVmvR(
  uint8_t level_idc
);
unsigned getH264MinCR(
  uint8_t level_idc
);
unsigned getH264MaxMvsPer2Mb(
  uint8_t level_idc
);

unsigned getH264SliceRate(
  uint8_t level_idc
);
unsigned getH264MinLumaBiPredSize(
  uint8_t level_idc
);
bool getH264direct_8x8_inference_flag(
  uint8_t level_idc
);
bool getH264frame_mbs_only_flag(
  uint8_t level_idc
);
unsigned getH264MaxSubMbRectSize(
  uint8_t level_idc
);

unsigned getH264cpbBrVclFactor(
  H264ProfileIdcValue profile_idc
);
unsigned getH264cpbBrNalFactor(
  H264ProfileIdcValue profile_idc
);

/* ### Blu-ray specifications : ############################################ */

/** \~english
 * \brief Blu-ray specifications expected aspect_ratio_idc values.
 *
 * Used to set up to two allowed aspect_ratio_idc values (both fields might
 * be set to the same value to allow only one value).
 */
typedef struct {
  H264AspectRatioIdcValue a;  /**< First valid aspect_ratio_idc value.       */
  H264AspectRatioIdcValue b;  /**< Second valid aspect_ratio_idc value.      */
} H264BdavExpectedAspectRatioRet;

static inline H264BdavExpectedAspectRatioRet getH264BdavExpectedAspectRatioIdc(
  unsigned frameWidth,
  unsigned frameHeight
)
{
  switch (frameWidth) {
  case 1920:
  case 1280:
    return (H264BdavExpectedAspectRatioRet) {
      H264_ASPECT_RATIO_IDC_1_BY_1,
      H264_ASPECT_RATIO_IDC_1_BY_1
    };

  case 1440:
    return (H264BdavExpectedAspectRatioRet) {
      H264_ASPECT_RATIO_IDC_4_BY_3,
      H264_ASPECT_RATIO_IDC_4_BY_3
    };

  case 720:
    if (frameHeight == 576) {
      return (H264BdavExpectedAspectRatioRet) {
        H264_ASPECT_RATIO_IDC_12_BY_11,
        H264_ASPECT_RATIO_IDC_16_BY_11
      };
    }
    return (H264BdavExpectedAspectRatioRet) {
      H264_ASPECT_RATIO_IDC_10_BY_11,
      H264_ASPECT_RATIO_IDC_40_BY_33
    };
  }

  return (H264BdavExpectedAspectRatioRet) {
    H264_ASPECT_RATIO_IDC_12_BY_11,
    H264_ASPECT_RATIO_IDC_16_BY_11
  };
}

static inline bool isRespectedH264BdavExpectedAspectRatio(
  H264BdavExpectedAspectRatioRet restr,
  H264AspectRatioIdcValue aspect_ratio_idc
)
{
  return restr.a == aspect_ratio_idc || restr.b == aspect_ratio_idc;
}

static inline H264VideoFormatValue getH264BdavExpectedVideoFormat(
  double frameRate
)
{
  if (FLOAT_COMPARE(frameRate, 25.0) || FLOAT_COMPARE(frameRate, 50.0))
    return H264_VIDEO_FORMAT_PAL;
  return H264_VIDEO_FORMAT_NTSC;
}

static inline H264ColourPrimariesValue getH264BdavExpectedColorPrimaries(
  unsigned frameHeight
)
{
  switch (frameHeight) {
  case 576: return H264_COLOR_PRIM_BT470BG;
  case 480: return H264_COLOR_PRIM_SMPTE170M;
  }
  return H264_COLOR_PRIM_BT709;
}

static inline H264TransferCharacteristicsValue
getH264BdavExpectedTransferCharacteritics(
  unsigned frameHeight
)
{
  switch (frameHeight) {
  case 576: return H264_TRANS_CHAR_BT470BG;
  case 480: return H264_TRANS_CHAR_SMPTE170M;
  }
  return H264_TRANS_CHAR_BT709;
}

static inline H264MatrixCoefficientsValue
getH264BdavExpectedMatrixCoefficients(
  unsigned frameHeight
)
{
  switch (frameHeight) {
  case 576: return H264_MATRX_COEF_BT470M;
  case 480: return H264_MATRX_COEF_SMPTE170M;
  }
  return H264_MATRX_COEF_BT709;
}

/** \~english
 * \brief Max allowed number of CPB configurations.
 *
 * This value may be fixed to a value equal or greater than 32 to disable
 * limitation (32 is the maximum value allowed by H.264 specifications).
 */
#define H264_BDAV_ALLOWED_CPB_CNT 1

#endif