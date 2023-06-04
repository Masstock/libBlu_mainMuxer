#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "ac3_core_check.h"


int checkAc3SyncInfoCompliance(
  const Ac3SyncInfoParameters * param
)
{
  assert(NULL != param);

  LIBBLU_AC3_DEBUG(
    "  CRC1 (crc1): 0x%" PRIX16 " (Checked after parsing).\n",
    param->crc1
  );

  LIBBLU_AC3_DEBUG(
    "  Sample Rate Code (fscod): 0x%" PRIx8 ".\n",
    param->fscod
  );
  LIBBLU_AC3_DEBUG(
    "   -> %s.\n",
    Ac3FscodStr(param->fscod, false)
  );

  if (EAC3_FSCOD_32000_HZ < param->fscod)
    LIBBLU_AC3_ERROR_RETURN(
      "Reserved value in use (fscod == 0x%" PRIX8 ").\n",
      param->fscod
    );

  LIBBLU_AC3_DEBUG(
    "  Frame Size Code (frmsizecod): %u (0x%02" PRIX8 ").\n",
    param->frmsizecod,
    param->frmsizecod
  );

  unsigned bitrate = norminalBitrate_frmsizecod(param->frmsizecod);
  if (0 == bitrate)
    LIBBLU_AC3_ERROR_RETURN(
      "Reserved value in use (frmsizecod == 0x%" PRIX8 ").\n",
      param->frmsizecod
    );

  LIBBLU_AC3_DEBUG(
    "   -> %u words, %u bytes, %u kbps.\n",
    framesize_frmsizecod(param->frmsizecod, param->fscod),
    framesize_frmsizecod(param->frmsizecod, param->fscod) << 1,
    bitrate
  );

  return 0;
}


int checkAc3AddbsiCompliance(
  const Ac3Addbsi * param
)
{

  LIBBLU_AC3_DEBUG(
    "   Additional Bit Stream Information Length (addbsil): "
    "%" PRIu8 " bits (0x%02" PRIX8 ").\n",
    param->addbsil + 1,
    param->addbsil
  );

  if (63 < param->addbsil)
    LIBBLU_AC3_ERROR_RETURN(
      "Additional Bit Stream Information Length value is out of range 0-63 "
      "('addbsil' == %" PRIu8 ").\n",
      param->addbsil
    );

  switch (param->type) {
    case AC3_ADDBSI_UNK:
      LIBBLU_AC3_DEBUG(
        "    Unknown extension data.\n"
      );
      break;

    case AC3_ADDBSI_EC3_EXT_TYPE_A:
      LIBBLU_AC3_DEBUG(
        "    Extension data matches EC3 Extension data (Dolby Atmos).\n"
      );

      LIBBLU_AC3_DEBUG(
        "    flag_ec3_extension_type_a: 0x1 (used for identification).\n"
      );
      LIBBLU_AC3_DEBUG(
        "    Objects Audio complexity (complexity_index_type_a): %u (0x%x).\n",
        param->ec3ExtTypeA.complexityIndex,
        param->ec3ExtTypeA.complexityIndex
      );
      LIBBLU_AC3_DEBUG(
        "     -> %u Audio Object(s).\n",
        param->ec3ExtTypeA.complexityIndex
      );
  }

  return 0;
}


