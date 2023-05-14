#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include "dts_patcher.h"

size_t computeDcaExtSSHeaderMixMetadataSize(
  const DcaExtSSHeaderMixMetadataParameters * param
)
{
  size_t size;

  size_t mixOutputMaskNbBits;

  /* [u2 nuMixMetadataAdjLevel] */
  /* [u2 nuBits4MixOutMask] */
  /* [u2 nuNumMixOutConfigs] */
  size = 6;

  /* Output Audio Mix Configurations Loop : */
  /* [u<nuBits4MixOutMask * ns> nuMixOutChMask[ns]] */
  COMPUTE_NUBITS4MIXOUTMASK(
    mixOutputMaskNbBits,
    param->mixOutputChannelsMask,
    param->nbMixOutputConfigs
  );

  size += mixOutputMaskNbBits * param->nbMixOutputConfigs;

  return size;
}

size_t computeDcaExtSSHeaderStaticFieldsSize(
  const DcaExtSSHeaderStaticFieldsParameters * param,
  const uint8_t nExtSSIndex
)
{
  size_t size;

  unsigned nAuPr, nSS;

  /* [u2 nuRefClockCode] */
  /* [u3 nuExSSFrameDurationCode] */
  /* [b1 bTimeStampFlag] */
  size = 13; /* Include some other fields */

  if (param->timeStampPresent) {
    /* [u32 nuTimeStamp] */
    /* [u4 nLSB] */
    size += 36;
  }

  /* [u3 nuNumAudioPresnt] */
  /* [u3 nuNumAssets] */
  /* Already counted */

  /* [u<(nExtSSIndex+1) * nuNumAudioPresnt> nuNumAssets] */
  size += (nExtSSIndex + 1) * param->nbAudioPresentations;

  for (nAuPr = 0; nAuPr < param->nbAudioPresentations; nAuPr++) {
    for (nSS = 0; nSS < (unsigned) nExtSSIndex + 1; nSS++) {
      if ((param->activeExtSSMask[nAuPr] >> nSS) & 0x1) {
        /* [u8 nuActiveAssetMask[nAuPr][nSS]] */
        size += 8;
      }
    }
  }

  /* [b1 bMixMetadataEnbl] */
  /* Already counted */

  if (param->mixMetadataEnabled) {
    /* [vn MixMedata()] */
    size += computeDcaExtSSHeaderMixMetadataSize(
      &param->mixMetadata
    );
  }

  return size;
}

int buildDcaExtSSHeaderStaticFields(
  DtsPatcherBitstreamHandlePtr handle,
  const DcaExtSSHeaderStaticFieldsParameters * param,
  const uint8_t nExtSSIndex
)
{
  int ret;

  unsigned nAuPr, nSS;

  /* [u2 nuRefClockCode] */
  ret = writeBitsDtsPatcherBitstreamHandle(
    handle,
    param->referenceClockCode,
    2
  );
  if (ret < 0)
    return -1;

  /* [u3 nuExSSFrameDurationCode] */
  ret = writeBitsDtsPatcherBitstreamHandle(
    handle,
    param->frameDurationCode,
    3
  );
  if (ret < 0)
    return -1;

  /* [b1 bTimeStampFlag] */
  ret = writeBitDtsPatcherBitstreamHandle(
    handle,
    param->timeStampPresent
  );
  if (ret < 0)
    return -1;

  if (param->timeStampPresent) {
    /* [u32 nuTimeStamp] */
    ret = writeBitsDtsPatcherBitstreamHandle(
      handle,
      param->timeStampValue,
      32
    );
    if (ret < 0)
      return -1;

    /* [u4 nLSB] */
    ret = writeBitsDtsPatcherBitstreamHandle(
      handle,
      param->timeStampLsb,
      4
    );
    if (ret < 0)
      return -1;
  }

  /* [u3 nuNumAudioPresnt] */
  ret = writeBitsDtsPatcherBitstreamHandle(
    handle,
    param->nbAudioPresentations - 1,
    3
  );
  if (ret < 0)
    return -1;

  /* [u3 nuNumAssets] */
  ret = writeBitsDtsPatcherBitstreamHandle(
    handle,
    param->nbAudioAssets - 1,
    3
  );
  if (ret < 0)
    return -1;

  for (nAuPr = 0; nAuPr < param->nbAudioPresentations; nAuPr++) {
    /* [u<nExtSSIndex+1> nuNumAssets] */
    ret = writeBitsDtsPatcherBitstreamHandle(
      handle,
      param->activeExtSSMask[nAuPr],
      nExtSSIndex + 1
    );
    if (ret < 0)
      return -1;
  }

  for (nAuPr = 0; nAuPr < param->nbAudioPresentations; nAuPr++) {
    for (nSS = 0; nSS < (unsigned) nExtSSIndex + 1; nSS++) {

      if ((param->activeExtSSMask[nAuPr] >> nSS) & 0x1) {
        /* [u8 nuActiveAssetMask[nAuPr][nSS]] */
        ret = writeBitsDtsPatcherBitstreamHandle(
          handle,
          param->activeAssetMask[nAuPr][nSS],
          8
        );
        if (ret < 0)
          return -1;
      }
    }
  }

  /* [b1 bMixMetadataEnbl] */
  ret = writeBitDtsPatcherBitstreamHandle(
    handle,
    param->mixMetadataEnabled
  );
  if (ret < 0)
    return -1;

  if (param->mixMetadataEnabled) {
    /* [vn MixMedata()] */
#if 0
    if (buildDcaExtSSHeaderMixMetadata(handle, &param->mixMetadata) < 0)
      return -1;
#else
    LIBBLU_DTS_ERROR_RETURN("WORK IN PROGRESS LINE %u.\n", __LINE__);
#endif
  }

  return 0;
}

