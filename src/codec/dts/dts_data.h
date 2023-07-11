/** \~english
 * \file dts_data.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief DTS audio bitstreams data structures module.
 */

#ifndef __LIBBLU_MUXER__CODECS__DTS__DATA_H__
#define __LIBBLU_MUXER__CODECS__DTS__DATA_H__

#include "../../util.h"

typedef enum {
  DTSHD_BSM__IS_VBR          = 0x0001,
  DTSHD_BSM__PBRS_PERFORMED  = 0x0002,
  DTSHD_BSM__NAVI_EMBEDDED   = 0x0004,
  DTSHD_BSM__CORE_PRESENT    = 0x0008,
  DTSHD_BSM__EXTSS_PRESENT   = 0x0010
} DtshdBitwStreamMetadataMask;

/** \~english
 * \brief Minimal allowed DTSHDHDR chunk size.
 *
 * 14 payload bytes.
 */
#define DTSHD_DTSHDHDR_SIZE  14

typedef struct {
  uint64_t Hdr_Byte_Size;
  uint32_t Hdr_Version;

  uint8_t RefClockCode;
  uint32_t TimeStamp;
  uint8_t TC_Frame_Rate;
  uint16_t Bitw_Stream_Metadata;
  uint8_t Num_Audio_Presentations;
  uint8_t Number_Of_Ext_Sub_Streams;
  unsigned Num_ExSS;
} DtshdFileHeaderChunk;

typedef struct {
  uint64_t FILEINFO_Text_Byte_Size;
  char * text;
} DtshdFileInfo;

/** \~english
 * \brief Minimal allowed CORESSMD chunk size.
 *
 * 11 payload bytes.
 */
#define DTSHD_CORESSMD_SIZE  11

typedef struct {
  uint64_t Core_Ss_Md_Bytes_Size;

  uint32_t Core_Ss_Max_Sample_Rate_Hz;
  uint16_t Core_Ss_Bit_Rate_Kbps;
  uint16_t Core_Ss_Channel_Mask;
  uint32_t Core_Ss_Frame_Payload_In_Bytes;
} DtshdCoreSubStrmMeta;

typedef struct {
  uint64_t Ext_Ss_Md_Bytes_Size;

  uint32_t Ext_Ss_Avg_Bit_Rate_Kbps;
  union {
    struct {
      uint32_t Ext_Ss_Peak_Bit_Rate_Kbps;
      uint16_t Pbr_Smooth_Buff_Size_Kb;
    } vbr;
    struct {
      uint32_t Ext_Ss_Frame_Payload_In_Bytes;
    } cbr;
  };
} DtshdExtSubStrmMeta;


#define DTSHD_AUPR_HDR_MINSIZE  21

#define DTSHD_AUPR_HDR_CORE_SIZE  7
#define DTSHD_AUPR_HDR_XLL_SIZE  1

typedef enum {
  DTSHD_BAM__CORE_PRESENT      = 0x0001,
  DTSHD_BAM__CORE_IN_EXTSS     = 0x0002,
  DTSHD_BAM__LOSSLESS_PRESENT  = 0x0004,
  DTSHD_BAM__LBR_PRESENT       = 0x0008,
} DtshdBitwAupresMetadataMask;

typedef struct {
  uint64_t Audio_Pres_Hdr_Md_Bytes_Size;

  uint8_t Audio_Pres_Index;
  uint16_t Bitw_Aupres_Metadata;
  uint32_t Max_Sample_Rate_Hz;
  uint32_t Num_Frames_Total;
  uint16_t Samples_Per_Frame_At_Max_Fs;
  uint64_t Num_Samples_Orig_Audio_At_Max_Fs;
  uint16_t Channel_Mask;
  uint16_t Codec_Delay_At_Max_Fs;

  uint32_t BC_Core_Max_Sample_Rate_Hz;
  uint16_t BC_Core_Bit_Rate_Kbps;
  uint16_t BC_Core_Channel_Mask;

  uint8_t LSB_Trim_Percent;
} DtshdAudioPresPropHeaderMeta;

#define DTSHD_AUPRINFO_MINSIZE  1

typedef struct {
  uint64_t Audio_Pres_Info_Text_Byte_Size;

  uint8_t Audio_Pres_Info_Text_Index;
  char * text;
} DtshdAudioPresText;

typedef struct {
  uint64_t Navi_Tbl_Md_Bytes_Size;
} DtshdNavMeta;

typedef struct {
  uint64_t Stream_Data_Byte_Size;
} DtshdStreamData;

#define DTSHD_TIMECODE_SIZE  29

typedef struct {
  uint64_t Timecode_Data_Byte_Size;

  uint32_t Timecode_Clock;
  uint8_t Timecode_Frame_Rate;
  uint64_t Start_Samples_Since_Midnight;
  uint32_t Start_Residual;
  uint64_t Reference_Samples_Since_Midnight;
  uint32_t Reference_Residual;
} DtshdTimecode;

typedef struct {
  uint64_t BuildVer_Data_Byte_Size;

  char * text;
} DtshdBuildVer;

typedef struct {
  uint64_t Blackout_Data_Byte_Size;

  uint8_t * Blackout_Frame;
} DtshdBlackout;

