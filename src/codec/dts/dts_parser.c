#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include "dts_parser.h"

/** \~english
 * \brief Return the number of bits set to 0b1 in given mask.
 *
 * \param mask Binary mask to count bits from.
 * \return unsigned Number of bits set to 0b1 in given mask.
 *
 * E.g. nbBitsSetTo1(0x8046) == 4.
 */
static unsigned nbBitsSetTo1(uint16_t mask)
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
  DtsContextPtr ctx
)
{
  int ret;

  DtshdFileHandlerPtr handle = ctx->dtshdFileHandle;

  ret = decodeDtshdFileChunk(
    ctx->file,
    handle,
    DTS_CTX_FAST_2ND_PASS(ctx)
  );

  if (0 == ret) {
    /* Successfully decoded chunk, collect parameters : */

    if (!handle->headerPresent)
      LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
        "Expect DTSHDHDR to be the first chunk of DTSHD formatted file.\n"
      );

    if (
      handle->headerPresent
      && handle->audioPresHeaderMetadataPresent
    ) {
      if (
        !handle->warningFlags.missingRequiredPbrFile
        && !handle->header.peakBitRateSmoothingPerformed
        && handle->audioPresHeaderMetadata.losslessComponentPresent
        && !isInitPbrFileHandler()
      ) {
        LIBBLU_WARNING(
          "Missing PBR file in parameters, "
          "unable to process PBR smoothing as request by input file.\n"
        );
        handle->warningFlags.missingRequiredPbrFile = true;
      }
    }

    getDtshdInitialDelay(
      ctx->dtshdFileHandle,
      &ctx->skippedFramesControl
    );
  }

  return ret;
}

int decodeDcaCoreSSFrameHeader(
  BitstreamReaderPtr file,
  DcaCoreSSFrameHeaderParameters * param
)
{
  int64_t frameStartOffset;
  uint32_t value;

  assert(NULL != file);
  assert(NULL != param);

  frameStartOffset = tellPos(file);

  /* [u32 SYNC] // 0x7FFE8001 */
  if (readValueBigEndian(file, 4, &value) < 0)
    return -1;
  if (value != DTS_SYNCWORD_CORE)
    LIBBLU_DTS_ERROR_RETURN(
      "Unexpected non-DCA Core Substream sync word (0x%04" PRIX16 ").\n",
      value
    );

  /* [b1 FTYPE] */
  if (readBits(file, &value, 1) < 0)
    return -1;
  param->terminationFrame = value & 0x1;

  /* [u5 SHORT] */
  if (readBits(file, &value, 5) < 0)
    return -1;
  param->samplesPerBlock = (value & 0x1F) + 1;

  /* [b1 CPF] */
  if (readBits(file, &value, 1) < 0)
    return -1;
  param->crcPresent = value & 0x1;

  /* [u7 NBLKS] */
  if (readBits(file, &value, 7) < 0)
    return -1;
  param->blocksPerChannel = (value & 0x7F) + 1;

  /* [u14 FSIZE] */
  if (readBits(file, &value, 14) < 0)
    return -1;
  param->frameLength = (value & 0x3FFF) + 1;

  /* [u6 AMODE] */
  if (readBits(file, &value, 6) < 0)
    return -1;
  param->audioChannelArrangement = value & 0x3F;

  /* [u4 SFREQ] */
  if (readBits(file, &value, 4) < 0)
    return -1;
  param->audioFreqCode = value & 0xF;

  /* [u5 RATE] */
  if (readBits(file, &value, 5) < 0)
    return -1;
  param->bitRateCode = value & 0x1F;
  param->bitrate = dtsBitRateCodeValue(value);

  /* [v1 FixedBit] */
  if (readBits(file, &value, 1) < 0)
    return -1;
  if (value & 0x1)
    LIBBLU_DTS_ERROR_RETURN(
      "Reserved value in use (DCA FixedBit == 0b%x).\n",
      value
    );

  /* [b1 DYNF] */
  if (readBits(file, &value, 1) < 0)
    return -1;
  param->embeddedDynRange = value & 0x1;

  /* [b1 TIMEF] */
  if (readBits(file, &value, 1) < 0)
    return -1;
  param->embeddedTimestamp = value & 0x1;

  /* [b1 AUXF] */
  if (readBits(file, &value, 1) < 0)
    return -1;
  param->auxData = value & 0x1;

  /* [b1 HDCD] */
  if (readBits(file, &value, 1) < 0)
    return -1;
  param->hdcd = value & 0x1;

  /* [u3 EXT_AUDIO_ID] */
  if (readBits(file, &value, 3) < 0)
    return -1;
  param->extAudioId = value & 0x7;

  /* [b1 EXT_AUDIO] */
  if (readBits(file, &value, 1) < 0)
    return -1;
  param->extAudio = value & 0x1;

  /* [b1 ASPF] */
  if (readBits(file, &value, 1) < 0)
    return -1;
  param->syncWordInsertionLevel = value & 0x1;

  /* [u2 LFF] */
  if (readBits(file, &value, 2) < 0)
    return -1;
  param->lfePresent = value & 0x3;

  /* [b1 HFLAG] */
  if (readBits(file, &value, 1) < 0)
    return -1;
  param->predHist = value & 0x1;

  if (param->crcPresent) {
    /* [u16 HCRC] */
    if (readBits(file, &value, 16) < 0)
      return -1;
    param->crcValue = value & 0xFFFF;
  }
  else
    param->crcValue = 0;

  /* [b1 FILTS] */
  if (readBits(file, &value, 1) < 0)
    return -1;
  param->multiRtInterpolatorSwtch = value & 0x1;

  /* [u4 VERNUM] */
  if (readBits(file, &value, 4) < 0)
    return -1;
  param->syntaxCode = value & 0xF;

  if (DTS_MAX_SYNTAX_VERNUM < param->syntaxCode)
    LIBBLU_DTS_ERROR_RETURN(
      "Incompatible DCA Encoder Software Revision code (0x%" PRIX8 ").\n",
      param->syntaxCode
    );

  /* [u2 CHIST] */
  if (readBits(file, &value, 2) < 0)
    return -1;
  param->copyHistory = value & 0x3;

  /* [v3 PCMR] */
  if (readBits(file, &value, 3) < 0)
    return -1;
  param->sourcePcmResCode = value & 0x7;

  /* [b1 SUMF] */
  if (readBits(file, &value, 1) < 0)
    return -1;
  param->frontSum = value & 0x1;

  /* [b1 SUMS] */
  if (readBits(file, &value, 1) < 0)
    return -1;
  param->surroundSum = value & 0x1;

  if (param->syntaxCode == 0x6 || param->syntaxCode == 0x7) {
    /* [u4 DIALNORM / DNG] */
    if (readBits(file, &value, 4) < 0)
      return -1;
    param->dialNormCode = value & 0xF;
  }
  else {
    /* [v4 UNSPEC] */
    if (skipBits(file, 4) < 0)
      return -1;
    param->dialNormCode = 0x0;
  }

  if (paddingByte(file) < 0)
    return -1;

  /* Compute / Fetch additionnal paramters */
  param->nbChannels = dtsCoreAudioChannelAssignCodeToNbCh(
    param->audioChannelArrangement
  );
  param->audioFreq = dtsCoreAudioSampFreqCodeValue(
    param->audioFreqCode
  );
  param->bitDepth = dtsCoreSourcePcmResCodeValue(
    param->sourcePcmResCode, &param->isEs
  );
  param->bitrate = dtsBitRateCodeValue(
    param->bitRateCode
  );
  if (param->syntaxCode == 0x6)
    param->dialNorm = - (16 + ((int) param->dialNormCode));
  else
    param->dialNorm = - ((int) param->dialNormCode);
  param->size = tellPos(file) - frameStartOffset;

  return 0;
}

