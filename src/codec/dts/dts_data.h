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
  DCA_SYNCWORD_CORE  = 0x7FFE8001,
  DCA_SYNCEXTSSH     = 0x64582025,
  DCA_SYNCXLL        = 0x41A29547
} DcaSyncword;

/* ### DTS-HD files : ###################################################### */

typedef enum {
  DCA_EXT_SS_REF_CLOCK_PERIOD_32000 = 0x0,
  DCA_EXT_SS_REF_CLOCK_PERIOD_44100 = 0x1,
  DCA_EXT_SS_REF_CLOCK_PERIOD_48000 = 0x2,
  DCA_EXT_SS_REF_CLOCK_PERIOD_RES   = 0x3
} DcaExtRefClockCode;

static inline unsigned getDcaExtReferenceClockValue(
  DcaExtRefClockCode code
)
{
  static const unsigned clock_periods[3] = {
    32000,
    44100,
    48000
  };

  if (code < ARRAY_SIZE(clock_periods))
    return clock_periods[code];
  return 0;
}

static inline float getRefClockPeriodDcaExtRefClockCode(
  DcaExtRefClockCode code
)
{
  unsigned RefClock = getDcaExtReferenceClockValue(code);
  if (!RefClock)
    return -1.f;
  return 1.f / RefClock;
}

typedef enum {
  DTSHD_TCFR_NOT_INDICATED  = 0x0,
  DTSHD_TCFR_23_976         = 0x1,
  DTSHD_TCFR_24             = 0x2,
  DTSHD_TCFR_25             = 0x3,
  DTSHD_TCFR_29_970_DROP    = 0x4,
  DTSHD_TCFR_29_970         = 0x5,
  DTSHD_TCFR_30_DROP        = 0x6,
  DTSHD_TCFR_30             = 0x7,
  DTSHD_TCFR_RESERVED
} DtshdTCFrameRate;

static inline const char * DtshdTCFrameRateStr(
  DtshdTCFrameRate TC_Frame_Rate
)
{
  static const char * strings[] = {
    "Not Indicated",
    "23.976",
    "24",
    "25",
    "29.970 Drop",
    "29.970",
    "30 Drop",
    "30"
  };

  if (TC_Frame_Rate < ARRAY_SIZE(strings))
    return strings[TC_Frame_Rate];
  return "Reserved";
}

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

  DcaExtRefClockCode RefClockCode;
  uint32_t TimeStamp;
  DtshdTCFrameRate TC_Frame_Rate;
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

typedef struct {
  bool missing_required_dtspbr_file;
} DtshdFileHandlerWarningFlags;

typedef struct {
  DtshdFileHeaderChunk DTSHDHDR;
  unsigned DTSHDHDR_count;

  DtshdFileInfo FILEINFO;
  unsigned FILEINFO_count;

  DtshdCoreSubStrmMeta CORESSMD;
  unsigned CORESSMD_count;

  DtshdExtSubStrmMeta EXTSS_MD;
  unsigned EXTSS_MD_count;

  DtshdAudioPresPropHeaderMeta AUPR_HDR;
  unsigned AUPR_HDR_count;

  DtshdAudioPresText AUPRINFO;
  unsigned AUPRINFO_count;

  DtshdNavMeta NAVI_TBL;
  unsigned NAVI_TBL_count;

  DtshdStreamData STRMDATA;
  unsigned STRMDATA_count;
  uint64_t off_STRMDATA;
  unsigned in_STRMDATA;

  DtshdTimecode TIMECODE;
  unsigned TIMECODE_count;

  DtshdBuildVer BUILDVER;
  unsigned BUILDVER_count;

  DtshdBlackout BLACKOUT;
  unsigned BLACKOUT_count;

  DtshdFileHandlerWarningFlags warningFlags;
} DtshdFileHandler;

/** \~english
 * \brief Return true if Peak Bit-Rate Smoothing process can be performed
 * on DTS-HD file extension substreams.
 *
 * \param handler DTS-HD file handler.
 * \return true PBRS process might be performed on at least one extension
 * substream.
 * \return false PBRS process is not required or cannot be performed.
 */
static inline bool shallPBRSPerformedDtshdFileHandler(
  const DtshdFileHandler * hdlr
)
{
  if (!hdlr->DTSHDHDR_count)
    return false;
  if (!hdlr->AUPR_HDR_count)
    return false;

  return
    !hdlr->warningFlags.missing_required_dtspbr_file
    && hdlr->AUPR_HDR.Bitw_Aupres_Metadata & DTSHD_BAM__LOSSLESS_PRESENT
    && !(hdlr->DTSHDHDR.Bitw_Stream_Metadata & DTSHD_BSM__PBRS_PERFORMED)
  ;
}

