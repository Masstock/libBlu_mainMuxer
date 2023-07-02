#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include "dts_checks.h"

static int _checkDcaCoreSSFrameHeaderCompliance(
  const DcaCoreSSFrameHeaderParameters * param,
  DtsDcaCoreSSWarningFlags * warning_flags
)
{

  LIBBLU_DTS_DEBUG_CORE(
    "  Sync word (SYNC): 0x%08" PRIX32 ".\n",
    DTS_SYNCWORD_CORE
  );

  LIBBLU_DTS_DEBUG_CORE(
    "  Frame type (FTYPE): %s (0x%d).\n",
    (param->terminationFrame) ? "Normal frame" : "Termination frame",
    param->terminationFrame
  );

  LIBBLU_DTS_DEBUG_CORE(
    "  Deficit sample count (SHORT): %" PRIu8 " samples (0x%02" PRIX8 ").\n",
    param->samplesPerBlock,
    param->samplesPerBlock - 1
  );

  if (!param->terminationFrame && param->samplesPerBlock != 32)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Unexpected number of core samples for a normal frame "
      "(expect %u, receive %u).\n",
      32, param->samplesPerBlock
    );

  LIBBLU_DTS_DEBUG_CORE(
    "  Deprecated CRC presence flag (CPF): %s (0b%x).\n",
    BOOL_PRESENCE(param->crcPresent),
    param->crcPresent
  );

  if (param->crcPresent && !warning_flags->presenceOfDeprecatedCrc) {
    LIBBLU_WARNING(
      "Presence of deprecated DCA CRC field.\n"
    );
    warning_flags->presenceOfDeprecatedCrc = true;
  }

  LIBBLU_DTS_DEBUG_CORE(
    "  Number of PCM samples blocks per channel (NBLKS): "
    "%" PRIu8 " blocks (0x% " PRIX8 ").\n",
    param->blocksPerChannel,
    param->blocksPerChannel - 1
  );

  if (param->blocksPerChannel < 6)
    LIBBLU_DTS_ERROR_RETURN(
      "Invalid NBLKS field range (%" PRIu8 " < 5).\n",
      param->blocksPerChannel
    );

  if (!param->terminationFrame) {
    if (
      (param->blocksPerChannel & 0x7)
      || 0 != (param->blocksPerChannel & (param->blocksPerChannel - 1))
    )
      LIBBLU_DTS_ERROR_RETURN(
        "Invalid number of PCM samples blocks (NBLKS) for a normal frame "
        "(shall be 8, 16, 32, 64 or 128 blocks, not %" PRIu8 ").\n",
        param->blocksPerChannel
      );
  }

  LIBBLU_DTS_DEBUG_CORE(
    "  Core frame size (FSIZE): %" PRId64 " bytes (0x% " PRIX64 ").\n",
    param->frameLength,
    param->frameLength - 1
  );

  if (param->frameLength < 96) {
    LIBBLU_DTS_ERROR_RETURN(
      "Invalid Core audio frame size range (%" PRId64 " < 95).\n",
      param->frameLength
    );
  }

  LIBBLU_DTS_DEBUG_CORE(
    "  Audio channel arrangement code (AMODE): 0x%02" PRIX8 ".\n",
    param->audioChannelArrangement
  );
  LIBBLU_DTS_DEBUG_CORE(
    "   => Audio channels: %s",
    dtsCoreAudioChannelAssignCodeStr(param->audioChannelArrangement)
  );

  if (param->lfePresent) {
    LIBBLU_DTS_DEBUG_CORE_NH(" + LFE;\n");
    LIBBLU_DTS_DEBUG_CORE(
      "   => Nb channels: %d channel(s).\n",
      param->nbChannels + 1
    );
    LIBBLU_DTS_DEBUG_CORE(
      "   => NOTE: Presence of LFE is defined by LFF flag.\n"
    );
  }
  else {
    LIBBLU_DTS_DEBUG_CORE_NH(";\n");
    LIBBLU_DTS_DEBUG_CORE(
      "  => Nb channels: %d channel(s).\n",
      param->nbChannels
    );
  }

  LIBBLU_DTS_DEBUG_CORE(
    "  Core audio sampling frequency code (SFREQ): 0x%02" PRIX8 ".\n",
    param->audioFreqCode
  );
  if (param->audioFreq != 0)
    LIBBLU_DTS_DEBUG_CORE("  => %d Hz.\n", param->audioFreq);
  else {
    LIBBLU_DTS_DEBUG_CORE("  => Reserved value.\n");
    LIBBLU_DTS_ERROR_RETURN(
      "Reserved value in use (SFREQ == 0x%02" PRIX8 ").\n",
      param->audioFreqCode
    );
  }

  if (param->audioFreq != 48000)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "BDAV specifications allows only 48 kHz as "
      "DCA Core audio sampling frequency (found %u Hz).\n",
      param->audioFreq
    );

  LIBBLU_DTS_DEBUG_CORE(
    "  Target transmission bitrate code (RATE): 0x%02" PRIX8 ".\n",
    param->bitRateCode
  );
  switch (param->bitrate) {
    case 0: /* Reserved value */
      LIBBLU_DTS_DEBUG_CORE(
        "   => Bitrate: Reserved value (0x%02" PRIX8 ").\n",
        param->bitRateCode
      );
      LIBBLU_DTS_ERROR_RETURN(
        "Reserved value in use (RATE == 0x%02" PRIX8 ").\n",
        param->bitRateCode
      );

    case 1: /* Open bit-rate */
      LIBBLU_DTS_DEBUG_CORE(
        "   => Bitrate: Reserved value (0x%02" PRIX8 ").\n",
        param->bitRateCode
      );
      LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
        "BDAV specifications disallows use of open transmission rate mode of "
        "DCA Core.\n"
      );
      break;

    default: /* Normal value */
      LIBBLU_DTS_DEBUG_CORE(
        "   => Bitrate: %u Kbits/s.\n",
        param->bitrate
      );
  }

  if (
    param->frameLength * param->blocksPerChannel * param->bitrate
    < ((unsigned) param->frameLength) * param->bitDepth * 8
  )
    LIBBLU_DTS_ERROR_RETURN(
      "Too big Core audio frame size at stream bit-rate "
      "(%" PRId64 " bytes).\n",
      param->frameLength
    );

  LIBBLU_DTS_DEBUG_CORE(
    "  Embedded dynamic range coefficients presence flag "
    "(DYNF): %s (0b%x).\n",
    BOOL_PRESENCE(param->embeddedDynRange),
    param->embeddedDynRange
  );

  LIBBLU_DTS_DEBUG_CORE(
    "  Time stamps fields presence flag (TIMEF): %s (0b%x).\n",
    BOOL_PRESENCE(param->embeddedTimestamp),
    param->embeddedTimestamp
  );

  LIBBLU_DTS_DEBUG_CORE(
    "  Core auxilary data presence flag (AUXF): %s (0b%x).\n",
    BOOL_PRESENCE(param->auxData),
    param->auxData
  );

  LIBBLU_DTS_DEBUG_CORE(
    "  HDCD mastered audio data flag (HDCD): %s (0b%x).\n",
    BOOL_INFO(param->hdcd),
    param->hdcd
  );

  if (param->hdcd && !warning_flags->usageOfHdcdEncoding) {
    LIBBLU_WARNING(
      "Signaled usage of HDCD mastering mode, "
      "normaly reserved for DTS-CDs.\n"
    );
    warning_flags->usageOfHdcdEncoding = true;
  }

  if (param->extAudio) {
    LIBBLU_DTS_DEBUG_CORE(
      "  Core audio extension data type (EXT_AUDIO_ID): "
      "%s (0x%02" PRIX8 ").\n",
      dtsCoreExtendedAudioCodingCodeStr(
        param->extAudioId
      ),
      param->extAudioId
    );

    if (!isValidDtsExtendedAudioCodingCode(param->extAudioId))
      LIBBLU_DTS_ERROR_RETURN(
        "Reserved value in use (EXT_AUDIO_ID == 0x%02" PRIX8 ").\n",
        param->extAudioId
      );
  }

  LIBBLU_DTS_DEBUG_CORE(
    "  Core audio extension presence (EXT_AUDIO): %s (0b%x).\n",
    BOOL_PRESENCE(param->extAudio),
    param->extAudio
  );

  LIBBLU_DTS_DEBUG_CORE(
    "  Audio data synchronization word insertion level (ASPF): %s (0b%x).\n",
    (param->syncWordInsertionLevel) ?
      "At Subsubframe level"
    :
      "At Subframe level",
    param->syncWordInsertionLevel
  );

  LIBBLU_DTS_DEBUG_CORE(
    "  Low Frequency Effects (LFE) channel presence code (LFF): "
    "0x%02" PRIX8 ".\n",
    param->lfePresent
  );

  if (param->lfePresent == 0x3)
    LIBBLU_DTS_ERROR_RETURN(
      "Invalid value in use (LFE == 0x%02" PRIX8 ").\n",
      param->lfePresent
    );

  LIBBLU_DTS_DEBUG_CORE(
    "   => LFE channel: %s.\n",
    BOOL_PRESENCE(0 < param->lfePresent)
  );
  if (0 < param->lfePresent) {
    LIBBLU_DTS_DEBUG_CORE(
      "   => Interpolation level: %d.\n",
      (param->lfePresent == 0x1) ? 128 : 64
    );
  }

  LIBBLU_DTS_DEBUG_CORE(
    "  ADPCM predicator history of last frame usage (HFLAG): %s (0b%x).\n",
    BOOL_INFO(param->predHist),
    param->predHist
  );

  LIBBLU_DTS_DEBUG_CORE(
    "  Filter bank interpolation FIR coefficients type (FILTS): %s (0x%x).\n",
    (param->multiRtInterpolatorSwtch) ? "Perfect" : "Non-perfect",
    param->multiRtInterpolatorSwtch
  );

  LIBBLU_DTS_DEBUG_CORE(
    "  Encoder sofware syntax revision (VERNUM): %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->syntaxCode,
    param->syntaxCode
  );

  if (param->syntaxCode == DTS_MAX_SYNTAX_VERNUM)
    LIBBLU_DTS_DEBUG_CORE("   => Current revision.\n");
  else if (param->syntaxCode < DTS_MAX_SYNTAX_VERNUM)
    LIBBLU_DTS_DEBUG_CORE("   => Future compatible revision.\n");
  else
    LIBBLU_DTS_DEBUG_CORE("   => Future incompatible revision.\n");

  LIBBLU_DTS_DEBUG_CORE(
    "  Copyright history information code (CHIST): 0x%02" PRIX8 ".\n",
    param->copyHistory
  );

  LIBBLU_DTS_DEBUG_CORE(
    "   => %s.\n",
    dtsCoreCopyrightHistoryCodeStr(param->copyHistory)
  );

  LIBBLU_DTS_DEBUG_CORE(
    "  Source PCM resolution code (PCMR): 0x%02" PRIX8 ".\n",
    param->sourcePcmResCode
  );
  LIBBLU_DTS_DEBUG_CORE(
    "   => Bit depth: %u bits.\n",
    param->bitDepth
  );
  LIBBLU_DTS_DEBUG_CORE(
    "   => DTS ES mastered: %s.\n",
    BOOL_INFO(param->isEs)
  );

  LIBBLU_DTS_DEBUG_CORE(
    "  Front speakers encoding mode flag (SUMF): %s (0b%x).\n",
    (param->frontSum) ?
      "Sum / Difference encoded (L = L + R, R = L - R)"
    :
      "Normal (L = L, R = R)",
    param->frontSum
  );

  LIBBLU_DTS_DEBUG_CORE(
    "  Surround speakers encoding mode flag (SUMS): %s (0b%x).\n",
    (param->surroundSum) ?
      "Sum / Difference encoded (Ls = Ls + Rs, Rs = Ls - Rs)"
    :
      "Normal (Ls = Ls, Rs = Rs)",
    param->surroundSum
  );

  if (param->syntaxCode == 0x6 || param->syntaxCode == 0x7) {
    LIBBLU_DTS_DEBUG_CORE(
      "  Dialog normalization code (DIALNORM/DNG): 0x%02" PRIX8 ".\n",
      param->dialNormCode
    );
    LIBBLU_DTS_DEBUG_CORE(
      "   => Dialog normalization gain: %d dB.\n",
      param->dialNorm
    );
  }

  return 0;
}