int checkAc3BitStreamInfoCompliance(
  const Ac3BitStreamInfoParameters * param
)
{
  LIBBLU_AC3_DEBUG(
    " Bit Stream Information, bsi()\n"
  );

  LIBBLU_AC3_DEBUG(
    "  Bit Stream Identification (bsid): %" PRIu8 " (0x%02" PRIx8 ").\n",
    param->bsid, param->bsid
  );

  LIBBLU_AC3_DEBUG(
    "  Bit Stream Mode (bsmod): 0x%02" PRIX8 ".\n",
    param->bsmod
  );
  LIBBLU_AC3_DEBUG(
    "   -> %s.\n",
    Ac3BsmodStr(param->bsmod, param->acmod)
  );

  LIBBLU_AC3_DEBUG(
    "  Audio Coding Mode (acmod): 0x%02" PRIX8 ".\n",
    param->acmod
  );

  if (param->acmod == AC3_ACMOD_CH1_CH2) {
    LIBBLU_DEBUG_COM("   => Dual-Mono second channel parameters present.\n");

    LIBBLU_EAC3_COMPLIANCE_ERROR_RETURN(
      "Invalid dual mono audio coding mode (acmod == 0x0).\n"
    );
  }
  LIBBLU_AC3_DEBUG(
    "   -> The number of channels is specified with the field 'lfeon'.\n"
  );

  if (threeFrontChannelsPresentAc3Acmod(param->acmod)) {
    LIBBLU_AC3_DEBUG(
      "  Center Mix Level (cmixlev): 0x%02" PRIX8 ".\n",
      param->cmixlev
    );
    LIBBLU_AC3_DEBUG(
      "   -> %s.\n",
      Ac3CmixlevStr(param->cmixlev)
    );

    if (AC3_CMIXLEV_RES == param->cmixlev)
      LIBBLU_AC3_ERROR_RETURN(
        "Reserved value in use (cmixlev == 0x%" PRIX8 ").\n",
        param->cmixlev
      );
  }

  if (isSurroundAc3Acmod(param->acmod)) {
    LIBBLU_AC3_DEBUG(
      "  Surround Mix Level (surmixlev): 0x%02" PRIX8 ".\n",
      param->surmixlev
    );
    LIBBLU_AC3_DEBUG(
      "   -> %s.\n",
      Ac3SurmixlevStr(param->surmixlev)
    );

    if (AC3_SURMIXLEV_RES == param->surmixlev)
      LIBBLU_AC3_ERROR_RETURN(
        "Reserved value in use (surmixlev == 0x%" PRIX8 ").\n",
        param->surmixlev
      );
  }

  if (param->acmod == AC3_ACMOD_L_R) {
    LIBBLU_AC3_DEBUG(
      "  Dolby Surround Mode (dsurmod): 0x%02" PRIX8 ".\n",
      param->dsurmod
    );
    LIBBLU_AC3_DEBUG(
      "   -> %s.\n",
      Ac3DsurmodStr(param->dsurmod)
    );

    if (AC3_DSURMOD_RES == param->dsurmod)
      LIBBLU_AC3_WARNING(
        "Non-fatal reserved value in use (dsurmod == 0x%" PRIX8 ", "
        "interpreted as 0x0, 'not indicated').\n",
        param->dsurmod
      );
  }

  LIBBLU_AC3_DEBUG(
    "  Low Frequency Effects (LFE) channel (lfeon): 0x%02" PRIX8 ".\n",
    param->lfeon
  );
  LIBBLU_AC3_DEBUG(
    "   -> %s.\n",
    BOOL_PRESENCE(param->lfeon)
  );
  LIBBLU_AC3_DEBUG(
    "   => %s (%u channels).\n",
    Ac3AcmodStr(param->acmod, param->lfeon),
    param->nbChannels
  );

  LIBBLU_AC3_DEBUG(
    "  Dialogue Normalization (dialnorm): 0x%02" PRIX8 ".\n",
    param->dialnorm
  );
  LIBBLU_AC3_DEBUG(
    "   -> -%" PRIu8 " dB.\n",
    param->dialnorm
  );

  /* TODO: Is fatal ? */
  if (!param->dialnorm)
    LIBBLU_AC3_WARNING(
      "Non-fatal reserved value in use (dialnorm == 0x%" PRIX8 ").\n",
      param->dialnorm
    );

  LIBBLU_AC3_DEBUG(
    "  Compression Gain Word Exists (compre): %s (0b%x).\n",
    BOOL_STR(param->compre),
    param->compre
  );
  if (param->compre) {
    LIBBLU_AC3_DEBUG(
      "   Compression Gain Word (compr): 0x%02" PRIX8 ".\n",
      param->compr
    );
  }

  LIBBLU_AC3_DEBUG(
    "  *Deprecated* Language Code Exists (langcode): %s (0b%x).\n",
    BOOL_STR(param->langcode),
    param->langcode
  );
  if (param->langcode) {
    LIBBLU_AC3_DEBUG(
      "   *Deprecated* Language Code (langcod): 0x%02" PRIX8 ".\n",
      param->langcod
    );

    if (0xFF != param->langcod)
      LIBBLU_AC3_WARNING(
        "Deprecated 'langcod' field is used and is not set to 0xFF "
        "(0x02" PRIX8 ").\n",
        param->langcod
      );
  }

  LIBBLU_AC3_DEBUG(
    "  Audio Production Information Exists (audprodie): %s (0b%x).\n",
    BOOL_STR(param->audprodie),
    param->audprodie
  );
  if (param->audprodie) {
    LIBBLU_AC3_DEBUG(
      "   Mixing Level (mixlevel): %" PRIu8 " (0x%02" PRIX8 ").\n",
      param->audprodi.mixlevel,
      param->audprodi.mixlevel
    );
    LIBBLU_AC3_DEBUG(
      "    -> %u dB SPL.\n",
      80u + param->audprodi.mixlevel
    );

    LIBBLU_AC3_DEBUG(
      "   Room Type (roomtyp): %" PRIu8 " (0x%02" PRIX8 ").\n",
      param->audprodi.roomtyp,
      param->audprodi.roomtyp
    );
    LIBBLU_AC3_DEBUG(
      "    -> %s.\n",
      Ac3RoomtypStr(param->audprodi.roomtyp)
    );

    if (AC3_ROOMTYP_RES == param->audprodi.roomtyp)
      LIBBLU_AC3_ERROR_RETURN(
        "Reserved value in use (roomtyp == 0x%" PRIX8 ").\n",
        param->audprodi.roomtyp
      );
  }

  if (param->acmod == AC3_ACMOD_CH1_CH2) {
    /* Dual Mono */
    LIBBLU_AC3_DEBUG(
      "  Dual-Mono (acmod == 0x00) specific parameters:\n"
    );

    LIBBLU_AC3_DEBUG(
      "   Dialogue Normalization for ch2 (dialnorm2): 0x%02" PRIX8 ".\n",
      param->dual_mono.dialnorm2
    );
    LIBBLU_AC3_DEBUG(
      "    -> -%" PRIu8 " dB.\n",
      param->dual_mono.dialnorm2
    );

    /* TODO: Is fatal ? */
    if (!param->dual_mono.dialnorm2)
      LIBBLU_AC3_WARNING(
        "Non-fatal reserved value in use (dialnorm2 == 0x%" PRIX8 ").\n",
        param->dual_mono.dialnorm2
      );

    LIBBLU_AC3_DEBUG(
      "   Compression Gain Word Exists for ch2 (compr2e): %s (0b%x).\n",
      BOOL_STR(param->dual_mono.compr2e),
      param->dual_mono.compr2e
    );
    if (param->dual_mono.compr2e) {
      LIBBLU_AC3_DEBUG(
        "     Compression Gain Word for ch2 (compr2): 0x%02" PRIX8 ".\n",
        param->dual_mono.compr2
      );
    }

    LIBBLU_AC3_DEBUG(
      "   *Deprecated* Language Code Exists for ch2 (langcod2e): %s (0b%x).\n",
      BOOL_STR(param->dual_mono.langcod2e),
      param->dual_mono.langcod2e
    );
    if (param->dual_mono.langcod2e) {
      LIBBLU_AC3_DEBUG(
        "    *Deprecated* Language Code for ch2 (langcod2): 0x%02" PRIX8 ".\n",
        param->dual_mono.langcod2
      );

      if (0xFF != param->dual_mono.langcod2)
        LIBBLU_AC3_WARNING(
          "Deprecated 'langcod2' field is used and is not set to 0xFF "
          "(0x02" PRIX8 ").\n",
          param->dual_mono.langcod2
        );
    }

    LIBBLU_AC3_DEBUG(
      "   Audio Production Information Exists for ch2 (audprodi2e): %s (0b%x).\n",
      BOOL_STR(param->dual_mono.audprodi2e),
      param->dual_mono.audprodi2e
    );
    if (param->dual_mono.audprodi2e) {
      LIBBLU_AC3_DEBUG(
        "    Mixing Level for ch2 (mixlevel2): %" PRIu8 " (0x%02" PRIX8 ").\n",
        param->dual_mono.audprodi2.mixlevel2,
        param->dual_mono.audprodi2.mixlevel2
      );
      LIBBLU_AC3_DEBUG(
        "     -> %u dB SPL.\n",
        80u + param->dual_mono.audprodi2.mixlevel2
      );

      LIBBLU_AC3_DEBUG(
        "    Room Type for ch2 (roomtyp2): %" PRIu8 " (0x%02" PRIX8 ").\n",
        param->dual_mono.audprodi2.roomtyp2,
        param->dual_mono.audprodi2.roomtyp2
      );
      LIBBLU_AC3_DEBUG(
        "     -> %s.\n",
        Ac3RoomtypStr(param->dual_mono.audprodi2.roomtyp2)
      );

      if (AC3_ROOMTYP_RES == param->dual_mono.audprodi2.roomtyp2)
        LIBBLU_AC3_ERROR_RETURN(
          "Reserved value in use (roomtyp2 == 0x%" PRIX8 ").\n",
          param->dual_mono.audprodi2.roomtyp2
        );
    }
  }

  LIBBLU_AC3_DEBUG(
    "  Copyrighted stream (copyrightb): %s (0b%x).\n",
    (param->copyrightb) ? "Protected" : "Not indicated",
    param->copyrightb
  );

  LIBBLU_AC3_DEBUG(
    "  Original Bit Stream (origbs): %s (0b%x).\n",
    (param->origbs) ? "Original" : "Copy of another bitsream",
    param->origbs
  );

  if (param->bsid == 6) {
    /* Alternate Bit Stream Syntax */
    /* ETSI TS 102 366 V1.4.1 - Annex D */
    if (checkAc3AlternateBitStreamInfoCompliance(param) < 0)
      return -1;
  }
  else {
    /* Legacy syntax */
    LIBBLU_AC3_DEBUG(
      "  Time Code first Half Exist (timecod1e): %s (0b%x).\n",
      BOOL_STR(param->timecod1e),
      param->timecod1e
    );

    if (param->timecod1e)
      LIBBLU_AC3_DEBUG(
        "   Time Code first Half (timecod1): 0x%04" PRIX16 ".\n",
        param->timecod1
      );

    LIBBLU_AC3_DEBUG(
      "  Time Code second Half Exist (timecod2e): %s (0b%x).\n",
      BOOL_STR(param->timecod2e),
      param->timecod2e
    );

    if (param->timecod2e)
      LIBBLU_AC3_DEBUG(
        "   Time Code second Half (timecod2): 0x%04" PRIX16 ".\n",
        param->timecod2
      );

    if (param->timecod1e || param->timecod2e) {
      unsigned hours   =  param->timecod1 >> 9;
      unsigned minutes = (param->timecod1 >> 3) & 0x3F;
      unsigned seconds = (
        ((param->timecod1 & 0x7) << 3)
        + (param->timecod2 >> 11)
      );
      unsigned frames  = (param->timecod1 >> 6) & 0x1F;
      unsigned samples = (param->timecod1 & 0x3F);

      LIBBLU_DEBUG_COM(
        "   => %02u:%02u:%02u %02u+(%02u/64) frame(s).\n",
        hours, minutes, seconds, frames, samples
      );

      if (23 < hours)
        LIBBLU_AC3_ERROR_RETURN(
          "Timecode hours value out of the 0-23 range "
          "(timecod1[0..4 MSB] == %u).\n",
          hours
        );

      if (59 < minutes)
        LIBBLU_AC3_ERROR_RETURN(
          "Timecode minutes value out of the 0-59 range "
          "(timecod1[5..10 MSB] == %u).\n",
          minutes
        );

      if (29 < frames)
        LIBBLU_AC3_ERROR_RETURN(
          "Timecode frames nb value out of the 0-29 range "
          "(timecod2[3..7 MSB] == %u).\n",
          frames
        );
    }
  }

  LIBBLU_AC3_DEBUG(
    "  Additional Bit Stream Information Exists (addbsie): %s (0b%x).\n",
    BOOL_STR(param->addbsie),
    param->addbsie
  );

  if (param->addbsie) {
    if (checkAc3AddbsiCompliance(&param->addbsi) < 0)
      return -1;
  }

  return 0;
}


