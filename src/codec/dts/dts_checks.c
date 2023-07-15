#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include "dts_checks.h"

/* ### DTS Coherent Acoustics (DCA) Core audio : ########################### */

static int _checkDcaCoreBSHeaderCompliance(
  const DcaCoreBSHeaderParameters * param,
  DtsDcaCoreSSWarningFlags * warn_flags
)
{

  LIBBLU_DTS_DEBUG_CORE(
    "  Sync word (SYNC): 0x%08" PRIX32 ".\n",
    DCA_SYNCWORD_CORE
  );

  LIBBLU_DTS_DEBUG_CORE(
    "  Frame type (FTYPE): %s (0x%d).\n",
    (param->FTYPE) ? "Normal frame" : "Termination frame",
    param->FTYPE
  );

  LIBBLU_DTS_DEBUG_CORE(
    "  Deficit sample count (SHORT): %" PRIu8 " samples (0x%02" PRIX8 ").\n",
    param->SHORT + 1,
    param->SHORT
  );

  if (!param->FTYPE && param->SHORT != 31)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Unexpected number of core samples for a normal frame "
      "(expect %u, receive %u).\n",
      32, param->SHORT
    );

  LIBBLU_DTS_DEBUG_CORE(
    "  Deprecated CRC presence flag (CPF): %s (0b%x).\n",
    BOOL_PRESENCE(param->CPF),
    param->CPF
  );

  if (param->CPF && !warn_flags->presenceOfDeprecatedCrc) {
    LIBBLU_WARNING(
      "Presence of deprecated DCA CRC field.\n"
    );
    warn_flags->presenceOfDeprecatedCrc = true;
  }

  LIBBLU_DTS_DEBUG_CORE(
    "  Number of PCM samples blocks per channel (NBLKS): "
    "%" PRIu8 " blocks (0x% " PRIX8 ").\n",
    param->NBLKS + 1,
    param->NBLKS
  );

  if (param->NBLKS <= 4)
    LIBBLU_DTS_ERROR_RETURN(
      "Invalid NBLKS field range (%" PRIu8 " <= 4).\n",
      param->NBLKS
    );

  if (!param->FTYPE) {
    unsigned nb_blk = param->NBLKS + 1;
    if ((nb_blk & 0x7) || 0 != (nb_blk & (nb_blk - 1)))
      LIBBLU_DTS_ERROR_RETURN(
        "Invalid number of PCM samples blocks (NBLKS) for a normal frame "
        "(shall be 8, 16, 32, 64 or 128 blocks, not %" PRIu8 ").\n",
        nb_blk
      );
  }

  LIBBLU_DTS_DEBUG_CORE(
    "  Core frame size (FSIZE): %u bytes (0x%04" PRIX16 ").\n",
    param->FSIZE + 1,
    param->FSIZE
  );

  if (param->FSIZE <= 94) {
    LIBBLU_DTS_ERROR_RETURN(
      "Invalid Core audio frame size range (%u <= 94).\n",
      param->FSIZE
    );
  }

  LIBBLU_DTS_DEBUG_CORE(
    "  Audio channel arrangement code (AMODE): %s (0x%02" PRIX8 ").\n",
    DcaCoreAudioChannelAssignCodeStr(param->AMODE),
    param->AMODE
  );

  unsigned samp_freq = getDcaCoreAudioSampFreqCode(param->SFREQ);
  LIBBLU_DTS_DEBUG_CORE(
    "  Core audio sampling frequency (SFREQ): %u Hz (0x%02" PRIX8 ").\n",
    samp_freq,
    param->SFREQ
  );

  if (0 == samp_freq)
    LIBBLU_DTS_ERROR_RETURN(
      "Reserved value in use (SFREQ == 0x%02" PRIX8 ").\n",
      param->SFREQ
    );

  if (48000 != samp_freq)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "BDAV specifications allows only 48 kHz as "
      "DCA Core audio sampling frequency (found %u Hz).\n",
      samp_freq
    );

  unsigned bitrate = getDcaCoreTransBitRate(param->RATE);
  if (DCA_RATE_OPEN == param->RATE)
    LIBBLU_DTS_DEBUG_CORE(
      "  Target transmission bitrate code (RATE): Open (0x%02" PRIX8 ").\n",
      param->RATE
    );
  else
    LIBBLU_DTS_DEBUG_CORE(
      "  Target transmission bitrate code (RATE): "
      "%u Kbits/s (0x%02" PRIX8 ").\n",
      bitrate,
      param->RATE
    );

  if (0 == bitrate)
    LIBBLU_DTS_ERROR_RETURN(
      "Reserved value in use (RATE == 0x%02" PRIX8 ").\n",
      param->RATE
    );
  if (DCA_RATE_OPEN == param->RATE)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "BDAV specifications disallows use of open transmission rate mode of "
      "DCA Core.\n"
    );

#if 0
  if (
    param->FSIZE * param->NBLKS * param->bitrate
    < ((unsigned) param->FSIZE) * param->bitDepth * 8
  )
    LIBBLU_DTS_ERROR_RETURN(
      "Too big Core audio frame size at stream bit-rate "
      "(%" PRId64 " bytes).\n",
      param->FSIZE
    );
#endif

  LIBBLU_DTS_DEBUG_CORE(
    "  Embedded dynamic range coefficients presence flag "
    "(DYNF): %s (0b%x).\n",
    BOOL_PRESENCE(param->DYNF),
    param->DYNF
  );

  LIBBLU_DTS_DEBUG_CORE(
    "  Time stamps fields presence flag (TIMEF): %s (0b%x).\n",
    BOOL_PRESENCE(param->TIMEF),
    param->TIMEF
  );

  LIBBLU_DTS_DEBUG_CORE(
    "  Core auxilary data presence flag (AUXF): %s (0b%x).\n",
    BOOL_PRESENCE(param->AUXF),
    param->AUXF
  );

  LIBBLU_DTS_DEBUG_CORE(
    "  HDCD mastered audio data flag (HDCD): %s (0b%x).\n",
    BOOL_INFO(param->HDCD),
    param->HDCD
  );

  if (param->HDCD && !warn_flags->usageOfHdcdEncoding) {
    LIBBLU_WARNING(
      "Signaled usage of HDCD mastering mode, "
      "normaly reserved for DTS-CDs.\n"
    );
    warn_flags->usageOfHdcdEncoding = true;
  }

  if (param->EXT_AUDIO) {
    LIBBLU_DTS_DEBUG_CORE(
      "  Core audio extension data type (EXT_AUDIO_ID): "
      "%s (0x%02" PRIX8 ").\n",
      DcaCoreExtendedAudioCodingCodeStr(
        param->EXT_AUDIO_ID
      ),
      param->EXT_AUDIO_ID
    );

    if (!isValidDcaCoreExtendedAudioCodingCode(param->EXT_AUDIO_ID))
      LIBBLU_DTS_ERROR_RETURN(
        "Reserved value in use (EXT_AUDIO_ID == 0x%02" PRIX8 ").\n",
        param->EXT_AUDIO_ID
      );
  }

  LIBBLU_DTS_DEBUG_CORE(
    "  Core audio extension presence (EXT_AUDIO): %s (0b%x).\n",
    BOOL_PRESENCE(param->EXT_AUDIO),
    param->EXT_AUDIO
  );

  LIBBLU_DTS_DEBUG_CORE(
    "  Audio data synchronization word insertion level (ASPF): %s (0b%x).\n",
    (param->ASPF) ?
      "At Subsubframe level"
    :
      "At Subframe level",
    param->ASPF
  );

  LIBBLU_DTS_DEBUG_CORE(
    "  Low Frequency Effects (LFE) channel presence code (LFF): "
    "%s (0x%" PRIX8 ").\n",
    DcaCoreLowFrequencyEffectsFlagStr(param->LFF),
    param->LFF
  );

  if (DCA_LFF_INVALID == param->LFF)
    LIBBLU_DTS_ERROR_RETURN("Invalid value in use (LFE == 0x03).\n");

  LIBBLU_DTS_DEBUG_CORE(
    "  ADPCM predicator history of last frame usage (HFLAG): %s (0b%x).\n",
    BOOL_INFO(param->HFLAG),
    param->HFLAG
  );

  LIBBLU_DTS_DEBUG_CORE(
    "  Filter bank interpolation FIR coefficients type (FILTS): %s (0x%x).\n",
    (param->FILTS) ? "Perfect" : "Non-perfect",
    param->FILTS
  );

  LIBBLU_DTS_DEBUG_CORE(
    "  Encoder sofware syntax revision (VERNUM): %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->VERNUM,
    param->VERNUM
  );

  if (param->VERNUM == DCA_MAX_SYNTAX_VERNUM)
    LIBBLU_DTS_DEBUG_CORE("   => Current revision.\n");
  else if (param->VERNUM < DCA_MAX_SYNTAX_VERNUM)
    LIBBLU_DTS_DEBUG_CORE("   => Future compatible revision.\n");
  else
    LIBBLU_DTS_DEBUG_CORE("   => Future incompatible revision.\n");

  LIBBLU_DTS_DEBUG_CORE(
    "  Copyright history information code (CHIST): %s (0x%02" PRIX8 ").\n",
    DcaCoreCopyrightHistoryCodeStr(param->CHIST),
    param->CHIST
  );

  LIBBLU_DTS_DEBUG_CORE(
    "  Source PCM resolution code (PCMR): %u bits%s (0x%02" PRIX8 ").\n",
    getDcaCoreSourcePcmResCode(param->PCMR),
    isEsDcaCoreSourcePcmResCode(param->PCMR) ? " DTS ES encoded" : "",
    param->PCMR
  );

  LIBBLU_DTS_DEBUG_CORE(
    "  Front speakers encoding mode flag (SUMF): %s (0b%x).\n",
    frontChDcaCoreChPairSumDiffEncodedFlagStr(param->SUMF),
    param->SUMF
  );

  LIBBLU_DTS_DEBUG_CORE(
    "  Surround speakers encoding mode flag (SUMS): %s (0b%x).\n",
    surChDcaCoreChPairSumDiffEncodedFlagStr(param->SUMF),
    param->SUMS
  );

  if (0x6 == param->VERNUM || 0x7 == param->VERNUM) {
    LIBBLU_DTS_DEBUG_CORE(
      "  Dialog normalization (DIALNORM): "
      "%d dB (DNG = 0x%02" PRIX8 ").\n",
      getDcaCoreDialogNormalizationValue(param->DIALNORM, param->VERNUM),
      param->DIALNORM
    );
  }
  else {
    LIBBLU_DTS_DEBUG_CORE(
      "  Unspecified place-holder (UNSPEC): 0x%" PRIX8 ".\n",
      param->DIALNORM
    );
  }

  return 0;
}