size_t computeDcaExtSSAssetDescriptorStaticFieldsSize(
  const DcaAudioAssetDescriptorStaticFieldsParameters * param
)
{
  size_t size;

  unsigned ns, nCh;
  unsigned spkrMaskNbBits;

  /* [b1 bAssetTypeDescrPresent] */
  size = 21; /* Include some other fields */

  if (param->assetTypeDescriptorPresent) {
    /* [u4 nuAssetTypeDescriptor] */
    size += 4;
  }

  /* [b1 bLanguageDescrPresent] */
  /* Already counted */

  if (param->languageDescriptorPresent) {
    /* [v24 LanguageDescriptor] */
    size += DTS_EXT_SS_LANGUAGE_DESC_SIZE;
  }

  /* [b1 bInfoTextPresent] */
  /* Already counted */

  if (param->infoTextPresent) {
    /* [u10 nuInfoTextByteSize] */
    /* [v<nuInfoTextByteSize> InfoTextString] */
    size += 10 + 8 * param->infoTextLength;
  }

  /* [u5 nuBitResolution] */
  /* [u4 nuMaxSampleRate] */
  /* [u8 nuTotalNumChs] */
  /* [b1 bOne2OneMapChannels2Speakers] */
  /* Already counted */

  if (param->directSpeakersFeed) {
    if (2 < param->nbChannels) {
      /* [b1 bEmbeddedStereoFlag] */
      size++;
    }

    if (6 < param->nbChannels) {
      /* [b1 bEmbeddedSixChFlag] */
      size++;
    }

    /* [b1 bSpkrMaskEnabled] */
    size += 4; /* Include some other fields */

    spkrMaskNbBits = 0;

    if (param->speakersMaskEnabled) {
      COMPUTE_NUNUMBITS4SAMASK(spkrMaskNbBits, param->speakersMask);

      /* [u2 nuNumBits4SAMask] */
      /* [v<nuNumBits4SAMask> nuSpkrActivityMask] */
      size += 2 + spkrMaskNbBits;
    }

    /* [u3 nuNumSpkrRemapSets] */
    /* Already counted */

    /* [v<nuNumBits4SAMask * nuNumSpkrRemapSets> nuStndrSpkrLayoutMask] */
    size += spkrMaskNbBits * param->nbOfSpeakersRemapSets;

    for (ns = 0; ns < param->nbOfSpeakersRemapSets; ns++) {
      /* [u5 nuNumDecCh4Remap[ns]] */
      size += 5;

      for (nCh = 0; nCh < param->nbChsInRemapSet[ns]; nCh++) {
        /* [v<nuNumDecCh4Remap> nuRemapDecChMask] */
        size += param->nbChRequiredByRemapSet[ns];

        /* [u<5 * nCoef> nuSpkrRemapCodes[ns][nCh][nc]] */
        size += 5 * param->nbRemapCoeffCodes[ns][nCh];
      }
    }
  }
  else {
    /* [u3 nuRepresentationType] */
    size += 3;
  }

  return size;
}

int buildDcaExtSSAssetDescriptorStaticFields(
  DtsPatcherBitstreamHandlePtr handle,
  const DcaAudioAssetDescriptorStaticFieldsParameters * param
)
{
  int ret;

  int64_t i;
  unsigned ns, nCh, nc;

  unsigned spkrMaskNbBits;

  /* [b1 bAssetTypeDescrPresent] */
  ret = writeBitDtsPatcherBitstreamHandle(
    handle,
    param->assetTypeDescriptorPresent
  );
  if (ret < 0)
    return -1;

  if (param->assetTypeDescriptorPresent) {
    /* [u4 nuAssetTypeDescriptor] */
    ret = writeBitsDtsPatcherBitstreamHandle(
      handle,
      param->assetTypeDescriptor,
      4
    );
    if (ret < 0)
      return -1;
  }

  /* [b1 bLanguageDescrPresent] */
  ret = writeBitDtsPatcherBitstreamHandle(
    handle,
    param->languageDescriptorPresent
  );
  if (ret < 0)
    return -1;

  if (param->languageDescriptorPresent) {
    /* [v24 LanguageDescriptor] */
    for (i = 0; i < DTS_EXT_SS_LANGUAGE_DESC_SIZE; i++) {
      ret = writeBitsDtsPatcherBitstreamHandle(
        handle,
        param->languageDescriptor[i],
        8
      );
      if (ret < 0)
        return -1;
    }
  }

  /* [b1 bInfoTextPresent] */
  ret = writeBitDtsPatcherBitstreamHandle(
    handle,
    param->infoTextPresent
  );
  if (ret < 0)
    return -1;

  if (param->infoTextPresent) {
    /* [u10 nuInfoTextByteSize] */
    ret = writeBitsDtsPatcherBitstreamHandle(
      handle,
      param->infoTextLength - 1,
      10
    );
    if (ret < 0)
      return -1;

    /* [v<nuInfoTextByteSize> InfoTextString] */
    for (i = 0; i < param->infoTextLength; i++) {
      ret = writeBitsDtsPatcherBitstreamHandle(
        handle,
        param->infoText[i],
        8
      );
      if (ret < 0)
        return -1;
    }
  }

  /* [u5 nuBitResolution] */
  ret = writeBitsDtsPatcherBitstreamHandle(
    handle,
    param->bitDepth - 1,
    5
  );
  if (ret < 0)
    return -1;

  /* [u4 nuMaxSampleRate] */
  ret = writeBitsDtsPatcherBitstreamHandle(
    handle,
    param->maxSampleRateCode,
    4
  );
  if (ret < 0)
    return -1;

  /* [u8 nuTotalNumChs] */
  ret = writeBitsDtsPatcherBitstreamHandle(
    handle,
    param->nbChannels - 1,
    8
  );
  if (ret < 0)
    return -1;

  /* [b1 bOne2OneMapChannels2Speakers] */
  ret = writeBitDtsPatcherBitstreamHandle(
    handle,
    param->directSpeakersFeed
  );
  if (ret < 0)
    return -1;

  if (param->directSpeakersFeed) {
    if (2 < param->nbChannels) {
      /* [b1 bEmbeddedStereoFlag] */
      ret = writeBitDtsPatcherBitstreamHandle(
        handle,
        param->embeddedStereoDownmix
      );
      if (ret < 0)
        return -1;
    }

    if (6 < param->nbChannels) {
      /* [b1 bEmbeddedSixChFlag] */
      ret = writeBitDtsPatcherBitstreamHandle(
        handle,
        param->embeddedSurround6chDownmix
      );
      if (ret < 0)
        return -1;
    }

    /* [b1 bSpkrMaskEnabled] */
    ret = writeBitDtsPatcherBitstreamHandle(
      handle,
      param->speakersMaskEnabled
    );
    if (ret < 0)
      return -1;

    spkrMaskNbBits = 0;

    if (param->speakersMaskEnabled) {
      COMPUTE_NUNUMBITS4SAMASK(spkrMaskNbBits, param->speakersMask);

      /* [u2 nuNumBits4SAMask] */
      ret = writeBitsDtsPatcherBitstreamHandle(
        handle,
        (spkrMaskNbBits >> 2) - 1,
        2
      );
      if (ret < 0)
        return -1;

      /* [v<nuNumBits4SAMask> nuSpkrActivityMask] */
      ret = writeBitsDtsPatcherBitstreamHandle(
        handle,
        param->speakersMask,
        spkrMaskNbBits
      );
      if (ret < 0)
        return -1;
    }

    /* [u3 nuNumSpkrRemapSets] */
    ret = writeBitsDtsPatcherBitstreamHandle(
      handle,
      param->nbOfSpeakersRemapSets,
      3
    );
    if (ret < 0)
      return -1;

    for (ns = 0; ns < param->nbOfSpeakersRemapSets; ns++) {
      /* [v<nuNumBits4SAMask> nuStndrSpkrLayoutMask] */
      ret = writeBitsDtsPatcherBitstreamHandle(
        handle,
        param->stdSpeakersLayoutMask[ns],
        spkrMaskNbBits
      );
      if (ret < 0)
        return -1;
    }

    for (ns = 0; ns < param->nbOfSpeakersRemapSets; ns++) {
      /* [u5 nuNumDecCh4Remap[ns]] */
      ret = writeBitsDtsPatcherBitstreamHandle(
        handle,
        param->nbChRequiredByRemapSet[ns] - 1,
        5
      );
      if (ret < 0)
        return -1;

      for (nCh = 0; nCh < param->nbChsInRemapSet[ns]; nCh++) {
        /* [v<nuNumDecCh4Remap> nuRemapDecChMask] */
        ret = writeBitsDtsPatcherBitstreamHandle(
          handle,
          param->decChLnkdToSetSpkrMask[ns][nCh],
          param->nbChRequiredByRemapSet[ns]
        );
        if (ret < 0)
          return -1;

        for (nc = 0; nc < param->nbRemapCoeffCodes[ns][nCh]; nc++) {
          /* [u5 nuSpkrRemapCodes[ns][nCh][nc]] */
          ret = writeBitsDtsPatcherBitstreamHandle(
            handle,
            param->outputSpkrRemapCoeffCodes[ns][nCh][nc],
            5
          );
          if (ret < 0)
            return -1;
        }
      }
    }
  }
  else {
    /* [u3 nuRepresentationType] */
    ret = writeBitsDtsPatcherBitstreamHandle(
      handle,
      param->representationType,
      3
    );
    if (ret < 0)
      return -1;
  }

  return 0;
}

