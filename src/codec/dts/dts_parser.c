#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include "dts_parser.h"

#define READ_BITS(d, br, s, e)                                                \
  do {                                                                        \
    uint32_t _val;                                                            \
    if (readBits(br, &_val, s) < 0)                                           \
      e;                                                                      \
    *(d) = _val;                                                              \
  } while (0)

#define READ_BITS_PO(d, br, s, e)                                             \
  do {                                                                        \
    uint32_t _val;                                                            \
    if (readBits(br, &_val, s) < 0)                                           \
      e;                                                                      \
    *(d) = _val + 1;                                                          \
  } while (0)

#define SKIP_BITS(br, s, e)                                                   \
  do {                                                                        \
    if (skipBits(br, s) < 0)                                                  \
      e;                                                                      \
  } while (0)

#define SKIP_BYTES(br, s, e)                                                  \
  do {                                                                        \
    if (skipBytes(br, s) < 0)                                                 \
      e;                                                                      \
  } while (0)

/** \~english
 * \brief Return the number of bits set to 0b1 in given mask.
 *
 * \param mask Binary mask to count bits from.
 * \return unsigned Number of bits set to 0b1 in given mask.
 *
 * E.g. _nbBitsSetTo1(0x8046) == 4.
 */
static unsigned _nbBitsSetTo1(
  uint16_t mask
)
{
  /* Brian Kernighan’s Algorithm */
  unsigned nb_bits;
  for (nb_bits = 0; mask; nb_bits++)
    mask &= (mask - 1);
  return nb_bits;
}

static int _parseDtshdChunk(
  DtsContext *ctx,
  LibbluESParsingSettings *settings
)
{
  DtshdFileHandler *hdlr = &ctx->dtshd;
  int ret = decodeDtshdFileChunk(ctx->bs, hdlr);

  if (0 == ret) {
    /* Successfully decoded chunk, collect parameters : */
    if (!hdlr->DTSHDHDR_count)
      LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
        "Expect DTSHDHDR to be the first chunk of DTSHD formatted file.\n"
      );

    if (
      !ctx->processed_dtspbr_file
      && shallPBRSPerformedDtshdFileHandler(hdlr)
    ) {
      /* Peak Bit-Rate Smoothing shall be performed, try to initialize */

      if (NULL == settings->options.pbr_filepath) {
        /* No .dtspbr file specified */
        LIBBLU_WARNING(
          "Missing PBR file in parameters, "
          "unable to perform PBR smoothing as requested by input file.\n"
        );
        hdlr->warningFlags.missing_required_dtspbr_file = true;
      }
      else {
        /* Parse Peak Bit-Rate statistics */
        if (initPbrFileHandler(settings->options.pbr_filepath, hdlr) < 0)
          return -1;

        LIBBLU_DTS_INFO("Perfoming two passes Peak Bit-Rate smoothing.\n");
        ctx->mode = DTS_PARSMOD_TWO_PASS_FIRST;
      }
      ctx->processed_dtspbr_file = true;
    }

    ctx->init_skip_delay = getInitialSkippingDelayDtshdFileHandler(hdlr);
  }

  return ret;
}

static int _decodeDcaCoreBitStreamHeader(
  BitstreamReaderPtr file,
  DcaCoreBSHeaderParameters *param
)
{
  uint32_t value;

  assert(NULL != file);
  assert(NULL != param);

  /* [u32 SYNC] // 0x7FFE8001 */
  uint32_t SYNC;
  READ_BITS(&SYNC, file, 32, return -1);
  assert(DCA_SYNCWORD_CORE == SYNC);

  /* [b1 FTYPE] */
  READ_BITS(&param->FTYPE, file, 1, return -1);

  /* [u5 SHORT] */
  READ_BITS(&param->SHORT, file, 5, return -1);

  /* [b1 CPF] */
  READ_BITS(&param->CPF, file, 1, return -1);

  /* [u7 NBLKS] */
  READ_BITS(&param->NBLKS, file, 7, return -1);

  /* [u14 FSIZE] */
  READ_BITS(&param->FSIZE, file, 14, return -1);

  /* [u6 AMODE] */
  READ_BITS(&param->AMODE, file, 6, return -1);

  /* [u4 SFREQ] */
  READ_BITS(&param->SFREQ, file, 4, return -1);

  /* [u5 RATE] */
  READ_BITS(&param->RATE, file, 5, return -1);

  /* [v1 FixedBit] */
  SKIP_BITS(file, 1, return -1);

  /* [b1 DYNF] */
  READ_BITS(&value, file, 1, return -1);
  param->DYNF = value & 0x1;

  /* [b1 TIMEF] */
  READ_BITS(&param->TIMEF, file, 1, return -1);

  /* [b1 AUXF] */
  READ_BITS(&param->AUXF, file, 1, return -1);

  /* [b1 HDCD] */
  READ_BITS(&param->HDCD, file, 1, return -1);

  /* [u3 EXT_AUDIO_ID] */
  READ_BITS(&param->EXT_AUDIO_ID, file, 3, return -1);

  /* [b1 EXT_AUDIO] */
  READ_BITS(&param->EXT_AUDIO, file, 1, return -1);

  /* [b1 ASPF] */
  READ_BITS(&param->ASPF, file, 1, return -1);

  /* [u2 LFF] */
  READ_BITS(&param->LFF, file, 2, return -1);

  /* [b1 HFLAG] */
  READ_BITS(&param->HFLAG, file, 1, return -1);

  param->HCRC = 0;
  if (param->CPF) {
    /* [u16 HCRC] */
    READ_BITS(&param->HCRC, file, 16, return -1);
  }

  /* [b1 FILTS] */
  READ_BITS(&param->FILTS, file, 1, return -1);

  /* [u4 VERNUM] */
  READ_BITS(&param->VERNUM, file, 4, return -1);

  if (DCA_MAX_SYNTAX_VERNUM < param->VERNUM)
    LIBBLU_DTS_ERROR_RETURN(
      "Incompatible DCA Encoder Software Revision code (0x%" PRIX8 ").\n",
      param->VERNUM
    );

  /* [u2 CHIST] */
  READ_BITS(&param->CHIST, file, 2, return -1);

  /* [v3 PCMR] */
  READ_BITS(&param->PCMR, file, 3, return -1);

  /* [b1 SUMF] */
  READ_BITS(&param->SUMF, file, 1, return -1);

  /* [b1 SUMS] */
  READ_BITS(&param->SUMS, file, 1, return -1);

  /* [u4 DIALNORM / UNSPEC] */
  READ_BITS(&param->DIALNORM, file, 4, return -1);

  return 0;
}

static void _printHeaderDcaCoreSS(
  DtsContext *ctx
)
{
  uint64_t nb_frames = 0, frame_pts = 0;

  if (ctx->core_pres) {
    nb_frames = ctx->core.nb_frames;
    frame_pts = getPTSDtsDcaCoreSSContext(&ctx->core);
  }

  lbc frame_time[STRT_H_M_S_MS_LEN];
  str_time(frame_time, STRT_H_M_S_MS_LEN, STRT_H_M_S_MS, frame_pts);

  int64_t startOff = tellPos(ctx->bs);
  LIBBLU_DTS_DEBUG(
    "0x%08" PRIX64 " === DCA Core Substream Frame %u - %" PRI_LBCS " ===\n",
    startOff, nb_frames, frame_time
  );
}

