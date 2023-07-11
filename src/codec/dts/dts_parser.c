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

#define SKIP_BITS(br, s, e)                                                   \
  do {                                                                        \
    if (skipBits(br, s) < 0)                                                  \
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
static unsigned _nbBitsSetTo1(uint16_t mask)
{
  /* Brian Kernighan’s Algorithm */
  unsigned nbBits = 0;

  while (mask) {
    mask &= (mask - 1);
    nbBits++;
  }

  return nbBits;
}

int parseDtshdChunk(
  DtsContext * ctx
)
{
  DtshdFileHandler * hdlr = &ctx->dtshdFileHandle;
  int ret = decodeDtshdFileChunk(ctx->bs, hdlr, isFast2nbPassDtsContext(ctx));

  if (0 == ret) {
    /* Successfully decoded chunk, collect parameters : */
    if (!hdlr->DTSHDHDR_count)
      LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
        "Expect DTSHDHDR to be the first chunk of DTSHD formatted file.\n"
      );

    if (canBePerformedPBRSDtshdFileHandler(hdlr)) {
      if (!isInitPbrFileHandler()) {
        LIBBLU_WARNING(
          "Missing PBR file in parameters, "
          "unable to perform PBR smoothing as requested by input file.\n"
        );
        hdlr->warningFlags.missingRequiredPbrFile = true;
      }
    }

    ctx->skippedFramesControl = getInitialSkippingDelayDtshdFileHandler(hdlr);
  }

  return ret;
}

int decodeDcaCoreBitStreamHeader(
  BitstreamReaderPtr file,
  DcaCoreBSHeaderParameters * param
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
  DtsContext * ctx
)
{
  uint64_t nb_frames = 0, frame_dts = 0;
  if (ctx->corePresent) {
    nb_frames = ctx->core.nb_frames;
    frame_dts = ctx->core.pts;
  }

  lbc frame_time[STRT_H_M_S_MS_LEN];
  str_time(frame_time, STRT_H_M_S_MS_LEN, STRT_H_M_S_MS, frame_dts);

  int64_t startOff = tellPos(ctx->bs);
  LIBBLU_DTS_DEBUG(
    "0x%08" PRIX64 " === DCA Core Substream Frame %u - %" PRI_LBCS " ===\n",
    startOff, nb_frames, frame_time
  );
}

int decodeDcaCoreSS(
  DtsContext * ctx
)
{
  DtsDcaCoreSSContext * core = &ctx->core;

  _printHeaderDcaCoreSS(ctx);
  DcaCoreSSFrameParameters frame;

  /* Bit stream header */
  if (decodeDcaCoreBitStreamHeader(ctx->bs, &frame.bs_header) < 0)
    return -1;

  unsigned frame_size   = frame.bs_header.FSIZE + 1;
  unsigned bsh_size     = getSizeDcaCoreBSHeader(&frame.bs_header);
  unsigned payload_size = frame_size - bsh_size;

  /* [vn payload] */ // TODO: Parse and check payload?
  if (skipBytes(ctx->bs, payload_size) < 0)
    return -1;

  /* Skip frame if context asks */
  if ((ctx->skipCurrentPeriod = (0 < ctx->skippedFramesControl))) {
    if (discardCurDtsAUCell(&ctx->curAccessUnit) < 0)
      return -1;
    ctx->skippedFramesControl--;
    return 0;
  }

  if (!ctx->corePresent) {
    initDtsDcaCoreSSContext(core);

    /* First Sync Frame */
    if (checkDcaCoreSSCompliance(&frame, &core->warning_flags) < 0)
      return -1;

    core->cur_frame = frame;
    ctx->corePresent = true;
  }
  else if (!isFast2nbPassDtsContext(ctx)) {
    if (!constantDcaCoreSS(&core->cur_frame, &frame)) {
      if (checkDcaCoreSSChangeCompliance(&core->cur_frame, &frame, &core->warning_flags) < 0)
        return -1;

      core->cur_frame = frame;
    }
    else
      LIBBLU_DTS_DEBUG_CORE(
        " NOTE: Skipping frame parameters compliance checks, same content.\n"
      );
  }

  core->nb_frames++;

  DtsAUCellPtr cell;
  if (NULL == (cell = recoverCurDtsAUCell(&ctx->curAccessUnit)))
    return -1;
  cell->length = frame_size;

  if (addCurCellToDtsAUFrame(&ctx->curAccessUnit) < 0)
    return -1;

  return 0;
}

int decodeDcaExtSSHeaderMixMetadata(
  BitstreamReaderPtr file,
  DcaExtSSHeaderMixMetadataParameters * param
)
{
  uint32_t value;

  unsigned ns;
  size_t mixOutputMaskNbBits;

  /* [u2 nuMixMetadataAdjLevel] */
  READ_BITS(&value, file, 2, return -1);
  param->nuMixMetadataAdjLevel = value;

  /* [u2 nuBits4MixOutMask] */
  READ_BITS(&value, file, 2, return -1);
  mixOutputMaskNbBits = (value + 1) << 2;

  /* [u2 nuNumMixOutConfigs] */
  READ_BITS(&value, file, 2, return -1);
  param->nuNumMixOutConfigs = value + 1;

  /* Output Audio Mix Configurations Loop : */
  for (ns = 0; ns < param->nuNumMixOutConfigs; ns++) {
    /* [u<nuBits4MixOutMask> nuMixOutChMask[ns]] */
    if (readBits(file, &value, mixOutputMaskNbBits) < 0)
      return -1;
    param->nuMixOutChMask[ns] = value;
    param->nNumMixOutCh[ns] = nbChDcaExtChMaskCode(
      param->nuMixOutChMask[ns]
    );
  }

  return 0;
}

int decodeDcaExtSSHeaderStaticFields(
  BitstreamReaderPtr file,
  DcaExtSSHeaderStaticFieldsParameters * param,
  const uint8_t nExtSSIndex
)
{
  uint32_t value;

  unsigned nAuPr, nSS;

  /* [u2 nuRefClockCode] */
  READ_BITS(&value, file, 2, return -1);
  param->referenceClockCode = value;

  /* [u3 nuExSSFrameDurationCode] */
  READ_BITS(&value, file, 3, return -1);
  param->frameDurationCode = value;
  param->frameDurationCodeValue = (param->frameDurationCode + 1) * 512;

  param->referenceClockFreq = dtsExtReferenceClockCodeValue(
    param->referenceClockCode
  );

  if (0 < param->referenceClockFreq)
    param->frameDuration =
      param->frameDurationCodeValue * ((float) 1 / param->referenceClockFreq)
    ;
  else
    param->frameDuration = -1;

  /* [b1 bTimeStampFlag] */
  READ_BITS(&value, file, 1, return -1);
  param->timeStampPresent = value;

  if (param->timeStampPresent) {
    /* [u32 nuTimeStamp] */
    READ_BITS(&value, file, 32, return -1);
    param->timeStampValue = value;

    /* [u4 nLSB] */
    READ_BITS(&value, file, 4, return -1);
    param->timeStampLsb = value;

    param->timeStamp =
      ((uint64_t) param->timeStampValue << 4)
      | param->timeStampLsb
    ;
  }

  /* [u3 nuNumAudioPresnt] */
  READ_BITS(&value, file, 3, return -1);
  param->nbAudioPresentations = value + 1;

  /* [u3 nuNumAssets] */
  READ_BITS(&value, file, 3, return -1);
  param->nbAudioAssets = value + 1;

  for (nAuPr = 0; nAuPr < param->nbAudioPresentations; nAuPr++) {
    /* [u<nExtSSIndex+1> nuNumAssets] */
    if (readBits(file, &value, nExtSSIndex + 1) < 0)
      return -1;

    param->activeExtSSMask[nAuPr] = value;
  }

  for (nAuPr = 0; nAuPr < param->nbAudioPresentations; nAuPr++) {
    for (nSS = 0; nSS < (unsigned) nExtSSIndex + 1; nSS++) {

      if ((param->activeExtSSMask[nAuPr] >> nSS) & 0x1) {
        /* [u8 nuActiveAssetMask[nAuPr][nSS]] */
        READ_BITS(&value, file, 8, return -1);
      }
      else
        value = 0;
      param->activeAssetMask[nAuPr][nSS] = value;
    }
  }

  /* [b1 bMixMetadataEnbl] */
  READ_BITS(&value, file, 1, return -1);
  param->mixMetadataEnabled = value;

  if (param->mixMetadataEnabled) {
    /* [vn MixMedata()] */
    if (decodeDcaExtSSHeaderMixMetadata(file, &param->mixMetadata) < 0)
      return -1;
  }

  return 0;
}

