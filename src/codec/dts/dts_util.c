#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include "dts_util.h"
#include "dts_dtshd_file.h"
#include "dts_xll.h"

void dcaExtChMaskStrPrintFun(
  uint16_t mask,
  int (*printFun) (const lbc *format, ...)
)
{
  unsigned i;
  char *sep;

  static const uint16_t chConfigsMasks[16] = {
    DCA_EXT_SS_CH_MASK_C,
    DCA_EXT_SS_CH_MASK_L_R,
    DCA_EXT_SS_CH_MASK_LS_RS,
    DCA_EXT_SS_CH_MASK_LFE,
    DCA_EXT_SS_CH_MASK_CS,
    DCA_EXT_SS_CH_MASK_LH_RH,
    DCA_EXT_SS_CH_MASK_LSR_RSR,
    DCA_EXT_SS_CH_MASK_CH,
    DCA_EXT_SS_CH_MASK_OH,
    DCA_EXT_SS_CH_MASK_LC_RC,
    DCA_EXT_SS_CH_MASK_LW_RW,
    DCA_EXT_SS_CH_MASK_LSS_RSS,
    DCA_EXT_SS_CH_MASK_LFE2,
    DCA_EXT_SS_CH_MASK_LHS_RHS,
    DCA_EXT_SS_CH_MASK_CHR,
    DCA_EXT_SS_CH_MASK_LHR_RHR
  };

  static const char *chConfigsNames[16] = {
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

  sep = "";
  for (i = 0; i < ARRAY_SIZE(chConfigsMasks); i++) {
    if (mask & chConfigsMasks[i]) {
      printFun(lbc_str("%s %s"), sep, chConfigsNames[i]);
      sep = ",";
    }
  }
}

int initDtsContext(
  DtsContext *ctx,
  LibbluESParsingSettings *settings
)
{
  const LibbluESSettingsOptions options = settings->options;
  BitstreamReaderPtr es_bs = NULL;
  BitstreamWriterPtr script_bs = NULL;
  EsmsHandlerPtr script = NULL;
  uint8_t src_file_idx;
  bool is_dtshd_file;

  /* ES input */
  es_bs = createBitstreamReaderDefBuf(settings->esFilepath);
  if (NULL == es_bs)
    LIBBLU_DTS_ERROR_FRETURN("Unable to open input ES.\n");

  /* Test if DTS-HD File from DTS-HDMA Suite */
  is_dtshd_file = isNextQWORDDtshdHdr(es_bs);

  /* Script output */
  script_bs = createBitstreamWriterDefBuf(settings->scriptFilepath);
  if (NULL == script_bs)
    LIBBLU_DTS_ERROR_FRETURN("Unable to create output script file.\n");

  /* Prepare ESMS output file */
  script = createEsmsHandler(ES_AUDIO, options, FMT_SPEC_INFOS_NONE);
  if (NULL == script)
    LIBBLU_DTS_ERROR_FRETURN("Unable to create output script handler.\n");

  if (appendSourceFileEsmsHandler(script, settings->esFilepath, &src_file_idx) < 0)
    LIBBLU_DTS_ERROR_FRETURN("Unable to add source file to script handler.\n");

  /* Write script header */
  if (writeEsmsHeader(script_bs) < 0)
    return -1;

  /* Save in context */
  *ctx = (DtsContext) {
#if defined(DTS_XLL_FORCE_REBUILD_SEIING) && DTS_XLL_FORCE_REBUILD_SEIING
    .mode = DTS_PARSMOD_TWO_PASS_FIRST,
#else
    .mode = DTS_PARSMOD_SINGLE_PASS, // Single pass by default
#endif

    .src_file_idx = src_file_idx,
    .bs = es_bs,
    .script_bs = script_bs,
    .script = script,
    .script_fp = settings->scriptFilepath,

    .is_dtshd_file = is_dtshd_file,
    .is_secondary = options.secondary,
    .skip_ext = options.extract_core
  };

  return 0;

free_return:
  closeBitstreamWriter(script_bs);
  destroyEsmsHandler(script);

  return -1;
}

int _setScriptProperties(
  DtsContext *ctx
)
{
  EsmsHandlerPtr script = ctx->script;
  LibbluStreamCodingType coding_type;

  if (ctx->ext_ss_pres && ctx->ext_ss.content.xllExtSS)
    coding_type = STREAM_CODING_TYPE_HDMA, script->bitrate = 24500000;
  else if (ctx->ext_ss_pres && ctx->ext_ss.content.xbrExtSS)
    coding_type = STREAM_CODING_TYPE_HDHR, script->bitrate = 24500000;
  else if (ctx->ext_ss_pres && ctx->ext_ss.content.lbrExtSS)
    coding_type = STREAM_CODING_TYPE_DTSE_SEC, script->bitrate = 256000;
  else if (ctx->core_pres)
    coding_type = STREAM_CODING_TYPE_DTS, script->bitrate = 2000000;
  else
    LIBBLU_DTS_ERROR_RETURN(
      "Unexpected empty input stream.\n"
    );

  script->prop.coding_type = coding_type;

  if (ctx->core_pres) {
    const DcaCoreBSHeaderParameters *bsh = &ctx->core.cur_frame.bs_header;
    script->PTS_final = getPTSDtsDcaCoreSSContext(&ctx->core);

    switch (bsh->AMODE) {
    case DCA_AMODE_A:
      /* Mono */
      script->prop.audio_format = 0x01;
      break;

    case DCA_AMODE_A_B:
      /* Dual-Mono */
      script->prop.audio_format = 0x02;
      break;

    case DCA_AMODE_L_R:
    case DCA_AMODE_L_R_SUMDIF:
    case DCA_AMODE_LT_LR:
      /* Stereo */
      script->prop.audio_format = 0x03;
      break;

    default:
      /* Multi-channel */
      script->prop.audio_format = 0x06;
    }

    switch (getDcaCoreAudioSampFreqCode(bsh->SFREQ)) {
    case 48000: /* 48 kHz */
      script->prop.sample_rate = SAMPLE_RATE_CODE_48000; break;
    case 96000: /* 96 kHz */
      script->prop.sample_rate = SAMPLE_RATE_CODE_96000; break;
    default: /* 192 kHz */
      script->prop.sample_rate = SAMPLE_RATE_CODE_192000;
    }

    script->prop.bit_depth = (getDcaCoreSourcePcmResCode(bsh->PCMR) - 12) >> 2;
  }
  else {
    assert(ctx->ext_ss_pres);

    if (!ctx->ext_ss.content.parsedParameters)
      LIBBLU_DTS_ERROR_RETURN(
        "Missing mandatory static fields in Extension Substream, "
        "unable to define audio properties.\n"
      );

    script->PTS_final = getPTSDtsDcaExtSSContext(&ctx->ext_ss);

    if (ctx->ext_ss.content.nbChannels <= 1)
      script->prop.audio_format = 0x01; /* Mono */
    else if (ctx->ext_ss.content.nbChannels == 2)
      script->prop.audio_format = 0x03; /* Stereo */
    else
      script->prop.audio_format = 0x06; /* Multi-channel */

    switch (ctx->ext_ss.content.audioFreq) {
    case 48000: /* 48 kHz */
      script->prop.sample_rate = SAMPLE_RATE_CODE_48000; break;
    case 96000: /* 96 kHz */
      script->prop.sample_rate = SAMPLE_RATE_CODE_96000; break;
    default: /* 192 kHz */
      script->prop.sample_rate = SAMPLE_RATE_CODE_192000;
    }

    script->prop.bit_depth = (ctx->ext_ss.content.bitDepth - 12) >> 2;
  }

  return 0;
}

static inline unsigned _getNbChannels(
  const DtsContext *ctx
)
{
  if (ctx->core_pres) {
    const DcaCoreBSHeaderParameters *bsh = &ctx->core.cur_frame.bs_header;
    return getNbChDcaCoreAudioChannelAssignCode(bsh->AMODE);
  }
  return 0; // TODO: Suppport secondary audio.
}

static void _printStreamInfos(
  const DtsContext *ctx
)
{
  /* Display infos : */
  lbc_printf(
    lbc_str("== Stream Infos =======================================================================\n")
  );
  lbc_printf(
    lbc_str("Codec: %s, %s (%u channels), Sample rate: %u Hz, Bits per sample: %ubits.\n"),
    LibbluStreamCodingTypeStr(ctx->script->prop.coding_type),
    AudioFormatCodeStr(ctx->script->prop.audio_format),
    _getNbChannels(ctx),
    valueSampleRateCode(ctx->script->prop.sample_rate),
    valueBitDepthCode(ctx->script->prop.bit_depth)
  );
  printStreamDurationEsmsHandler(ctx->script);
  lbc_printf(
    lbc_str("=======================================================================================\n")
  );
}

int completeDtsContext(
  DtsContext *ctx
)
{

  if (_setScriptProperties(ctx) < 0)
    return -1;
  _printStreamInfos(ctx);

  if (completePesCuttingScriptEsmsHandler(ctx->script_bs, ctx->script) < 0)
    return -1;
  closeBitstreamWriter(ctx->script_bs);
  ctx->script_bs = NULL;

  if (updateEsmsFile(ctx->script_fp, ctx->script) < 0)
    return -1;

  cleanDtsContext(ctx);
  return 0;
}

void cleanDtsContext(
  DtsContext *ctx
)
{
  if (NULL == ctx)
    return;

  closeBitstreamReader(ctx->bs);
  closeBitstreamWriter(ctx->script_bs);
  destroyEsmsHandler(ctx->script);
  cleanDtshdFileHandler(ctx->dtshd);
  cleanDtsDcaExtSSContext(ctx->ext_ss);
  cleanDtsXllFrameContext(ctx->xll);
  cleanDtsAUFrame(ctx->cur_au);
}

int initParsingDtsContext(
  DtsContext *ctx
)
{
  cleanDtshdFileHandler(ctx->dtshd);
  cleanDtsDcaExtSSContext(ctx->ext_ss);

  memset(&ctx->dtshd, 0, sizeof(DtshdFileHandler));
  memset(&ctx->core, 0, sizeof(DtsDcaCoreSSContext));
  ctx->core_pres = false;
  memset(&ctx->ext_ss, 0, sizeof(DtsDcaExtSSContext));
  ctx->ext_ss_pres = false;
  ctx->nb_audio_frames = 0;

  return seekPos(ctx->bs, 0, SEEK_SET);
}

DtsFrameInitializationRet initNextDtsFrame(
  DtsContext *ctx
)
{
  uint32_t sync_word;
  DtsAUInnerType type;
  DtsFrameInitializationRet ret;
  switch ((sync_word = nextUint32(ctx->bs))) {
  case DCA_SYNCWORD_CORE:
    /* DTS Coherent Acoustics Core */
    type = DTS_FRM_INNER_CORE_SS;
    ret = DTS_FRAME_INIT_CORE_SUBSTREAM;
    break;

  case DCA_SYNCEXTSSH:
    type = DTS_FRM_INNER_EXT_SS_HDR;
    ret = DTS_FRAME_INIT_EXT_SUBSTREAM;
    break;

  default:
    LIBBLU_DTS_ERROR_RETURN(
      "Unknown sync word 0x%08" PRIX32 ".\n",
      sync_word
    );
  }

  DtsAUCellPtr cell;
  if (NULL == (cell = initDtsAUCell(&ctx->cur_au, type)))
    return -1;
  cell->offset = tellPos(ctx->bs);

  return ret;
}


static uint64_t _getAndUpdatePTSCore(
  DtsDcaCoreSSContext *core,
  unsigned *nb_audio_frames
)
{
  const DcaCoreBSHeaderParameters *bsh = &core->cur_frame.bs_header;
  uint64_t pts = getPTSDtsDcaCoreSSContext(core);

  core->nb_parsed_samples += (bsh->NBLKS + 1) * (bsh->SHORT + 1);
  if (0 < core->nb_frames)
    (*nb_audio_frames)++; // Subsequent core ss starts a new audio frame.

  return pts;
}

static uint64_t _getAndUpdatePTSExtSS(
  DtsDcaExtSSContext *ext_ss
)
{
  const DcaExtSSFrameParameters *frame = ext_ss->curFrames[ext_ss->currentExtSSIdx];
  assert(NULL != frame);
  const DcaExtSSHeaderSFParameters *sf = &frame->header.static_fields;
  uint64_t pts = getPTSDtsDcaExtSSContext(ext_ss);

  ext_ss->nb_parsed_samples += sf->nuExSSFrameDurationCode;

  return pts;
}

static uint64_t _getAndUpdatePTS(
  DtsContext *ctx
)
{
  DtsAUContentType content_type = identifyContentTypeDtsAUFrame(&ctx->cur_au);

  switch (content_type) {
  case DTS_AU_EMPTY:
    break;
  case DTS_AU_CORE_SS:
    return _getAndUpdatePTSCore(&ctx->core, &ctx->nb_audio_frames);
  case DTS_AU_EXT_SS:
    return _getAndUpdatePTSExtSS(&ctx->ext_ss);
  }

  return 0;
}

int completeDtsFrame(
  DtsContext *ctx
)
{
  uint64_t pts = _getAndUpdatePTS(ctx);

  if (doGenerateScriptDtsContext(ctx)) {
    int ret = processCompleteFrameDtsAUFrame(
      ctx->script_bs,
      ctx->script,
      &ctx->cur_au,
      ctx->src_file_idx,
      pts
    );
    if (ret < 0)
      return -1;
  }
  else {
    if (DTS_PARSMOD_TWO_PASS_FIRST == ctx->mode) {
      /* Peak Bit-Rate Smoothing first pass, save audio frame size. */
      DtsPbrSmoothingStats *pbrs = &ctx->xll.pbrSmoothing;
      uint32_t au_size = getSizeDtsAUFrame(&ctx->cur_au);
      if (saveFrameSizeDtsPbrSmoothing(pbrs, ctx->nb_audio_frames, pts, au_size) < 0)
        return -1;
    }

    discardWholeCurDtsAUFrame(&ctx->cur_au);
  }

  return 0;
}