static int _decodeDcaCoreSS(
  DtsContext *ctx
)
{
  DtsDcaCoreSSContext *core = &ctx->core;

  _printHeaderDcaCoreSS(ctx);
  DcaCoreSSFrameParameters frame;

  /* Bit stream header */
  if (_decodeDcaCoreBitStreamHeader(ctx->bs, &frame.bs_header) < 0)
    return -1;

  uint32_t frame_size   = frame.bs_header.FSIZE + 1;
  uint32_t bsh_size     = getSizeDcaCoreBSHeader(&frame.bs_header);
  uint32_t payload_size = frame_size - bsh_size;

  /* [vn payload] */ // TODO: Parse and check payload?
  if (skipBytes(ctx->bs, payload_size) < 0)
    return -1;

  /* Skip frame if context asks */
  if ((ctx->skip_cur_au = (0 < ctx->init_skip_delay))) {
    if (discardCurDtsAUCell(&ctx->cur_au) < 0)
      return -1;
    ctx->init_skip_delay--;
    return 0;
  }

  if (!isFast2nbPassDtsContext(ctx)) {
    if (!ctx->core_pres) {
      /* First Sync Frame */
      if (checkDcaCoreSSCompliance(&frame, &core->warn_flags) < 0)
        return -1;
    }
    else {
      if (!constantDcaCoreSS(&core->cur_frame, &frame)) {
        if (checkDcaCoreSSChangeCompliance(&core->cur_frame, &frame, &core->warn_flags) < 0)
          return -1;
      }
      else
        LIBBLU_DTS_DEBUG_CORE(
          " NOTE: Skipping frame parameters compliance checks, same content.\n"
        );
    }
  }
  else {
    LIBBLU_DTS_DEBUG_CORE(
      " *not checked, already done*\n"
    );
  }

  ctx->core_pres = true;
  core->cur_frame = frame;
  core->nb_frames++;

  DtsAUCellPtr cell;
  if (NULL == (cell = recoverCurDtsAUCell(&ctx->cur_au)))
    return -1;
  cell->size = frame_size;

  if (addCurCellToDtsAUFrame(&ctx->cur_au) < 0)
    return -1;

  return 0;
}

/* ### DTS Extension Substream : ########################################### */

#define NumSpkrTableLookUp  nbChDcaExtChMaskCode
#define CountBitsSet_to_1  _nbBitsSetTo1

static int _decodeDcaExtSSHeaderMixMetadata(
  BitstreamReaderPtr file,
  DcaExtSSHeaderMixMetadataParameters *param
)
{
  /* [u2 nuMixMetadataAdjLevel] */
  READ_BITS(&param->nuMixMetadataAdjLevel, file, 2, return -1);

  /* [u2 nuBits4MixOutMask] */
  unsigned nuBits4MixOutMask;
  READ_BITS(&nuBits4MixOutMask, file, 2, return -1);
  nuBits4MixOutMask = (nuBits4MixOutMask + 1) << 2;

  /* [u2 nuNumMixOutConfigs] */
  READ_BITS_PO(&param->nuNumMixOutConfigs, file, 2, return -1);

  /* Output Audio Mix Configurations Loop : */
  for (unsigned ns = 0; ns < param->nuNumMixOutConfigs; ns++) {
    /* [u<nuBits4MixOutMask> nuMixOutChMask[ns]] */
    READ_BITS(&param->nuMixOutChMask[ns], file, nuBits4MixOutMask, return -1);
    /* [0 nNumMixOutCh] */
    param->nNumMixOutCh[ns] = NumSpkrTableLookUp(param->nuMixOutChMask[ns]);
  }

  return 0;
}

static int _decodeDcaExtSSHeaderStaticFields(
  BitstreamReaderPtr file,
  DcaExtSSHeaderSFParameters *param,
  unsigned nExtSSIndex
)
{

  /* [u2 nuRefClockCode] */
  READ_BITS(&param->nuRefClockCode, file, 2, return -1);

  /* [u3 nuExSSFrameDurationCode] */
  READ_BITS(&param->nuExSSFrameDurationCode_code, file, 3, return -1);
  param->nuExSSFrameDurationCode = 512 * (param->nuExSSFrameDurationCode_code + 1);

  /* [b1 bTimeStampFlag] */
  READ_BITS(&param->bTimeStampFlag, file, 1, return -1);

  if (param->bTimeStampFlag) {
    /* [u32 nuTimeStamp] */
    READ_BITS(&param->nuTimeStamp, file, 32, return -1);

    /* [u4 nLSB] */
    READ_BITS(&param->nLSB, file, 4, return -1);
  }

  /* [u3 nuNumAudioPresnt] */
  READ_BITS_PO(&param->nuNumAudioPresnt, file, 3, return -1);

  /* [u3 nuNumAssets] */
  READ_BITS_PO(&param->nuNumAssets, file, 3, return -1);

  for (unsigned nAuPr = 0u; nAuPr < param->nuNumAudioPresnt; nAuPr++) {
    /* [u<nExtSSIndex+1> nuActiveExSSMask] */
    READ_BITS(&param->nuActiveExSSMask[nAuPr], file, nExtSSIndex+1, return -1);
  }

  MEMSET_ARRAY(param->nuActiveAssetMask, 0);
  for (unsigned nAuPr = 0u; nAuPr < param->nuNumAudioPresnt; nAuPr++) {
    for (unsigned nSS = 0u; nSS <= nExtSSIndex; nSS++) {
      if ((param->nuActiveExSSMask[nAuPr] >> nSS) & 0x1) {
        /* [u8 nuActiveAssetMask[nAuPr][nSS]] */
        READ_BITS(&param->nuActiveAssetMask[nAuPr][nSS], file, 8, return -1);
      }
    }
  }

  /* [b1 bMixMetadataEnbl] */
  READ_BITS(&param->bMixMetadataEnbl, file, 1, return -1);

  if (param->bMixMetadataEnbl) {
    /* [vn MixMedata()] */
    if (_decodeDcaExtSSHeaderMixMetadata(file, &param->mixMetadata) < 0)
      return -1;
  }

  return 0;
}