static bool _constantDcaCoreSSFrameHeader(
  const DcaCoreSSFrameHeaderParameters * first,
  const DcaCoreSSFrameHeaderParameters * second
)
{
  return CHECK(
    EQUAL(->terminationFrame)
    EQUAL(->samplesPerBlock)
    EQUAL(->blocksPerChannel)
    EQUAL(->crcPresent)
    EQUAL(->audioChannelArrangement)
    EQUAL(->audioFreqCode)
    EQUAL(->sourcePcmResCode)
    EQUAL(->bitRateCode)
    EQUAL(->auxData)
    EQUAL(->hdcd)
    EQUAL(->extAudio)
    EQUAL(->lfePresent)
    EQUAL(->extAudioId)
    EQUAL(->syntaxCode)
  );
}

static int _checkDcaCoreSSFrameHeaderChangeCompliance(
  const DcaCoreSSFrameHeaderParameters * old,
  const DcaCoreSSFrameHeaderParameters * cur
)
{

  /* Audio channel arragement */
  if (old->audioChannelArrangement != cur->audioChannelArrangement)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Core audio channel arrangement shall remain "
      "the same during whole bitstream."
    );

  /* Sampling frequency */
  if (old->audioFreqCode != cur->audioFreqCode)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Core audio sampling frequency shall remain "
      "the same during whole bitstream."
    );

  /* Bit depth */
  if (old->sourcePcmResCode != cur->sourcePcmResCode)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Core audio bit depth shall remain "
      "the same during whole bitstream."
    );

  /* Bit-rate */
  if (old->bitRateCode != cur->bitRateCode)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Core audio bit-rate shall remain "
      "the same during whole bitstream."
    );

  /* Low Frequency Effects flag */
  if (old->lfePresent != cur->lfePresent)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Core audio LFE configuration shall remain "
      "the same during whole bitstream."
    );

  /* Extension */
  if (old->extAudio != cur->extAudio || old->extAudioId != cur->extAudioId)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Core audio extension coding parameters shall remain "
      "the same during whole bitstream."
    );

  /* Syntax version */
  if (old->syntaxCode != cur->syntaxCode)
    LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
      "Core audio syntax version shall remain "
      "the same during whole bitstream."
    );

  return 0;
}