static bool _constantDcaCoreSSFrameHeader(
  const DcaCoreBSHeaderParameters * first,
  const DcaCoreBSHeaderParameters * second
)
{
  return CHECK(
    EQUAL(->FTYPE)
    EQUAL(->SHORT)
    EQUAL(->NBLKS)
    EQUAL(->CPF)
    EQUAL(->AMODE)
    EQUAL(->SFREQ)
    EQUAL(->PCMR)
    EQUAL(->RATE)
    EQUAL(->AUXF)
    EQUAL(->HDCD)
    EQUAL(->EXT_AUDIO)
    EQUAL(->LFF)
    EQUAL(->EXT_AUDIO_ID)
    EQUAL(->VERNUM)
  );
}

static int _checkDcaCoreBSHeaderChangeCompliance(
  const DcaCoreBSHeaderParameters * old,
  const DcaCoreBSHeaderParameters * cur
)
{

  /* Audio channel arragement */
  if (old->AMODE != cur->AMODE)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Core audio channel arrangement shall remain "
      "the same during whole bitstream."
    );

  /* Sampling frequency */
  if (old->SFREQ != cur->SFREQ)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Core audio sampling frequency shall remain "
      "the same during whole bitstream."
    );

  /* Bit depth */
  if (old->PCMR != cur->PCMR)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Core audio bit depth shall remain "
      "the same during whole bitstream."
    );

  /* Bit-rate */
  if (old->RATE != cur->RATE)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Core audio bit-rate shall remain "
      "the same during whole bitstream."
    );

  /* Low Frequency Effects flag */
  if (old->LFF != cur->LFF)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Core audio LFE configuration shall remain "
      "the same during whole bitstream."
    );

  /* Extension */
  if (old->EXT_AUDIO != cur->EXT_AUDIO || old->EXT_AUDIO_ID != cur->EXT_AUDIO_ID)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Core audio extension coding parameters shall remain "
      "the same during whole bitstream."
    );

  /* Syntax version */
  if (old->VERNUM != cur->VERNUM)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Core audio syntax version shall remain "
      "the same during whole bitstream."
    );

  return 0;
}

int checkDcaCoreSSCompliance(
  const DcaCoreSSFrameParameters * frame,
  DtsDcaCoreSSWarningFlags * warn_flags
)
{
  LIBBLU_DTS_DEBUG_CORE(" Bit stream header:\n");
  if (_checkDcaCoreBSHeaderCompliance(&frame->bs_header, warn_flags) < 0)
    return -1;

  // LIBBLU_DTS_DEBUG_CORE(" Frame payload:\n");
  // LIBBLU_DTS_DEBUG_CORE("  Size: %" PRId64 " bytes.\n", frame->payloadSize);
  return 0;
}

bool constantDcaCoreSS(
  const DcaCoreSSFrameParameters * first,
  const DcaCoreSSFrameParameters * second
)
{
  return CHECK(
    SUB_FUN_PTR(->bs_header, _constantDcaCoreSSFrameHeader)
  );
}

int checkDcaCoreSSChangeCompliance(
  const DcaCoreSSFrameParameters * old,
  const DcaCoreSSFrameParameters * cur,
  DtsDcaCoreSSWarningFlags * warn_flags
)
{
  if (checkDcaCoreSSCompliance(cur, warn_flags) < 0)
    return -1;

  return _checkDcaCoreBSHeaderChangeCompliance(
    &old->bs_header,
    &cur->bs_header
  );
}



static int _checkDcaExtSSHeaderMixMetadataCompliance(
  const DcaExtSSHeaderMixMetadataParameters * param
)
{

  LIBBLU_DTS_DEBUG_EXTSS(
    "    Mixing Metadata Adujstement Level (nuMixMetadataAdjLevel): "
    "%s (0x%" PRIX8 ").\n",
    dcaExtMixMetadataAjdLevelStr(param->nuMixMetadataAdjLevel),
    param->nuMixMetadataAdjLevel
  );

  if (DCA_EXT_SS_MIX_ADJ_LVL_RESERVED == param->nuMixMetadataAdjLevel)
    LIBBLU_DTS_ERROR_RETURN(
      "Reserved value in use (nuMixMetadataAdjLevel == 0x%" PRIX8 ").\n",
      param->nuMixMetadataAdjLevel
    );

  LIBBLU_DTS_DEBUG_EXTSS(
    "    Number of Mixing Configurations (nuNumMixOutConfigs): %u (0x%X).\n",
    param->nuNumMixOutConfigs,
    param->nuNumMixOutConfigs - 1
  );

  LIBBLU_DTS_DEBUG_EXTSS(
    "    Mixing Configurations speaker layout for Mixer Output Channels "
    "(nuMixOutChMask[ns]):\n"
  );
  for (unsigned ns = 0; ns < param->nuNumMixOutConfigs; ns++) {
    char ch_mask_str[DCA_CHMASK_STR_BUFSIZE];
    buildDcaExtChMaskString(ch_mask_str, param->nuMixOutChMask[ns]);

    LIBBLU_DTS_DEBUG_EXTSS(
      "     -> Configuration %u: %s (%u channels, 0x%04" PRIX16 ");\n",
      ns, ch_mask_str,
      param->nNumMixOutCh[ns],
      param->nuMixOutChMask[ns]
    );
  }

  return 0;
}

