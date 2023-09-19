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
  DtsDcaCoreSSWarningFlags warn_flags;

  uint64_t pts;
} DtsDcaCoreSSContext;

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

  uint8_t src_file_idx;
  BitstreamReaderPtr bs;
  BitstreamWriterPtr script_bs;
  EsmsHandlerPtr script;
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
  bool processed_dtspbr_file;

  DtsAUFrame cur_au;
  bool skip_cur_au; /**< Skip all extension substreams until next core frame is parsed. */

  unsigned nb_audio_frames;
} DtsContext;

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

static inline bool isDtshdFileDtsContext(
  const DtsContext * ctx
)
{
  return ctx->is_dtshd_file;
}

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

static inline bool nextPassDtsContext(
  DtsContext * ctx
)
{
  if (ctx->mode == DTS_PARSMOD_TWO_PASS_FIRST) {
    ctx->mode = DTS_PARSMOD_TWO_PASS_SECOND; // Switch to second pass
    return true;
  }

  return false;
}

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