/** \~english
 * \file dts_util.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief DTS audio bitstreams common utilities module.
 */

#ifndef __LIBBLU_MUXER__CODECS__DTS__UTIL_H__
#define __LIBBLU_MUXER__CODECS__DTS__UTIL_H__

#include "../../util.h"
#include "../../esms/scriptCreation.h"
#include "../common/esParsingSettings.h"

#include "dts_error.h"
#include "dts_data.h"
#include "dts_xll_util.h"
#include "dts_frame.h"

typedef struct {
  bool missingRequiredPbrFile;
} DtshdFileHandlerWarningFlags;

typedef struct {
  DtshdFileHeaderChunk header;
  bool headerPresent;

  DtshdFileInfo fileInfo;
  bool fileInfoPresent;

  DtshdCoreSubStrmMeta coreMetadata;
  bool coreMetadataPresent;

  DtshdExtSubStrmMeta extMetadata;
  bool extMetadataPresent;

  DtshdAudioPresPropHeaderMeta audioPresHeaderMetadata;
  bool audioPresHeaderMetadataPresent;

  DtshdAudioPresText audioPresText;
  bool audioPresTextPresent;

  DtshdNavMeta navigationMetadata;
  bool navigationMetadataPresent;

  DtshdStreamData streamData;
  bool streamDataPresent;
  bool inStreamData;

  DtshdTimecode timecode;
  bool timecodePresent;

  DtshdBuildVer buildVer;
  bool buildVerPresent;

  DtshdBlackout blackout;
  bool blackoutPresent;

  DtshdFileHandlerWarningFlags warningFlags;
} DtshdFileHandler, *DtshdFileHandlerPtr;

typedef enum {
  DTS_AUDIO         = STREAM_CODING_TYPE_DTS,
  DTS_HDHR_AUDIO    = STREAM_CODING_TYPE_HDHR,
  DTS_HDMA_AUDIO    = STREAM_CODING_TYPE_HDMA,
  DTS_EXPRESS       = STREAM_CODING_TYPE_DTSE_SEC,
  DTS_AUDIO_DEFAULT = 0xFF
} DtsAudioStreamIds;

typedef enum {
  DTS_SFREQ_8_KHZ     = 0x1,
  DTS_SFREQ_16_KHZ    = 0x2,
  DTS_SFREQ_32_KHZ    = 0x3,
  DTS_SFREQ_11025_HZ  = 0x6,
  DTS_SFREQ_22050_HZ  = 0x7,
  DTS_SFREQ_44100_HZ  = 0x8,
  DTS_SFREQ_12_KHZ    = 0xB,
  DTS_SFREQ_24_KHZ    = 0xC,
  DTS_SFREQ_48_KHZ    = 0xD
} DcaCoreAudioSampFreqCode;

unsigned dtsCoreAudioSampFreqCodeValue(
  const DcaCoreAudioSampFreqCode code
);

/** \~english
 * \brief DTS Core channel assignement code values.
 *
 * Other values means user defined.
 */
typedef enum {
  DTS_AMODE_A                          = 0x0,
  DTS_AMODE_A_B                        = 0x1,
  DTS_AMODE_L_R                        = 0x2,
  DTS_AMODE_L_R_SUMDIF                 = 0x3,
  DTS_AMODE_LT_LR                      = 0x4,
  DTS_AMODE_C_L_R                      = 0x5,
  DTS_AMODE_L_R_S                      = 0x6,
  DTS_AMODE_C_L_R_S                    = 0x7,
  DTS_AMODE_L_R_SL_SR                  = 0x8,
  DTS_AMODE_C_L_R_SL_SR                = 0x9,
  DTS_AMODE_CL_CR_L_R_SL_SR            = 0xA,
  DTS_AMODE_C_L_R_LR_RR_OV             = 0xB,
  DTS_AMODE_CF_LF_LF_RF_LR_RR          = 0xC,
  DTS_AMODE_CL_C_CR_L_R_SL_SR          = 0xD,
  DTS_AMODE_CL_CR_L_R_SL1_SL2_SR1_SR2  = 0xE,
  DTS_AMODE_CL_C_CR_L_R_SL_S_SR        = 0xF
} DcaCoreAudioChannelAssignCode;

/** \~english
 * \brief Return from given #DcaCoreAudioChannelAssignCode value the number
 * of audio channels associated.
 *
 * \param code
 * \return unsigned
 */
unsigned dtsCoreAudioChannelAssignCodeToNbCh(
  const DcaCoreAudioChannelAssignCode code
);

const char * dtsCoreAudioChannelAssignCodeStr(
  const DcaCoreAudioChannelAssignCode code
);

