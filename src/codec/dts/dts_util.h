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
static inline bool canBePerformedPBRSDtshdFileHandler(
  const DtshdFileHandler * hdlr
)
{
  if (!hdlr->DTSHDHDR_count)
    return false;
  if (!hdlr->AUPR_HDR_count)
    return false;

  return
    !hdlr->warningFlags.missingRequiredPbrFile
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

static inline unsigned getPBRSmoothingBufSizeDtshdFileHandler(
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

typedef enum {
  DTS_AUDIO         = STREAM_CODING_TYPE_DTS,
  DTS_HDHR_AUDIO    = STREAM_CODING_TYPE_HDHR,
  DTS_HDMA_AUDIO    = STREAM_CODING_TYPE_HDMA,
  DTS_EXPRESS       = STREAM_CODING_TYPE_DTSE_SEC,
  DTS_AUDIO_DEFAULT = 0xFF
} DtsAudioStreamIds;

typedef struct {
  bool presenceOfDeprecatedCrc;
  bool usageOfHdcdEncoding;
} DtsDcaCoreSSWarningFlags;

typedef struct {
  DcaCoreSSFrameParameters cur_frame;

  unsigned nb_frames;
  DtsDcaCoreSSWarningFlags warning_flags;

  uint64_t pts;
} DtsDcaCoreSSContext;

/** \~english
 * \brief Initialize #DtsDcaCoreSSContext fields to default values.
 */
static inline void initDtsDcaCoreSSContext(
  DtsDcaCoreSSContext * ctx
)
{
  *ctx = (DtsDcaCoreSSContext) {0};
}

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

  uint64_t pts;
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
      .pts = 0                                                           \
    }                                                                         \
  )

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

unsigned buildDcaExtChMaskString(
  char dst[static DCA_CHMASK_STR_BUFSIZE],
  uint16_t Channel_Mask
);

unsigned nbChDcaExtChMaskCode(
  const uint16_t mask
);

// void dcaExtChMaskStrPrintFun(
//   uint16_t mask,
//   int (*printFun) (const lbc * format, ...)
// );

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

  unsigned src_file_idx;
  BitstreamReaderPtr bs;
  BitstreamWriterPtr script_bs;
  EsmsFileHeaderPtr script;
  const lbc * script_fp;

  DtshdFileHandler dtshdFileHandle;
  bool is_dtshd_file;

  DtsDcaCoreSSContext core;
  bool corePresent;

  DtsDcaExtSSContext extSS;
  bool extSSPresent;

  DtsXllFrameContextPtr xllCtx;

  bool isSecondaryStream;
  bool skipExtensionSubstreams;
  unsigned skippedFramesControl;
  bool skipCurrentPeriod; /**< Skip all extension substreams until next core frame is parsed. */

  DtsAUFrame curAccessUnit;
} DtsContext;

#define DTS_CTX_OUT_ESMS_FILE_IDX_PTR(ctx)                                    \
  (&(ctx)->src_file_idx)

#define DTS_CTX_BUILD_ESMS_SCRIPT(ctx)                                        \
  DTS_PARSMOD_ESMS_FILE_GENERATION((ctx)->mode)

static inline bool isFast2nbPassDtsContext(
  const DtsContext * ctx
)
{
  return DTS_PARSMOD_TWO_PASS_SECOND == ctx->mode;
}

static inline bool isDtshdFileDtsContext(
  const DtsContext * ctx
)
{
  return ctx->is_dtshd_file;
}

/* Handling functions : */
int createDtsContext(
  DtsContext * ctx,
  LibbluESParsingSettings * settings
);

int completeDtsContext(
  DtsContext * ctx
);

void cleanDtsContext(
  DtsContext * ctx
);

bool nextPassDtsContext(
  DtsContext * ctx
);

int initParsingDtsContext(
  DtsContext * ctx
);

typedef enum {
  DTS_FRAME_INIT_ERROR           = -1,
  DTS_FRAME_INIT_CORE_SUBSTREAM  = 0,
  DTS_FRAME_INIT_EXT_SUBSTREAM   = 1
} DtsFrameInitializationRet;

DtsFrameInitializationRet initNextDtsFrame(
  DtsContext * ctx
);

int completeDtsFrame(
  DtsContext * ctx
);

#endif