static int _decodeDcaExtSSAssetDescSF(
  BitstreamReaderPtr file,
  DcaAudioAssetDescSFParameters *param
)
{

  /* [b1 bAssetTypeDescrPresent] */
  READ_BITS(&param->bAssetTypeDescrPresent, file, 1, return -1);

  if (param->bAssetTypeDescrPresent) {
    /* [u4 nuAssetTypeDescriptor] */
    READ_BITS(&param->nuAssetTypeDescriptor, file, 4, return -1);
  }

  /* [b1 bLanguageDescrPresent] */
  READ_BITS(&param->bLanguageDescrPresent, file, 1, return -1);

  if (param->bLanguageDescrPresent) {
    /* [v24 LanguageDescriptor] */
    MEMSET_ARRAY(param->LanguageDescriptor, 0);
    for (unsigned i = 0; i < DTS_EXT_SS_LANGUAGE_DESC_SIZE; i++)
      READ_BITS(&param->LanguageDescriptor[i], file, 8, return -1);
  }

  /* [b1 bInfoTextPresent] */
  READ_BITS(&param->bInfoTextPresent, file, 1, return -1);

  if (param->bInfoTextPresent) {
    /* [u10 nuInfoTextByteSize] */
    READ_BITS_PO(&param->nuInfoTextByteSize, file, 10, return -1);

    if (DTS_EXT_SS_MAX_STRING_TEXT_MAX_LEN < param->nuInfoTextByteSize)
      LIBBLU_DTS_ERROR_RETURN(
        "DCA Extension Substream asset descriptor Additional Text Info "
        "unsupported size (exceed %zu characters).\n",
        DTS_EXT_SS_MAX_STRING_TEXT_MAX_LEN
      );

    /* [v<nuInfoTextByteSize> InfoTextString] */
    MEMSET_ARRAY(param->InfoTextString, 0);
    for (unsigned i = 0; i < param->nuInfoTextByteSize; i++)
      READ_BITS(&param->InfoTextString[i], file, 8, return -1);
  }

  /* [u5 nuBitResolution] */
  READ_BITS_PO(&param->nuBitResolution, file, 5, return -1);

  /* [u4 nuMaxSampleRate] */
  READ_BITS(&param->nuMaxSampleRate, file, 4, return -1);

  // param->maxSampleRate = getSampleFrequencyDcaExtMaxSampleRate(
  //   param->nuMaxSampleRate
  // );

  /* [u8 nuTotalNumChs] */
  READ_BITS_PO(&param->nuTotalNumChs, file, 8, return -1);

  if (DTS_EXT_SS_MAX_CHANNELS_NB < param->nuTotalNumChs)
    LIBBLU_DTS_ERROR_RETURN(
      "DCA Extension Substream asset descriptor unsupported total "
      "number of channels (exceed %zu).\n",
      DTS_EXT_SS_MAX_CHANNELS_NB
    );

  /* [b1 bOne2OneMapChannels2Speakers] */
  READ_BITS(&param->bOne2OneMapChannels2Speakers, file, 1, return -1);

  // Defaults :
  param->bEmbeddedStereoFlag = false;
  param->bEmbeddedSixChFlag = false;
  unsigned nuNumBits4SAMask = 0;

  if (param->bOne2OneMapChannels2Speakers) {
    if (2 < param->nuTotalNumChs) {
      /* [b1 bEmbeddedStereoFlag] */
      READ_BITS(&param->bEmbeddedStereoFlag, file, 1, return -1);
    }

    if (6 < param->nuTotalNumChs) {
      /* [b1 bEmbeddedSixChFlag] */
      READ_BITS(&param->bEmbeddedSixChFlag, file, 1, return -1);
    }

    /* [b1 bSpkrMaskEnabled] */
    READ_BITS(&param->bSpkrMaskEnabled, file, 1, return -1);

    if (param->bSpkrMaskEnabled) {
      /* [u2 nuNumBits4SAMask] */
      READ_BITS(&nuNumBits4SAMask, file, 2, return -1);
      nuNumBits4SAMask = (nuNumBits4SAMask + 1) << 2;

      /* [v<nuNumBits4SAMask> nuSpkrActivityMask] */
      READ_BITS(&param->nuSpkrActivityMask, file, nuNumBits4SAMask, return -1);
    }

    /* [u3 nuNumSpkrRemapSets] */
    READ_BITS(&param->nuNumSpkrRemapSets, file, 3, return -1);

    if (!nuNumBits4SAMask && 0 < param->nuNumSpkrRemapSets) {
      /**
       * This point is not very precise in specifications, but it seams to
       * be possible to had speakers remapping sets without having a speakers
       * activity mask, making value 'nuNumBits4SAMask' (or 'spkrMaskNbBits' here)
       * to be not defined.
       * This is why defining remapping sets without 'bSpkrMaskEnabled' set to 0b1
       * is considered as an error.
       */
      LIBBLU_DTS_ERROR_RETURN(
        "DCA Extension substream asset descriptor unsupported "
        "0-sized nuNumBits4SAMask with bSpkrMaskEnabled == 0b1.\n"
      );
    }

    for (unsigned ns = 0; ns < param->nuNumSpkrRemapSets; ns++) {
      /* [v<nuNumBits4SAMask> nuStndrSpkrLayoutMask] */
      READ_BITS(&param->nuStndrSpkrLayoutMask[ns], file, nuNumBits4SAMask, return -1);

      /* [0 nuNumSpeakers] */
      param->nuNumSpeakers[ns] = NumSpkrTableLookUp(param->nuStndrSpkrLayoutMask[ns]);
    }

    for (unsigned ns = 0; ns < param->nuNumSpkrRemapSets; ns++) {
      /* [u5 nuNumDecCh4Remap[ns]] */
      READ_BITS_PO(&param->nuNumDecCh4Remap[ns], file, 5, return -1);

      for (unsigned nCh = 0; nCh < param->nuNumSpeakers[ns]; nCh++) {
        /* [v<nuNumDecCh4Remap> nuRemapDecChMask] */
        READ_BITS(&param->nuRemapDecChMask[ns][nCh], file, param->nuNumDecCh4Remap[ns], return -1);

        /* [0 nCoeff] */
        param->nCoeff[ns][nCh] = CountBitsSet_to_1(param->nuRemapDecChMask[ns][nCh]);

        for (unsigned nc = 0; nc < param->nCoeff[ns][nCh]; nc++) {
          /* [u5 nuSpkrRemapCodes[ns][nCh][nc]] */
          READ_BITS(&param->nuSpkrRemapCodes[ns][nCh][nc], file, 5, return -1);
        }
      }
    }
  }
  else {
    /* [u3 nuRepresentationType] */
    READ_BITS(&param->nuRepresentationType, file, 3, return -1);
  }

  return 0;
}

static int _decodeDcaExtSSAssetDescDM(
  BitstreamReaderPtr file,
  DcaAudioAssetDescDMParameters *param,
  DcaAudioAssetDescSFParameters *ad_sf,
  DcaExtSSHeaderSFParameters *sf
)
{

  /* [b1 bDRCCoefPresent] */
  READ_BITS(&param->bDRCCoefPresent, file, 1, return -1);

  if (param->bDRCCoefPresent) {
    /* [u8 nuDRCCode] */
    READ_BITS(&param->nuDRCCode, file, 8, return -1);
  }

  /* [b1 bDialNormPresent] */
  READ_BITS(&param->bDialNormPresent, file, 1, return -1);

  if (param->bDialNormPresent) {
    /* [u5 nuDialNormCode] */
    READ_BITS(&param->nuDialNormCode, file, 5, return -1);
  }

  if (param->bDRCCoefPresent && ad_sf->bEmbeddedStereoFlag) {
    /* [u8 nuDRC2ChDmixCode] */
    READ_BITS(&param->nuDRC2ChDmixCode, file, 8, return -1);
  }

  param->bMixMetadataPresent = false;
  if (sf->bMixMetadataEnbl) {
    /* [b1 bMixMetadataPresent] */
    READ_BITS(&param->bMixMetadataPresent, file, 1, return -1);
  }

#if DCA_EXT_SS_DISABLE_MIX_META_SUPPORT
  if (param->mixMetadataPresent)
    LIBBLU_DTS_ERROR_RETURN(
      "This stream contains Mixing metadata, "
      "which are not supported in this compiled program version"
    );
#else
  if (param->bMixMetadataPresent) {
    /* [b1 bExternalMixFlag] */
    READ_BITS(&param->bExternalMixFlag, file, 1, return -1);

    /* [u6 nuPostMixGainAdjCode] */
    READ_BITS(&param->nuPostMixGainAdjCode, file, 6, return -1);

    /* [u2 nuControlMixerDRC] */
    READ_BITS(&param->nuControlMixerDRC, file, 2, return -1);

    if (param->nuControlMixerDRC < 3) {
      /* [u3 nuLimit4EmbeddedDRC] */
      READ_BITS(&param->nuLimit4EmbeddedDRC, file, 3, return -1);
    }
    else if (param->nuControlMixerDRC == 3) {
      /* [u8 nuCustomDRCCode] */
      READ_BITS(&param->nuCustomDRCCode, file, 8, return -1);
    }

    /* [b1 bEnblPerChMainAudioScale] */
    READ_BITS(&param->bEnblPerChMainAudioScale, file, 1, return -1);

    for (
      unsigned ns = 0;
      ns < sf->mixMetadata.nuNumMixOutConfigs;
      ns++
    ) {
      if (param->bEnblPerChMainAudioScale) {
        for (unsigned nCh = 0; nCh < sf->mixMetadata.nNumMixOutCh[ns]; nCh++) {
          /* [u6 nuMainAudioScaleCode[ns][nCh]] // Unique per channel */
          READ_BITS(&param->nuMainAudioScaleCode[ns][nCh], file, 6, return -1);
        }
      }
      else {
        /* [u6 nuMainAudioScaleCode[ns][0]] // Unique for all channels */
        READ_BITS(&param->nuMainAudioScaleCode[ns][0], file, 6, return -1);
      }
    }

    /**
     * Preparing Down-mixes properties for mix parameters
     * (Main complete mix is considered as the first one) :
     */
    param->nEmDM = 1;
    param->nDecCh[0] = ad_sf->nuTotalNumChs;

    if (ad_sf->bEmbeddedSixChFlag) {
      param->nDecCh[param->nEmDM] = 6;
      param->nEmDM++;
    }

    if (ad_sf->bEmbeddedStereoFlag) {
      param->nDecCh[param->nEmDM] = 2;
      param->nEmDM++;
    }

    for (unsigned ns = 0; ns < sf->mixMetadata.nuNumMixOutConfigs; ns++) {
      /* Mix Configurations loop */
      for (unsigned nE = 0; nE < param->nEmDM; nE++) {
        /* Embedded downmix loop */
        for (unsigned nCh = 0; nCh < param->nDecCh[nE]; nCh++) {
          /* Supplemental Channel loop */

          /* [v<nNumMixOutCh[ns]> nuMixMapMask[ns][nE][nCh]] */
          READ_BITS(
            &param->nuMixMapMask[ns][nE][nCh],
            file,
            sf->mixMetadata.nNumMixOutCh[ns],
            return -1
          );

          /* [0 nuNumMixCoefs[ns][nE][nCh]] */
          param->nuNumMixCoefs[ns][nE][nCh] = CountBitsSet_to_1(param->nuMixMapMask[ns][nE][nCh]);

          for (unsigned nC = 0; nC < param->nuNumMixCoefs[ns][nE][nCh]; nC++) {
            /* [u6 nuMixCoeffs[ns][nE][nCh][nC]] */
            READ_BITS(&param->nuMixCoeffs[ns][nE][nCh][nC], file, 6, return -1);
          }
        }
      }
    }
  } /* End if (bMixMetadataPresent) */
#endif

  return 0;
}