static int _checkDcaExtSSHeaderStaticFieldsCompliance(
  const DcaExtSSHeaderStaticFieldsParameters * param,
  bool is_secondary,
  unsigned nExtSSIndex,
  DtsDcaExtSSWarningFlags * warn_flags
)
{

  LIBBLU_DTS_DEBUG_EXTSS(
    "   Reference clock code (nuRefClockCode): "
    "0x%02" PRIX8 ".\n",
    param->referenceClockCode
  );
  LIBBLU_DTS_DEBUG_EXTSS(
    "    => %u Hz.\n",
    param->referenceClockFreq
  );

  if (!param->referenceClockFreq)
    LIBBLU_DTS_ERROR_RETURN(
      "Reserved value in use (nuRefClockCode == 0x3).\n"
    );

  if (param->referenceClockCode != DCA_EXT_SS_REF_CLOCK_PERIOD_48000)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Invalid reference clock frequency of %u Hz. "
      "BDAV specifications only allows multiples of 48 kHz clock.\n",
      param->referenceClockFreq
    );

  LIBBLU_DTS_DEBUG_EXTSS(
    "   Frame duration code (nuExSSFrameDurationCode): 0x%02" PRIX8 ".\n",
    param->frameDurationCode
  );
  LIBBLU_DTS_DEBUG_EXTSS(
    "    => Frame duration value (nuExSSFrameDuration): %u clock samples.\n",
    param->frameDurationCodeValue
  );
  LIBBLU_DTS_DEBUG_EXTSS(
    "    => Frame duration in seconds (ExSSFrameDuration): %f s.\n",
    param->frameDuration
  );

  if (!is_secondary) {
    if (param->frameDurationCodeValue != 512)
      LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
        "Primary audio Extension Substream frames shall carry "
        "512 clock samples (found %u).\n",
        param->frameDurationCodeValue
      );
  }
  else {
    if (param->frameDurationCodeValue != 4096)
      LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
        "Secondary audio Extension Substream frames shall carry "
        "4096 clock samples (found %u).\n",
        param->frameDurationCodeValue
      );
  }

  LIBBLU_DTS_DEBUG_EXTSS(
    "   Timecode presence (bTimeStampFlag): %s (0b%x).\n",
    BOOL_PRESENCE(param->timeStampPresent),
    param->timeStampPresent
  );

  if (param->timeStampPresent) {
    LIBBLU_DTS_DEBUG_EXTSS(
      "    Timecode data (nuTimeStamp / nLSB): "
      "0x%08" PRIX32 " 0x%01" PRIX8 ".\n",
      param->timeStampValue,
      param->timeStampLsb
    );
    LIBBLU_DTS_DEBUG_EXTSS(
      "     => %02u:%02u:%02u.%05u.\n",
      (param->timeStamp / param->referenceClockFreq) / 3600,
      (param->timeStamp / param->referenceClockFreq) / 60 % 60,
      (param->timeStamp / param->referenceClockFreq) % 60,
      (param->timeStamp % param->referenceClockFreq)
    );
  }

  LIBBLU_DTS_DEBUG_EXTSS(
    "   Number of Audio Presentations (nuNumAudioPresnt): %u (0x%X).\n",
    param->nbAudioPresentations,
    param->nbAudioPresentations - 1
  );

  if (1 < param->nbAudioPresentations)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Unsupported multiple Audio Presentations in Extension Substream.\n"
    );

  LIBBLU_DTS_DEBUG_EXTSS(
    "   Number of Audio Assets (nuNumAssets): %u (0x%X).\n",
    param->nbAudioAssets,
    param->nbAudioAssets - 1
  );

  if (1 < param->nbAudioAssets)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Unsupported multiple Audio Assets in Extension Substream.\n"
    );

  LIBBLU_DTS_DEBUG_EXTSS(
    "   Active Extension(s) Substream(s) for Audio Presentation(s) "
    "(nuActiveExSSMask[nAuPr] / nuActiveAssetMask[nAuPr][nSS]):\n"
  );

  for (unsigned nAuPr = 0; nAuPr < param->nbAudioPresentations; nAuPr++) {
    LIBBLU_DTS_DEBUG_EXTSS("    -> Audio Presentation %u: ", nAuPr);

    if (!param->activeExtSSMask[nAuPr])
      LIBBLU_DTS_DEBUG_EXTSS_NH("No Extension Substream (0x0);\n");

    else {
      const char * sep = "";

      for (unsigned nSS = 0; nSS < nExtSSIndex + 1; nSS++) {
        if ((param->activeExtSSMask[nAuPr] >> nSS) & 0x1) {
          LIBBLU_DTS_DEBUG_EXTSS_NH("%sSubstream %u", sep, nSS);
          sep = ", ";
        }
      }
      LIBBLU_DTS_DEBUG_EXTSS_NH(
        " (0x%02" PRIX8 ");\n", param->activeExtSSMask[nAuPr]
      );
    }
  }

  if (!is_secondary) {
    if (param->activeExtSSMask[0] != 0x1)
      LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
        "Unexpected Active Audio Asset Mask for primary audio "
        "(0x%02" PRIX8 ", expect 0x1).\n",
        param->activeExtSSMask[0]
      );
  }
  else {
    if (param->activeExtSSMask[0] != 0x5)
      LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
        "Unexpected Active Audio Asset Mask for primary audio "
        "(0x%02" PRIX8 ", expect 0x5).\n",
        param->activeExtSSMask[0]
      );
  }

  LIBBLU_DTS_DEBUG_EXTSS(
    "   Mixing Metadata presence (bMixMetadataEnbl): %s (0b%x).\n",
    BOOL_PRESENCE(param->mixMetadataEnabled),
    param->mixMetadataEnabled
  );

  if (param->mixMetadataEnabled) {
    if (!warn_flags->nonCompleteAudioPresentationAssetType) {
      LIBBLU_INFO(
        LIBBLU_DTS_PREFIX
        "Presence of Mixing Metadata in Extension Substream.\n"
      );
      warn_flags->nonCompleteAudioPresentationAssetType = true;
    }

    LIBBLU_DTS_DEBUG_EXTSS("   Mixing Metadata:\n");

    if (_checkDcaExtSSHeaderMixMetadataCompliance(&param->mixMetadata) < 0)
      return -1;
  }

  return 0;
}