typedef enum {
  DTS_PCMR_16,
  DTS_PCMR_16_ES,
  DTS_PCMR_20,
  DTS_PCMR_20_ES,
  DTS_PCMR_24,
  DTS_PCMR_24_ES
} DcaCoreSourcePcmResCode;

unsigned dtsCoreSourcePcmResCodeValue(
  const DcaCoreSourcePcmResCode code,
  bool * isEs
);

/** \~english
 * \brief DTS Core transmission bitrate code values.
 *
 */
typedef enum {
  DTS_RATE_32    = 0x00,
  DTS_RATE_56    = 0x01,
  DTS_RATE_64    = 0x02,
  DTS_RATE_96    = 0x03,
  DTS_RATE_112   = 0x04,
  DTS_RATE_128   = 0x05,
  DTS_RATE_192   = 0x06,
  DTS_RATE_224   = 0x07,
  DTS_RATE_256   = 0x08,
  DTS_RATE_320   = 0x09,
  DTS_RATE_384   = 0x0A,
  DTS_RATE_448   = 0x0B,
  DTS_RATE_512   = 0x0C,
  DTS_RATE_576   = 0x0D,
  DTS_RATE_640   = 0x0E,
  DTS_RATE_768   = 0x0F,
  DTS_RATE_960   = 0x10,
  DTS_RATE_1024  = 0x11,
  DTS_RATE_1152  = 0x12,
  DTS_RATE_1280  = 0x13,
  DTS_RATE_1344  = 0x14,
  DTS_RATE_1408  = 0x15,
  DTS_RATE_1411  = 0x16,
  DTS_RATE_1472  = 0x17,
  DTS_RATE_1536  = 0x18,
  DTS_RATE_OPEN  = 0x1D
} DcaCoreTransBitRateCodeCat;

/** \~english
 * \brief Return from given #DcaCoreTransBitRateCodeCat value the bitrate value
 * in Kbit/s associated.
 *
 * If given code == #DTS_RATE_OPEN, 1 is returned.
 *
 * \param code
 * \return unsigned
 */
unsigned dtsBitRateCodeValue(
  const DcaCoreTransBitRateCodeCat code
);

typedef enum {
  DTS_EXT_AUDIO_ID_XCH   = 0x0,  /**< Channel extension.                     */
  DTS_EXT_AUDIO_ID_X96   = 0x2,  /**< Sampling frequency extension.          */
  DTS_EXT_AUDIO_ID_XXCH  = 0x6   /**< Channel extension.                     */
} DcaCoreExtendedAudioCodingCode;

bool isValidDtsExtendedAudioCodingCode(
  const DcaCoreExtendedAudioCodingCode code
);

const char * dtsCoreExtendedAudioCodingCodeStr(
  const DcaCoreExtendedAudioCodingCode code
);

typedef enum {
  DTS_CHIST_NO_COPY     = 0x0,
  DTS_CHIST_FIRST_GEN   = 0x1,
  DTS_CHIST_SECOND_GEN  = 0x2,
  DTS_CHIST_FREE_COPY   = 0x3
} DcaCoreCopyrightHistoryCode;

const char * dtsCoreCopyrightHistoryCodeStr(
  const DcaCoreCopyrightHistoryCode code
);

typedef struct {
  bool presenceOfDeprecatedCrc;
  bool usageOfHdcdEncoding;
} DtsDcaCoreSSWarningFlags;

#define INIT_DTS_DCA_CORE_SS_WARNING_FLAGS()                                  \
  (                                                                           \
    (DtsDcaCoreSSWarningFlags) {                                              \
      .presenceOfDeprecatedCrc = false,                                       \
      .usageOfHdcdEncoding = false                                            \
    }                                                                         \
  )

typedef struct {
  DcaCoreSSFrameParameters curFrame;

  unsigned nbFrames;
  DtsDcaCoreSSWarningFlags warningFlags;

  uint64_t frameDts;
} DtsDcaCoreSSContext;

/** \~english
 * \brief Initialize #DtsDcaCoreSSContext fields to default values.
 */
#define INIT_DTS_DCA_CORE_SS_CTX()                                            \
  (                                                                           \
    (DtsDcaCoreSSContext) {                                                   \
      .nbFrames = 0,                                                          \
      .warningFlags = INIT_DTS_DCA_CORE_SS_WARNING_FLAGS(),                   \
      .frameDts = 0                                                           \
    }                                                                         \
  )

typedef enum {
  DCA_EXT_SS_REF_CLOCK_PERIOD_32000 = 0x0,
  DCA_EXT_SS_REF_CLOCK_PERIOD_44100 = 0x1,
  DCA_EXT_SS_REF_CLOCK_PERIOD_48000 = 0x2
} DcaExtReferenceClockCode;