typedef enum {
  DCA_SYNCWORD_CORE  = 0x7FFE8001,
  DCA_SYNCEXTSSH     = 0x64582025,
  DCA_SYNCXLL        = 0x41A29547
} DcaSyncword;

/* ### DTS Coherent Acoustics (DCA) Core audio : ########################### */

/* ###### AMODE : ########################################################## */

/** \~english
 * \brief DCA Core channel assignement code, AMODE values.
 *
 * Other values means user defined.
 */
typedef enum {
  DCA_AMODE_A                          = 0x0,
  DCA_AMODE_A_B                        = 0x1,
  DCA_AMODE_L_R                        = 0x2,
  DCA_AMODE_L_R_SUMDIF                 = 0x3,
  DCA_AMODE_LT_LR                      = 0x4,
  DCA_AMODE_C_L_R                      = 0x5,
  DCA_AMODE_L_R_S                      = 0x6,
  DCA_AMODE_C_L_R_S                    = 0x7,
  DCA_AMODE_L_R_SL_SR                  = 0x8,
  DCA_AMODE_C_L_R_SL_SR                = 0x9,
  DCA_AMODE_CL_CR_L_R_SL_SR            = 0xA,
  DCA_AMODE_C_L_R_LR_RR_OV             = 0xB,
  DCA_AMODE_CF_LF_LF_RF_LR_RR          = 0xC,
  DCA_AMODE_CL_C_CR_L_R_SL_SR          = 0xD,
  DCA_AMODE_CL_CR_L_R_SL1_SL2_SR1_SR2  = 0xE,
  DCA_AMODE_CL_C_CR_L_R_SL_S_SR        = 0xF
} DcaCoreAudioChannelAssignCode;

/** \~english
 * \brief Return from given AMODE field value the number of audio channels
 * associated (CHS).
 *
 * \param AMODE AMODE field value.
 * \return unsigned Number of audio channels (CHS).
 */
static inline unsigned getNbChDcaCoreAudioChannelAssignCode(
  DcaCoreAudioChannelAssignCode AMODE
)
{
  static const unsigned nb_ch[] = {
    1, 1, 2, 2, 2, 3, 3, 3, 4, 5, 6, 6, 6, 7, 8, 8
  };
  if (AMODE < ARRAY_SIZE(nb_ch))
    return nb_ch[AMODE];
  return 0; /* User defined */
}

/** \~english
 * \brief Return AMODE field value string representation.
 *
 * \param AMODE
 * \return const char*
 */
static inline const char * DcaCoreAudioChannelAssignCodeStr(
  const DcaCoreAudioChannelAssignCode AMODE
)
{
  static const char * strings[] = {
    "C (Mono)",
    "C+C (Dual-Mono)",
    "L, R (Stereo 2.)",
    "(L+R), (L-R) (Sum-differential Stereo)",
    "Lt, Rt (Stereo 2. total)",
    "C, L, R (Stereo 3.)",
    "L, R, S (Surround 3.)",
    "C, L, R, S (Quadraphonic)",
    "L, R, Ls, Rs (Quadraphonic)",
    "C, L, R, Ls, Rs (Surround 5.)",
    "C, L, R, Ls, Rs, Oh",
    "C, L, R, Ls, Cs, Rs",
    "Lc, C, Rc, L, R, Ls, Rs (Surround 7.)",
    "Lc, Rc, L, R, Lss, Ls, Rs, Rss",
    "Lc, C, Rc, L, R, Ls, Cs, Rs"
  };

  if (AMODE < ARRAY_SIZE(strings))
    return strings[AMODE];
  return "User defined";
}

/* ###### SFREQ : ########################################################## */

/** \~english
 * \brief DCA Core audio sampling frequency code, SFREQ values.
 *
 */
typedef enum {
  DCA_SFREQ_8_KHZ     = 0x1,
  DCA_SFREQ_16_KHZ    = 0x2,
  DCA_SFREQ_32_KHZ    = 0x3,
  DCA_SFREQ_11025_HZ  = 0x6,
  DCA_SFREQ_22050_HZ  = 0x7,
  DCA_SFREQ_44100_HZ  = 0x8,
  DCA_SFREQ_12_KHZ    = 0xB,
  DCA_SFREQ_24_KHZ    = 0xC,
  DCA_SFREQ_48_KHZ    = 0xD
} DcaCoreAudioSampFreqCode;

static inline unsigned getDcaCoreAudioSampFreqCode(
  DcaCoreAudioSampFreqCode SFREQ
)
{
  static const unsigned values[] = {
    0,
    8000, 16000, 32000,
    0, 0,
    11025, 22050, 44100,
    0, 0,
    12000, 24000, 48000
  };

  if (SFREQ < ARRAY_SIZE(values))
    return values[SFREQ];
  return 0;
}

/* ###### RATE : ########################################################### */

/** \~english
 * \brief DCA Core transmission bitrate code values.
 *
 */