static int _checkDcaAudioAssetDescriptorStaticFieldsCompliance(
  const DcaAudioAssetDescSFParameters * param,
  bool is_secondary,
  DtsDcaExtSSWarningFlags * warn_flags
)
{

  LIBBLU_DTS_DEBUG_EXTSS(
    "     Asset type descriptor presence (bAssetTypeDescrPresent): "
    "%s (0b%x).\n",
    BOOL_PRESENCE(param->assetTypeDescriptorPresent),
    param->assetTypeDescriptorPresent
  );

  if (param->assetTypeDescriptorPresent) {
    LIBBLU_DTS_DEBUG_EXTSS(
      "     Asset type descriptor (nuAssetTypeDescriptor): 0x%02" PRIX8 ".\n",
      param->assetTypeDescriptor
    );

    LIBBLU_DTS_DEBUG_EXTSS(
      "      => %s.\n",
      dtsExtAssetTypeDescCodeStr(param->assetTypeDescriptor)
    );

    if (param->assetTypeDescriptor == DCA_EXT_SS_ASSET_TYPE_DESC_RESERVED)
      LIBBLU_DTS_ERROR_RETURN(
        "Reserved value in use (nuAssetTypeDescriptor == 15)"
      );
  }

  LIBBLU_DTS_DEBUG_EXTSS(
    "     Language Descriptor Presence (bLanguageDescrPresent): "
    "%s (0b%x).\n",
    BOOL_PRESENCE(param->languageDescriptorPresent),
    param->languageDescriptorPresent
  );

  if (param->languageDescriptorPresent) {
    LIBBLU_DTS_DEBUG_EXTSS(
      "     Language Descriptor (LanguageDescriptor): "
      "0x%02" PRIX8 "%02" PRIX8 "%02" PRIX8 ".\n",
      param->languageDescriptor[0],
      param->languageDescriptor[1],
      param->languageDescriptor[2]
    );

    LIBBLU_DTS_DEBUG_EXTSS(
      "      => ISO Latin-1: '%s'.\n",
      (char *) param->languageDescriptor
    );
  }

  LIBBLU_DTS_DEBUG_EXTSS(
    "     Additional Textual Information Presence (bInfoTextPresent): "
    "%s (0x%x).\n",
    BOOL_PRESENCE(param->infoTextPresent),
    param->infoTextPresent
  );

  if (param->infoTextPresent) {
    LIBBLU_DTS_DEBUG_EXTSS(
      "     Additional Textual Information size (nuInfoTextByteSize): "
      "%" PRId64 " bytes "
      "(0x%" PRIX64 ").\n",
      param->infoTextLength,
      param->infoTextLength - 1
    );

    LIBBLU_DTS_DEBUG_EXTSS(
      "     Additional Textual Information (InfoTextString):\n"
    );
    LIBBLU_DTS_DEBUG_EXTSS(
      "      => ISO Latin-1: '%s'.\n",
      (char *) param->infoText
    );
  }

  LIBBLU_DTS_DEBUG_EXTSS(
    "     PCM audio Bit Depth (nuBitResolution): %u bits (0x%X).\n",
    param->bitDepth,
    param->bitDepth - 1
  );

  if (param->bitDepth != 16 && param->bitDepth != 24)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Unexpected Extension Substream PCM Bit Depth of %u bits "
      "(BDAV specifications only allows 16 or 24 bits).\n",
      param->bitDepth
    );

  LIBBLU_DTS_DEBUG_EXTSS(
    "     Maximal audio Sampling Rate code "
    "(nuMaxSampleRate): 0x%02" PRIX8 ".\n",
    param->maxSampleRateCode
  );

  LIBBLU_DTS_DEBUG_EXTSS(
    "      => %u Hz.\n",
    param->maxSampleRate
  );

  if (
    is_secondary
    && param->maxSampleRate != 48000
  )
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Unexpected Extension Substream audio Sampling Rate of %u Hz "
      "for a secondary stream (BDAV specifications only allows 48 kHz).\n",
      param->maxSampleRate
    );

  if (
    param->maxSampleRate != 48000
    && param->maxSampleRate != 96000
    && param->maxSampleRate != 192000
  )
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Unexpected Extension Substream audio Sampling Rate of %u Hz "
      "(BDAV specifications only allows 48, 96 or 192 kHz).\n",
      param->maxSampleRate
    );

  LIBBLU_DTS_DEBUG_EXTSS(
    "     Total Number of encoded audio Channels (nuTotalNumChs): "
    "%u (0x%X).\n",
    param->nbChannels,
    param->nbChannels - 1
  );

  if (is_secondary && 6 < param->nbChannels)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Unexpected Extension Substream number of channels %u "
      "for a secondary audio stream "
      "(BDAV specifications only allows up to 6 channels).\n",
      param->nbChannels
    );

  if (param->maxSampleRate == 192000 && 6 < param->nbChannels)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Unexpected Extension Substream number of channels %u "
      "for a 192 kHz sampling rate "
      "(BDAV specifications only allows up to 6 channels at 192 kHz).\n",
      param->nbChannels
    );

  if (8 < param->nbChannels)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Unexpected Extension Substream number of channels %u "
      "(BDAV specifications only allows up to 8 channels).\n",
      param->nbChannels
    );

  LIBBLU_DTS_DEBUG_EXTSS(
    "     Audio Channels maps Direct Speakers feeds "
    "(bOne2OneMapChannels2Speakers): %s (0b%x).\n",
    BOOL_INFO(param->directSpeakersFeed),
    param->directSpeakersFeed
  );

  if (!param->directSpeakersFeed) {
    if (
      is_secondary
      && !DCA_EXT_SS_STRICT_NOT_DIRECT_SPEAKERS_FEED_REJECT
    ) {
      if (!warn_flags->nonDirectSpeakersFeedTolerance) {
        LIBBLU_WARNING(
          "Usage of a non recommended alternate channels declaration syntax "
          "in Extension Substream Asset Descriptor "
          "tolerated for Secondary audio (No direct-speakers-feeds).\n"
        );
        warn_flags->nonDirectSpeakersFeedTolerance = true;
      }
    }
    else if (
      param->nbChannels == 2
      && !DCA_EXT_SS_STRICT_NOT_DIRECT_SPEAKERS_FEED_REJECT
    ) {
      if (!warn_flags->nonDirectSpeakersFeedTolerance) {
        LIBBLU_WARNING(
          "Usage of a non recommended alternate channels declaration syntax "
          "in Extension Substream Asset Descriptor "
          "tolerated for Stereo audio (No direct-speakers-feeds).\n"
        );
        warn_flags->nonDirectSpeakersFeedTolerance = true;
      }
    }
    else
      LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
        "Absence of audio speakers channels mapping in Extension Substream. "
        "Audio channels shall be mapped to a specific speaker "
        "(bOne2OneMapChannels2Speakers == 0b0).\n"
      );
  }

  if (param->directSpeakersFeed) {
    LIBBLU_DTS_DEBUG_EXTSS("     Channels direct speakers feed related:\n");

    LIBBLU_DTS_DEBUG_EXTSS("      Embedded downmixes:\n");
    if (2 < param->nbChannels) {
      LIBBLU_DTS_DEBUG_EXTSS(
        "       - Stereo downmix (bEmbeddedStereoFlag): %s (0b%x).\n",
        BOOL_PRESENCE(param->embeddedStereoDownmix),
        param->embeddedStereoDownmix
      );

      if (
        param->embeddedStereoDownmix
        && !warn_flags->presenceOfStereoDownmix
      ) {
        LIBBLU_INFO(
          "Presence of an optional embedded Stereo Down-mix "
          "in Extension Substream.\n"
        );
        warn_flags->presenceOfStereoDownmix = true;
      }
    }
    else
      LIBBLU_DTS_DEBUG_EXTSS("       - Stereo downmix: Not applicable.\n");

    if (
      !param->embeddedStereoDownmix
      && is_secondary
      && 2 < param->nbChannels
      && !warn_flags->absenceOfStereoDownmixForSec
    ) {
      LIBBLU_WARNING(
        "Missing recommended Stereo Down-mix for a Secondary Stream audio "
        "track with more than two channels.\n"
      );
      warn_flags->absenceOfStereoDownmixForSec = true;
    }

    if (6 < param->nbChannels) {
      LIBBLU_DTS_DEBUG_EXTSS(
        "       - Surround 6 ch. downmix (bEmbeddedSixChFlag): %s (0b%x).\n",
        BOOL_PRESENCE(param->embeddedSurround6chDownmix),
        param->embeddedSurround6chDownmix
      );

      if (!param->embeddedSurround6chDownmix)
        LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
          "Missing mandatory Surround 6 Ch. down-mix required by BDAV "
          "specifications for Extension Substreams with more than "
          "6 channels.\n"
        );
    }
    else
      LIBBLU_DTS_DEBUG_EXTSS(
        "       - Surround 6 ch. downmix: Not applicable.\n"
      );

    LIBBLU_DTS_DEBUG_EXTSS(
      "      Channels loudspeakers activity layout mask presence "
      "(bSpkrMaskEnabled): %s (0b%x).\n",
      BOOL_PRESENCE(param->bSpkrMaskEnabled),
      param->bSpkrMaskEnabled
    );

    if (!param->bSpkrMaskEnabled)
      LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
        "Missing presence of loudspeakers activity layout mask in Extension "
        "Substream Asset Descriptor.\n"
      );

    if (param->bSpkrMaskEnabled) {
      char spkr_activity_mask_str[DCA_CHMASK_STR_BUFSIZE];
      unsigned nb_ch_spkr_activity_mask = buildDcaExtChMaskString(
        spkr_activity_mask_str,
        param->nuSpkrActivityMask
      );

      LIBBLU_DTS_DEBUG_EXTSS(
        "      Channels loudspeakers activity layout mask "
        "(nuSpkrActivityMask): %s (%u channel(s), 0x%" PRIX16 ").\n",
        spkr_activity_mask_str,
        nb_ch_spkr_activity_mask,
        param->nuSpkrActivityMask
      );

      if (nb_ch_spkr_activity_mask != param->nbChannels)
        LIBBLU_DTS_ERROR_RETURN(
          "Unexpected number of channels in loudspeakers activity layout mask "
          "of Extension Substream Asset Descriptor "
          "(expect %u channels, got %u).\n",
          param->nbChannels,
          nb_ch_spkr_activity_mask
        );
    }

    LIBBLU_DTS_DEBUG_EXTSS(
      "      Number of Speakers Remapping Sets (nuNumSpkrRemapSets): "
      "%u (0x%X).\n",
      param->nuNumSpkrRemapSets,
      param->nuNumSpkrRemapSets
    );

    if (0 < param->nuNumSpkrRemapSets) {
      LIBBLU_DTS_DEBUG_EXTSS("      Remapping Sets:\n");

      for (unsigned ns = 0; ns < param->nuNumSpkrRemapSets; ns++) {
        LIBBLU_DTS_DEBUG_EXTSS("       - Set %u:\n", ns);

        char stndr_spkr_layout_mask_str[DCA_CHMASK_STR_BUFSIZE];
        unsigned nb_ch_stndr_spkr_layout_mask = buildDcaExtChMaskString(
          stndr_spkr_layout_mask_str,
          param->nuSpkrActivityMask
        );

        LIBBLU_DTS_DEBUG_EXTSS(
          "        Standard replaced Loudspeaker Layout Mask "
          "(nuStndrSpkrLayoutMask): %s (%u channels, 0x%04" PRIX16 ").\n",
          stndr_spkr_layout_mask_str,
          nb_ch_stndr_spkr_layout_mask,
          param->nuStndrSpkrLayoutMask[ns]
        );

        LIBBLU_DTS_DEBUG_EXTSS(
          "        Number of Channels to be Decoded for Speakers Remapping "
          "(nuNumDecCh4Remap): %u channel(s).\n",
          param->nbChRequiredByRemapSet[ns]
        );

        if (param->nbChsInRemapSet[ns] < param->nbChRequiredByRemapSet[ns])
          LIBBLU_DTS_ERROR_RETURN(
            "Unexpected number of channels in Standard Loudspeakers Remapping "
            "Set to decode (expect at least %u channels, got).\n",
            param->nbChsInRemapSet[ns],
            param->nbChRequiredByRemapSet[ns]
          );

        LIBBLU_DTS_DEBUG_EXTSS(
          "        Channels remapping masks and associated coefficients "
          "(nuRemapDecChMask / nuSpkrRemapCodes):\n"
        );
        for (unsigned nCh = 0; nCh < param->nbChsInRemapSet[ns]; nCh++) {
          if (!param->nbRemapCoeffCodes[ns][nCh])
            LIBBLU_DTS_DEBUG_EXTSS_NH("         -> Muted (0x0).\n");
          else {
            char stndr_spkr_layout_mask_str[DCA_CHMASK_STR_BUFSIZE];
            unsigned nb_ch_stndr_spkr_layout_mask = buildDcaExtChMaskString(
              stndr_spkr_layout_mask_str,
              param->nuRemapDecChMask[ns][nCh]
            );

            LIBBLU_DTS_DEBUG_EXTSS_NH(
              "         -> %s (%u channels, 0x%02" PRIX16 "): ",
              stndr_spkr_layout_mask_str,
              nb_ch_stndr_spkr_layout_mask,
              param->nuRemapDecChMask[ns][nCh]
            );

            const char * sep = "";
            for (unsigned nc = 0; nc < param->nbRemapCoeffCodes[ns][nCh]; nc++) {
              LIBBLU_DTS_DEBUG_EXTSS_NH(
                "%s%" PRIu8, sep,
                param->outputSpkrRemapCoeffCodes[ns][nCh][nc]
              );
              sep = ", ";
            }

            LIBBLU_DTS_DEBUG_EXTSS_NH(".\n");
          }
        }
      }
    }
  }
  else {
    LIBBLU_DTS_DEBUG_EXTSS("     Unmapped channels related:\n");

    LIBBLU_DTS_DEBUG_EXTSS("      Embedded downmixes:\n");
    LIBBLU_DTS_DEBUG_EXTSS("       - Stereo downmix: Not applicable.\n");
    LIBBLU_DTS_DEBUG_EXTSS(
      "       - Surround 6 ch. downmix: Not applicable.\n"
    );

    LIBBLU_DTS_DEBUG_EXTSS("      Representation type code: 0x%02" PRIX8 ".\n");
    LIBBLU_DTS_DEBUG_EXTSS(
      "       -> %s.\n",
      dtsExtRepresentationTypeCodeStr(param->representationType)
    );

    if (param->representationType == DCA_EXT_SS_LT_RT_MATRIX_SURROUND)
      LIBBLU_DTS_DEBUG_EXTSS("      Implied channels mask: Lt, Rt.\n");
    if (param->representationType == DCA_EXT_SS_LH_RH_HEADPHONE)
      LIBBLU_DTS_DEBUG_EXTSS("      Implied channels mask: Lh, Rh.\n");

    if (is_secondary) {
      if (param->representationType != DCA_EXT_SS_MIX_REPLACEMENT)
        LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
          "Unexpected Representation Type in Extension Substream Asset "
          "Descriptor for a Secondary Stream, expect "
          "Mixing/Replacement type, received '%s'.\n",
          dtsExtRepresentationTypeCodeStr(param->representationType)
        );
    }
    else {
      if (
        param->representationType != DCA_EXT_SS_LT_RT_MATRIX_SURROUND
        || param->representationType != DCA_EXT_SS_LH_RH_HEADPHONE
      )
        LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
          "Unexpected Representation Type in Extension Substream Asset "
          "Descriptor, expect a valid stereo type, received '%s'.\n",
          dtsExtRepresentationTypeCodeStr(param->representationType)
        );
    }
  }

  return 0;
}