unsigned dtsExtReferenceClockCodeValue(
  const DcaExtReferenceClockCode code
);

typedef struct {
  bool presenceOfUserDefinedBits;
  bool presenceOfMixMetadata;
  bool nonCompleteAudioPresentationAssetType;
  bool nonDirectSpeakersFeedTolerance;
  bool presenceOfStereoDownmix;
  bool absenceOfStereoDownmixForSec;
} DtsDcaExtSSWarningFlags;

#define INIT_DTS_DCA_EXT_SS_WARNING_FLAGS()                                   \
  (                                                                           \
    (DtsDcaExtSSWarningFlags) {                                               \
      .presenceOfUserDefinedBits = false,                                     \
      .presenceOfMixMetadata = false,                                         \
      .nonCompleteAudioPresentationAssetType = false,                         \
      .nonDirectSpeakersFeedTolerance = false,                                \
      .presenceOfStereoDownmix = false,                                       \
      .absenceOfStereoDownmixForSec = false                                   \
    }                                                                         \
  )

typedef struct {
  bool xllExtSS;  /**< DTS XLL */
  bool xbrExtSS;  /**< DTS XBR */
  bool lbrExtSS;  /**< DTS LBR */

  /* Minimal parameters : */
  bool parsedParameters;
  unsigned nbChannels;
  unsigned audioFreq;
  unsigned bitDepth;
} DtsDcaExtSSFrameContent;

#define INIT_DTS_DCA_EXT_SS_FRAME_CONTENT()                                   \
  (                                                                           \
    (DtsDcaExtSSFrameContent) {                                               \
      .xllExtSS = false,                                                      \
      .xbrExtSS = false,                                                      \
      .lbrExtSS = false,                                                      \
      .parsedParameters = false                                               \
    }                                                                         \
  )

typedef struct {
  DcaExtSSFrameParameters * curFrames[DCA_EXT_SS_MAX_NB_INDEXES];
  bool presentIndexes[DCA_EXT_SS_MAX_NB_INDEXES];
  uint8_t currentExtSSIdx;

  DtsDcaExtSSFrameContent content;

  unsigned nbFrames;
  DtsDcaExtSSWarningFlags warningFlags;

  uint64_t frameDts;
} DtsDcaExtSSContext;

/** \~english
 * \brief Initialize #DtsDcaExtSSContext fields to default values.
 */
#define INIT_DTS_DCA_EXT_SS_CTX()                                             \
  (                                                                           \
    (DtsDcaExtSSContext) {                                                    \
      .nbFrames = 0,                                                          \
      .content = INIT_DTS_DCA_EXT_SS_FRAME_CONTENT(),                         \
      .warningFlags = INIT_DTS_DCA_EXT_SS_WARNING_FLAGS(),                    \
      .frameDts = 0                                                           \
    }                                                                         \
  )

typedef enum {
  DCA_EXT_SS_MIX_ADJ_LVL_ONLY_BITSTREAM_META   = 0x0,
  DCA_EXT_SS_MIX_ADJ_LVL_SYS_META_FEATURE_1    = 0x1,
  DCA_EXT_SS_MIX_ADJ_LVL_SYS_META_FEATURE_1_2  = 0x2,
  DCA_EXT_SS_MIX_ADJ_LVL_RESERVED              = 0x3
} DcaExtMixMetadataAdjLevel;

const char * dcaExtMixMetadataAjdLevelStr(
  DcaExtMixMetadataAdjLevel lvl
);

bool isValidDcaExtMixMetadataAdjLevel(
  DcaExtMixMetadataAdjLevel lvl
);

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

unsigned dcaExtChMaskToNbCh(
  const uint16_t mask
);

void dcaExtChMaskStrPrintFun(
  uint16_t mask,
  int (*printFun) (const lbc * format, ...)
);

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
} DcaExtSourceSampleRateCode;

unsigned dtsExtSourceSampleRateCodeValue(
  const DcaExtSourceSampleRateCode code
);

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

const char * dtsExtAssetTypeDescCodeStr(
  const DcaExtAssetTypeDescCode code
);

typedef enum {
  DCA_EXT_SS_MIX_REPLACEMENT         = 0x0,
  DCA_EXT_SS_NOT_APPLICABLE_1        = 0x1,
  DCA_EXT_SS_LT_RT_MATRIX_SURROUND   = 0x2,
  DCA_EXT_SS_LH_RH_HEADPHONE  = 0x3,
  DCA_EXT_SS_NOT_APPLICABLE_2        = 0x4,
} DcaExtRepresentationTypeCode;

const char * dtsExtRepresentationTypeCodeStr(
  const DcaExtRepresentationTypeCode code
);