typedef enum {
  DCA_RATE_32    = 0x00,
  DCA_RATE_56    = 0x01,
  DCA_RATE_64    = 0x02,
  DCA_RATE_96    = 0x03,
  DCA_RATE_112   = 0x04,
  DCA_RATE_128   = 0x05,
  DCA_RATE_192   = 0x06,
  DCA_RATE_224   = 0x07,
  DCA_RATE_256   = 0x08,
  DCA_RATE_320   = 0x09,
  DCA_RATE_384   = 0x0A,
  DCA_RATE_448   = 0x0B,
  DCA_RATE_512   = 0x0C,
  DCA_RATE_576   = 0x0D,
  DCA_RATE_640   = 0x0E,
  DCA_RATE_768   = 0x0F,
  DCA_RATE_960   = 0x10,
  DCA_RATE_1024  = 0x11,
  DCA_RATE_1152  = 0x12,
  DCA_RATE_1280  = 0x13,
  DCA_RATE_1344  = 0x14,
  DCA_RATE_1408  = 0x15,
  DCA_RATE_1411  = 0x16,
  DCA_RATE_1472  = 0x17,
  DCA_RATE_1536  = 0x18,
  DCA_RATE_OPEN  = 0x1D
} DcaCoreTransBitRate;

/** \~english
 * \brief Return from given #DcaCoreTransBitRate value the bitrate value
 * in Kbit/s associated.
 *
 * If given RATE == #DCA_RATE_OPEN, 1 is returned.
 *
 * \param RATE
 * \return unsigned
 */
static inline unsigned getDcaCoreTransBitRate(
  DcaCoreTransBitRate RATE
)
{
  static const unsigned bitrates[] = {
    32, 56, 64, 96, 112, 128, 192, 224, 256, 320, 384,
    448, 512, 576, 640, 768, 960, 1024, 1152, 1280,
    1344, 1408, 1411, 1472, 1536, 1 // 1 == 'open'
  };
  if (RATE < ARRAY_SIZE(bitrates))
    return bitrates[RATE];
  return 0;
}

/* ###### EXT_AUDIO_ID : ################################################### */

typedef enum {
  DCA_EXT_AUDIO_ID_XCH   = 0x0,  /**< Channel extension.                     */
  DCA_EXT_AUDIO_ID_X96   = 0x2,  /**< Sampling frequency extension.          */
  DCA_EXT_AUDIO_ID_XXCH  = 0x6   /**< Channel extension.                     */
} DcaCoreExtendedAudioCodingCode;

static inline bool isValidDcaCoreExtendedAudioCodingCode(
  DcaCoreExtendedAudioCodingCode EXT_AUDIO_ID
)
{
  return
    EXT_AUDIO_ID == DCA_EXT_AUDIO_ID_XCH
    || EXT_AUDIO_ID == DCA_EXT_AUDIO_ID_X96
    || EXT_AUDIO_ID == DCA_EXT_AUDIO_ID_XXCH
  ;
}

static inline const char * DcaCoreExtendedAudioCodingCodeStr(
  DcaCoreExtendedAudioCodingCode EXT_AUDIO_ID
)
{
  switch (EXT_AUDIO_ID) {
    case DCA_EXT_AUDIO_ID_XCH:
      return "DTS XCH (Extra centre surround channel extension)";
    case DCA_EXT_AUDIO_ID_X96:
      return "DTS X96 (96KHz sampling frequency extension)";
    case DCA_EXT_AUDIO_ID_XXCH:
      return "DTS XXCH (Channel extension).\n";
  }
  return "Reserved value";
}

/* ###### LFF : ############################################################ */

/** \~english
 * \brief DCA Core Low Frequency effects Flag, LFF values.
 *
 */
typedef enum {
  DCA_LFF_NOT_PRESENT    = 0x0,  /**< Not present.                           */
  DCA_LFF_PRESENT_128IF  = 0x1,  /**< Present, 128 Interpolation Factor.     */
  DCA_LFF_PRESENT_64IF   = 0x2,  /**< Present, 64 Interpolation Factor.      */
  DCA_LFF_INVALID        = 0x3   /**< Invalid value.                         */
} DcaCoreLowFrequencyEffectsFlag;

static inline const char * DcaCoreLowFrequencyEffectsFlagStr(
  DcaCoreLowFrequencyEffectsFlag LFF
)
{
  static const char * strings[] = {
    "Not present",
    "Present, 128 Interpolation Factor",
    "Present, 64 Interpolation Factor"
  };
  if (LFF < ARRAY_SIZE(strings))
    return strings[LFF];
  return "Invalid";
}

/* ###### VERNUM : ######################################################### */

/** \~english
 * \brief Maximal supported Encoder Software Revision code.
 */
#define DCA_MAX_SYNTAX_VERNUM 0x7

/* ###### CHIST : ########################################################## */

typedef enum {
  DCA_CHIST_NO_COPY     = 0x0,
  DCA_CHIST_FIRST_GEN   = 0x1,
  DCA_CHIST_SECOND_GEN  = 0x2,
  DCA_CHIST_FREE_COPY   = 0x3
} DcaCoreCopyrightHistoryCode;

static inline const char * DcaCoreCopyrightHistoryCodeStr(
  DcaCoreCopyrightHistoryCode CHIST
)
{
  static const char * strings[] = {
    "Forbidden copy",
    "First generation",
    "Second generation",
    "Free copy"
  };

  if (CHIST < ARRAY_SIZE(strings))
    return strings[CHIST];
  return "Unknown";
}

