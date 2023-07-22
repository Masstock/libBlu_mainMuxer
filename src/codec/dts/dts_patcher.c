#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include "dts_patcher.h"

#define COMPUTE_NUBITS4MIXOUTMASK(res, masks, nb_masks)                       \
  do {                                                                        \
    unsigned _i, _max;                                                        \
    for (_max = 0, _i = 0; _i < (nb_masks); _i++)                             \
      _max = MAX(_max, (masks)[_i]);                                          \
    unsigned _siz = 4;                                                        \
    for (; _siz < _max; _siz += 4)                                            \
      ;                                                                       \
    *(res) = _siz;                                                            \
  } while(0)

#define WRITE_BITS(hdl, v, s, e)                                              \
  do {                                                                        \
    if (writeBitsDtsPatcherBitstreamHandle(hdl, v, s) < 0)                    \
      e;                                                                      \
  } while (0)

static size_t _computeDcaExtSSHeaderMMSize(
  const DcaExtSSHeaderMixMetadataParameters * param
)
{
  /* [u2 nuMixMetadataAdjLevel] */
  /* [u2 nuBits4MixOutMask] */
  /* [u2 nuNumMixOutConfigs] */
  size_t size = 6;

  /* Output Audio Mix Configurations Loop : */
  /* [u<nuBits4MixOutMask * nuNumMixOutConfigs> nuMixOutChMask[ns]] */
  unsigned nuBits4MixOutMask;
  COMPUTE_NUBITS4MIXOUTMASK(&nuBits4MixOutMask, param->nuMixOutChMask, param->nuNumMixOutConfigs);
  size += nuBits4MixOutMask * param->nuNumMixOutConfigs;

  return size;
}

static size_t _computeDcaExtSSHeaderSFSize(
  const DcaExtSSHeaderSFParameters * param,
  uint8_t nExtSSIndex
)
{
  /* [u2 nuRefClockCode] */
  /* [u3 nuExSSFrameDurationCode] */
  /* [b1 bTimeStampFlag] */
  size_t size = 13; /* Include some other fields */

  if (param->bTimeStampFlag) {
    /* [u32 nuTimeStamp] */
    /* [u4 nLSB] */
    size += 36;
  }

  /* [u3 nuNumAudioPresnt] */
  /* [u3 nuNumAssets] */
  /* Already counted */

  /* [u<(nExtSSIndex+1) * nuNumAudioPresnt> nuNumAssets] */
  size += (nExtSSIndex + 1) * param->nuNumAudioPresnt;

  for (unsigned nAuPr = 0; nAuPr < param->nuNumAudioPresnt; nAuPr++) {
    for (unsigned nSS = 0; nSS < nExtSSIndex + 1u; nSS++) {
      if ((param->nuActiveExSSMask[nAuPr] >> nSS) & 0x1) {
        /* [u8 nuActiveAssetMask[nAuPr][nSS]] */
        size += 8;
      }
    }
  }

  /* [b1 bMixMetadataEnbl] */
  /* Already counted */

  if (param->bMixMetadataEnbl) {
    /* [vn MixMedata()] */
    size += _computeDcaExtSSHeaderMMSize(&param->mixMetadata);
  }

  return size;
}