static int _decodeDcaAudioAssetExSSLBRParameters(
  BitstreamReaderPtr file,
  DcaAudioAssetExSSLBRParameters *p
)
{
  /* [u14 nuExSSLBRFsize] */
  READ_BITS_PO(&p->nuExSSLBRFsize, file, 14, return -1);

  /* [b1 bExSSLBRSyncPresent] */
  READ_BITS(&p->bExSSLBRSyncPresent, file, 1, return -1);

  if (p->bExSSLBRSyncPresent) {
    /* [u2 nuExSSLBRSyncDistInFrames] */
    READ_BITS(&p->nuExSSLBRSyncDistInFrames_code, file, 2, return -1);
    p->nuExSSLBRSyncDistInFrames = 1 << p->nuExSSLBRSyncDistInFrames_code;
  }

  return 0;
}

static int _decodeDcaAudioAssetSSXllParameters(
  BitstreamReaderPtr file,
  DcaAudioAssetExSSXllParameters *p,
  unsigned nuBits4ExSSFsize
)
{
  /* [u<nuBits4ExSSFsize> nuExSSXLLFsize] */
  READ_BITS_PO(&p->nuExSSXLLFsize, file, nuBits4ExSSFsize, return -1);

  /* [b1 bExSSXLLSyncPresent] */
  READ_BITS(&p->bExSSXLLSyncPresent, file, 1, return -1);

  if (p->bExSSXLLSyncPresent) {
    /* [u4 nuPeakBRCntrlBuffSzkB] */
    READ_BITS(&p->nuPeakBRCntrlBuffSzkB, file, 4, return -1);
    p->nuPeakBRCntrlBuffSzkB <<= 4;

    /* [u5 nuBitsInitDecDly] */
    READ_BITS(&p->nuBitsInitDecDly, file, 5, return -1);
    p->nuBitsInitDecDly++;

    /* [u<nuBitsInitDecDly> nuInitLLDecDlyFrames] */
    READ_BITS(&p->nuInitLLDecDlyFrames, file, p->nuBitsInitDecDly, return -1);

    /* [u<nuBits4ExSSFsize> nuExSSXLLSyncOffset] */
    READ_BITS(&p->nuExSSXLLSyncOffset, file, nuBits4ExSSFsize, return -1);
  }

  return 0;
}

static int _decodeDcaExSSAssetDescDecNDMode0(
  BitstreamReaderPtr file,
  DcaAudioAssetDescDecNDParameters *param,
  unsigned nuBits4ExSSFsize
)
{
  DcaAudioAssetDescDecNDCodingComponents *cc = &param->coding_components;

  /* [u12 nuCoreExtensionMask] */
  READ_BITS(&param->nuCoreExtensionMask, file, 12, return -1);

  if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_CORE_DCA) {
    DcaAudioAssetExSSCoreParameters *p = &cc->ExSSCore;

    /* [u14 nuExSSCoreFsize] */
    READ_BITS_PO(&p->nuExSSCoreFsize, file, 14, return -1);

    /* [b1 bExSSCoreSyncPresent] */
    READ_BITS(&p->bExSSCoreSyncPresent, file, 1, return -1);

    if (p->bExSSCoreSyncPresent) {
      /* [u2 nuExSSCoreSyncDistInFrames] */
      READ_BITS(&p->nuExSSCoreSyncDistInFrames_code, file, 2, return -1);
      p->nuExSSCoreSyncDistInFrames = 1 << p->nuExSSCoreSyncDistInFrames_code;
    }
  }

  if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_XBR) {
    /* [u14 nuExSSXBRFsize] */
    READ_BITS_PO(&cc->ExSSXBR.nuExSSXBRFsize, file, 14, return -1);
  }

  if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_XXCH) {
    /* [u14 nuExSSXXCHFsize] */
    READ_BITS_PO(&cc->ExSSXXCH.nuExSSXXCHFsize, file, 14, return -1);
  }

  if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_X96) {
    /* [u12 nuExSSX96Fsize] */
    READ_BITS_PO(&cc->ExSSX96.nuExSSX96Fsize, file, 14, return -1);
  }

  if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_LBR) {
    if (_decodeDcaAudioAssetExSSLBRParameters(file, &cc->ExSSLBR) < 0)
      return -1;
  }

  if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_XLL) {
    DcaAudioAssetExSSXllParameters *p = &cc->ExSSXLL;
    if (_decodeDcaAudioAssetSSXllParameters(file, p, nuBits4ExSSFsize) < 0)
      return -1;
  }

  if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_RESERVED_1) {
    /* [v16 *Ignore*] */
    READ_BITS(&cc->res_ext_1_data, file, 16, return -1);
  }

  if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_RESERVED_2) {
    /* [v16 *Ignore*] */
    READ_BITS(&cc->res_ext_2_data, file, 16, return -1);
  }

  return 0;
}

static int _decodeDcaExSSAssetDescDecNDMode1(
  BitstreamReaderPtr file,
  DcaAudioAssetDescDecNDParameters *param,
  unsigned nuBits4ExSSFsize
)
{
  param->nuCoreExtensionMask = DCA_EXT_SS_COD_COMP_EXTSUB_XLL;

  return _decodeDcaAudioAssetSSXllParameters(
    file,
    &param->coding_components.ExSSXLL,
    nuBits4ExSSFsize
  );
}

static int _decodeDcaExSSAssetDescDecNDMode2(
  BitstreamReaderPtr file,
  DcaAudioAssetDescDecNDParameters *param
)
{
  param->nuCoreExtensionMask = DCA_EXT_SS_COD_COMP_EXTSUB_LBR;

  return _decodeDcaAudioAssetExSSLBRParameters(
    file,
    &param->coding_components.ExSSLBR
  );
}

static int _decodeDcaExSSAssetDescDecNDMode3(
  BitstreamReaderPtr file,
  DcaAudioAssetDescDecNDParameters *param
)
{
  param->nuCoreExtensionMask = 0;

  DcaAudioAssetDescDecNDAuxiliaryCoding *aux = &param->auxilary_coding;

  /* [u14 nuExSSAuxFsize] */
  READ_BITS_PO(&aux->nuExSSAuxFsize, file, 14, return -1);

  /* [u8 nuAuxCodecID] */
  READ_BITS(&aux->nuAuxCodecID, file, 8, return -1);

  /* [b1 bExSSAuxSyncPresent] */
  READ_BITS(&aux->bExSSAuxSyncPresent, file, 1, return -1);

  if (aux->bExSSAuxSyncPresent) {
    /* [u2 nuExSSAuxSyncDistInFrames] */
    READ_BITS(&aux->nuExSSAuxSyncDistInFrames_code, file, 2, return -1);
    aux->nuExSSAuxSyncDistInFrames = 1 << aux->nuExSSAuxSyncDistInFrames_code;
  }

  return 0;
}