/* ###### PCMR : ########################################################### */

typedef enum {
  DCA_PCMR_16,
  DCA_PCMR_16_ES,
  DCA_PCMR_20,
  DCA_PCMR_20_ES,
  DCA_PCMR_24,
  DCA_PCMR_24_ES
} DcaCoreSourcePcmResCode;

static inline unsigned getDcaCoreSourcePcmResCode(
  DcaCoreSourcePcmResCode PCMR
)
{
  static const unsigned bitDepths[7] = {
    16, 16, 20, 20, 0, 24, 24
  };

  if (PCMR < ARRAY_SIZE(bitDepths))
    return bitDepths[PCMR];
  return 0; // Invalid
}

static inline bool isEsDcaCoreSourcePcmResCode(
  DcaCoreSourcePcmResCode PCMR
)
{
  return PCMR & 0x1;
}

/* ###### SUMF/SUMS : ###################################################### */

/** \~english
 * \brief DCA Core Front/Surround channels Sum/Difference flag field,
 * SUMF/SUMS values.
 *
 */
typedef enum {
  DCA_SUM_NOT_ENCODED  = 0x0,
  DCA_SUM_ENCODED      = 0x1
} DcaCoreChPairSumDiffEncodedFlag;

static inline const char * frontChDcaCoreChPairSumDiffEncodedFlagStr(
  DcaCoreChPairSumDiffEncodedFlag SUMF
)
{
  if (SUMF)
    return "Normal, L=L, R=R";
  return "Sum/Difference, L=L+R, R=L-R";
}

static inline const char * surChDcaCoreChPairSumDiffEncodedFlagStr(
  DcaCoreChPairSumDiffEncodedFlag SUMS
)
{
  if (SUMS)
    return "Normal, Ls=Ls, Rs=Rs";
  return "Sum/Difference, Ls=Ls+Rs, Rs=Ls-Rs";
}

/* ###### DIALNORM/DNG : ################################################### */

static inline int getDcaCoreDialogNormalizationValue(
  uint8_t DIALNORM,
  unsigned VERNUM
)
{
  if (0x6 == VERNUM)
    return 0 - (16 + DIALNORM);
  if (0x7 == VERNUM)
    return 0 - DIALNORM;
  return 0;
}

/* ######################################################################### */

/** \~english
 * \brief DCA Bit-stream header.
 *
 */
typedef struct {
  bool FTYPE;            /**< Frame type. */
  uint8_t SHORT;         /**< Deficit Sample Count. */
  bool CPF;              /**< CRC Present flag. */
  uint8_t NBLKS;         /**< Number of PCM Sample Blocks.  */
  uint16_t FSIZE;        /**< Primary Frame Byte Size. */
  uint8_t AMODE;         /**< Audio Channel Arrangement. */
  uint8_t SFREQ;         /**< Core Audio Sampling Frequency. */
  uint8_t RATE;          /**< Transmission Bit Rate. */
  bool DYNF;             /**< Embedded Dynamic Range Flag. */
  bool TIMEF;            /**< Embedded Time Stamp Flag. */
  bool AUXF;             /**< Auxiliary Data Flag. */
  bool HDCD;             /**< HDCD. */
  uint8_t EXT_AUDIO_ID;  /**< Extension Audio Descriptor Flag. */
  bool EXT_AUDIO;        /**< Extended Coding Flag. */
  bool ASPF;             /**< Audio Sync Word Insertion Flag. */
  uint8_t LFF;           /**< Low Frequency Effects Flag. */
  bool HFLAG;            /**< Predictor History Flag Switch. */
  uint16_t HCRC;         /**< Header CRC Check Bytes. */
  bool FILTS;            /**< Multirate Interpolator Switch. */
  uint8_t VERNUM;        /**< Encoder Software Revision. */
  uint8_t CHIST;         /**< Copy History. */
  uint8_t PCMR;          /**< Source PCM Resolution. */
  bool SUMF;             /**< Front Sum/Difference Flag. */
  bool SUMS;             /**< Surrounds Sum/Difference Flag. */
  uint8_t DIALNORM;      /**< Dialog Normalization / Unspecified. */
} DcaCoreBSHeaderParameters;

/** \~english
 * \brief DCA Core frame bit-stream header minimal size in bytes.
 *
 */
#define DCA_CORE_BSH_MINSIZE  13

static inline unsigned getSizeDcaCoreBSHeader(
  const DcaCoreBSHeaderParameters * bsh
)
{
  if (bsh->CPF)
    return DCA_CORE_BSH_MINSIZE + 2; // Extra HCRC field word.
  return DCA_CORE_BSH_MINSIZE;
}

typedef struct {
  DcaCoreBSHeaderParameters bs_header;
} DcaCoreSSFrameParameters;

typedef struct {
  int64_t size;

  bool syncWordPresent;
  unsigned peakBitRateSmoothingBufSizeCode;
  size_t peakBitRateSmoothingBufSize;
  unsigned initialXllDecodingDelayInFrames;
  size_t nbBytesOffXllSync;

  uint8_t steamId;
} DcaAudioAssetSSXllParameters;

/* ######################################################################### */

