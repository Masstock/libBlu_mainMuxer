/** \~english
 * \file h264_data.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief H.264 video (AVC) bitstreams data structures module.
 */

#ifndef __LIBBLU_MUXER_CODECS_H264_DATA_H__
#define __LIBBLU_MUXER_CODECS_H264_DATA_H__

#include "../../elementaryStreamProperties.h"

#define H264_AR_CHANGE_EXPECTED_SEPARATOR                   ':'
#define H264_LEVEL_CHANGE_EXPECTED_SEPARATOR                '.'
#define H264_NAL_BYTE_ARRAY_SIZE_MULTIPLIER                1024

/* Switches : */
#define DISABLE_NAL_REPLACEMENT_DATA_OPTIMIZATION             0
#define ALLOW_BUFFERING_PERIOD_CHANGE                         1

/* nal_ref_idc codes : */
#define NAL_REF_IDC_NONE                                    0x0
#define NAL_REF_IDC_LOW                                     0x1
#define NAL_REF_IDC_MEDIUM                                  0x2
#define NAL_REF_IDC_HIGH                                    0x3

#define NAL_SEQUENCE_PARAMETERS_SET_REF_IDC    NAL_REF_IDC_HIGH
#define NAL_SEI_REF_IDC                         NAL_REF_IDC_LOW

/* nal_unit_type codes : */
typedef enum {
  NAL_UNIT_TYPE_UNSPECIFIED                           =  0,
  NAL_UNIT_TYPE_NON_IDR_SLICE                         =  1,
  NAL_UNIT_TYPE_SLICE_PART_A_LAYER                    =  2,
  NAL_UNIT_TYPE_SLICE_PART_B_LAYER                    =  3,
  NAL_UNIT_TYPE_SLICE_PART_C_LAYER                    =  4,
  NAL_UNIT_TYPE_IDR_SLICE                             =  5,
  NAL_UNIT_TYPE_SUPPLEMENTAL_ENHANCEMENT_INFORMATION  =  6,
  NAL_UNIT_TYPE_SEQUENCE_PARAMETERS_SET               =  7,
  NAL_UNIT_TYPE_PIC_PARAMETERS_SET                    =  8,
  NAL_UNIT_TYPE_ACCESS_UNIT_DELIMITER                 =  9,
  NAL_UNIT_TYPE_END_OF_SEQUENCE                       = 10,
  NAL_UNIT_TYPE_END_OF_STREAM                         = 11,
  NAL_UNIT_TYPE_FILLER_DATA                           = 12,
  NAL_UNIT_TYPE_PREFIX_NAL_UNIT                       = 14,
  NAL_UNIT_TYPE_CODED_SLICE_EXT                       = 20,
  NAL_UNIT_TYPE_CODED_SLICE_DEPTH_EXT                 = 21
} H264NalUnitType;

#define H264_MAX_CHROMA_CHANNELS_NB                           2
#define H264_MAX_CPB_CONFIGURATIONS                          32


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