size_t computeDcaExtSSAssetDescriptorDynamicMetadataSize(
  const DcaAudioAssetDescriptorDynamicMetadataParameters * param,
  const DcaAudioAssetDescriptorStaticFieldsParameters * assetStaticFieldsParam,
  const DcaExtSSHeaderStaticFieldsParameters * staticFieldsParam
)
{
  size_t size;

  /* [b1 bDRCCoefPresent] */
  size = 2; /* Include some other fields */

  if (param->drcEnabled) {
    /* [u8 nuDRCCode] */
    size += 8;
  }

  /* [b1 bDialNormPresent] */
  /* Already counted */

  if (param->dialNormEnabled) {
    /* [u5 nuDialNormCode] */
    size += 5;
  }

  if (param->drcEnabled && assetStaticFieldsParam->embeddedStereoDownmix) {
    /* [u8 nuDRC2ChDmixCode] */
    size += 8;
  }

  if ((NULL != staticFieldsParam) && staticFieldsParam->mixMetadataEnabled) {
    /* [b1 bMixMetadataPresent] */
    size++;
  }

  /* TODO */

  return size;
}

int buildDcaExtSSAssetDescriptorDynamicMetadata(
  DtsPatcherBitstreamHandlePtr handle,
  const DcaAudioAssetDescriptorDynamicMetadataParameters * param,
  const DcaAudioAssetDescriptorStaticFieldsParameters * assetStaticFieldsParam,
  const DcaExtSSHeaderStaticFieldsParameters * staticFieldsParam
)
{
  int ret;

  bool staticFieldsPresent;

  /* unsigned ns, nE, nCh, nC; */

  /* [b1 bDRCCoefPresent] */
  if (writeBitDtsPatcherBitstreamHandle(handle, param->drcEnabled) < 0)
    return -1;

  if (param->drcEnabled) {
    /* [u8 nuDRCCode] */
    ret = writeBitsDtsPatcherBitstreamHandle(
      handle, param->drcParameters.drcCode, 8
    );
    if (ret < 0)
      return -1;
  }

  /* [b1 bDialNormPresent] */
  if (writeBitDtsPatcherBitstreamHandle(handle, param->dialNormEnabled) < 0)
    return -1;

  if (param->dialNormEnabled) {
    /* [u5 nuDialNormCode] */
    if (writeBitsDtsPatcherBitstreamHandle(handle, param->dialNormCode, 5) < 0)
      return -1;
  }

  if (param->drcEnabled && assetStaticFieldsParam->embeddedStereoDownmix) {
    /* [u8 nuDRC2ChDmixCode] */
    ret = writeBitsDtsPatcherBitstreamHandle(
      handle,
      param->drcParameters.drc2ChCode,
      8
    );
    if (ret < 0)
      return -1;
  }

  staticFieldsPresent = (NULL != staticFieldsParam);

  if (staticFieldsPresent && staticFieldsParam->mixMetadataEnabled) {
    /* [b1 bMixMetadataPresent] */
    ret = writeBitDtsPatcherBitstreamHandle(
      handle,
      param->mixMetadataPresent
    );
    if (ret < 0)
      return -1;
  }

#if DCA_EXT_SS_DISABLE_MIX_META_SUPPORT
  if (param->mixMetadataPresent)
    LIBBLU_DTS_ERROR_RETURN(
      "Unable to patch mixing data section, "
      "not supported in this compiled program version"
    );
#else
  if (param->mixMetadataPresent) {
    LIBBLU_DTS_ERROR_RETURN("WORK IN PROGRESS LINE %u.\n", __LINE__);
    /**
     * TODO: Complete.
     * Don't forget to update buildDcaExtSSAssetDescriptorDynamicMetadata().
     */
  }
#endif

#if 0
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

size_t computeDcaExtSSAssetDescriptorDecNavDataSize(
  const DcaAudioAssetDescriptorDecoderNavDataParameters * param,
  const DcaAudioAssetDescriptorStaticFieldsParameters * assetStaticFieldsParam,
  const DcaAudioAssetDescriptorDynamicMetadataParameters * dynMetadataParam,
  const DcaExtSSHeaderStaticFieldsParameters * staticFieldsParam,
  const size_t nuBits4ExSSFsize
)
{
  size_t size;

  unsigned ns;

  uint32_t bits4InitLLDecDly;

  /* [u2 nuCodingMode] */
  size = 3; /* Include some other fields */

  switch (param->codingMode) {
    case DCA_EXT_SS_CODING_MODE_DTS_HD_COMPONENTS:
      /* DTS-HD component(s). */

      /* [u12 nuCoreExtensionMask] */
      size += 12;

      if (
        param->codingComponentsUsedMask
        & DCA_EXT_SS_COD_COMP_EXTSUB_CORE_DCA
      ) {
        /* [u14 nuExSSCoreFsize] */
        /* [b1 bExSSCoreSyncPresent] */
        size += 15;

        if (param->codingComponents.extSSCore.syncWordPresent) {
          /* [u2 nuExSSCoreSyncDistInFrames] */
          size += 2;
        }
      }

      if (param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_XBR) {
        /* [u14 nuExSSXBRFsize] */
        size += 14;
      }

      if (param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_XXCH) {
        /* [u14 nuExSSXXCHFsize] */
        size += 14;
      }

      if (param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_X96) {
        /* [u12 nuExSSX96Fsize] */
        size += 12;
      }

      if (param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_LBR) {
        /* [u14 nuExSSLBRFsize] */
        /* [b1 bExSSLBRSyncPresent] */
        size += 15;

        if (param->codingComponents.extSSLbr.syncWordPresent) {
          /* [u2 nuExSSLBRSyncDistInFrames] */
          size += 2;
        }
      }

      if (param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_XLL) {
        /* [u<nuBits4ExSSFsize> nuExSSXLLFsize] */
        /* [b1 bExSSXLLSyncPresent] */
        size += nuBits4ExSSFsize + 1;

        if (param->codingComponents.extSSXll.syncWordPresent) {
          COMPUTE_NUBITSINITDECDLY(
            bits4InitLLDecDly,
            param->codingComponents.extSSXll.initialXllDecodingDelayInFrames
          );

          /* [u4 nuPeakBRCntrlBuffSzkB] */
          /* [u5 nuBitsInitDecDly] */
          /* [u<nuBitsInitDecDly> nuInitLLDecDlyFrames] */
          /* [u<nuBits4ExSSFsize> nuExSSXLLSyncOffset] */
          size += 9 + bits4InitLLDecDly + nuBits4ExSSFsize;
        }
      }

      if (param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_RESERVED_1) {
        /* [v16 *Ignore*] */
        size += 16;
      }

      if (param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_RESERVED_2) {
        /* [v16 *Ignore*] */
        size += 16;
      }

      break;

    case DCA_EXT_SS_CODING_MODE_DTS_HD_LOSSLESS_WITHOUT_CORE:
      /* DTS-HD Master Audio without retro-compatible core. */

      /* [u<nuBits4ExSSFsize> nuExSSXLLFsize] */
      /* [b1 bExSSXLLSyncPresent] */
      size += nuBits4ExSSFsize + 1;

      if (param->codingComponents.extSSXll.syncWordPresent) {
        COMPUTE_NUBITSINITDECDLY(
          bits4InitLLDecDly,
          param->codingComponents.extSSXll.initialXllDecodingDelayInFrames
        );

        /* [u4 nuPeakBRCntrlBuffSzkB] */
        /* [u5 nuBitsInitDecDly] */
        /* [u<nuBitsInitDecDly> nuInitLLDecDlyFrames] */
        /* [u<nuBits4ExSSFsize> nuExSSXLLSyncOffset] */
        size += 9 + bits4InitLLDecDly + nuBits4ExSSFsize;
      }
      break;

    case DCA_EXT_SS_CODING_MODE_DTS_HD_LOW_BITRATE:
      /* DTS-HD Express. */

      /* [u14 nuExSSLBRFsize] */
      /* [b1 bExSSLBRSyncPresent] */
      size += 15;

      if (param->codingComponents.extSSLbr.syncWordPresent) {
        /* [u2 nuExSSLBRSyncDistInFrames] */
        size += 2;
      }
      break;

    case DCA_EXT_SS_CODING_MODE_AUXILIARY_CODING:
      /* Auxiliary audio coding. */

      /* [u14 nuExSSAuxFsize] */
      /* [u8 nuAuxCodecId] */
      /* [b1 bExSSAuxSyncPresent] */
      size += 23;

      if (param->auxilaryCoding.syncWordPresent) {
        /* [u2 nuExSSAuxSyncDistInFrames] */
        size += 2;
      }
  }

  if (param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_XLL) {
    /* Extension Substream contains DTS-XLL component. */

    /* [u3 nuDTSHDStreamID] */
    size += 3;
  }

  if (
    assetStaticFieldsParam->directSpeakersFeed
    && NULL != staticFieldsParam
    && staticFieldsParam->mixMetadataEnabled
    && !dynMetadataParam->mixMetadataPresent
  ) {
    /* [b1 bOnetoOneMixingFlag] */
    size++;
  }

  if (param->mixMetadata.oneTrackToOneChannelMix) {
    /* [b1 bEnblPerChMainAudioScale] */
    size++;

    for (
      ns = 0;
      ns < staticFieldsParam->mixMetadata.nbMixOutputConfigs;
      ns++
    ) {
      if (param->mixMetadata.perChannelMainAudioScaleCode) {
        /**
         * [
         *  u<6 * nNumMixOutCh[ns]>
         *  nuMainAudioScaleCode[ns][nCh]
         * ]
         */
        size += 6 * staticFieldsParam->mixMetadata.nbMixOutputChannels[ns];
      }
      else {
        /* [u6 nuMainAudioScaleCode[ns][0]] */
        size += 6;
      }
    }
  }

  /* [b1 bDecodeAssetInSecondaryDecoder] */
  /* Already counted */

#if DCA_EXT_SS_ENABLE_DRC_2
  if (param->extraDataPresent) {
    /* [b1 bDRCMetadataRev2Present] */
    size++;

    if (param->drcRev2Present) {
      /* [u4 DRCversion_Rev2] */
      size += 4;

      if (param->drcRev2.version == DCA_EXT_SS_DRC_REV_2_VERSION_1) {
        /* [0 nRev2_DRCs] */
        /* [u<8*nRev2_DRCs> DRCCoeff_Rev2[subsubFrame]] */
        size += 8 * (staticFieldsParam->frameDurationCodeValue / 256);
      }
    }
  }
#endif

  return size;
}

int buildDcaExtSSAssetDescriptorDecNavData(
  DtsPatcherBitstreamHandlePtr handle,
  const DcaAudioAssetDescriptorDecoderNavDataParameters * param,
  const DcaAudioAssetDescriptorStaticFieldsParameters * assetStaticFieldsParam,
  const DcaAudioAssetDescriptorDynamicMetadataParameters * dynMetadataParam,
  const DcaExtSSHeaderStaticFieldsParameters * staticFieldsParam,
  const size_t nuBits4ExSSFsize
)
{
  int ret;

  unsigned ns, nCh;

  uint32_t bits4InitLLDecDly;

  /* [u2 nuCodingMode] */
  if (writeBitsDtsPatcherBitstreamHandle(handle, param->codingMode, 2) < 0)
    return -1;

  switch (param->codingMode) {
    case DCA_EXT_SS_CODING_MODE_DTS_HD_COMPONENTS:
      /* DTS-HD component(s). */

      /* [u12 nuCoreExtensionMask] */
      ret = writeBitsDtsPatcherBitstreamHandle(
        handle,
        param->codingComponentsUsedMask,
        12
      );
      if (ret < 0)
        return -1;

      if (
        param->codingComponentsUsedMask
        & DCA_EXT_SS_COD_COMP_EXTSUB_CORE_DCA
      ) {
        /* [u14 nuExSSCoreFsize] */
        ret = writeBitsDtsPatcherBitstreamHandle(
          handle,
          param->codingComponents.extSSCore.size - 1,
          14
        );
        if (ret < 0)
          return -1;

        /* [b1 bExSSCoreSyncPresent] */
        ret = writeBitDtsPatcherBitstreamHandle(
          handle,
          param->codingComponents.extSSCore.syncWordPresent
        );
        if (ret < 0)
          return -1;

        if (param->codingComponents.extSSCore.syncWordPresent) {
          /* [u2 nuExSSCoreSyncDistInFrames] */
          ret = writeBitsDtsPatcherBitstreamHandle(
            handle,
            param->codingComponents.extSSCore.syncDistanceInFramesCode,
            2
          );
          if (ret < 0)
            return -1;
        }
      }

      if (param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_XBR) {
        /* [u14 nuExSSXBRFsize] */
        ret = writeBitsDtsPatcherBitstreamHandle(
          handle,
          param->codingComponents.extSSXbr.size - 1,
          14
        );
        if (ret < 0)
          return -1;
      }

      if (param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_XXCH) {
        /* [u14 nuExSSXXCHFsize] */
        ret = writeBitsDtsPatcherBitstreamHandle(
          handle,
          param->codingComponents.extSSXxch.size - 1,
          14
        );
        if (ret < 0)
          return -1;
      }

      if (param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_X96) {
        /* [u12 nuExSSX96Fsize] */
        ret = writeBitsDtsPatcherBitstreamHandle(
          handle,
          param->codingComponents.extSSX96.size - 1,
          12
        );
        if (ret < 0)
          return -1;
      }

      if (param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_LBR) {
        /* [u14 nuExSSLBRFsize] */
        ret = writeBitsDtsPatcherBitstreamHandle(
          handle,
          param->codingComponents.extSSLbr.size - 1,
          14
        );
        if (ret < 0)
          return -1;

        /* [b1 bExSSLBRSyncPresent] */
        ret = writeBitDtsPatcherBitstreamHandle(
          handle,
          param->codingComponents.extSSLbr.syncWordPresent
        );
        if (ret < 0)
          return -1;

        if (param->codingComponents.extSSLbr.syncWordPresent) {
          /* [u2 nuExSSLBRSyncDistInFrames] */
          ret = writeBitsDtsPatcherBitstreamHandle(
            handle,
            param->codingComponents.extSSLbr.syncDistanceInFramesCode,
            2
          );
        }
      }

      if (param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_XLL) {
        /* [u<nuBits4ExSSFsize> nuExSSXLLFsize] */
        ret = writeBitsDtsPatcherBitstreamHandle(
          handle,
          param->codingComponents.extSSXll.size - 1,
          nuBits4ExSSFsize
        );
        if (ret < 0)
          return -1;

        /* [b1 bExSSXLLSyncPresent] */
        ret = writeBitDtsPatcherBitstreamHandle(
          handle,
          param->codingComponents.extSSXll.syncWordPresent
        );
        if (ret < 0)
          return -1;

        if (param->codingComponents.extSSXll.syncWordPresent) {
          /* [u4 nuPeakBRCntrlBuffSzkB] */
          ret = writeBitsDtsPatcherBitstreamHandle(
            handle,
            param->codingComponents.extSSXll.peakBitRateSmoothingBufSize >> 4,
            4
          );
          if (ret < 0)
            return -1;

          COMPUTE_NUBITSINITDECDLY(
            bits4InitLLDecDly,
            param->codingComponents.extSSXll.initialXllDecodingDelayInFrames
          );

          /* [u5 nuBitsInitDecDly] */
          ret = writeBitsDtsPatcherBitstreamHandle(
            handle,
            bits4InitLLDecDly - 1,
            5
          );
          if (ret < 0)
            return -1;

          /* [u<nuBitsInitDecDly> nuInitLLDecDlyFrames] */
          ret = writeBitsDtsPatcherBitstreamHandle(
            handle,
            param->codingComponents.extSSXll.initialXllDecodingDelayInFrames,
            bits4InitLLDecDly
          );
          if (ret < 0)
            return -1;

          /* [u<nuBits4ExSSFsize> nuExSSXLLSyncOffset] */
          ret = writeBitsDtsPatcherBitstreamHandle(
            handle,
            param->codingComponents.extSSXll.nbBytesOffXllSync,
            nuBits4ExSSFsize
          );
          if (ret < 0)
            return -1;
        }
      }

      if (param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_RESERVED_1) {
        /* [v16 *Ignore*] */
        ret = writeBitsDtsPatcherBitstreamHandle(
          handle,
          param->codingComponents.reservedExtension1_data,
          16
        );
        if (ret < 0)
          return -1;
      }

      if (param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_RESERVED_2) {
        /* [v16 *Ignore*] */
        ret = writeBitsDtsPatcherBitstreamHandle(
          handle,
          param->codingComponents.reservedExtension2_data,
          16
        );
        if (ret < 0)
          return -1;
      }

      break;

    case DCA_EXT_SS_CODING_MODE_DTS_HD_LOSSLESS_WITHOUT_CORE:
      /* DTS-HD Master Audio without retro-compatible core. */
      LIBBLU_DTS_ERROR_RETURN("WORK IN PROGRESS LINE %u.\n", __LINE__);
      break;

#if 0
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
#endif

    case DCA_EXT_SS_CODING_MODE_DTS_HD_LOW_BITRATE:
      /* DTS-HD Express. */
      LIBBLU_DTS_ERROR_RETURN("WORK IN PROGRESS LINE %u.\n", __LINE__);
      break;

#if 0
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
#endif


    case DCA_EXT_SS_CODING_MODE_AUXILIARY_CODING:
      /* Auxiliary audio coding. */
      LIBBLU_DTS_ERROR_RETURN("WORK IN PROGRESS LINE %u.\n", __LINE__);

#if 0
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
#endif
  }

  if (param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_XLL) {
    /* Extension Substream contains DTS-XLL component. */

    /* [u3 nuDTSHDStreamID] */
    ret = writeBitsDtsPatcherBitstreamHandle(
      handle,
      param->codingComponents.extSSXll.steamId,
      3
    );
    if (ret < 0)
      return -1;
  }

  if (
    assetStaticFieldsParam->directSpeakersFeed
    && NULL != staticFieldsParam
    && staticFieldsParam->mixMetadataEnabled
    && !dynMetadataParam->mixMetadataPresent
  ) {
    /* [b1 bOnetoOneMixingFlag] */
    ret = writeBitDtsPatcherBitstreamHandle(
      handle,
      param->mixMetadata.oneTrackToOneChannelMix
    );
    if (ret < 0)
      return -1;
  }

  if (param->mixMetadata.oneTrackToOneChannelMix) {
    /* [b1 bEnblPerChMainAudioScale] */
    ret = writeBitDtsPatcherBitstreamHandle(
      handle,
      param->mixMetadata.perChannelMainAudioScaleCode
    );
    if (ret < 0)
      return -1;

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
          /* [u6 nuMainAudioScaleCode[ns][nCh]] */
          ret = writeBitsDtsPatcherBitstreamHandle(
            handle,
            param->mixMetadata.mainAudioScaleCodes[ns][nCh],
            6
          );
          if (ret < 0)
            return -1;
        }
      }
      else {
        /* [u6 nuMainAudioScaleCode[ns][0]] */
        ret = writeBitsDtsPatcherBitstreamHandle(
          handle,
          param->mixMetadata.mainAudioScaleCodes[ns][0],
          6
        );
        if (ret < 0)
          return -1;
      }
    }
  }

  /* [b1 bDecodeAssetInSecondaryDecoder] */
  ret = writeBitDtsPatcherBitstreamHandle(
    handle,
    param->decodeInSecondaryDecoder
  );
  if (ret < 0)
    return -1;