static int _checkDcaAudioAssetDescriptorDynamicMetadataCompliance(
  const DcaAudioAssetDescDMParameters * param,
  const DcaAudioAssetDescSFParameters * sf
)
{

  LIBBLU_DTS_DEBUG_EXTSS(
    "     Dynamic Range Control Presence (bDRCCoefPresent): %s (0b%x).\n",
    BOOL_PRESENCE(param->drcEnabled),
    param->drcEnabled
  );

  if (param->drcEnabled) {
    LIBBLU_DTS_DEBUG_EXTSS(
      "     Dynamic Range Control Coefficient Code "
      "(nuDRCCode): %" PRIu8 ".\n",
      param->drcParameters.drcCode
    );
    LIBBLU_DTS_DEBUG_EXTSS(
      "      -> Gain value (DRC_dB): %.2f dB.\n",
      -32.0 + ((float) param->drcParameters.drcCode) * 0.25
    );
    if (sf->embeddedStereoDownmix) {
      LIBBLU_DTS_DEBUG_EXTSS(
        "     Dynamic Range Control Coefficient Code for Stereo Downmix "
        "(nuDRC2ChDmixCode): %" PRIu8 ".\n",
        param->drcParameters.drc2ChCode
      );
      LIBBLU_DTS_DEBUG_EXTSS(
        "      -> Gain value (DRC_dB): %.2f dB.\n",
        -32.0 + ((float) param->drcParameters.drc2ChCode) * 0.25
      );
    }
  }

  LIBBLU_DTS_DEBUG_EXTSS(
    "     Dialogue Normalization Presence (bDialNormPresent): %s (0b%x).\n",
    BOOL_PRESENCE(param->dialNormEnabled),
    param->dialNormEnabled
  );

  if (param->drcEnabled) {
    LIBBLU_DTS_DEBUG_EXTSS(
      "     Dialogue Normalization Code (nuDRCCode): %" PRIu8 ".\n",
      param->dialNormCode
    );
    LIBBLU_DTS_DEBUG_EXTSS(
      "      -> Dialog Normlization Gain value (DNG): %d dB.\n",
      -((int) param->dialNormCode)
    );
  }

  LIBBLU_DTS_DEBUG_EXTSS(
    "     Mixing Metadata Presence (bMixMetadataPresent): %s (0b%x).\n",
    BOOL_PRESENCE(param->mixMetadataPresent),
    param->mixMetadataPresent
  );

  if (param->mixMetadataPresent) {
    LIBBLU_DTS_DEBUG_EXTSS(
      "     Mixing metadata:\n"
    );
    LIBBLU_DTS_DEBUG_EXTSS(
      "      (TODO) Not implemented yet.\n"
    );
  }

  return 0;
}