int decodeDcaCoreSS(
  DtsContextPtr ctx
)
{
  bool constantSyncFrame;

  DcaCoreSSFrameParameters syncFrame;

  lbc frameClockTime[STRT_H_M_S_MS_LEN];

  int64_t startOff;
  DtsAUCellPtr cell;

  assert(NULL != ctx);

  str_time(
    frameClockTime,
    STRT_H_M_S_MS_LEN, STRT_H_M_S_MS,
    (ctx->corePresent) ? ctx->core.frameDts : 0
  );

  startOff = tellPos(ctx->file);
  LIBBLU_DTS_DEBUG(
    "0x%08" PRIX64 " === DCA Core Substream Frame %u - %" PRI_LBCS " ===\n",
    startOff,
    (ctx->corePresent) ? ctx->core.nbFrames : 0,
    frameClockTime
  );

  if (decodeDcaCoreSSFrameHeader(ctx->file, &syncFrame.header) < 0)
    return -1;

  /* [vn payload] */
  syncFrame.payloadSize = syncFrame.header.frameLength - syncFrame.header.size;
  if (syncFrame.payloadSize < 0)
    LIBBLU_DTS_ERROR_RETURN(
      "Unexpected negative Core Substream payload size (%" PRId64 " bytes).\n",
      syncFrame.payloadSize
    );

  if (skipBytes(ctx->file, (size_t) syncFrame.payloadSize) < 0)
    return -1;

  /* Skip frame if context asks */
  if ((ctx->skipCurrentPeriod = (0 < ctx->skippedFramesControl))) {
    if (discardCurDtsAUCell(ctx->curAccessUnit) < 0)
      return -1;
    ctx->skippedFramesControl--;
    return 0;
  }

  if (!ctx->corePresent) {
    /* if (!IS_ENTRY_POINT_DCA_CORE_SS(syncFrame))
      LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
        "Expect first Core frame to be a valid entry point "
        "(expect Predictor History to be switch off).\n"
      ); */

    ctx->core = INIT_DTS_DCA_CORE_SS_CTX();

    /* First Sync Frame */
    if (checkDcaCoreSSCompliance(syncFrame, &ctx->core.warningFlags) < 0)
      return -1;

    ctx->core.curFrame = syncFrame;
    ctx->corePresent = true;
  }
  else if (!DTS_CTX_FAST_2ND_PASS(ctx)) {
    constantSyncFrame = constantDcaCoreSS(ctx->core.curFrame, syncFrame);

    if (!constantSyncFrame) {
      if (
        checkDcaCoreSSChangeCompliance(
          ctx->core.curFrame, syncFrame, &ctx->core.warningFlags
        ) < 0
      )
        return -1;

      ctx->core.curFrame = syncFrame;
    }
    else
      LIBBLU_DTS_DEBUG_CORE(
        " NOTE: Skipping frame parameters compliance checks, same content.\n"
      );
  }

  ctx->core.nbFrames++;

  if (NULL == (cell = recoverCurDtsAUCell(ctx->curAccessUnit)))
    return -1;
  cell->length = tellPos(ctx->file) - startOff;

  if (addCurCellToDtsAUFrame(ctx->curAccessUnit) < 0)
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
  if (readBits(file, &value, 2) < 0)
    return -1;
  param->adjustmentLevel = value;

  /* [u2 nuBits4MixOutMask] */
  if (readBits(file, &value, 2) < 0)
    return -1;
  mixOutputMaskNbBits = (value + 1) << 2;

  /* [u2 nuNumMixOutConfigs] */
  if (readBits(file, &value, 2) < 0)
    return -1;
  param->nbMixOutputConfigs = value + 1;

  /* Output Audio Mix Configurations Loop : */
  for (ns = 0; ns < param->nbMixOutputConfigs; ns++) {
    /* [u<nuBits4MixOutMask> nuMixOutChMask[ns]] */
    if (readBits(file, &value, mixOutputMaskNbBits) < 0)
      return -1;
    param->mixOutputChannelsMask[ns] = value;
    param->nbMixOutputChannels[ns] = dcaExtChMaskToNbCh(
      param->mixOutputChannelsMask[ns]
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
  if (readBits(file, &value, 2) < 0)
    return -1;
  param->referenceClockCode = value;

  /* [u3 nuExSSFrameDurationCode] */
  if (readBits(file, &value, 3) < 0)
    return -1;
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
  if (readBits(file, &value, 1) < 0)
    return -1;
  param->timeStampPresent = value;

  if (param->timeStampPresent) {
    /* [u32 nuTimeStamp] */
    if (readBits(file, &value, 32) < 0)
      return -1;
    param->timeStampValue = value;

    /* [u4 nLSB] */
    if (readBits(file, &value, 4) < 0)
      return -1;
    param->timeStampLsb = value;

    param->timeStamp =
      ((uint64_t) param->timeStampValue << 4)
      | param->timeStampLsb
    ;
  }

  /* [u3 nuNumAudioPresnt] */
  if (readBits(file, &value, 3) < 0)
    return -1;
  param->nbAudioPresentations = value + 1;

  /* [u3 nuNumAssets] */
  if (readBits(file, &value, 3) < 0)
    return -1;
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
        if (readBits(file, &value, 8) < 0)
          return -1;
      }
      else
        value = 0;
      param->activeAssetMask[nAuPr][nSS] = value;
    }
  }

  /* [b1 bMixMetadataEnbl] */
  if (readBits(file, &value, 1) < 0)
    return -1;
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
  if (readBits(file, &value, 1) < 0)
    return -1;
  param->assetTypeDescriptorPresent = value;

  if (param->assetTypeDescriptorPresent) {
    /* [u4 nuAssetTypeDescriptor] */
    if (readBits(file, &value, 4) < 0)
      return -1;
    param->assetTypeDescriptor = value;
  }

  /* [b1 bLanguageDescrPresent] */
  if (readBits(file, &value, 1) < 0)
    return -1;
  param->languageDescriptorPresent = value;

  if (param->languageDescriptorPresent) {
    /* [v24 LanguageDescriptor] */
    for (i = 0; i < DTS_EXT_SS_LANGUAGE_DESC_SIZE; i++) {
      if (readBits(file, &value, 8) < 0)
        return -1;
      param->languageDescriptor[i] = value;
    }
    param->languageDescriptor[i] = '\0';
  }

  /* [b1 bInfoTextPresent] */
  if (readBits(file, &value, 1) < 0)
    return -1;
  param->infoTextPresent = value;

  if (param->infoTextPresent) {
    /* [u10 nuInfoTextByteSize] */
    if (readBits(file, &value, 10) < 0)
      return -1;
    param->infoTextLength = value + 1;

    if (DTS_EXT_SS_MAX_STRING_TEXT_MAX_LEN < param->infoTextLength)
      LIBBLU_DTS_ERROR_RETURN(
        "DCA Extension Substream asset descriptor Additional Text Info "
        "unsupported size (exceed %zu characters).\n",
        DTS_EXT_SS_MAX_STRING_TEXT_MAX_LEN
      );

    /* [v<nuInfoTextByteSize> InfoTextString] */
    for (i = 0; i < param->infoTextLength; i++) {
      if (readBits(file, &value, 8) < 0)
        return -1;
      param->infoText[i] = value;
    }
    param->infoText[i] = '\0';
  }

  /* [u5 nuBitResolution] */
  if (readBits(file, &value, 5) < 0)
    return -1;
  param->bitDepth = value + 1;

  /* [u4 nuMaxSampleRate] */
  if (readBits(file, &value, 4) < 0)
    return -1;
  param->maxSampleRateCode = value;

  param->maxSampleRate = dtsExtSourceSampleRateCodeValue(
    param->maxSampleRateCode
  );

  /* [u8 nuTotalNumChs] */
  if (readBits(file, &value, 8) < 0)
    return -1;
  param->nbChannels = value + 1;

  if (DTS_EXT_SS_MAX_CHANNELS_NB < param->nbChannels)
    LIBBLU_DTS_ERROR_RETURN(
      "DCA Extension Substream asset descriptor unsupported total "
      "number of channels (exceed %zu).\n",
      DTS_EXT_SS_MAX_CHANNELS_NB
    );

  /* [b1 bOne2OneMapChannels2Speakers] */
  if (readBits(file, &value, 1) < 0)
    return -1;
  param->directSpeakersFeed = value;

  if (param->directSpeakersFeed) {
    if (2 < param->nbChannels) {
      /* [b1 bEmbeddedStereoFlag] */
      if (readBits(file, &value, 1) < 0)
        return -1;
      param->embeddedStereoDownmix = value;
    }
    else
      param->embeddedStereoDownmix = false;

    if (6 < param->nbChannels) {
      /* [b1 bEmbeddedSixChFlag] */
      if (readBits(file, &value, 1) < 0)
        return -1;
      param->embeddedSurround6chDownmix = value;
    }
    else
      param->embeddedSurround6chDownmix = false;

    /* [b1 bSpkrMaskEnabled] */
    if (readBits(file, &value, 1) < 0)
      return -1;
    param->speakersMaskEnabled = value;

    if (param->speakersMaskEnabled) {
      /* [u2 nuNumBits4SAMask] */
      if (readBits(file, &value, 2) < 0)
        return -1;
      spkrMaskNbBits = (value + 1) << 2;

      /* [v<nuNumBits4SAMask> nuSpkrActivityMask] */
      if (readBits(file, &value, spkrMaskNbBits) < 0)
        return -1;
      param->speakersMask = value;
    }
    else
      spkrMaskNbBits = 0;

    /* [u3 nuNumSpkrRemapSets] */
    if (readBits(file, &value, 3) < 0)
      return -1;
    param->nbOfSpeakersRemapSets = value;

    if (!spkrMaskNbBits && 0 < param->nbOfSpeakersRemapSets) {
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

    for (ns = 0; ns < param->nbOfSpeakersRemapSets; ns++) {
      /* [v<nuNumBits4SAMask> nuStndrSpkrLayoutMask] */
      if (readBits(file, &value, spkrMaskNbBits) < 0)
        return -1;
      param->stdSpeakersLayoutMask[ns] = value;

      /* [0 nuNumSpeakers] */
      param->nbChsInRemapSet[ns] = dcaExtChMaskToNbCh(
        param->stdSpeakersLayoutMask[ns]
      );
    }

    for (ns = 0; ns < param->nbOfSpeakersRemapSets; ns++) {
      /* [u5 nuNumDecCh4Remap[ns]] */
      if (readBits(file, &value, 5) < 0)
        return -1;
      param->nbChRequiredByRemapSet[ns] = value + 1;

      for (nCh = 0; nCh < param->nbChsInRemapSet[ns]; nCh++) {
        /* [v<nuNumDecCh4Remap> nuRemapDecChMask] */
        if (readBits(file, &value, param->nbChRequiredByRemapSet[ns]) < 0)
          return -1;
        param->decChLnkdToSetSpkrMask[ns][nCh] = value;

        /* [0 nCoeff] */
        param->nbRemapCoeffCodes[ns][nCh] = nbBitsSetTo1(
          param->decChLnkdToSetSpkrMask[ns][nCh]
        );

        for (nc = 0; nc < param->nbRemapCoeffCodes[ns][nCh]; nc++) {
          /* [u5 nuSpkrRemapCodes[ns][nCh][nc]] */
          if (readBits(file, &value, 5) < 0)
            return -1;
          param->outputSpkrRemapCoeffCodes[ns][nCh][nc] = value;
        }
      }
    }
  }
  else {
    param->embeddedStereoDownmix = false;
    param->embeddedSurround6chDownmix = false;

    /* [u3 nuRepresentationType] */
    if (readBits(file, &value, 3) < 0)
      return -1;
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
  if (readBits(file, &value, 1) < 0)
    return -1;
  param->drcEnabled = value;

  if (param->drcEnabled) {
    /* [u8 nuDRCCode] */
    if (readBits(file, &value, 8) < 0)
      return -1;
    param->drcParameters.drcCode = value;
  }

  /* [b1 bDialNormPresent] */
  if (readBits(file, &value, 1) < 0)
    return -1;
  param->dialNormEnabled = value;

  if (param->dialNormEnabled) {
    /* [u5 nuDialNormCode] */
    if (readBits(file, &value, 5) < 0)
      return -1;
    param->dialNormCode = value;
  }

  if (param->drcEnabled && assetStaticFieldsParam->embeddedStereoDownmix) {
    /* [u8 nuDRC2ChDmixCode] */
    if (readBits(file, &value, 8) < 0)
      return -1;
    param->drcParameters.drc2ChCode = value;
  }

  if (NULL != staticFieldsParam && staticFieldsParam->mixMetadataEnabled) {
    /* [b1 bMixMetadataPresent] */
    if (readBits(file, &value, 1) < 0)
      return -1;
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
    if (readBits(file, &value, 1) < 0)
      return -1;
    param->mixMetadata.useExternalMix = value;

    /* [u6 nuPostMixGainAdjCode] */
    if (readBits(file, &value, 6) < 0)
      return -1;
    param->mixMetadata.postMixGainCode = value;

    /* [u2 nuControlMixerDRC] */
    if (readBits(file, &value, 2) < 0)
      return -1;
    param->mixMetadata.drcMixerControlCode = value;

    if (param->mixMetadata.drcMixerControlCode < 3) {
      /* [u3 nuLimit4EmbeddedDRC] */
      if (readBits(file, &value, 3) < 0)
        return -1;
      param->mixMetadata.limitDRCPriorMix = value;
    }
    else if (param->mixMetadata.drcMixerControlCode == 3) {
      /* [u8 nuCustomDRCCode] */
      if (readBits(file, &value, 8) < 0)
        return -1;
      param->mixMetadata.customMixDRCCoeffCode = value;
    }

    /* [b1 bEnblPerChMainAudioScale] */
    if (readBits(file, &value, 1) < 0)
      return -1;
    param->mixMetadata.perMainAudioChSepScal = value;

    for (
      ns = 0;
      ns < staticFieldsParam->mixMetadata.nbMixOutputConfigs;
      ns++
    ) {
      if (param->mixMetadata.perMainAudioChSepScal) {
        for (
          nCh = 0;
          nCh < staticFieldsParam->mixMetadata.nbMixOutputChannels[ns];
          nCh++
        ) {
          /* [u6 nuMainAudioScaleCode[ns][nCh]] // Unique per channel */
          if (readBits(file, &value, 6) < 0)
            return -1;
          param->mixMetadata.scalingAudioParam[ns][nCh] = value;
        }
      }
      else {
        /* [u6 nuMainAudioScaleCode[ns][0]] // Unique for all channels */
        if (readBits(file, &value, 6) < 0)
          return -1;
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

    for (ns = 0; ns < staticFieldsParam->mixMetadata.nbMixOutputConfigs; ns++) {
      /* Mix Configurations loop */
      for (nE = 0; nE < param->mixMetadata.nbDownMixes; nE++) {
        /* Embedded downmix loop */
        for (nCh = 0; nCh < param->mixMetadata.nbChPerDownMix[nE]; nCh++) {
          /* Supplemental Channel loop */

          /* [v<nNumMixOutCh[ns]> nuMixMapMask[ns][nE][nCh]] */
          if (
            readBits(
              file, &value,
              staticFieldsParam->mixMetadata.nbMixOutputChannels[ns]
            ) < 0
          )
            return -1;
          param->mixMetadata.mixOutputMappingMask[ns][nE][nCh] = value;

          /* [0 nuNumMixCoefs[ns][nE][nCh]] */
          param->mixMetadata.mixOutputMappingNbCoeff[ns][nE][nCh] =
            nbBitsSetTo1(param->mixMetadata.mixOutputMappingMask[ns][nE][nCh])
          ;

          for (nC = 0; nC < param->mixMetadata.mixOutputMappingNbCoeff[ns][nE][nCh]; nC++) {
            /* [u6 nuMixCoeffs[ns][nE][nCh][nC]] */
            if (readBits(file, &value, 6) < 0)
              return -1;
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
  if (readBits(file, &value, 2) < 0)
    return -1;
  param->codingMode = value;

  switch (param->codingMode) {
    case DCA_EXT_SS_CODING_MODE_DTS_HD_COMPONENTS:
      /* DTS-HD component(s). */

      /* [u12 nuCoreExtensionMask] */
      if (readBits(file, &value, 12) < 0)
        return -1;
      param->codingComponentsUsedMask = value;

      if (
        param->codingComponentsUsedMask
        & DCA_EXT_SS_COD_COMP_EXTSUB_CORE_DCA
      ) {
        /* [u14 nuExSSCoreFsize] */
        if (readBits(file, &value, 14) < 0)
          return -1;
        param->codingComponents.extSSCore.size = value + 1;

        /* [b1 bExSSCoreSyncPresent] */
        if (readBits(file, &value, 1) < 0)
          return -1;
        param->codingComponents.extSSCore.syncWordPresent = value;

        if (param->codingComponents.extSSCore.syncWordPresent) {
          /* [u2 nuExSSCoreSyncDistInFrames] */
          if (readBits(file, &value, 2) < 0)
            return -1;
          param->codingComponents.extSSCore.syncDistanceInFramesCode = value;
          param->codingComponents.extSSCore.syncDistanceInFrames = 1 << value;
        }
      }

      if (param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_XBR) {
        /* [u14 nuExSSXBRFsize] */
        if (readBits(file, &value, 14) < 0)
          return -1;
        param->codingComponents.extSSXbr.size = value + 1;
      }

      if (param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_XXCH) {
        /* [u14 nuExSSXXCHFsize] */
        if (readBits(file, &value, 14) < 0)
          return -1;
        param->codingComponents.extSSXxch.size = value + 1;
      }

      if (param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_X96) {
        /* [u12 nuExSSX96Fsize] */
        if (readBits(file, &value, 12) < 0)
          return -1;
        param->codingComponents.extSSX96.size = value + 1;
      }

      if (param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_LBR) {
        /* [u14 nuExSSLBRFsize] */
        if (readBits(file, &value, 14) < 0)
          return -1;
        param->codingComponents.extSSLbr.size = value + 1;

        /* [b1 bExSSLBRSyncPresent] */
        if (readBits(file, &value, 1) < 0)
          return -1;
        param->codingComponents.extSSLbr.syncWordPresent = value;

        if (param->codingComponents.extSSLbr.syncWordPresent) {
          /* [u2 nuExSSLBRSyncDistInFrames] */
          if (readBits(file, &value, 2) < 0)
            return -1;
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
        if (readBits(file, &value, 1) < 0)
          return -1;
        param->codingComponents.extSSXll.syncWordPresent = value;

        if (param->codingComponents.extSSXll.syncWordPresent) {
          /* [u4 nuPeakBRCntrlBuffSzkB] */
          if (readBits(file, &value, 4) < 0)
            return -1;
          param->codingComponents.extSSXll.peakBitRateSmoothingBufSize = value << 4;

          /* [u5 nuBitsInitDecDly] */
          if (readBits(file, &value, 5) < 0)
            return -1;

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
        if (readBits(file, &value, 16) < 0)
          return -1;
        param->codingComponents.reservedExtension1_data = value;
      }

      if (param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_RESERVED_2) {
        /* [v16 *Ignore*] */
        if (readBits(file, &value, 16) < 0)
          return -1;
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
      if (readBits(file, &value, 1) < 0)
        return -1;
      param->codingComponents.extSSXll.syncWordPresent = value;

      if (param->codingComponents.extSSXll.syncWordPresent) {
        /* [u4 nuPeakBRCntrlBuffSzkB] */
        if (readBits(file, &value, 4) < 0)
          return -1;
        param->codingComponents.extSSXll.peakBitRateSmoothingBufSizeCode = value;
        param->codingComponents.extSSXll.peakBitRateSmoothingBufSize = value << 4;

        /* [u5 nuBitsInitDecDly] */
        if (readBits(file, &value, 5) < 0)
          return -1;

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
      if (readBits(file, &value, 14) < 0)
        return -1;
      param->codingComponents.extSSLbr.size = value + 1;

      /* [b1 bExSSLBRSyncPresent] */
      if (readBits(file, &value, 1) < 0)
        return -1;
      param->codingComponents.extSSLbr.syncWordPresent = value;

      if (param->codingComponents.extSSLbr.syncWordPresent) {
        /* [u2 nuExSSLBRSyncDistInFrames] */
        if (readBits(file, &value, 2) < 0)
          return -1;
        param->codingComponents.extSSLbr.syncDistanceInFramesCode = value;
        param->codingComponents.extSSLbr.syncDistanceInFrames = 1 << value;
      }
      break;

    case DCA_EXT_SS_CODING_MODE_AUXILIARY_CODING:
      /* Auxiliary audio coding. */

      param->codingComponentsUsedMask = 0;

      /* [u14 nuExSSAuxFsize] */
      if (readBits(file, &value, 14) < 0)
        return -1;
      param->auxilaryCoding.size = value + 1;

      /* [u8 nuAuxCodecId] */
      if (readBits(file, &value, 8) < 0)
        return -1;
      param->auxilaryCoding.auxCodecId = value;

      /* [b1 bExSSAuxSyncPresent] */
      if (readBits(file, &value, 1) < 0)
        return -1;
      param->auxilaryCoding.syncWordPresent = value;

      if (param->auxilaryCoding.syncWordPresent) {
        /* [u2 nuExSSAuxSyncDistInFrames] */
        if (readBits(file, &value, 2) < 0)
          return -1;
        param->auxilaryCoding.syncDistanceInFramesCode = value;
        param->auxilaryCoding.syncDistanceInFrames = 1 << value;
      }
  }

  if (param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_XLL) {
    /* Extension Substream contains DTS-XLL component. */

    /* [u3 nuDTSHDStreamID] */
    if (readBits(file, &value, 3) < 0)
      return -1;
    param->codingComponents.extSSXll.steamId = value;
  }

  if (
    assetStaticFieldsParam->directSpeakersFeed
    && NULL != staticFieldsParam
    && staticFieldsParam->mixMetadataEnabled
    && !dynMetadataParam->mixMetadataPresent
  ) {
    /* [b1 bOnetoOneMixingFlag] */
    if (readBits(file, &value, 1) < 0)
      return -1;
    param->mixMetadata.oneTrackToOneChannelMix = value;
    param->mixMetadataFieldsPresent = true;
  }
  else {
    param->mixMetadata.oneTrackToOneChannelMix = false;
    param->mixMetadataFieldsPresent = false;
  }

  if (param->mixMetadata.oneTrackToOneChannelMix) {
    /* [b1 bEnblPerChMainAudioScale] */
    if (readBits(file, &value, 1) < 0)
      return -1;
    param->mixMetadata.perChannelMainAudioScaleCode = value;

    for (
      ns = 0;
      ns < staticFieldsParam->mixMetadata.nbMixOutputConfigs;
      ns++
    ) {

      if (param->mixMetadata.perChannelMainAudioScaleCode) {
        for (
          nCh = 0;
          nCh < staticFieldsParam->mixMetadata.nbMixOutputChannels[ns];
          nCh++
        ) {
          /* [u6 nuMainAudioScaleCode[mixConfigIdx][channelIdx]] */
          if (readBits(file, &value, 6) < 0)
            return -1;
          param->mixMetadata.mainAudioScaleCodes[ns][nCh] = value;
        }
      }
      else {
        /* [u6 nuMainAudioScaleCode[mixConfigIdx][0]] */
        if (readBits(file, &value, 6) < 0)
          return -1;
        param->mixMetadata.mainAudioScaleCodes[ns][0] = value;
      }
    }
  }

  /* [b1 bDecodeAssetInSecondaryDecoder] */
  if (readBits(file, &value, 1) < 0)
    return -1;
  param->decodeInSecondaryDecoder = value;

#if DCA_EXT_SS_ENABLE_DRC_2
  param->extraDataPresent = (
    0 < expectedEndOffsetOfAssetDesc - tellBinaryPos(file)
  );

  if (param->extraDataPresent) {
    /* [b1 bDRCMetadataRev2Present] */
    if (readBits(file, &value, 1) < 0)
      return -1;
    param->drcRev2Present = value;

    if (param->drcRev2Present) {
      /* [u4 DRCversion_Rev2] */
      if (readBits(file, &value, 4) < 0)
        return -1;
      param->drcRev2.version = value;

      if (param->drcRev2.version == DCA_EXT_SS_DRC_REV_2_VERSION_1) {
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
  bool saveResField;

  startOff = tellBinaryPos(file);

  /* -- Main Asset Parameters Section (Size, Index and Per Stream Static Metadata) -- */
  /* [u9 nuAssetDescriptFsize] */
  if (readBits(file, &value, 9) < 0)
    return -1;
  param->descriptorLength = value + 1;

  endOff = startOff + (param->descriptorLength * 8);

  /* [u3 nuAssetIndex] */
  if (readBits(file, &value, 3) < 0)
    return -1;
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
  param->reservedFieldLength = remainingBits / 8;
  param->paddingBits = remainingBits & 0x7; /* % 8 */
  saveResField = (
    remainingBits <= 8 * DCA_EXT_SS_MAX_SUPP_RES_FIELD_SIZE
  );

  /* [v<nuExtSSHeaderSize - tellPos() - 16> Reserved] */
  if (0 < param->reservedFieldLength) {
    if (saveResField) {
      if (
        readBytes(
          file,
          param->reservedFieldData,
          param->reservedFieldLength
        ) < 0
      )
        return -1;
    }
    else {
      if (skipBytes(file, param->reservedFieldLength))
        return -1;
    }
  }

  /* [v0..7 ByteAlign] */
  if (saveResField) {
    if (readBits(file, &value, param->paddingBits) < 0)
      return -1;
    param->reservedFieldData[param->reservedFieldLength] = (uint8_t) value;
  }
  else {
    if (skipBits(file, param->paddingBits) < 0)
      return -1;
  }
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
  bool saveResField;

  assert(NULL != file);
  assert(NULL != param);

  staticFieldsParam = &param->staticFields;

  startOff = tellBinaryPos(file);

  /* [u32 SYNCEXTSSH] // 0x64582025 */
  if (readValueBigEndian(file, 4, &value) < 0)
    return -1;
  if (value != DTS_SYNCWORD_SUBSTREAM)
    LIBBLU_DTS_ERROR_RETURN(
      "Unexpected non-DCA Extension Substream sync word (0x%04" PRIX16 ").\n",
      value
    );

  /* [v8 UserDefinedBits] */
  if (readBits(file, &value, 8) < 0)
    return -1;
  param->userDefinedBits = value;

#if !DCA_EXT_SS_DISABLE_CRC
  if (initCrc(BITSTREAM_CRC_CTX(file), DCA_EXT_SS_CRC_PARAM(), DCA_EXT_SS_CRC_INITIAL_V) < 0)
    return -1;
#endif

  /* [u2 nExtSSIndex] */
  if (readBits(file, &value, 2) < 0)
    return -1;
  param->extSSIdx = value;

  /** [u2 bHeaderSizeType]
   * nuBits4Header = (bHeaderSizeType) ? 8 : 12;
   * nuBits4ExSSFsize = (bHeaderSizeType) ? 16 : 20;
   */
  if (readBits(file, &value, 1) < 0)
    return -1;
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
  if (readBits(file, &value, 1) < 0)
    return -1;
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
    if (readBits(file, &value, 1) < 0)
      return -1;

    param->audioAssetBckwdCompCorePresent[nAst] = value;
  }

  for (nAst = 0; nAst < staticFieldsParam->nbAudioAssets; nAst++) {
    if (param->audioAssetBckwdCompCorePresent[nAst]) {
      /* [u2 nuBcCoreExtSSIndex[nAst]] */
      if (readBits(file, &value, 2) < 0)
        return -1;
      param->audioAssetBckwdCompCore[nAst].extSSIndex = value;

      /* [u3 nuBcCoreAssetIndex[nAst]] */
      if (readBits(file, &value, 3) < 0)
        return -1;
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
  param->reservedFieldLength = remainingBits / 8;
  param->paddingBits = remainingBits & 0x7; /* % 8 */
  saveResField = (
    remainingBits <= 8 * DCA_EXT_SS_MAX_SUPP_RES_FIELD_SIZE
  );

  /* [v<nuExtSSHeaderSize - tellPos() - 16> Reserved] */
  if (0 < param->reservedFieldLength) {
    if (saveResField) {
      if (
        readBytes(
          file,
          param->reservedFieldData,
          param->reservedFieldLength
        ) < 0
      )
        return -1;
    }
    else {
      if (skipBytes(file, param->reservedFieldLength))
        return -1;
    }
  }

  /* [v0..7 ByteAlign] */
  if (saveResField) {
    if (readBits(file, &value, param->paddingBits) < 0)
      return -1;
    param->reservedFieldData[param->reservedFieldLength] = (uint8_t) value;
  }
  else {
    if (skipBits(file, param->paddingBits) < 0)
      return -1;
  }

#if !DCA_EXT_SS_DISABLE_CRC
  if (endCrc(BITSTREAM_CRC_CTX(file), &value) < 0)
    return -1;

  extSSCrcResult = value & 0xFFFF;
#endif

  /* [u16 nCRC16ExtSSHeader] */
  if (readBits(file, &value, 16) < 0)
    return -1;
  param->crcValue = value;

#if !DCA_EXT_SS_DISABLE_CRC
  if (param->crcValue != extSSCrcResult)
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
  DtsContextPtr ctx,
  DcaExtSSHeaderParameters param,
  unsigned xllAssetId,
  DcaXllFrameSFPosition * assetContentOffsets
)
{
  int ret;

  size_t targetFrameSize, pbrSmoothingBufSize;
  DcaXllFrameSFPosition buildedFrameContent;
  int syncWordOffEntryIndex;
  unsigned syncDecodingDelay;

  bool syncWordPresent;
  int64_t syncWordOff;
  unsigned decodingDelay;

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

  if (
    NULL != ctx->dtshdFileHandle
    && ctx->dtshdFileHandle->extMetadataPresent
  ) {
    pbrSmoothingBufSize =
      ctx->dtshdFileHandle->extMetadata.peakBitRateSmoothingBufSizeKb
    ;
  }
  else
    LIBBLU_DTS_ERROR_RETURN(
      "Unable to set DTS PBR smoothing buffer size, "
      "missing information in DTS-HD header.\n"
    );

  ret = substractPbrFrameSliceDtsXllFrameContext(
    ctx->xllCtx,
    targetFrameSize,
    &buildedFrameContent,
    &syncWordOffEntryIndex,
    &syncDecodingDelay
  );
  if (ret < 0)
    return -1;

  syncWordPresent = (0 <= syncWordOffEntryIndex);
  syncWordOff = 0;
  decodingDelay = 0;

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
    pbrSmoothingBufSize,
    targetFrameSize
  );

  return replaceCurDtsAUCell(
    ctx->curAccessUnit,
    DTS_AU_INNER_EXT_SS_HDR(param)
  );
}

int decodeDcaExtSS(
  DtsContextPtr ctx
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

  DcaExtSSFrameParameters * curFrame;
  uint8_t extSSIdx;

  lbc frameClockTime[STRT_H_M_S_MS_LEN];

  DtsAUCellPtr cell;

  assert(NULL != ctx);

  startOff = tellPos(ctx->file);
  str_time(
    frameClockTime,
    STRT_H_M_S_MS_LEN, STRT_H_M_S_MS,
    (ctx->extSSPresent) ? ctx->extSS.frameDts : 0
  );

  LIBBLU_DTS_DEBUG(
    "0x%08" PRIX64 " === DCA Extension Substream Frame %u - %" PRI_LBCS " ===\n",
    startOff,
    (ctx->extSSPresent) ? ctx->extSS.nbFrames : 0,
    frameClockTime
  );

  ignoreFrame = ctx->skipExtensionSubstreams || ctx->skipCurrentPeriod;

  if (decodeDcaExtSSHeader(ctx->file, &extFrame.header) < 0)
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
        extFrame.header,
        ctx->isSecondaryStream,
        &ctx->extSS.warningFlags
      ) < 0
    )
      return -1;

    extSSIdx = extFrame.header.extSSIdx;

    if (!ctx->extSS.presentIndexes[extSSIdx]) {
      assert(!ctx->extSS.presentIndexes[extSSIdx]);

      curFrame = (DcaExtSSFrameParameters *) malloc(
        sizeof(DcaExtSSFrameParameters)
      );
      if (NULL == curFrame)
        LIBBLU_DTS_ERROR_RETURN(
          "Memory allocation error.\n"
        );

      ctx->extSS.currentExtSSIdx = extSSIdx;
      ctx->extSS.curFrames[extSSIdx] = curFrame;
      ctx->extSS.presentIndexes[extSSIdx] = true;
    }

#if 0
    else {
      curFrame = ctx->extSS.curFrames[extSSIdx];

      if (
        checkDcaExtSSHeaderChangeCompliance(
          curFrame->header,
          extFrame.header,
          &ctx->extSS.warningFlags
        ) < 0
      )
        return -1;
    }
#endif

    /* Frame content is always updated */
    *(ctx->extSS.curFrames[extSSIdx]) = extFrame;

    if (NULL == (cell = recoverCurDtsAUCell(ctx->curAccessUnit)))
      return -1;
    cell->length = tellPos(ctx->file) - startOff;

    if (DTS_CTX_FAST_2ND_PASS(ctx)) {
      ret = patchDcaExtSSHeader(
        ctx, extFrame.header, xllAssetId, &xllAssetOffsets
      );
      if (ret < 0)
        return -1;
    }

    if (addCurCellToDtsAUFrame(ctx->curAccessUnit) < 0)
      return -1;

    ctx->extSS.nbFrames++;
  }
  else {
    LIBBLU_DTS_DEBUG_EXTSS(" *Not checked, skipped*\n");
    if (discardCurDtsAUCell(ctx->curAccessUnit) < 0)
      return -1;
  }

  /* Decode extension substream assets */
  decodeAssets =
    !ignoreFrame
    && !DTS_CTX_FAST_2ND_PASS(ctx)
    && (
      isInitPbrFileHandler()
      || DTS_XLL_FORCE_UNPACKING
      || DTS_XLL_FORCE_REBUILD_SEIING
    )
  ;

  for (nAst = 0; nAst < extFrame.header.staticFields.nbAudioAssets; nAst++) {
    assetStartOff = tellPos(ctx->file);
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
      cell = initDtsAUCell(ctx->curAccessUnit, DTS_FRM_INNER_EXT_SS_ASSET);
      if (NULL == cell)
        return -1;
      cell->startOffset = assetStartOff;
      cell->length = assetLength;

      if (DTS_CTX_FAST_2ND_PASS(ctx) && ctx->extSS.content.xllExtSS) {
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
            ctx->curAccessUnit,
            DTS_AU_INNER_EXT_SS_ASSET(xllAssetOffsets)
          );
        }
      }
    }

    if (decodeAssets) {
      if (decodeDcaExtSubAsset(ctx->file, &ctx->xllCtx, *asset, extFrame.header, assetLength) < 0)
        return -1;

      assetReadedLength = tellPos(ctx->file) - assetStartOff;
    }
    else
      assetReadedLength = 0;

    if (skipBytes(ctx->file, assetLength - assetReadedLength) < 0)
      return -1;

    if (!ignoreFrame) {
      if (addCurCellToDtsAUFrame(ctx->curAccessUnit) < 0)
        return -1;
    }
  }

  /* Check Extension Substream Size : */
  if (
    !DTS_CTX_FAST_2ND_PASS(ctx)
    && (
      tellPos(ctx->file) - startOff
      != extFrame.header.extensionSubstreamFrameLength
    )
  )
    LIBBLU_DTS_ERROR_RETURN(
      "Unexpected Extension substream frame length "
      "(parsed length: %zu bytes, expected %zu bytes).\n",
      tellPos(ctx->file) - startOff,
      extFrame.header.extensionSubstreamFrameLength
    );

  /* lbc_printf("%" PRId64 "\n", extFrame.header.extensionSubstreamFrameLength); */

  return 0;
}

int parseDts(
  DtsContextPtr ctx,
  EsmsFileHeaderPtr esms
)
{

  while (!isEof(DTS_CTX_IN_FILE(ctx))) {

    /* Progress bar : */
    printFileParsingProgressionBar(DTS_CTX_IN_FILE(ctx));

    if (DTS_CTX_IS_DTSHD_FILE(ctx)) {
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

    if (completeDtsFrame(ctx, esms) < 0)
      return -1;
  }

  return 0;
}

int analyzeDts(
  LibbluESParsingSettings * settings
)
{
  BitstreamReaderPtr file;
  DtsContextPtr ctx = NULL;

  /* Script output */
  EsmsFileHeaderPtr esmsInfos = NULL;

  if (NULL == (file = createBitstreamReader(settings->esFilepath, READ_BUFFER_LEN)))
    goto free_return;
  if (NULL == (ctx = createDtsContext(file, settings->scriptFilepath, settings->options)))
    goto free_return;

  /* Prepare ESMS output file */
  esmsInfos = createEsmsFileHandler(ES_AUDIO, settings->options, FMT_SPEC_INFOS_NONE);
  if (NULL == esmsInfos)
    goto free_return;

  if (writeEsmsHeader(DTS_CTX_OUT_ESMS(ctx)) < 0)
    goto free_return;

  if (
    appendSourceFileEsms(
      esmsInfos, settings->esFilepath,
      DTS_CTX_OUT_ESMS_FILE_IDX_PTR(ctx)
    ) < 0
  )
    goto free_return;

  if (NULL != settings->options.pbrFilepath) {
    if (initPbrFileHandler(settings->options.pbrFilepath) < 0)
      return -1;
  }

  do {
    if (initParsingDtsContext(ctx))
      goto free_return;

    if (DTS_CTX_FAST_2ND_PASS(ctx)) {
      if (NULL == (ctx->xllCtx))
        LIBBLU_ERROR_FRETURN(
          "PBR smoothing is only available for DTS XLL streams.\n"
        );

      /* Compute DTS smoothing */
      /* lbc_printf("%u frames\n", ctx->xllCtx->pbrSmoothing.nbUsedFrames); */
      if (performComputationDtsXllPbr(ctx->xllCtx) < 0)
        return -1;

      ctx->core.nbFrames = 0;
      ctx->extSS.nbFrames = 0;
    }

    if (parseDts(ctx, esmsInfos) < 0)
      goto free_return;
  } while (nextPassDtsContext(ctx));

  lbc_printf(" === Parsing finished with success. ===              \n");

  /* [u8 endMarker] */
  if (writeByte(DTS_CTX_OUT_ESMS(ctx), ESMS_SCRIPT_END_MARKER) < 0)
    goto free_return;

  /* Update if required ESMS header */
  if (updateDtsEsmsHeader(ctx, esmsInfos) < 0)
    return -1;

  if (addEsmsFileEnd(DTS_CTX_OUT_ESMS(ctx), esmsInfos) < 0)
    return -1;

  /* Display infos : */
  lbc_printf("== Stream Infos =======================================================================\n");
  /* lbc_printf(
    "Codec: %s, Nb channels: %d, Sample rate: %u Hz, "
    "Bits per sample: %ubits.\n",
  ); */
  if (ctx->corePresent)
    lbc_printf("Core frames: %u frame(s).\n", ctx->core.nbFrames);
  if (ctx->extSSPresent)
    lbc_printf("Extension Substream frames: %u frame(s).\n", ctx->extSS.nbFrames);
  lbc_printf("=======================================================================================\n");

  closeBitstreamReader(file);
  releasePbrFileHandler();
  destroyDtsContext(ctx);

  if (updateEsmsHeader(settings->scriptFilepath, esmsInfos) < 0)
    return -1;
  destroyEsmsFileHandler(esmsInfos);

  return 0;

free_return:
  closeBitstreamReader(file);
  releasePbrFileHandler();
  destroyDtsContext(ctx);
  destroyEsmsFileHandler(esmsInfos);

  return -1;
}