#if DCA_EXT_SS_ENABLE_DRC_2
  if (param->extraDataPresent) {
    /* [b1 bDRCMetadataRev2Present] */
    ret = writeBitDtsPatcherBitstreamHandle(
      handle,
      param->drcRev2Present
    );
    if (ret < 0)
      return -1;

    if (param->drcRev2Present) {
      /* [u4 DRCversion_Rev2] */
      ret = writeBitsDtsPatcherBitstreamHandle(
        handle,
        param->drcRev2.version,
        4
      );
      if (ret < 0)
        return -1;

      if (param->drcRev2.version == DCA_EXT_SS_DRC_REV_2_VERSION_1) {
        /* [0 nRev2_DRCs] */
        /* [u<8*nRev2_DRCs> DRCCoeff_Rev2[subsubFrame]] */
        LIBBLU_DTS_ERROR_RETURN(
          "Rebuilding does not currently support Rev2 DRC metadata.\n"
        );
      }
    }
  }
#else
  (void) drcCoeffNb;
#endif

  return 0;
}

size_t computeDcaExtSSAudioAssetDescriptorSize(
  const DcaAudioAssetDescriptorParameters * param,
  const DcaExtSSHeaderStaticFieldsParameters * staticFieldsParam,
  const size_t nuBits4ExSSFsize
)
{
  size_t size;

  bool savedReservedField;

  /* -- Main Asset Parameters Section -- */
  /* (Size, Index and Per Stream Static Metadata) */

  /* [u9 nuAssetDescriptFsize] */
  /* [u3 nuAssetIndex] */
  size = 12;

  if (NULL != staticFieldsParam && param->staticFieldsPresent)
    size += computeDcaExtSSAssetDescriptorStaticFieldsSize(
      &param->staticFields
    );

  /* -- Dynamic Metadata Section (DRC, DNC and Mixing Metadata) -- */
  size += computeDcaExtSSAssetDescriptorDynamicMetadataSize(
    &param->dynMetadata,
    &param->staticFields,
    staticFieldsParam
  );

  /* -- Decoder Navigation Data Section (Coding mode...) -- */
  size += computeDcaExtSSAssetDescriptorDecNavDataSize(
    &param->decNavData,
    &param->staticFields,
    &param->dynMetadata,
    staticFieldsParam,
    nuBits4ExSSFsize
  );

  /* [vn Reserved] */
  /* [v0...7 ZeroPadForFsize] */
  savedReservedField = DCA_EXT_SS_IS_SUPP_RES_FIELD_SIZES(
    param->reservedFieldLength,
    param->paddingBits
  );
  if (savedReservedField)
    size += param->reservedFieldLength + param->paddingBits;

  return SHF_ROUND_UP(size, 3); /* Padding if required */
}