static int _buildDcaExtSSHeaderSF(
  DtsPatcherBitstreamHandlePtr handle,
  const DcaExtSSHeaderSFParameters * param,
  uint8_t nExtSSIndex
)
{

  /* [u2 nuRefClockCode] */
  WRITE_BITS(handle, param->nuRefClockCode, 2, return -1);

  /* [u3 nuExSSFrameDurationCode] */
  WRITE_BITS(handle, param->nuExSSFrameDurationCode, 3, return -1);

  /* [b1 bTimeStampFlag] */
  WRITE_BITS(handle, param->bTimeStampFlag, 1, return -1);

  if (param->bTimeStampFlag) {
    /* [u32 nuTimeStamp] */
    WRITE_BITS(handle, param->nuTimeStamp, 32, return -1);

    /* [u4 nLSB] */
    WRITE_BITS(handle, param->nLSB, 4, return -1);
  }

  /* [u3 nuNumAudioPresnt] */
  WRITE_BITS(handle, param->nuNumAudioPresnt - 1, 3, return -1);

  /* [u3 nuNumAssets] */
  WRITE_BITS(handle, param->nuNumAssets - 1, 3, return -1);

  for (unsigned nAuPr = 0; nAuPr < param->nuNumAudioPresnt; nAuPr++) {
    /* [u<nExtSSIndex+1> nuNumAssets] */
    WRITE_BITS(handle, param->nuActiveExSSMask[nAuPr], nExtSSIndex + 1, return -1);
  }

  for (unsigned nAuPr = 0; nAuPr < param->nuNumAudioPresnt; nAuPr++) {
    for (unsigned nSS = 0; nSS < (unsigned) nExtSSIndex + 1; nSS++) {
      if ((param->nuActiveExSSMask[nAuPr] >> nSS) & 0x1) {
        /* [u8 nuActiveAssetMask[nAuPr][nSS]] */
        WRITE_BITS(handle, param->nuActiveAssetMask[nAuPr][nSS], 8, return -1);
      }
    }
  }

  /* [b1 bMixMetadataEnbl] */
  WRITE_BITS(handle, param->bMixMetadataEnbl, 1, return -1);

  if (param->bMixMetadataEnbl) {
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

#define COMPUTE_NUNUMBITS4SAMASK(res, mask)                                   \
  do {                                                                        \
    unsigned _siz = 4;                                                        \
    for (; _siz < lb_fast_log2_32(mask); _siz += 4)                           \
      ;                                                                       \
    *(res) = _siz;                                                            \
  } while(0)

static size_t _computeDcaExtSSAssetDescSFSize(
  const DcaAudioAssetDescSFParameters * param
)
{

  /* [b1 bAssetTypeDescrPresent] */
  size_t size = 21; /* Include some other fields */

  if (param->bAssetTypeDescrPresent) {
    /* [u4 nuAssetTypeDescriptor] */
    size += 4;
  }

  /* [b1 bLanguageDescrPresent] */
  /* Already counted */

  if (param->bLanguageDescrPresent) {
    /* [v24 LanguageDescriptor] */
    size += DTS_EXT_SS_LANGUAGE_DESC_SIZE;
  }

  /* [b1 bInfoTextPresent] */
  /* Already counted */

  if (param->bInfoTextPresent) {
    /* [u10 nuInfoTextByteSize] */
    /* [v<nuInfoTextByteSize> InfoTextString] */
    size += 10 + 8 * param->nuInfoTextByteSize;
  }

  /* [u5 nuBitResolution] */
  /* [u4 nuMaxSampleRate] */
  /* [u8 nuTotalNumChs] */
  /* [b1 bOne2OneMapChannels2Speakers] */
  /* Already counted */

  if (param->bOne2OneMapChannels2Speakers) {
    if (2 < param->nuTotalNumChs) {
      /* [b1 bEmbeddedStereoFlag] */
      size++;
    }

    if (6 < param->nuTotalNumChs) {
      /* [b1 bEmbeddedSixChFlag] */
      size++;
    }

    /* [b1 bSpkrMaskEnabled] */
    size += 4; /* Include some other fields */

    unsigned nuNumBits4SAMask = 0;
    if (param->bSpkrMaskEnabled) {
      COMPUTE_NUNUMBITS4SAMASK(&nuNumBits4SAMask, param->nuSpkrActivityMask);

      /* [u2 nuNumBits4SAMask] */
      /* [v<nuNumBits4SAMask> nuSpkrActivityMask] */
      size += 2 + nuNumBits4SAMask;
    }

    /* [u3 nuNumSpkrRemapSets] */
    /* Already counted */

    /* [v<nuNumBits4SAMask * nuNumSpkrRemapSets> nuStndrSpkrLayoutMask] */
    size += nuNumBits4SAMask * param->nuNumSpkrRemapSets;

    for (unsigned ns = 0; ns < param->nuNumSpkrRemapSets; ns++) {
      /* [u5 nuNumDecCh4Remap[ns]] */
      size += 5;

      for (unsigned nCh = 0; nCh < param->nuNumSpeakers[ns]; nCh++) {
        /* [v<nuNumDecCh4Remap> nuRemapDecChMask] */
        /* [u<5 * nCoef> nuSpkrRemapCodes[ns][nCh][nc]] */
        size += param->nuNumDecCh4Remap[ns] + 5 * param->nCoeff[ns][nCh];
      }
    }
  }
  else {
    /* [u3 nuRepresentationType] */
    size += 3;
  }

  return size;
}

static int _buildDcaExtSSAssetDescSF(
  DtsPatcherBitstreamHandlePtr handle,
  const DcaAudioAssetDescSFParameters * param
)
{

  /* [b1 bAssetTypeDescrPresent] */
  WRITE_BITS(handle, param->bAssetTypeDescrPresent, 1, return -1);

  if (param->bAssetTypeDescrPresent) {
    /* [u4 nuAssetTypeDescriptor] */
    WRITE_BITS(handle, param->nuAssetTypeDescriptor, 4, return -1);
  }

  /* [b1 bLanguageDescrPresent] */
  WRITE_BITS(handle, param->bLanguageDescrPresent, 1, return -1);

  if (param->bLanguageDescrPresent) {
    /* [v24 LanguageDescriptor] */
    for (unsigned i = 0; i < DTS_EXT_SS_LANGUAGE_DESC_SIZE; i++) {
      WRITE_BITS(handle, param->LanguageDescriptor[i], 8, return -1);
    }
  }

  /* [b1 bInfoTextPresent] */
  WRITE_BITS(handle, param->bInfoTextPresent, 1, return -1);

  if (param->bInfoTextPresent) {
    /* [u10 nuInfoTextByteSize] */
    WRITE_BITS(handle, param->nuInfoTextByteSize - 1, 10, return -1);

    /* [v<nuInfoTextByteSize> InfoTextString] */
    for (unsigned i = 0; i < param->nuInfoTextByteSize; i++) {
      WRITE_BITS(handle, param->InfoTextString[i], 8, return -1);
    }
  }

  /* [u5 nuBitResolution] */
  WRITE_BITS(handle, param->nuBitResolution - 1, 5, return -1);

  /* [u4 nuMaxSampleRate] */
  WRITE_BITS(handle, param->nuMaxSampleRate, 4, return -1);

  /* [u8 nuTotalNumChs] */
  WRITE_BITS(handle, param->nuTotalNumChs - 1, 8, return -1);

  /* [b1 bOne2OneMapChannels2Speakers] */
  WRITE_BITS(handle, param->bOne2OneMapChannels2Speakers, 1, return -1);

  if (param->bOne2OneMapChannels2Speakers) {
    if (2 < param->nuTotalNumChs) {
      /* [b1 bEmbeddedStereoFlag] */
      WRITE_BITS(handle, param->bEmbeddedStereoFlag, 1, return -1);
    }

    if (6 < param->nuTotalNumChs) {
      /* [b1 bEmbeddedSixChFlag] */
      WRITE_BITS(handle, param->bEmbeddedSixChFlag, 1, return -1);
    }

    /* [b1 bSpkrMaskEnabled] */
    WRITE_BITS(handle, param->bSpkrMaskEnabled, 1, return -1);

    unsigned nuNumBits4SAMask = 0;
    if (param->bSpkrMaskEnabled) {
      COMPUTE_NUNUMBITS4SAMASK(&nuNumBits4SAMask, param->nuSpkrActivityMask);

      /* [u2 nuNumBits4SAMask] */
      WRITE_BITS(handle, (nuNumBits4SAMask >> 2) - 1, 2, return -1);

      /* [v<nuNumBits4SAMask> nuSpkrActivityMask] */
      WRITE_BITS(handle, param->nuSpkrActivityMask, nuNumBits4SAMask, return -1);
    }

    /* [u3 nuNumSpkrRemapSets] */
    WRITE_BITS(handle, param->nuNumSpkrRemapSets, 3, return -1);

    for (unsigned ns = 0; ns < param->nuNumSpkrRemapSets; ns++) {
      /* [v<nuNumBits4SAMask> nuStndrSpkrLayoutMask] */
      WRITE_BITS(handle, param->nuStndrSpkrLayoutMask[ns], nuNumBits4SAMask, return -1);
    }

    for (unsigned ns = 0; ns < param->nuNumSpkrRemapSets; ns++) {
      /* [u5 nuNumDecCh4Remap[ns]] */
      WRITE_BITS(handle, param->nuNumDecCh4Remap[ns] - 1, 5, return -1);

      for (unsigned nCh = 0; nCh < param->nuNumSpeakers[ns]; nCh++) {
        /* [v<nuNumDecCh4Remap> nuRemapDecChMask] */
        WRITE_BITS(
          handle,
          param->nuRemapDecChMask[ns][nCh],
          param->nuNumDecCh4Remap[ns],
          return -1
        );

        for (unsigned nc = 0; nc < param->nCoeff[ns][nCh]; nc++) {
          /* [u5 nuSpkrRemapCodes[ns][nCh][nc]] */
          WRITE_BITS(handle, param->nuSpkrRemapCodes[ns][nCh][nc], 5, return -1);
        }
      }
    }
  }
  else {
    /* [u3 nuRepresentationType] */
    WRITE_BITS(handle, param->nuRepresentationType, 3, return -1);
  }

  return 0;
}

/** \~english
 * \brief Return the length in bits of the dynamic metadata section fields of
 * Extension Substream Asset Descriptor based on given parameters.
 *
 * \param param Ext SS Asset Descriptor dynamic metadata fields parameters.
 * \param ad_sf Ext SS Asset Descriptor static fields
 * parameters.
 * \param sf Ext SS Static Fields parameters. Can be NULL,
 * if so Static Fields are considered as absent.
 * \return size_t Computed length in bits.
 */
static size_t _computeDcaExtSSAssetDescDMSize(
  const DcaAudioAssetDescDMParameters * param,
  const DcaAudioAssetDescSFParameters * ad_sf,
  bool bStaticFieldsPresent,
  const DcaExtSSHeaderSFParameters * sf
)
{

  /* [b1 bDRCCoefPresent] */
  size_t size = 2; /* Include some other fields */

  if (param->bDRCCoefPresent) {
    /* [u8 nuDRCCode] */
    size += 8;
  }

  /* [b1 bDialNormPresent] */
  /* Already counted */

  if (param->bDialNormPresent) {
    /* [u5 nuDialNormCode] */
    size += 5;
  }

  if (param->bDRCCoefPresent && ad_sf->bEmbeddedStereoFlag) {
    /* [u8 nuDRC2ChDmixCode] */
    size += 8;
  }

  if (bStaticFieldsPresent && sf->bMixMetadataEnbl) {
    /* [b1 bMixMetadataPresent] */
    size++;
  }

  /* TODO */

  return size;
}

static int _buildDcaExtSSAssetDescDM(
  DtsPatcherBitstreamHandlePtr handle,
  const DcaAudioAssetDescDMParameters * param,
  const DcaAudioAssetDescSFParameters * ad_sf,
  bool bStaticFieldsPresent,
  const DcaExtSSHeaderSFParameters * sf
)
{

  /* [b1 bDRCCoefPresent] */
  WRITE_BITS(handle, param->bDRCCoefPresent, 1, return -1);

  if (param->bDRCCoefPresent) {
    /* [u8 nuDRCCode] */
    WRITE_BITS(handle, param->nuDRCCode, 8, return -1);
  }

  /* [b1 bDialNormPresent] */
  WRITE_BITS(handle, param->bDialNormPresent, 1, return -1);

  if (param->bDialNormPresent) {
    /* [u5 nuDialNormCode] */
    WRITE_BITS(handle, param->nuDialNormCode, 5, return -1);
  }

  if (param->bDRCCoefPresent && ad_sf->bEmbeddedStereoFlag) {
    /* [u8 nuDRC2ChDmixCode] */
    WRITE_BITS(handle, param->nuDRC2ChDmixCode, 8, return -1);
  }

  if (bStaticFieldsPresent && sf->bMixMetadataEnbl) {
    /* [b1 bMixMetadataPresent] */
    WRITE_BITS(handle, param->bMixMetadataPresent, 1, return -1);
  }

#if DCA_EXT_SS_DISABLE_MIX_META_SUPPORT
  if (param->mixMetadataPresent)
    LIBBLU_DTS_ERROR_RETURN(
      "Unable to patch mixing data section, "
      "not supported in this compiled program version"
    );
#else
  if (param->bMixMetadataPresent) {
    LIBBLU_DTS_ERROR_RETURN("WORK IN PROGRESS LINE %u.\n", __LINE__);
    /**
     * TODO: Complete.
     * Don't forget to update _buildDcaExtSSAssetDescDM().
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
    param->mixMetadata.nuControlMixerDRC = value;

    if (param->mixMetadata.nuControlMixerDRC < 3) {
      /* [u3 nuLimit4EmbeddedDRC] */
      if (readBits(file, &value, 3) < 0)
        return -1;
      param->mixMetadata.limitDRCPriorMix = value;
    }
    else if (param->mixMetadata.nuControlMixerDRC == 3) {
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
      ns < sf->mixMetadata.nuNumMixOutConfigs;
      ns++
    ) {
      if (param->mixMetadata.perMainAudioChSepScal) {
        for (
          nCh = 0;
          nCh < sf->mixMetadata.nbMixOutputChannels[ns];
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
    param->mixMetadata.nEmDM = 1;
    param->mixMetadata.nDecCh[0] = ad_sf->nbChannels;

    if (ad_sf->embeddedSurround6chDownmix) {
      param->mixMetadata.nDecCh[param->mixMetadata.nEmDM] = 6;
      param->mixMetadata.nEmDM++;
    }

    if (ad_sf->embeddedStereoDownmix) {
      param->mixMetadata.nDecCh[param->mixMetadata.nEmDM] = 2;
      param->mixMetadata.nEmDM++;
    }

    for (ns = 0; ns < sf->mixMetadata.nuNumMixOutConfigs; ns++) {
      /* Mix Configurations loop */
      for (nE = 0; nE < param->mixMetadata.nEmDM; nE++) {
        /* Embedded downmix loop */
        for (nCh = 0; nCh < param->mixMetadata.nDecCh[nE]; nCh++) {
          /* Supplemental Channel loop */

          /* [v<nNumMixOutCh[ns]> nuMixMapMask[ns][nE][nCh]] */
          if (
            readBits(
              file, &value,
              sf->mixMetadata.nbMixOutputChannels[ns]
            ) < 0
          )
            return -1;
          param->mixMetadata.nuMixMapMask[ns][nE][nCh] = value;

          /* [0 nuNumMixCoefs[ns][nE][nCh]] */
          param->mixMetadata.nuNumMixCoefs[ns][nE][nCh] =
            _nbBitsSetTo1(param->mixMetadata.nuMixMapMask[ns][nE][nCh])
          ;

          for (nC = 0; nC < param->mixMetadata.nuNumMixCoefs[ns][nE][nCh]; nC++) {
            /* [u6 nuMixCoeffs[ns][nE][nCh][nC]] */
            if (readBits(file, &value, 6) < 0)
              return -1;
            param->mixMetadata.nuMixCoeffs[ns][nE][nCh][nC] = value;
          }
        }
      }
    }
  } /* End if (bMixMetadataPresent) */
#endif

  return 0;
}

#define COMPUTE_NUBITSINITDECDLY(res, delay_value)                            \
  do {                                                                        \
    *(res) = MAX(1, lb_fast_log2_32(delay_value));                            \
  } while(0)

/** \~english
 * \brief Return the length in bits of the decoder navigation data section
 * fields of Extension Substream Asset Descriptor based on given parameters.
 *
 * \param param Ext SS Asset Descriptor decoder navigation data fields
 * parameters.
 * \return size_t Computed length in bits.
 */
static size_t _computeDcaExtSSAssetDescDNDSize(
  const DcaAudioAssetDescDecNDParameters * param,
  const DcaAudioAssetDescSFParameters * ad_sf,
  const DcaAudioAssetDescDMParameters * ad_md,
  bool bStaticFieldsPresent,
  const DcaExtSSHeaderSFParameters * sf,
  unsigned nuBits4ExSSFsize
)
{

  /* [u2 nuCodingMode] */
  size_t size = 3; /* Include some other fields */

  const DcaAudioAssetDescDecNDCodingComponents * cc = &param->coding_components;
  switch (param->nuCodingMode) {
    case DCA_EXT_SS_CODING_MODE_DTS_HD_COMPONENTS:
      /* DTS-HD component(s). */

      /* [u12 nuCoreExtensionMask] */
      size += 12;

      if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_CORE_DCA) {
        /* [u14 nuExSSCoreFsize] */
        /* [b1 bExSSCoreSyncPresent] */
        size += 15;

        if (cc->ExSSCore.bExSSCoreSyncPresent) {
          /* [u2 nuExSSCoreSyncDistInFrames] */
          size += 2;
        }
      }

      if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_XBR) {
        /* [u14 nuExSSXBRFsize] */
        size += 14;
      }

      if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_XXCH) {
        /* [u14 nuExSSXXCHFsize] */
        size += 14;
      }

      if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_X96) {
        /* [u12 nuExSSX96Fsize] */
        size += 12;
      }

      if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_LBR) {
        /* [u14 nuExSSLBRFsize] */
        /* [b1 bExSSLBRSyncPresent] */
        size += 15;

        if (cc->ExSSLBR.bExSSLBRSyncPresent) {
          /* [u2 nuExSSLBRSyncDistInFrames] */
          size += 2;
        }
      }

      if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_XLL) {
        /* [u<nuBits4ExSSFsize> nuExSSXLLFsize] */
        /* [b1 bExSSXLLSyncPresent] */
        size += nuBits4ExSSFsize + 1;

        if (cc->ExSSXLL.bExSSXLLSyncPresent) {
          unsigned nuBitsInitDecDly;
          COMPUTE_NUBITSINITDECDLY(
            &nuBitsInitDecDly,
            cc->ExSSXLL.nuInitLLDecDlyFrames
          );

          /* [u4 nuPeakBRCntrlBuffSzkB] */
          /* [u5 nuBitsInitDecDly] */
          /* [u<nuBitsInitDecDly> nuInitLLDecDlyFrames] */
          /* [u<nuBits4ExSSFsize> nuExSSXLLSyncOffset] */
          size += 9 + nuBitsInitDecDly + nuBits4ExSSFsize;
        }
      }

      if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_RESERVED_1) {
        /* [v16 *Ignore*] */
        size += 16;
      }

      if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_RESERVED_2) {
        /* [v16 *Ignore*] */
        size += 16;
      }

      break;

    case DCA_EXT_SS_CODING_MODE_DTS_HD_LOSSLESS_WITHOUT_CORE:
      /* DTS-HD Master Audio without retro-compatible core. */

      /* [u<nuBits4ExSSFsize> nuExSSXLLFsize] */
      /* [b1 bExSSXLLSyncPresent] */
      size += nuBits4ExSSFsize + 1;

      if (cc->ExSSXLL.bExSSXLLSyncPresent) {
        unsigned nuBitsInitDecDly;
        COMPUTE_NUBITSINITDECDLY(
          &nuBitsInitDecDly,
          cc->ExSSXLL.nuInitLLDecDlyFrames
        );

        /* [u4 nuPeakBRCntrlBuffSzkB] */
        /* [u5 nuBitsInitDecDly] */
        /* [u<nuBitsInitDecDly> nuInitLLDecDlyFrames] */
        /* [u<nuBits4ExSSFsize> nuExSSXLLSyncOffset] */
        size += 9 + nuBitsInitDecDly + nuBits4ExSSFsize;
      }
      break;

    case DCA_EXT_SS_CODING_MODE_DTS_HD_LOW_BITRATE:
      /* DTS-HD Express. */

      /* [u14 nuExSSLBRFsize] */
      /* [b1 bExSSLBRSyncPresent] */
      size += 15;

      if (cc->ExSSLBR.bExSSLBRSyncPresent) {
        /* [u2 nuExSSLBRSyncDistInFrames] */
        size += 2;
      }
      break;

    case DCA_EXT_SS_CODING_MODE_AUXILIARY_CODING:
      /* Auxiliary audio coding. */

      /* [u14 nuExSSAuxFsize] */
      /* [u8 nuAuxCodecID] */
      /* [b1 bExSSAuxSyncPresent] */
      size += 23;

      if (param->auxilary_coding.bExSSAuxSyncPresent) {
        /* [u2 nuExSSAuxSyncDistInFrames] */
        size += 2;
      }
  }

  if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_XLL) {
    /* Extension Substream contains DTS-XLL component. */

    /* [u3 nuDTSHDStreamID] */
    size += 3;
  }

  if (
    ad_sf->bOne2OneMapChannels2Speakers
    && bStaticFieldsPresent
    && sf->bMixMetadataEnbl
    && !ad_md->bMixMetadataPresent
  ) {
    /* [b1 bOnetoOneMixingFlag] */
    size++;
  }

  if (param->bOnetoOneMixingFlag) {
    /* [b1 bEnblPerChMainAudioScale] */
    size++;

    for (unsigned ns = 0; ns < sf->mixMetadata.nuNumMixOutConfigs; ns++) {
      if (param->bEnblPerChMainAudioScale) {
        /* [u<6 * nNumMixOutCh[ns]> nuMainAudioScaleCode[ns][nCh]] */
        size += 6 * sf->mixMetadata.nNumMixOutCh[ns];
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
  if (param->extra_data_pres) {
    /* [b1 bDRCMetadataRev2Present] */
    size++;

    if (param->bDRCMetadataRev2Present) {
      /* [u4 DRCversion_Rev2] */
      size += 4;

      if (DCA_EXT_SS_DRC_REV_2_VERSION_1 == param->DRCversion_Rev2) {
        /* [0 nRev2_DRCs] */
        /* [u<8*nRev2_DRCs> DRCCoeff_Rev2[subsubFrame]] */
        size += 8 * (sf->nuExSSFrameDurationCode / 256);
      }
    }
  }
#endif

  return size;
}

static int _buildDcaExtSSAssetDescDND(
  DtsPatcherBitstreamHandlePtr handle,
  const DcaAudioAssetDescDecNDParameters * param,
  const DcaAudioAssetDescSFParameters * ad_sf,
  const DcaAudioAssetDescDMParameters * ad_dm,
  bool bStaticFieldsPresent,
  const DcaExtSSHeaderSFParameters * sf,
  size_t nuBits4ExSSFsize
)
{

  /* [u2 nuCodingMode] */
  WRITE_BITS(handle, param->nuCodingMode, 2, return -1);

  const DcaAudioAssetDescDecNDCodingComponents * cc = &param->coding_components;
  switch (param->nuCodingMode) {
    case DCA_EXT_SS_CODING_MODE_DTS_HD_COMPONENTS:
      /* DTS-HD component(s). */

      /* [u12 nuCoreExtensionMask] */
      WRITE_BITS(handle, param->nuCoreExtensionMask, 12, return -1);

      if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_CORE_DCA) {
        const DcaAudioAssetExSSCoreParameters * p = &cc->ExSSCore;

        /* [u14 nuExSSCoreFsize] */
        WRITE_BITS(handle, p->nuExSSCoreFsize - 1, 14, return -1);

        /* [b1 bExSSCoreSyncPresent] */
        WRITE_BITS(handle, p->bExSSCoreSyncPresent, 1, return -1);

        if (p->bExSSCoreSyncPresent) {
          /* [u2 nuExSSCoreSyncDistInFrames] */
          WRITE_BITS(handle, p->nuExSSCoreSyncDistInFrames, 2, return -1);
        }
      }

      if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_XBR) {
        const DcaAudioAssetExSSXBRParameters * p = &cc->ExSSXBR;

        /* [u14 nuExSSXBRFsize] */
        WRITE_BITS(handle, p->nuExSSXBRFsize - 1, 14, return -1);
      }

      if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_XXCH) {
        const DcaAudioAssetExSSXXCHParameters * p = &cc->ExSSXXCH;

        /* [u14 nuExSSXXCHFsize] */
        WRITE_BITS(handle, p->nuExSSXXCHFsize - 1, 14, return -1);
      }

      if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_X96) {
        const DcaAudioAssetExSSX96Parameters * p = &cc->ExSSX96;

        /* [u12 nuExSSX96Fsize] */
        WRITE_BITS(handle, p->nuExSSX96Fsize - 1, 12, return -1);
      }

      if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_LBR) {
        const DcaAudioAssetExSSLBRParameters * p = &cc->ExSSLBR;

        /* [u14 nuExSSLBRFsize] */
        WRITE_BITS(handle, p->nuExSSLBRFsize - 1, 14, return -1);

        /* [b1 bExSSLBRSyncPresent] */
        WRITE_BITS(handle, p->bExSSLBRSyncPresent, 1, return -1);

        if (p->bExSSLBRSyncPresent) {
          /* [u2 nuExSSLBRSyncDistInFrames] */
          WRITE_BITS(handle, p->nuExSSLBRSyncDistInFrames, 2, return -1);
        }
      }

      if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_XLL) {
        const DcaAudioAssetExSSXllParameters * p = &cc->ExSSXLL;

        /* [u<nuBits4ExSSFsize> nuExSSXLLFsize] */
        WRITE_BITS(handle, p->nuExSSXLLFsize - 1, nuBits4ExSSFsize, return -1);

        /* [b1 bExSSXLLSyncPresent] */
        WRITE_BITS(handle, p->bExSSXLLSyncPresent, 1, return -1);

        if (p->bExSSXLLSyncPresent) {
          /* [u4 nuPeakBRCntrlBuffSzkB] */
          WRITE_BITS(handle, p->nuPeakBRCntrlBuffSzkB >> 4, 4, return -1);

          uint32_t nuBitsInitDecDly;
          COMPUTE_NUBITSINITDECDLY(&nuBitsInitDecDly, p->nuInitLLDecDlyFrames);

          /* [u5 nuBitsInitDecDly] */
          WRITE_BITS(handle, nuBitsInitDecDly - 1, 5, return -1);

          /* [u<nuBitsInitDecDly> nuInitLLDecDlyFrames] */
          WRITE_BITS(handle, p->nuInitLLDecDlyFrames, nuBitsInitDecDly, return -1);

          /* [u<nuBits4ExSSFsize> nuExSSXLLSyncOffset] */
          WRITE_BITS(handle, p->nuExSSXLLSyncOffset, nuBits4ExSSFsize, return -1);
        }
      }

      if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_RESERVED_1) {
        /* [v16 *Ignore*] */
        WRITE_BITS(handle, cc->res_ext_1_data, 16, return -1);
      }

      if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_RESERVED_2) {
        /* [v16 *Ignore*] */
        WRITE_BITS(handle, cc->res_ext_2_data, 16, return -1);
      }
      break;

    case DCA_EXT_SS_CODING_MODE_DTS_HD_LOSSLESS_WITHOUT_CORE:
      /* DTS-HD Master Audio without retro-compatible core. */
      LIBBLU_DTS_ERROR_RETURN("WORK IN PROGRESS LINE %u.\n", __LINE__);
      break;