int decodeDcaExtSSAssetDescriptorStaticFields(
  BitstreamReaderPtr file,
  DcaAudioAssetDescriptorStaticFieldsParameters * param
)
{
  uint32_t value;

  int64_t i;
  unsigned ns, nCh, nc;

  size_t spkrMaskNbBits;

  /* [b1 bAssetTypeDescrPresent] */
  READ_BITS(&value, file, 1, return -1);
  param->assetTypeDescriptorPresent = value;

  if (param->assetTypeDescriptorPresent) {
    /* [u4 nuAssetTypeDescriptor] */
    READ_BITS(&value, file, 4, return -1);
    param->assetTypeDescriptor = value;
  }

  /* [b1 bLanguageDescrPresent] */
  READ_BITS(&value, file, 1, return -1);
  param->languageDescriptorPresent = value;

  if (param->languageDescriptorPresent) {
    /* [v24 LanguageDescriptor] */
    for (i = 0; i < DTS_EXT_SS_LANGUAGE_DESC_SIZE; i++) {
      READ_BITS(&value, file, 8, return -1);
      param->languageDescriptor[i] = value;
    }
    param->languageDescriptor[i] = '\0';
  }

  /* [b1 bInfoTextPresent] */
  READ_BITS(&value, file, 1, return -1);
  param->infoTextPresent = value;

  if (param->infoTextPresent) {
    /* [u10 nuInfoTextByteSize] */
    READ_BITS(&value, file, 10, return -1);
    param->infoTextLength = value + 1;

    if (DTS_EXT_SS_MAX_STRING_TEXT_MAX_LEN < param->infoTextLength)
      LIBBLU_DTS_ERROR_RETURN(
        "DCA Extension Substream asset descriptor Additional Text Info "
        "unsupported size (exceed %zu characters).\n",
        DTS_EXT_SS_MAX_STRING_TEXT_MAX_LEN
      );

    /* [v<nuInfoTextByteSize> InfoTextString] */
    for (i = 0; i < param->infoTextLength; i++) {
      READ_BITS(&value, file, 8, return -1);
      param->infoText[i] = value;
    }
    param->infoText[i] = '\0';
  }

  /* [u5 nuBitResolution] */
  READ_BITS(&value, file, 5, return -1);
  param->bitDepth = value + 1;

  /* [u4 nuMaxSampleRate] */
  READ_BITS(&value, file, 4, return -1);
  param->maxSampleRateCode = value;

  param->maxSampleRate = dtsExtSourceSampleRateCodeValue(
    param->maxSampleRateCode
  );

  /* [u8 nuTotalNumChs] */
  READ_BITS(&value, file, 8, return -1);
  param->nbChannels = value + 1;

  if (DTS_EXT_SS_MAX_CHANNELS_NB < param->nbChannels)
    LIBBLU_DTS_ERROR_RETURN(
      "DCA Extension Substream asset descriptor unsupported total "
      "number of channels (exceed %zu).\n",
      DTS_EXT_SS_MAX_CHANNELS_NB
    );

  /* [b1 bOne2OneMapChannels2Speakers] */
  READ_BITS(&value, file, 1, return -1);
  param->directSpeakersFeed = value;

  if (param->directSpeakersFeed) {
    if (2 < param->nbChannels) {
      /* [b1 bEmbeddedStereoFlag] */
      READ_BITS(&value, file, 1, return -1);
      param->embeddedStereoDownmix = value;
    }
    else
      param->embeddedStereoDownmix = false;

    if (6 < param->nbChannels) {
      /* [b1 bEmbeddedSixChFlag] */
      READ_BITS(&value, file, 1, return -1);
      param->embeddedSurround6chDownmix = value;
    }
    else
      param->embeddedSurround6chDownmix = false;

    /* [b1 bSpkrMaskEnabled] */
    READ_BITS(&value, file, 1, return -1);
    param->bSpkrMaskEnabled = value;

    if (param->bSpkrMaskEnabled) {
      /* [u2 nuNumBits4SAMask] */
      READ_BITS(&value, file, 2, return -1);
      spkrMaskNbBits = (value + 1) << 2;

      /* [v<nuNumBits4SAMask> nuSpkrActivityMask] */
      if (readBits(file, &value, spkrMaskNbBits) < 0)
        return -1;
      param->nuSpkrActivityMask = value;
    }
    else
      spkrMaskNbBits = 0;

    /* [u3 nuNumSpkrRemapSets] */
    READ_BITS(&value, file, 3, return -1);
    param->nuNumSpkrRemapSets = value;

    if (!spkrMaskNbBits && 0 < param->nuNumSpkrRemapSets) {
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

    for (ns = 0; ns < param->nuNumSpkrRemapSets; ns++) {
      /* [v<nuNumBits4SAMask> nuStndrSpkrLayoutMask] */
      if (readBits(file, &value, spkrMaskNbBits) < 0)
        return -1;
      param->nuStndrSpkrLayoutMask[ns] = value;

      /* [0 nuNumSpeakers] */
      param->nbChsInRemapSet[ns] = nbChDcaExtChMaskCode(
        param->nuStndrSpkrLayoutMask[ns]
      );
    }

    for (ns = 0; ns < param->nuNumSpkrRemapSets; ns++) {
      /* [u5 nuNumDecCh4Remap[ns]] */
      READ_BITS(&value, file, 5, return -1);
      param->nbChRequiredByRemapSet[ns] = value + 1;

      for (nCh = 0; nCh < param->nbChsInRemapSet[ns]; nCh++) {
        /* [v<nuNumDecCh4Remap> nuRemapDecChMask] */
        if (readBits(file, &value, param->nbChRequiredByRemapSet[ns]) < 0)
          return -1;
        param->nuRemapDecChMask[ns][nCh] = value;

        /* [0 nCoeff] */
        param->nbRemapCoeffCodes[ns][nCh] = _nbBitsSetTo1(
          param->nuRemapDecChMask[ns][nCh]
        );

        for (nc = 0; nc < param->nbRemapCoeffCodes[ns][nCh]; nc++) {
          /* [u5 nuSpkrRemapCodes[ns][nCh][nc]] */
          READ_BITS(&value, file, 5, return -1);
          param->outputSpkrRemapCoeffCodes[ns][nCh][nc] = value;
        }
      }
    }
  }
  else {
    param->embeddedStereoDownmix = false;
    param->embeddedSurround6chDownmix = false;

    /* [u3 nuRepresentationType] */
    READ_BITS(&value, file, 3, return -1);
    param->representationType = value;
  }

  return 0;
}

int decodeDcaExtSSAssetDescriptorDynamicMetadata(
  BitstreamReaderPtr file,
  DcaAudioAssetDescriptorDynamicMetadataParameters * param,
  DcaAudioAssetDescriptorStaticFieldsParameters * assetStaticFieldsParam,
  DcaExtSSHeaderStaticFieldsParameters * staticFieldsParam
)
{
  uint32_t value;

  unsigned ns, nE, nCh, nC;

  /* [b1 bDRCCoefPresent] */
  READ_BITS(&value, file, 1, return -1);
  param->drcEnabled = value;

  if (param->drcEnabled) {
    /* [u8 nuDRCCode] */
    READ_BITS(&value, file, 8, return -1);
    param->drcParameters.drcCode = value;
  }

  /* [b1 bDialNormPresent] */
  READ_BITS(&value, file, 1, return -1);
  param->dialNormEnabled = value;

  if (param->dialNormEnabled) {
    /* [u5 nuDialNormCode] */
    READ_BITS(&value, file, 5, return -1);
    param->dialNormCode = value;
  }

  if (param->drcEnabled && assetStaticFieldsParam->embeddedStereoDownmix) {
    /* [u8 nuDRC2ChDmixCode] */
    READ_BITS(&value, file, 8, return -1);
    param->drcParameters.drc2ChCode = value;
  }

  if (NULL != staticFieldsParam && staticFieldsParam->mixMetadataEnabled) {
    /* [b1 bMixMetadataPresent] */
    READ_BITS(&value, file, 1, return -1);
    param->mixMetadataPresent = value;
  }
  else
    param->mixMetadataPresent = false;

#if DCA_EXT_SS_DISABLE_MIX_META_SUPPORT
  if (param->mixMetadataPresent)
    LIBBLU_DTS_ERROR_RETURN(
      "This stream contains Mixing metadata, "
      "which are not supported in this compiled program version"
    );
#else
  if (param->mixMetadataPresent) {
    /* [b1 bExternalMixFlag] */
    READ_BITS(&value, file, 1, return -1);
    param->mixMetadata.useExternalMix = value;

    /* [u6 nuPostMixGainAdjCode] */
    READ_BITS(&value, file, 6, return -1);
    param->mixMetadata.postMixGainCode = value;

    /* [u2 nuControlMixerDRC] */
    READ_BITS(&value, file, 2, return -1);
    param->mixMetadata.drcMixerControlCode = value;

    if (param->mixMetadata.drcMixerControlCode < 3) {
      /* [u3 nuLimit4EmbeddedDRC] */
      READ_BITS(&value, file, 3, return -1);
      param->mixMetadata.limitDRCPriorMix = value;
    }
    else if (param->mixMetadata.drcMixerControlCode == 3) {
      /* [u8 nuCustomDRCCode] */
      READ_BITS(&value, file, 8, return -1);
      param->mixMetadata.customMixDRCCoeffCode = value;
    }

    /* [b1 bEnblPerChMainAudioScale] */
    READ_BITS(&value, file, 1, return -1);
    param->mixMetadata.perMainAudioChSepScal = value;

    for (
      ns = 0;
      ns < staticFieldsParam->mixMetadata.nuNumMixOutConfigs;
      ns++
    ) {
      if (param->mixMetadata.perMainAudioChSepScal) {
        for (
          nCh = 0;
          nCh < staticFieldsParam->mixMetadata.nNumMixOutCh[ns];
          nCh++
        ) {
          /* [u6 nuMainAudioScaleCode[ns][nCh]] // Unique per channel */
          READ_BITS(&value, file, 6, return -1);
          param->mixMetadata.scalingAudioParam[ns][nCh] = value;
        }
      }
      else {
        /* [u6 nuMainAudioScaleCode[ns][0]] // Unique for all channels */
        READ_BITS(&value, file, 6, return -1);
        param->mixMetadata.scalingAudioParam[ns][0] = value;
      }
    }

    /**
     * Preparing Down-mixes properties for mix parameters
     * (Main complete mix is considered as the first one) :
     */
    param->mixMetadata.nbDownMixes = 1;
    param->mixMetadata.nbChPerDownMix[0] = assetStaticFieldsParam->nbChannels;

    if (assetStaticFieldsParam->embeddedSurround6chDownmix) {
      param->mixMetadata.nbChPerDownMix[param->mixMetadata.nbDownMixes] = 6;
      param->mixMetadata.nbDownMixes++;
    }

    if (assetStaticFieldsParam->embeddedStereoDownmix) {
      param->mixMetadata.nbChPerDownMix[param->mixMetadata.nbDownMixes] = 2;
      param->mixMetadata.nbDownMixes++;
    }

    for (ns = 0; ns < staticFieldsParam->mixMetadata.nuNumMixOutConfigs; ns++) {
      /* Mix Configurations loop */
      for (nE = 0; nE < param->mixMetadata.nbDownMixes; nE++) {
        /* Embedded downmix loop */
        for (nCh = 0; nCh < param->mixMetadata.nbChPerDownMix[nE]; nCh++) {
          /* Supplemental Channel loop */

          /* [v<nNumMixOutCh[ns]> nuMixMapMask[ns][nE][nCh]] */
          if (
            readBits(
              file, &value,
              staticFieldsParam->mixMetadata.nNumMixOutCh[ns]
            ) < 0
          )
            return -1;
          param->mixMetadata.mixOutputMappingMask[ns][nE][nCh] = value;

          /* [0 nuNumMixCoefs[ns][nE][nCh]] */
          param->mixMetadata.mixOutputMappingNbCoeff[ns][nE][nCh] =
            _nbBitsSetTo1(param->mixMetadata.mixOutputMappingMask[ns][nE][nCh])
          ;

          for (nC = 0; nC < param->mixMetadata.mixOutputMappingNbCoeff[ns][nE][nCh]; nC++) {
            /* [u6 nuMixCoeffs[ns][nE][nCh][nC]] */
            READ_BITS(&value, file, 6, return -1);
            param->mixMetadata.mixOutputCoefficients[ns][nE][nCh][nC] = value;
          }
        }
      }
    }
  } /* End if (bMixMetadataPresent) */
#endif

  return 0;
}

int decodeDcaExtSSAssetDescriptorDecNavData(
  BitstreamReaderPtr file,
  DcaAudioAssetDescriptorDecoderNavDataParameters * param,
  DcaAudioAssetDescriptorStaticFieldsParameters * assetStaticFieldsParam,
  DcaAudioAssetDescriptorDynamicMetadataParameters * dynMetadataParam,
  DcaExtSSHeaderStaticFieldsParameters * staticFieldsParam,
  size_t nuBits4ExSSFsize,
  int64_t expectedEndOffsetOfAssetDesc
)
{
  uint32_t value;

  unsigned ns, nCh, drcCoeffNb;

  /* [u2 nuCodingMode] */
  READ_BITS(&value, file, 2, return -1);
  param->codingMode = value;

  switch (param->codingMode) {
    case DCA_EXT_SS_CODING_MODE_DTS_HD_COMPONENTS:
      /* DTS-HD component(s). */

      /* [u12 nuCoreExtensionMask] */
      READ_BITS(&value, file, 12, return -1);
      param->codingComponentsUsedMask = value;

      if (
        param->codingComponentsUsedMask
        & DCA_EXT_SS_COD_COMP_EXTSUB_CORE_DCA
      ) {
        /* [u14 nuExSSCoreFsize] */
        READ_BITS(&value, file, 14, return -1);
        param->codingComponents.extSSCore.size = value + 1;

        /* [b1 bExSSCoreSyncPresent] */
        READ_BITS(&value, file, 1, return -1);
        param->codingComponents.extSSCore.syncWordPresent = value;

        if (param->codingComponents.extSSCore.syncWordPresent) {
          /* [u2 nuExSSCoreSyncDistInFrames] */
          READ_BITS(&value, file, 2, return -1);
          param->codingComponents.extSSCore.syncDistanceInFramesCode = value;
          param->codingComponents.extSSCore.syncDistanceInFrames = 1 << value;
        }
      }

      if (param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_XBR) {
        /* [u14 nuExSSXBRFsize] */
        READ_BITS(&value, file, 14, return -1);
        param->codingComponents.extSSXbr.size = value + 1;
      }

      if (param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_XXCH) {
        /* [u14 nuExSSXXCHFsize] */
        READ_BITS(&value, file, 14, return -1);
        param->codingComponents.extSSXxch.size = value + 1;
      }

      if (param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_X96) {
        /* [u12 nuExSSX96Fsize] */
        READ_BITS(&value, file, 12, return -1);
        param->codingComponents.extSSX96.size = value + 1;
      }

      if (param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_LBR) {
        /* [u14 nuExSSLBRFsize] */
        READ_BITS(&value, file, 14, return -1);
        param->codingComponents.extSSLbr.size = value + 1;

        /* [b1 bExSSLBRSyncPresent] */
        READ_BITS(&value, file, 1, return -1);
        param->codingComponents.extSSLbr.syncWordPresent = value;

        if (param->codingComponents.extSSLbr.syncWordPresent) {
          /* [u2 nuExSSLBRSyncDistInFrames] */
          READ_BITS(&value, file, 2, return -1);
          param->codingComponents.extSSLbr.syncDistanceInFramesCode = value;
          param->codingComponents.extSSLbr.syncDistanceInFrames = 1 << value;
        }
      }

      if (param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_XLL) {
        /* [u<nuBits4ExSSFsize> nuExSSXLLFsize] */
        if (readBits(file, &value, nuBits4ExSSFsize) < 0)
          return -1;
        param->codingComponents.extSSXll.size = value + 1;

        /* [b1 bExSSXLLSyncPresent] */
        READ_BITS(&value, file, 1, return -1);
        param->codingComponents.extSSXll.syncWordPresent = value;

        if (param->codingComponents.extSSXll.syncWordPresent) {
          /* [u4 nuPeakBRCntrlBuffSzkB] */
          READ_BITS(&value, file, 4, return -1);
          param->codingComponents.extSSXll.peakBitRateSmoothingBufSize = value << 4;

          /* [u5 nuBitsInitDecDly] */
          READ_BITS(&value, file, 5, return -1);

          /* [u<nuBitsInitDecDly> nuInitLLDecDlyFrames] */
          if (readBits(file, &value, value + 1) < 0)
            return -1;
          param->codingComponents.extSSXll.initialXllDecodingDelayInFrames = value;

          /* [u<nuBits4ExSSFsize> nuExSSXLLSyncOffset] */
          if (readBits(file, &value, nuBits4ExSSFsize) < 0)
            return -1;
          param->codingComponents.extSSXll.nbBytesOffXllSync = value;
        }
      }

      if (param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_RESERVED_1) {
        /* [v16 *Ignore*] */
        READ_BITS(&value, file, 16, return -1);
        param->codingComponents.reservedExtension1_data = value;
      }

      if (param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_RESERVED_2) {
        /* [v16 *Ignore*] */
        READ_BITS(&value, file, 16, return -1);
        param->codingComponents.reservedExtension2_data = value;
      }

      break;

    case DCA_EXT_SS_CODING_MODE_DTS_HD_LOSSLESS_WITHOUT_CORE:
      /* DTS-HD Master Audio without retro-compatible core. */

      param->codingComponentsUsedMask = DCA_EXT_SS_COD_COMP_EXTSUB_XLL;

      /* [u<nuBits4ExSSFsize> nuExSSXLLFsize] */
      if (readBits(file, &value, nuBits4ExSSFsize) < 0)
        return -1;
      param->codingComponents.extSSXll.size = value + 1;

      /* [b1 bExSSXLLSyncPresent] */
      READ_BITS(&value, file, 1, return -1);
      param->codingComponents.extSSXll.syncWordPresent = value;

      if (param->codingComponents.extSSXll.syncWordPresent) {
        /* [u4 nuPeakBRCntrlBuffSzkB] */
        READ_BITS(&value, file, 4, return -1);
        param->codingComponents.extSSXll.peakBitRateSmoothingBufSizeCode = value;
        param->codingComponents.extSSXll.peakBitRateSmoothingBufSize = value << 4;

        /* [u5 nuBitsInitDecDly] */
        READ_BITS(&value, file, 5, return -1);

        /* [u<nuBitsInitDecDly> nuInitLLDecDlyFrames] */
        if (readBits(file, &value, value + 1) < 0)
          return -1;
        param->codingComponents.extSSXll.initialXllDecodingDelayInFrames = value;

        /* [u<nuBits4ExSSFsize> nuExSSXLLSyncOffset] */
        if (readBits(file, &value, nuBits4ExSSFsize) < 0)
          return -1;
        param->codingComponents.extSSXll.nbBytesOffXllSync = value;
      }
      break;

    case DCA_EXT_SS_CODING_MODE_DTS_HD_LOW_BITRATE:
      /* DTS-HD Express. */
      param->codingComponentsUsedMask = DCA_EXT_SS_COD_COMP_EXTSUB_LBR;

      /* [u14 nuExSSLBRFsize] */
      READ_BITS(&value, file, 14, return -1);
      param->codingComponents.extSSLbr.size = value + 1;

      /* [b1 bExSSLBRSyncPresent] */
      READ_BITS(&value, file, 1, return -1);
      param->codingComponents.extSSLbr.syncWordPresent = value;

      if (param->codingComponents.extSSLbr.syncWordPresent) {
        /* [u2 nuExSSLBRSyncDistInFrames] */
        READ_BITS(&value, file, 2, return -1);
        param->codingComponents.extSSLbr.syncDistanceInFramesCode = value;
        param->codingComponents.extSSLbr.syncDistanceInFrames = 1 << value;
      }
      break;

    case DCA_EXT_SS_CODING_MODE_AUXILIARY_CODING:
      /* Auxiliary audio coding. */

      param->codingComponentsUsedMask = 0;

      /* [u14 nuExSSAuxFsize] */
      READ_BITS(&value, file, 14, return -1);
      param->auxilaryCoding.size = value + 1;

      /* [u8 nuAuxCodecId] */
      READ_BITS(&value, file, 8, return -1);
      param->auxilaryCoding.auxCodecId = value;

      /* [b1 bExSSAuxSyncPresent] */
      READ_BITS(&value, file, 1, return -1);
      param->auxilaryCoding.syncWordPresent = value;

      if (param->auxilaryCoding.syncWordPresent) {
        /* [u2 nuExSSAuxSyncDistInFrames] */
        READ_BITS(&value, file, 2, return -1);
        param->auxilaryCoding.syncDistanceInFramesCode = value;
        param->auxilaryCoding.syncDistanceInFrames = 1 << value;
      }
  }

  if (param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_XLL) {
    /* Extension Substream contains DTS-XLL component. */

    /* [u3 nuDTSHDStreamID] */
    READ_BITS(&value, file, 3, return -1);
    param->codingComponents.extSSXll.steamId = value;
  }

  if (
    assetStaticFieldsParam->directSpeakersFeed
    && NULL != staticFieldsParam
    && staticFieldsParam->mixMetadataEnabled
    && !dynMetadataParam->mixMetadataPresent
  ) {
    /* [b1 bOnetoOneMixingFlag] */
    READ_BITS(&value, file, 1, return -1);
    param->mixMetadata.oneTrackToOneChannelMix = value;
    param->mixMetadataFieldsPresent = true;
  }
  else {
    param->mixMetadata.oneTrackToOneChannelMix = false;
    param->mixMetadataFieldsPresent = false;
  }

  if (param->mixMetadata.oneTrackToOneChannelMix) {
    /* [b1 bEnblPerChMainAudioScale] */
    READ_BITS(&value, file, 1, return -1);
    param->mixMetadata.perChannelMainAudioScaleCode = value;

    for (
      ns = 0;
      ns < staticFieldsParam->mixMetadata.nuNumMixOutConfigs;
      ns++
    ) {

      if (param->mixMetadata.perChannelMainAudioScaleCode) {
        for (
          nCh = 0;
          nCh < staticFieldsParam->mixMetadata.nNumMixOutCh[ns];
          nCh++
        ) {
          /* [u6 nuMainAudioScaleCode[mixConfigIdx][channelIdx]] */
          READ_BITS(&value, file, 6, return -1);
          param->mixMetadata.mainAudioScaleCodes[ns][nCh] = value;
        }
      }
      else {
        /* [u6 nuMainAudioScaleCode[mixConfigIdx][0]] */
        READ_BITS(&value, file, 6, return -1);
        param->mixMetadata.mainAudioScaleCodes[ns][0] = value;
      }
    }
  }

  /* [b1 bDecodeAssetInSecondaryDecoder] */
  READ_BITS(&value, file, 1, return -1);
  param->decodeInSecondaryDecoder = value;

#if DCA_EXT_SS_ENABLE_DRC_2
  param->extraDataPresent = (
    0 < expectedEndOffsetOfAssetDesc - tellBinaryPos(file)
  );

  if (param->extraDataPresent) {
    /* [b1 bDRCMetadataRev2Present] */
    READ_BITS(&value, file, 1, return -1);
    param->drcRev2Present = value;

    if (param->drcRev2Present) {
      /* [u4 DRCversion_Rev2] */
      READ_BITS(&value, file, 4, return -1);
      param->drcRev2.Hdr_Version = value;

      if (param->drcRev2.Hdr_Version == DCA_EXT_SS_DRC_REV_2_VERSION_1) {
        /* [0 nRev2_DRCs] */
        drcCoeffNb = staticFieldsParam->frameDurationCodeValue / 256;

        /* [u<8*nRev2_DRCs> DRCCoeff_Rev2[subsubFrame]] */
        if (skipBytes(file, drcCoeffNb))
          return -1;
      }
    }
  }
#else
  param->extraDataPresent = false;
  (void) drcCoeffNb;
#endif

  return 0;
}

int decodeDcaExtSSAssetDescriptor(
  BitstreamReaderPtr file,
  DcaAudioAssetDescriptorParameters * param,
  DcaExtSSHeaderStaticFieldsParameters * staticFieldsParam,
  size_t nuBits4ExSSFsize
)
{
  uint32_t value;

  int64_t startOff, endOff, remainingBits;
  bool save_reserved_field;

  startOff = tellBinaryPos(file);

  /* -- Main Asset Parameters Section (Size, Index and Per Stream Static Metadata) -- */
  /* [u9 nuAssetDescriptFsize] */
  READ_BITS(&value, file, 9, return -1);
  param->descriptorLength = value + 1;

  endOff = startOff + (param->descriptorLength * 8);

  /* [u3 nuAssetIndex] */
  READ_BITS(&value, file, 3, return -1);
  param->assetIndex = value;

  param->staticFieldsPresent = (NULL != staticFieldsParam);

  if (param->staticFieldsPresent) {
    if (
      decodeDcaExtSSAssetDescriptorStaticFields(
        file,
        &param->staticFields
      ) < 0
    )
      return -1;
  }
  else
    param->staticFields.directSpeakersFeed = false;

  /* -- Dynamic Metadata Section (DRC, DNC and Mixing Metadata) -- */
  if (
    decodeDcaExtSSAssetDescriptorDynamicMetadata(
      file,
      &param->dynMetadata, &param->staticFields, staticFieldsParam
    ) < 0
  )
    return -1;

  /* -- Decoder Navigation Data Section (Coding mode...) -- */
  if (
    decodeDcaExtSSAssetDescriptorDecNavData(
      file,
      &param->decNavData,
      &param->staticFields,
      &param->dynMetadata,
      staticFieldsParam,
      nuBits4ExSSFsize,
      endOff
    ) < 0
  )
    return -1;

  /* Compute expected header end offset */
  remainingBits = endOff - tellBinaryPos(file);

  if (remainingBits < 0)
    LIBBLU_DTS_ERROR_RETURN(
      "Unexpected DCA Extension substream asset descriptor length "
      "(expected: %zu bits, %zu bits parsed).\n",
      param->descriptorLength * 8,
      tellBinaryPos(file) - startOff
    );

#if 1
  /* Compute reserved fields size (and determine if these are saved or not). */
  param->resFieldLength = remainingBits / 8;
  param->paddingBits = remainingBits & 0x7; /* % 8 */
  save_reserved_field = (
    remainingBits <= 8 * DCA_EXT_SS_MAX_SUPP_RES_FIELD_SIZE
  );

  /* [v<nuExtSSHeaderSize - tellPos() - 16> Reserved] */
  if (0 < param->resFieldLength) {
    if (save_reserved_field) {
      int ret = readBytes(file, param->resFieldData, param->resFieldLength);
      if (ret < 0)
        return -1;
    }
    else {
      if (skipBytes(file, param->resFieldLength))
        return -1;
    }
  }

  /* [v0..7 ByteAlign] */
  if (save_reserved_field) {
    if (readBits(file, &value, param->paddingBits) < 0)
      return -1;
    param->resFieldData[param->resFieldLength] = (uint8_t) value;
  }
  else
    SKIP_BITS(file, param->paddingBits, return -1);
#endif

  return 0;
}

int decodeDcaExtSSHeader(
  BitstreamReaderPtr file,
  DcaExtSSHeaderParameters * param
)
{
  uint32_t value;

  int64_t startOff, endOff, remainingBits;
  unsigned nAst;

  DcaExtSSHeaderStaticFieldsParameters * staticFieldsParam;
  size_t headerLengthFieldNbBits;
  size_t extSSFrameSizeFieldNbBits;
  uint16_t extSSCrcResult;
  bool save_reserved_field;

  assert(NULL != file);
  assert(NULL != param);

  staticFieldsParam = &param->staticFields;

  startOff = tellBinaryPos(file);

  /* [u32 SYNCEXTSSH] // 0x64582025 */
  uint32_t SYNCEXTSSH;
  READ_BITS(&SYNCEXTSSH, file, 32, return -1);
  assert(DCA_SYNCEXTSSH == SYNCEXTSSH);

  /* [v8 UserDefinedBits] */
  READ_BITS(&value, file, 8, return -1);
  param->userDefinedBits = value;

#if !DCA_EXT_SS_DISABLE_CRC
  initCrcContext(crcCtxBitstream(file), DCA_EXT_SS_CRC_PARAM(), DCA_EXT_SS_CRC_INITIAL_V);
#endif

  /* [u2 nExtSSIndex] */
  READ_BITS(&value, file, 2, return -1);
  param->extSSIdx = value;

  /** [u2 bHeaderSizeType]
   * nuBits4Header = (bHeaderSizeType) ? 8 : 12;
   * nuBits4ExSSFsize = (bHeaderSizeType) ? 16 : 20;
   */
  READ_BITS(&value, file, 1, return -1);
  param->longHeaderSizeFlag = value;
  headerLengthFieldNbBits = (param->longHeaderSizeFlag) ? 12 : 8;
  extSSFrameSizeFieldNbBits = (param->longHeaderSizeFlag) ? 20 : 16;

  /* [u<nuBits4Header> nuExtSSHeaderSize] */
  if (readBits(file, &value, headerLengthFieldNbBits) < 0)
    return -1;
  param->extensionSubstreamHeaderLength = value + 1;

  /* [u<extSubFrmLengthFieldSize> nuExtSSFsize] */
  if (readBits(file, &value, extSSFrameSizeFieldNbBits) < 0)
    return -1;
  param->extensionSubstreamFrameLength = value + 1;

  /* [b1 bStaticFieldsPresent] */
  READ_BITS(&value, file, 1, return -1);
  param->staticFieldsPresent = value;

  if (param->staticFieldsPresent) {
    /* [vn StaticFields] */
    if (
      decodeDcaExtSSHeaderStaticFields(
        file, staticFieldsParam, param->extSSIdx
      ) < 0
    )
      return -1;
  }
  else {
    staticFieldsParam->nbAudioPresentations = 1;
    staticFieldsParam->nbAudioAssets = 1;
  }

  for (nAst = 0; nAst < staticFieldsParam->nbAudioAssets; nAst++) {
    /* [u<nuBits4ExSSFsize> nuAssetFsize[nAst]] */
    if (readBits(file, &value, extSSFrameSizeFieldNbBits) < 0)
      return -1;
    param->audioAssetsLengths[nAst] = value + 1;
  }

  for (nAst = 0; nAst < staticFieldsParam->nbAudioAssets; nAst++) {
    /* [vn AssetDecriptor()] */
    if (
      decodeDcaExtSSAssetDescriptor(
        file,
        param->audioAssets + nAst,
        (param->staticFieldsPresent) ? staticFieldsParam : NULL,
        extSSFrameSizeFieldNbBits
      ) < 0
    )
      return -1;
  }

  for (nAst = 0; nAst < staticFieldsParam->nbAudioAssets; nAst++) {
    /* [b1 bBcCorePresent[nAst]] */
    READ_BITS(&value, file, 1, return -1);

    param->audioAssetBckwdCompCorePresent[nAst] = value;
  }

  for (nAst = 0; nAst < staticFieldsParam->nbAudioAssets; nAst++) {
    if (param->audioAssetBckwdCompCorePresent[nAst]) {
      /* [u2 nuBcCoreExtSSIndex[nAst]] */
      READ_BITS(&value, file, 2, return -1);
      param->audioAssetBckwdCompCore[nAst].extSSIndex = value;

      /* [u3 nuBcCoreAssetIndex[nAst]] */
      READ_BITS(&value, file, 3, return -1);
      param->audioAssetBckwdCompCore[nAst].assetIndex = value;
    }
  }

  /* Compute expected header end offset */
  endOff = startOff + (param->extensionSubstreamHeaderLength * 8);
  remainingBits = endOff - tellBinaryPos(file) - 16;

  if (remainingBits < 0) {
    lbc_printf("LENGTH: 0x%" PRIX64".\n", param->extensionSubstreamHeaderLength);
    LIBBLU_DTS_ERROR_RETURN(
      "Unexpected DCA Extension Substream header length.\n"
    );
  }



  /* Compute reserved fields size (and determine if these are saved or not). */
  param->resFieldLength = remainingBits / 8;
  param->paddingBits    = remainingBits & 0x7; /* % 8 */
  save_reserved_field = (
    remainingBits <= 8 * DCA_EXT_SS_MAX_SUPP_RES_FIELD_SIZE
  );

  /* [v<nuExtSSHeaderSize - tellPos() - 16> Reserved] */
  if (0 < param->resFieldLength) {
    if (save_reserved_field) {
      int ret = readBytes(file, param->resFieldData, param->resFieldLength);
      if (ret < 0)
        return -1;
    }
    else {
      if (skipBytes(file, param->resFieldLength))
        return -1;
    }
  }

  /* [v0..7 ByteAlign] */
  if (save_reserved_field) {
    READ_BITS(
      &param->resFieldData[param->resFieldLength],
      file, param->paddingBits, return -1
    );
  }
  else
    SKIP_BITS(file, param->paddingBits, return -1);

#if !DCA_EXT_SS_DISABLE_CRC
  extSSCrcResult = completeCrcContext(crcCtxBitstream(file));
#endif

  /* [u16 nCRC16ExtSSHeader] */
  READ_BITS(&value, file, 16, return -1);
  param->HCRC = value;

#if !DCA_EXT_SS_DISABLE_CRC
  if (param->HCRC != bswap16(extSSCrcResult))
    LIBBLU_DTS_ERROR_RETURN(
      "DCA Extension Substream CRC check failure.\n"
    );
#endif

  return 0;
}

int decodeDcaExtSubAsset(
  BitstreamReaderPtr dtsInput,
  DtsXllFrameContextPtr * xllCtx,
  DcaAudioAssetDescriptorParameters assetParam,
  DcaExtSSHeaderParameters extSSParam,
  size_t assetLength
)
{

  switch (assetParam.decNavData.codingMode) {
    case DCA_EXT_SS_CODING_MODE_DTS_HD_COMPONENTS:
    case DCA_EXT_SS_CODING_MODE_DTS_HD_LOSSLESS_WITHOUT_CORE:
    case DCA_EXT_SS_CODING_MODE_DTS_HD_LOW_BITRATE:
      if (assetParam.decNavData.codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_CORE_DCA)
        return 0;
      if (assetParam.decNavData.codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_XBR)
        return 0;
      if (assetParam.decNavData.codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_XXCH)
        return 0;
      if (assetParam.decNavData.codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_X96)
        return 0;
      if (assetParam.decNavData.codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_LBR)
        return 0;

      if (assetParam.decNavData.codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_XLL) {
        /* Initialize XLL context */
        if (initDtsXllFrameContext(xllCtx, assetParam, extSSParam) < 0)
          return -1;

        /* Parse XLL PBR period and decode it if it is ready */
        if (
          parseDtsXllFrame(
            dtsInput, *xllCtx,
            assetParam,
            assetLength
          ) < 0
        )
          return -1;
      }

      break;

    default:
      break;
  }

  return 0;
}

static void updateDcaExtSSHeaderXllParam(
  DcaExtSSHeaderParameters * param,
  unsigned assetId,
  bool syncWordPresent,
  unsigned decodingDelay,
  size_t syncWordOffset,
  size_t pbrSmoothingBufSize,
  size_t assetSize
)
{
  DcaAudioAssetSSXllParameters * xllParam;

  param->audioAssetsLengths[assetId] = assetSize;
  param->longHeaderSizeFlag = true;

  xllParam = &param->audioAssets[assetId].decNavData.codingComponents.extSSXll;
  xllParam->size = assetSize;
  xllParam->syncWordPresent = syncWordPresent;
  xllParam->initialXllDecodingDelayInFrames = decodingDelay;
  xllParam->nbBytesOffXllSync = syncWordOffset;
  xllParam->peakBitRateSmoothingBufSize = pbrSmoothingBufSize;
}

int patchDcaExtSSHeader(
  DtsContext * ctx,
  DcaExtSSHeaderParameters param,
  unsigned xllAssetId,
  DcaXllFrameSFPosition * assetContentOffsets
)
{
  size_t targetFrameSize;
  int ret;
#if 1
  ret = getFrameTargetSizeDtsXllPbr(
    ctx->xllCtx,
    ctx->extSS.nbFrames,
    &targetFrameSize
  );
  if (ret < 0)
    return -1;
#else
  targetFrameSize = param.audioAssetsLengths[xllAssetId];
#endif

  unsigned PBRSmoothingBufSizeKb = getPBRSmoothingBufSizeDtshdFileHandler(
    &ctx->dtshdFileHandle
  );
  if (0 == PBRSmoothingBufSizeKb)
    LIBBLU_DTS_ERROR_RETURN(
      "Unable to set DTS PBR smoothing buffer size, "
      "missing information in DTS-HD header.\n"
    );

  DcaXllFrameSFPosition buildedFrameContent;
  int syncWordOffEntryIndex;
  unsigned syncDecodingDelay;
  ret = substractPbrFrameSliceDtsXllFrameContext(
    ctx->xllCtx,
    targetFrameSize,
    &buildedFrameContent,
    &syncWordOffEntryIndex,
    &syncDecodingDelay
  );
  if (ret < 0)
    return -1;

  bool syncWordPresent = (0 <= syncWordOffEntryIndex);
  int64_t syncWordOff = 0;
  unsigned decodingDelay = 0;

  if (syncWordPresent) {
    ret = getRelativeOffsetDcaXllFrameSFPosition(
      buildedFrameContent,
      buildedFrameContent.sourceOffsets[syncWordOffEntryIndex].off,
      &syncWordOff
    );
    assert(0 <= ret);

    decodingDelay = syncDecodingDelay;
  }

  if (NULL != assetContentOffsets)
    *assetContentOffsets = buildedFrameContent;

#if 0
  lbc_printf(
    "Frame %u size: %zu bytes.\n",
    ctx->extSS.nbFrames,
    targetFrameSize
  );
  printDcaXllFrameSFPositionIndexes(
    buildedFrameContent,
    2
  );
  lbc_printf("  Sync word: %s.\n", BOOL_PRESENCE(syncWordPresent));
  if (syncWordPresent) {
    lbc_printf("   -> Offset: %" PRId64 " byte(s).\n", syncWordOff);
    lbc_printf(
      "   -> Offset (ref): %zu byte(s).\n",
      param.audioAssets[xllAssetId].decNavData.codingComponents.extSSXll.nbBytesOffXllSync
    );
    lbc_printf("   -> Delay: %u frame(s).\n", decodingDelay);
    lbc_printf(
      "   -> Delay (ref): %u frame(s).\n",
      param.audioAssets[xllAssetId].decNavData.codingComponents.extSSXll.initialXllDecodingDelayInFrames
    );

    if (decodingDelay >= 2)
      exit(0);
  }
#endif

  updateDcaExtSSHeaderXllParam(
    &param,
    xllAssetId,
    syncWordPresent,
    decodingDelay,
    (size_t) syncWordOff,
    PBRSmoothingBufSizeKb,
    targetFrameSize
  );

  return replaceCurDtsAUCell(
    &ctx->curAccessUnit,
    DTS_AU_INNER_EXT_SS_HDR(param)
  );
}

int decodeDcaExtSS(
  DtsContext * ctx
)
{
  int ret;

  DcaExtSSFrameParameters extFrame;
  int64_t startOff, assetLength, assetStartOff, assetReadedLength;

  unsigned nAst; /* Audio Asset ID */

  bool ignoreFrame, decodeAssets;

  DcaAudioAssetDescriptorParameters * asset;

  const unsigned xllAssetId = 0x0; /* DTS XLL only allowed in first asset */
  DcaXllFrameSFPosition xllAssetOffsets;
  bool updateAssetContent;

  DcaExtSSFrameParameters * cur_frame;
  uint8_t extSSIdx;

  lbc frameClockTime[STRT_H_M_S_MS_LEN];

  DtsAUCellPtr cell;

  assert(NULL != ctx);

  startOff = tellPos(ctx->bs);
  str_time(
    frameClockTime,
    STRT_H_M_S_MS_LEN, STRT_H_M_S_MS,
    (ctx->extSSPresent) ? ctx->extSS.pts : 0
  );

  LIBBLU_DTS_DEBUG(
    "0x%08" PRIX64 " === DCA Extension Substream Frame %u - %" PRI_LBCS " ===\n",
    startOff,
    (ctx->extSSPresent) ? ctx->extSS.nbFrames : 0,
    frameClockTime
  );

  ignoreFrame = ctx->skipExtensionSubstreams || ctx->skipCurrentPeriod;

  if (decodeDcaExtSSHeader(ctx->bs, &extFrame.header) < 0)
    return -1;

  if (!ignoreFrame) {
    if (!ctx->extSSPresent) {
      ctx->extSS = INIT_DTS_DCA_EXT_SS_CTX();
      memset(
        &ctx->extSS.presentIndexes,
        false,
        DCA_EXT_SS_MAX_NB_INDEXES * sizeof(bool)
      );
      ctx->extSSPresent = true;
    }

    if (
      checkDcaExtSSHeaderCompliance(
        &extFrame.header,
        ctx->isSecondaryStream,
        &ctx->extSS.warningFlags
      ) < 0
    )
      return -1;

    extSSIdx = extFrame.header.extSSIdx;

    if (!ctx->extSS.presentIndexes[extSSIdx]) {
      assert(!ctx->extSS.presentIndexes[extSSIdx]);

      cur_frame = (DcaExtSSFrameParameters *) malloc(
        sizeof(DcaExtSSFrameParameters)
      );
      if (NULL == cur_frame)
        LIBBLU_DTS_ERROR_RETURN(
          "Memory allocation error.\n"
        );

      ctx->extSS.currentExtSSIdx = extSSIdx;
      ctx->extSS.curFrames[extSSIdx] = cur_frame;
      ctx->extSS.presentIndexes[extSSIdx] = true;
    }

#if 0
    else {
      cur_frame = ctx->extSS.curFrames[extSSIdx];

      if (
        checkDcaExtSSHeaderChangeCompliance(
          cur_frame->header,
          extFrame.header,
          &ctx->extSS.warningFlags
        ) < 0
      )
        return -1;
    }
#endif

    /* Frame content is always updated */
    *(ctx->extSS.curFrames[extSSIdx]) = extFrame;

    if (NULL == (cell = recoverCurDtsAUCell(&ctx->curAccessUnit)))
      return -1;
    cell->length = tellPos(ctx->bs) - startOff;

    if (isFast2nbPassDtsContext(ctx)) {
      ret = patchDcaExtSSHeader(
        ctx, extFrame.header, xllAssetId, &xllAssetOffsets
      );
      if (ret < 0)
        return -1;
    }

    if (addCurCellToDtsAUFrame(&ctx->curAccessUnit) < 0)
      return -1;

    ctx->extSS.nbFrames++;
  }
  else {
    LIBBLU_DTS_DEBUG_EXTSS(" *Not checked, skipped*\n");
    if (discardCurDtsAUCell(&ctx->curAccessUnit) < 0)
      return -1;
  }

  /* Decode extension substream assets */
  decodeAssets =
    !ignoreFrame
    && !isFast2nbPassDtsContext(ctx)
    && (
      isInitPbrFileHandler()
      || DTS_XLL_FORCE_UNPACKING
      || DTS_XLL_FORCE_REBUILD_SEIING
    )
  ;

  for (nAst = 0; nAst < extFrame.header.staticFields.nbAudioAssets; nAst++) {
    assetStartOff = tellPos(ctx->bs);
    assetLength = extFrame.header.audioAssetsLengths[nAst];
    asset = extFrame.header.audioAssets + nAst;

    LIBBLU_DTS_DEBUG_EXTSS(
      "0x%08" PRIX64 " === DTS Extension Substream Component %u ===\n",
      assetStartOff, nAst
    );

    if (!ignoreFrame) {
      /* Frame content saving */
      if (!ctx->extSS.content.parsedParameters) {
        if (asset->staticFieldsPresent) {
          ctx->extSS.content.nbChannels = asset->staticFields.nbChannels;
          ctx->extSS.content.audioFreq = asset->staticFields.maxSampleRate;
          ctx->extSS.content.bitDepth = asset->staticFields.bitDepth;
          ctx->extSS.content.parsedParameters = true;
        }
      }

      if (asset->decNavData.codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_XLL) {
        if (nAst != xllAssetId) {
          /* Avoid bugs with single DTS XLL context */
          LIBBLU_DTS_ERROR_RETURN(
            "Unsupported multiple or not in the first index "
            "DTS XLL audio assets.\n"
          );
        }
        ctx->extSS.content.xllExtSS = true;
      }
      if (asset->decNavData.codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_LBR)
        ctx->extSS.content.lbrExtSS = true;
      if (asset->decNavData.codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_XBR)
        ctx->extSS.content.xbrExtSS = true;

      /* Asset Access Unit cell */
      cell = initDtsAUCell(&ctx->curAccessUnit, DTS_FRM_INNER_EXT_SS_ASSET);
      if (NULL == cell)
        return -1;
      cell->startOffset = assetStartOff;
      cell->length = assetLength;

      if (isFast2nbPassDtsContext(ctx) && ctx->extSS.content.xllExtSS) {
        /* Replace if required frame content to apply PBR smoothing. */
        updateAssetContent = (
          xllAssetOffsets.nbSourceOffsets != 1
          || xllAssetOffsets.sourceOffsets[0].off != assetStartOff
        );

        if (updateAssetContent) {
          /**
           * Update required because content offsets have been changed
           * by PBR process.
           */
          ret = replaceCurDtsAUCell(
            &ctx->curAccessUnit,
            DTS_AU_INNER_EXT_SS_ASSET(xllAssetOffsets)
          );
        }
      }
    }

    if (decodeAssets) {
      if (decodeDcaExtSubAsset(ctx->bs, &ctx->xllCtx, *asset, extFrame.header, assetLength) < 0)
        return -1;

      assetReadedLength = tellPos(ctx->bs) - assetStartOff;
    }
    else
      assetReadedLength = 0;

    if (skipBytes(ctx->bs, assetLength - assetReadedLength) < 0)
      return -1;

    if (!ignoreFrame) {
      if (addCurCellToDtsAUFrame(&ctx->curAccessUnit) < 0)
        return -1;
    }
  }

  /* Check Extension Substream Size : */
  if (
    !isFast2nbPassDtsContext(ctx)
    && (
      tellPos(ctx->bs) - startOff
      != extFrame.header.extensionSubstreamFrameLength
    )
  )
    LIBBLU_DTS_ERROR_RETURN(
      "Unexpected Extension substream frame length "
      "(parsed length: %zu bytes, expected %zu bytes).\n",
      tellPos(ctx->bs) - startOff,
      extFrame.header.extensionSubstreamFrameLength
    );

  /* lbc_printf("%" PRId64 "\n", extFrame.header.extensionSubstreamFrameLength); */

  return 0;
}

int parseDts(
  DtsContext * ctx
)
{

  while (!isEof(ctx->bs)) {
    /* Progress bar : */
    printFileParsingProgressionBar(ctx->bs);

    if (isDtshdFileDtsContext(ctx)) {
      /* Read DTS-HD file chunks : */
      switch (parseDtshdChunk(ctx)) {
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
        if (decodeDcaCoreSS(ctx) < 0)
          return -1;
        break;

      case DTS_FRAME_INIT_EXT_SUBSTREAM:
        /* DTS Coherent Acoustics Extension Substream */
        if (decodeDcaExtSS(ctx) < 0)
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
      ctx->dtshdFileHandle.off_STRMDATA += frame_size;
  }

  return 0;
}

int analyzeDts(
  LibbluESParsingSettings * settings
)
{
  DtsContext ctx;
  if (createDtsContext(&ctx, settings) < 0)
    return -1;

  if (NULL != settings->options.pbrFilepath) {
    if (initPbrFileHandler(settings->options.pbrFilepath) < 0)
      return -1;
  }

  do {
    if (initParsingDtsContext(&ctx))
      goto free_return;

    if (isFast2nbPassDtsContext(&ctx)) {
      if (NULL == ctx.xllCtx)
        LIBBLU_ERROR_FRETURN(
          "PBR smoothing is only available for DTS XLL streams.\n"
        );

      /* Compute PBR Smoothing resulting frame sizes */
      if (performComputationDtsXllPbr(ctx.xllCtx) < 0)
        return -1;

      ctx.core.nb_frames = 0;
      ctx.extSS.nbFrames = 0;
    }

    if (parseDts(&ctx) < 0)
      goto free_return;
  } while (nextPassDtsContext(&ctx));

  lbc_printf(" === Parsing finished with success. ===              \n");


  int ret = completeDtsContext(&ctx);
  releasePbrFileHandler();
  return ret;

free_return:
  cleanDtsContext(&ctx);
  releasePbrFileHandler();
  return -1;
}