int buildDcaExtSSAudioAssetDescriptor(
  DtsPatcherBitstreamHandlePtr handle,
  const DcaAudioAssetDescriptorParameters * param,
  const DcaExtSSHeaderStaticFieldsParameters * staticFieldsParam,
  const size_t nuBits4ExSSFsize,
  const size_t descriptorSize
)
{
  int ret;

  int64_t off, resFieldSize;
  size_t descBoundaryOff;

  /* -- Main Asset Parameters Section (Size, Index and Per Stream Static Metadata) -- */

#if !defined(NDEBUG)
  size_t startOff;

  startOff = handle->currentByteUsedBits + handle->dataUsedLength * 8;
#endif

  getBlockBinaryBoundaryOffDtsPatcherBitstreamHandle(
    handle,
    &descBoundaryOff
  );

  /* [u9 nuAssetDescriptFsize] */
  ret = writeBitsDtsPatcherBitstreamHandle(
    handle,
    descriptorSize - 1,
    9
  );
  if (ret < 0)
    return -1;

  /* [u3 nuAssetIndex] */
  ret = writeBitsDtsPatcherBitstreamHandle(
    handle,
    param->assetIndex,
    3
  );
  if (ret < 0)
    return -1;

  if (NULL != staticFieldsParam && param->staticFieldsPresent) {
    ret = buildDcaExtSSAssetDescriptorStaticFields(
      handle, &param->staticFields
    );
    if (ret < 0)
      return -1;
  }

  /* -- Dynamic Metadata Section (DRC, DNC and Mixing Metadata) -- */
  ret = buildDcaExtSSAssetDescriptorDynamicMetadata(
    handle,
    &param->dynMetadata,
    &param->staticFields,
    staticFieldsParam
  );
  if (ret < 0)
    return -1;

  /* -- Decoder Navigation Data Section (Coding mode...) -- */
  ret = buildDcaExtSSAssetDescriptorDecNavData(
    handle,
    &param->decNavData,
    &param->staticFields,
    &param->dynMetadata,
    staticFieldsParam,
    nuBits4ExSSFsize
  );
  if (ret < 0)
    return -1;

  /* Append if it was saved (meaning size does'nt exceed limit). */
  if (
    DCA_EXT_SS_IS_SUPP_RES_FIELD_SIZES(
      param->reservedFieldLength, param->paddingBits
    )
  ) {
    /* Copy saved reserved field */
    resFieldSize = 8 * param->reservedFieldLength + param->paddingBits;
    for (off = 0; 0 < resFieldSize; off++) {
      ret = writeBitsDtsPatcherBitstreamHandle(
        handle,
        param->reservedFieldData[off],
        MIN(8, resFieldSize)
      );
      if (ret < 0)
        return -1;
      resFieldSize -= 8;
    }
  }

  /* Padding if required */
  ret = blockByteAlignDtsPatcherBitstreamHandle(
    handle,
    descBoundaryOff
  );
  if (ret < 0)
    return -1;

  assert(startOff + descriptorSize * 8 == handle->currentByteUsedBits + handle->dataUsedLength * 8);

  return 0;
}