static int _checkDcaAudioAssetDescriptorDecoderNavDataCompliance(
  const DcaAudioAssetDescDecNDParameters * param,
  const DcaExtSSHeaderMixMetadataParameters mix_meta,
  bool is_sec_stream
)
{

  LIBBLU_DTS_DEBUG_EXTSS(
    "     Data Coding Mode (nuCodingMode): 0x%x.\n",
    param->nuCodingMode
  );
  LIBBLU_DTS_DEBUG_EXTSS(
    "      => %s.\n", dtsAudioAssetCodingModeStr(param->nuCodingMode)
  );

  if (is_sec_stream) {
    if (param->nuCodingMode != DCA_EXT_SS_CODING_MODE_DTS_HD_LOW_BITRATE)
      LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
        "Unexpected Secondary audio track coding mode. BDAV specifications "
        "require DTS-HD Low bit-rate coding mode for Extension Substream.\n"
      );
  }
  else {
    if (param->nuCodingMode != DCA_EXT_SS_CODING_MODE_DTS_HD_COMPONENTS)
      LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
        "Unexpected Primary audio track coding mode. BDAV specifications "
        "require DTS-HD Components coding mode with retro-compatible "
        "Core Substream.\n"
      );
  }

  LIBBLU_DTS_DEBUG_EXTSS("     Coding Mode related:\n");

  switch (param->nuCodingMode) {
    case DCA_EXT_SS_CODING_MODE_DTS_HD_COMPONENTS:
    case DCA_EXT_SS_CODING_MODE_DTS_HD_LOSSLESS_WITHOUT_CORE:
    case DCA_EXT_SS_CODING_MODE_DTS_HD_LOW_BITRATE:
      if (param->nuCodingMode == DCA_EXT_SS_CODING_MODE_DTS_HD_COMPONENTS) {
        LIBBLU_DTS_DEBUG_EXTSS(
          "      Coding Components Used in Asset mask "
          "(nuCoreExtensionMask): 0x%04" PRIX16 ".\n",
          param->nuCoreExtensionMask
        );
      }
      else {
        LIBBLU_DTS_DEBUG_EXTSS(
          "      Implied Coding Components Used in Asset mask: "
          "0x%04" PRIX16 ".\n",
          param->nuCoreExtensionMask
        );
      }

      if (!param->nuCoreExtensionMask)
        LIBBLU_DTS_DEBUG_EXTSS("       => *Empty*\n");

      if (
        param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_CORE_DCA
      ) {
        LIBBLU_DTS_DEBUG_EXTSS(
          "       => DCA Core Component within Core Substream "
          "(0x%04" PRIX16 ").\n",
          DCA_EXT_SS_COD_COMP_CORE_DCA
        );
      }
      if (
        param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_CORE_EXT_XXCH
      ) {
        LIBBLU_DTS_DEBUG_EXTSS(
          "       => DCA XXCH 5.1+ Channels Extension within "
          "Core Substream (0x%04" PRIX16 ").\n",
          DCA_EXT_SS_COD_COMP_CORE_EXT_XXCH
        );
      }
      if (
        param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_CORE_EXT_X96
      ) {
        LIBBLU_DTS_DEBUG_EXTSS(
          "       => DCA X96 96kHz Sampling Frequency Extension within "
          "Core Substream (0x%04" PRIX16 ").\n",
          DCA_EXT_SS_COD_COMP_CORE_EXT_X96
        );
      }
      if (
        param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_CORE_EXT_XCH
      ) {
        LIBBLU_DTS_DEBUG_EXTSS(
          "       => DCA XCH 6.1 Channels Extension within "
          "Core Substream (0x%04" PRIX16 ").\n",
          DCA_EXT_SS_COD_COMP_CORE_EXT_XCH
        );
      }

      if (
        param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_CORE_DCA
      ) {
        LIBBLU_DTS_DEBUG_EXTSS(
          "       => DCA Core Component within cur-> Extension Substream "
          "(0x%04" PRIX16 ").\n",
          DCA_EXT_SS_COD_COMP_EXTSUB_CORE_DCA
        );

        LIBBLU_DTS_DEBUG_EXTSS(
          "        Size of Core Component in Extension Substream "
          "(nuExSSCoreFsize): %" PRId64 " bytes (0x%" PRIX64 ").\n",
          param->codingComponents.extSSCore.size,
          param->codingComponents.extSSCore.size - 1
        );

        LIBBLU_DTS_DEBUG_EXTSS(
          "        Core Sync Word Presence (bExSSCoreSyncPresent): "
          "%s (0b%x).\n",
          BOOL_PRESENCE(param->codingComponents.extSSCore.syncWordPresent),
          param->codingComponents.extSSCore.syncWordPresent
        );

        if (param->codingComponents.extSSCore.syncWordPresent) {
          LIBBLU_DTS_DEBUG_EXTSS(
            "        Core Sync Word distance (nuExSSCoreSyncDistInFrames): "
            "%u frames (0x%X).\n",
            param->codingComponents.extSSCore.syncDistanceInFrames,
            param->codingComponents.extSSCore.syncDistanceInFramesCode
          );
        }
      }

      if (
        param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_XBR
      ) {
        LIBBLU_DTS_DEBUG_EXTSS(
          "       => DCA XBR Extended Bit Rate within cur-> "
          "Extension Substream (commercial name: DTS-HDHR) "
          "(0x%04" PRIX16 ").\n",
          DCA_EXT_SS_COD_COMP_EXTSUB_XBR
        );

        LIBBLU_DTS_DEBUG_EXTSS(
          "        Size of XBR Extension in Extension Substream "
          "(nuExSSXBRFsize): %" PRId64 " bytes (0x%" PRIX64 ").\n",
          param->codingComponents.extSSXbr.size,
          param->codingComponents.extSSXbr.size - 1
        );
      }

      if (
        param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_XXCH
      ) {
        LIBBLU_DTS_DEBUG_EXTSS(
          "       => DCA XXCH 5.1+ Channels Extension within cur-> "
          "Extension Substream (0x%04" PRIX16 ").\n",
          DCA_EXT_SS_COD_COMP_EXTSUB_XXCH
        );

        LIBBLU_DTS_DEBUG_EXTSS(
          "        Size of XXCH Extension in Extension Substream "
          "(nuExSSXXChFsize): %" PRId64 " bytes (0x%" PRIX64 ").\n",
          param->codingComponents.extSSXxch.size,
          param->codingComponents.extSSXxch.size - 1
        );
      }

      if (
        param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_X96
      ) {
        LIBBLU_DTS_DEBUG_EXTSS(
          "       => DCA X96 96kHz Sampling Frequency Extension within cur-> "
          "Extension Substream (0x%04" PRIX16 ").\n",
          DCA_EXT_SS_COD_COMP_EXTSUB_X96
        );

        LIBBLU_DTS_DEBUG_EXTSS(
          "        Size of X96 Extension in Extension Substream "
          "(nuExSSX96Fsize): %" PRId64 " bytes (0x%" PRIX64 ").\n",
          param->codingComponents.extSSX96.size,
          param->codingComponents.extSSX96.size - 1
        );
      }

      if (
        param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_LBR
      ) {
        LIBBLU_DTS_DEBUG_EXTSS(
          "       => DCA LBR Low Bitrate Component within cur-> Extension "
          "Substream (commercial name: DTS-Express) (0x%04" PRIX16 ").\n",
          DCA_EXT_SS_COD_COMP_EXTSUB_LBR
        );

        LIBBLU_DTS_DEBUG_EXTSS(
          "        Size of LBR Extension in Extension Substream "
          "(nuExSSLBRFsize): %" PRId64 " bytes (0x%" PRIX64 ").\n",
          param->codingComponents.extSSLbr.size,
          param->codingComponents.extSSLbr.size - 1
        );

        LIBBLU_DTS_DEBUG_EXTSS(
          "        LBR Sync Word Presence (bExSSLBRSyncPresent): "
          "%s (0b%x).\n",
          BOOL_PRESENCE(param->codingComponents.extSSLbr.syncWordPresent),
          param->codingComponents.extSSLbr.syncWordPresent
        );

        if (param->codingComponents.extSSLbr.syncWordPresent) {
          LIBBLU_DTS_DEBUG_EXTSS(
            "        LBR Sync Word distance (nuExSSLBRSyncDistInFrames): "
            "%u frames (0x%X).\n",
            param->codingComponents.extSSLbr.syncDistanceInFrames,
            param->codingComponents.extSSLbr.syncDistanceInFramesCode
          );
        }
      }

      if (
        param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_XLL
      ) {
        LIBBLU_DTS_DEBUG_EXTSS(
          "       => DCA XLL Lossless Extension within cur-> Extension "
          "Substream (commercial name: DTS-HDMA) (0x%04" PRIX16 ").\n",
          DCA_EXT_SS_COD_COMP_EXTSUB_XLL
        );

        LIBBLU_DTS_DEBUG_EXTSS(
          "        Size of XLL Extension in Extension Substream "
          "(nuExSSXLLFsize): %" PRId64 " bytes (0x%" PRIX64 ").\n",
          param->codingComponents.extSSXll.size,
          param->codingComponents.extSSXll.size - 1
        );

        LIBBLU_DTS_DEBUG_EXTSS(
          "        XLL Sync Word Presence (bExSSXLLSyncPresent): "
          "%s (0b%x).\n",
          BOOL_PRESENCE(param->codingComponents.extSSXll.syncWordPresent),
          param->codingComponents.extSSXll.syncWordPresent
        );

        if (param->codingComponents.extSSXll.syncWordPresent) {
          LIBBLU_DTS_DEBUG_EXTSS(
            "        Peak Bit-Rate Smoothing Buffer Size "
            "(nuPeakBRCntrlBuffSzkB): %zu kBits (0x%X).\n",
            param->codingComponents.extSSXll.peakBitRateSmoothingBufSize,
            param->codingComponents.extSSXll.peakBitRateSmoothingBufSizeCode
          );

          LIBBLU_DTS_DEBUG_EXTSS(
            "        Initial XLL Decoding Delay (nuInitLLDecDlyFrames): "
            "%u frames (0x%X).\n",
            param->codingComponents.extSSXll.initialXllDecodingDelayInFrames,
            param->codingComponents.extSSXll.initialXllDecodingDelayInFrames
          );

          LIBBLU_DTS_DEBUG_EXTSS(
            "        Number of Bytes Offset to XLL Sync word "
            "(nuExSSXLLSyncOffset): %zu bytes (0x%zX).\n",
            param->codingComponents.extSSXll.nbBytesOffXllSync,
            param->codingComponents.extSSXll.nbBytesOffXllSync
          );

          LIBBLU_DTS_DEBUG_EXTSS(
            "        DTS-HD XLL Stream ID (nuDTSHDStreamID): 0x%02" PRIX8 ".\n",
            param->codingComponents.extSSXll.steamId,
            param->codingComponents.extSSXll.steamId
          );
        }
      }

      break;

    case DCA_EXT_SS_CODING_MODE_AUXILIARY_CODING:
      LIBBLU_DTS_DEBUG_EXTSS(
        "      Size of Auxilary Coded Data (nuExSSAuxFsize): "
        "%" PRId64 " bytes (0x%" PRIX64 ").\n",
        param->auxilaryCoding.size,
        param->auxilaryCoding.size - 1
      );

      LIBBLU_DTS_DEBUG_EXTSS(
        "      Auxilary Codec Identification (nuAuxCodecID): "
        "0x%02" PRIX8 ".\n",
        param->auxilaryCoding.auxCodecId
      );

      LIBBLU_DTS_DEBUG_EXTSS(
        "      Aux Sync Word Presence (bExSSAuxSyncPresent): "
        "%s (0b%x).\n",
        BOOL_PRESENCE(param->auxilaryCoding.syncWordPresent),
        param->auxilaryCoding.syncWordPresent
      );

      if (param->auxilaryCoding.syncWordPresent) {
        LIBBLU_DTS_DEBUG_EXTSS(
          "      Aux Sync Word distance (nuExSSAuxSyncDistInFrames): "
          "%u frames (0x%X).\n",
          param->auxilaryCoding.syncDistanceInFrames,
          param->auxilaryCoding.syncDistanceInFramesCode
        );
      }
  }

  if (param->mixMetadataFieldsPresent) {
    LIBBLU_DTS_DEBUG_EXTSS(
      "     One-to-one mixing (bOnetoOneMixingFlag): %s (0b%x).\n",
      BOOL_STR(param->mixMetadata.oneTrackToOneChannelMix),
      param->mixMetadata.oneTrackToOneChannelMix
    );

    if (param->mixMetadata.oneTrackToOneChannelMix) {
      LIBBLU_DTS_DEBUG_EXTSS("     One-to-one mixing related:\n");

      LIBBLU_DTS_DEBUG_EXTSS(
        "      Enable Per Channel Scaling (bEnblPerChMainAudioScale): "
        "%s (0b%x).\n",
        BOOL_INFO(param->mixMetadata.perChannelMainAudioScaleCode),
        param->mixMetadata.perChannelMainAudioScaleCode
      );

      LIBBLU_DTS_DEBUG_EXTSS(
        "      Scaling Codes of Main Audio (nuMainAudioScaleCode[ns][%s]):\n",
        (param->mixMetadata.perChannelMainAudioScaleCode) ? "0" : "ch"
      );
      for (unsigned ns = 0; ns < mix_meta.nuNumMixOutConfigs; ns++) {
        LIBBLU_DTS_DEBUG_EXTSS("       - Config %u: ", ns);
        if (param->mixMetadata.perChannelMainAudioScaleCode) {
          for (unsigned nCh = 0; nCh < mix_meta.nNumMixOutCh[ns]; nCh++) {
            if (0 < nCh)
              LIBBLU_DTS_DEBUG_EXTSS_NH(", ");
            LIBBLU_DTS_DEBUG_EXTSS_NH(
              "%u", param->mixMetadata.mainAudioScaleCodes[ns][nCh]
            );
          }
          LIBBLU_DTS_DEBUG_EXTSS_NH(".\n");
        }
        else {
          LIBBLU_DTS_DEBUG_EXTSS_NH(
            "%u.\n", param->mixMetadata.mainAudioScaleCodes[ns][0]
          );
        }
      }
    }
  }

  LIBBLU_DTS_DEBUG_EXTSS(
    "     Decode asset in Secondary Decoder "
    "(bDecodeAssetInSecondaryDecoder): "
    "%s (0b%x).\n",
    BOOL_INFO(param->decodeInSecondaryDecoder),
    param->decodeInSecondaryDecoder
  );

  if (param->extraDataPresent) {
    LIBBLU_DTS_DEBUG_EXTSS("     NOTE: Extra data fields present at end.\n");

    LIBBLU_DTS_DEBUG_EXTSS(
      "     Rev2 DRC Metadata Presence (bDRCMetadataRev2Present): "
      "%s (0b%x).\n",
      BOOL_PRESENCE(param->drcRev2Present),
      param->drcRev2Present
    );

    if (param->drcRev2Present) {
      LIBBLU_DTS_DEBUG_EXTSS(
        "     Rev2 DRC Metadata Version (bDRCMetadataRev2Present): "
        "%u (0b%x).\n",
        param->drcRev2.Hdr_Version,
        param->drcRev2.Hdr_Version - 1
      );
    }
  }

  return 0;
}