int checkDcaCoreSSCompliance(
  const DcaCoreSSFrameParameters * frame,
  DtsDcaCoreSSWarningFlags * warningFlags
)
{
  LIBBLU_DTS_DEBUG_CORE(" Frame header:\n");

  if (_checkDcaCoreSSFrameHeaderCompliance(&frame->header, warningFlags) < 0)
    return -1;

  LIBBLU_DTS_DEBUG_CORE(" Frame payload:\n");
  LIBBLU_DTS_DEBUG_CORE("  Size: %" PRId64 " bytes.\n", frame->payloadSize);

  return 0;
}

bool constantDcaCoreSS(
  const DcaCoreSSFrameParameters * first,
  const DcaCoreSSFrameParameters * second
)
{
  return CHECK(
    EQUAL(->payloadSize)
    SUB_FUN_PTR(->header, _constantDcaCoreSSFrameHeader)
  );
}

int checkDcaCoreSSChangeCompliance(
  const DcaCoreSSFrameParameters * old,
  const DcaCoreSSFrameParameters * cur,
  DtsDcaCoreSSWarningFlags * warning_flags
)
{
  if (checkDcaCoreSSCompliance(cur, warning_flags) < 0)
    return -1;

  return _checkDcaCoreSSFrameHeaderChangeCompliance(
    &old->header,
    &cur->header
  );
}