size_t computeDcaExtSSHeaderSize(
  const DcaExtSSHeaderParameters * param,
  const size_t * assetDescsSizes
)
{
  size_t size;

  unsigned nAst;

  const DcaExtSSHeaderStaticFieldsParameters * staticFieldsParam;

  size_t headerLengthFieldNbBits;
  size_t extSSFrameSizeFieldNbBits;
  size_t assetDescSize;

  /* [v32 SYNCEXTSSH] */
  /* [u8 UserDefinedBits] */
  /* [u2 nExtSSIndex] */
  /* [b1 bHeaderSizeType] */
  /* [u<nuBits4Header> nuExtSSHeaderSize] */
  /* [u<extSubFrmLengthFieldSize> nuExtSSFsize] */
  /* [b1 bStaticFieldsPresent] */

  headerLengthFieldNbBits = (param->longHeaderSizeFlag) ? 12 : 8;
  extSSFrameSizeFieldNbBits = (param->longHeaderSizeFlag) ? 20 : 16;
  size = 60 + headerLengthFieldNbBits + extSSFrameSizeFieldNbBits;

  staticFieldsParam = &param->staticFields;

  if (param->staticFieldsPresent) {
    size += computeDcaExtSSHeaderStaticFieldsSize(
      staticFieldsParam,
      param->extSSIdx
    );
  }

  /* [u<nuBits4ExSSFsize * nuNumAssets> nuAssetFsize[nAst]] */
  /* Included further */

  for (nAst = 0; nAst < staticFieldsParam->nbAudioAssets; nAst++) {
    if (NULL != assetDescsSizes)
      assetDescSize = assetDescsSizes[nAst];
    else
      assetDescSize = computeDcaExtSSAudioAssetDescriptorSize(
        param->audioAssets + nAst,
        staticFieldsParam,
        extSSFrameSizeFieldNbBits
      );

    /* [vn AssetDecriptor()] */
    size +=
      extSSFrameSizeFieldNbBits
      + 8 * assetDescSize
      + 1
    ; /* Include some other fields */
  }

  /* [b<1 * nuNumAssets> bBcCorePresent[nAst]] */
  /* Already included */

  for (nAst = 0; nAst < staticFieldsParam->nbAudioAssets; nAst++) {
    if (param->audioAssetBckwdCompCorePresent[nAst]) {
      /* [u2 nuBcCoreExtSSIndex[nAst]] */
      /* [u3 nuBcCoreAssetIndex[nAst]] */
      size += 5;
    }
  }

  /* [vn Reserved] */
  /* [v0..7 ByteAlign] */
  if (
    DCA_EXT_SS_IS_SUPP_RES_FIELD_SIZES(
      param->reservedFieldLength,
      param->paddingBits
    )
  ) {
    size += param->reservedFieldLength * 8 + param->paddingBits;
  }

  /* [u16 nCRC16ExtSSHeader] */
  /* Already included */

  return SHF_ROUND_UP(size, 3); /* Padding if required */
}