static int _checkDcaAudioAssetDescriptorCompliance(
  const DcaAudioAssetDescParameters * param,
  bool is_sec_stream,
  const DcaExtSSHeaderMixMetadataParameters mix_meta,
  DtsDcaExtSSWarningFlags * warn_flags
)
{

  LIBBLU_DTS_DEBUG_EXTSS(
    "    Size of Audio Asset Descriptor (nuAssetDescriptFsize): "
    "%" PRId64 " bytes (0x%" PRIX64 ").\n",
    param->descriptorLength,
    param->descriptorLength - 1
  );

  LIBBLU_DTS_DEBUG_EXTSS(
    "    Audio Asset Identifier (nuAssetIndex): %u (0x%X).\n",
    param->assetIndex,
    param->assetIndex
  );

  if (param->staticFieldsPresent) {
    LIBBLU_DTS_DEBUG_EXTSS(
      "    Static fields:\n"
    );

    int ret = _checkDcaAudioAssetDescriptorStaticFieldsCompliance(
      &param->staticFields,
      is_sec_stream,
      warn_flags
    );
    if (ret < 0)
      return -1;
  }
  else {
    LIBBLU_DTS_DEBUG_EXTSS(
      "    NOTE: Static fields absent (bStaticFieldsPresent == false).\n"
    );
  }

  LIBBLU_DTS_DEBUG_EXTSS(
    "    Dynamic Metadata:\n"
  );

  int ret = _checkDcaAudioAssetDescriptorDynamicMetadataCompliance(
    &param->dynMetadata,
    &param->staticFields
  );
  if (ret < 0)
    return -1;

  LIBBLU_DTS_DEBUG_EXTSS(
    "    Decoder Navigation Data:\n"
  );

  ret = _checkDcaAudioAssetDescriptorDecoderNavDataCompliance(
    &param->decNavData,
    mix_meta,
    is_sec_stream
  );
  if (ret < 0)
    return -1;

  LIBBLU_DTS_DEBUG_EXTSS(
    "    Reserved data field length (Reserved): %" PRId64 " byte(s).\n",
    param->resFieldLength
  );
  LIBBLU_DTS_DEBUG_EXTSS(
    "    Byte boundary padding field "
    "(ZeroPadForFsize): %" PRId64 " bit(s).\n",
    param->paddingBits
  );

  if (
    DCA_EXT_SS_IS_SUPP_RES_FIELD_SIZES(
      param->resFieldLength, param->paddingBits
    )
  ) {
    LIBBLU_DTS_DEBUG_EXTSS(
      "     -> Content:"
    );

    int64_t size = param->resFieldLength + (param->paddingBits + 7) / 8;
    for (int64_t i = 0; i < size; i++)
      LIBBLU_DTS_DEBUG_EXTSS_NH(" 0x%" PRIX8, param->resFieldData[i]);
    LIBBLU_DTS_DEBUG_EXTSS_NH(".\n");
  }

  return 0;
}

