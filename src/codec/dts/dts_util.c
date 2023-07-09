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

unsigned dtsExtReferenceClockCodeValue(
  const DcaExtReferenceClockCode code
)
{
  static const unsigned clockPeriods[3] = {
    32000,
    44100,
    48000
  };

  if (code < ARRAY_SIZE(clockPeriods))
    return clockPeriods[code];
  return 0;
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

unsigned dtsExtSourceSampleRateCodeValue(
  const DcaExtSourceSampleRateCode code
)
{
  static const unsigned sampleRates[] = {
    8000,
    16000,
    32000,
    64000,
    128000,
    22050,
    44100,
    88200,
    176400,
    352800,
    12000,
    24000,
    48000,
    96000,
    192000,
    384000
  };

  if (code < ARRAY_SIZE(sampleRates))
    return sampleRates[code];
  return 0;
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

DtsContextPtr createDtsContext(
  BitstreamReaderPtr inputFile,
  const lbc * outputFilename,
  LibbluESSettingsOptions options
)
{
  DtsContextPtr ctx;

  if (NULL == (ctx = (DtsContextPtr) malloc(sizeof(DtsContext))))
    LIBBLU_ERROR_NRETURN(
      "Memory allocation error.\n"
    );

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

  ctx->file = inputFile;
  ctx->esmsScriptOutputFile = NULL;
  ctx->dtshdFileHandle = NULL;

  ctx->corePresent = false;
  ctx->extSSPresent = false;

  ctx->xllCtx = NULL;

  /* Default parameters: */
  ctx->isSecondaryStream = options.secondaryStream;
  ctx->skipExtensionSubstreams = options.extractCore;
  ctx->skippedFramesControl = 0;

  ctx->curAccessUnit = NULL;

  ctx->esmsScriptOutputFile = createBitstreamWriter(
    outputFilename, WRITE_BUFFER_LEN
  );
  if (NULL == ctx->esmsScriptOutputFile)
    goto free_return;

  if (NULL == (ctx->curAccessUnit = createDtsAUFrame()))
    goto free_return;

  /* Test if DTS-HD File from DTS-HDMA Suite */
  if (isDtshdFile(inputFile)) {
    if (NULL == (ctx->dtshdFileHandle = createDtshdFileHandler()))
      goto free_return;
  }

  return ctx;

free_return:
  destroyDtsContext(ctx);
  return NULL;
}

void destroyDtsContext(
  DtsContextPtr ctx
)
{
  unsigned i;

  if (NULL == ctx)
    return;

  for (i = 0; i < DCA_EXT_SS_MAX_NB_INDEXES; i++) {
    if (ctx->extSS.presentIndexes[i])
      free(ctx->extSS.curFrames[i]);
  }

  closeBitstreamWriter(ctx->esmsScriptOutputFile);
  destroyDtshdFileHandler(ctx->dtshdFileHandle);
  destroyDtsXllFrameContext(ctx->xllCtx);
  destroyDtsAUFrame(ctx->curAccessUnit);
  free(ctx);
}

bool nextPassDtsContext(
  DtsContextPtr ctx
)
{
  assert(NULL != ctx);

  if (ctx->mode == DTS_PARSMOD_TWO_PASS_FIRST) {
    /* Switch to second pass */
    ctx->mode = DTS_PARSMOD_TWO_PASS_SECOND;
    return true;
  }

  return false;
}

int initParsingDtsContext(
  DtsContextPtr ctx
)
{
  return seekPos(DTS_CTX_IN_FILE(ctx), 0, SEEK_SET);
}

DtsFrameInitializationRet initNextDtsFrame(
  DtsContextPtr ctx
)
{
  DtsFrameInitializationRet ret;
  uint32_t syncWord;

  DtsAUCellPtr cell;
  DtsAUInnerType type;

  switch ((syncWord = nextUint32(ctx->file))) {
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

  if (NULL == (cell = initDtsAUCell(ctx->curAccessUnit, type)))
    return -1;

  cell->startOffset = tellPos(ctx->file);

  return ret;
}

#if 0
int addToEsmsDtsFrame(
  DtsContextPtr ctx,
  EsmsFileHeaderPtr dtsInfos,
  DtsCurrentPeriod * curPeriod
)
{

  if (!curPeriod->isExtSubstream) {
    /* Core sync frame */
    DtsDcaCoreSSContext * core = &ctx->core;

    assert(ctx->corePresent);

    if (initEsmsAudioPesFrame(dtsInfos, false, false, core->frameDts, 0) < 0)
      return -1;

    if (
      appendAddPesPayloadCommand(
        dtsInfos, ctx->sourceFileIdx, 0x0, curPeriod->syncFrameStartOffset,
        core->cur_frame.header.FSIZE
      ) < 0
    )
      return -1;

    core->frameDts +=
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
    DtsDcaExtSSContext * ext = &ctx->extSS;

    assert(ctx->extSSPresent);

    if (initEsmsAudioPesFrame(dtsInfos, true, false, ext->frameDts, 0) < 0)
      return -1;

    if (
      appendAddPesPayloadCommand(
        dtsInfos, ctx->sourceFileIdx,
        0x0, curPeriod->syncFrameStartOffset,
        ext->curFrames[
          ext->currentExtSSIdx
        ]->header.extensionSubstreamFrameLength
      ) < 0
    )
      return -1;

    ext->frameDts +=
      (
        (
          (uint64_t) ext->curFrames[
            ext->currentExtSSIdx
          ]->header.staticFields.frameDurationCodeValue
        ) * MAIN_CLOCK_27MHZ
      )
      / ext->curFrames[ext->currentExtSSIdx]->header.staticFields.referenceClockFreq
    ;
  }

  return writeEsmsPesPacket(
    ctx->esmsScriptOutputFile, dtsInfos
  );
}
#endif

int completeDtsFrame(
  DtsContextPtr ctx,
  EsmsFileHeaderPtr dtsInfos
)
{
  uint64_t dts;
  int ret;

  if (DTS_CTX_BUILD_ESMS_SCRIPT(ctx)) {
    switch (identifyContentTypeDtsAUFrame(ctx->curAccessUnit)) {
      case DTS_AU_CORE_SS: {
        const DcaCoreBSHeaderParameters * bsh = &ctx->core.cur_frame.bs_header;

        dts = ctx->core.frameDts;
        ctx->core.frameDts +=
          (1ull * (bsh->NBLKS + 1) * (bsh->SHORT + 1) * MAIN_CLOCK_27MHZ)
          / getDcaCoreAudioSampFreqCode(bsh->SFREQ);
        break;
      }

      case DTS_AU_EXT_SS:
        dts = ctx->extSS.frameDts;

        assert(NULL != ctx->extSS.curFrames[ctx->extSS.currentExtSSIdx]);

        ctx->extSS.frameDts +=
          (
            (
              (uint64_t) ctx->extSS.curFrames[
                ctx->extSS.currentExtSSIdx
              ]->header.staticFields.frameDurationCodeValue
            ) * MAIN_CLOCK_27MHZ
          )
          / (
            ctx->extSS.curFrames[
              ctx->extSS.currentExtSSIdx
            ]->header.staticFields.referenceClockFreq
          )
        ;
        break;

      default:
        dts = 0;
        break;
    }

    ret = processCompleteFrameDtsAUFrame(
      ctx->esmsScriptOutputFile,
      dtsInfos,
      ctx->curAccessUnit,
      ctx->sourceFileIdx,
      dts
    );
  }
  else {
    /* TODO: Add data to PBR smoothing process stats */
    discardWholeCurDtsAUFrame(ctx->curAccessUnit);
    ret = 0;
  }

  return ret;
}

int updateDtsEsmsHeader(
  DtsContextPtr ctx,
  EsmsFileHeaderPtr dtsInfos
)
{
  LibbluStreamCodingType coding_type;

  if (ctx->extSSPresent && ctx->extSS.content.xllExtSS)
    coding_type = STREAM_CODING_TYPE_HDMA, dtsInfos->bitrate = 24500000;
  else if (ctx->extSSPresent && ctx->extSS.content.xbrExtSS)
    coding_type = STREAM_CODING_TYPE_HDHR, dtsInfos->bitrate = 24500000;
  else if (ctx->extSSPresent && ctx->extSS.content.lbrExtSS)
    coding_type = STREAM_CODING_TYPE_DTSE_SEC, dtsInfos->bitrate = 256000;
  else if (ctx->corePresent)
    coding_type = STREAM_CODING_TYPE_DTS, dtsInfos->bitrate = 2000000;
  else
    LIBBLU_DTS_ERROR_RETURN(
      "Unexpected empty input stream.\n"
    );

  dtsInfos->prop.coding_type = coding_type;

  if (ctx->corePresent) {
    const DcaCoreBSHeaderParameters * bsh = &ctx->core.cur_frame.bs_header;
    dtsInfos->endPts = ctx->core.frameDts;

    switch (bsh->AMODE) {
      case DCA_AMODE_A:
        /* Mono */
        dtsInfos->prop.audio_format = 0x01;
        break;

      case DCA_AMODE_A_B:
        /* Dual-Mono */
        dtsInfos->prop.audio_format = 0x02;
        break;

      case DCA_AMODE_L_R:
      case DCA_AMODE_L_R_SUMDIF:
      case DCA_AMODE_LT_LR:
        /* Stereo */
        dtsInfos->prop.audio_format = 0x03;
        break;

      default:
        /* Multi-channel */
        dtsInfos->prop.audio_format = 0x06;
    }

    switch (getDcaCoreAudioSampFreqCode(bsh->SFREQ)) {
      case 48000: /* 48 kHz */
        dtsInfos->prop.sample_rate = SAMPLE_RATE_CODE_48000; break;
      case 96000: /* 96 kHz */
        dtsInfos->prop.sample_rate = SAMPLE_RATE_CODE_96000; break;
      default: /* 192 kHz */
        dtsInfos->prop.sample_rate = SAMPLE_RATE_CODE_192000;
    }

    dtsInfos->prop.bit_depth = getDcaCoreSourcePcmResCode(bsh->PCMR);
  }
  else {
    assert(ctx->extSSPresent);

    if (!ctx->extSS.content.parsedParameters)
      LIBBLU_DTS_ERROR_RETURN(
        "Missing mandatory static fields in Extension Substream, "
        "unable to define audio properties.\n"
      );

    dtsInfos->endPts = ctx->extSS.frameDts;

    if (ctx->extSS.content.nbChannels <= 1)
      dtsInfos->prop.audio_format = 0x01; /* Mono */
    else if (ctx->extSS.content.nbChannels == 2)
      dtsInfos->prop.audio_format = 0x03; /* Stereo */
    else
      dtsInfos->prop.audio_format = 0x06; /* Multi-channel */

    switch (ctx->extSS.content.audioFreq) {
      case 48000: /* 48 kHz */
        dtsInfos->prop.sample_rate = SAMPLE_RATE_CODE_48000; break;
      case 96000: /* 96 kHz */
        dtsInfos->prop.sample_rate = SAMPLE_RATE_CODE_96000; break;
      default: /* 192 kHz */
        dtsInfos->prop.sample_rate = SAMPLE_RATE_CODE_192000;
    }

    dtsInfos->prop.bit_depth = ctx->extSS.content.bitDepth;
  }

  return 0;
}