size_t appendDcaExtSSHeader(
  EsmsFileHeaderPtr script,
  const size_t insertingOffset,
  const DcaExtSSHeaderParameters * param
)
{
  DtsPatcherBitstreamHandlePtr handle;
  int ret;

  const uint8_t * array;
  size_t arraySize;

  unsigned nAst;
  int64_t off, resFieldSize;

  const DcaExtSSHeaderStaticFieldsParameters * staticFieldsParam;

  size_t headerLengthFieldNbBits;
  size_t extSSFrameSizeFieldNbBits;
  uint32_t crc16HdrValue;

  size_t extSSHeaderLen;
  size_t extSSAssetDescLen[DCA_EXT_SS_MAX_NB_AUDIO_ASSETS];
  size_t extSSFrameLen;

  staticFieldsParam = &param->staticFields;

  /* Compute size fields */
  headerLengthFieldNbBits = (param->longHeaderSizeFlag) ? 12 : 8;
  extSSFrameSizeFieldNbBits = (param->longHeaderSizeFlag) ? 20 : 16;

  MEMSET_ARRAY(extSSAssetDescLen, 0);
  for (nAst = 0; nAst < staticFieldsParam->nbAudioAssets; nAst++) {
    extSSAssetDescLen[nAst] = computeDcaExtSSAudioAssetDescriptorSize(
      param->audioAssets + nAst,
      staticFieldsParam,
      extSSFrameSizeFieldNbBits
    );
  }

  extSSHeaderLen = computeDcaExtSSHeaderSize(
    param,
    extSSAssetDescLen
  );

  extSSFrameLen = extSSHeaderLen;
  for (nAst = 0; nAst < staticFieldsParam->nbAudioAssets; nAst++)
    extSSFrameLen += param->audioAssetsLengths[nAst];

  /* Create destination bitarray handle */
  if (NULL == (handle = createDtsPatcherBitstreamHandle()))
    return 0;

  /* [v32 SYNCEXTSSH] */
  if (writeBitsDtsPatcherBitstreamHandle(handle, DTS_SYNCWORD_SUBSTREAM, 32) < 0)
    goto free_return;

  /* [u8 UserDefinedBits] */
  if (writeBitsDtsPatcherBitstreamHandle(handle, param->userDefinedBits, 8) < 0)
    goto free_return;

  /* Init DTS ExtSS Header CRC 16 checksum (nCRC16ExtSSHeader) */
  initCrcDtsPatcherBitstreamHandle(
    handle,
    DCA_EXT_SS_CRC_PARAM(),
    DCA_EXT_SS_CRC_INITIAL_V
  );

  /* [u2 nExtSSIndex] */
  if (writeBitsDtsPatcherBitstreamHandle(handle, param->extSSIdx, 2) < 0)
    goto free_return;

  /* [b1 bHeaderSizeType] */
  if (writeBitDtsPatcherBitstreamHandle(handle, param->longHeaderSizeFlag) < 0)
    goto free_return;

  /* [u<nuBits4Header> nuExtSSHeaderSize] */
  ret = writeValueDtsPatcherBitstreamHandle(
    handle,
    extSSHeaderLen - 1,
    headerLengthFieldNbBits
  );
  if (ret < 0)
    goto free_return;

  /* [u<extSubFrmLengthFieldSize> nuExtSSFsize] */
  ret = writeValueDtsPatcherBitstreamHandle(
    handle,
    extSSFrameLen - 1,
    extSSFrameSizeFieldNbBits
  );
  if (ret < 0)
    goto free_return;

  /* [b1 bStaticFieldsPresent] */
  if (writeBitDtsPatcherBitstreamHandle(handle, param->staticFieldsPresent) < 0)
    return -1;

  if (param->staticFieldsPresent) {
    ret = buildDcaExtSSHeaderStaticFields(
      handle,
      staticFieldsParam,
      param->extSSIdx
    );
    if (ret < 0)
      return -1;
  }

  for (nAst = 0; nAst < staticFieldsParam->nbAudioAssets; nAst++) {
    /* [u<nuBits4ExSSFsize> nuAssetFsize[nAst]] */
    ret = writeValueDtsPatcherBitstreamHandle(
      handle,
      param->audioAssetsLengths[nAst] - 1,
      extSSFrameSizeFieldNbBits
    );
    if (ret < 0)
      goto free_return;
  }

  for (nAst = 0; nAst < staticFieldsParam->nbAudioAssets; nAst++) {
    /* [vn AssetDecriptor()] */
    if (
      buildDcaExtSSAudioAssetDescriptor(
        handle,
        param->audioAssets + nAst,
        (param->staticFieldsPresent) ? staticFieldsParam : NULL,
        extSSFrameSizeFieldNbBits,
        extSSAssetDescLen[nAst]
      ) < 0
    )
      goto free_return;
  }

  for (nAst = 0; nAst < staticFieldsParam->nbAudioAssets; nAst++) {
    /* [b1 bBcCorePresent[nAst]] */
    ret = writeBitDtsPatcherBitstreamHandle(
      handle,
      param->audioAssetBckwdCompCorePresent[nAst]
    );
    if (ret < 0)
      goto free_return;
  }

  for (nAst = 0; nAst < staticFieldsParam->nbAudioAssets; nAst++) {
    if (param->audioAssetBckwdCompCorePresent[nAst]) {
      /* [u2 nuBcCoreExtSSIndex[nAst]] */
      ret = writeBitsDtsPatcherBitstreamHandle(
        handle,
        param->audioAssetBckwdCompCore[nAst].extSSIndex,
        2
      );
      if (ret < 0)
        goto free_return;

      /* [u3 nuBcCoreAssetIndex[nAst]] */
      ret = writeBitsDtsPatcherBitstreamHandle(
        handle,
        param->audioAssetBckwdCompCore[nAst].assetIndex,
        3
      );
      if (ret < 0)
        goto free_return;
    }
  }

  /* [vn Reserved] */
  /* Append if it was saved (meaning size does'nt exceed limit). */
  if (
    DCA_EXT_SS_IS_SUPP_RES_FIELD_SIZES(
      param->reservedFieldLength, param->paddingBits
    )
  ) {
    /* Copy saved reserved field */
    resFieldSize = 8 * param->reservedFieldLength + param->paddingBits;
    for (off = 0; 0 < resFieldSize; off++) {
      ret = writeBitsDtsPatcherBitstreamHandle(
        handle,
        param->reservedFieldData[off],
        MIN(8, resFieldSize)
      );
      if (ret < 0)
        goto free_return;
      resFieldSize -= 8;
    }
  }

  /* [v0..7 ByteAlign] */
  if (byteAlignDtsPatcherBitstreamHandle(handle) < 0)
    goto free_return;

  crc16HdrValue = finalizeCrcDtsPatcherBitstreamHandle(handle);

  /* [u16 nCRC16ExtSSHeader] */
  if (writeValueDtsPatcherBitstreamHandle(handle, crc16HdrValue, 16) < 0)
    goto free_return;

  assert(extSSHeaderLen == handle->dataUsedLength);

 /*  lb_print_data(handle->data, handle->dataUsedLength);
  exit(-1); */

  /* Append generated bitstream to the script */
  ret = getGeneratedArrayDtsPatcherBitstreamHandle(
    handle,
    &array,
    &arraySize
  );
  if (ret < 0)
    goto free_return;

  ret = appendAddDataCommand(
    script,
    insertingOffset,
    INSERTION_MODE_ERASE,
    arraySize,
    array
  );
  if (ret < 0)
    goto free_return;

  destroyDtsPatcherBitstreamHandle(handle);
  return arraySize;

free_return:
  destroyDtsPatcherBitstreamHandle(handle);
  return 0;
}

size_t appendDcaExtSSAsset(
  EsmsFileHeaderPtr script,
  const size_t insertingOffset,
  const DcaXllFrameSFPosition * param,
  const unsigned scriptSourceFileId
)
{
  int ret, i;

  size_t origInsertingOffset;
  size_t curInsertingOffset;
  size_t len, off;

  curInsertingOffset = origInsertingOffset = insertingOffset;
  for (i = 0; i < param->nbSourceOffsets; i++) {
    len = param->sourceOffsets[i].len;
    off = param->sourceOffsets[i].off;

#if 0
    lbc_printf("taking %zu bytes from offset 0x%zX.\n", len, off);

    if (off == 0xA30C9)
      exit(EXIT_FAILURE);
#endif

    ret = appendAddPesPayloadCommand(
      script,
      scriptSourceFileId,
      curInsertingOffset,
      off,
      len
    );
    if (ret < 0)
      return 0;
    curInsertingOffset += len;
  }

  return curInsertingOffset - origInsertingOffset;
}