#if 0
      param->nuCoreExtensionMask = DCA_EXT_SS_COD_COMP_EXTSUB_XLL;

      /* [u<nuBits4ExSSFsize> nuExSSXLLFsize] */
      if (readBits(file, &value, nuBits4ExSSFsize) < 0)
        return -1;
      cc->extSSXll.size = value + 1;

      /* [b1 bExSSXLLSyncPresent] */
      if (readBits(file, &value, 1) < 0)
        return -1;
      cc->extSSXll.syncWordPresent = value;

      if (cc->extSSXll.syncWordPresent) {
        /* [u4 nuPeakBRCntrlBuffSzkB] */
        if (readBits(file, &value, 4) < 0)
          return -1;
        cc->extSSXll.peakBitRateSmoothingBufSizeCode = value;
        cc->extSSXll.peakBitRateSmoothingBufSize = value << 4;

        /* [u5 nuBitsInitDecDly] */
        if (readBits(file, &value, 5) < 0)
          return -1;

        /* [u<nuBitsInitDecDly> nuInitLLDecDlyFrames] */
        if (readBits(file, &value, value + 1) < 0)
          return -1;
        cc->extSSXll.initialXllDecodingDelayInFrames = value;

        /* [u<nuBits4ExSSFsize> nuExSSXLLSyncOffset] */
        if (readBits(file, &value, nuBits4ExSSFsize) < 0)
          return -1;
        cc->extSSXll.nbBytesOffXllSync = value;
      }