static int _decodeDcaExtSSAssetDescDecND(
  BitstreamReaderPtr file,
  DcaAudioAssetDescDecNDParameters *param,
  DcaAudioAssetDescSFParameters *ad_sf,
  DcaAudioAssetDescDMParameters *ad_dm,
  DcaExtSSHeaderSFParameters *sf,
  unsigned nuBits4ExSSFsize
)
{

  /* [u2 nuCodingMode] */
  READ_BITS(&param->nuCodingMode, file, 2, return -1);

  int ret = -1;
  switch (param->nuCodingMode) {
  case DCA_EXT_SS_CODING_MODE_DTS_HD_COMPONENTS:
    /* DTS-HD component(s). */
    ret = _decodeDcaExSSAssetDescDecNDMode0(file, param, nuBits4ExSSFsize);
    break;

  case DCA_EXT_SS_CODING_MODE_DTS_HD_LOSSLESS_WITHOUT_CORE:
    /* DTS-HD Master Audio without retro-compatible core. */
    ret = _decodeDcaExSSAssetDescDecNDMode1(file, param, nuBits4ExSSFsize);
    break;

  case DCA_EXT_SS_CODING_MODE_DTS_HD_LOW_BITRATE:
    /* DTS-HD Express. */
    ret = _decodeDcaExSSAssetDescDecNDMode2(file, param);
    break;

  case DCA_EXT_SS_CODING_MODE_AUXILIARY_CODING:
    /* Auxiliary audio coding. */
    ret = _decodeDcaExSSAssetDescDecNDMode3(file, param);
    break;
  }
  if (ret < 0)
    return ret;

  if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_XLL) {
    /* Extension Substream contains DTS-XLL component. */
    DcaAudioAssetExSSXllParameters *p = &param->coding_components.ExSSXLL;

    /* [u3 nuDTSHDStreamID] */
    READ_BITS(&p->nuDTSHDStreamID, file, 3, return -1);
  }

  /* Check presence of mix metadata */
  param->mix_md_fields_pres = (
    ad_sf->bOne2OneMapChannels2Speakers
    && (NULL != sf && sf->bMixMetadataEnbl)
    && !ad_dm->bMixMetadataPresent
  );

  param->bOnetoOneMixingFlag = false; // Default
  if (param->mix_md_fields_pres) {
    /* [b1 bOnetoOneMixingFlag] */
    READ_BITS(&param->bOnetoOneMixingFlag, file, 1, return -1);
  }

  if (param->bOnetoOneMixingFlag) {
    /* [b1 bEnblPerChMainAudioScale] */
    READ_BITS(&param->bEnblPerChMainAudioScale, file, 1, return -1);

    for (unsigned ns = 0; ns < sf->mixMetadata.nuNumMixOutConfigs; ns++) {
      if (param->bEnblPerChMainAudioScale) {
        for (unsigned nCh = 0; nCh < sf->mixMetadata.nNumMixOutCh[ns]; nCh++) {
          /* [u6 nuMainAudioScaleCode[mixConfigIdx][channelIdx]] */
          READ_BITS(&param->nuMainAudioScaleCode[ns][nCh], file, 6, return -1);
        }
      }
      else {
        /* [u6 nuMainAudioScaleCode[mixConfigIdx][0]] */
        READ_BITS(&param->nuMainAudioScaleCode[ns][0], file, 6, return -1);
      }
    }
  }

  /* [b1 bDecodeAssetInSecondaryDecoder] */
  READ_BITS(&param->bDecodeAssetInSecondaryDecoder, file, 1, return -1);

  return 0;
}

static int _decodeDcaExtSSAssetDescriptor(
  BitstreamReaderPtr file,
  DcaAudioAssetDescParameters *param,
  bool bStaticFieldsPresent,
  DcaExtSSHeaderSFParameters *sf,
  unsigned nuBits4ExSSFsize
)
{
  int64_t start_off = tellBinaryPos(file);

  /* -- Main Asset Parameters Section (Size, Index and Per Stream Static Metadata) -- */

  /* [u9 nuAssetDescriptFsize] */
  READ_BITS_PO(&param->nuAssetDescriptFsize, file, 9, return -1);

  int64_t exp_end_off = start_off + (param->nuAssetDescriptFsize * 8);

  /* [u3 nuAssetIndex] */
  READ_BITS(&param->nuAssetIndex, file, 3, return -1);

  DcaAudioAssetDescSFParameters *ad_sf = &param->static_fields; // Static fields
  ad_sf->bOne2OneMapChannels2Speakers = false; // Default

  if (bStaticFieldsPresent) {
    if (_decodeDcaExtSSAssetDescSF(file, ad_sf) < 0)
      return -1;
  }

  /* -- Dynamic Metadata Section (DRC, DNC and Mixing Metadata) -- */
  DcaAudioAssetDescDMParameters *ad_dm = &param->dyn_md; // Dynamic metadata
  if (_decodeDcaExtSSAssetDescDM(file, &param->dyn_md, ad_sf, sf) < 0)
    return -1;

  /* -- Decoder Navigation Data Section (Coding mode...) -- */
  DcaAudioAssetDescDecNDParameters *ad_dnd = &param->dec_nav_data;
  if (_decodeDcaExtSSAssetDescDecND(file, ad_dnd, ad_sf, ad_dm, sf, nuBits4ExSSFsize) < 0)
    return -1;

#if DCA_EXT_SS_ENABLE_DRC_2
  ad_dnd->extra_data_pres = (0 < exp_end_off - tellBinaryPos(file));

  if (ad_dnd->extra_data_pres) {
    /* [b1 bDRCMetadataRev2Present] */
    READ_BITS(&ad_dnd->bDRCMetadataRev2Present, file, 1, return -1);

    if (ad_dnd->bDRCMetadataRev2Present) {
      /* [u4 DRCversion_Rev2] */
      READ_BITS(&ad_dnd->DRCversion_Rev2, file, 4, return -1);

      if (DCA_EXT_SS_DRC_REV_2_VERSION_1 == ad_dnd->DRCversion_Rev2) {
        /* [0 nRev2_DRCs] */
        uint16_t nRev2_DRCs = sf->nuExSSFrameDurationCode >> 8;

        /* [u<8*nRev2_DRCs> DRCCoeff_Rev2[subsubFrame]] */
        SKIP_BYTES(file, nRev2_DRCs, return -1);
      }
    }
  }
#else
  ad_dnd->extra_data_pres = false;
#endif

  /* Compute expected header end offset */
  int64_t rem_bits = (exp_end_off - tellBinaryPos(file));

  if (rem_bits < 0)
    LIBBLU_DTS_ERROR_RETURN(
      "Unexpected DCA Extension substream asset descriptor length "
      "(expected: %zu bits, %zu bits parsed).\n",
      param->nuAssetDescriptFsize << 3,
      tellBinaryPos(file) - start_off
    );

  /* Compute reserved fields size (and determine if these are saved or not). */
  param->Reserved_size        = rem_bits >> 3;
  param->ZeroPadForFsize_size = rem_bits & 0x7; /* % 8 */
  bool save_reserved_field = isSavedReservedFieldDcaExtSS(
    param->Reserved_size,
    param->ZeroPadForFsize_size
  );

  /* [v<nuExtSSHeaderSize - tellPos() - 16> Reserved] */
  if (0 < param->Reserved_size) {
    if (save_reserved_field) {
      if (readBytes(file, param->Reserved, param->Reserved_size) < 0)
        return -1;
    }
    else {
      if (skipBytes(file, param->Reserved_size))
        return -1;
    }
  }

  /* [v0..7 ByteAlign] */
  if (save_reserved_field) {
    READ_BITS(
      &param->Reserved[param->Reserved_size],
      file, param->ZeroPadForFsize_size,
      return -1
    );
  }
  else
    SKIP_BITS(file, param->ZeroPadForFsize_size, return -1);

  return 0;
}