/** \~english
 * \brief Return the initial skipped delay in number of audio frames from the
 * DTS-HD file.
 *
 * The returned value is the number of access units to skip from bitstream
 * start. One access unit might be composed of up to one Core audio frame and
 * zero, one or more Extension substreams audio frames.
 *
 * \param handle DTS-HD file handle.
 * \return The number of to-be-skipped access units at bitstream start.
 */
static inline unsigned getInitialSkippingDelayDtshdFileHandler(
  const DtshdFileHandler * hdlr
)
{
  const DtshdAudioPresPropHeaderMeta * AUPR_HDR = &hdlr->AUPR_HDR;

  if (!hdlr->AUPR_HDR_count)
    return 0;

  unsigned delay_samples = AUPR_HDR->Codec_Delay_At_Max_Fs;
  unsigned samples_per_frame = AUPR_HDR->Samples_Per_Frame_At_Max_Fs;
  return (delay_samples + (samples_per_frame >> 1)) / samples_per_frame;
}

static inline unsigned getPBRSmoothingBufSizeKiBDtshdFileHandler(
  const DtshdFileHandler * hdlr
)
{
  const DtshdFileHeaderChunk * DTSHDHDR = &hdlr->DTSHDHDR;
  const DtshdExtSubStrmMeta * EXTSS_MD = &hdlr->EXTSS_MD;

  if (!hdlr->DTSHDHDR_count || !hdlr->EXTSS_MD_count)
    return 0;
  if (0 == (DTSHDHDR->Bitw_Stream_Metadata & DTSHD_BSM__IS_VBR))
    return 0;
  return EXTSS_MD->vbr.Pbr_Smooth_Buff_Size_Kb;
}

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
#define DCA_CORE_BSH_MINSIZE  13u

static inline uint32_t getSizeDcaCoreBSHeader(
  const DcaCoreBSHeaderParameters * bsh
)
{
  if (bsh->CPF)
    return DCA_CORE_BSH_MINSIZE + 2u; // Extra HCRC field word.
  return DCA_CORE_BSH_MINSIZE;
}

typedef struct {
  DcaCoreBSHeaderParameters bs_header;
} DcaCoreSSFrameParameters;

/* ### DTS Extension Substream : ########################################### */

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
 * If field size exceed Reserved_size * 8 + ZeroPadForFsize_size bits,
 * the reserved field data isn't saved.
 */
#define DCA_EXT_SS_MAX_SUPP_RES_FIELD_SIZE 16

static inline bool isSavedReservedFieldDcaExtSS(
  unsigned Reserved_size,
  unsigned ZeroPadForFsize_size
)
{
  unsigned size = (Reserved_size << 3) + ZeroPadForFsize_size;
  return 8 <= size && size <= (DCA_EXT_SS_MAX_SUPP_RES_FIELD_SIZE << 3);
}

typedef enum {
  DCA_EXT_SS_SRC_SAMPLE_RATE_8000    = 0x0,
  DCA_EXT_SS_SRC_SAMPLE_RATE_16000   = 0x1,
  DCA_EXT_SS_SRC_SAMPLE_RATE_32000   = 0x2,
  DCA_EXT_SS_SRC_SAMPLE_RATE_64000   = 0x3,
  DCA_EXT_SS_SRC_SAMPLE_RATE_128000  = 0x4,
  DCA_EXT_SS_SRC_SAMPLE_RATE_22050   = 0x5,
  DCA_EXT_SS_SRC_SAMPLE_RATE_44100   = 0x6,
  DCA_EXT_SS_SRC_SAMPLE_RATE_88200   = 0x7,
  DCA_EXT_SS_SRC_SAMPLE_RATE_176400  = 0x8,
  DCA_EXT_SS_SRC_SAMPLE_RATE_352800  = 0x9,
  DCA_EXT_SS_SRC_SAMPLE_RATE_12000   = 0xA,
  DCA_EXT_SS_SRC_SAMPLE_RATE_24000   = 0xB,
  DCA_EXT_SS_SRC_SAMPLE_RATE_48000   = 0xC,
  DCA_EXT_SS_SRC_SAMPLE_RATE_96000   = 0xD,
  DCA_EXT_SS_SRC_SAMPLE_RATE_192000  = 0xE,
  DCA_EXT_SS_SRC_SAMPLE_RATE_384000  = 0xF,
} DcaExtMaxSampleRate;