static int _checkDcaExtSSHeaderMixMetadataCompliance(
  const DcaExtSSHeaderMixMetadataParameters * param
)
{

  LIBBLU_DTS_DEBUG_EXTSS(
    "    Mixing Metadata Adujstement Level (nuMixMetadataAdjLevel): "
    "0x%02" PRIX8 ".\n",
    param->adjustmentLevel
  );

  LIBBLU_DTS_DEBUG_EXTSS(
    "     => %s.\n",
    dcaExtMixMetadataAjdLevelStr(param->adjustmentLevel)
  );

  if (!isValidDcaExtMixMetadataAdjLevel(param->adjustmentLevel))
    LIBBLU_DTS_ERROR_RETURN(
      "Reserved value in use (nuMixMetadataAdjLevel == 0x%02" PRIX8 ").\n",
      param->adjustmentLevel
    );

  LIBBLU_DTS_DEBUG_EXTSS(
    "    Number of Mixing Configurations (nuNumMixOutConfigs): %u (0x%X).\n",
    param->nbMixOutputConfigs,
    param->nbMixOutputConfigs - 1
  );

  LIBBLU_DTS_DEBUG_EXTSS(
    "    Mixing Configurations speaker layout for Mixer Output Channels "
    "(nuMixOutChMask[ns]):\n"
  );
  for (unsigned ns = 0; ns < param->nbMixOutputConfigs; ns++) {
    LIBBLU_DTS_DEBUG_EXTSS("     -> Configuration %u:", ns);
    if (isEnabledLibbbluStatus(LIBBLU_DEBUG_DTS_PARSING_EXTSS))
      dcaExtChMaskStrPrintFun(
        param->mixOutputChannelsMask[ns],
        lbc_deb_printf
      );
    LIBBLU_DTS_DEBUG_EXTSS_NH(
      " (%u channels, 0x%04" PRIX16 ");\n",
      param->nbMixOutputChannels[ns],
      param->mixOutputChannelsMask[ns]
    );
  }

  return 0;
}