static int _decodeDcaExtSSHeader(
  BitstreamReaderPtr file,
  DcaExtSSHeaderParameters *param
)
{
  int64_t start_off = tellBinaryPos(file);

  /* [u32 SYNCEXTSSH] // 0x64582025 */
  uint32_t SYNCEXTSSH;
  READ_BITS(&SYNCEXTSSH, file, 32, return -1);
  assert(DCA_SYNCEXTSSH == SYNCEXTSSH);

  /* [v8 UserDefinedBits] */
  READ_BITS(&param->UserDefinedBits, file, 8, return -1);

#if !DCA_EXT_SS_DISABLE_CRC
  initCrcContext(crcCtxBitstream(file), DCA_EXT_SS_CRC_PARAM, DCA_EXT_SS_CRC_INITIAL_V);
#endif

  /* [u2 nExtSSIndex] */
  READ_BITS(&param->nExtSSIndex, file, 2, return -1);

  /** [b1 bHeaderSizeType]
   * nuBits4Header    = (bHeaderSizeType) ? 12 : 8;
   * nuBits4ExSSFsize = (bHeaderSizeType) ? 20 : 16;
   */
  READ_BITS(&param->bHeaderSizeType, file, 1, return -1);
  unsigned nuBits4Header    = (param->bHeaderSizeType) ? 12 : 8;
  unsigned nuBits4ExSSFsize = (param->bHeaderSizeType) ? 20 : 16;

  /* [u<nuBits4Header> nuExtSSHeaderSize] */
  READ_BITS_PO(&param->nuExtSSHeaderSize, file, nuBits4Header, return -1);

  /* [u<extSubFrmLengthFieldSize> nuExtSSFsize] */
  READ_BITS_PO(&param->nuExtSSFsize, file, nuBits4ExSSFsize, return -1);

  /* [b1 bStaticFieldsPresent] */
  READ_BITS(&param->bStaticFieldsPresent, file, 1, return -1);

  DcaExtSSHeaderSFParameters *sf = &param->static_fields;
  if (param->bStaticFieldsPresent) {
    /* [vn StaticFields] */
    if (_decodeDcaExtSSHeaderStaticFields(file, sf, param->nExtSSIndex) < 0)
      return -1;
  }
  else {
    // Default
    sf->nuNumAudioPresnt = 1;
    sf->nuNumAssets = 1;
  }

  for (unsigned nAst = 0; nAst < sf->nuNumAssets; nAst++) {
    /* [u<nuBits4ExSSFsize> nuAssetFsize[nAst]] */
    READ_BITS_PO(&param->audioAssetsLengths[nAst], file, nuBits4ExSSFsize, return -1);
  }

  for (unsigned nAst = 0; nAst < sf->nuNumAssets; nAst++) {
    DcaAudioAssetDescParameters *desc = &param->audioAssets[nAst];
    bool sf_pres = param->bStaticFieldsPresent;

    /* [vn AssetDecriptor()] */
    if (_decodeDcaExtSSAssetDescriptor(file, desc, sf_pres, sf, nuBits4ExSSFsize) < 0)
      return -1;
  }

  for (unsigned nAst = 0; nAst < sf->nuNumAssets; nAst++) {
    /* [b1 bBcCorePresent[nAst]] */
    READ_BITS(&param->bBcCorePresent[nAst], file, 1, return -1);
  }

  for (unsigned nAst = 0; nAst < sf->nuNumAssets; nAst++) {
    if (param->bBcCorePresent[nAst]) {
      /* [u2 nuBcCoreExtSSIndex[nAst]] */
      READ_BITS(&param->nuBcCoreExtSSIndex[nAst], file, 2, return -1);

      /* [u3 nuBcCoreAssetIndex[nAst]] */
      READ_BITS(&param->nuBcCoreAssetIndex[nAst], file, 3, return -1);
    }
  }

  /* Compute expected header end offset */
  int64_t exp_end_off = start_off + (param->nuExtSSHeaderSize * 8);
  int64_t rem_bits = exp_end_off - tellBinaryPos(file) - 16;

  if (rem_bits < 0)
    LIBBLU_DTS_ERROR_RETURN(
      "Unexpected DCA Extension Substream header length (LENGTH: 0x%" PRIX64").\n",
      param->nuExtSSHeaderSize
    );

  /* Compute reserved fields size (and determine if these are saved or not). */
  param->Reserved_size        = rem_bits / 8;
  param->ZeroPadForFsize_size = rem_bits & 0x7; /* % 8 */
  bool save_reserved_field = (
    rem_bits <= 8 * DCA_EXT_SS_MAX_SUPP_RES_FIELD_SIZE
  );

  /* [v<nuExtSSHeaderSize - tellPos() - 16> Reserved] */
  if (0 < param->Reserved_size) {
    if (save_reserved_field) {
      if (readBytes(file, param->Reserved, param->Reserved_size) < 0)
        return -1;
    }
    else {
      if (skipBytes(file, param->Reserved_size))
        return -1;
    }
  }

  /* [v0..7 ByteAlign] */
  if (save_reserved_field) {
    READ_BITS(
      &param->Reserved[param->Reserved_size],
      file, param->ZeroPadForFsize_size, return -1
    );
  }
  else
    SKIP_BITS(file, param->ZeroPadForFsize_size, return -1);

#if !DCA_EXT_SS_DISABLE_CRC
  uint16_t crc_result = completeCrcContext(crcCtxBitstream(file));
#endif

  /* [u16 nCRC16ExtSSHeader] */
  READ_BITS(&param->nCRC16ExtSSHeader, file, 16, return -1);

#if !DCA_EXT_SS_DISABLE_CRC
  if (param->nCRC16ExtSSHeader != bswap16(crc_result))
    LIBBLU_DTS_ERROR_RETURN(
      "DCA Extension Substream CRC check failure.\n"
    );
#endif

  return 0;
}

static int _decodeDcaExtSubAsset(
  DtsContext *ctx,
  DcaAudioAssetDescParameters asset,
  uint32_t asset_length
)
{
  const DcaAudioAssetDescDecNDParameters *ad_dnd = &asset.dec_nav_data;
  uint16_t coding_mask = ad_dnd->nuCoreExtensionMask;

  if (DCA_EXT_SS_CODING_MODE_AUXILIARY_CODING == ad_dnd->nuCodingMode)
    return 0; // No decoding procedure for auxiliary coding

  if (coding_mask & DCA_EXT_SS_COD_COMP_EXTSUB_CORE_DCA)
    return 0; // Unsupported DCA core in extSS.
  if (coding_mask & DCA_EXT_SS_COD_COMP_EXTSUB_XBR)
    return 0; // Unsupported DCA XBR ext in extSS.
  if (coding_mask & DCA_EXT_SS_COD_COMP_EXTSUB_XXCH)
    return 0; // Unsupported DCA XXCH ext in extSS.
  if (coding_mask & DCA_EXT_SS_COD_COMP_EXTSUB_X96)
    return 0; // Unsupported DCA X96 ext in extSS.
  if (coding_mask & DCA_EXT_SS_COD_COMP_EXTSUB_LBR)
    return 0; // Unsupported LBR ext in extSS.

  if (coding_mask & DCA_EXT_SS_COD_COMP_EXTSUB_XLL) {
    /* Initialize XLL context */
    if (initDtsXllFrameContext(&ctx->xll, asset, &ctx->dtshd) < 0)
      return -1;
    ctx->xll_present = true;

    /* Parse XLL PBR period and decode it if it is ready */
    if (parseDtsXllFrame(ctx->bs, &ctx->xll, asset_length, ad_dnd) < 0)
      return -1;
  }

  return 0;
}