#define DCA_EXT_SS_DISABLE_MIX_META_SUPPORT false
#define DCA_EXT_SS_ENABLE_DRC_2 true
#define DCA_EXT_SS_DISABLE_CRC false
#define DCA_EXT_SS_STRICT_NOT_DIRECT_SPEAKERS_FEED_REJECT false

#define DTS_EXT_SS_LANGUAGE_DESC_SIZE  3

#define DTS_EXT_SS_MAX_STRING_TEXT_MAX_LEN  1024

#define DTS_EXT_SS_MAX_CHANNELS_NB  25

#define DTS_EXT_SS_MAX_SPEAKERS_SETS_NB  16

#define DTS_EXT_SS_MAX_OUT_MIX_CONFIGS_NB  4
#define DTS_EXT_SS_MAX_NB_REMAP_SETS  7
#define DTS_EXT_SS_MAX_DOWNMIXES_NB  3

#define DTS_EXT_SS_MAX_AUDIO_PRES_NB  8
#define DTS_EXT_SS_MAX_EXT_SUBSTREAMS_NB  4

/** \~english
 * \brief Define the maximal supported Ext SS Reserved fields size in bytes.
 *
 * If field size exceed resFieldLength * 8 + paddingBits bits,
 * the reserved field data isn't saved.
 */
#define DCA_EXT_SS_MAX_SUPP_RES_FIELD_SIZE 16

#define DCA_EXT_SS_IS_SUPP_RES_FIELD_SIZES(ReservedSize, ByteAlignSize)       \
  (                                                                           \
    (8 * (ReservedSize) + (ByteAlignSize))                                    \
    <= 8 * DCA_EXT_SS_MAX_SUPP_RES_FIELD_SIZE                                 \
  )

typedef struct {
  bool assetTypeDescriptorPresent;
  uint8_t assetTypeDescriptor;

  bool languageDescriptorPresent;
  uint8_t languageDescriptor[
    DTS_EXT_SS_LANGUAGE_DESC_SIZE + 1
  ];

  bool infoTextPresent;
  uint8_t infoText[DTS_EXT_SS_MAX_STRING_TEXT_MAX_LEN + 1];
  int64_t infoTextLength;

  unsigned bitDepth;
  uint8_t maxSampleRateCode;
  unsigned nbChannels;

  bool directSpeakersFeed;
  bool embeddedStereoDownmix;
  bool embeddedSurround6chDownmix;
  uint8_t representationType; /* Only if directSpeakersFeed == 0b0 */

  bool bSpkrMaskEnabled;
  uint16_t nuSpkrActivityMask;

  unsigned nuNumSpkrRemapSets;
  uint16_t nuStndrSpkrLayoutMask[DTS_EXT_SS_MAX_NB_REMAP_SETS];
  unsigned nbChsInRemapSet[DTS_EXT_SS_MAX_NB_REMAP_SETS];
  unsigned nbChRequiredByRemapSet[DTS_EXT_SS_MAX_NB_REMAP_SETS];
  /* decodedChannelsLinkedToSetSpeakerLayoutMask : */
  uint16_t nuRemapDecChMask[DTS_EXT_SS_MAX_NB_REMAP_SETS][DTS_EXT_SS_MAX_CHANNELS_NB];
  uint8_t nbRemapCoeffCodes[DTS_EXT_SS_MAX_NB_REMAP_SETS][DTS_EXT_SS_MAX_CHANNELS_NB];
  uint8_t outputSpkrRemapCoeffCodes[DTS_EXT_SS_MAX_NB_REMAP_SETS][DTS_EXT_SS_MAX_CHANNELS_NB][DTS_EXT_SS_MAX_SPEAKERS_SETS_NB];

  /* Computed parameters */
  unsigned maxSampleRate;
} DcaAudioAssetDescriptorStaticFieldsParameters;

/* Value used if 'bDRCMetadatRev2Present' is false: */
#define DEFAULT_DRC_VERSION_VALUE -1

