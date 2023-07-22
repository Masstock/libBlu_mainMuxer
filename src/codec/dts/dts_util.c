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

unsigned buildDcaExtChMaskString(
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

unsigned nbChDcaExtChMaskCode(
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

void dcaExtChMaskStrPrintFun(
  uint16_t mask,
  int (*printFun) (const lbc * format, ...)
)
{
  unsigned i;
  char * sep;

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

  static const char * chConfigsNames[16] = {
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

const char * dtsExtAssetTypeDescCodeStr(
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

const char * dtsExtRepresentationTypeCodeStr(
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

const char * dtsAudioAssetCodingModeStr(
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

const char * dtsXllCommonHeaderCrc16PresenceCodeStr(
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

int initDtsContext(
  DtsContext * ctx,
  LibbluESParsingSettings * settings
)
{
  const LibbluESSettingsOptions options = settings->options;
  BitstreamReaderPtr es_bs = NULL;
  BitstreamWriterPtr script_bs = NULL;
  EsmsFileHeaderPtr script = NULL;
  unsigned src_file_idx;
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
  script = createEsmsFileHandler(ES_AUDIO, options, FMT_SPEC_INFOS_NONE);
  if (NULL == script)
    LIBBLU_DTS_ERROR_FRETURN("Unable to create output script handler.\n");

  if (appendSourceFileEsms(script, settings->esFilepath, &src_file_idx) < 0)
    LIBBLU_DTS_ERROR_FRETURN("Unable to add source file to script handler.\n");

  /* Write script header */
  if (writeEsmsHeader(script_bs) < 0)
    return -1;

  /* Save in context */
  *ctx = (DtsContext) {
    .src_file_idx = src_file_idx,
    .bs = es_bs,
    .script_bs = script_bs,
    .script = script,
    .script_fp = settings->scriptFilepath,

    .is_dtshd_file = is_dtshd_file,
    .is_secondary = options.secondaryStream,
    .skip_ext = options.extractCore
  };

  if (isInitPbrFileHandler()) {
    LIBBLU_DTS_INFO("Perfoming two passes Peak Bit-Rate smoothing.\n");
    ctx->mode = DTS_PARSMOD_TWO_PASS_FIRST;
  }
  else {
#if DTS_XLL_FORCE_REBUILD_SEIING
    ctx->mode = DTS_PARSMOD_TWO_PASS_FIRST;
#else
    ctx->mode = DTS_PARSMOD_SINGLE_PASS;
#endif
  }

  return 0;

free_return:
  closeBitstreamWriter(script_bs);
  destroyEsmsFileHandler(script);

  return -1;
}

static int _writeScriptEndMarker(
  DtsContext * ctx
)
{
  /* [v8 endMarker] */
  if (writeByte(ctx->script_bs, ESMS_SCRIPT_END_MARKER) < 0)
    LIBBLU_DTS_ERROR_RETURN("Unable to write script end marker.\n");
  return 0;
}

int _setScriptProperties(
  DtsContext * ctx
)
{
  EsmsFileHeaderPtr script = ctx->script;
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
    const DcaCoreBSHeaderParameters * bsh = &ctx->core.cur_frame.bs_header;
    script->endPts = ctx->core.pts;

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

    script->prop.bit_depth = getDcaCoreSourcePcmResCode(bsh->PCMR) >> 4;
  }
  else {
    assert(ctx->ext_ss_pres);

    if (!ctx->ext_ss.content.parsedParameters)
      LIBBLU_DTS_ERROR_RETURN(
        "Missing mandatory static fields in Extension Substream, "
        "unable to define audio properties.\n"
      );

    script->endPts = ctx->ext_ss.pts;

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

    script->prop.bit_depth = ctx->ext_ss.content.bitDepth >> 4;
  }

  return 0;
}

static inline unsigned _getNbChannels(
  const DtsContext * ctx
)
{
  if (ctx->core_pres) {
    const DcaCoreBSHeaderParameters * bsh = &ctx->core.cur_frame.bs_header;
    return getNbChDcaCoreAudioChannelAssignCode(bsh->AMODE);
  }
  return 0; // TODO: Suppport secondary audio.
}

static void _printStreamInfos(
  const DtsContext * ctx
)
{
  /* Display infos : */
  lbc_printf("== Stream Infos =======================================================================\n");
  lbc_printf(
    "Codec: %" PRI_LBCS ", %s (%u channels), Sample rate: %u Hz, Bits per sample: %ubits.\n",
    streamCodingTypeStr(ctx->script->prop.coding_type),
    AudioFormatCodeStr(ctx->script->prop.audio_format),
    _getNbChannels(ctx),
    valueSampleRateCode(ctx->script->prop.sample_rate),
    valueBitDepthCode(ctx->script->prop.bit_depth)
  );
  lbc_printf(
    "Stream Duration: %02" PRIu64 ":%02" PRIu64 ":%02" PRIu64 "\n",
    (ctx->core.pts / MAIN_CLOCK_27MHZ) / 60 / 60,
    (ctx->core.pts / MAIN_CLOCK_27MHZ) / 60 % 60,
    (ctx->core.pts / MAIN_CLOCK_27MHZ) % 60
  );
  lbc_printf("=======================================================================================\n");
}

int completeDtsContext(
  DtsContext * ctx
)
{

  if (_writeScriptEndMarker(ctx) < 0)
    return -1;

  if (_setScriptProperties(ctx) < 0)
    return -1;
  _printStreamInfos(ctx);

  if (addEsmsFileEnd(ctx->script_bs, ctx->script) < 0)
    return -1;
  closeBitstreamWriter(ctx->script_bs);
  ctx->script_bs = NULL;

  if (updateEsmsHeader(ctx->script_fp, ctx->script) < 0)
    return -1;

  cleanDtsContext(ctx);
  return 0;
}

void cleanDtsContext(
  DtsContext * ctx
)
{
  if (NULL == ctx)
    return;

  closeBitstreamReader(ctx->bs);
  closeBitstreamWriter(ctx->script_bs);
  destroyEsmsFileHandler(ctx->script);
  cleanDtshdFileHandler(ctx->dtshd);
  cleanDtsDcaExtSSContext(ctx->ext_ss);
  cleanDtsXllFrameContext(ctx->xll);
  cleanDtsAUFrame(ctx->cur_au);
}

int initParsingDtsContext(
  DtsContext * ctx
)
{
  cleanDtshdFileHandler(ctx->dtshd);
  cleanDtsDcaExtSSContext(ctx->ext_ss);

  memset(&ctx->dtshd, 0, sizeof(DtshdFileHandler));
  memset(&ctx->core, 0, sizeof(DtsDcaCoreSSContext));
  ctx->core_pres = false;
  memset(&ctx->ext_ss, 0, sizeof(DtsDcaExtSSContext));
  ctx->ext_ss_pres = false;

  return seekPos(ctx->bs, 0, SEEK_SET);
}

DtsFrameInitializationRet initNextDtsFrame(
  DtsContext * ctx
)
{
  DtsFrameInitializationRet ret;
  uint32_t syncWord;

  DtsAUCellPtr cell;
  DtsAUInnerType type;

  switch ((syncWord = nextUint32(ctx->bs))) {
    case DCA_SYNCWORD_CORE:
      /* DTS Coherent Acoustics Core */
      ret = DTS_FRAME_INIT_CORE_SUBSTREAM;
      type = DTS_FRM_INNER_CORE_SS;
      break;

    case DCA_SYNCEXTSSH:
      ret = DTS_FRAME_INIT_EXT_SUBSTREAM;
      type = DTS_FRM_INNER_EXT_SS_HDR;
      break;

    default:
      LIBBLU_DTS_ERROR_RETURN(
        "Unknown sync word 0x%08" PRIX32 ".\n",
        syncWord
      );
  }

  if (NULL == (cell = initDtsAUCell(&ctx->cur_au, type)))
    return -1;
  cell->startOffset = tellPos(ctx->bs);

  return ret;
}

#if 0
int addToEsmsDtsFrame(
  DtsContext * ctx,
  EsmsFileHeaderPtr dtsInfos,
  DtsCurrentPeriod * curPeriod
)
{

  if (!curPeriod->isExtSubstream) {
    /* Core sync frame */
    DtsDcaCoreSSContext * core = &ctx->core;

    assert(ctx->core_pres);

    if (initEsmsAudioPesFrame(dtsInfos, false, false, core->pts, 0) < 0)
      return -1;

    if (
      appendAddPesPayloadCommand(
        dtsInfos, ctx->src_file_idx, 0x0, curPeriod->syncFrameStartOffset,
        core->cur_frame.header.FSIZE
      ) < 0
    )
      return -1;

    core->pts +=
      (
        (
          (uint64_t) core->cur_frame.header.NBLKS
          * core->cur_frame.header.SHORT
        ) * MAIN_CLOCK_27MHZ
      )
      / core->cur_frame.header.audioFreq
    ;
  }
  else {
    /* Extension substream sync frame */
    DtsDcaExtSSContext * ext = &ctx->ext_ss;

    assert(ctx->ext_ss_pres);

    if (initEsmsAudioPesFrame(dtsInfos, true, false, ext->pts, 0) < 0)
      return -1;

    if (
      appendAddPesPayloadCommand(
        dtsInfos, ctx->src_file_idx,
        0x0, curPeriod->syncFrameStartOffset,
        ext->curFrames[
          ext->currentExtSSIdx
        ]->header.extensionSubstreamFrameLength
      ) < 0
    )
      return -1;

    ext->pts +=
      (
        (
          (uint64_t) ext->curFrames[
            ext->currentExtSSIdx
          ]->header.static_fields.frameDurationCodeValue
        ) * MAIN_CLOCK_27MHZ
      )
      / ext->curFrames[ext->currentExtSSIdx]->header.static_fields.referenceClockFreq
    ;
  }

  return writeEsmsPesPacket(
    ctx->esmsScriptOutputFile, dtsInfos
  );
}
#endif

static uint64_t _getAndUpdatePTSCore(
  DtsDcaCoreSSContext * core
)
{
  const DcaCoreBSHeaderParameters * bsh = &core->cur_frame.bs_header;

  uint64_t increment =
    (MAIN_CLOCK_27MHZ * (bsh->NBLKS + 1) * (bsh->SHORT + 1))
    / getDcaCoreAudioSampFreqCode(bsh->SFREQ)
  ;

  uint64_t pts = core->pts;
  core->pts += increment;
  return pts;
}

static uint64_t _getAndUpdatePTSExtSS(
  DtsDcaExtSSContext * extss
)
{
  const DcaExtSSFrameParameters * frame = extss->curFrames[extss->currentExtSSIdx];
  assert(NULL != frame);
  const DcaExtSSHeaderSFParameters * sf = &frame->header.static_fields;

  uint64_t increment = MAIN_CLOCK_27MHZ * getExSSFrameDurationDcaExtRefClockCode(sf);

  uint64_t pts = extss->pts;
  extss->pts += increment;
  return pts;
}

static uint64_t _getAndUpdatePTS(
  DtsContext * ctx
)
{
  switch (identifyContentTypeDtsAUFrame(&ctx->cur_au)) {
    case DTS_AU_EMPTY:
      break;
    case DTS_AU_CORE_SS:
      return _getAndUpdatePTSCore(&ctx->core);
    case DTS_AU_EXT_SS:
      return _getAndUpdatePTSExtSS(&ctx->ext_ss);
  }

  return 0;
}

int completeDtsFrame(
  DtsContext * ctx
)
{
  int ret = 0;
  if (doGenerateScriptDtsContext(ctx)) {
    uint64_t pts = _getAndUpdatePTS(ctx);

    ret = processCompleteFrameDtsAUFrame(
      ctx->script_bs,
      ctx->script,
      &ctx->cur_au,
      ctx->src_file_idx,
      pts
    );
  }
  else {
    /* TODO: Add data to PBR smoothing process stats */
    discardWholeCurDtsAUFrame(&ctx->cur_au);
  }

  return ret;
}