static inline unsigned getSampleFrequencyDcaExtMaxSampleRate(
  const DcaExtMaxSampleRate code
)
{
  static const unsigned sample_rates[] = {
    8000u,    16000u,  32000u,  64000u,
    128000u,  22050u,  44100u,  88200u,
    176400u, 352800u,  12000u,  24000u,
    48000u,   96000u, 192000u, 384000u
  };

  if (code < ARRAY_SIZE(sample_rates))
    return sample_rates[code];
  return 0;
}

typedef enum {
  DCA_EXT_SS_MIX_ADJ_LVL_ONLY_BITSTREAM_META   = 0x0,
  DCA_EXT_SS_MIX_ADJ_LVL_SYS_META_FEATURE_1    = 0x1,
  DCA_EXT_SS_MIX_ADJ_LVL_SYS_META_FEATURE_1_2  = 0x2,
  DCA_EXT_SS_MIX_ADJ_LVL_RESERVED              = 0x3
} DcaExtMixMetadataAdjLevel;

static inline const char * dcaExtMixMetadataAjdLevelStr(
  DcaExtMixMetadataAdjLevel lvl
)
{
  static const char * strings[] = {
    "Use only bitstream embedded metadata",
    "Allow system to adjust feature 1",
    "Allow system to adjust both feature 1 and feature 2",
    "Reserved value"
  };

  if (lvl < ARRAY_SIZE(strings))
    return strings[lvl];
  return "Reserved value";
}

typedef enum {
  DCA_EXT_SS_ASSET_TYPE_DESC_MUSIC                              = 0x0,
  DCA_EXT_SS_ASSET_TYPE_DESC_EFFECTS                            = 0x1,
  DCA_EXT_SS_ASSET_TYPE_DESC_DIALOG                             = 0x2,
  DCA_EXT_SS_ASSET_TYPE_DESC_COMMENTARY                         = 0x3,
  DCA_EXT_SS_ASSET_TYPE_DESC_VISUALLY_IMPAIRED                  = 0x4,
  DCA_EXT_SS_ASSET_TYPE_DESC_HEARING_IMPAIRED                   = 0x5,
  DCA_EXT_SS_ASSET_TYPE_DESC_ISOLATED_MUSIC_OBJ                 = 0x6,
  DCA_EXT_SS_ASSET_TYPE_DESC_MUSIC_AND_EFFECTS                  = 0x7,
  DCA_EXT_SS_ASSET_TYPE_DESC_DIALOG_AND_COMMENTARY              = 0x8,
  DCA_EXT_SS_ASSET_TYPE_DESC_EFFECTS_AND_COMMENTARY             = 0x9,
  DCA_EXT_SS_ASSET_TYPE_DESC_ISOLATED_MUSIC_OBJ_AND_COMMENTARY  = 0xA,
  DCA_EXT_SS_ASSET_TYPE_DESC_ISOLATED_MUSIC_OBJ_AND_EFFECTS     = 0xB,
  DCA_EXT_SS_ASSET_TYPE_DESC_KARAOKE                            = 0xC,
  DCA_EXT_SS_ASSET_TYPE_DESC_MUSIC_EFFECTS_AND_DIALOG           = 0xD,
  DCA_EXT_SS_ASSET_TYPE_DESC_COMPLETE_PRESENTATION              = 0xE,
  DCA_EXT_SS_ASSET_TYPE_DESC_RESERVED                           = 0xF
} DcaExtAssetTypeDescCode;

static inline const char * dtsExtAssetTypeDescCodeStr(
  const DcaExtAssetTypeDescCode code
)
{
  static const char * typStr[] = {
    "Music",
    "Effects",
    "Dialog",
    "Commentary",
    "Visually impaired",
    "Hearing impaired",
    "Isolated music object",
    "Music and Effects",
    "Dialog and Commentary",
    "Effects and Commentary",
    "Isolated Music Object and Commentary",
    "Isolated Music Object and Effects",
    "Karaoke",
    "Music, Effects and Dialog",
    "Complete Audio Presentation",
    "Reserved value"
  };

  if (code < ARRAY_SIZE(typStr))
    return typStr[code];
  return "unk";
}

#define DTS_PERIOD_MAX_SUPPORTED_SYNC_FRAMES 1

