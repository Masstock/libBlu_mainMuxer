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
  const DcaExtSSHeaderSFParameters * param,
  bool is_secondary,
  unsigned nExtSSIndex,
  DtsDcaExtSSWarningFlags * warn_flags
)
{
  unsigned RefClock = getDcaExtReferenceClockValue(param->nuRefClockCode);
  LIBBLU_DTS_DEBUG_EXTSS(
    "   Reference clock (nuRefClockCode): %u Hz (0x%02" PRIX8 ").\n",
    RefClock,
    param->nuRefClockCode
  );

  if (DCA_EXT_SS_REF_CLOCK_PERIOD_RES == param->nuRefClockCode)
    LIBBLU_DTS_ERROR_RETURN(
      "Reserved value in use (nuRefClockCode == 0x3).\n"
    );

  if (param->nuRefClockCode != DCA_EXT_SS_REF_CLOCK_PERIOD_48000)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Invalid reference clock frequency of %u Hz. "
      "BDAV specifications only allows multiples of 48 kHz clock.\n",
      RefClock
    );

  LIBBLU_DTS_DEBUG_EXTSS(
    "   Frame duration code (nuExSSFrameDurationCode): "
    "%" PRIu32 " clock samples (0x%02" PRIX8 ").\n",
    param->nuExSSFrameDurationCode,
    param->nuExSSFrameDurationCode_code
  );
  LIBBLU_DTS_DEBUG_EXTSS(
    "    => Frame duration in seconds (ExSSFrameDuration): %f s.\n",
    getExSSFrameDurationDcaExtRefClockCode(param)
  );

  if (!is_secondary) {
    if (512 != param->nuExSSFrameDurationCode)
      LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
        "Primary audio Extension Substream frames shall carry "
        "512 clock samples (found %u).\n",
        param->nuExSSFrameDurationCode
      );
  }
  else {
    if (4096 != param->nuExSSFrameDurationCode)
      LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
        "Secondary audio Extension Substream frames shall carry "
        "4096 clock samples (found %u).\n",
        param->nuExSSFrameDurationCode
      );
  }

  LIBBLU_DTS_DEBUG_EXTSS(
    "   Timecode presence (bTimeStampFlag): %s (0b%x).\n",
    BOOL_PRESENCE(param->bTimeStampFlag),
    param->bTimeStampFlag
  );

  if (param->bTimeStampFlag) {
    LIBBLU_DTS_DEBUG_EXTSS(
      "    Timecode data (nuTimeStamp / nLSB): "
      "0x%08" PRIX32 " 0x%01" PRIX8 ".\n",
      param->nuTimeStamp,
      param->nLSB
    );
    uint64_t timeStamp = param->nuTimeStamp << 4 | param->nLSB;
    LIBBLU_DTS_DEBUG_EXTSS(
      "     => %02u:%02u:%02u.%05u.\n",
      (timeStamp / RefClock) / 3600,
      (timeStamp / RefClock) / 60 % 60,
      (timeStamp / RefClock) % 60,
      (timeStamp % RefClock)
    );
  }

  LIBBLU_DTS_DEBUG_EXTSS(
    "   Number of Audio Presentations (nuNumAudioPresnt): %u (0x%X).\n",
    param->nuNumAudioPresnt,
    param->nuNumAudioPresnt - 1
  );

  if (1 < param->nuNumAudioPresnt)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Unsupported multiple Audio Presentations in Extension Substream.\n"
    );

  LIBBLU_DTS_DEBUG_EXTSS(
    "   Number of Audio Assets (nuNumAssets): %u (0x%X).\n",
    param->nuNumAssets,
    param->nuNumAssets - 1
  );

  if (1 < param->nuNumAssets)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Unsupported multiple Audio Assets in Extension Substream.\n"
    );

  LIBBLU_DTS_DEBUG_EXTSS(
    "   Active Extension(s) Substream(s) for Audio Presentation(s) "
    "(nuActiveExSSMask[nAuPr] / nuActiveAssetMask[nAuPr][nSS]):\n"
  );

  for (unsigned nAuPr = 0; nAuPr < param->nuNumAudioPresnt; nAuPr++) {
    LIBBLU_DTS_DEBUG_EXTSS("    -> Audio Presentation %u: ", nAuPr);

    if (!param->nuActiveExSSMask[nAuPr])
      LIBBLU_DTS_DEBUG_EXTSS_NH("No Extension Substream (0x0);\n");

    else {
      const char * sep = "";

      for (unsigned nSS = 0; nSS < nExtSSIndex + 1; nSS++) {
        if ((param->nuActiveExSSMask[nAuPr] >> nSS) & 0x1) {
          LIBBLU_DTS_DEBUG_EXTSS_NH("%sSubstream %u", sep, nSS);
          sep = ", ";
        }
      }
      LIBBLU_DTS_DEBUG_EXTSS_NH(
        " (0x%02" PRIX8 ");\n", param->nuActiveExSSMask[nAuPr]
      );
    }
  }

  if (!is_secondary) {
    if (param->nuActiveExSSMask[0] != 0x1)
      LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
        "Unexpected Active Audio Asset Mask for primary audio "
        "(0x%02" PRIX8 ", expect 0x1).\n",
        param->nuActiveExSSMask[0]
      );
  }
  else {
    if (param->nuActiveExSSMask[0] != 0x5)
      LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
        "Unexpected Active Audio Asset Mask for primary audio "
        "(0x%02" PRIX8 ", expect 0x5).\n",
        param->nuActiveExSSMask[0]
      );
  }

  LIBBLU_DTS_DEBUG_EXTSS(
    "   Mixing Metadata presence (bMixMetadataEnbl): %s (0b%x).\n",
    BOOL_PRESENCE(param->bMixMetadataEnbl),
    param->bMixMetadataEnbl
  );

  if (param->bMixMetadataEnbl) {
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

static int _checkDcaAudioAssetDescSFCompliance(
  const DcaAudioAssetDescSFParameters * param,
  bool is_secondary,
  DtsDcaExtSSWarningFlags * warn_flags
)
{

  LIBBLU_DTS_DEBUG_EXTSS(
    "     Asset type descriptor presence (bAssetTypeDescrPresent): "
    "%s (0b%x).\n",
    BOOL_PRESENCE(param->bAssetTypeDescrPresent),
    param->bAssetTypeDescrPresent
  );

  if (param->bAssetTypeDescrPresent) {
    LIBBLU_DTS_DEBUG_EXTSS(
      "     Asset type descriptor (nuAssetTypeDescriptor): 0x%02" PRIX8 ".\n",
      param->nuAssetTypeDescriptor
    );

    LIBBLU_DTS_DEBUG_EXTSS(
      "      => %s.\n",
      dtsExtAssetTypeDescCodeStr(param->nuAssetTypeDescriptor)
    );

    if (param->nuAssetTypeDescriptor == DCA_EXT_SS_ASSET_TYPE_DESC_RESERVED)
      LIBBLU_DTS_ERROR_RETURN(
        "Reserved value in use (nuAssetTypeDescriptor == 15)"
      );
  }

  LIBBLU_DTS_DEBUG_EXTSS(
    "     Language Descriptor Presence (bLanguageDescrPresent): "
    "%s (0b%x).\n",
    BOOL_PRESENCE(param->bLanguageDescrPresent),
    param->bLanguageDescrPresent
  );

  if (param->bLanguageDescrPresent) {
    LIBBLU_DTS_DEBUG_EXTSS(
      "     Language Descriptor (LanguageDescriptor): "
      "0x%02" PRIX8 "%02" PRIX8 "%02" PRIX8 ".\n",
      param->LanguageDescriptor[0],
      param->LanguageDescriptor[1],
      param->LanguageDescriptor[2]
    );

    LIBBLU_DTS_DEBUG_EXTSS(
      "      => ISO Latin-1: '%s'.\n",
      (char *) param->LanguageDescriptor
    );
  }

  LIBBLU_DTS_DEBUG_EXTSS(
    "     Additional Textual Information Presence (bInfoTextPresent): "
    "%s (0x%x).\n",
    BOOL_PRESENCE(param->bInfoTextPresent),
    param->bInfoTextPresent
  );

  if (param->bInfoTextPresent) {
    LIBBLU_DTS_DEBUG_EXTSS(
      "     Additional Textual Information size (nuInfoTextByteSize): "
      "%" PRId64 " bytes "
      "(0x%" PRIX64 ").\n",
      param->nuInfoTextByteSize,
      param->nuInfoTextByteSize - 1
    );

    LIBBLU_DTS_DEBUG_EXTSS(
      "     Additional Textual Information (InfoTextString):\n"
    );
    LIBBLU_DTS_DEBUG_EXTSS(
      "      => ISO Latin-1: '%s'.\n",
      (char *) param->InfoTextString
    );
  }

  LIBBLU_DTS_DEBUG_EXTSS(
    "     PCM audio Bit Depth (nuBitResolution): %u bits (0x%X).\n",
    param->nuBitResolution,
    param->nuBitResolution - 1
  );

  if (param->nuBitResolution != 16 && param->nuBitResolution != 24)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Unexpected Extension Substream PCM Bit Depth of %u bits "
      "(BDAV specifications only allows 16 or 24 bits).\n",
      param->nuBitResolution
    );

  unsigned sample_rate = getSampleFrequencyDcaExtMaxSampleRate(
    param->nuMaxSampleRate
  );
  LIBBLU_DTS_DEBUG_EXTSS(
    "     Maximal audio Sampling Rate code "
    "(nuMaxSampleRate): %u Hz (0x%02" PRIX8 ").\n",
    sample_rate,
    param->nuMaxSampleRate
  );

  if (is_secondary && 48000 != sample_rate)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Unexpected Extension Substream audio Sampling Rate of %u Hz "
      "for a secondary stream (BDAV specifications only allows 48 kHz).\n",
      sample_rate
    );

  if (48000 != sample_rate && 96000 != sample_rate && 192000 != sample_rate)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Unexpected Extension Substream audio Sampling Rate of %u Hz "
      "(BDAV specifications only allows 48, 96 or 192 kHz).\n",
      sample_rate
    );

  LIBBLU_DTS_DEBUG_EXTSS(
    "     Total Number of encoded audio Channels (nuTotalNumChs): "
    "%u (0x%X).\n",
    param->nuTotalNumChs,
    param->nuTotalNumChs - 1
  );

  if (is_secondary && 6 < param->nuTotalNumChs)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Unexpected Extension Substream number of channels %u "
      "for a secondary audio stream "
      "(BDAV specifications only allows up to 6 channels).\n",
      param->nuTotalNumChs
    );

  if (192000 == sample_rate && 6 < param->nuTotalNumChs)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Unexpected Extension Substream number of channels %u "
      "for a 192 kHz sampling rate "
      "(BDAV specifications only allows up to 6 channels at 192 kHz).\n",
      param->nuTotalNumChs
    );

  if (8 < param->nuTotalNumChs)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Unexpected Extension Substream number of channels %u "
      "(BDAV specifications only allows up to 8 channels).\n",
      param->nuTotalNumChs
    );

  LIBBLU_DTS_DEBUG_EXTSS(
    "     Audio Channels maps Direct Speakers feeds "
    "(bOne2OneMapChannels2Speakers): %s (0b%x).\n",
    BOOL_INFO(param->bOne2OneMapChannels2Speakers),
    param->bOne2OneMapChannels2Speakers
  );

  if (!param->bOne2OneMapChannels2Speakers) {
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
      param->nuTotalNumChs == 2
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

  if (param->bOne2OneMapChannels2Speakers) {
    LIBBLU_DTS_DEBUG_EXTSS("     Channels direct speakers feed related:\n");

    LIBBLU_DTS_DEBUG_EXTSS("      Embedded downmixes:\n");
    if (2 < param->nuTotalNumChs) {
      LIBBLU_DTS_DEBUG_EXTSS(
        "       - Stereo downmix (bEmbeddedStereoFlag): %s (0b%x).\n",
        BOOL_PRESENCE(param->bEmbeddedStereoFlag),
        param->bEmbeddedStereoFlag
      );

      if (
        param->bEmbeddedStereoFlag
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
      !param->bEmbeddedStereoFlag
      && is_secondary
      && 2 < param->nuTotalNumChs
      && !warn_flags->absenceOfStereoDownmixForSec
    ) {
      LIBBLU_WARNING(
        "Missing recommended Stereo Down-mix for a Secondary Stream audio "
        "track with more than two channels.\n"
      );
      warn_flags->absenceOfStereoDownmixForSec = true;
    }

    if (6 < param->nuTotalNumChs) {
      LIBBLU_DTS_DEBUG_EXTSS(
        "       - Surround 6 ch. downmix (bEmbeddedSixChFlag): %s (0b%x).\n",
        BOOL_PRESENCE(param->bEmbeddedSixChFlag),
        param->bEmbeddedSixChFlag
      );

      if (!param->bEmbeddedSixChFlag)
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

      if (nb_ch_spkr_activity_mask != param->nuTotalNumChs)
        LIBBLU_DTS_ERROR_RETURN(
          "Unexpected number of channels in loudspeakers activity layout mask "
          "of Extension Substream Asset Descriptor "
          "(expect %u channels, got %u).\n",
          param->nuTotalNumChs,
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
          param->nuNumDecCh4Remap[ns]
        );

        if (param->nuNumSpeakers[ns] < param->nuNumDecCh4Remap[ns])
          LIBBLU_DTS_ERROR_RETURN(
            "Unexpected number of channels in Standard Loudspeakers Remapping "
            "Set to decode (expect at least %u channels, got).\n",
            param->nuNumSpeakers[ns],
            param->nuNumDecCh4Remap[ns]
          );

        LIBBLU_DTS_DEBUG_EXTSS(
          "        Channels remapping masks and associated coefficients "
          "(nuRemapDecChMask / nuSpkrRemapCodes):\n"
        );
        for (unsigned nCh = 0; nCh < param->nuNumSpeakers[ns]; nCh++) {
          if (!param->nCoeff[ns][nCh])
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
            for (unsigned nc = 0; nc < param->nCoeff[ns][nCh]; nc++) {
              LIBBLU_DTS_DEBUG_EXTSS_NH(
                "%s%" PRIu8, sep,
                param->nuSpkrRemapCodes[ns][nCh][nc]
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
      dtsExtRepresentationTypeCodeStr(param->nuRepresentationType)
    );

    if (param->nuRepresentationType == DCA_EXT_SS_LT_RT_MATRIX_SURROUND)
      LIBBLU_DTS_DEBUG_EXTSS("      Implied channels mask: Lt, Rt.\n");
    if (param->nuRepresentationType == DCA_EXT_SS_LH_RH_HEADPHONE)
      LIBBLU_DTS_DEBUG_EXTSS("      Implied channels mask: Lh, Rh.\n");

    if (is_secondary) {
      if (param->nuRepresentationType != DCA_EXT_SS_MIX_REPLACEMENT)
        LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
          "Unexpected Representation Type in Extension Substream Asset "
          "Descriptor for a Secondary Stream, expect "
          "Mixing/Replacement type, received '%s'.\n",
          dtsExtRepresentationTypeCodeStr(param->nuRepresentationType)
        );
    }
    else {
      if (
        param->nuRepresentationType != DCA_EXT_SS_LT_RT_MATRIX_SURROUND
        || param->nuRepresentationType != DCA_EXT_SS_LH_RH_HEADPHONE
      )
        LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
          "Unexpected Representation Type in Extension Substream Asset "
          "Descriptor, expect a valid stereo type, received '%s'.\n",
          dtsExtRepresentationTypeCodeStr(param->nuRepresentationType)
        );
    }
  }

  return 0;
}

static int _checkDcaAudioAssetDescDMCompliance(
  const DcaAudioAssetDescDMParameters * dm,
  const DcaAudioAssetDescSFParameters * ad_sf,
  const DcaExtSSHeaderSFParameters * sf
)
{

  LIBBLU_DTS_DEBUG_EXTSS(
    "     Dynamic Range Control Presence (bDRCCoefPresent): %s (0b%x).\n",
    BOOL_PRESENCE(dm->bDRCCoefPresent),
    dm->bDRCCoefPresent
  );

  if (dm->bDRCCoefPresent) {
    LIBBLU_DTS_DEBUG_EXTSS(
      "     Dynamic Range Control Coefficient Code "
      "(nuDRCCode): %" PRIu8 ".\n",
      dm->nuDRCCode
    );
    LIBBLU_DTS_DEBUG_EXTSS(
      "      -> Gain value (DRC_dB): %.2f dB.\n",
      -32.0 + 0.25 * dm->nuDRCCode
    );
  }

  LIBBLU_DTS_DEBUG_EXTSS(
    "     Dialogue Normalization Presence (bDialNormPresent): %s (0b%x).\n",
    BOOL_PRESENCE(dm->bDialNormPresent),
    dm->bDialNormPresent
  );

  if (dm->bDialNormPresent) {
    LIBBLU_DTS_DEBUG_EXTSS(
      "     Dialogue Normalization Code (nuDRCCode): %" PRIu8 ".\n",
      dm->nuDialNormCode
    );
    LIBBLU_DTS_DEBUG_EXTSS(
      "      -> Dialog Normlization Gain value (DNG): %d dB.\n",
      -((int) dm->nuDialNormCode)
    );
  }

  if (dm->bDRCCoefPresent && ad_sf->bEmbeddedStereoFlag) {
    LIBBLU_DTS_DEBUG_EXTSS(
      "     Dynamic Range Control Coefficient Code for Stereo Downmix "
      "(nuDRC2ChDmixCode): %" PRIu8 ".\n",
      dm->nuDRC2ChDmixCode
    );
    LIBBLU_DTS_DEBUG_EXTSS(
      "      -> Gain value (DRC_dB): %.2f dB.\n",
      -32.0 + 0.25 * dm->nuDRC2ChDmixCode
    );
  }

  if (sf->bMixMetadataEnbl) {
    LIBBLU_DTS_DEBUG_EXTSS(
      "     Mixing Metadata Presence (bMixMetadataPresent): %s (0b%x).\n",
      BOOL_PRESENCE(dm->bMixMetadataPresent),
      dm->bMixMetadataPresent
    );
  }

  if (dm->bMixMetadataPresent) {
    LIBBLU_DTS_DEBUG_EXTSS(
      "     Mixing metadata:\n"
    );
    LIBBLU_DTS_DEBUG_EXTSS(
      "      (TODO) Not implemented yet.\n"
    );
  }

  return 0;
}

static int _checkDcaAudioAssetDescDNDCompliance(
  const DcaAudioAssetDescDecNDParameters * param,
  const DcaExtSSHeaderMixMetadataParameters * mix_meta,
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

      if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_CORE_DCA) {
        LIBBLU_DTS_DEBUG_EXTSS(
          "       => DCA Core Component within Core Substream "
          "(0x%04" PRIX16 ").\n",
          DCA_EXT_SS_COD_COMP_CORE_DCA
        );
      }

      if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_CORE_EXT_XXCH) {
        LIBBLU_DTS_DEBUG_EXTSS(
          "       => DCA XXCH 5.1+ Channels Extension within "
          "Core Substream (0x%04" PRIX16 ").\n",
          DCA_EXT_SS_COD_COMP_CORE_EXT_XXCH
        );
      }

      if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_CORE_EXT_X96) {
        LIBBLU_DTS_DEBUG_EXTSS(
          "       => DCA X96 96kHz Sampling Frequency Extension within "
          "Core Substream (0x%04" PRIX16 ").\n",
          DCA_EXT_SS_COD_COMP_CORE_EXT_X96
        );
      }

      if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_CORE_EXT_XCH) {
        LIBBLU_DTS_DEBUG_EXTSS(
          "       => DCA XCH 6.1 Channels Extension within "
          "Core Substream (0x%04" PRIX16 ").\n",
          DCA_EXT_SS_COD_COMP_CORE_EXT_XCH
        );
      }

      if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_CORE_DCA) {
        const DcaAudioAssetExSSCoreParameters * p = &param->coding_components.ExSSCore;

        LIBBLU_DTS_DEBUG_EXTSS(
          "       => DCA Core Component within current Extension Substream "
          "(0x%04" PRIX16 ").\n",
          DCA_EXT_SS_COD_COMP_EXTSUB_CORE_DCA
        );

        LIBBLU_DTS_DEBUG_EXTSS(
          "        Size of Core Component in Extension Substream "
          "(nuExSSCoreFsize): %" PRIu16 " bytes (0x%04" PRIX16 ").\n",
          p->nuExSSCoreFsize,
          p->nuExSSCoreFsize - 1
        );

        LIBBLU_DTS_DEBUG_EXTSS(
          "        Core Sync Word Presence (bExSSCoreSyncPresent): "
          "%s (0b%x).\n",
          BOOL_PRESENCE(p->bExSSCoreSyncPresent),
          p->bExSSCoreSyncPresent
        );

        if (p->bExSSCoreSyncPresent) {
          LIBBLU_DTS_DEBUG_EXTSS(
            "        Core Sync Word distance (nuExSSCoreSyncDistInFrames): "
            "%u frames (0x%X).\n",
            p->nuExSSCoreSyncDistInFrames,
            p->nuExSSCoreSyncDistInFrames_code
          );
        }
      }

      if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_XBR) {
        const DcaAudioAssetExSSXBRParameters * p = &param->coding_components.ExSSXBR;

        LIBBLU_DTS_DEBUG_EXTSS(
          "       => DCA XBR Extended Bit Rate within current "
          "Extension Substream (commercial name: DTS-HDHR) "
          "(0x%04" PRIX16 ").\n",
          DCA_EXT_SS_COD_COMP_EXTSUB_XBR
        );

        LIBBLU_DTS_DEBUG_EXTSS(
          "        Size of XBR Extension in Extension Substream "
          "(nuExSSXBRFsize): %" PRIu16 " bytes (0x%04" PRIX16 ").\n",
          p->nuExSSXBRFsize,
          p->nuExSSXBRFsize - 1
        );
      }

      if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_XXCH) {
        const DcaAudioAssetExSSXXCHParameters * p = &param->coding_components.ExSSXXCH;

        LIBBLU_DTS_DEBUG_EXTSS(
          "       => DCA XXCH 5.1+ Channels Extension within current "
          "Extension Substream (0x%04" PRIX16 ").\n",
          DCA_EXT_SS_COD_COMP_EXTSUB_XXCH
        );

        LIBBLU_DTS_DEBUG_EXTSS(
          "        Size of XXCH Extension in Extension Substream "
          "(nuExSSXXCHFsize): %" PRIu16 " bytes (0x%04" PRIX16 ").\n",
          p->nuExSSXXCHFsize,
          p->nuExSSXXCHFsize - 1
        );
      }

      if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_X96) {
        const DcaAudioAssetExSSX96Parameters * p = &param->coding_components.ExSSX96;

        LIBBLU_DTS_DEBUG_EXTSS(
          "       => DCA X96 96kHz Sampling Frequency Extension within current "
          "Extension Substream (0x%04" PRIX16 ").\n",
          DCA_EXT_SS_COD_COMP_EXTSUB_X96
        );

        LIBBLU_DTS_DEBUG_EXTSS(
          "        Size of X96 Extension in Extension Substream "
          "(nuExSSX96Fsize): %" PRIu16 " bytes (0x%04" PRIX16 ").\n",
          p->nuExSSX96Fsize,
          p->nuExSSX96Fsize - 1
        );
      }

      if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_LBR) {
        const DcaAudioAssetExSSLBRParameters * p = &param->coding_components.ExSSLBR;

        LIBBLU_DTS_DEBUG_EXTSS(
          "       => DCA LBR Low Bitrate Component within current Extension "
          "Substream (commercial name: DTS-Express) (0x%04" PRIX16 ").\n",
          DCA_EXT_SS_COD_COMP_EXTSUB_LBR
        );

        LIBBLU_DTS_DEBUG_EXTSS(
          "        Size of LBR Extension in Extension Substream "
          "(nuExSSLBRFsize): %" PRIu16 " bytes (0x%04" PRIX16 ").\n",
          p->nuExSSLBRFsize,
          p->nuExSSLBRFsize - 1
        );

        LIBBLU_DTS_DEBUG_EXTSS(
          "        LBR Sync Word Presence (bExSSLBRSyncPresent): "
          "%s (0b%x).\n",
          BOOL_PRESENCE(p->bExSSLBRSyncPresent),
          p->bExSSLBRSyncPresent
        );

        if (p->bExSSLBRSyncPresent) {
          LIBBLU_DTS_DEBUG_EXTSS(
            "        LBR Sync Word distance (nuExSSLBRSyncDistInFrames): "
            "%u frames (0x%X).\n",
            p->nuExSSLBRSyncDistInFrames,
            p->nuExSSLBRSyncDistInFrames_code
          );
        }
      }

      if (param->nuCoreExtensionMask & DCA_EXT_SS_COD_COMP_EXTSUB_XLL) {
        const DcaAudioAssetExSSXllParameters * p = &param->coding_components.ExSSXLL;

        LIBBLU_DTS_DEBUG_EXTSS(
          "       => DCA XLL Lossless Extension within current Extension "
          "Substream (commercial name: DTS-HDMA) (0x%04" PRIX16 ").\n",
          DCA_EXT_SS_COD_COMP_EXTSUB_XLL
        );

        LIBBLU_DTS_DEBUG_EXTSS(
          "        Size of XLL Extension in Extension Substream "
          "(nuExSSXLLFsize): %" PRIu32 " bytes (0x%" PRIX32 ").\n",
          p->nuExSSXLLFsize,
          p->nuExSSXLLFsize - 1
        );

        LIBBLU_DTS_DEBUG_EXTSS(
          "        XLL Sync Word Presence (bExSSXLLSyncPresent): "
          "%s (0b%x).\n",
          BOOL_PRESENCE(p->bExSSXLLSyncPresent),
          p->bExSSXLLSyncPresent
        );

        if (p->bExSSXLLSyncPresent) {
          LIBBLU_DTS_DEBUG_EXTSS(
            "        Peak Bit-Rate Smoothing Buffer Size "
            "(nuPeakBRCntrlBuffSzkB): %" PRIu8 " kBits (0x%" PRIX8 ").\n",
            p->nuPeakBRCntrlBuffSzkB,
            p->nuPeakBRCntrlBuffSzkB >> 4
          );

          LIBBLU_DTS_DEBUG_EXTSS(
            "        Size of the XLL Decoding Delay field (nuBitsInitDecDly): "
            "%" PRIu8 " bits (0x%" PRIX8 ").\n",
            p->nuBitsInitDecDly,
            p->nuBitsInitDecDly
          );

          LIBBLU_DTS_DEBUG_EXTSS(
            "        Initial XLL Decoding Delay (nuInitLLDecDlyFrames): "
            "%" PRIu32 " frames (0x%" PRIX32 ").\n",
            p->nuInitLLDecDlyFrames,
            p->nuInitLLDecDlyFrames
          );

          LIBBLU_DTS_DEBUG_EXTSS(
            "        Number of Bytes Offset to XLL Sync word "
            "(nuExSSXLLSyncOffset): %" PRIu32 " bytes (0x%" PRIX32 ").\n",
            p->nuExSSXLLSyncOffset,
            p->nuExSSXLLSyncOffset
          );
        }

        LIBBLU_DTS_DEBUG_EXTSS(
          "        DTS-HD XLL Stream ID (nuDTSHDStreamID): 0x%02" PRIX8 ".\n",
          p->nuDTSHDStreamID
        );
      }
      break;

    case DCA_EXT_SS_CODING_MODE_AUXILIARY_CODING: {
      const DcaAudioAssetDescDecNDAuxiliaryCoding * aux = &param->auxilary_coding;

      LIBBLU_DTS_DEBUG_EXTSS(
        "      Size of Auxilary Coded Data (nuExSSAuxFsize): "
        "%" PRIu16 " bytes (0x%" PRIX16 ").\n",
        aux->nuExSSAuxFsize,
        aux->nuExSSAuxFsize - 1
      );

      LIBBLU_DTS_DEBUG_EXTSS(
        "      Auxilary Codec Identification (nuAuxCodecID): 0x%02" PRIX8 ".\n",
        aux->nuAuxCodecID
      );

      LIBBLU_DTS_DEBUG_EXTSS(
        "      Aux Sync Word Presence (bExSSAuxSyncPresent): %s (0b%x).\n",
        BOOL_PRESENCE(aux->bExSSAuxSyncPresent),
        aux->bExSSAuxSyncPresent
      );

      if (aux->bExSSAuxSyncPresent) {
        LIBBLU_DTS_DEBUG_EXTSS(
          "      Aux Sync Word distance (nuExSSAuxSyncDistInFrames): "
          "%" PRIu8 " frames (0x%" PRIX8 ").\n",
          aux->nuExSSAuxSyncDistInFrames,
          aux->nuExSSAuxSyncDistInFrames_code
        );
      }
    }
  }

  if (param->mix_md_fields_pres) {
    LIBBLU_DTS_DEBUG_EXTSS(
      "     One-to-one mixing (bOnetoOneMixingFlag): %s (0b%x).\n",
      BOOL_STR(param->bOnetoOneMixingFlag),
      param->bOnetoOneMixingFlag
    );

    if (param->bOnetoOneMixingFlag) {
      LIBBLU_DTS_DEBUG_EXTSS("     One-to-one mixing related:\n");

      LIBBLU_DTS_DEBUG_EXTSS(
        "      Enable Per Channel Scaling (bEnblPerChMainAudioScale): "
        "%s (0b%x).\n",
        BOOL_INFO(param->bEnblPerChMainAudioScale),
        param->bEnblPerChMainAudioScale
      );

      LIBBLU_DTS_DEBUG_EXTSS(
        "      Scaling Codes of Main Audio (nuMainAudioScaleCode[ns][%s]):\n",
        (param->bEnblPerChMainAudioScale) ? "0" : "ch"
      );
      for (unsigned ns = 0; ns < mix_meta->nuNumMixOutConfigs; ns++) {
        LIBBLU_DTS_DEBUG_EXTSS("       - Config %u: ", ns);
        if (param->bEnblPerChMainAudioScale) {
          for (unsigned nCh = 0; nCh < mix_meta->nNumMixOutCh[ns]; nCh++) {
            if (0 < nCh)
              LIBBLU_DTS_DEBUG_EXTSS_NH(", ");
            LIBBLU_DTS_DEBUG_EXTSS_NH(
              "%u", param->nuMainAudioScaleCode[ns][nCh]
            );
          }
          LIBBLU_DTS_DEBUG_EXTSS_NH(".\n");
        }
        else {
          LIBBLU_DTS_DEBUG_EXTSS_NH(
            "%u.\n", param->nuMainAudioScaleCode[ns][0]
          );
        }
      }
    }
  }

  LIBBLU_DTS_DEBUG_EXTSS(
    "     Decode asset in Secondary Decoder (bDecodeAssetInSecondaryDecoder): "
    "%s (0b%x).\n",
    BOOL_INFO(param->bDecodeAssetInSecondaryDecoder),
    param->bDecodeAssetInSecondaryDecoder
  );

  if (param->extra_data_pres) {
    LIBBLU_DTS_DEBUG_EXTSS("     NOTE: Extra data fields present at end.\n");

    LIBBLU_DTS_DEBUG_EXTSS(
      "     Rev2 DRC Metadata Presence (bDRCMetadataRev2Present): "
      "%s (0b%x).\n",
      BOOL_PRESENCE(param->bDRCMetadataRev2Present),
      param->bDRCMetadataRev2Present
    );

    if (param->bDRCMetadataRev2Present) {
      LIBBLU_DTS_DEBUG_EXTSS(
        "     Rev2 DRC Metadata Version (DRCversion_Rev2): "
        "%" PRIu8 " (0x%" PRIX8 ").\n",
        param->DRCversion_Rev2,
        param->DRCversion_Rev2
      );
    }
  }

  return 0;
}