#endif

    case DCA_EXT_SS_CODING_MODE_DTS_HD_LOW_BITRATE:
      /* DTS-HD Express. */
      LIBBLU_DTS_ERROR_RETURN("WORK IN PROGRESS LINE %u.\n", __LINE__);
      break;

#if 0
    case DCA_EXT_SS_CODING_MODE_DTS_HD_LOW_BITRATE:
      /* DTS-HD Express. */

      param->nuCoreExtensionMask = DCA_EXT_SS_COD_COMP_EXTSUB_LBR;

      /* [u14 nuExSSLBRFsize] */
      if (readBits(file, &value, 14) < 0)
        return -1;
      cc->extSSLbr.size = value + 1;

      /* [b1 bExSSLBRSyncPresent] */
      if (readBits(file, &value, 1) < 0)
        return -1;
      cc->extSSLbr.syncWordPresent = value;

      if (cc->extSSLbr.syncWordPresent) {
        /* [u2 nuExSSLBRSyncDistInFrames] */
        if (readBits(file, &value, 2) < 0)
          return -1;
        cc->extSSLbr.syncDistanceInFramesCode = value;
        cc->extSSLbr.syncDistanceInFrames = 1 << value;
      }
      break;
#endif


    case DCA_EXT_SS_CODING_MODE_AUXILIARY_CODING:
      /* Auxiliary audio coding. */
      LIBBLU_DTS_ERROR_RETURN("WORK IN PROGRESS LINE %u.\n", __LINE__);