typedef struct {
  bool drcEnabled;       /**< bDRCCoefPresent                                */
  struct {
    uint8_t drcCode;     /**< nuDRCCode                                      */
    uint8_t drc2ChCode;  /**< nuDRC2ChDmixCode                               */
  } drcParameters;       /**< DRC parameters present if (bDRCCoefPresent)    */

  bool dialNormEnabled;  /**< bDialNormPresent                               */
  uint8_t dialNormCode;  /**< nuDialNormCode                                 */

  bool mixMetadataPresent;  /**< bMixMetadataPresent                         */
  struct {
#if !DCA_EXT_SS_DISABLE_MIX_META_SUPPORT
    bool useExternalMix;            /**< bExternalMixFlag                    */
    uint8_t postMixGainCode;        /**< nuPostMixGainAdjCode                */
    uint8_t drcMixerControlCode;    /**< nuControlMixerDRC                   */
    union {
      uint8_t limitDRCPriorMix;       /**< nuLimit4EmbeddedDRC               */
      uint8_t customMixDRCCoeffCode;  /**< nuCustomDRCCode                   */
    };

    bool perMainAudioChSepScal;     /**< bEnblPerChMainAudioScale            */
    uint8_t scalingAudioParam[
      DTS_EXT_SS_MAX_OUT_MIX_CONFIGS_NB
    ][DTS_EXT_SS_MAX_CHANNELS_NB];             /**< nuMainAudioScaleCode, if
      'perMainAudioChSepScal == 0b0', the scaling code shared between all
      channels of one mix config is stored as for the channel of index 0,
      'scalingAudioParam[configId][0]'.                                      */

    unsigned nbDownMixes;
    unsigned nbChPerDownMix[DTS_EXT_SS_MAX_DOWNMIXES_NB];
    uint16_t mixOutputMappingMask[DTS_EXT_SS_MAX_OUT_MIX_CONFIGS_NB][DTS_EXT_SS_MAX_DOWNMIXES_NB][DTS_EXT_SS_MAX_CHANNELS_NB];
    unsigned mixOutputMappingNbCoeff[DTS_EXT_SS_MAX_OUT_MIX_CONFIGS_NB][DTS_EXT_SS_MAX_DOWNMIXES_NB][DTS_EXT_SS_MAX_CHANNELS_NB];
    uint8_t mixOutputCoefficients[DTS_EXT_SS_MAX_OUT_MIX_CONFIGS_NB][DTS_EXT_SS_MAX_DOWNMIXES_NB][DTS_EXT_SS_MAX_DOWNMIXES_NB][DTS_EXT_SS_MAX_SPEAKERS_SETS_NB];
#endif
  } mixMetadata;

} DcaAudioAssetDescriptorDynamicMetadataParameters;

typedef enum {
  DCA_EXT_SS_CODING_MODE_DTS_HD_COMPONENTS             = 0x0,
  DCA_EXT_SS_CODING_MODE_DTS_HD_LOSSLESS_WITHOUT_CORE  = 0x1,
  DCA_EXT_SS_CODING_MODE_DTS_HD_LOW_BITRATE            = 0x2,
  DCA_EXT_SS_CODING_MODE_AUXILIARY_CODING              = 0x3
} DcaAudioAssetCodingMode;

typedef enum {
  DCA_EXT_SS_COD_COMP_CORE_DCA         = (1 <<  0),
  DCA_EXT_SS_COD_COMP_CORE_EXT_XXCH    = (1 <<  1),
  DCA_EXT_SS_COD_COMP_CORE_EXT_X96     = (1 <<  2),
  DCA_EXT_SS_COD_COMP_CORE_EXT_XCH     = (1 <<  3),
  DCA_EXT_SS_COD_COMP_EXTSUB_CORE_DCA  = (1 <<  4),
  DCA_EXT_SS_COD_COMP_EXTSUB_XBR       = (1 <<  5),
  DCA_EXT_SS_COD_COMP_EXTSUB_XXCH      = (1 <<  6),
  DCA_EXT_SS_COD_COMP_EXTSUB_X96       = (1 <<  7),
  DCA_EXT_SS_COD_COMP_EXTSUB_LBR       = (1 <<  8),
  DCA_EXT_SS_COD_COMP_EXTSUB_XLL       = (1 <<  9),
  DCA_EXT_SS_COD_COMP_RESERVED_1       = (1 << 10),
  DCA_EXT_SS_COD_COMP_RESERVED_2       = (1 << 11)
} DcaAudioAssetCodingComponentMask;

typedef enum {
  DCA_EXT_SS_DRC_REV_2_VERSION_1 = 1
} DcaAudioDrcMetadataRev2Version;

typedef struct {
  DcaAudioAssetCodingMode codingMode;
  uint16_t codingComponentsUsedMask;

  union {
    struct {
      struct {
        int64_t size;
        bool syncWordPresent;
        unsigned syncDistanceInFramesCode;
        unsigned syncDistanceInFrames;
      } extSSCore;

      struct {
        int64_t size;
      } extSSXbr;

      struct {
        int64_t size;
      } extSSXxch;

      struct {
        int64_t size;
      } extSSX96;

      struct {
        int64_t size;
        bool syncWordPresent;
        unsigned syncDistanceInFramesCode;
        unsigned syncDistanceInFrames;
      } extSSLbr;

      DcaAudioAssetSSXllParameters extSSXll;

      uint16_t reservedExtension1_data;
      uint16_t reservedExtension2_data;
    } codingComponents;

    struct {
      int64_t size;
      uint8_t auxCodecId;
      bool syncWordPresent;
      unsigned syncDistanceInFramesCode;
      unsigned syncDistanceInFrames;
    } auxilaryCoding;
  };

  struct {
    bool oneTrackToOneChannelMix;
    bool perChannelMainAudioScaleCode;
    uint8_t mainAudioScaleCodes[DTS_EXT_SS_MAX_OUT_MIX_CONFIGS_NB][DTS_EXT_SS_MAX_CHANNELS_NB];
  } mixMetadata;
  bool mixMetadataFieldsPresent;

  bool decodeInSecondaryDecoder;

  bool extraDataPresent;
  bool drcRev2Present;
  struct {
    unsigned Hdr_Version;
  } drcRev2;
} DcaAudioAssetDescriptorDecoderNavDataParameters;