typedef enum {
  DCA_EXT_SS_CH_MASK_C        = 0x0001,
  DCA_EXT_SS_CH_MASK_L_R      = 0x0002,
  DCA_EXT_SS_CH_MASK_LS_RS    = 0x0004,
  DCA_EXT_SS_CH_MASK_LFE      = 0x0008,
  DCA_EXT_SS_CH_MASK_CS       = 0x0010,
  DCA_EXT_SS_CH_MASK_LH_RH    = 0x0020,
  DCA_EXT_SS_CH_MASK_LSR_RSR  = 0x0040,
  DCA_EXT_SS_CH_MASK_CH       = 0x0080,
  DCA_EXT_SS_CH_MASK_OH       = 0x0100,
  DCA_EXT_SS_CH_MASK_LC_RC    = 0x0200,
  DCA_EXT_SS_CH_MASK_LW_RW    = 0x0400,
  DCA_EXT_SS_CH_MASK_LSS_RSS  = 0x0800,
  DCA_EXT_SS_CH_MASK_LFE2     = 0x1000,
  DCA_EXT_SS_CH_MASK_LHS_RHS  = 0x2000,
  DCA_EXT_SS_CH_MASK_CHR      = 0x4000,
  DCA_EXT_SS_CH_MASK_LHR_RHR  = 0x8000
} DcaExtChMaskCode;

#define DCA_CHMASK_STR_BUFSIZE  240

static inline unsigned buildDcaExtChMaskString(
  char dst[static DCA_CHMASK_STR_BUFSIZE],
  uint16_t Channel_Mask
)
{
  static const char * ch_config_str[16] = {
    "C",
    "L, R",
    "Ls, Rs",
    "LFE",
    "Cs",
    "Lh, Rh",
    "Lsr, Rsr",
    "Ch",
    "Oh",
    "Lc, Rc",
    "Lw, Rw",
    "Lss, Rss",
    "LFE2",
    "Lhs, Rhs",
    "Chr",
    "Lhr, Rhr"
  };
  static const unsigned nb_ch_config[16] = {
    1, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2, 2, 1, 2, 1, 2
  };

  unsigned nb_channels = 0;
  char * str_ptr = dst;

  const char * sep = "";
  for (unsigned i = 0; i < 16; i++) {
    if (Channel_Mask & (1 << i)) {
      lb_str_cat(&str_ptr, sep);
      lb_str_cat(&str_ptr, ch_config_str[i]);
      nb_channels += nb_ch_config[i];
      sep = ", ";
    }
  }

  return nb_channels;
}

static inline unsigned nbChDcaExtChMaskCode(
  const uint16_t mask
)
{
  unsigned nbCh, i;

  static const uint16_t chConfigs[16][2] = {
    {DCA_EXT_SS_CH_MASK_C,       1},
    {DCA_EXT_SS_CH_MASK_L_R,     2},
    {DCA_EXT_SS_CH_MASK_LS_RS,   2},
    {DCA_EXT_SS_CH_MASK_LFE,     1},
    {DCA_EXT_SS_CH_MASK_CS,      1},
    {DCA_EXT_SS_CH_MASK_LH_RH,   2},
    {DCA_EXT_SS_CH_MASK_LSR_RSR, 2},
    {DCA_EXT_SS_CH_MASK_CH,      1},
    {DCA_EXT_SS_CH_MASK_OH,      1},
    {DCA_EXT_SS_CH_MASK_LC_RC,   2},
    {DCA_EXT_SS_CH_MASK_LW_RW,   2},
    {DCA_EXT_SS_CH_MASK_LSS_RSS, 2},
    {DCA_EXT_SS_CH_MASK_LFE2,    1},
    {DCA_EXT_SS_CH_MASK_LHS_RHS, 2},
    {DCA_EXT_SS_CH_MASK_CHR,     1},
    {DCA_EXT_SS_CH_MASK_LHR_RHR, 2}
  };

  for (nbCh = i = 0; i < ARRAY_SIZE(chConfigs); i++)
    nbCh += (mask & chConfigs[i][0]) ? chConfigs[i][1] : 0;

  return nbCh;
}

typedef enum {
  DCA_EXT_SS_MIX_REPLACEMENT         = 0x0,
  DCA_EXT_SS_NOT_APPLICABLE_1        = 0x1,
  DCA_EXT_SS_LT_RT_MATRIX_SURROUND   = 0x2,
  DCA_EXT_SS_LH_RH_HEADPHONE         = 0x3,
  DCA_EXT_SS_NOT_APPLICABLE_2        = 0x4,
} DcaExtRepresentationTypeCode;

static inline const char * dtsExtRepresentationTypeCodeStr(
  const DcaExtRepresentationTypeCode code
)
{
  static const char * typStr[] = {
    "Audio Asset for Mixing/Replacement purpose",
    "Not applicable",
    "Lt/Rt Encoded for Matrix Surround",
    "Lh/Rh Headphone playback",
    "Not Applicable"
  };

  if (code < ARRAY_SIZE(typStr))
    return typStr[code];
  return "Reserved value";
}