#if 0
    case DCA_EXT_SS_CODING_MODE_AUXILIARY_CODING:
      /* Auxiliary audio coding. */

      param->nuCoreExtensionMask = 0;

      /* [u14 nuExSSAuxFsize] */
      if (readBits(file, &value, 14) < 0)
        return -1;
      param->auxilary_coding.size = value + 1;

      /* [u8 nuAuxCodecID] */
      if (readBits(file, &value, 8) < 0)
        return -1;
      param->auxilary_coding.auxCodecId = value;

      /* [b1 bExSSAuxSyncPresent] */
      if (readBits(file, &value, 1) < 0)
        return -1;
      param->auxilary_coding.syncWordPresent = value;

      if (param->auxilary_coding.syncWordPresent) {
        /* [u2 nuExSSAuxSyncDistInFrames] */
        if (readBits(file, &value, 2) < 0)
          return -1;
        param->auxilary_coding.syncDistanceInFramesCode = value;
        param->auxilary_coding.syncDistanceInFrames = 1 << value;
      }
#endif
  }

  if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_XLL) {
    /* Extension Substream contains DTS-XLL component. */
    const DcaAudioAssetExSSXllParameters * p = &cc->ExSSXLL;

    /* [u3 nuDTSHDStreamID] */
    WRITE_BITS(handle, p->nuDTSHDStreamID, 3, return -1);
  }

  if (
    ad_sf->bOne2OneMapChannels2Speakers
    && bStaticFieldsPresent
    && sf->bMixMetadataEnbl
    && !ad_dm->bMixMetadataPresent
  ) {
    /* [b1 bOnetoOneMixingFlag] */
    WRITE_BITS(handle, param->bOnetoOneMixingFlag, 1, return -1);
  }

  if (param->bOnetoOneMixingFlag) {
    /* [b1 bEnblPerChMainAudioScale] */
    WRITE_BITS(handle, param->bEnblPerChMainAudioScale, 1, return -1);

    for (unsigned ns = 0; ns < sf->mixMetadata.nuNumMixOutConfigs; ns++) {
      if (param->bEnblPerChMainAudioScale) {
        for (unsigned nCh = 0; nCh < sf->mixMetadata.nNumMixOutCh[ns]; nCh++) {
          /* [u6 nuMainAudioScaleCode[ns][nCh]] */
          WRITE_BITS(handle, param->nuMainAudioScaleCode[ns][nCh], 6, return -1);
        }
      }
      else {
        /* [u6 nuMainAudioScaleCode[ns][0]] */
        WRITE_BITS(handle, param->nuMainAudioScaleCode[ns][0], 6, return -1);
      }
    }
  }

  /* [b1 bDecodeAssetInSecondaryDecoder] */
  WRITE_BITS(handle, param->bDecodeAssetInSecondaryDecoder, 1, return -1);