static int _computeTargetExtSSAssetSize(
  DtsContext *ctx,
  uint8_t *xll_asset_idx_ret,
  uint32_t *xll_target_size
)
{
  uint32_t target_size;
  if (getFrameTargetSizeDtsXllPbr(&ctx->xll, ctx->nb_audio_frames, &target_size) < 0)
    return -1;

  uint32_t non_xll_size = 0;
  if (ctx->core_pres)
    non_xll_size += ctx->core.cur_frame.bs_header.FSIZE + 1;

  uint8_t xll_asset_idx = UINT8_MAX;
  for (unsigned i = 0; i < DCA_EXT_SS_MAX_NB_INDEXES; i++) {
    if (ctx->ext_ss.presentIndexes[i]) {
      const DcaExtSSHeaderParameters *hdr = &ctx->ext_ss.curFrames[i]->header;
      non_xll_size += hdr->nuExtSSHeaderSize;

      for (unsigned j = 0; j < hdr->static_fields.nuNumAssets; j++) {
        uint32_t non_xll_asset_size = hdr->audioAssetsLengths[j];
        const DcaAudioAssetDescDecNDParameters *dnd = &hdr->audioAssets[j].dec_nav_data;

        if (dnd->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_XLL) {
          assert(UINT8_MAX == xll_asset_idx); // Assert unique XLL component
          non_xll_asset_size -= dnd->coding_components.ExSSXLL.nuExSSXLLFsize;
          xll_asset_idx = i;
        }

        non_xll_size += non_xll_asset_size;
      }
    }
  }

  assert(UINT8_MAX != xll_asset_idx); // Assert present XLL component

  if (target_size <= non_xll_size)
    LIBBLU_DTS_ERROR_RETURN(
      "Unexpected PBRS resulting frame size (%" PRIu32 " < %" PRIu32 ").\n",
      target_size,
      non_xll_size
    );

  *xll_asset_idx_ret = xll_asset_idx;
  *xll_target_size = target_size - non_xll_size;

  return 0;
}

static int _patchDcaExtSSHeader(
  DtsContext *ctx,
  DcaExtSSHeaderParameters ext_ss_hdr,
  DcaXllFrameSFPosition *asset_content_offsets
)
{
  uint32_t target_xll_size;
  int ret;

  /* Compute target asset size */
#if 1
  uint8_t xll_asset_id;
  if (_computeTargetExtSSAssetSize(ctx, &xll_asset_id, &target_xll_size) < 0)
    return -1;
#else
  targetFrameSize = param.audioAssetsLengths[xllAssetId];
#endif

  /* Collect XLL frames to build asset */
  DcaXllFrameSFPosition builded_frame_content;
  int sync_word_off_idx;
  unsigned init_dec_delay;
  ret = substractPbrFrameSliceDtsXllFrameContext(
    &ctx->xll,
    target_xll_size,
    &builded_frame_content,
    &sync_word_off_idx,
    &init_dec_delay
  );
  if (ret < 0)
    return -1;

  /* Compute updated parameters */
  bool sync_word_present = (0 <= sync_word_off_idx);
  uint32_t sync_word_off = 0;

  if (sync_word_present) {
    ret = getRelativeOffsetDcaXllFrameSFPosition(
      builded_frame_content,
      builded_frame_content.sourceOffsets[sync_word_off_idx].offset,
      &sync_word_off
    );
    if (ret < 0)
      LIBBLU_DTS_ERROR_RETURN("Internal error, PBR buffer underflow.\n");
  }

  if (NULL != asset_content_offsets)
    *asset_content_offsets = builded_frame_content;

#if 0
  lbc_printf(
    "Frame %u size: %zu bytes.\n",
    ctx->ext_ss.nbFrames,
    targetFrameSize
  );
  debugPrintDcaXllFrameSFPositionIndexes(
    buildedFrameContent,
    2
  );
  lbc_printf("  Sync word: %s.\n", BOOL_PRESENCE(syncWordPresent));
  if (syncWordPresent) {
    lbc_printf("   -> Offset: %" PRId64 " byte(s).\n", syncWordOff);
    lbc_printf(
      "   -> Offset (ref): %zu byte(s).\n",
      param.audioAssets[xllAssetId].dec_nav_data.codingComponents.extSSXll.nbBytesOffXllSync
    );
    lbc_printf("   -> Delay: %u frame(s).\n", decodingDelay);
    lbc_printf(
      "   -> Delay (ref): %u frame(s).\n",
      param.audioAssets[xllAssetId].dec_nav_data.codingComponents.extSSXll.initialXllDecodingDelayInFrames
    );

    if (decodingDelay >= 2)
      exit(0);
  }
#endif

  unsigned PBRSmoothingBufSizeKb = getPBRSmoothingBufSizeKiBDtshdFileHandler(
    &ctx->dtshd
  );
  if (0 == PBRSmoothingBufSizeKb)
    LIBBLU_DTS_ERROR_RETURN(
      "Unable to set DTS PBR smoothing buffer size, "
      "missing information in DTS-HD header.\n"
    );

  /* Set ExtSS header */
  ext_ss_hdr.audioAssetsLengths[xll_asset_id] = target_xll_size;
  ext_ss_hdr.bHeaderSizeType = true;

  /* Set XLL asset header */
  DcaAudioAssetDescParameters *asset = &ext_ss_hdr.audioAssets[xll_asset_id];
  DcaAudioAssetExSSXllParameters *a_xll = &asset->dec_nav_data.coding_components.ExSSXLL;
  a_xll->nuExSSXLLFsize        = target_xll_size;
  a_xll->bExSSXLLSyncPresent   = sync_word_present;
  a_xll->nuInitLLDecDlyFrames  = init_dec_delay;
  a_xll->nuExSSXLLSyncOffset   = sync_word_off;
  a_xll->nuPeakBRCntrlBuffSzkB = PBRSmoothingBufSizeKb;

  return replaceCurDtsAUCell(
    &ctx->cur_au,
    (DtsAUInnerReplacementParam) {
      .ext_ss_hdr = ext_ss_hdr
    }
  );
}

// #define SET_DEBUG