typedef struct {
  bool bAssetTypeDescrPresent;
  uint8_t nuAssetTypeDescriptor;

  bool bLanguageDescrPresent;
  uint8_t LanguageDescriptor[DTS_EXT_SS_LANGUAGE_DESC_SIZE+1];

  bool bInfoTextPresent;
  uint16_t nuInfoTextByteSize;
  uint8_t InfoTextString[DTS_EXT_SS_MAX_STRING_TEXT_MAX_LEN+1];

  uint8_t nuBitResolution;
  uint8_t nuMaxSampleRate;
  uint16_t nuTotalNumChs;

  bool bOne2OneMapChannels2Speakers;
  bool bEmbeddedStereoFlag;
  bool bEmbeddedSixChFlag;
  uint8_t nuRepresentationType; /* Only if bOne2OneMapChannels2Speakers == 0b0 */

  bool bSpkrMaskEnabled;
  uint16_t nuSpkrActivityMask;

  unsigned nuNumSpkrRemapSets;
  uint16_t nuStndrSpkrLayoutMask[DTS_EXT_SS_MAX_NB_REMAP_SETS];
  unsigned nuNumSpeakers[DTS_EXT_SS_MAX_NB_REMAP_SETS];

  unsigned nuNumDecCh4Remap[DTS_EXT_SS_MAX_NB_REMAP_SETS];
  /* decodedChannelsLinkedToSetSpeakerLayoutMask : */
  uint16_t nuRemapDecChMask[DTS_EXT_SS_MAX_NB_REMAP_SETS][DTS_EXT_SS_MAX_CHANNELS_NB];
  uint8_t nCoeff[DTS_EXT_SS_MAX_NB_REMAP_SETS][DTS_EXT_SS_MAX_CHANNELS_NB];
  uint8_t nuSpkrRemapCodes[DTS_EXT_SS_MAX_NB_REMAP_SETS][DTS_EXT_SS_MAX_CHANNELS_NB][DTS_EXT_SS_MAX_SPEAKERS_SETS_NB];
} DcaAudioAssetDescSFParameters;

/* Value used if 'bDRCMetadatRev2Present' is false: */
#define DEFAULT_DRC_VERSION_VALUE -1

typedef struct {
  bool bDRCCoefPresent;
  uint8_t nuDRCCode;
  uint8_t nuDRC2ChDmixCode;

  bool bDialNormPresent;  /**< bDialNormPresent                               */
  uint8_t nuDialNormCode;  /**< nuDialNormCode                                 */

  bool bMixMetadataPresent;  /**< bMixMetadataPresent                         */
#if !DCA_EXT_SS_DISABLE_MIX_META_SUPPORT
  bool bExternalMixFlag;            /**< bExternalMixFlag                    */
  uint8_t nuPostMixGainAdjCode;        /**< nuPostMixGainAdjCode                */
  uint8_t nuControlMixerDRC;    /**< nuControlMixerDRC                   */
  union {
    uint8_t nuLimit4EmbeddedDRC;       /**< nuLimit4EmbeddedDRC               */
    uint8_t nuCustomDRCCode;  /**< nuCustomDRCCode                   */
  };

  bool bEnblPerChMainAudioScale;     /**< bEnblPerChMainAudioScale            */
  uint8_t nuMainAudioScaleCode[
    DTS_EXT_SS_MAX_OUT_MIX_CONFIGS_NB
  ][DTS_EXT_SS_MAX_CHANNELS_NB];             /**< nuMainAudioScaleCode, if
    'perMainAudioChSepScal == 0b0', the scaling code shared between all
    channels of one mix config is stored as for the channel of index 0,
    'nuMainAudioScaleCode[configId][0]'.                                      */

  unsigned nEmDM;
  unsigned nDecCh[DTS_EXT_SS_MAX_DOWNMIXES_NB];
  uint16_t nuMixMapMask[DTS_EXT_SS_MAX_OUT_MIX_CONFIGS_NB][DTS_EXT_SS_MAX_DOWNMIXES_NB][DTS_EXT_SS_MAX_CHANNELS_NB];
  unsigned nuNumMixCoefs[DTS_EXT_SS_MAX_OUT_MIX_CONFIGS_NB][DTS_EXT_SS_MAX_DOWNMIXES_NB][DTS_EXT_SS_MAX_CHANNELS_NB];
  uint8_t nuMixCoeffs[DTS_EXT_SS_MAX_OUT_MIX_CONFIGS_NB][DTS_EXT_SS_MAX_DOWNMIXES_NB][DTS_EXT_SS_MAX_DOWNMIXES_NB][DTS_EXT_SS_MAX_SPEAKERS_SETS_NB];
#endif
} DcaAudioAssetDescDMParameters;