int checkDcaExtSSHeaderCompliance(
  const DcaExtSSHeaderParameters * param,
  bool is_sec_stream,
  DtsDcaExtSSWarningFlags * warn_flags
)
{

  LIBBLU_DTS_DEBUG_EXTSS(" Extension Substream Header:\n");

  LIBBLU_DTS_DEBUG_EXTSS(
    "  Sync word (SYNCEXTSSH): 0x%08" PRIX32 ".\n",
    DCA_SYNCEXTSSH
  );

  LIBBLU_DTS_DEBUG_EXTSS(
    "  User defined bits (UserDefinedBits): 0x%02" PRIX8 ".\n",
    param->userDefinedBits
  );

  if (param->userDefinedBits && !warn_flags->presenceOfUserDefinedBits) {
    LIBBLU_INFO(LIBBLU_DTS_PREFIX "Presence of an user-data byte.\n");
    warn_flags->presenceOfUserDefinedBits = true;
  }

  LIBBLU_DTS_DEBUG_EXTSS(
    "  Extension Substream Index (nExtSSIndex): %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->extSSIdx, param->extSSIdx
  );

  if (
    (param->extSSIdx != 0x0 && param->extSSIdx != 0x2)
    || ((param->extSSIdx == 0x2) ^ is_sec_stream)
  )
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Unexpected Extension substream index value "
      "(0x%02" PRIX8 " parsed, expect 0x0 for primary audio "
      "or 0x2 for secondary audio).\n",
      param->extSSIdx
    );

  LIBBLU_DTS_DEBUG_EXTSS(
    "  Short size fields flag (bHeaderSizeType): %s (0b%x).\n",
    BOOL_STR(param->longHeaderSizeFlag),
    param->longHeaderSizeFlag
  );
  if (param->longHeaderSizeFlag)
    LIBBLU_DTS_DEBUG_EXTSS(
      "   => Short size fields, header size expressed using 8 bits "
      "(up to 256 bytes).\n"
    );
  else
    LIBBLU_DTS_DEBUG_EXTSS(
      "   => Long size fields, header size expressed using 12 bits "
      "(up to 4 kBytes).\n"
    );

  LIBBLU_DTS_DEBUG_EXTSS(
    "  Extension Substream header size (nuExtSSHeaderSize): "
    "%zu bytes (0x%zx).\n",
    param->extensionSubstreamHeaderLength,
    param->extensionSubstreamHeaderLength - 1
  );

  LIBBLU_DTS_DEBUG_EXTSS(
    "  Extension Substream Frame size "
    "(nuExtSSFsize): %zu bytes (0x%zx).\n",
    param->extensionSubstreamFrameLength,
    param->extensionSubstreamFrameLength - 1
  );

  LIBBLU_DTS_DEBUG_EXTSS(
    "  Per Stream Static Fields presence "
    "(bStaticFieldsPresent): %s (0b%d).\n",
    (param->staticFieldsPresent) ? "Present" : "Missing",
    param->staticFieldsPresent
  );

  if (!param->staticFieldsPresent)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Expect presence of Stream Static fields in Extension Substream.\n"
    );

  if (param->staticFieldsPresent) {
    LIBBLU_DTS_DEBUG_EXTSS("  Static Fields:\n");

    int ret = _checkDcaExtSSHeaderStaticFieldsCompliance(
      &param->staticFields,
      is_sec_stream,
      param->extSSIdx,
      warn_flags
    );
    if (ret < 0)
      return -1;
  }
  else {
    LIBBLU_DTS_DEBUG_EXTSS("  Static Fields absence default parameters:\n");
    LIBBLU_DTS_DEBUG_EXTSS("   Number of Audio Presentations: 1.\n");
    LIBBLU_DTS_DEBUG_EXTSS("   Number of Audio Assets: 1.\n");
  }

  LIBBLU_DTS_DEBUG_EXTSS(
    "  Size of Encoded Audio Assets data (nuAssetFsize[nAst]):\n"
  );
  for (unsigned nAst = 0; nAst < param->staticFields.nbAudioAssets; nAst++)
    LIBBLU_DTS_DEBUG_EXTSS(
      "   -> Asset %u: %zu bytes.\n",
      nAst, param->audioAssetsLengths[nAst]
    );

  LIBBLU_DTS_DEBUG_EXTSS(
    "  Audio Asset Descriptors (AssetDescriptor{}):\n"
  );
  for (unsigned nAst = 0; nAst < param->staticFields.nbAudioAssets; nAst++) {
    LIBBLU_DTS_DEBUG_EXTSS("   - Asset %u:\n", nAst);

    int ret = _checkDcaAudioAssetDescriptorCompliance(
      &param->audioAssets[nAst],
      is_sec_stream,
      param->staticFields.mixMetadata,
      warn_flags
    );
    if (ret < 0)
      return -1;
  }

  LIBBLU_DTS_DEBUG_EXTSS(
    "  Backward Compatible Core Presence (bBcCorePresent[nAst]):\n"
  );
  for (unsigned nAst = 0; nAst < param->staticFields.nbAudioAssets; nAst++) {
    LIBBLU_DTS_DEBUG_EXTSS(
      "   - Asset %u: %s (0b%x).\n",
      nAst,
      BOOL_PRESENCE(param->audioAssetBckwdCompCorePresent[nAst]),
      param->audioAssetBckwdCompCorePresent[nAst]
    );

    if (param->audioAssetBckwdCompCorePresent[nAst]) {
      LIBBLU_DTS_DEBUG_EXTSS(
        "    Backward Compatible Core location Extension Substream Index "
        "(nuBcCoreExtSSIndex): %" PRIu8 " (0x%02" PRIX8 ").\n",
        param->audioAssetBckwdCompCore[nAst].extSSIndex,
        param->audioAssetBckwdCompCore[nAst].extSSIndex
      );
      LIBBLU_DTS_DEBUG_EXTSS(
        "    Backward Compatible Core location Asset Index "
        "(nuBcCoreAssetIndex): %" PRIu8 " (0x%02" PRIX8 ").\n",
        param->audioAssetBckwdCompCore[nAst].assetIndex,
        param->audioAssetBckwdCompCore[nAst].assetIndex
      );
    }
  }

  if (is_sec_stream) {
    if (param->audioAssetBckwdCompCorePresent[0])
      LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
        "Unexpected presence of backward compatible Core in Extension "
        "Substream header for a Secondary audio stream.\n"
      );
  }
  else {
    if (!param->audioAssetBckwdCompCorePresent[0])
      LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
        "Missing backward compatible Core in Extension Substream header.\n"
      );

    if (
      param->audioAssetBckwdCompCore[0].extSSIndex != 0
      || param->audioAssetBckwdCompCore[0].assetIndex != 0
    )
      LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
        "Unexpected location of backward compatible core in Extension "
        "Substream (expect Substream Index 0 / Audio Asset 0, "
        "SSIdx %u / AA %u parsed).\n",
        param->audioAssetBckwdCompCore[0].extSSIndex,
        param->audioAssetBckwdCompCore[0].assetIndex
      );
  }

  LIBBLU_DTS_DEBUG_EXTSS(
    "  Reserved data field length (Reserved): %" PRId64 " byte(s).\n",
    param->resFieldLength
  );

  LIBBLU_DTS_DEBUG_EXTSS(
    "  Byte boundary padding field (ByteAlign): %" PRId64 " bit(s).\n",
    param->paddingBits
  );

  if (
    DCA_EXT_SS_IS_SUPP_RES_FIELD_SIZES(
      param->resFieldLength, param->paddingBits
    )
  ) {
    LIBBLU_DTS_DEBUG_EXTSS(
      "   -> Content:"
    );

    int64_t size = param->resFieldLength + (param->paddingBits + 7) / 8;
    for (int64_t i = 0; i < size; i++)
      LIBBLU_DTS_DEBUG_EXTSS_NH(" 0x%02" PRIX8, param->resFieldData[i]);
    LIBBLU_DTS_DEBUG_EXTSS_NH(".\n");
  }

  LIBBLU_DTS_DEBUG_EXTSS(
    "  Extension Substream Header CRC16 checksum (nCRC16ExtSSHeader): "
    "0x%04" PRIX16 ".\n",
    param->HCRC
  );
#if !DCA_EXT_SS_DISABLE_CRC
  LIBBLU_DTS_DEBUG_EXTSS(
    "   -> Verified.\n"
  );
#else
  LIBBLU_DTS_DEBUG_EXTSS(
    "   -> Unckecked.\n"
  );
#endif

  return 0;
}