static int _decodeDcaExtSS(
  DtsContext *ctx
)
{
  assert(NULL != ctx);

  int64_t frame_offset = tellPos(ctx->bs);

  lbc frame_time[STRT_H_M_S_MS_LEN];
  str_time(
    frame_time,
    STRT_H_M_S_MS_LEN, STRT_H_M_S_MS,
    (ctx->ext_ss_pres) ? getPTSDtsDcaExtSSContext(&ctx->ext_ss) : 0
  );

  LIBBLU_DTS_DEBUG(
    "0x%08" PRIX64 " === DCA Extension Substream Frame %u - %" PRI_LBCS " ===\n",
    frame_offset,
    (ctx->ext_ss_pres) ? ctx->ext_ss.nbFrames : 0,
    frame_time
  );

  bool skip_frame = ctx->skip_ext || ctx->skip_cur_au;

  /* Extension Substream Header */
  DcaExtSSFrameParameters frame;
  if (_decodeDcaExtSSHeader(ctx->bs, &frame.header) < 0)
    return -1;

  DcaXllFrameSFPosition xllAssetOffsets = {0};

  if (!skip_frame) {
    if (!isFast2nbPassDtsContext(ctx)) {
      if (checkDcaExtSSHeaderCompliance(&frame.header, ctx->is_secondary, &ctx->ext_ss.warningFlags) < 0)
        return -1;
    }
    else {
      LIBBLU_DTS_DEBUG_EXTSS(" *Not checked, already done*\n");
    }

#ifdef SET_DEBUG
    fprintf(
      stderr, "%" PRIu32 ",%" PRIu32 "\n",
      // frame_time,
      frame.header.nuExtSSFsize + ctx->core.cur_frame.bs_header.FSIZE + 1,
      frame.header.audioAssets[0].dec_nav_data.coding_components.ExSSXLL.nuExSSXLLFsize
    );
#endif

    uint8_t extSSIdx = frame.header.nExtSSIndex;
    if (!ctx->ext_ss.presentIndexes[extSSIdx]) {
      assert(!ctx->ext_ss.presentIndexes[extSSIdx]);

      DcaExtSSFrameParameters *cur_frame;
      if (NULL == (cur_frame = malloc(sizeof(DcaExtSSFrameParameters))))
        LIBBLU_DTS_ERROR_RETURN("Memory allocation error.\n");

      ctx->ext_ss.currentExtSSIdx = extSSIdx;
      ctx->ext_ss.curFrames[extSSIdx] = cur_frame;
      ctx->ext_ss.presentIndexes[extSSIdx] = true;
    }

#if 0
    else {
      cur_frame = ctx->ext_ss.curFrames[extSSIdx];

      if (
        checkDcaExtSSHeaderChangeCompliance(
          cur_frame->header,
          extFrame.header,
          &ctx->ext_ss.warningFlags
        ) < 0
      )
        return -1;
    }
#endif

    /* Frame content is always updated */
    memcpy(ctx->ext_ss.curFrames[extSSIdx], &frame, sizeof(DcaExtSSFrameParameters));

    int64_t cell_size = tellPos(ctx->bs) - frame_offset;
    if (cell_size < 0 || UINT32_MAX < cell_size)
      LIBBLU_DTS_ERROR_RETURN("ExtSS frame size error.\n");

    DtsAUCellPtr cell;
    if (NULL == (cell = recoverCurDtsAUCell(&ctx->cur_au)))
      return -1;
    cell->size = cell_size;

    if (isFast2nbPassDtsContext(ctx)) {
      if (_patchDcaExtSSHeader(ctx, frame.header, &xllAssetOffsets) < 0)
        return -1;
    }

    if (addCurCellToDtsAUFrame(&ctx->cur_au) < 0)
      return -1;

    ctx->ext_ss.nbFrames++;
    ctx->ext_ss_pres = true;
  }
  else {
    LIBBLU_DTS_DEBUG_EXTSS(" *Not checked, skipped*\n");
    if (discardCurDtsAUCell(&ctx->cur_au) < 0)
      return -1;
  }

  /* Decode extension substream assets */
  bool decode_assets =
    !skip_frame
    && !isFast2nbPassDtsContext(ctx)
    && (
      isInitPbrFileHandler()
      || DTS_XLL_FORCE_UNPACKING
      || DTS_XLL_FORCE_REBUILD_SEIING
    )
  ;

  const DcaExtSSHeaderSFParameters *sf = &frame.header.static_fields;
  for (unsigned nAst = 0; nAst < sf->nuNumAssets; nAst++) {
    int64_t asset_off = tellPos(ctx->bs);

    LIBBLU_DTS_DEBUG_EXTSS(
      "0x%08" PRIX64 " === DTS Extension Substream Component %u ===\n",
      asset_off, nAst
    );

    uint32_t asset_size = frame.header.audioAssetsLengths[nAst];
    DcaAudioAssetDescParameters *asset = &frame.header.audioAssets[nAst];

    if (!skip_frame) {
      /* Frame content saving */
      if (!ctx->ext_ss.content.parsedParameters) {
        if (frame.header.bStaticFieldsPresent) {
          ctx->ext_ss.content.nbChannels = asset->static_fields.nuTotalNumChs;
          ctx->ext_ss.content.audioFreq  = asset->static_fields.nuMaxSampleRate;
          ctx->ext_ss.content.bitDepth   = asset->static_fields.nuBitResolution;
          ctx->ext_ss.content.parsedParameters = true;
        }
      }

      if (asset->dec_nav_data.nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_XLL) {
        if (nAst != 0x0) // Avoid bugs with single DTS XLL context
          LIBBLU_DTS_ERROR_RETURN(
            "Unsupported multiple or not in the first index "
            "DTS XLL audio assets.\n"
          );
        ctx->ext_ss.content.xllExtSS = true;
      }
      if (asset->dec_nav_data.nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_LBR)
        ctx->ext_ss.content.lbrExtSS = true;
      if (asset->dec_nav_data.nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_XBR)
        ctx->ext_ss.content.xbrExtSS = true;

      /* Asset Access Unit cell */
      DtsAUCellPtr cell;
      if (NULL == (cell = initDtsAUCell(&ctx->cur_au, DTS_FRM_INNER_EXT_SS_ASSET)))
        return -1;
      cell->offset = asset_off;
      cell->size   = asset_size;

      if (isFast2nbPassDtsContext(ctx) && ctx->ext_ss.content.xllExtSS) {
        /* Replace if required frame content to apply PBR smoothing. */
        bool updated_asset = (
          xllAssetOffsets.nbSourceOffsets != 1
          || xllAssetOffsets.sourceOffsets[0].offset != asset_off
        );

        if (updated_asset) {
          /**
           * Update required because content offsets have been changed
           * by PBR process.
           */
          int ret = replaceCurDtsAUCell(
            &ctx->cur_au,
            (DtsAUInnerReplacementParam) {.ext_ss_asset = xllAssetOffsets}
          );
          if (ret < 0)
            return -1;
        }
      }
    }

    int64_t asset_dec_length = 0;
    if (decode_assets) {
      if (_decodeDcaExtSubAsset(ctx, *asset, asset_size) < 0)
        return -1;
      asset_dec_length = tellPos(ctx->bs) - asset_off;
    }

    if (asset_dec_length < 0 || asset_size < asset_dec_length)
      LIBBLU_DTS_ERROR_RETURN("Invalid asset decoding size.\n");
    if (skipBytes(ctx->bs, asset_size - asset_dec_length) < 0)
      return -1;

    if (!skip_frame) {
      if (addCurCellToDtsAUFrame(&ctx->cur_au) < 0)
        return -1;
    }
  }

  /* Check Extension Substream Size : */
  int64_t ext_ss_size = tellPos(ctx->bs) - frame_offset;
  if (
    !isFast2nbPassDtsContext(ctx)
    && (frame.header.nuExtSSFsize != ext_ss_size)
  ) {
    LIBBLU_DTS_ERROR_RETURN(
      "Unexpected Extension substream frame length "
      "(parsed length: %" PRId64 " bytes, expected %" PRIu32 " bytes).\n",
      ext_ss_size,
      frame.header.nuExtSSFsize
    );
  }

  return 0;
}

int parseDts(
  DtsContext *ctx,
  LibbluESParsingSettings *settings
)
{

  while (!isEof(ctx->bs)) {
    /* Progress bar : */
    printFileParsingProgressionBar(ctx->bs);

    if (isDtshdFileDtsContext(ctx)) {
      /* Read DTS-HD file chunks : */
      switch (_parseDtshdChunk(ctx, settings)) {
      case 0: /* Read next DTS-HD file chunk */
        continue;

      case 1:
        break; /* In DTS-HD Stream Data chunk, read DTS audio frames */

      default:
        return -1; /* Error */
      }
    }

    int64_t start_off = tellPos(ctx->bs);

    switch (initNextDtsFrame(ctx)) {
    case DTS_FRAME_INIT_CORE_SUBSTREAM:
      /* DTS Coherent Acoustics Core */
      if (_decodeDcaCoreSS(ctx) < 0)
        return -1;
      break;

    case DTS_FRAME_INIT_EXT_SUBSTREAM:
      /* DTS Coherent Acoustics Extension Substream */
      if (_decodeDcaExtSS(ctx) < 0)
        return -1;
      break;

    default: /* Error */
      return -1;
    }

    if (completeDtsFrame(ctx) < 0)
      return -1;

    int64_t frame_size = tellPos(ctx->bs) - start_off;
    if (frame_size < 0)
      LIBBLU_DTS_ERROR_RETURN("Negative frame size.\n");
    if (isDtshdFileDtsContext(ctx))
      ctx->dtshd.off_STRMDATA += frame_size;
  }

  return 0;
}

int analyzeDts(
  LibbluESParsingSettings *settings
)
{
  DtsContext ctx;
  if (initDtsContext(&ctx, settings) < 0)
    return -1;

  do {
    if (initParsingDtsContext(&ctx))
      goto free_return;

    if (isFast2nbPassDtsContext(&ctx)) {
      if (!ctx.xll_present)
        LIBBLU_ERROR_FRETURN(
          "PBR smoothing is only available for DTS XLL streams.\n"
        );

      /* Compute PBR Smoothing resulting frame sizes */
      if (performComputationDtsXllPbr(&ctx.xll) < 0)
        return -1;

      ctx.core.nb_frames = 0;
      ctx.ext_ss.nbFrames = 0;
    }

    if (parseDts(&ctx, settings) < 0)
      goto free_return;
  } while (nextPassDtsContext(&ctx));

  lbc_printf(lbc_str(" === Parsing finished with success. ===              \n"));


  int ret = completeDtsContext(&ctx);
  releasePbrFileHandler();
  return ret;

free_return:
  cleanDtsContext(&ctx);
  releasePbrFileHandler();
  return -1;
}