static int _checkDcaAudioAssetDescriptorCompliance(
  const DcaAudioAssetDescParameters * param,
  bool is_sec_stream,
  bool bStaticFieldsPresent,
  const DcaExtSSHeaderSFParameters * sf,
  DtsDcaExtSSWarningFlags * warn_flags
)
{
  const DcaAudioAssetDescSFParameters * ad_sf = &param->static_fields;
  const DcaAudioAssetDescDMParameters * ad_dm = &param->dyn_md;
  const DcaAudioAssetDescDecNDParameters * ad_dnd = &param->dec_nav_data;

  LIBBLU_DTS_DEBUG_EXTSS(
    "    Size of Audio Asset Descriptor (nuAssetDescriptFsize): "
    "%" PRId64 " bytes (0x%" PRIX64 ").\n",
    param->nuAssetDescriptFsize,
    param->nuAssetDescriptFsize - 1
  );

  LIBBLU_DTS_DEBUG_EXTSS(
    "    Audio Asset Identifier (nuAssetIndex): %" PRIu8 " (0x%" PRIX8 ").\n",
    param->nuAssetIndex,
    param->nuAssetIndex
  );

  LIBBLU_DTS_DEBUG_EXTSS("    Static fields:\n");
  if (bStaticFieldsPresent) {
    if (_checkDcaAudioAssetDescSFCompliance(ad_sf, is_sec_stream, warn_flags) < 0)
      return -1;
  }
  else {
    LIBBLU_DTS_DEBUG_EXTSS(
      "     => Absent (bStaticFieldsPresent == false).\n"
    );
  }

  LIBBLU_DTS_DEBUG_EXTSS("    Dynamic Metadata:\n");
  if (_checkDcaAudioAssetDescDMCompliance(ad_dm, ad_sf, sf) < 0)
    return -1;

  LIBBLU_DTS_DEBUG_EXTSS("    Decoder Navigation Data:\n");
  if (_checkDcaAudioAssetDescDNDCompliance(ad_dnd, &sf->mixMetadata, is_sec_stream) < 0)
    return -1;

  LIBBLU_DTS_DEBUG_EXTSS(
    "    Reserved data field length (Reserved): %u byte(s).\n",
    param->Reserved_size
  );
  LIBBLU_DTS_DEBUG_EXTSS(
    "    Byte boundary padding field (ZeroPadForFsize): %u bit(s).\n",
    param->ZeroPadForFsize_size
  );

  if (isSavedReservedFieldDcaExtSS(param->Reserved_size, param->ZeroPadForFsize_size)) {
    LIBBLU_DTS_DEBUG_EXTSS("     -> Content:");
    unsigned size = (param->Reserved_size + (param->ZeroPadForFsize_size + 7)) >> 3;
    for (unsigned i = 0; i < size; i++)
      LIBBLU_DTS_DEBUG_EXTSS_NH(" 0x%" PRIX8, param->Reserved[i]);
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
    param->UserDefinedBits
  );

  if (param->UserDefinedBits && !warn_flags->presenceOfUserDefinedBits) {
    LIBBLU_INFO(LIBBLU_DTS_PREFIX "Presence of an user-data byte.\n");
    warn_flags->presenceOfUserDefinedBits = true;
  }

  LIBBLU_DTS_DEBUG_EXTSS(
    "  Extension Substream Index (nExtSSIndex): %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->nExtSSIndex, param->nExtSSIndex
  );

  if (
    (param->nExtSSIndex != 0x0 && param->nExtSSIndex != 0x2)
    || ((param->nExtSSIndex == 0x2) ^ is_sec_stream)
  )
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Unexpected Extension substream index value "
      "(0x%02" PRIX8 " parsed, expect 0x0 for primary audio "
      "or 0x2 for secondary audio).\n",
      param->nExtSSIndex
    );

  LIBBLU_DTS_DEBUG_EXTSS(
    "  Short size fields flag (bHeaderSizeType): %s (0b%x).\n",
    BOOL_STR(param->bHeaderSizeType),
    param->bHeaderSizeType
  );
  if (0 == param->bHeaderSizeType)
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
    param->nuExtSSHeaderSize,
    param->nuExtSSHeaderSize - 1
  );

  LIBBLU_DTS_DEBUG_EXTSS(
    "  Extension Substream Frame size "
    "(nuExtSSFsize): %" PRIu32 " bytes (0x%*0" PRIu32 ").\n",
    param->nuExtSSFsize,
    param->bHeaderSizeType ? 5 : 4,
    param->nuExtSSFsize - 1
  );

  LIBBLU_DTS_DEBUG_EXTSS(
    "  Per Stream Static Fields presence "
    "(bStaticFieldsPresent): %s (0b%d).\n",
    (param->bStaticFieldsPresent) ? "Present" : "Missing",
    param->bStaticFieldsPresent
  );

  if (!param->bStaticFieldsPresent)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Expect presence of Stream Static fields in Extension Substream.\n"
    );

  if (param->bStaticFieldsPresent) {
    LIBBLU_DTS_DEBUG_EXTSS("  Static Fields:\n");

    int ret = _checkDcaExtSSHeaderStaticFieldsCompliance(
      &param->static_fields,
      is_sec_stream,
      param->nExtSSIndex,
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
  for (unsigned nAst = 0; nAst < param->static_fields.nuNumAssets; nAst++)
    LIBBLU_DTS_DEBUG_EXTSS(
      "   -> Asset %u: %zu bytes.\n",
      nAst, param->audioAssetsLengths[nAst]
    );

  LIBBLU_DTS_DEBUG_EXTSS(
    "  Audio Asset Descriptors (AssetDescriptor{}):\n"
  );
  for (unsigned nAst = 0; nAst < param->static_fields.nuNumAssets; nAst++) {
    LIBBLU_DTS_DEBUG_EXTSS("   - Asset %u:\n", nAst);

    int ret = _checkDcaAudioAssetDescriptorCompliance(
      &param->audioAssets[nAst],
      is_sec_stream,
      param->bStaticFieldsPresent,
      &param->static_fields,
      warn_flags
    );
    if (ret < 0)
      return -1;
  }

  LIBBLU_DTS_DEBUG_EXTSS(
    "  Backward Compatible Core Presence (bBcCorePresent[nAst]):\n"
  );
  for (unsigned nAst = 0; nAst < param->static_fields.nuNumAssets; nAst++) {
    LIBBLU_DTS_DEBUG_EXTSS(
      "   - Asset %u: %s (0b%x).\n",
      nAst,
      BOOL_PRESENCE(param->bBcCorePresent[nAst]),
      param->bBcCorePresent[nAst]
    );

    if (param->bBcCorePresent[nAst]) {
      LIBBLU_DTS_DEBUG_EXTSS(
        "    Backward Compatible Core location Extension Substream Index "
        "(nuBcCoreExtSSIndex): %" PRIu8 " (0x%02" PRIX8 ").\n",
        param->nuBcCoreExtSSIndex[nAst],
        param->nuBcCoreExtSSIndex[nAst]
      );
      LIBBLU_DTS_DEBUG_EXTSS(
        "    Backward Compatible Core location Asset Index "
        "(nuBcCoreAssetIndex): %" PRIu8 " (0x%02" PRIX8 ").\n",
        param->nuBcCoreAssetIndex[nAst],
        param->nuBcCoreAssetIndex[nAst]
      );
    }
  }

  if (is_sec_stream) {
    if (param->bBcCorePresent[0])
      LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
        "Unexpected presence of backward compatible Core in Extension "
        "Substream header for a Secondary audio stream.\n"
      );
  }
  else {
    if (!param->bBcCorePresent[0])
      LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
        "Missing backward compatible Core in Extension Substream header.\n"
      );

    if (0 != param->nuBcCoreExtSSIndex[0] || 0 != param->nuBcCoreAssetIndex[0])
      LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
        "Unexpected location of backward compatible core in Extension "
        "Substream (expect Substream Index 0 / Audio Asset 0, "
        "'nuBcCoreExtSSIndex' == %u / 'nuBcCoreAssetIndex' == %u parsed).\n",
        param->nuBcCoreExtSSIndex[0],
        param->nuBcCoreAssetIndex[0]
      );
  }

  LIBBLU_DTS_DEBUG_EXTSS(
    "  Reserved data field length (Reserved): %u byte(s).\n",
    param->Reserved_size
  );

  LIBBLU_DTS_DEBUG_EXTSS(
    "  Byte boundary padding field (ByteAlign): %u bit(s).\n",
    param->ZeroPadForFsize_size
  );

  if (isSavedReservedFieldDcaExtSS(param->Reserved_size, param->ZeroPadForFsize_size)) {
    LIBBLU_DTS_DEBUG_EXTSS("   -> Content:");
    unsigned size = param->Reserved_size + (param->ZeroPadForFsize_size + 7) / 8;
    for (unsigned i = 0; i < size; i++)
      LIBBLU_DTS_DEBUG_EXTSS_NH(" 0x%02" PRIX8, param->Reserved[i]);
    LIBBLU_DTS_DEBUG_EXTSS_NH(".\n");
  }

  LIBBLU_DTS_DEBUG_EXTSS(
    "  Extension Substream Header CRC16 checksum (nCRC16ExtSSHeader): "
    "0x%04" PRIX16 ".\n",
    param->nCRC16ExtSSHeader
  );
#if !DCA_EXT_SS_DISABLE_CRC
  LIBBLU_DTS_DEBUG_EXTSS("   -> Verified.\n");
#else
  LIBBLU_DTS_DEBUG_EXTSS("   -> Unckecked.\n");
#endif

  return 0;
}