static int _checkDcaExtSSHeaderStaticFieldsCompliance(
  const DcaExtSSHeaderStaticFieldsParameters * param,
  bool isSecondaryStream,
  unsigned nExtSSIndex,
  DtsDcaExtSSWarningFlags * warning_flags
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

  if (!isSecondaryStream) {
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

  if (!isSecondaryStream) {
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
    if (!warning_flags->nonCompleteAudioPresentationAssetType) {
      LIBBLU_INFO(
        LIBBLU_DTS_PREFIX
        "Presence of Mixing Metadata in Extension Substream.\n"
      );
      warning_flags->nonCompleteAudioPresentationAssetType = true;
    }

    LIBBLU_DTS_DEBUG_EXTSS("   Mixing Metadata:\n");

    if (_checkDcaExtSSHeaderMixMetadataCompliance(&param->mixMetadata) < 0)
      return -1;
  }

  return 0;
}

static int _checkDcaAudioAssetDescriptorStaticFieldsCompliance(
  const DcaAudioAssetDescriptorStaticFieldsParameters * param,
  bool isSecondaryStream,
  DtsDcaExtSSWarningFlags * warning_flags
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
    isSecondaryStream
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

  if (isSecondaryStream && 6 < param->nbChannels)
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
      isSecondaryStream
      && !DCA_EXT_SS_STRICT_NOT_DIRECT_SPEAKERS_FEED_REJECT
    ) {
      if (!warning_flags->nonDirectSpeakersFeedTolerance) {
        LIBBLU_WARNING(
          "Usage of a non recommended alternate channels declaration syntax "
          "in Extension Substream Asset Descriptor "
          "tolerated for Secondary audio (No direct-speakers-feeds).\n"
        );
        warning_flags->nonDirectSpeakersFeedTolerance = true;
      }
    }
    else if (
      param->nbChannels == 2
      && !DCA_EXT_SS_STRICT_NOT_DIRECT_SPEAKERS_FEED_REJECT
    ) {
      if (!warning_flags->nonDirectSpeakersFeedTolerance) {
        LIBBLU_WARNING(
          "Usage of a non recommended alternate channels declaration syntax "
          "in Extension Substream Asset Descriptor "
          "tolerated for Stereo audio (No direct-speakers-feeds).\n"
        );
        warning_flags->nonDirectSpeakersFeedTolerance = true;
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
        && !warning_flags->presenceOfStereoDownmix
      ) {
        LIBBLU_INFO(
          "Presence of an optional embedded Stereo Down-mix "
          "in Extension Substream.\n"
        );
        warning_flags->presenceOfStereoDownmix = true;
      }
    }
    else
      LIBBLU_DTS_DEBUG_EXTSS("       - Stereo downmix: Not applicable.\n");

    if (
      !param->embeddedStereoDownmix
      && isSecondaryStream
      && 2 < param->nbChannels
      && !warning_flags->absenceOfStereoDownmixForSec
    ) {
      LIBBLU_WARNING(
        "Missing recommended Stereo Down-mix for a Secondary Stream audio "
        "track with more than two channels.\n"
      );
      warning_flags->absenceOfStereoDownmixForSec = true;
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
      BOOL_PRESENCE(param->speakersMaskEnabled),
      param->speakersMaskEnabled
    );

    if (!param->speakersMaskEnabled)
      LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
        "Missing presence of loudspeakers activity layout mask in Extension "
        "Substream Asset Descriptor.\n"
      );

    if (param->speakersMaskEnabled) {
      unsigned nb_ch_from_mask = dcaExtChMaskToNbCh(param->speakersMask);

      LIBBLU_DTS_DEBUG_EXTSS(
        "      Channels loudspeakers activity layout mask "
        "(nuSpkrActivityMask): 0x%" PRIX16 ".\n",
        param->speakersMask
      );
      LIBBLU_DTS_DEBUG_EXTSS("       => Channel(s):");
      if (isEnabledLibbbluStatus(LIBBLU_DEBUG_DTS_PARSING_EXTSS))
        dcaExtChMaskStrPrintFun(
          param->speakersMask,
          lbc_deb_printf
        );
      LIBBLU_DTS_DEBUG_EXTSS_NH(
        " (%u channel(s), 0x%" PRIX16 ").\n",
        nb_ch_from_mask,
        param->speakersMask
      );

      if (nb_ch_from_mask != param->nbChannels)
        LIBBLU_DTS_ERROR_RETURN(
          "Unexpected number of channels in loudspeakers activity layout mask "
          "of Extension Substream Asset Descriptor "
          "(expect %u channels, got %u).\n",
          param->nbChannels,
          nb_ch_from_mask
        );
    }

    LIBBLU_DTS_DEBUG_EXTSS(
      "      Number of Speakers Remapping Sets (nuNumSpkrRemapSets): "
      "%u (0x%X).\n",
      param->nbOfSpeakersRemapSets,
      param->nbOfSpeakersRemapSets
    );

    if (0 < param->nbOfSpeakersRemapSets) {
      LIBBLU_DTS_DEBUG_EXTSS("      Remapping Sets:\n");

      for (unsigned ns = 0; ns < param->nbOfSpeakersRemapSets; ns++) {
        LIBBLU_DTS_DEBUG_EXTSS("       - Set %u:\n", ns);

        LIBBLU_DTS_DEBUG_EXTSS(
          "        Standard replaced Loudspeaker Layout Mask "
          "(nuStndrSpkrLayoutMask): 0x%" PRIX16 ".\n",
          param->stdSpeakersLayoutMask[ns]
        );
        LIBBLU_DTS_DEBUG_EXTSS("         => Channel(s):");
        if (isEnabledLibbbluStatus(LIBBLU_DEBUG_DTS_PARSING_EXTSS))
          dcaExtChMaskStrPrintFun(
            param->stdSpeakersLayoutMask[ns],
            lbc_deb_printf
          );
        LIBBLU_DTS_DEBUG_EXTSS_NH(
          " (%u channel(s)).\n",
          param->nbChsInRemapSet[ns]
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
          LIBBLU_DTS_DEBUG_EXTSS("         -> ");
          if (!param->nbRemapCoeffCodes[ns][nCh])
            LIBBLU_DTS_DEBUG_EXTSS_NH("Muted (0x0).\n");
          else {
            if (isEnabledLibbbluStatus(LIBBLU_DEBUG_DTS_PARSING_EXTSS))
              dcaExtChMaskStrPrintFun(
                param->decChLnkdToSetSpkrMask[ns][nCh],
                lbc_deb_printf
              );
            LIBBLU_DTS_DEBUG_EXTSS_NH(
              " (0x%" PRIX16 "): ",
              param->decChLnkdToSetSpkrMask[ns][nCh]
            );

            for (unsigned nc = 0; nc < param->nbRemapCoeffCodes[ns][nCh]; nc++) {
              if (0 < nc)
                LIBBLU_DTS_DEBUG_EXTSS_NH(", ");
              LIBBLU_DTS_DEBUG_EXTSS_NH(
                "%" PRIu8,
                param->outputSpkrRemapCoeffCodes[ns][nCh][nc]
              );
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

    if (isSecondaryStream) {
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
  const DcaAudioAssetDescriptorDynamicMetadataParameters * param,
  const DcaAudioAssetDescriptorStaticFieldsParameters * sf
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
  const DcaAudioAssetDescriptorDecoderNavDataParameters * param,
  const DcaExtSSHeaderMixMetadataParameters mix_meta,
  bool is_sec_stream
)
{

  LIBBLU_DTS_DEBUG_EXTSS(
    "     Data Coding Mode (nuCodingMode): 0x%x.\n",
    param->codingMode
  );
  LIBBLU_DTS_DEBUG_EXTSS(
    "      => %s.\n", dtsAudioAssetCodingModeStr(param->codingMode)
  );

  if (is_sec_stream) {
    if (param->codingMode != DCA_EXT_SS_CODING_MODE_DTS_HD_LOW_BITRATE)
      LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
        "Unexpected Secondary audio track coding mode. BDAV specifications "
        "require DTS-HD Low bit-rate coding mode for Extension Substream.\n"
      );
  }
  else {
    if (param->codingMode != DCA_EXT_SS_CODING_MODE_DTS_HD_COMPONENTS)
      LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(
        "Unexpected Primary audio track coding mode. BDAV specifications "
        "require DTS-HD Components coding mode with retro-compatible "
        "Core Substream.\n"
      );
  }

  LIBBLU_DTS_DEBUG_EXTSS("     Coding Mode related:\n");

  switch (param->codingMode) {
    case DCA_EXT_SS_CODING_MODE_DTS_HD_COMPONENTS:
    case DCA_EXT_SS_CODING_MODE_DTS_HD_LOSSLESS_WITHOUT_CORE:
    case DCA_EXT_SS_CODING_MODE_DTS_HD_LOW_BITRATE:
      if (param->codingMode == DCA_EXT_SS_CODING_MODE_DTS_HD_COMPONENTS) {
        LIBBLU_DTS_DEBUG_EXTSS(
          "      Coding Components Used in Asset mask "
          "(nuCoreExtensionMask): 0x%04" PRIX16 ".\n",
          param->codingComponentsUsedMask
        );
      }
      else {
        LIBBLU_DTS_DEBUG_EXTSS(
          "      Implied Coding Components Used in Asset mask: "
          "0x%04" PRIX16 ".\n",
          param->codingComponentsUsedMask
        );
      }

      if (!param->codingComponentsUsedMask)
        LIBBLU_DTS_DEBUG_EXTSS("       => *Empty*\n");

      if (
        param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_CORE_DCA
      ) {
        LIBBLU_DTS_DEBUG_EXTSS(
          "       => DCA Core Component within Core Substream "
          "(0x%04" PRIX16 ").\n",
          DCA_EXT_SS_COD_COMP_CORE_DCA
        );
      }
      if (
        param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_CORE_EXT_XXCH
      ) {
        LIBBLU_DTS_DEBUG_EXTSS(
          "       => DCA XXCH 5.1+ Channels Extension within "
          "Core Substream (0x%04" PRIX16 ").\n",
          DCA_EXT_SS_COD_COMP_CORE_EXT_XXCH
        );
      }
      if (
        param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_CORE_EXT_X96
      ) {
        LIBBLU_DTS_DEBUG_EXTSS(
          "       => DCA X96 96kHz Sampling Frequency Extension within "
          "Core Substream (0x%04" PRIX16 ").\n",
          DCA_EXT_SS_COD_COMP_CORE_EXT_X96
        );
      }
      if (
        param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_CORE_EXT_XCH
      ) {
        LIBBLU_DTS_DEBUG_EXTSS(
          "       => DCA XCH 6.1 Channels Extension within "
          "Core Substream (0x%04" PRIX16 ").\n",
          DCA_EXT_SS_COD_COMP_CORE_EXT_XCH
        );
      }

      if (
        param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_CORE_DCA
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
        param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_XBR
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
        param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_XXCH
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
        param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_X96
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
        param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_LBR
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
        param->codingComponentsUsedMask & DCA_EXT_SS_COD_COMP_EXTSUB_XLL
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
      for (unsigned ns = 0; ns < mix_meta.nbMixOutputConfigs; ns++) {
        LIBBLU_DTS_DEBUG_EXTSS("       - Config %u: ", ns);
        if (param->mixMetadata.perChannelMainAudioScaleCode) {
          for (unsigned nCh = 0; nCh < mix_meta.nbMixOutputChannels[ns]; nCh++) {
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
  const DcaAudioAssetDescriptorParameters * param,
  bool is_sec_stream,
  const DcaExtSSHeaderMixMetadataParameters mix_meta,
  DtsDcaExtSSWarningFlags * warning_flags
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
      warning_flags
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
    param->reservedFieldLength
  );
  LIBBLU_DTS_DEBUG_EXTSS(
    "    Byte boundary padding field "
    "(ZeroPadForFsize): %" PRId64 " bit(s).\n",
    param->paddingBits
  );

  if (
    DCA_EXT_SS_IS_SUPP_RES_FIELD_SIZES(
      param->reservedFieldLength, param->paddingBits
    )
  ) {
    LIBBLU_DTS_DEBUG_EXTSS(
      "     -> Content:"
    );

    int64_t size = param->reservedFieldLength + (param->paddingBits + 7) / 8;
    for (int64_t i = 0; i < size; i++)
      LIBBLU_DTS_DEBUG_EXTSS_NH(" 0x%" PRIX8, param->reservedFieldData[i]);
    LIBBLU_DTS_DEBUG_EXTSS_NH(".\n");
  }

  return 0;
}

int checkDcaExtSSHeaderCompliance(
  const DcaExtSSHeaderParameters * param,
  bool is_sec_stream,
  DtsDcaExtSSWarningFlags * warning_flags
)
{

  LIBBLU_DTS_DEBUG_EXTSS(" Extension Substream Header:\n");

  LIBBLU_DTS_DEBUG_EXTSS(
    "  Sync word (SYNCEXTSSH): 0x%08" PRIX32 ".\n",
    DTS_SYNCWORD_SUBSTREAM
  );

  LIBBLU_DTS_DEBUG_EXTSS(
    "  User defined bits (UserDefinedBits): 0x%02" PRIX8 ".\n",
    param->userDefinedBits
  );

  if (param->userDefinedBits && !warning_flags->presenceOfUserDefinedBits) {
    LIBBLU_INFO(LIBBLU_DTS_PREFIX "Presence of an user-data byte.\n");
    warning_flags->presenceOfUserDefinedBits = true;
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
      warning_flags
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
      warning_flags
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
    param->reservedFieldLength
  );

  LIBBLU_DTS_DEBUG_EXTSS(
    "  Byte boundary padding field (ByteAlign): %" PRId64 " bit(s).\n",
    param->paddingBits
  );

  if (
    DCA_EXT_SS_IS_SUPP_RES_FIELD_SIZES(
      param->reservedFieldLength, param->paddingBits
    )
  ) {
    LIBBLU_DTS_DEBUG_EXTSS(
      "   -> Content:"
    );

    int64_t size = param->reservedFieldLength + (param->paddingBits + 7) / 8;
    for (int64_t i = 0; i < size; i++)
      LIBBLU_DTS_DEBUG_EXTSS_NH(" 0x%02" PRIX8, param->reservedFieldData[i]);
    LIBBLU_DTS_DEBUG_EXTSS_NH(".\n");
  }

  LIBBLU_DTS_DEBUG_EXTSS(
    "  Extension Substream Header CRC16 checksum (nCRC16ExtSSHeader): "
    "0x%04" PRIX16 ".\n",
    param->crcValue
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