typedef struct {
  int64_t descriptorLength;

  unsigned assetIndex;

  DcaAudioAssetDescriptorStaticFieldsParameters staticFields;
  bool staticFieldsPresent; /* Copy from ExtSS Header */
  DcaAudioAssetDescriptorDynamicMetadataParameters dynMetadata;
  DcaAudioAssetDescriptorDecoderNavDataParameters decNavData;

  unsigned resFieldLength;
  unsigned paddingBits;
  uint8_t resFieldData[
    DCA_EXT_SS_MAX_SUPP_RES_FIELD_SIZE
  ];
} DcaAudioAssetDescriptorParameters;

typedef struct {
  uint8_t nuMixMetadataAdjLevel;
  unsigned nuNumMixOutConfigs;

  uint16_t nuMixOutChMask[DTS_EXT_SS_MAX_OUT_MIX_CONFIGS_NB];
  unsigned nNumMixOutCh[DTS_EXT_SS_MAX_OUT_MIX_CONFIGS_NB];
} DcaExtSSHeaderMixMetadataParameters;

typedef struct {
  uint8_t referenceClockCode;

  uint8_t frameDurationCode;
  unsigned frameDurationCodeValue;

  bool timeStampPresent;
  uint32_t timeStampValue;
  uint8_t timeStampLsb;

  unsigned nbAudioPresentations;
  unsigned nbAudioAssets;

  uint8_t activeExtSSMask[DTS_EXT_SS_MAX_AUDIO_PRES_NB];
  uint8_t activeAssetMask[DTS_EXT_SS_MAX_AUDIO_PRES_NB][DTS_EXT_SS_MAX_EXT_SUBSTREAMS_NB];

  bool mixMetadataEnabled;
  DcaExtSSHeaderMixMetadataParameters mixMetadata;

  /* Computed parameters */
  unsigned referenceClockFreq;
  float frameDuration;
  uint64_t timeStamp;
} DcaExtSSHeaderStaticFieldsParameters;

#define DCA_EXT_SS_CRC_LENGTH 16
#define DCA_EXT_SS_CRC_POLY 0x11021
#define DCA_EXT_SS_CRC_MODULO 0x10000
#define DCA_EXT_SS_CRC_INITIAL_V 0xFFFF

#define DCA_EXT_SS_CRC_PARAM()                                                \
  (CrcParam) {.table = ccitt_crc_16_table, .length = 16}

#define DCA_EXT_SS_MAX_NB_INDEXES 4
#define DCA_EXT_SS_MAX_NB_AUDIO_ASSETS 8

typedef struct {
  uint8_t userDefinedBits;
  uint8_t extSSIdx;

  bool longHeaderSizeFlag;

  int64_t extensionSubstreamHeaderLength;
  int64_t extensionSubstreamFrameLength;

  bool staticFieldsPresent; /* Mandatory in most cases */
  DcaExtSSHeaderStaticFieldsParameters staticFields;

  int64_t audioAssetsLengths[DCA_EXT_SS_MAX_NB_AUDIO_ASSETS];
  DcaAudioAssetDescriptorParameters audioAssets[DCA_EXT_SS_MAX_NB_AUDIO_ASSETS];

  bool audioAssetBckwdCompCorePresent[DCA_EXT_SS_MAX_NB_AUDIO_ASSETS];
  struct {
    uint8_t extSSIndex;
    uint8_t assetIndex;
  } audioAssetBckwdCompCore[DCA_EXT_SS_MAX_NB_AUDIO_ASSETS];

  int64_t resFieldLength;  /**< Reserved */
  int64_t paddingBits;
  uint8_t resFieldData[
    DCA_EXT_SS_MAX_SUPP_RES_FIELD_SIZE
  ];  /**< Reserved field data content array, only in use if field size does
    not exceed its size. See #DCA_EXT_SS_MAX_SUPP_RES_FIELD_SIZE.            */

  uint16_t HCRC;
} DcaExtSSHeaderParameters;

typedef struct {
  DcaExtSSHeaderParameters header;
} DcaExtSSFrameParameters;

#define DTS_XLL_MAX_SUPPORTED_OFILE_POS 8

typedef struct {
  int64_t off;
  size_t len;
} DcaXllFrameSFPositionIndex;

/** \~english
 * \brief DTS XLL frame Source File position.
 *
 */
typedef struct {
  DcaXllFrameSFPositionIndex sourceOffsets[
    DTS_XLL_MAX_SUPPORTED_OFILE_POS
  ];
  int nbSourceOffsets;
} DcaXllFrameSFPosition;

#define INIT_DTS_XLL_FRAME_SF_POS()                                           \
  (                                                                           \
    (DcaXllFrameSFPosition) {                                                 \
      .nbSourceOffsets = 0                                                    \
    }                                                                         \
  )

#define IS_EMPTY_XLL_FRAME_SF_POS(frmPos)                                     \
  ((frmPos).nbSourceOffsets == 0)

/** \~english
 * \brief Register given source file offset and length to given
 * #DcaXllFrameSFPosition structure.
 *
 * If too many source offsets are already been defined, the given error
 * instruction is executed.
 *
 * \param #DcaXllFrameSFPosition Destination frame source file position
 * recorder.
 * \param offset off_t Original file XLL frame piece start offset.
 * \param length size_t XLL frame piece length.
 * \param errinstr Error instruction executed if too many indexes have been
 * used, E.g. 'LIBBLU_ERROR_RETURN("Error")'.
 */
