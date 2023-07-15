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

typedef struct {
  DcaExtSSFrameParameters * curFrames[DCA_EXT_SS_MAX_NB_INDEXES];
  bool presentIndexes[DCA_EXT_SS_MAX_NB_INDEXES];
  uint8_t currentExtSSIdx;

  DtsDcaExtSSFrameContent content;

  unsigned nbFrames;
  DtsDcaExtSSWarningFlags warningFlags;

  uint64_t pts;
} DtsDcaExtSSContext;

static inline void cleanDtsDcaExtSSContext(
  DtsDcaExtSSContext ctx
)
{
  for (unsigned i = 0; i < DCA_EXT_SS_MAX_NB_INDEXES; i++)
    free(ctx.curFrames[i]);
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

typedef struct {
  DtsContextParsingMode mode;

  unsigned src_file_idx;
  BitstreamReaderPtr bs;
  BitstreamWriterPtr script_bs;
  EsmsFileHeaderPtr script;
  const lbc * script_fp;

  DtshdFileHandler dtshd;
  bool is_dtshd_file;

  DtsDcaCoreSSContext core;
  bool core_pres;

  DtsDcaExtSSContext ext_ss;
  bool ext_ss_pres;

  DtsXllFrameContext xll;
  bool xll_present;

  bool is_secondary;
  bool skip_ext;
  unsigned init_skip_delay;

  DtsAUFrame cur_au;
  bool skip_cur_au; /**< Skip all extension substreams until next core frame is parsed. */
} DtsContext;

static inline bool doGenerateScriptDtsContext(
  const DtsContext * ctx
)
{
  return
    DTS_PARSMOD_SINGLE_PASS == ctx->mode
    || DTS_PARSMOD_TWO_PASS_SECOND == ctx->mode
  ;
}

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
int initDtsContext(
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