typedef enum {
  DCA_EXT_SS_CODING_MODE_DTS_HD_COMPONENTS             = 0x0,
  DCA_EXT_SS_CODING_MODE_DTS_HD_LOSSLESS_WITHOUT_CORE  = 0x1,
  DCA_EXT_SS_CODING_MODE_DTS_HD_LOW_BITRATE            = 0x2,
  DCA_EXT_SS_CODING_MODE_AUXILIARY_CODING              = 0x3
} DcaAudioAssetCodingMode;

static inline const char * dtsAudioAssetCodingModeStr(
  const DcaAudioAssetCodingMode mode
)
{
  static const char * modStr[] = {
    "DTS-HD Components Coding Mode",
    "DTS-HD Lossless (without Core) Coding Mode",
    "DTS-HD Low bit-rate (without Core) Coding Mode",
    "Auxilary Coding Mode"
  };

  if (mode < ARRAY_SIZE(modStr))
    return modStr[mode];
  return "unk";
}

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
  uint16_t nuExSSCoreFsize;
  bool bExSSCoreSyncPresent;
  uint8_t nuExSSCoreSyncDistInFrames_code;
  uint8_t nuExSSCoreSyncDistInFrames;
} DcaAudioAssetExSSCoreParameters;

typedef struct {
  uint16_t nuExSSXBRFsize;
} DcaAudioAssetExSSXBRParameters;

typedef struct {
  uint16_t nuExSSXXCHFsize;
} DcaAudioAssetExSSXXCHParameters;

typedef struct {
  uint16_t nuExSSX96Fsize;
} DcaAudioAssetExSSX96Parameters;

typedef struct {
  uint16_t nuExSSLBRFsize;
  bool bExSSLBRSyncPresent;
  uint8_t nuExSSLBRSyncDistInFrames_code;
  uint8_t nuExSSLBRSyncDistInFrames;
} DcaAudioAssetExSSLBRParameters;

typedef struct {
  uint32_t nuExSSXLLFsize;
  bool bExSSXLLSyncPresent;
  uint8_t nuPeakBRCntrlBuffSzkB;
  uint8_t nuBitsInitDecDly;
  uint32_t nuInitLLDecDlyFrames;
  uint32_t nuExSSXLLSyncOffset;

  uint8_t nuDTSHDStreamID;
} DcaAudioAssetExSSXllParameters;

typedef struct {
  DcaAudioAssetExSSCoreParameters ExSSCore;
  DcaAudioAssetExSSXBRParameters ExSSXBR;
  DcaAudioAssetExSSXXCHParameters ExSSXXCH;
  DcaAudioAssetExSSX96Parameters ExSSX96;
  DcaAudioAssetExSSLBRParameters ExSSLBR;
  DcaAudioAssetExSSXllParameters ExSSXLL;
  uint16_t res_ext_1_data;
  uint16_t res_ext_2_data;
} DcaAudioAssetDescDecNDCodingComponents;

typedef struct {
  uint16_t nuExSSAuxFsize;
  uint8_t nuAuxCodecID;
  bool bExSSAuxSyncPresent;
  uint8_t nuExSSAuxSyncDistInFrames_code;
  uint8_t nuExSSAuxSyncDistInFrames;
} DcaAudioAssetDescDecNDAuxiliaryCoding;

typedef struct {
  DcaAudioAssetCodingMode nuCodingMode;
  uint16_t nuCoreExtensionMask;
  union {
    DcaAudioAssetDescDecNDCodingComponents coding_components;
    DcaAudioAssetDescDecNDAuxiliaryCoding auxilary_coding;
  };

  bool mix_md_fields_pres;
  bool bOnetoOneMixingFlag;

  bool bEnblPerChMainAudioScale;
  uint8_t nuMainAudioScaleCode[DTS_EXT_SS_MAX_OUT_MIX_CONFIGS_NB][DTS_EXT_SS_MAX_CHANNELS_NB];

  bool bDecodeAssetInSecondaryDecoder;

  bool extra_data_pres;
  bool bDRCMetadataRev2Present;
  uint8_t DRCversion_Rev2;
} DcaAudioAssetDescDecNDParameters;