#define ADD_DTS_XLL_FRAME_SF_POS(dst, offset, length, errinstr)               \
  {                                                                           \
    if (DTS_XLL_MAX_SUPPORTED_OFILE_POS <= (dst).nbSourceOffsets)             \
      errinstr;                                                               \
    (dst).sourceOffsets[(dst).nbSourceOffsets].off = (offset);                \
    (dst).sourceOffsets[(dst).nbSourceOffsets].len = (length);                \
    (dst).nbSourceOffsets++;                                                  \
  }

typedef struct {
  unsigned number;     /**< Used for stats, number of the PBR frame in
    bitstream.                                                               */
  size_t size;         /**< Frame size in bytes.                             */

  DcaXllFrameSFPosition pos;

  unsigned pbrDelay;   /**< Frame decoding delay according to audio asset
    parameters. If this value is greater than zero, data is accumulated in
    PBR buffer prior to decoding.                                            */
} DcaXllPbrFrame, *DcaXllPbrFramePtr;

/** \~english
 * \brief DTS XLL PBR smoothing buffer maximum size in bytes.\n
 *
 * Value is equal to 240 kBytes in binary representation (245 760 bytes).
 */
#define DTS_XLL_MAX_PBR_BUFFER_SIZE (240 << 10)

/** \~english
 * \brief Max supported DTS-HDMA nVersion number.
 */
#define DTS_XLL_MAX_VERSION 1

/** \~english
 * \brief
 *
 */
#define DTS_XLL_HEADER_MIN_SIZE 12

#define DTS_XLL_MAX_CHSETS_NB 3

#define DTS_XLL_MAX_SEGMENTS_NB 1024

/** \~english
 * \brief Up to 48 kHz.
 *
 */
#define DTS_XLL_MAX_SAMPLES_PER_SEGMENT_UT_48KHZ 256

/** \~english
 * \brief More than 48 kHz.
 *
 */
#define DTS_XLL_MAX_SAMPLES_PER_SEGMENT_MT_48KHZ 512

/** \~english
 * \brief
 *
 * At 512 samples/seg, up to 128 segments.
 * At 256 samples/seg, up to 256 segments.
 */
#define DTS_XLL_MAX_SAMPLES_NB 65536

typedef struct {
  unsigned version;

  size_t headerSize;
  size_t frameSize;
  unsigned frameSizeFieldLength;
  unsigned nbChSetsPerFrame;
  unsigned nbSegmentsInFrameCode;
  unsigned nbSamplesPerSegmentCode;
  unsigned nbBitsSegmentSizeField;
  uint8_t crc16Pres;
  bool scalableLSBs;
  unsigned nbBitsChMaskField;
  unsigned fixedLsbWidth;

  size_t reservedFieldSize;
  unsigned paddingBits;

  uint16_t headerCrc;

  /* Computed parameters : */
  unsigned nbSegmentsInFrame;
  unsigned nbSamplesPerSegment;
} DtsXllCommonHeader;

#define DTS_XLL_MAX_CH_NB DTS_EXT_SS_MAX_CHANNELS_NB

#define DTS_XLL_MAX_DOWMIX_COEFF_NB                                           \
  (                                                                           \
    (DTS_XLL_MAX_CH_NB + 1)                                                   \
    * (DTS_XLL_MAX_CH_NB * (DTS_XLL_MAX_CHSETS_NB - 1))                       \
  )

typedef struct {
  size_t chSetHeaderSize;
  unsigned nbChannels;
  uint32_t residualChType;
  unsigned bitRes;
  unsigned bitWidth;
  uint8_t origSamplFreqIdx;
  uint8_t samplFreqInterplFactrMod;

  uint8_t replSetMemberId;
  struct {
    bool activeReplSet;
  };  /**< Only used if replSetMemberId == true.                             */

  /* Common fields: */
  bool isPrimaryChSet;
  bool downMixCoeffEmbedded;
  bool partOfHierarchy;

  union {
    struct {
      bool downMixPerformed;
      uint8_t downMixType;
      uint16_t downMixCoeff[DTS_XLL_MAX_DOWMIX_COEFF_NB];

      bool chMaskEnabled;
      union {
        uint64_t chMask;
        struct {
          uint16_t radius;
          uint16_t theta;
          uint16_t phi;
        } sphericalChCoords[DTS_XLL_MAX_CH_NB];
      };
    };  /**< Only if bOne2OneMapChannels2Speakers == true.                   */

    struct {
      bool mapCoeffPresent;  /**< Not supported (TODO).                      */
    };  /**< Otherwise (bOne2OneMapChannels2Speakers == false).              */
  };

  unsigned nbFreqBands;

  /* Computed parameters : */
  unsigned samplingFreq;
} DtsXllChannelSetSubHeader;

typedef struct {
  DtsXllCommonHeader comHdr;
  DtsXllChannelSetSubHeader subHdr[DTS_XLL_MAX_CHSETS_NB];

  DcaXllFrameSFPosition originalPosition;
} DtsXllFrameParameters;

#endif