#if DCA_EXT_SS_ENABLE_DRC_2
  if (param->extra_data_pres) {
    /* [b1 bDRCMetadataRev2Present] */
    WRITE_BITS(handle, param->bDRCMetadataRev2Present, 1, return -1);

    if (param->bDRCMetadataRev2Present) {
      /* [u4 DRCversion_Rev2] */
      WRITE_BITS(handle, param->DRCversion_Rev2, 4, return -1);

      if (DCA_EXT_SS_DRC_REV_2_VERSION_1 == param->DRCversion_Rev2) {
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

/** \~english
 * \brief Return the length in bytes of the Extension Substream Asset
 * Descriptor based on given parameters.
 *
 * \param param Ext SS Asset Descriptor parameters.
 * \param sf Ext SS Static Fields parameters. Can be NULL, if
 * so Static Fields are considered as absent.
 * \param nuBits4ExSSFsize 'nuBits4ExSSFsize' value, the size of Ext SS frame
 * size fields. Defined by 'bHeaderSizeType' field in Ext SS Header.
 * \return size_t Computed length in bytes.
 */
static size_t _computeDcaExtSSAudioAssetDescriptorSize(
  const DcaAudioAssetDescParameters * param,
  bool bStaticFieldsPresent,
  const DcaExtSSHeaderSFParameters * sf,
  uint32_t nuBits4ExSSFsize
)
{
  /* -- Main Asset Parameters Section -- */
  /* (Size, Index and Per Stream Static Metadata) */

  /* [u9 nuAssetDescriptFsize] */
  /* [u3 nuAssetIndex] */
  size_t size = 12;

  if (bStaticFieldsPresent)
    size += _computeDcaExtSSAssetDescSFSize(&param->static_fields);

  /* -- Dynamic Metadata Section (DRC, DNC and Mixing Metadata) -- */
  size += _computeDcaExtSSAssetDescDMSize(
    &param->dyn_md,
    &param->static_fields,
    bStaticFieldsPresent,
    sf
  );

  /* -- Decoder Navigation Data Section (Coding mode...) -- */
  size += _computeDcaExtSSAssetDescDNDSize(
    &param->dec_nav_data,
    &param->static_fields,
    &param->dyn_md,
    bStaticFieldsPresent,
    sf,
    nuBits4ExSSFsize
  );

  /* [vn Reserved] */
  /* [v0...7 ZeroPadForFsize] */
  if (isSavedReservedFieldDcaExtSS(param->Reserved_size, param->ZeroPadForFsize_size))
    size += param->Reserved_size + param->ZeroPadForFsize_size;

  return SHF_ROUND_UP(size, 3); /* Padding if required */
}

/** \~english
 * \brief Build an Extension Substream Audio Asset Descriptor based on given
 * parameters.
 *
 * \param handle Destination Bitstream builder handle.
 * \param param Ext SS Asset Descriptor parameters.
 * \param sf Ext SS Static Fields parameters. Can be NULL, if
 * so Static Fields are considered as absent.
 * \param nuBits4ExSSFsize 'nuBits4ExSSFsize' value, the size of Ext SS frame
 * size fields. Defined by 'bHeaderSizeType' field in Ext SS Header.
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
static int _buildDcaExtSSAudioAssetDescriptor(
  DtsPatcherBitstreamHandlePtr handle,
  const DcaAudioAssetDescParameters * param,
  bool bStaticFieldsPresent,
  const DcaExtSSHeaderSFParameters * sf,
  unsigned nuBits4ExSSFsize,
  uint16_t nuAssetDescriptFsize
)
{
  int ret;

  /* -- Main Asset Parameters Section (Size, Index and Per Stream Static Metadata) -- */

#if !defined(NDEBUG)
  size_t start_off = handle->currentByteUsedBits + handle->dataUsedLength * 8;
#endif

  size_t desc_boundary_off;
  getBlockBinaryBoundaryOffDtsPatcherBitstreamHandle(
    handle,
    &desc_boundary_off
  );

  /* [u9 nuAssetDescriptFsize] */
  WRITE_BITS(handle, nuAssetDescriptFsize - 1, 9, return -1);

  /* [u3 nuAssetIndex] */
  WRITE_BITS(handle, param->nuAssetIndex, 3, return -1);

  if (bStaticFieldsPresent) {
    if (_buildDcaExtSSAssetDescSF(handle, &param->static_fields) < 0)
      return -1;
  }

  /* -- Dynamic Metadata Section (DRC, DNC and Mixing Metadata) -- */
  ret = _buildDcaExtSSAssetDescDM(
    handle,
    &param->dyn_md,
    &param->static_fields,
    bStaticFieldsPresent,
    sf
  );
  if (ret < 0)
    return -1;

  /* -- Decoder Navigation Data Section (Coding mode...) -- */
  ret = _buildDcaExtSSAssetDescDND(
    handle,
    &param->dec_nav_data,
    &param->static_fields,
    &param->dyn_md,
    bStaticFieldsPresent,
    sf,
    nuBits4ExSSFsize
  );
  if (ret < 0)
    return -1;

  /* Append if it was saved (meaning size does'nt exceed limit). */
  if (isSavedReservedFieldDcaExtSS(param->Reserved_size, param->ZeroPadForFsize_size)) {
    /* Copy saved reserved field */
    for (unsigned off = 0; off < param->Reserved_size; off++) {
      WRITE_BITS(handle, param->Reserved[off], 8, return -1);
    }

    uint8_t ZeroPadForFsize = param->Reserved[param->Reserved_size];
    WRITE_BITS(handle, ZeroPadForFsize, param->ZeroPadForFsize_size, return -1);
  }

  /* Padding if required */
  ret = blockByteAlignDtsPatcherBitstreamHandle(
    handle,
    desc_boundary_off
  );
  if (ret < 0)
    return -1;

  assert(
    start_off + nuAssetDescriptFsize * 8
    == handle->currentByteUsedBits + handle->dataUsedLength * 8
  );

  return 0;
}

/** \~english
 * \brief Return the length in bytes of the Extension Substream header based
 * on given parameters.
 *
 * \param param Ext SS parameters.
 * \param assetDescsSizes Optionnal assets descriptors sizes. Avoid
 * re-computation if already done. Can be NULL, disabling this functionnality.
 * The size of the array must be equal to the specified number of assets in
 * given Ext SS parameters.
 * \return uint16_t Computed length in bytes.
 */
static uint16_t _computeDcaExtSSHeaderSize(
  const DcaExtSSHeaderParameters * param,
  uint16_t nuAssetDescriptFsizeArr[static DCA_EXT_SS_MAX_NB_AUDIO_ASSETS]
)
{

  /* [v32 SYNCEXTSSH] */
  /* [u8 UserDefinedBits] */
  /* [u2 nExtSSIndex] */
  /* [b1 bHeaderSizeType] */
  /* [u<nuBits4Header> nuExtSSHeaderSize] */
  /* [u<extSubFrmLengthFieldSize> nuExtSSFsize] */
  /* [b1 bStaticFieldsPresent] */

  unsigned nuBits4Header    = (param->bHeaderSizeType) ? 12 : 8;
  unsigned nuBits4ExSSFsize = (param->bHeaderSizeType) ? 20 : 16;
  uint16_t size = 60 + nuBits4Header + nuBits4ExSSFsize;

  const DcaExtSSHeaderSFParameters * sf = &param->static_fields;

  if (param->bStaticFieldsPresent) {
    size += _computeDcaExtSSHeaderSFSize(
      sf,
      param->nExtSSIndex
    );
  }

  /* [u<nuBits4ExSSFsize * nuNumAssets> nuAssetFsize[nAst]] */
  /* Included further */

  for (unsigned nAst = 0; nAst < sf->nuNumAssets; nAst++) {
    uint16_t nuAssetDescriptFsize = nuAssetDescriptFsizeArr[nAst];

    /* [vn AssetDecriptor()] */
    size +=
      nuBits4ExSSFsize
      + 8 * nuAssetDescriptFsize
      + 1
    ; /* Include some other fields */
  }

  /* [b<1 * nuNumAssets> bBcCorePresent[nAst]] */
  /* Already included */

  for (unsigned nAst = 0; nAst < sf->nuNumAssets; nAst++) {
    if (param->bBcCorePresent[nAst]) {
      /* [u2 nuBcCoreExtSSIndex[nAst]] */
      /* [u3 nuBcCoreAssetIndex[nAst]] */
      size += 5;
    }
  }

  /* [vn Reserved] */
  /* [v0..7 ByteAlign] */
  if (isSavedReservedFieldDcaExtSS(param->Reserved_size, param->ZeroPadForFsize_size)) {
    size += param->Reserved_size * 8 + param->ZeroPadForFsize_size;
  }

  /* [u16 nCRC16ExtSSHeader] */
  /* Already included */

  return SHF_ROUND_UP(size, 3); /* Padding if required */
}

uint32_t appendDcaExtSSHeader(
  EsmsFileHeaderPtr script,
  uint32_t insert_off,
  const DcaExtSSHeaderParameters * param
)
{
  const DcaExtSSHeaderSFParameters * sf = &param->static_fields;

  /* Compute size fields */
  unsigned nuBits4Header    = (param->bHeaderSizeType) ? 12 : 8;
  unsigned nuBits4ExSSFsize = (param->bHeaderSizeType) ? 20 : 16;

  uint16_t nuAssetDescriptFsizeArr[DCA_EXT_SS_MAX_NB_AUDIO_ASSETS];
  MEMSET_ARRAY(nuAssetDescriptFsizeArr, 0);

  for (unsigned nAst = 0; nAst < sf->nuNumAssets; nAst++) {
    nuAssetDescriptFsizeArr[nAst] = _computeDcaExtSSAudioAssetDescriptorSize(
      &param->audioAssets[nAst],
      param->bStaticFieldsPresent,
      sf,
      nuBits4ExSSFsize
    );
  }

  uint16_t nuExtSSHeaderSize = _computeDcaExtSSHeaderSize(
    param,
    nuAssetDescriptFsizeArr
  );

  uint32_t nuExtSSFsize = nuExtSSHeaderSize;
  for (unsigned nAst = 0; nAst < sf->nuNumAssets; nAst++)
    nuExtSSFsize += param->audioAssetsLengths[nAst];

  /* Create destination bitarray handle */
  DtsPatcherBitstreamHandlePtr handle;
  if (NULL == (handle = createDtsPatcherBitstreamHandle()))
    return 0;

  /* [v32 SYNCEXTSSH] */
  WRITE_BITS(handle, DCA_SYNCEXTSSH, 32, return -1);

  /* [u8 UserDefinedBits] */
  WRITE_BITS(handle, param->UserDefinedBits, 8, return -1);

  /* Init DTS ExtSS Header CRC 16 checksum (nCRC16ExtSSHeader) */
  initCrcDtsPatcherBitstreamHandle(
    handle,
    DCA_EXT_SS_CRC_PARAM(),
    DCA_EXT_SS_CRC_INITIAL_V
  );

  /* [u2 nExtSSIndex] */
  WRITE_BITS(handle, param->nExtSSIndex, 2, return -1);

  /* [b1 bHeaderSizeType] */
  WRITE_BITS(handle, param->bHeaderSizeType, 1, return -1);

  /* [u<nuBits4Header> nuExtSSHeaderSize] */
  WRITE_BITS(handle, nuExtSSHeaderSize - 1, nuBits4Header, return -1);

  /* [u<nuBits4ExSSFsize> nuExtSSFsize] */
  WRITE_BITS(handle, nuExtSSFsize - 1, nuBits4ExSSFsize, return -1);

  /* [b1 bStaticFieldsPresent] */
  WRITE_BITS(handle, param->bStaticFieldsPresent, 1, return -1);

  if (param->bStaticFieldsPresent) {
    if (_buildDcaExtSSHeaderSF(handle, sf, param->nExtSSIndex) < 0)
      return -1;
  }

  for (unsigned nAst = 0; nAst < sf->nuNumAssets; nAst++) {
    /* [u<nuBits4ExSSFsize> nuAssetFsize[nAst]] */
    uint32_t nuAssetFsize = param->audioAssetsLengths[nAst] - 1;
    WRITE_BITS(handle, nuAssetFsize, nuBits4ExSSFsize, return -1);
  }

  for (unsigned nAst = 0; nAst < sf->nuNumAssets; nAst++) {
    /* [vn AssetDecriptor()] */
    int ret = _buildDcaExtSSAudioAssetDescriptor(
      handle,
      &param->audioAssets[nAst],
      param->bStaticFieldsPresent, sf,
      nuBits4ExSSFsize,
      nuAssetDescriptFsizeArr[nAst]
    );
    if (ret < 0)
      goto free_return;
  }

  for (unsigned nAst = 0; nAst < sf->nuNumAssets; nAst++) {
    /* [b1 bBcCorePresent[nAst]] */
    WRITE_BITS(handle, param->bBcCorePresent[nAst], 1, return -1);
  }

  for (unsigned nAst = 0; nAst < sf->nuNumAssets; nAst++) {
    if (param->bBcCorePresent[nAst]) {
      /* [u2 nuBcCoreExtSSIndex[nAst]] */
      uint8_t nuBcCoreExtSSIndex = param->nuBcCoreExtSSIndex[nAst];
      WRITE_BITS(handle, nuBcCoreExtSSIndex, 2, return -1);

      /* [u3 nuBcCoreAssetIndex[nAst]] */
      uint8_t nuBcCoreAssetIndex = param->nuBcCoreAssetIndex[nAst];
      WRITE_BITS(handle, nuBcCoreAssetIndex, 3, return -1);
    }
  }

  /* [vn Reserved] */
  /* Append if it was saved (meaning size doesn't exceed limit). */
  if (isSavedReservedFieldDcaExtSS(param->Reserved_size, param->ZeroPadForFsize_size)) {
    /* Copy saved reserved field */
    for (unsigned off = 0; off < param->Reserved_size; off++) {
      WRITE_BITS(handle, param->Reserved[off], 8, return -1);
    }
    uint8_t pad_bits = param->Reserved[param->Reserved_size];
    WRITE_BITS(handle, pad_bits, param->ZeroPadForFsize_size, return -1);
  }

  /* [v0..7 ByteAlign] */
  if (byteAlignDtsPatcherBitstreamHandle(handle) < 0)
    goto free_return;

  /* [u16 nCRC16ExtSSHeader] */
  uint16_t nCRC16ExtSSHeader = finalizeCrcDtsPatcherBitstreamHandle(handle);
  WRITE_BITS(handle, nCRC16ExtSSHeader, 16, return -1);

  assert(nuExtSSHeaderSize == handle->dataUsedLength);

  /* Append generated bitstream to the script */
  const uint8_t * arr;
  size_t arr_size;
  if (getGeneratedArrayDtsPatcherBitstreamHandle(handle, &arr, &arr_size) < 0)
    goto free_return;

  if (appendAddDataCommand(script, insert_off, INSERTION_MODE_ERASE, arr_size, arr) < 0)
    goto free_return;

  destroyDtsPatcherBitstreamHandle(handle);
  return arr_size;

free_return:
  destroyDtsPatcherBitstreamHandle(handle);
  return 0;
}

uint32_t appendDcaExtSSAsset(
  EsmsFileHeaderPtr script,
  uint32_t insert_off,
  const DcaXllFrameSFPosition * param,
  unsigned scriptSourceFileId
)
{
  uint32_t frame_rel_off = 0;
  for (unsigned i = 0; i < param->nbSourceOffsets; i++) {
    DcaXllFrameSFPositionIndex pos = param->sourceOffsets[i];

#if 0
    lbc_printf("taking %zu bytes from offset 0x%zX.\n", len, off);

    if (off == 0xA30C9)
      exit(EXIT_FAILURE);
#endif

    int ret = appendAddPesPayloadCommand(
      script,
      scriptSourceFileId,
      insert_off + frame_rel_off,
      pos.offset,
      pos.size
    );
    if (ret < 0)
      return 0;
    frame_rel_off += pos.size;
  }

  return frame_rel_off;
}