typedef struct {
  uint16_t nuAssetDescriptFsize;
  uint8_t nuAssetIndex;

  DcaAudioAssetDescSFParameters static_fields;
  DcaAudioAssetDescDMParameters dyn_md;
  DcaAudioAssetDescDecNDParameters dec_nav_data;

  unsigned Reserved_size;
  unsigned ZeroPadForFsize_size;
  uint8_t Reserved[DCA_EXT_SS_MAX_SUPP_RES_FIELD_SIZE];
} DcaAudioAssetDescParameters;

typedef struct {
  uint8_t nuMixMetadataAdjLevel;
  uint8_t nuNumMixOutConfigs;

  uint16_t nuMixOutChMask[DTS_EXT_SS_MAX_OUT_MIX_CONFIGS_NB];
  unsigned nNumMixOutCh[DTS_EXT_SS_MAX_OUT_MIX_CONFIGS_NB];
} DcaExtSSHeaderMixMetadataParameters;

typedef struct {
  DcaExtRefClockCode nuRefClockCode;

  uint8_t nuExSSFrameDurationCode_code;
  uint32_t nuExSSFrameDurationCode;

  bool bTimeStampFlag;
  uint32_t nuTimeStamp;
  uint8_t nLSB;

  uint8_t nuNumAudioPresnt;
  uint8_t nuNumAssets;

  uint8_t nuActiveExSSMask[DTS_EXT_SS_MAX_AUDIO_PRES_NB];
  uint8_t nuActiveAssetMask[DTS_EXT_SS_MAX_AUDIO_PRES_NB][DTS_EXT_SS_MAX_EXT_SUBSTREAMS_NB];

  bool bMixMetadataEnbl;
  DcaExtSSHeaderMixMetadataParameters mixMetadata;

  /* Computed parameters */
  // unsigned referenceClockFreq;
  // float frameDuration;
  // uint64_t timeStamp;
} DcaExtSSHeaderSFParameters;

static inline float getExSSFrameDurationDcaExtRefClockCode(
  const DcaExtSSHeaderSFParameters * sf
)
{
  unsigned RefClockPeriod_den = getDcaExtReferenceClockValue(
    sf->nuRefClockCode
  );
  assert(0 != RefClockPeriod_den);

  return (1.f * sf->nuExSSFrameDurationCode) / RefClockPeriod_den;
}

#define DCA_EXT_SS_CRC_LENGTH 16
#define DCA_EXT_SS_CRC_POLY 0x11021
#define DCA_EXT_SS_CRC_MODULO 0x10000
#define DCA_EXT_SS_CRC_INITIAL_V 0xFFFF

#define DCA_EXT_SS_CRC_PARAM                                                  \
  (CrcParam) {.table = ccitt_crc_16_table, .length = 16}

#define DCA_EXT_SS_MAX_NB_INDEXES 4
#define DCA_EXT_SS_MAX_NB_AUDIO_ASSETS 8

typedef struct {
  uint8_t UserDefinedBits;
  uint8_t nExtSSIndex;

  bool bHeaderSizeType;

  int64_t nuExtSSHeaderSize;
  uint32_t nuExtSSFsize;

  bool bStaticFieldsPresent; /* Mandatory in most cases */
  DcaExtSSHeaderSFParameters static_fields;

  uint32_t audioAssetsLengths[DCA_EXT_SS_MAX_NB_AUDIO_ASSETS];
  DcaAudioAssetDescParameters audioAssets[DCA_EXT_SS_MAX_NB_AUDIO_ASSETS];

  bool bBcCorePresent[DCA_EXT_SS_MAX_NB_AUDIO_ASSETS];
  uint8_t nuBcCoreExtSSIndex[DCA_EXT_SS_MAX_NB_AUDIO_ASSETS];
  uint8_t nuBcCoreAssetIndex[DCA_EXT_SS_MAX_NB_AUDIO_ASSETS];

  unsigned Reserved_size;  /**< Reserved */
  unsigned ZeroPadForFsize_size;
  uint8_t Reserved[DCA_EXT_SS_MAX_SUPP_RES_FIELD_SIZE];  /**< Reserved field
    data content array, only in use if field size does not exceed its size.
    See #DCA_EXT_SS_MAX_SUPP_RES_FIELD_SIZE.                                 */

  uint16_t nCRC16ExtSSHeader;
} DcaExtSSHeaderParameters;

typedef struct {
  DcaExtSSHeaderParameters header;
} DcaExtSSFrameParameters;

#define DTS_XLL_MAX_SUPPORTED_OFILE_POS 8

typedef struct {
  int64_t offset;
  uint32_t size;
} DcaXllFrameSFPositionIndex;