const char * dtsAudioAssetCodingModeStr(
  const DcaAudioAssetCodingMode mode
);

#define DTS_PERIOD_MAX_SUPPORTED_SYNC_FRAMES 1

typedef enum {
  DCA_XLL_CRC16_ABSENCE                        = 0x0,
  DCA_XLL_CRC16_AT_END_OF_MSB0                 = 0x1,
  DCA_XLL_CRC16_AT_END_OF_MSB0_AND_LSB0        = 0x2,
  DCA_XLL_CRC16_AT_END_OF_MSB0_LSB0_AND_BANDS  = 0x3
} DtsXllCommonHeaderCrc16PresenceCode;

const char * dtsXllCommonHeaderCrc16PresenceCodeStr(
  DtsXllCommonHeaderCrc16PresenceCode code
);

#if 0
typedef struct {
  bool initialized;

  int64_t syncFrameStartOffset;
  bool isExtSubstream; /**< false: Core substream, true: Extension substream. */
  bool skipped;
} DtsCurrentPeriod;
#endif

/** \~english
 * \brief Parsing mode of input DTS stream.
 *
 * Used to determine actions to perform on signal. It is mainly used during if
 * input signal requires a two passes PBR smoothing processing.
 */
typedef enum {
  DTS_PARSMOD_SINGLE_PASS,     /**< Single pass parsing,
    no additional processing required.                                       */
  DTS_PARSMOD_TWO_PASS_FIRST,  /**< Two passes parsing,
    first part used to check and build input stream stats to prepare PBR
    smoothing.                                                               */
  DTS_PARSMOD_TWO_PASS_SECOND  /**< Two passes parsing,
    second part used to build processed ouput stream from stats produced
    during first pass.                                                       */
} DtsContextParsingMode;

#define DTS_PARSMOD_ESMS_FILE_GENERATION(mode)                                \
  (                                                                           \
    (mode) == DTS_PARSMOD_SINGLE_PASS                                         \
    || (mode) == DTS_PARSMOD_TWO_PASS_SECOND                                  \
  )

typedef struct {
  DtsContextParsingMode mode;

  unsigned sourceFileIdx;
  BitstreamReaderPtr file;
  BitstreamWriterPtr esmsScriptOutputFile;

  DtshdFileHandlerPtr dtshdFileHandle;

  DtsDcaCoreSSContext core;
  bool corePresent;

  DtsDcaExtSSContext extSS;
  bool extSSPresent;

  DtsXllFrameContextPtr xllCtx;

  bool isSecondaryStream;
  bool skipExtensionSubstreams;
  unsigned skippedFramesControl;
  bool skipCurrentPeriod; /**< Skip all extension substreams until next core frame is parsed. */

  DtsAUFramePtr curAccessUnit;
} DtsContext, *DtsContextPtr;

#define DTS_CTX_IS_DTSHD_FILE(ctx)                                            \
  (                                                                           \
    NULL != (ctx)->dtshdFileHandle                                            \
  )

#define DTS_CTX_IN_FILE(ctx)                                                  \
  ((ctx)->file)

#define DTS_CTX_OUT_ESMS(ctx)                                                 \
  ((ctx)->esmsScriptOutputFile)

#define DTS_CTX_OUT_ESMS_FILE_IDX_PTR(ctx)                                    \
  (&(ctx)->sourceFileIdx)

#define DTS_CTX_BUILD_ESMS_SCRIPT(ctx)                                        \
  DTS_PARSMOD_ESMS_FILE_GENERATION((ctx)->mode)

#define DTS_CTX_FAST_2ND_PASS(ctx)                                            \
  ((ctx)->mode == DTS_PARSMOD_TWO_PASS_SECOND)

/* Handling functions : */
DtsContextPtr createDtsContext(
  BitstreamReaderPtr inputFile,
  const lbc * outputFilename,
  LibbluESSettingsOptions options
);

void destroyDtsContext(
  DtsContextPtr ctx
);

bool nextPassDtsContext(
  DtsContextPtr ctx
);

int initParsingDtsContext(
  DtsContextPtr ctx
);

typedef enum {
  DTS_FRAME_INIT_ERROR           = -1,
  DTS_FRAME_INIT_CORE_SUBSTREAM  = 0,
  DTS_FRAME_INIT_EXT_SUBSTREAM   = 1
} DtsFrameInitializationRet;

DtsFrameInitializationRet initNextDtsFrame(
  DtsContextPtr ctx
);

int completeDtsFrame(
  DtsContextPtr ctx,
  EsmsFileHeaderPtr dtsInfos
);

int updateDtsEsmsHeader(
  DtsContextPtr ctx,
  EsmsFileHeaderPtr dtsInfos
);

#endif