typedef struct {
  uint8_t nalRefIdc;
  uint8_t nalUnitType;

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

typedef enum {
  H264_PRIM_PIC_TYPE_MASK_I       = 0x84,
  H264_PRIM_PIC_TYPE_MASK_IP      = 0xA5,
  H264_PRIM_PIC_TYPE_MASK_IPB     = 0xE7,
  H264_PRIM_PIC_TYPE_MASK_SI      = 0x10,
  H264_PRIM_PIC_TYPE_MASK_SISP    = 0x18,
  H264_PRIM_PIC_TYPE_MASK_ISI     = 0x94,
  H264_PRIM_PIC_TYPE_MASK_IPSISP  = 0xBD,
  H264_PRIM_PIC_TYPE_MASK_ALL     = 0xFF
} H264PrimaryPictureTypeMask;

const char * h264PrimaryPictureTypeStr(
  const H264PrimaryPictureType val
);

#define H264_CHECK_PRIMARY_PICTURE_TYPE(ret, restr, typ)                      \
  {                                                                           \
    static const unsigned primPicTypMasks[] = {                               \
      H264_PRIM_PIC_TYPE_MASK_I,                                              \
      H264_PRIM_PIC_TYPE_MASK_IP,                                             \
      H264_PRIM_PIC_TYPE_MASK_IPB,                                            \
      H264_PRIM_PIC_TYPE_MASK_SI,                                             \
      H264_PRIM_PIC_TYPE_MASK_SISP,                                           \
      H264_PRIM_PIC_TYPE_MASK_ISI,                                            \
      H264_PRIM_PIC_TYPE_MASK_IPSISP,                                         \
      H264_PRIM_PIC_TYPE_MASK_ALL                                             \
    };                                                                        \
    assert((restr) < ARRAY_SIZE(primPicTypMasks));                            \
    (ret) = primPicTypMasks[(restr)] & (typ);                                 \
  }

typedef struct {
  H264PrimaryPictureType primaryPictType;
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

#define H264_PROFILE_IS_BASELINE_MAIN_EXTENDED(profileIdc)                    \
  (                                                                           \
    (profileIdc) == H264_PROFILE_BASELINE                                     \
    || (profileIdc) == H264_PROFILE_MAIN                                      \
    || (profileIdc) == H264_PROFILE_EXTENDED                                  \
  )

#define H264_PROFILE_IS_HIGH(profileIdc)                                      \
  (                                                                           \
    (profileIdc) == H264_PROFILE_CAVLC_444_INTRA                              \
    || (profileIdc) == H264_PROFILE_SCALABLE_HIGH                             \
    || (profileIdc) == H264_PROFILE_HIGH                                      \
    || (profileIdc) == H264_PROFILE_MULTIVIEW_HIGH                            \
    || (profileIdc) == H264_PROFILE_HIGH_10                                   \
    || (profileIdc) == H264_PROFILE_HIGH_422                                  \
    || (profileIdc) == H264_PROFILE_STEREO_HIGH                               \
    || (profileIdc) == H264_PROFILE_MFC_HIGH                                  \
    || (profileIdc) == H264_PROFILE_DEPTH_MFC_HIGH                            \
    || (profileIdc) == H264_PROFILE_MULTIVIEW_DEPTH_HIGH                      \
    || (profileIdc) == H264_PROFILE_ENHANCED_MULTIVIEW_DEPTH_HIGH             \
    || (profileIdc) == H264_PROFILE_HIGH_444_PREDICTIVE                       \
  )

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

const char * h264ProfileIdcValueStr (
  const H264ProfileIdcValue val,
  const H264ContraintFlags constraints
);

/* Chroma format codes                                       */
/* chroma_format_idc values :                                */
typedef enum {
  H264_CHROMA_FORMAT_400 = 0,
  H264_CHROMA_FORMAT_420 = 1,
  H264_CHROMA_FORMAT_422 = 2,
  H264_CHROMA_FORMAT_444 = 3
} H264ChromaFormatIdcValue;

const char * h264ChromaFormatIdcValueStr(
  const H264ChromaFormatIdcValue val
);

typedef struct {
  bool seqScalingListPresent[12];
  uint8_t scalingList4x4[6][16];
  uint8_t scalingList8x8[6][64];
  bool useDefaultScalingMatrix4x4[6];
  bool useDefaultScalingMatrix8x8[6];
} H264SeqScalingMatrix;

typedef struct {
  uint64_t bitRate;
  uint64_t cpbSize;
  bool cbrFlag;

  uint32_t bitRateValueMinus1;
  uint32_t cpbSizeValueMinus1;
} H264SchedSel;

typedef struct {
  unsigned cpbCntMinus1;
  uint8_t bitRateScale;
  uint8_t cpbSizeScale;

  H264SchedSel schedSel[
    H264_MAX_CPB_CONFIGURATIONS
  ];

  uint8_t initialCpbRemovalDelayLengthMinus1;
  uint8_t cpbRemovalDelayLengthMinus1;
  uint8_t dpbOutputDelayLengthMinus1;
  uint8_t timeOffsetLength;
} H264HrdParameters;

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

const char * h264AspectRatioIdcValueStr(
  const H264AspectRatioIdcValue val
);

typedef enum {
  H264_VIDEO_FORMAT_COMPONENT    = 0,
  H264_VIDEO_FORMAT_PAL          = 1,
  H264_VIDEO_FORMAT_NTSC         = 2,
  H264_VIDEO_FORMAT_SECAM        = 3,
  H264_VIDEO_FORMAT_MAC          = 4,
  H264_VIDEO_FORMAT_UNSPECIFIED  = 5,
} H264VideoFormatValue;

const char * h264VideoFormatValueStr(
  const H264VideoFormatValue val
);

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

const char * h264ColorPrimariesValueStr(
  const H264ColourPrimariesValue val
);

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

const char * h264TransferCharacteristicsValueStr(
  const H264TransferCharacteristicsValue val
);

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

const char * h264MatrixCoefficientsValueStr(
  const H264MatrixCoefficientsValue val
);

typedef struct {
  H264ColourPrimariesValue colourPrimaries;
  H264TransferCharacteristicsValue transferCharact;
  H264MatrixCoefficientsValue matrixCoeff;
} H264VuiColourDescriptionParameters;

typedef struct {
  bool motionVectorsOverPicBoundaries;
  uint8_t maxBytesPerPicDenom;
  uint8_t maxBitsPerPicDenom;
  uint8_t log2MaxMvLengthHorizontal;
  uint8_t log2MaxMvLengthVertical;
  uint16_t maxNumReorderFrames;
  uint8_t maxDecFrameBuffering;

  unsigned long maxBytesPerPic;
  unsigned long maxBitsPerMb;
} H264VuiVideoSeqBitstreamRestrictionsParameters;

typedef struct {
  bool aspectRatioInfoPresent;
  H264AspectRatioIdcValue aspectRatioIdc;
  struct {
    unsigned width;
    unsigned height;
  } extendedSAR;

  bool overscanInfoPresent;
  bool overscanAppropriate;

  bool videoSignalTypePresent;
  struct {
    H264VideoFormatValue videoFormat;
    bool videoFullRange;
    bool colourDescPresent;
    H264VuiColourDescriptionParameters colourDescription;
  } videoSignalType;

  bool chromaLocInfoPresent;
  struct {
    unsigned sampleLocTypeTopField;
    unsigned sampleLocTypeBottomField;
  } ChromaLocInfo;

  bool timingInfoPresent;
  struct {
    uint32_t numUnitsInTick;
    uint32_t timeScale;
    uint32_t clockTick;

    bool fixedFrameRateFlag;

    double frameRate;
    HdmvFrameRateCode frameRateCode;
    unsigned maxFPS;
  } timingInfo;

  bool nalHrdParamPresent;
  H264HrdParameters nalHrdParam;
  bool vclHrdParamPresent;
  H264HrdParameters vclHrdParam;
  bool cpbDpbDelaysPresent;

  bool lowDelayHrd;

  bool picStructPresent;
  bool bitstreamRestrictionsPresent;
  H264VuiVideoSeqBitstreamRestrictionsParameters bistreamRestrictions;
} H264VuiParameters;

typedef struct {
  H264ProfileIdcValue profileIdc;
  H264ContraintFlags constraintFlags;
  uint8_t levelIdc;

  unsigned seqParametersSetId;

  H264ChromaFormatIdcValue chromaFormat;
  bool separateColourPlaneFlag444;

  uint8_t bitDepthLumaMinus8;
  uint8_t bitDepthChromaMinus8;
  bool qpprimeYZeroTransformBypass;

  bool seqScalingMatrixPresent;
  H264SeqScalingMatrix seqScalingMatrix;

  uint8_t log2MaxFrameNumMinus4;
  unsigned MaxFrameNum;

  uint8_t picOrderCntType;

  uint8_t log2MaxPicOrderCntLsbMinus4;

  bool deltaPicOrderAlwaysZero;
  int offsetForNonRefPic;
  int offsetForTopToBottomField;
  unsigned numRefFramesInPicOrderCntCycle;
  int offsetForRefFrame[255];

  unsigned maxNumRefFrames;
  bool gapsInFrameNumValueAllowed;

  unsigned picWidthInMbsMinus1;
  unsigned picHeightInMapUnitsMinus1;
  bool frameMbsOnly;

  bool mbAdaptativeFrameField;
  bool direct8x8Interference;

  bool frameCropping;
  struct {
    unsigned left;
    unsigned right;
    unsigned top;
    unsigned bottom;
  } frameCropOffset;

  bool vuiParametersPresent;
  H264VuiParameters vuiParameters;

  /* Computed parameters: */
  unsigned BitDepthLuma;
  unsigned QpBdOffsetLuma;
  unsigned BitDepthChroma;
  unsigned QpBdOffsetChroma;

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
  unsigned FrameHeightInMbs;

  unsigned ChromaArrayType;
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

typedef enum {
  H264_WEIGHTED_PRED_B_SLICES_DEFAULT  = 0,
  H264_WEIGHTED_PRED_B_SLICES_EXPLICIT = 1,
  H264_WEIGHTED_PRED_B_SLICES_IMPLICIT = 2,
  H264_WEIGHTED_PRED_B_SLICES_RESERVED = 3
} H264WeightedBipredIdc;

const char * h264WeightedBipredIdcStr(
  const H264WeightedBipredIdc val
);

typedef struct {
  H264SliceGroupMapTypeValue mapType;

  union {
    /* H264_SLICE_GROUP_TYPE_INTERLEAVED : */
    unsigned runLengthMinus1[H264_MAX_ALLOWED_SLICE_GROUPS];

    /* H264_SLICE_GROUP_TYPE_FOREGROUND_LEFTOVER : */
    struct {
      unsigned topLeft[H264_MAX_ALLOWED_SLICE_GROUPS];
      unsigned bottomRight[H264_MAX_ALLOWED_SLICE_GROUPS];
    };

    /* H264_SLICE_GROUP_TYPE_CHANGING_123 : */
    struct {
      bool changeDirection;
      unsigned changeRateMinus1;
    };

    /* H264_SLICE_GROUP_TYPE_EXPLICIT : */
    unsigned picSizeInMapUnitsMinus1;
    /* slice_group_id not parsed. */
  } data;
} H264PicParametersSetSliceGroupsParameters;

typedef struct {
  unsigned picParametersSetId;
  unsigned seqParametersSetId;

  bool entropyCodingMode;
  bool bottomFieldPicOrderInFramePresent;

  unsigned nbSliceGroups;
  H264PicParametersSetSliceGroupsParameters sliceGroups;

  unsigned numRefIdxl0DefaultActiveMinus1;
  unsigned numRefIdxl1DefaultActiveMinus1;

  bool weightedPred;
  H264WeightedBipredIdc weightedBipredIdc;

  int picInitQpMinus26;
  int picInitQsMinus26;
  int chromaQpIndexOff;

  bool deblockingFilterControlPresent;
  bool contraintIntraPred;
  bool redundantPicCntPresent;

  bool picParamExtensionPresent;
  bool transform8x8Mode;

  bool picScalingMatrixPresent;
  unsigned nbScalingMatrix;
  bool picScalingListPresent[12];
  uint8_t scalingList4x4[6][16];
  uint8_t scalingList8x8[6][64];
  bool useDefaultScalingMatrix4x4[6];
  bool useDefaultScalingMatrix8x8[6];

  int secondChromaQpIndexOff;
} H264PicParametersSetParameters;

typedef struct {
  uint32_t initialCpbRemovalDelay;
  uint32_t initialCpbRemovalDelayOff;
} H264HrdBufferingPeriodParameters;

typedef struct {
  unsigned seqParametersSetId;

  H264HrdBufferingPeriodParameters nalHrdParam[H264_MAX_CPB_CONFIGURATIONS];
  H264HrdBufferingPeriodParameters vclHrdParam[H264_MAX_CPB_CONFIGURATIONS];
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

const char * h264PicStructValueStr(
  const H264PicStructValue val
);

#define H264_PIC_STRUCT_MAX_NUM_CLOCK_TS 3

typedef enum {
  H264_CT_TYPE_PROGRESSIVE = 0,
  H264_CT_TYPE_INTERLACED  = 1,
  H264_CT_TYPE_UNKNOWN     = 2,
  H264_CT_TYPE_RESERVED    = 3
} H264CtTypeValue;

const char * h264CtTypeValueStr(
  const H264CtTypeValue val
);

typedef enum {
  H264_COUNTING_T_NO_DROP_UNUSED_TIME_OFF  = 0,
  H264_COUNTING_T_NO_DROP                  = 1,
  H264_COUNTING_T_DROP_ZEROS               = 2,
  H264_COUNTING_T_DROP_MAXFPS              = 3,
  H264_COUNTING_T_DROP_ZEROS_AND_ONES      = 4,
  H264_COUNTING_T_DROP_UNSPECIFIED_INDIV   = 5,
  H264_COUNTING_T_DROP_UNSPECIFIED_NB      = 6
} H264CountingTypeValue;

const char * h264CountingTypeValueStr(
  const H264CountingTypeValue val
);

typedef struct {
  H264CtTypeValue ctType;
  bool nuitFieldBased;
  H264CountingTypeValue countingType;
} H264ClockTimestampParam;

typedef struct {
  /* if CpbDpbDelaysPresentFlag */
  uint32_t cpbRemovalDelay;
  uint32_t dpbOutputDelay;

  /* if pic_struct_present_flag */
  H264PicStructValue picStruct;
  unsigned numClockTS;

  bool clockTimestampPresent[H264_PIC_STRUCT_MAX_NUM_CLOCK_TS];
  H264ClockTimestampParam clockTimestampParam[
    H264_PIC_STRUCT_MAX_NUM_CLOCK_TS
  ];
  struct {
    bool fullTimestamp;
    bool discontinuity;
    bool cntDropped;
    unsigned nFrames;

    bool secondsValuePresent;
    bool minutesValuePresent;
    bool hoursValuePresent;

    unsigned secondsValue;
    unsigned minutesValue;
    unsigned hoursValue;

    uint32_t timeOffset;
  } ClockTimestamp[H264_PIC_STRUCT_MAX_NUM_CLOCK_TS];
} H264SeiPicTiming;

#define H264_MAX_USER_DATA_PAYLOAD_SIZE 1024

typedef struct {
  uint8_t uuidIsoIec11578[16];

  bool userDataPayloadOverflow;
  size_t userDataPayloadLength;
  uint8_t userDataPayload[H264_MAX_USER_DATA_PAYLOAD_SIZE];
} H264SeiUserDataUnregistered;

typedef struct {
  unsigned recoveryFrameCnt;
  bool exactMatch;
  bool brokenLink;
  uint8_t changingSliceGroupIdc;
} H264SeiRecoveryPoint;

typedef enum {
  H264_SEI_TYPE_BUFFERING_PERIOD       = 0,
  H264_SEI_TYPE_PIC_TIMING             = 1,
  H264_SEI_TYPE_USER_DATA_UNREGISTERED = 5,
  H264_SEI_TYPE_RECOVERY_POINT         = 6
} H264SeiPayloadTypeValue;

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
  unsigned messagesNb;
  H264SeiMessageParameters messages[H264_MAX_SUPPORTED_RBSP_SEI_MESSAGES];
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
  H264_SLICE_TYPE_P_UNRESCTRICTED  = 0,
  H264_SLICE_TYPE_B_UNRESCTRICTED  = 1,
  H264_SLICE_TYPE_I_UNRESCTRICTED  = 2,
  H264_SLICE_TYPE_SP_UNRESCTRICTED = 3,
  H264_SLICE_TYPE_SI_UNRESCTRICTED = 4,
  H264_SLICE_TYPE_P                = 5,
  H264_SLICE_TYPE_B                = 6,
  H264_SLICE_TYPE_I                = 7,
  H264_SLICE_TYPE_SP               = 8,
  H264_SLICE_TYPE_SI               = 9
} H264SliceTypeValue;

const char * h264SliceTypeValueStr(
  const H264SliceTypeValue val
);

#define H264_MAX_SLICE_TYPE_VALUE 9

#define H264_SLICE_IS_TYPE_P(slice_type)                                      \
  (                                                                           \
    (1 << (slice_type))                                                       \
    & (                                                                       \
      (1 << H264_SLICE_TYPE_P_UNRESCTRICTED)                                  \
      | (1 << H264_SLICE_TYPE_P)                                              \
    )                                                                         \
  )

#define H264_SLICE_IS_TYPE_B(slice_type)                                      \
  (                                                                           \
    (1 << (slice_type))                                                       \
    & (                                                                       \
      (1 << H264_SLICE_TYPE_B_UNRESCTRICTED)                                  \
      | (1 << H264_SLICE_TYPE_B)                                              \
    )                                                                         \
  )

#define H264_SLICE_IS_TYPE_I(slice_type)                                      \
  (                                                                           \
    (1 << (slice_type))                                                       \
    & (                                                                       \
      (1 << H264_SLICE_TYPE_I_UNRESCTRICTED)                                  \
      | (1 << H264_SLICE_TYPE_I)                                              \
    )                                                                         \
  )

#define H264_SLICE_IS_TYPE_SP(slice_type)                                     \
  (                                                                           \
    (1 << (slice_type))                                                       \
    & (                                                                       \
      (1 << H264_SLICE_TYPE_SP_UNRESCTRICTED)                                 \
      | (1 << H264_SLICE_TYPE_SP)                                             \
    )                                                                         \
  )

#define H264_SLICE_IS_TYPE_SI(slice_type)                                     \
  (                                                                           \
    (1 << (slice_type))                                                       \
    & (                                                                       \
      (1 << H264_SLICE_TYPE_SI_UNRESCTRICTED)                                 \
      | (1 << H264_SLICE_TYPE_SI)                                             \
    )                                                                         \
  )

#define H264_SLICE_TYPE_IS_UNRESTRICTED(slice_type)       \
  ((slice_type) <= 4)

typedef enum {
  H264_COLOUR_PLANE_ID_Y  = 0,
  H264_COLOUR_PLANE_ID_CB = 1,
  H264_COLOUR_PLANE_ID_CR = 2
} H264ColourPlaneIdValue;

const char * h264ColourPlaneIdValueStr(
  const H264ColourPlaneIdValue val
);

#define H264_MAX_ALLOWED_MOD_OF_PIC_NUMS_IDC 32

typedef enum {
  H264_MOD_OF_PIC_IDC_SUBSTRACT_ABS_DIFF = 0,
  H264_MOD_OF_PIC_IDC_ADD_ABS_DIFF       = 1,
  H264_MOD_OF_PIC_IDC_LONG_TERM          = 2,
  H264_MOD_OF_PIC_IDC_END_LOOP           = 3
} H264ModificationOfPicNumsIdcValue;

const char * h264ModificationOfPicNumsIdcValueStr(
  const H264ModificationOfPicNumsIdcValue val
);

typedef struct {
  H264ModificationOfPicNumsIdcValue modOfPicNumsIdc;
  uint32_t absDiffPicNumMinus1;
  uint32_t longTermPicNum;
} H264RefPicListModificationPictureIndex;

typedef struct {
  bool refPicListModificationFlagl0;
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
  unsigned lumaLog2WeightDenom;
  unsigned chromaLog2WeightDenom;

  struct {
    bool lumaWeightFlag;
    int lumaWeight;
    int lumaOffset;

    bool chromaWeightFlag;
    int chromaWeight[H264_MAX_CHROMA_CHANNELS_NB];
    int chromaOffset[H264_MAX_CHROMA_CHANNELS_NB];
  } weightL0[H264_REF_PICT_LIST_MAX_NB];

  struct {
    bool lumaWeightFlag;
    int lumaWeight;
    int lumaOffset;

    bool chromaWeightFlag;
    int chromaWeight[H264_MAX_CHROMA_CHANNELS_NB];
    int chromaOffset[H264_MAX_CHROMA_CHANNELS_NB];
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
  static const char * instr[] = {
    "End of instructions marker",
    "Mark a short-term reference picture as 'unused for reference'",
    "Mark a long-term reference picture as 'unused for reference'",
    "Mark a short-term reference picture as 'used for long-term reference'",
    "Set max long-term frame index",
    "Mark all reference pictures as 'unused for reference'",
    "Mark current picture as 'used for long-term reference'"
  };

  if (val < ARRAY_SIZE(instr))
    return instr[val];
  return "Unknown";
}

typedef struct H264MemoryManagementControlOperationsBlock {
  struct H264MemoryManagementControlOperationsBlock * nextOperation;

  H264MemoryManagementControlOperationValue operation;
  unsigned difference_of_pic_nums_minus1;
  unsigned long_term_pic_num;
  unsigned long_term_frame_idx;
  unsigned max_long_term_frame_idx_plus1;
} H264MemMngmntCtrlOpBlk, *H264MemMngmntCtrlOpBlkPtr;

H264MemMngmntCtrlOpBlkPtr createH264MemoryManagementControlOperations(
  void
);
void closeH264MemoryManagementControlOperations(
  H264MemMngmntCtrlOpBlkPtr op
);
H264MemMngmntCtrlOpBlkPtr copyH264MemoryManagementControlOperations(
  H264MemMngmntCtrlOpBlkPtr op
);

typedef struct {
  bool IdrPicFlag; /* For quick access */

  union {
    /* if IdrPicFlag */
    struct {
      bool noOutputOfPriorPics;
      bool longTermReference;
    };

    /* else */
    struct {
      H264MemMngmntCtrlOpBlkPtr MemMngmntCtrlOp;
      bool adaptativeRefPicMarkingMode;
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

const char * h264DeblockingFilterIdcStr(
  const H264DeblockingFilterIdc val
);

typedef struct {
  uint32_t firstMbInSlice;

  H264SliceTypeValue sliceType;
  uint8_t picParametersSetId;
  H264ColourPlaneIdValue colourPlaneId;
  uint16_t frameNum;

  bool field_pic_flag;
  bool bottom_field_flag;

  uint16_t idrPicId;
  uint16_t picOrderCntLsb;
  int deltaPicOrderCntBottom;

  long deltaPicOrderCnt[2];

  uint8_t redundantPicCnt;

  /* if (slice_type == B) */
  bool directSpatialMvPred;

  /* if (slice_type == P || SP || B) */
  bool numRefIdxActiveOverride;
  unsigned numRefIdxl0ActiveMinus1;
  unsigned numRefIdxl1ActiveMinus1;

  H264RefPicListModification refPicListMod;

  H264PredWeightTable predWeightTable;

  bool decRefPicMarkingPresent;
  H264DecRefPicMarking decRefPicMarking;

  uint8_t cabacInitIdc;
  int sliceQpDelta;

  bool spForSwitch;
  int sliceQsDelta;

  H264DeblockingFilterIdc disableDeblockingFilterIdc;
  int sliceAlphaC0OffsetDiv2;
  int sliceBetaOffsetDiv2;

  uint32_t sliceGroupChangeCycle;

  /* Computed parameters: */
  bool refPic;
  bool IdrPicFlag;
  bool isSliceExt;

  unsigned picOrderCntType; /* Copy from active SPS */

  bool mbaffFrameFlag;
  unsigned picHeightInMbs;
  unsigned picHeightInSamplesL;
  unsigned picHeightInSamplesC;

  unsigned picSizeInMbs;
} H264SliceHeaderParameters;

typedef struct {
  H264SliceHeaderParameters header;
  /* H264SliceDataParameters sliceData; */
} H264SliceLayerWithoutPartitioningParameters;

typedef struct {
  uint8_t nalUnitType;

  int64_t startOffset;
  int64_t length;

  bool replace;
  void * replacementParam; /* Type defined by 'nalUnitType' */
} H264AUNalUnit, *H264AUNalUnitPtr;

typedef struct {
  BitstreamReaderPtr inputFile;

  bool packetInitialized;
  uint16_t currentRbspCellBits;
  size_t remainingRbspCellBits;

  uint8_t refIdc;
  uint8_t type;
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

/** binary mask:
 * 0 - I slice type
 * 1 - P slice type
 * 2 - B slice type
 */
typedef enum {
  H264_UNRESTRICTED_SLICE_TYPE       = 0x0,

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
    | H264_RESTRICTED_B_SLICE_TYPE
} H264AllowedSliceTypes;

#define H264_IS_VALID_SLICE_TYPE(restr, type)                                 \
  (                                                                           \
    (restr) == H264_UNRESTRICTED_SLICE_TYPE                                   \
    || (((restr) | ((restr) << 5)) & (1 << (type)))                           \
  )

typedef enum {
  H264_ENTROPY_CODING_MODE_UNRESTRICTED = 0x0,
  H264_ENTROPY_CODING_MODE_CAVLC_ONLY   = 0x1,
  H264_ENTROPY_CODING_MODE_CABAC_ONLY   = 0x2
} H264EntropyCodingModeRestriction;

/** \~english
 * \brief Check if given 'entropy_coding_mode_flag' comply with entropy coding
 * mode restriction.
 *
 * \param restr H264EntropyCodingModeRestriction Restriction condition.
 * \param flag bool PPS 'entropy_coding_mode_flag' flag.
 * \return true PPS entropy coding mode respect restriction condition.
 * \return false PPS entropy coding mode violate restriction condition.
 */
#define H264_IS_VALID_ENTROPY_CODING_MODE_RESTR(restr, flag)                  \
  (                                                                           \
    (restr) == H264_ENTROPY_CODING_MODE_UNRESTRICTED                          \
    || ((flag) ^ ((restr) == H264_ENTROPY_CODING_MODE_CAVLC_ONLY))            \
  )

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
  H264AllowedSliceTypes allowedSliceTypes;
  bool forbiddenSliceDataPartitionLayersNal;
  bool forbiddenArbitrarySliceOrder;
  H264ChromaFormatIdcRestriction restrictedChromaFormatIdc;
  unsigned maxAllowedBitDepthMinus8;
  bool forcedDirect8x8InferenceFlag; /* forcedDirect8x8InferenceFlag || direct_8x8_inference_flag */
  bool restrictedFrameMbsOnlyFlag; /* restrictedFrameMbsOnlyFlag || frame_mbs_only_flag */
  bool forbiddenWeightedPredModesUse;
  bool forbiddenQpprimeYZeroTransformBypass;
  H264EntropyCodingModeRestriction restrictedEntropyCodingMode;
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

#define H264_PARTIAL_PICTURE(curProgParam)  \
  (                                         \
    (curProgParam).bottomFieldPresent ||    \
    (curProgParam).topFieldPresent          \
  )

#define H264_COMPLETE_PICTURE(curProgParam) \
  (                                         \
    (curProgParam).bottomFieldPresent &&    \
    (curProgParam).topFieldPresent          \
  )

typedef struct {
  bool initializedParam;

  double frameRate;
  int64_t frameDuration;
  int64_t fieldDuration;
  /* int64_t startPts; */

  H264PicStructValue lastPicStruct;
  int64_t lastDts;
  int64_t dtsIncrement;

  bool reachVclNaluOfaPrimCodedPic;
  int64_t curNbAU; /**< Current number of access units. */

  int64_t nbPics;
  unsigned curNbConsecutiveBPics;
  unsigned idxPicInGop;

  unsigned curNbSlices;
  uint32_t lastFirstMbInSlice;
  unsigned curNbMbs;

  uint8_t lastNalUnitType;

  /* Separate fields pictures management related: */
  bool bottomFieldPresent;
  bool topFieldPresent;
  bool idrPresent; /* IDR picture present in Access Unit */

  int64_t picOrderCntAU; /* Access Unit lowest picOrderCnt */
  bool initPicOrderCntAU;

  unsigned PrevRefFrameNum; /* PrevRefFrameNum */

  /* Last picture parameters for Picture Order Count 0: */
  int lastIDRPicOrderCnt;

  int PicOrderCntMsb;
  int PicOrderCntLsb;

  bool presenceOfMemManCtrlOp5;
  bool bottomFieldPicture;

  int TopFieldOrderCnt; /* Top-field or frame order count. */

  int maxPicOrderCnt;
  int64_t cumulPicOrderCnt;

  /* Frames sizes parsing : */
  size_t largestFrameSize;
  size_t largestIFrameSize;

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

  bool picOrderCntType2use;
  /* TODO: Forbiddens presence of two consecutive non-referential frames/non-complementary fields. */

  /* Triggered options : */
  bool useVuiRebuilding;
  bool useVuiUpdate;
  bool stillPictureTolerance;
  bool discardUselessAccessUnitDelimiter;
  bool discardUselessSequenceParameterSet;

  size_t curFrameLength; /* in bytes */
  unsigned curFrameNbNalUnits;
  unsigned allocatedNalUnitsCells;
  bool inProcessNalUnitCell;
  H264AUNalUnit * curFrameNalUnits;
} H264CurrentProgressParam;

typedef struct {
  void * linkedParam; /* Type defined by list name */

  size_t length;
  unsigned dataSectionIdx;
} H264ModifiedNalUnit;

typedef struct {
  H264ModifiedNalUnit * sequenceParametersSets;
  unsigned nbSequenceParametersSet;

  bool patchBufferingPeriodSeiPresent;
  H264ModifiedNalUnit bufferingPeriodSeiMsg;
} H264ModifiedNalUnitsList;

typedef struct {
  bool notFirstSyntaxElement;
} H264CabacParsingContext;

const char * H264NalUnitTypeStr(
  const uint8_t nal_unit_type
);

/* Spec data functions : */
unsigned getH264MaxMBPS(
  uint8_t levelIdc
);
unsigned getH264MaxFS(
  uint8_t levelIdc
);
unsigned getH264MaxDpbMbs(
  uint8_t levelIdc
);
unsigned getH264MaxBR(
  uint8_t levelIdc
);
unsigned getH264MaxCPB(
  uint8_t levelIdc
);
unsigned getH264MaxVmvR(
  uint8_t levelIdc
);
unsigned getH264MinCR(
  uint8_t levelIdc
);
unsigned getH264MaxMvsPer2Mb(
  uint8_t levelIdc
);

unsigned getH264SliceRate(
  uint8_t levelIdc
);
unsigned getH264MinLumaBiPredSize(
  uint8_t levelIdc
);
bool getH264direct_8x8_inference_flag(
  uint8_t levelIdc
);
bool getH264frame_mbs_only_flag(
  uint8_t levelIdc
);
unsigned getH264MaxSubMbRectSize(
  uint8_t levelIdc
);

unsigned getH264cpbBrVclFactor(
  H264ProfileIdcValue profileIdc
);
unsigned getH264cpbBrNalFactor(
  H264ProfileIdcValue profileIdc
);

/* ### Blu-ray specifications : ############################################ */

typedef struct {
  H264AspectRatioIdcValue a;
  H264AspectRatioIdcValue b;
} H264BdavExpectedAspectRatioRet;

#define NEW_H264_BDAV_EXPECTED_ASPECT_RATIO_RET(val1, val2)                   \
  (                                                                           \
    (H264BdavExpectedAspectRatioRet) {                                        \
      .a = (val1),                                                            \
      .b = (val2)                                                             \
    }                                                                         \
  )

#define CHECK_H264_BDAV_EXPECTED_ASPECT_RATIO(ret, val)                       \
  (                                                                           \
    (ret).a == (val)                                                          \
    || (ret).b == (val)                                                       \
  )

H264BdavExpectedAspectRatioRet getH264BdavExpectedAspectRatioIdc(
  const unsigned frameWidth,
  const unsigned frameHeight
);

H264VideoFormatValue getH264BdavExpectedVideoFormat(
  const double frameRate
);

H264ColourPrimariesValue getH264BdavExpectedColorPrimaries(
  const unsigned frameHeight
);

H264TransferCharacteristicsValue getH264BdavExpectedTransferCharacteritics(
  const unsigned frameHeight
);

H264MatrixCoefficientsValue getH264BdavExpectedMatrixCoefficients(
  const unsigned frameHeight
);

/** \~english
 * \brief Max allowed number of CPB configurations.
 *
 * This value may be fixed to a value equal or greater than 32 to disable
 * limitation (32 is the maximum value allowed by H.264 specifications).
 */
#define H264_BDAV_ALLOWED_CPB_CNT 1

#endif