/** \~english
 * \brief DTS XLL frame Source File position.
 *
 */
typedef struct {
  DcaXllFrameSFPositionIndex sourceOffsets[
    DTS_XLL_MAX_SUPPORTED_OFILE_POS
  ];
  unsigned nbSourceOffsets;
} DcaXllFrameSFPosition;

static inline int addDcaXllFrameSFPosition(
  DcaXllFrameSFPosition * dst,
  int64_t offset,
  uint32_t size
)
{
  if (DTS_XLL_MAX_SUPPORTED_OFILE_POS <= dst->nbSourceOffsets)
    return -1;

  dst->sourceOffsets[dst->nbSourceOffsets++] = (DcaXllFrameSFPositionIndex) {
    .offset = offset,
    .size = size
  };
  return 0;
}

typedef struct {
  unsigned number;     /**< Used for stats, number of the PBR frame in
    bitstream.                                                               */

  DcaXllFrameSFPosition pos;
  uint32_t size;              /**< Frame size in bytes.                      */

  unsigned pbrDelay;   /**< Frame decoding delay according to audio asset
    parameters. If this value is greater than zero, data is accumulated in
    PBR buffer prior to decoding.                                            */
} DcaXllPbrFrame, *DcaXllPbrFramePtr;

/** \~english
 * \brief DTS XLL PBR smoothing buffer maximum size in bytes.\n
 *
 * Value is equal to 240 kBytes in binary representation (245 760 bytes).
 */
#define DTS_XLL_MAX_PBR_BUFFER_SIZE (240ul << 10)

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

typedef enum {
  DCA_XLL_CRC16_ABSENCE                        = 0x0,
  DCA_XLL_CRC16_AT_END_OF_MSB0                 = 0x1,
  DCA_XLL_CRC16_AT_END_OF_MSB0_AND_LSB0        = 0x2,
  DCA_XLL_CRC16_AT_END_OF_MSB0_LSB0_AND_BANDS  = 0x3
} DtsXllCommonHeaderCrc16PresenceCode;

static inline const char * dtsXllCommonHeaderCrc16PresenceCodeStr(
  DtsXllCommonHeaderCrc16PresenceCode code
)
{
  static const char * codeStr[] = {
    "No CRC16 checksum within frequency bands",
    "CRC16 checksum present at the end of frequency band 0's MSB",
    "CRC16 checksums present at the end of frequency band 0's MSB and LSB",
    "CRC checksums present at the end of frequency band 0's MSB and LSB and "
    "other frequency bands where they exists"
  };

  if (code < ARRAY_SIZE(codeStr))
    return codeStr[code];
  return "unk";
}

typedef struct {
  uint8_t nVersion;

  uint8_t nHeaderSize;
  uint8_t nBits4FrameFsize;
  uint32_t nLLFrameSize;

  uint8_t nNumChSetsInFrame;
  uint8_t nSegmentsInFrame_code;
  uint16_t nSegmentsInFrame;
  uint8_t nSmplInSeg_code;
  uint16_t nSmplInSeg;
  uint8_t nBits4SSize;
  uint8_t nBandDataCRCEn;
  bool bScalableLSBs;
  uint8_t nBits4ChMask;
  uint8_t nuFixedLSBWidth;

  uint32_t Reserved_size;

  uint16_t nCRC16Header;

  /* Computed parameters : */
  // unsigned nbSegmentsInFrame;
  // unsigned nbSamplesPerSegment;
} DtsXllCommonHeader;

#define DTS_XLL_MIN_HEADER_SIZE  90u

#define DTS_XLL_MAX_CH_NB  DTS_EXT_SS_MAX_CHANNELS_NB

#define DTS_XLL_MAX_DOWMIX_COEFF_NB                                           \
  ((DTS_XLL_MAX_CH_NB+1) * (DTS_XLL_MAX_CH_NB * (DTS_XLL_MAX_CHSETS_NB-1)))

typedef struct {
  uint16_t nChSetHeaderSize;
  uint8_t nChSetLLChannel;
  uint16_t nResidualChEncode;
  uint8_t nBitResolution;
  uint8_t nBitWidth;
  uint8_t sFreqIndex;

#if 0
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
#endif

  /* Computed parameters : */
  unsigned samplingFreq;
} DtsXllChannelSetSubHeader;

typedef struct {
  DtsXllCommonHeader comHdr;
  DtsXllChannelSetSubHeader subHdr[DTS_XLL_MAX_CHSETS_NB];

  DcaXllFrameSFPosition originalPosition;
} DtsXllFrameParameters;

#endif