int checkAc3AlternateBitStreamInfoCompliance(
  const Ac3BitStreamInfoParameters * param
)
{

  LIBBLU_AC3_DEBUG(
    "  Extra Bit Stream Information #1 Exists (xbsi1e): %s (0b%x).\n",
    BOOL_STR(param->xbsi1e),
    param->xbsi1e
  );

  if (param->xbsi1e) {
    /* Extra Bit Stream Information #1 */
    const Ac3ExtraBitStreamInfo1 * extraParam = &param->xbsi1;

    LIBBLU_AC3_DEBUG(
      "   Preferred Stereo Downmix Mode (dmixmod): 0x%x.\n",
      extraParam->dmixmod
    );
    LIBBLU_AC3_DEBUG(
      "    -> %s.\n",
      Ac3DmixmodStr(extraParam->dmixmod)
    );

    LIBBLU_AC3_DEBUG(
      "   Lt/Rt Center Mix Level (ltrtcmixlev): 0x%x.\n",
      extraParam->ltrtcmixlev
    );
    LIBBLU_AC3_DEBUG(
      "    -> %s.\n",
      Ac3LtrtcmixlevStr(extraParam->ltrtcmixlev)
    );

    LIBBLU_AC3_DEBUG(
      "   Lt/Rt Surround Mix Level (ltrtsurmixlev): 0x%x.\n",
      extraParam->ltrtsurmixlev
    );
    LIBBLU_AC3_DEBUG(
      "    -> Surround mixing level coefficient (slev): %s.\n",
      Ac3LtrtsurmixlevStr(extraParam->ltrtsurmixlev)
    );

    LIBBLU_AC3_DEBUG(
      "   Lo/Ro Center Mix Level (lorocmixlev): 0x%x.\n",
      extraParam->lorocmixlev
    );
    LIBBLU_AC3_DEBUG(
      "    -> %s.\n",
      Ac3lLorocmixlevStr(extraParam->lorocmixlev)
    );

    LIBBLU_AC3_DEBUG(
      "   Lt/Rt Surround Mix Level (ltrtsurmixlev): 0x%x.\n",
      extraParam->ltrtsurmixlev
    );
    LIBBLU_AC3_DEBUG(
      "    -> Surround mixing level coefficient (slev): %s.\n",
      Ac3LtrtsurmixlevStr(extraParam->ltrtsurmixlev)
    );

    if (extraParam->ltrtsurmixlev <= AC3_LTRTSURMIXLEV_RES_2)
      LIBBLU_AC3_WARNING(
        "Non-fatal reserved value in use (ltrtsurmixlev == 0x%" PRIX8 ", "
        "interpreted as 0x3, '0.841 (-1.5 dB)').\n",
        extraParam->ltrtsurmixlev
      );
  }

  LIBBLU_AC3_DEBUG(
    "  Extra Bit Stream Information #2 Exists (xbsi2e): %s (0b%x).\n",
    BOOL_STR(param->xbsi2e),
    param->xbsi2e
  );

  if (param->xbsi2e) {
    /* Extra Bit Stream Information #2 */
    const Ac3ExtraBitStreamInfo2 * extraParam = &param->xbsi2;

    LIBBLU_AC3_DEBUG(
      "   Dolby Surround EX Mode (dsurexmod): 0x%X.\n",
      extraParam->dsurexmod
    );
    LIBBLU_AC3_DEBUG(
      "    -> %s.\n",
      Ac3DsurexmodStr(extraParam->dsurexmod)
    );

    LIBBLU_AC3_DEBUG(
      "   Dolby Headphone Mode (dheadphonmod): 0x%X.\n",
      extraParam->dheadphonmod
    );
    LIBBLU_AC3_DEBUG(
      "    -> %s.\n",
      Ac3DheadphonmodStr(extraParam->dheadphonmod)
    );

    if (AC3_DHEADPHONMOD_RES == extraParam->dheadphonmod)
      LIBBLU_AC3_WARNING(
        "Non-fatal reserved value in use (dheadphonmod == 0x%" PRIX8 ", "
        "interpreted as 0x0, 'not indicated').\n",
        extraParam->dheadphonmod
      );

    LIBBLU_AC3_DEBUG(
      "   A/D Converter Type (adconvtyp): %s (0b%x).\n",
      BOOL_STR(extraParam->adconvtyp),
      extraParam->adconvtyp
    );
    LIBBLU_AC3_DEBUG(
      "    -> %s.\n",
      (extraParam->adconvtyp) ? "HDCD" : "Standard"
    );

    LIBBLU_DEBUG_COM(
      "   Extra Bit Stream Information "
      "(xbsi2): %" PRIu8 " (0x%02" PRIX16 ").\n",
      extraParam->xbsi2,
      extraParam->xbsi2
    );

    LIBBLU_DEBUG_COM(
      "   Encoder Information boolean (Meaning reserved to the encoder) "
      "(encinfo): %s (0b%x).\n",
      (extraParam->encinfo) ? "True" : "False",
      extraParam->encinfo
    );
  }

  return 0;
}
