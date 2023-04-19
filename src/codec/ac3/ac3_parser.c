#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "ac3_parser.h"

#define USE_CRC  false
#define TRUE_HD_SHOW_DETAILS_DISABLED_PARAM  false

static size_t _frmsizecodToNominalBitrate(
  uint8_t code
)
{
  static const size_t frmSizeCodes[] = {
     32,  40,  48,  56,  64,
     80,  96, 112, 128, 160,
    192, 224, 256, 320, 384,
    448, 512, 576, 640
  };

  if (code < ARRAY_SIZE(frmSizeCodes) * 2)
    return frmSizeCodes[code / 2];
  return 0;
}

static uint8_t _getSyncFrameBsid(
  BitstreamReaderPtr ac3Input
)
{
  uint64_t value = nextUint64(ac3Input);

  /* [v40 *variousBits*] [u5 bsid] [v19 *variousBits*] */
  return (value >> 19) & 0x1F;
}

static int _checkAc3SyncInfoCompliance(
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

  if (0x0 == param->sampleRate)
    LIBBLU_AC3_ERROR_RETURN(
      "Reserved value in use (fscod == 0x%" PRIX8 ").\n",
      param->fscod
    );

  LIBBLU_AC3_DEBUG(
    "  Frame Size Code (frmsizecod): "
    "%zu words (%zu bytes, %zu kbps, 0x%" PRIx8 ").\n",
    param->frameSize / AC3_WORD_SIZE,
    param->frameSize,
    param->bitrate,
    param->frmsizecod
  );

  if (0x0 == param->bitrate)
    LIBBLU_AC3_ERROR_RETURN(
      "Reserved value in use (frmsizecod == 0x%" PRIX8 ").\n",
      param->frmsizecod
    );

  return 0;
}

static bool _constantAc3SyncInfoCheck(
  const Ac3SyncInfoParameters * first,
  const Ac3SyncInfoParameters * second
)
{
  return CHECK(
    EQUAL(->fscod)
    EQUAL(->frmsizecod)
  );
}

static int _decodeAc3SyncInfo(
  BitstreamReaderPtr ac3Input,
  Ac3SyncFrameParameters * param,
  bool useCrcCheck
)
{
  /* For (bsid <= 0x8) syntax */
  Ac3SyncInfoParameters syncInfoParam;
  uint32_t value;

  LIBBLU_AC3_DEBUG(" Synchronization Information, syncinfo()\n");

  /* [v16 syncword] // 0x0B77 */
  if (readValueBigEndian(ac3Input, 2, &value) < 0)
    return -1;
  LIBBLU_AC3_DEBUG(
    "  Sync Word (syncword): 0x%04" PRIX32 ".\n",
    value
  );

  if (AC3_SYNCWORD != value)
    LIBBLU_AC3_ERROR_RETURN(
      "Unexpected 'syncword' value 0x%04X, expect 0x0B77.\n",
      value
    );

  /* Init the CRC check if requested */
  if (useCrcCheck) {
    if (initCrc(BITSTREAM_CRC_CTX(ac3Input), AC3_CRC_PARAMS, 0) < 0)
      return -1;
  }

  /* [v16 crc1] */
  if (readValueBigEndian(ac3Input, 2, &value) < 0)
    return -1;
  syncInfoParam.crc1 = value;

  /* [u2 fscod] */
  if (readBits(ac3Input, &value, 2) < 0)
    return -1;
  syncInfoParam.fscod = value;

  /* [u6 frmsizecod] */
  if (readBits(ac3Input, &value, 6) < 0)
    return -1;
  syncInfoParam.frmsizecod = value;

  syncInfoParam.sampleRate = sampleRateAc3Fscod(syncInfoParam.fscod);
  size_t bitrate = _frmsizecodToNominalBitrate(syncInfoParam.frmsizecod);
  syncInfoParam.bitrate = bitrate;
  syncInfoParam.frameSize = bitrate * 2 * AC3_WORD_SIZE;

  if (!param->checkMode) {
    /* Compliance checks */
    if (_checkAc3SyncInfoCompliance(&syncInfoParam) < 0)
      return -1;

    if (48000 != syncInfoParam.sampleRate)
      LIBBLU_AC3_ERROR_RETURN(
        "Forbidden sample rate value %u Hz, expect 48000 Hz.\n",
        syncInfoParam.sampleRate
      );

    if (syncInfoParam.bitrate < BDAV_AC3_MINIMAL_BITRATE)
      LIBBLU_AC3_ERROR_RETURN(
        "Bit-rate is lower than 96 kbps, %u kbps.\n",
        syncInfoParam.bitrate
      );

    param->syncInfo = syncInfoParam;
  }
  else {
    /* Consistency check */
    if (!_constantAc3SyncInfoCheck(&syncInfoParam, &param->syncInfo))
      LIBBLU_AC3_ERROR_RETURN("syncinfo() values are not constant.\n");
  }

  return 0;
}

static int _decodeEac3SyncInfo(
  BitstreamReaderPtr ac3Input
)
{
  /* For (bsid == 16) syntax */
  uint32_t value;

  LIBBLU_AC3_DEBUG(" Synchronization Information, syncinfo()\n");

  /* [v16 syncword] // 0x0B77 */
  if (readValueBigEndian(ac3Input, 2, &value) < 0)
    return -1;

  LIBBLU_AC3_DEBUG("  Sync Word (syncword): 0x%04" PRIX16 ".\n", value);

  if (AC3_SYNCWORD != value)
    LIBBLU_AC3_ERROR_RETURN(
      "Unexpected 'syncword' value 0x%04X, expect 0x0B77.\n",
      value
    );

  return 0;
}

static int _decodeAc3Addbsi(
  BitstreamReaderPtr ac3Input,
  Ac3Addbsi * param
)
{
  uint32_t value;

  /* [u6 addbsil] */
  if (readBits(ac3Input, &value, 6) < 0)
    return -1;
  param->addbsil = value;

  if (param->addbsil == 1) {
    /* Assuming presence of EC3 Extension      */
    /* Reference: ETSI TS 103 420 V1.2.1 - 8.3 */

    /* Try [v7 reserved] [b1 flag_ec3_extension_type_a] ... */
    if (readBits(ac3Input, &value, 16) < 0)
      return -1;

    if ((value >> 8) == 0x1) {
      /* If 'reserved' == 0x0 && 'flag_ec3_extension_type_a' == 0x1 */
      param->type = AC3_ADDBSI_EC3_EXT_TYPE_A;

      /* [u8 complexity_index_type_a] */
      param->ec3ExtTypeA.complexityIndex = value & 0xFF;
    }
    /* else Unknown extension. */
  }
  else {
    /* Unknown extension. */
    /* [v<(addbsil + 1) * 8> addbsi] */
    if (skipBits(ac3Input, (value + 1) * 8) < 0)
      return -1;
  }

  return 0;
}

static int _checkAc3AddbsiCompliance(
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

static int _checkAc3AlternateBitStreamInfoCompliance(
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

    if (AC3_DMIXMOD_RES == extraParam->dmixmod)
      LIBBLU_AC3_WARNING(
        "Non-fatal reserved value in use (dmixmod == 0x%" PRIX8 ", "
        "interpreted as 0x0, 'Not indicated').\n",
        extraParam->dmixmod
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
      (extraParam->adconvtyp) ? "Standard" : "HDCD"
    );

    LIBBLU_AC3_DEBUG(
      "   A/D Converter Type (adconvtyp): %s (0b%x).\n",
      BOOL_STR(extraParam->adconvtyp),
      extraParam->adconvtyp
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

  if (param->acmod == AC3_ACMOD_CH1_CH2)
    LIBBLU_DEBUG_COM("   => Dual-Mono second channel parameters present.\n");
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
      param->audioProdInfo.mixlevel,
      param->audioProdInfo.mixlevel
    );
    LIBBLU_AC3_DEBUG(
      "    -> %u dB SPL.\n",
      80 + (unsigned) param->audioProdInfo.mixlevel
    );

    LIBBLU_AC3_DEBUG(
      "   Room Type (roomtyp): %" PRIu8 " (0x%02" PRIX8 ").\n",
      param->audioProdInfo.roomtyp,
      param->audioProdInfo.roomtyp
    );
    LIBBLU_AC3_DEBUG(
      "    -> %s.\n",
      Ac3RoomtypStr(param->audioProdInfo.roomtyp)
    );

    if (AC3_ROOMTYP_RES == param->audioProdInfo.roomtyp)
      LIBBLU_AC3_ERROR_RETURN(
        "Reserved value in use (roomtyp == 0x%" PRIX8 ").\n",
        param->audioProdInfo.roomtyp
      );
  }

  if (param->acmod == AC3_ACMOD_CH1_CH2) {
    /* Dual Mono */
    LIBBLU_AC3_DEBUG(
      "  Dual-Mono (acmod == 0x00) specific parameters:\n"
    );

    LIBBLU_AC3_DEBUG(
      "   Dialogue Normalization for ch2 (dialnorm2): 0x%02" PRIX8 ".\n",
      param->dualMonoParam.dialnorm2
    );
    LIBBLU_AC3_DEBUG(
      "    -> -%" PRIu8 " dB.\n",
      param->dualMonoParam.dialnorm2
    );

    /* TODO: Is fatal ? */
    if (!param->dualMonoParam.dialnorm2)
      LIBBLU_AC3_WARNING(
        "Non-fatal reserved value in use (dialnorm2 == 0x%" PRIX8 ").\n",
        param->dualMonoParam.dialnorm2
      );

    LIBBLU_AC3_DEBUG(
      "   Compression Gain Word Exists for ch2 (compr2e): %s (0b%x).\n",
      BOOL_STR(param->dualMonoParam.compr2e),
      param->dualMonoParam.compr2e
    );
    if (param->dualMonoParam.compr2e) {
      LIBBLU_AC3_DEBUG(
        "     Compression Gain Word for ch2 (compr2): 0x%02" PRIX8 ".\n",
        param->dualMonoParam.compr2
      );
    }

    LIBBLU_AC3_DEBUG(
      "   *Deprecated* Language Code Exists for ch2 (langcod2e): %s (0b%x).\n",
      BOOL_STR(param->dualMonoParam.langcod2e),
      param->dualMonoParam.langcod2e
    );
    if (param->dualMonoParam.langcod2e) {
      LIBBLU_AC3_DEBUG(
        "    *Deprecated* Language Code for ch2 (langcod2): 0x%02" PRIX8 ".\n",
        param->dualMonoParam.langcod2
      );

      if (0xFF != param->dualMonoParam.langcod2)
        LIBBLU_AC3_WARNING(
          "Deprecated 'langcod2' field is used and is not set to 0xFF "
          "(0x02" PRIX8 ").\n",
          param->dualMonoParam.langcod2
        );
    }

    LIBBLU_AC3_DEBUG(
      "   Audio Production Information Exists for ch2 (audprodi2e): %s (0b%x).\n",
      BOOL_STR(param->dualMonoParam.audprodi2e),
      param->dualMonoParam.audprodi2e
    );
    if (param->dualMonoParam.audprodi2e) {
      LIBBLU_AC3_DEBUG(
        "    Mixing Level for ch2 (mixlevel2): %" PRIu8 " (0x%02" PRIX8 ").\n",
        param->dualMonoParam.audioProdInfo.mixlevel2,
        param->dualMonoParam.audioProdInfo.mixlevel2
      );
      LIBBLU_AC3_DEBUG(
        "     -> %u dB SPL.\n",
        80 + (unsigned) param->dualMonoParam.audioProdInfo.mixlevel2
      );

      LIBBLU_AC3_DEBUG(
        "    Room Type for ch2 (roomtyp2): %" PRIu8 " (0x%02" PRIX8 ").\n",
        param->dualMonoParam.audioProdInfo.roomtyp2,
        param->dualMonoParam.audioProdInfo.roomtyp2
      );
      LIBBLU_AC3_DEBUG(
        "     -> %s.\n",
        Ac3RoomtypStr(param->dualMonoParam.audioProdInfo.roomtyp2)
      );

      if (AC3_ROOMTYP_RES == param->dualMonoParam.audioProdInfo.roomtyp2)
        LIBBLU_AC3_ERROR_RETURN(
          "Reserved value in use (roomtyp2 == 0x%" PRIX8 ").\n",
          param->dualMonoParam.audioProdInfo.roomtyp2
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
    if (_checkAc3AlternateBitStreamInfoCompliance(param) < 0)
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
    if (_checkAc3AddbsiCompliance(&param->addbsi) < 0)
      return -1;
  }

  return 0;
}

static bool _constantAc3BitStreamInfoCheck(
  const Ac3BitStreamInfoParameters * first,
  const Ac3BitStreamInfoParameters * second
)
{
  return CHECK(
    EQUAL(->bsid)
    EQUAL(->bsmod)
    EQUAL(->acmod)
    EQUAL(->lfeon)
  );
}

int decodeAc3BitStreamInfo(
  BitstreamReaderPtr ac3Input,
  Ac3SyncFrameParameters * param
)
{
  /* For (bsid <= 0x8) syntax */
  uint32_t value;

  Ac3BitStreamInfoParameters bsiParam = {0};

  /* [u5 bsid] */
  if (readBits(ac3Input, &value, 5) < 0)
    return -1;
  bsiParam.bsid = value;

  if (8 < bsiParam.bsid)
    LIBBLU_AC3_ERROR_RETURN(
      "Unexpected or unsupported Bit Stream Identifier "
      "(bsid == %" PRIu8 ").\n",
      bsiParam.bsid
    );

  /* [u3 bsmod] */
  if (readBits(ac3Input, &value, 3) < 0)
    return -1;
  bsiParam.bsmod = value;

  /* [u3 acmod] */
  if (readBits(ac3Input, &value, 3) < 0)
    return -1;
  bsiParam.acmod = value;

  if (threeFrontChannelsPresentAc3Acmod(bsiParam.acmod)) {
    /* If 3 front channels present. */

    /* [u2 cmixlevel] */
    if (readBits(ac3Input, &value, 2) < 0)
      return -1;
    bsiParam.cmixlev = value;
  }

  if (isSurroundAc3Acmod(bsiParam.acmod)) {
    /* If surround channel(s) present. */

    /* [u2 surmixlev] */
    if (readBits(ac3Input, &value, 2) < 0)
      return -1;
    bsiParam.surmixlev = value;
  }

  if (bsiParam.acmod == AC3_ACMOD_L_R) {
    /* If Stereo 2/0 (L, R) */

    /* [u2 dsurmod] */
    if (readBits(ac3Input, &value, 2) < 0)
      return -1;
    bsiParam.dsurmod = value;
  }

  /* [b1 lfeon] */
  if (readBits(ac3Input, &value, 1) < 0)
      return -1;
  bsiParam.lfeon = value;

  bsiParam.nbChannels = nbChannelsAc3Acmod(
    bsiParam.acmod,
    bsiParam.lfeon
  );

  /* [u5 dialnorm] */
  if (readBits(ac3Input, &value, 5) < 0)
    return -1;
  bsiParam.dialnorm = value;

  /* [b1 compre] */
  if (readBits(ac3Input, &value, 1) < 0)
    return -1;
  bsiParam.compre = value;

  if (bsiParam.compre) {
    /* [u8 compr] */
    if (readBits(ac3Input, &value, 8) < 0)
      return -1;
    bsiParam.compr = value;
  }

  /* [b1 langcode] */
  if (readBits(ac3Input, &value, 1) < 0)
    return -1;
  bsiParam.langcode = value;

  if (bsiParam.langcode) {
    /* [u8 langcod] */
    if (readBits(ac3Input, &value, 8) < 0)
      return -1;
    bsiParam.langcod = value;
  }

  /* [b1 audprodie] */
  if (readBits(ac3Input, &value, 1) < 0)
    return -1;
  bsiParam.audprodie = value;

  if (bsiParam.audprodie) {
    /* [u5 mixlevel] */
    if (readBits(ac3Input, &value, 5) < 0)
      return -1;
    bsiParam.audioProdInfo.mixlevel = value;

    /* [u2 roomtyp] */
    if (readBits(ac3Input, &value, 2) < 0)
      return -1;
    bsiParam.audioProdInfo.roomtyp = value;
  }

  if (bsiParam.acmod == AC3_ACMOD_CH1_CH2) {
    /* If 1+1 duplicated mono, require parameters for the second mono builded channel. */

    /* [u5 dialnorm2] */
    if (readBits(ac3Input, &value, 5) < 0)
      return -1;
    bsiParam.dualMonoParam.dialnorm2 = value;

    /* [b1 compr2e] */
    if (readBits(ac3Input, &value, 1) < 0)
      return -1;
    bsiParam.dualMonoParam.compr2e = value;

    if (bsiParam.dualMonoParam.compr2e) {
      /* [u8 compr2] */
      if (readBits(ac3Input, &value, 8) < 0)
        return -1;
      bsiParam.dualMonoParam.compr2 = value;
    }

    /* [b1 langcod2e] */
    if (readBits(ac3Input, &value, 1) < 0)
      return -1;
    bsiParam.dualMonoParam.langcod2e = value;

    if (bsiParam.dualMonoParam.langcod2e) {
      /* [u8 langcod2] */
      if (readBits(ac3Input, &value, 8) < 0)
        return -1;
      bsiParam.dualMonoParam.langcod2 = value;
    }

    /* [b1 audprodi2e] */
    if (readBits(ac3Input, &value, 1) < 0)
      return -1;
    bsiParam.dualMonoParam.audprodi2e = value;

    if (bsiParam.dualMonoParam.audprodi2e) {
      /* [u5 mixlevel2] */
      if (readBits(ac3Input, &value, 5) < 0)
        return -1;
      bsiParam.dualMonoParam.audioProdInfo.mixlevel2 = value;

      /* [u2 roomtyp2] */
      if (readBits(ac3Input, &value, 2) < 0)
        return -1;
      bsiParam.dualMonoParam.audioProdInfo.roomtyp2 = value;
    }
  }

  /* [b1 copyrightb] */
  if (readBits(ac3Input, &value, 1) < 0)
    return -1;
  bsiParam.copyrightb = value;

  /* [b1 origbs] */
  if (readBits(ac3Input, &value, 1) < 0)
    return -1;
  bsiParam.origbs = value;

  if (bsiParam.bsid == 0x6) {
    /* Alternate Bit Stream Syntax */
    /* ETSI TS 102 366 V1.4.1 - Annex D */

    /* [b1 xbsi1e] */
    if (readBits(ac3Input, &value, 1) < 0)
      return -1;
    bsiParam.xbsi1e = value;

    if (bsiParam.xbsi1e) {
      /* [u2 dmixmod] */
      if (readBits(ac3Input, &value, 2) < 0)
        return -1;
      bsiParam.xbsi1.dmixmod = value;

      /* [u3 ltrtcmixlev] */
      if (readBits(ac3Input, &value, 3) < 0)
        return -1;
      bsiParam.xbsi1.ltrtcmixlev = value;

      /* [u3 ltrtsurmixlev] */
      if (readBits(ac3Input, &value, 3) < 0)
        return -1;
      bsiParam.xbsi1.ltrtsurmixlev = value;

      /* [u3 lorocmixlev] */
      if (readBits(ac3Input, &value, 3) < 0)
        return -1;
      bsiParam.xbsi1.lorocmixlev = value;

      /* [u3 lorosurmixlev] */
      if (readBits(ac3Input, &value, 3) < 0)
        return -1;
      bsiParam.xbsi1.lorosurmixlev = value;
    }

    /* [b1 xbsi2e] */
    if (readBits(ac3Input, &value, 1) < 0)
      return -1;
    bsiParam.xbsi2e = value;

    if (bsiParam.xbsi2e) {
      /* [u2 dsurexmod] */
      if (readBits(ac3Input, &value, 2) < 0)
        return -1;
      bsiParam.xbsi2.dsurexmod = value;

      /* [u2 dheadphonmod] */
      if (readBits(ac3Input, &value, 2) < 0)
        return -1;
      bsiParam.xbsi2.dheadphonmod = value;

      /* [b1 adconvtyp] */
      if (readBits(ac3Input, &value, 1) < 0)
        return -1;
      bsiParam.xbsi2.adconvtyp = value;

      /* [u8 xbsi2] */
      if (readBits(ac3Input, &value, 8) < 0)
        return -1;
      bsiParam.xbsi2.xbsi2 = value;

      /* [b1 encinfo] */
      if (readBits(ac3Input, &value, 1) < 0)
        return -1;
      bsiParam.xbsi2.encinfo = value;
    }
  }
  else {
    /* [b1 timecod1e] */
    if (readBits(ac3Input, &value, 1) < 0)
      return -1;
    bsiParam.timecod1e = value;

    if (bsiParam.timecod1e) {
      /* [u16 timecod1] */
      if (readBits(ac3Input, &value, 16) < 0)
        return -1;
      bsiParam.timecod1 = value;
    }

    /* [b1 timecod2e] */
    if (readBits(ac3Input, &value, 1) < 0)
      return -1;
    bsiParam.timecod2e = value;

    if (bsiParam.timecod2e) {
      /* [u16 timecod2] */
      if (readBits(ac3Input, &value, 16) < 0)
        return -1;
      bsiParam.timecod2 = value;
    }
  }

  /* [b1 addbsie] */
  if (readBits(ac3Input, &value, 1) < 0)
    return -1;
  bsiParam.addbsie = value;

  if (bsiParam.addbsie) {
    if (_decodeAc3Addbsi(ac3Input, &bsiParam.addbsi) < 0)
      return -1;
  }

  if (paddingByte(ac3Input) < 0)
    return -1;

  if (!param->checkMode) {
    /* Compliance checks */

    if (checkAc3BitStreamInfoCompliance(&bsiParam) < 0)
      return -1;

    param->bitstreamInfo = bsiParam;
  }
  else {
    /* Consistency check */
    if (!_constantAc3BitStreamInfoCheck(&bsiParam, &param->bitstreamInfo))
      LIBBLU_AC3_ERROR_RETURN(
        "Bit Stream Information contains forbidden non-constant values.\n"
      );
  }

  return 0;
}

void buildStrReprEac3Chanmap(
  char * dst,
  uint16_t mask
)
{
  int i;
  char * sep;

  static const char * chanmapStr[16] = {
    "LFE"
    "LFE2",
    "Lts, Rts",
    "Vhc",
    "Vhl, Vhr",
    "Lw, Rw",
    "Lsd, Rsd",
    "Ts",
    "Cs",
    "Lrs, Rrs",
    "Lc, Rc",
    "Rs",
    "Ls",
    "R",
    "C",
    "L",
  };

  sep = "";
  for (i = 16; 0 < i; i--) {
    if (mask & (1 << i)) {
      lb_str_cat(&dst, sep);
      lb_str_cat(&dst, chanmapStr[i]);

      sep = ", ";
    }
  }
}

static int _checkEac3BitStreamInfoCompliance(
  const Eac3BitStreamInfoParameters * param
)
{

  LIBBLU_AC3_DEBUG(" Bit stream information, bsi()\n");

  LIBBLU_AC3_DEBUG(
    "  Stream type (strmtyp): Type %u (0x%X).\n",
    param->strmtyp, param->strmtyp
  );
  LIBBLU_AC3_DEBUG(
    "   -> %s.\n",
    Eac3StrmtypStr(param->strmtyp)
  );

  if (EAC3_STRMTYP_RES <= param->strmtyp)
    LIBBLU_AC3_ERROR_RETURN(
      "Reserved value in use (strmtyp == 0x%x).\n",
      param->strmtyp
    );

  if (param->strmtyp != EAC3_STRMTYP_TYPE_1)
    LIBBLU_AC3_ERROR_RETURN(
      "E-AC3 Bit Stream type must be independant "
      "(strmtyp == 0x%x, %s).\n",
      param->strmtyp,
      Eac3StrmtypStr(param->strmtyp)
    );

  LIBBLU_AC3_DEBUG(
    "  Substream ID (substreamid): 0x%02" PRIX8 ".\n",
    param->substreamid
  );

  if (param->substreamid != 0x0)
    LIBBLU_AC3_ERROR_RETURN(
      "Unexpected non-zero Substream Identification (substreamid == %u).\n",
      param->substreamid
    );

  LIBBLU_AC3_DEBUG(
    "  Frame size (frmsiz): %" PRIu16 " words (0x%04" PRIX16 ").\n",
    param->frmsiz, param->frmsiz
  );
  LIBBLU_AC3_DEBUG(
    "   -> %u bytes.\n",
    param->frameSize
  );

  LIBBLU_AC3_DEBUG(
    "  Sample Rate Code (fscod): %" PRIu8 " (0x%02" PRIX8 ").\n",
    param->fscod, param->fscod
  );
  LIBBLU_AC3_DEBUG(
    "   -> %s.\n",
    Ac3FscodStr(param->fscod, true)
  );
  /* Check for fscod value is delayed to display sample rate after fscod2 */

  if (param->fscod == EAC3_FSCOD_FSCOD2) {
    LIBBLU_AC3_DEBUG(
      "  Sample Rate Code 2 (fscod2): %" PRIu8 " (0x%02" PRIX8 ").\n",
      param->fscod2, param->fscod2
    );
    LIBBLU_AC3_DEBUG(
      "   -> %s.\n",
      Eac3Fscod2Str(param->fscod2)
    );

    if (EAC3_FSCOD2_RES <= param->fscod2)
      LIBBLU_AC3_ERROR_RETURN(
        "Reserved value in use (fscod2 == 0x%x).\n",
        param->fscod2
      );

    LIBBLU_AC3_DEBUG(
      "  Number of Audio Blocks (numblkscod): 0x%02" PRIX8 " (induced).\n",
      param->numblkscod
    );
  }
  else {
    LIBBLU_AC3_DEBUG(
      "  Number of Audio Blocks (numblkscod): 0x%02" PRIX8 ".\n",
      param->numblkscod
    );
  }

  LIBBLU_AC3_DEBUG(
    "   -> %u block(s) per syncframe.\n",
    param->numBlksPerSync
  );

  if (EAC3_FSCOD_48000_HZ != param->fscod)
    LIBBLU_AC3_ERROR_RETURN(
      "Unexpected sample rate of %u Hz, only 48 kHz bitstreams are allowed "
      "(fscod == 0x%x, fscod2 == 0x%x).\n",
      param->fscod, param->fscod2
    );

  LIBBLU_AC3_DEBUG(
    "  Audio Coding Mode (acmod): 0x%02" PRIX8 ".\n",
    param->acmod
  );

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
    "  Bit Stream Identification (bsid): %" PRIu8 " (0x%02" PRIx8 ").\n",
    param->bsid, param->bsid
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

  if (param->acmod == AC3_ACMOD_CH1_CH2) {
    /* Dual Mono */
    LIBBLU_AC3_DEBUG(
      "  Dual-Mono (acmod == 0x00) specific parameters:\n"
    );

    LIBBLU_AC3_DEBUG(
      "   Dialogue Normalization for ch2 (dialnorm2): 0x%02" PRIX8 ".\n",
      param->dualMonoParam.dialnorm2
    );
    LIBBLU_AC3_DEBUG(
      "    -> -%" PRIu8 " dB.\n",
      param->dualMonoParam.dialnorm2
    );

    /* TODO: Is fatal ? */
    if (!param->dualMonoParam.dialnorm2)
      LIBBLU_AC3_WARNING(
        "Non-fatal reserved value in use (dialnorm2 == 0x%" PRIX8 ").\n",
        param->dualMonoParam.dialnorm2
      );

    LIBBLU_AC3_DEBUG(
      "   Compression Gain Word Exists for ch2 (compr2e): %s (0b%x).\n",
      BOOL_STR(param->dualMonoParam.compr2e),
      param->dualMonoParam.compr2e
    );
    if (param->dualMonoParam.compr2e) {
      LIBBLU_AC3_DEBUG(
        "     Compression Gain Word for ch2 (compr2): 0x%02" PRIX8 ".\n",
        param->dualMonoParam.compr2
      );
    }
  }

  if (param->strmtyp == EAC3_STRMTYP_TYPE_1) {
    /* Dependent substream. */

    LIBBLU_AC3_DEBUG(
      "  Custom Channel Map Exists (chanmape): %s (0b%x).\n",
      BOOL_STR(param->chanmape),
      param->chanmape
    );

    if (param->chanmape) {
      char chanmapRepr[EAC3_CHANMAP_STR_BUFSIZE];

      buildStrReprEac3Chanmap(chanmapRepr, param->chanmap);

      LIBBLU_AC3_DEBUG(
        "   Custom Channel Map (chanmap): 0x%04" PRIX16 ".\n",
        param->chanmap
      );
      LIBBLU_AC3_DEBUG(
        "    -> %s (%u channel(s)).\n",
        chanmapRepr,
        param->nbChannelsFromMap
      );

      /* [1] E.1.3.1.8 / [2] 2.3.1.8 Violation */
      if (param->nbChannels != param->nbChannelsFromMap)
        LIBBLU_AC3_ERROR_RETURN(
          "Unexpected Custom Channel Map, the number of speakers does not match "
          "the expected value according to 'acmod' and 'lfeon' fields "
          "(expect %u, got %u).\n",
          param->nbChannels,
          param->nbChannelsFromMap
        );
    }
  }

  LIBBLU_AC3_DEBUG(
    "  Mixing Meta-Data Exists (mixmdate): %s (0b%x).\n",
    BOOL_STR(param->mixmdate),
    param->mixmdate
  );

  if (param->mixmdate) {
    if (AC3_ACMOD_L_R < param->acmod) {
      /* More than two channels. */

      LIBBLU_AC3_DEBUG(
        "   Preferred Stereo Downmix Mode (dmixmod): 0x%x.\n",
        param->Mixdata.dmixmod
      );

      LIBBLU_AC3_DEBUG(
        "    -> %s.\n",
        Ac3DmixmodStr(param->Mixdata.dmixmod)
      );

      if (AC3_DMIXMOD_RES == param->Mixdata.dmixmod)
        LIBBLU_AC3_WARNING(
          "Non-fatal reserved value in use (dmixmod == 0x%" PRIX8 ", "
          "interpreted as 0x0, 'Not indicated').\n",
          param->Mixdata.dmixmod
        );
    }

    if (threeFrontChannelsPresentAc3Acmod(param->acmod)) {
      /* Three front channels present. */

      LIBBLU_AC3_DEBUG(
        "   Lt/Rt Center Mix Level (ltrtcmixlev): 0x%x.\n",
        param->Mixdata.ltrtcmixlev
      );
      LIBBLU_AC3_DEBUG(
        "    -> %s.\n",
        Ac3LtrtcmixlevStr(param->Mixdata.ltrtcmixlev)
      );

      LIBBLU_AC3_DEBUG(
        "   Lo/Ro Center Mix Level (lorocmixlev): 0x%x.\n",
        param->Mixdata.lorocmixlev
      );
      LIBBLU_AC3_DEBUG(
        "    -> %s.\n",
        Ac3lLorocmixlevStr(param->Mixdata.lorocmixlev)
      );
    }

    if (isSurroundAc3Acmod(param->acmod)) {
      /* Surround channels present. */

      LIBBLU_AC3_DEBUG(
        "   Lt/Rt Surround Mix Level (ltrtsurmixlev): 0x%x.\n",
        param->Mixdata.ltrtsurmixlev
      );
      LIBBLU_AC3_DEBUG(
        "    -> Surround mixing level coefficient (slev): %s.\n",
        Ac3LtrtsurmixlevStr(param->Mixdata.ltrtsurmixlev)
      );

      if (param->Mixdata.ltrtsurmixlev <= AC3_LTRTSURMIXLEV_RES_2)
        LIBBLU_AC3_WARNING(
          "Non-fatal reserved value in use (ltrtsurmixlev == 0x%" PRIX8 ", "
          "interpreted as 0x3, '0.841 (-1.5 dB)').\n",
          param->Mixdata.ltrtsurmixlev
        );

      LIBBLU_AC3_DEBUG(
        "   Lo/Ro Surround Mix Level (lorosurmixlev): 0x%x.\n",
        param->Mixdata.lorosurmixlev
      );
      LIBBLU_AC3_DEBUG(
        "    -> Surround mixing level coefficient (slev): %s.\n",
        Ac3LorosurmixlevStr(param->Mixdata.lorosurmixlev)
      );

      if (param->Mixdata.lorosurmixlev <= AC3_LOROSURMIXLEV_RES_2)
        LIBBLU_AC3_WARNING(
          "Non-fatal reserved value in use (lorosurmixlev == 0x%" PRIX8 ", "
          "interpreted as 0x3, '0.841 (-1.5 dB)').\n",
          param->Mixdata.lorosurmixlev
        );
    }

    if (param->lfeon) {

      LIBBLU_AC3_DEBUG(
        "   LFE mix level code exists (lfemixlevcode): %s (0b%x).\n",
        BOOL_STR(param->Mixdata.lfemixlevcode),
        param->Mixdata.lfemixlevcode
      );

      if (param->Mixdata.lfemixlevcode) {

        LIBBLU_AC3_DEBUG(
          "    LFE mix level code (lfemixlevcod): "
          "%" PRIu8 " (0x%02" PRIX8 ").\n",
          param->Mixdata.lfemixlevcod
        );
        LIBBLU_AC3_DEBUG(
          "     -> LFE mix level: %d dB.\n",
          10 - (int) param->Mixdata.lfemixlevcod
        );
      }
    }

    if (param->strmtyp == EAC3_STRMTYP_TYPE_0) {
      LIBBLU_AC3_DEBUG(
        "   Independant stream type (strmtyp == 0x00) specific parameters:\n"
      );

      /* TODO */
      LIBBLU_AC3_DEBUG(
        "    *Not checked*\n"
      );
    }
  }

  LIBBLU_AC3_DEBUG(
    "  Informational Metadata Exists (infomdate): %s (0b%x).\n",
    BOOL_STR(param->infomdate),
    param->infomdate
  );

  if (param->infomdate) {
    const Eac3Infomdat * infParam = &param->InformationalMetadata;

    LIBBLU_AC3_DEBUG(
      "   Bit Stream Mode (bsmod): 0x%02" PRIX8 ".\n",
      infParam->bsmod
    );
    LIBBLU_AC3_DEBUG(
      "    -> %s.\n",
      Ac3BsmodStr(infParam->bsmod, param->acmod)
    );

    LIBBLU_AC3_DEBUG(
      "   Copyrighted stream (copyrightb): %s (0b%x).\n",
      (infParam->copyrightb) ? "Protected" : "Not indicated",
      infParam->copyrightb
    );

    LIBBLU_AC3_DEBUG(
      "   Original Bit Stream (origbs): %s (0b%x).\n",
      (infParam->origbs) ? "Original" : "Copy of another bitsream",
      infParam->origbs
    );

    if (param->acmod == AC3_ACMOD_L_R) {
      /* if in 2/0 mode */

      LIBBLU_AC3_DEBUG(
        "   Dolby Surround Mode (dsurmod): 0x%02" PRIX8 ".\n",
        infParam->dsurmod
      );
      LIBBLU_AC3_DEBUG(
        "    -> %s.\n",
        Ac3DsurmodStr(infParam->dsurmod)
      );

      if (AC3_DSURMOD_RES == infParam->dsurmod)
        LIBBLU_AC3_WARNING(
          "Non-fatal reserved value in use (dsurmod == 0x%" PRIX8 ", "
          "interpreted as 0x0, 'not indicated').\n",
          infParam->dsurmod
        );

      LIBBLU_AC3_DEBUG(
        "   Dolby Headphone Mode (dheadphonmod): 0x%X.\n",
        infParam->dheadphonmod
      );
      LIBBLU_AC3_DEBUG(
        "    -> %s.\n",
        Ac3DheadphonmodStr(infParam->dheadphonmod)
      );

      if (AC3_DHEADPHONMOD_RES == infParam->dheadphonmod)
        LIBBLU_AC3_WARNING(
          "Non-fatal reserved value in use (dheadphonmod == 0x%" PRIX8 ", "
          "interpreted as 0x0, 'not indicated').\n",
          infParam->dheadphonmod
        );
    }

    if (bothSurroundChannelsPresentAc3mod(param->acmod)) {
      /* if both surround channels exist */

      LIBBLU_AC3_DEBUG(
        "   Dolby Surround EX Mode (dsurexmod): 0x%X.\n",
        infParam->dsurexmod
      );
      LIBBLU_AC3_DEBUG(
        "    -> %s.\n",
        Ac3DsurexmodStr(infParam->dsurexmod)
      );
    }

    LIBBLU_AC3_DEBUG(
      "   Audio Production Information Exists (audprodie): %s (0b%x).\n",
      BOOL_STR(infParam->audprodie),
      infParam->audprodie
    );

    if (infParam->audprodie) {

      LIBBLU_AC3_DEBUG(
        "    Mixing Level (mixlevel): %" PRIu8 " (0x%02" PRIX8 ").\n",
        infParam->audioProdInfo.mixlevel,
        infParam->audioProdInfo.mixlevel
      );
      LIBBLU_AC3_DEBUG(
        "     -> %u dB SPL.\n",
        80 + (unsigned) infParam->audioProdInfo.mixlevel
      );

      LIBBLU_AC3_DEBUG(
        "    Room Type (roomtyp): %" PRIu8 " (0x%02" PRIX8 ").\n",
        infParam->audioProdInfo.roomtyp,
        infParam->audioProdInfo.roomtyp
      );
      LIBBLU_AC3_DEBUG(
        "     -> %s.\n",
        Ac3RoomtypStr(infParam->audioProdInfo.roomtyp)
      );

      if (AC3_ROOMTYP_RES == infParam->audioProdInfo.roomtyp)
        LIBBLU_AC3_WARNING(
          "Non-fatal reserved value in use (roomtyp == 0x%" PRIX8 ", "
          "interpreted as 0x0, 'not indicated').\n",
          infParam->audioProdInfo.roomtyp
        );

      LIBBLU_AC3_DEBUG(
        "    A/D Converter Type (adconvtyp): %s (0b%x).\n",
        BOOL_STR(infParam->audioProdInfo.adconvtyp),
        infParam->audioProdInfo.adconvtyp
      );
      LIBBLU_AC3_DEBUG(
        "     -> %s.\n",
        (infParam->audioProdInfo.adconvtyp) ? "Standard" : "HDCD"
      );
    }

    if (param->acmod == AC3_ACMOD_CH1_CH2) {
      /* Dual Mono */
      LIBBLU_AC3_DEBUG(
        "   Dual-Mono (acmod == 0x00) specific parameters:\n"
      );

      LIBBLU_AC3_DEBUG(
        "    Audio Production Information Exists for ch2 (audprodi2e): "
        "%s (0b%x).\n",
        BOOL_STR(infParam->audprodi2e),
        infParam->audprodi2e
      );

      if (infParam->audprodi2e) {

        LIBBLU_AC3_DEBUG(
          "     Mixing Level for ch2 (mixlevel2): "
          "%" PRIu8 " (0x%02" PRIX8 ").\n",
          infParam->audioProdInfo2.mixlevel2,
          infParam->audioProdInfo2.mixlevel2
        );
        LIBBLU_AC3_DEBUG(
          "      -> %u dB SPL.\n",
          80 + (unsigned) infParam->audioProdInfo2.mixlevel2
        );

        LIBBLU_AC3_DEBUG(
          "     Room Type for ch2 (roomtyp2): %" PRIu8 " (0x%02" PRIX8 ").\n",
          infParam->audioProdInfo2.roomtyp2,
          infParam->audioProdInfo2.roomtyp2
        );
        LIBBLU_AC3_DEBUG(
          "      -> %s.\n",
          Ac3RoomtypStr(infParam->audioProdInfo2.roomtyp2)
        );

        if (AC3_ROOMTYP_RES == infParam->audioProdInfo2.roomtyp2)
          LIBBLU_AC3_WARNING(
            "Non-fatal reserved value in use (roomtyp2 == 0x%" PRIX8 ", "
            "interpreted as 0x0, 'not indicated').\n",
            infParam->audioProdInfo2.roomtyp2
          );

        LIBBLU_AC3_DEBUG(
          "     A/D Converter Type for ch2 (adconvtyp2): %s (0b%x).\n",
          BOOL_STR(infParam->audioProdInfo2.adconvtyp2),
          infParam->audioProdInfo2.adconvtyp2
        );
        LIBBLU_AC3_DEBUG(
          "      -> %s.\n",
          (infParam->audioProdInfo2.adconvtyp2) ? "Standard" : "HDCD"
        );
      }
    }

    if (notHalfSampleRateAc3Fscod(param->fscod)) {
      /* if not half sample rate */

      LIBBLU_AC3_DEBUG(
        "   Source Sample Rate Code (sourcefscod): %s (0b%x).\n",
        infParam->sourcefscod,
        infParam->sourcefscod
      );

      if (infParam->sourcefscod)
        LIBBLU_AC3_DEBUG(
          "    -> Source material was sampled at twice the sample rate "
          "indicated by fscod.\n"
        );
      else
        LIBBLU_AC3_DEBUG(
          "    -> Source material was sampled at the same sample rate "
          "indicated by fscod.\n"
        );
    }
  }

  /* TODO : convsync */
  if (
    param->strmtyp == EAC3_STRMTYP_TYPE_0
    && param->numblkscod != EAC3_NUMBLKSCOD_6_BLOCKS
  ) {
    /* If independant substream with fewer than 6 blocks of audio. */

    LIBBLU_AC3_DEBUG(
      "  Converter Synchronization Flag (convsync): %s (0b%x).\n",
      BOOL_STR(param->convsync),
      param->convsync
    );
  }

  if (param->strmtyp == EAC3_STRMTYP_TYPE_2) {
    /* if bit stream converted from AC-3 */

    if (param->numblkscod == EAC3_NUMBLKSCOD_6_BLOCKS) {
      LIBBLU_AC3_DEBUG(
        "  Block Identification (blkid): %s (0b%x, induced).\n",
        BOOL_STR(param->blkid),
        param->blkid
      );
    }
    else {
      LIBBLU_AC3_DEBUG(
        "  Block Identification (blkid): %s (0b%x).\n",
        BOOL_STR(param->blkid),
        param->blkid
      );
    }

    if (param->blkid) {
      LIBBLU_AC3_DEBUG(
        "   Frame Size Code (frmsizecod): 0x%02" PRIX8 ".\n",
        param->frmsizecod
      );
    }
  }

  LIBBLU_AC3_DEBUG(
    "  Additional Bit Stream Information Exists (addbsie): %s (0b%x).\n",
    BOOL_STR(param->addbsie),
    param->addbsie
  );

  if (param->addbsie) {
    if (_checkAc3AddbsiCompliance(&param->addbsi) < 0)
      return -1;
  }

  return 0; /* OK */
}

static bool _constantEac3BitStreamInfoCheck(
  const Eac3BitStreamInfoParameters * first,
  const Eac3BitStreamInfoParameters * second
)
{
  return CHECK(
    EQUAL(->strmtyp)
    EQUAL(->substreamid)
    EQUAL(->frmsiz)
    EQUAL(->fscod)
    EQUAL(->fscod2)
    EQUAL(->numblkscod)
    EQUAL(->acmod)
    EQUAL(->lfeon)
    EQUAL(->bsid)
    EQUAL(->chanmape)
    EQUAL(->chanmap)
    EQUAL(->mixmdate)
    EQUAL(->convsync)
    EQUAL(->blkid)
    EQUAL(->frmsizecod)
  );
}

static int _decodeEac3BitStreamInfo(
  BitstreamReaderPtr ac3Input,
  Ac3SyncFrameParameters * param
)
{
  /* For (bsid == 0x10) syntax */
  uint32_t value;

  Eac3BitStreamInfoParameters bsiParam = {0};

  /* [u2 strmtyp] */
  if (readBits(ac3Input, &value, 2) < 0)
    return -1;
  bsiParam.strmtyp = value;

  /* [u3 substreamid] */
  if (readBits(ac3Input, &value, 3) < 0)
    return -1;
  bsiParam.substreamid = value;

  /* [u11 frmsiz] */
  if (readBits(ac3Input, &value, 11) < 0)
    return -1;
  bsiParam.frmsiz = value;
  bsiParam.frameSize = (value + 1) * AC3_WORD_SIZE;

  /* [u2 fscod] */
  if (readBits(ac3Input, &value, 2) < 0)
    return -1;
  bsiParam.fscod = value;
  bsiParam.sampleRate = sampleRateAc3Fscod(value);

  if (bsiParam.fscod == 0x3) {
    /* [u2 fscod2] */
    if (readBits(ac3Input, &value, 2) < 0)
      return -1;
    bsiParam.fscod2 = value;
    bsiParam.sampleRate = sampleRateEac3Fscod2(value);

    bsiParam.numblkscod = EAC3_NUMBLKSCOD_6_BLOCKS;
  }
  else {
    /* [u2 numblkscod] */
    if (readBits(ac3Input, &value, 2) < 0)
      return -1;
    bsiParam.numblkscod = value;
  }

  /* Converting numblkscod to Number of Blocks in Sync Frame. */
  bsiParam.numBlksPerSync = nbBlocksEac3Numblkscod(bsiParam.numblkscod);

  /* [u3 acmod] */
  if (readBits(ac3Input, &value, 3) < 0)
    return -1;
  bsiParam.acmod = value;

  /* [b1 lfeon] */
  if (readBits(ac3Input, &value, 1) < 0)
    return -1;
  bsiParam.lfeon = value;

  bsiParam.nbChannels = nbChannelsAc3Acmod(
    bsiParam.acmod,
    bsiParam.lfeon
  );

  /* [u5 bsid] // 0x10/16 in this syntax */
  if (readBits(ac3Input, &value, 5) < 0)
    return -1;
  bsiParam.bsid = value;

  if (bsiParam.bsid <= 10 || 16 < bsiParam.bsid)
    LIBBLU_AC3_ERROR_RETURN(
      "Unexpected or unsupported Bit Stream Identifier "
      "(bsid == %" PRIu8 ", parser expect 16).\n",
      bsiParam.bsid
    );

  /* [u5 dialnorm] */
  if (readBits(ac3Input, &value, 5) < 0)
    return -1;
  bsiParam.dialnorm = value;

  /* [b1 compre] */
  if (readBits(ac3Input, &value, 1) < 0)
    return -1;
  bsiParam.compre = value;

  if (bsiParam.compre) {
    /* [u8 compr] */
    if (readBits(ac3Input, &value, 8) < 0)
      return -1;
    bsiParam.compr = value;
  }

  if (bsiParam.acmod == 0x0) {
    /* If 1+1 duplicated mono, require parameters for the second mono builded channel. */

    /* [u5 dialnorm2] */
    if (readBits(ac3Input, &value, 5) < 0)
      return -1;
    bsiParam.dualMonoParam.dialnorm2 = value;

    /* [b1 compr2e] */
    if (readBits(ac3Input, &value, 1) < 0)
      return -1;
    bsiParam.dualMonoParam.compr2e = value;

    if (bsiParam.dualMonoParam.compr2e) {
      /* [u8 compr2] */
      if (readBits(ac3Input, &value, 8) < 0)
        return -1;
      bsiParam.dualMonoParam.compr2 = value;
    }
  }

  if (bsiParam.strmtyp == 0x1) {
    /* [b1 chanmape] */
    if (readBits(ac3Input, &value, 1) < 0)
      return -1;
    bsiParam.chanmape = value;

    if (bsiParam.chanmape) {
      /* [u16 chanmap] */
      if (readBits(ac3Input, &value, 16) < 0)
        return -1;
      bsiParam.chanmap = value;
      bsiParam.nbChannelsFromMap = nbChannelsEac3Chanmap(value);
    }
  }

  /* [b1 mixmdate] */
  if (readBits(ac3Input, &value, 1) < 0)
    return -1;
  bsiParam.mixmdate = value;

  if (bsiParam.mixmdate) {
    /* Mixing meta-data */
    /* Not checked. */

    if (0x2 < bsiParam.acmod) {
      /* More than two channels. */

      /* [u2 dmixmod] */
      if (readBits(ac3Input, &value, 2) < 0)
        return -1;
      bsiParam.Mixdata.dmixmod = value;
    }

    if ((bsiParam.acmod & 0x1) && (0x2 < bsiParam.acmod)) {
      /* Three front channels present. */

      /* [u3 ltrtcmixlev] */
      if (readBits(ac3Input, &value, 3) < 0)
        return -1;
      bsiParam.Mixdata.ltrtcmixlev = value;

      /* [u3 lorocmixlev] */
      if (readBits(ac3Input, &value, 3) < 0)
        return -1;
      bsiParam.Mixdata.lorocmixlev = value;
    }

    if (bsiParam.acmod & 0x4) {
      /* Surround channels present. */

      /* [u3 ltrtsurmixlev] */
      if (readBits(ac3Input, &value, 3) < 0)
        return -1;
      bsiParam.Mixdata.ltrtsurmixlev = value;

      /* [u3 lorosurmixlev] */
      if (readBits(ac3Input, &value, 3) < 0)
        return -1;
      bsiParam.Mixdata.lorosurmixlev = value;
    }

    if (bsiParam.lfeon) {
      /* [b1 lfemixlevcode] */
      if (readBits(ac3Input, &value, 1) < 0)
        return -1;
      bsiParam.Mixdata.lfemixlevcode = value;

      if (value) {
        /* [u5 lfemixlevcod] */
        if (readBits(ac3Input, &value, 5) < 0)
          return -1;
        bsiParam.Mixdata.lfemixlevcod = value;
      }
    }

    if (bsiParam.strmtyp == 0x0) {
      /* [b1 pgmscle] */
      if (readBits(ac3Input, &value, 1) < 0)
        return -1;
      bsiParam.Mixdata.pgmscle = value;

      if (value) {
        /* [u6 pgmscl] */
        if (readBits(ac3Input, &value, 6) < 0)
          return -1;
        bsiParam.Mixdata.pgmscl = value;
      }

      if (bsiParam.acmod == 0x0) {
        /* [b1 pgmscl2e] */
        if (readBits(ac3Input, &value, 1) < 0)
          return -1;
        bsiParam.Mixdata.pgmscl2e = value;

        if (value) {
          /* [u6 pgmscl2] */
          if (readBits(ac3Input, &value, 6) < 0)
            return -1;
          bsiParam.Mixdata.pgmscl2 = value;
        }
      }

      /* [b1 extpgmscle] */
      if (readBits(ac3Input, &value, 1) < 0)
        return -1;
      bsiParam.Mixdata.extpgmscle = value;

      if (bsiParam.Mixdata.extpgmscle) {
        /* [u6 extpgmscl] */
        if (readBits(ac3Input, &value, 6) < 0)
          return -1;
        bsiParam.Mixdata.extpgmscl = value;
      }

      /* [u2 mixdef] */
      if (readBits(ac3Input, &value, 2) < 0)
        return -1;
      bsiParam.Mixdata.mixdef = value;

      if (bsiParam.Mixdata.mixdef == 0x1) {
        /* [b1 premixcmpsel] */
        if (readBits(ac3Input, &value, 1) < 0)
          return -1;
        bsiParam.Mixdata.premixcmpsel = value;

        /* [b1 drcsrc] */
        if (readBits(ac3Input, &value, 1) < 0)
          return -1;
        bsiParam.Mixdata.drcsrc = value;

        /* [u3 premixcmpscl] */
        if (readBits(ac3Input, &value, 3) < 0)
          return -1;
        bsiParam.Mixdata.premixcmpscl = value;
      }

      else if (bsiParam.Mixdata.mixdef == 0x2) {
        /* [v12 mixdata] */
        if (skipBits(ac3Input, 12) < 0)
          return -1;
      }

      else if (bsiParam.Mixdata.mixdef == 0x3) {
        /* [u5 mixdeflen] */
        if (readBits(ac3Input, &value, 5) < 0)
          return -1;

        /* [v<(mixdeflen + 2) * 8> mixdata] */
        if (skipBits(ac3Input, (value + 2) * 8) < 0)
          return -1;
      }

      if (bsiParam.acmod < 0x2) {
        /* Mono or Dual-Mono */

        /* [b1 paninfoe] */
        if (readBits(ac3Input, &value, 1) < 0)
          return -1;

        if (value) {
          /* [u8 panmean] */
          /* [u6 paninfo] */
          if (skipBits(ac3Input, 14) < 0)
            return -1;
        }

        if (bsiParam.acmod == 0x0) {
          /* Dual-Mono */

          /* [b1 paninfo2e] */
          if (readBits(ac3Input, &value, 1) < 0)
            return -1;

          if (value) {
            /* [u8 panmean2] */
            /* [u6 paninfo2] */
            if (skipBits(ac3Input, 14) < 0)
              return -1;
          }
        }
      }

      /* [b1 frmmixcfginfoe] */
      if (readBits(ac3Input, &value, 1) < 0)
        return -1;

      if (value) {
        if (bsiParam.numblkscod == 0x0) {
          /* [u5 blkmixcfginfo[0]] */
          if (skipBits(ac3Input, 5) < 0)
            return -1;
        }
        else {
          unsigned i;

          for (i = 0; i < bsiParam.numBlksPerSync; i++) {
            /* [b1 blkmixcfginfoe] */
            if (readBits(ac3Input, &value, 1) < 0)
              return -1;

            if (value) {
              /* [u5 blkmixcfginfo[i]] */
              if (skipBits(ac3Input, 5) < 0)
                return -1;
            }
          }
        }
      }
    }
  }

  /* [b1 infomdate] */
  if (readBits(ac3Input, &value, 1) < 0)
    return -1;
  bsiParam.infomdate = value;

  if (bsiParam.infomdate) {
    /* [u3 bsmod] */
    if (readBits(ac3Input, &value, 1) < 0)
      return -1;
    bsiParam.InformationalMetadata.bsmod = value;

    /* [b1 copyrightb] */
    if (readBits(ac3Input, &value, 1) < 0)
      return -1;
    bsiParam.InformationalMetadata.copyrightb = value;

    /* [b1 origbs] */
    if (readBits(ac3Input, &value, 1) < 0)
      return -1;
    bsiParam.InformationalMetadata.origbs = value;

    if (bsiParam.acmod == 0x2) {
      /* If Stereo 2.0 */

      /* [u2 dsurmod] */
      if (readBits(ac3Input, &value, 2) < 0)
        return -1;
      bsiParam.InformationalMetadata.dsurmod = value;

      /* [u2 dheadphonmod] */
      if (readBits(ac3Input, &value, 2) < 0)
        return -1;
      bsiParam.InformationalMetadata.dheadphonmod = value;
    }

    if (0x6 <= bsiParam.acmod) {
      /* If Both Surround channels present (2/2, 3/2 modes) */
      /* [u2 dsurexmod] */
      if (readBits(ac3Input, &value, 2) < 0)
        return -1;
      bsiParam.InformationalMetadata.dsurexmod = value;
    }

    /* [b1 audprodie] */
    if (readBits(ac3Input, &value, 1) < 0)
      return -1;
    bsiParam.InformationalMetadata.audprodie = value;

    if (bsiParam.InformationalMetadata.audprodie) {
      /* [u5 mixlevel] */
      if (readBits(ac3Input, &value, 5) < 0)
        return -1;
      bsiParam.InformationalMetadata.audioProdInfo.mixlevel = value;

      /* [u2 roomtyp] */
      if (readBits(ac3Input, &value, 2) < 0)
        return -1;
      bsiParam.InformationalMetadata.audioProdInfo.roomtyp = value;

      /* [b1 adconvtyp] */
      if (readBits(ac3Input, &value, 1) < 0)
        return -1;
      bsiParam.InformationalMetadata.audioProdInfo.adconvtyp = value;
    }

    if (bsiParam.acmod == 0x0) {
      /* If Dual-Mono mode */

      /* [b1 audprodi2e] */
      if (readBits(ac3Input, &value, 1) < 0)
        return -1;
      bsiParam.InformationalMetadata.audprodi2e = value;

      if (bsiParam.InformationalMetadata.audprodi2e) {
        /* [u5 mixlevel2] */
        if (readBits(ac3Input, &value, 5) < 0)
          return -1;
        bsiParam.InformationalMetadata.audioProdInfo2.mixlevel2 = value;

        /* [u2 roomtyp2] */
        if (readBits(ac3Input, &value, 2) < 0)
          return -1;
        bsiParam.InformationalMetadata.audioProdInfo2.roomtyp2 = value;

        /* [b1 adconvtyp2] */
        if (readBits(ac3Input, &value, 1) < 0)
          return -1;
        bsiParam.InformationalMetadata.audioProdInfo2.adconvtyp2 = value;
      }
    }

    if (bsiParam.fscod < 0x3) {
      /* [b1 sourcefscod] */
      if (readBits(ac3Input, &value, 1) < 0)
        return -1;
      bsiParam.InformationalMetadata.sourcefscod = value;
    }
  }

  if (bsiParam.strmtyp == 0x0 && bsiParam.numblkscod != 0x3) {
    /* [b1 convsync] */
    if (readBits(ac3Input, &value, 1) < 0)
      return -1;
    bsiParam.convsync = value;
  }

  if (bsiParam.strmtyp == 0x2) {
    if (bsiParam.numblkscod == 0x3)
      bsiParam.blkid = true;
    else {
      /* [b1 blkid] */
      if (readBits(ac3Input, &value, 1) < 0)
        return -1;
      bsiParam.blkid = value;
    }

    if (bsiParam.blkid) {
      /* [u6 frmsizecod] */
      if (readBits(ac3Input, &value, 6) < 0)
        return -1;
      bsiParam.frmsizecod = value;
    }
  }

  /* [b1 addbsie] */
  if (readBits(ac3Input, &value, 1) < 0)
    return -1;
  bsiParam.addbsie = value;

  if (bsiParam.addbsie) {
    if (_decodeAc3Addbsi(ac3Input, &bsiParam.addbsi) < 0)
      return -1;
  }

  if (paddingByte(ac3Input) < 0)
    return -1;

  if (!param->eac3Present) {
    /* Compliance checks */

    if (!param->checkMode)
      LIBBLU_ERROR_RETURN(
        "Expect a AC-3 core syncframe before E-AC3 syncframe.\n"
      );

    if (_checkEac3BitStreamInfoCompliance(&bsiParam) < 0)
      return -1;

    param->eac3BitstreamInfo = bsiParam;
    param->eac3Present = true;
    param->checkMode = false;
  }
  else {
    /* Consistency check */
    if (!_constantEac3BitStreamInfoCheck(&bsiParam, &param->eac3BitstreamInfo))
      LIBBLU_AC3_ERROR_RETURN("bsi() values are not constant.\n");
  }

  return 0;
}

static int _decodeAc3Sync(
  BitstreamReaderPtr ac3Input,
  Ac3SyncFrameParameters * param,
  bool * isEac3Frame,
  bool useCrcCheck
)
{
  uint8_t bsid;

  uint64_t startFrameOff = tellPos(ac3Input);

  /* Get bsid to know correct syntax structure : */
  bsid = _getSyncFrameBsid(ac3Input);
  LIBBLU_DEBUG_COM("# Identified bsid: 0x%" PRIx8 ".\n", bsid);

  if (bsid <= 8) {
    /* AC-3 Bit stream syntax */
    LIBBLU_DEBUG_COM(" === AC3 Sync Frame ===\n");

    /* syncinfo() */
    if (_decodeAc3SyncInfo(ac3Input, param, useCrcCheck) < 0)
      return -1;

    /* bsi() */
    if (decodeAc3BitStreamInfo(ac3Input, param) < 0)
      return -1;

    if (useCrcCheck) {
      /* Skipping remaining frame bytes with CRC enabled */
      size_t firstPartSize, firstPartRemSize, secondPartSize;
      uint32_t crcResult;

      firstPartSize  = (param->syncInfo.frameSize * 5 / 8);
      firstPartRemSize = firstPartSize - (tellPos(ac3Input) - startFrameOff);
      secondPartSize = (param->syncInfo.frameSize * 3 / 8);

      /* Compute CRC for the remaining bytes of the first 5/8 of the sync */
      if (skipBytes(ac3Input, firstPartRemSize) < 0)
        return -1;

      if (endCrc(BITSTREAM_CRC_CTX(ac3Input), &crcResult) < 0)
        return -1;

      if (0x0 != crcResult)
        LIBBLU_AC3_ERROR_RETURN("CRC checksum error at the 5/8 of the frame.\n");

      /* Compute CRC for the remaining bytes */
      if (initCrc(BITSTREAM_CRC_CTX(ac3Input), AC3_CRC_PARAMS, 0) < 0)
        return -1;

      if (skipBytes(ac3Input, secondPartSize) < 0)
        return -1;

      if (endCrc(BITSTREAM_CRC_CTX(ac3Input), &crcResult) < 0)
        return -1;

      if (0x0 != crcResult)
        LIBBLU_AC3_ERROR_RETURN("CRC checksum error at the end of the frame.\n");
    }
    else {
      /* Skipping remaining frame bytes with CRC disabled */
      size_t syncFrameRemSize =
        param->syncInfo.frameSize - (tellPos(ac3Input) - startFrameOff)
      ;

      if (skipBytes(ac3Input, syncFrameRemSize) < 0)
        return -1;
    }

    if (NULL != isEac3Frame)
      *isEac3Frame = false;
  }
  else if (10 < bsid && bsid <= 16) {
    /* E-AC-3 Bit stream syntax */
    size_t syncFrameRemSize;

    LIBBLU_DEBUG_COM(" === E-AC3 Sync Frame ===\n");

    /* syncinfo() */
    if (_decodeEac3SyncInfo(ac3Input) < 0)
      return -1;

    if (useCrcCheck) {
      if (initCrc(BITSTREAM_CRC_CTX(ac3Input), AC3_CRC_PARAMS, 0) < 0)
        return -1;
    }

    /* bsi() */
    if (_decodeEac3BitStreamInfo(ac3Input, param) < 0)
      return -1;

    /* Skipping remaining frame bytes */
    syncFrameRemSize =
      param->eac3BitstreamInfo.frameSize - (tellPos(ac3Input) - startFrameOff)
    ;
    if (skipBytes(ac3Input, syncFrameRemSize) < 0)
      return -1;

    if (useCrcCheck) {
      uint32_t crcResult;

      if (endCrc(BITSTREAM_CRC_CTX(ac3Input), &crcResult) < 0)
        return -1;

      if (0x0 != crcResult)
        LIBBLU_AC3_ERROR("CRC checksum error at the end of the frame.\n");
    }

    if (NULL != isEac3Frame)
      *isEac3Frame = true;
  }
  else
    LIBBLU_AC3_ERROR("Unknown or unsupported bsid value %u.\n", bsid);

  return 0;
}

/* ### MLP : ############################################################### */

/** \~english
 * \brief Return 'format_sync' field from 'major_sync_info()' if present.
 *
 * \param input
 * \return uint32_t
 */
static uint32_t _getMlpMajorSyncFormatSyncWord(
  BitstreamReaderPtr input
)
{
  uint64_t value = nextUint64(input);

  /* [v4 check_nibble] [u12 access_unit_length] [u16 input_timing] */
  /* [v32 format_sync] */

  return (value & 0xFFFFFFFF);
}

static bool _isPresentMlpMajorSyncFormatSyncWord(
  BitstreamReaderPtr input
)
{
  uint32_t format_sync = _getMlpMajorSyncFormatSyncWord(input) >> 8;

  return (MLP_SYNCWORD_PREFIX == format_sync);
}

int unpackMlpMajorSyncFormatInfo(
  uint32_t value,
  MlpMajorSyncFormatInfo * param
)
{
  /* [u4 audio_sampling_frequency] */
  param->audio_sampling_frequency = (value >> 28) & 0xF;

  /* [v1 6ch_multichannel_type] */
  param->u6ch_multichannel_type = (value >> 27) & 0x1;

  /* if (MLP_6CH_MULTICHANNEL_TYPE_RES <= param->u6ch_multichannel_type)
    LIBBLU_AC3_ERROR_RETURN(
      "Reserved value in use (6ch_multichannel_type == 0x%x).\n",
      param->u6ch_multichannel_type
    ); */

  /* [v1 8ch_multichannel_type] */
  param->u8ch_multichannel_type = (value >> 26) & 0x1;

  /* if (MLP_8CH_MULTICHANNEL_TYPE_RES <= param->u8ch_multichannel_type)
    LIBBLU_AC3_ERROR_RETURN(
      "Reserved value in use (8ch_multichannel_type == 0x%x).\n",
      param->u8ch_multichannel_type
    ); */

  /* [v2 reserved] // ignored */

  /* [u2 2ch_presentation_channel_modifier] */
  param->u2ch_presentation_channel_modifier = (value >> 22) & 0x3;

  /* [u2 6ch_presentation_channel_modifier] */
  param->u6ch_presentation_channel_modifier = (value >> 20) & 0x3;

  /* [u5 6ch_presentation_channel_assignment] */
  param->u6ch_presentation_channel_assignment = (value >> 15) & 0x1F;

  /* [u2 8ch_presentation_channel_modifier] */
  param->u8ch_presentation_channel_modifier = (value >> 13) & 0x3;

  /* [u13 8ch_presentation_channel_assignment] */
  param->u8ch_presentation_channel_assignment = value & 0x1FFF;

  param->sampleRate = sampleRateMlpAudioSamplingFrequency(
    param->audio_sampling_frequency
  );

  param->u6ch_presentation_channel_nb = getNbChannels6ChPresentationAssignment(
    param->u6ch_presentation_channel_assignment
  );

  param->u8ch_presentation_channel_nb = getNbChannels8ChPresentationAssignment(
    param->u8ch_presentation_channel_assignment
  );

  return 0;
}

int unpackMlpMajorSyncextSubstreamInfo(
  uint8_t value,
  MlpSubstreamInfo * param
)
{
  /* [b1 16-channel presentation is present] */
  param->b16ch_presentation_present = value >> 7;

  /* [u3 8-channel presentation] */
  param->u8ch_presentation = (value >> 4) & 0x7;

  /* [u2 6-channel presentation] */
  param->u6ch_presentation = (value >> 2) & 0x3;

  /* [v2 reserved] // ignored */

  return 0;
}

int decodeMlpExtraChannelMeaning(
  BitstreamReaderPtr ac3Input,
  MlpExtraChannelMeaning * param,
  MlpSubstreamInfo substream_info
)
{
  int64_t startOff, expectedSize, usedSize, paddingLength;
  uint32_t value;
  unsigned i;

  /* Get the start offset to compute extension used size */
  startOff = tellBinaryPos(ac3Input);

  if (substream_info.b16ch_presentation_present) {
    /* 16ch_channel_meaning() */
    Mlp16ChChannelMeaning * v16ChParam =
      &param->content.v16ch_channel_meaning
    ;

    param->type = MLP_EXTRA_CH_MEANING_CONTENT_16CH_MEANING;

    /* [u5 16ch_dialogue_norm] */
    if (readBits(ac3Input, &value, 5) < 0)
      return -1;
    v16ChParam->u16ch_dialogue_norm = value;

    /* [u6 16ch_mix_level] */
    if (readBits(ac3Input, &value, 6) < 0)
      return -1;
    v16ChParam->u16ch_mix_level = value;

    /* [u5 16ch_channel_count] */
    if (readBits(ac3Input, &value, 5) < 0)
      return -1;
    v16ChParam->u16ch_channel_count = value;

    /* [b1 dyn_object_only] */
    if (readBits(ac3Input, &value, 1) < 0)
      return -1;
    v16ChParam->dyn_object_only = value;

    /* TODO: Implement (!dyn_object_only) */
    if (v16ChParam->dyn_object_only) {
      /* [b1 lfe_present] */
      if (readBits(ac3Input, &value, 1) < 0)
        return -1;
      v16ChParam->lfe_present = value;
    }
    else
      LIBBLU_AC3_ERROR_RETURN(
        "16ch_channel_meaning() with "
        "multiple type content not supported yet.\n"
      );
  }
  else
    param->type = MLP_EXTRA_CH_MEANING_CONTENT_UNKNOWN;

  /* Check extension size. */
  expectedSize = (int64_t) param->size;

  /* Used size included 4 bits of extra_channel_meaning_length field */
  usedSize = (tellBinaryPos(ac3Input) - startOff) + 4;

  if (expectedSize < usedSize)
    LIBBLU_AC3_ERROR_RETURN(
      "Too many bits in extra_channel_meaning_data "
      "(expect %" PRId64 " bits, got %" PRId64 ").\n",
      expectedSize, usedSize
    );

  /* Parse remaining reserved bits */
  /* [vn reserved] */
  paddingLength = expectedSize - usedSize - 4;
  param->unknownDataSize = 0;

  for (i = 0; 0 < paddingLength; i++, paddingLength -= 8) {
    unsigned size = MIN(paddingLength, 8);

    if (readBits(ac3Input, &value, size) < 0)
        return -1;
    param->unknownData[i] = value;
    param->unknownDataSize += size;
  }

  return 0;
}

int decodeMlpChannelMeaning(
  BitstreamReaderPtr ac3Input,
  MlpMajorSyncChannelMeaning * param,
  MlpSubstreamInfo substream_info
)
{
  uint32_t value;

  /* [v6 reserved] // ignored */
  if (skipBits(ac3Input, 6) < 0)
    return -1;

  /* [b1 2ch_control_enabled] */
  if (readBit(ac3Input, &param->b2ch_control_enabled) < 0)
    return -1;

  /* [b1 6ch_control_enabled] */
  if (readBit(ac3Input, &param->b6ch_control_enabled) < 0)
    return -1;

  /* [b1 8ch_control_enabled] */
  if (readBit(ac3Input, &param->b8ch_control_enabled) < 0)
    return -1;

  /* [v1 reserved] // ignored */
  if (skipBits(ac3Input, 1) < 0)
    return -1;

  /* [s7 drc_start_up_gain] */
  if (readBits(ac3Input, &value, 7) < 0)
    return -1;
  param->drc_start_up_gain = lb_conv_signed8(value, 7);

  /* [u6 2ch_dialogue_norm] */
  if (readBits(ac3Input, &value, 6) < 0)
    return -1;
  param->u2ch_dialogue_norm = value;

  /* [u6 2ch_mix_level] */
  if (readBits(ac3Input, &value, 6) < 0)
    return -1;
  param->u2ch_mix_level = value;

  /* [u5 6ch_dialogue_norm] */
  if (readBits(ac3Input, &value, 5) < 0)
    return -1;
  param->u6ch_dialogue_norm = value;

  /* [u6 6ch_mix_level] */
  if (readBits(ac3Input, &value, 6) < 0)
    return -1;
  param->u6ch_mix_level = value;

  /* [u5 6ch_source_format] */
  if (readBits(ac3Input, &value, 5) < 0)
    return -1;
  param->u6ch_source_format = value;

  /* [u5 8ch_dialogue_norm] */
  if (readBits(ac3Input, &value, 5) < 0)
    return -1;
  param->u8ch_dialogue_norm = value;

  /* [u6 8ch_mix_level] */
  if (readBits(ac3Input, &value, 6) < 0)
    return -1;
  param->u8ch_mix_level = value;

  /* [u6 8ch_source_format] */
  if (readBits(ac3Input, &value, 6) < 0)
    return -1;
  param->u8ch_source_format = value;

  /* [v1 reserved] // ignored */
  if (skipBits(ac3Input, 1) < 0)
    return -1;

  /* [b1 extra_channel_meaning_present] */
  if (readBit(ac3Input, &(param->extra_channel_meaning_present)) < 0)
    return -1;

  if (param->extra_channel_meaning_present) {
    MlpExtraChannelMeaning * extraParam = &param->extra_channel_meaning;

    /* [u4 extra_channel_meaning_length] */
    if (readBits(ac3Input, &value, 4) < 0)
      return -1;
    extraParam->extra_channel_meaning_length = value;
    extraParam->size = (value + 1) * 16;

    if (decodeMlpExtraChannelMeaning(ac3Input, extraParam, substream_info) < 0)
      return -1;

    /* [vn padding] */
    if (paddingBoundary(ac3Input, 16) < 0)
      return -1;
  }

  return 0;
}

int decodeMlpMajorSyncInfo(
  BitstreamReaderPtr ac3Input,
  MlpMajorSyncParameters * param
)
{
  uint32_t value;

  CrcParam crcParam = TRUE_HD_MS_CRC_PARAMS;

  if (initCrc(BITSTREAM_CRC_CTX(ac3Input), crcParam, 0) < 0)
    return -1;

  /* [v32 format_sync] */
  if (readBits(ac3Input, &value, 32) < 0)
    return -1;

  switch (value) {
    case MLP_SYNCWORD:
      LIBBLU_AC3_ERROR_RETURN(
        "Unexpected DVD Audio MLP format sync word "
        "(format_sync == 0x%08X).\n",
        MLP_SYNCWORD
      );

    case TRUE_HD_SYNCWORD:
      break; /* OK */

    default:
      LIBBLU_AC3_ERROR_RETURN(
        "Unexpected sync word in MLP Major Sync "
        "(format_sync == 0x%08" PRIX32 ").\n",
        value
      );
  }

  /* [v32 format_info] */
  uint32_t format_info;
  if (readBits(ac3Input, &format_info, 32) < 0)
    return -1;
  /* Unpacking is delayed to flags field parsing */

  /* [v16 signature] */
  if (readBits(ac3Input, &value, 16) < 0)
    return -1;

  if (TRUE_HD_SIGNATURE != value) {
    LIBBLU_AC3_ERROR_RETURN(
      "Invalid MLP Major Sync signature word "
      "(signature == 0x%04" PRIX32 ", expect 0x%04X).\n",
      value,
      TRUE_HD_SIGNATURE
    );
  }

  /** [v16 flags]
   * -> b1:  constantFifoBufDelay;
   * -> v3:  reserved;
   * -> b1:  formatInfoAlternative8chAssSyntax;
   * -> v11: reserved.
   *
   * NOTE: reserved fields are ignored.
   */
  if (readBits(ac3Input, &value, 16) < 0)
    return -1;
  param->flags = value;
  param->constantFifoBufDelay = (value >> 15) & 0x1;
  param->formatInfoAlternative8chAssSyntax = (value >> 11) & 0x1;

  /* Unpack format_info field using flags */
  int ret = unpackMlpMajorSyncFormatInfo(format_info, &(param->format_info));
  if (ret < 0)
    return -1;

  /* [v16 reserved] // ignored */
  if (readBits(ac3Input, &value, 16) < 0)
    return -1;

  /* [b1 variable_rate] */
  if (readBits(ac3Input, &value, 1) < 0)
    return -1;
  param->variable_bitrate = value;

  /* [u15 peak_data_rate] */
  if (readBits(ac3Input, &value, 15) < 0)
    return -1;
  param->peak_data_rate = value;

  /* [u4 substreams] */
  if (readBits(ac3Input, &value, 4) < 0)
    return -1;
  param->substreams = value;

  /* [v2 reserved] // ignored */
  if (readBits(ac3Input, &value, 2) < 0)
    return -1;

  /* [u2 extended_substream_info] */
  if (readBits(ac3Input, &value, 2) < 0)
    return -1;
  param->extended_substream_info = value;

  /* [v8 substream_info] */
  if (readBits(ac3Input, &value, 8) < 0)
    return -1;
  if (unpackMlpMajorSyncextSubstreamInfo(value, &param->substream_info) < 0)
    return -1;

  /* [v64 channel_meaning()] */
  if (decodeMlpChannelMeaning(ac3Input, &param->channel_meaning, param->substream_info) < 0)
    return -1;

  if (endCrc(BITSTREAM_CRC_CTX(ac3Input), &value) < 0)
    return -1;
  uint16_t crc_result = (value & 0xFFFF);

  /* [u16 major_sync_info_CRC] */
  if (readBits(ac3Input, &value, 16) < 0)
    return -1;
  param->major_sync_info_CRC = value;

  if (crc_result != param->major_sync_info_CRC)
    LIBBLU_AC3_ERROR_RETURN(
      "Unexpected Major Sync CRC value result.\n"
    );

  param->peakDataRate = DIV_ROUND_UP(
    param->peak_data_rate * param->format_info.sampleRate,
    16
  );

  return 0;
}

/** \~english
 * \brief
 *
 * \param buf
 * \param value
 * \param size Field size in bits, must be between 0-32 and a multiple of 4.
 */
static inline void _applyNibbleXorMlp(
  uint8_t * buf,
  uint32_t value,
  size_t size
)
{
  assert(NULL != buf);
  assert(size <= 32 && 0 == (size % 4));

  for (; 0 < size; size -= 4) {
    *buf ^= (value >> (size - 4)) & 0xF;
  }
}

/** \~english
 * \brief
 *
 * \param d Value destination pointer.
 * \param f Input BitstreamReaderPtr.
 * \param s Value size in bits.
 * \param n Nibble value pointer.
 * \param e Error instruction.
 */
#define READ_BITS_NIBBLE_MLP(d, f, s, n, e)                                   \
  do {                                                                        \
    uint32_t _val;                                                            \
    if (readBits(f, &_val, s) < 0)                                            \
      e;                                                                      \
    _applyNibbleXorMlp(n, _val, s);                                           \
    *(d) = _val;                                                              \
  } while (0)

int decodeMlpAccessUnit(
  BitstreamReaderPtr ac3Input,
  MlpAccessUnitParameters * param
)
{
  uint32_t value;

  LIBBLU_AC3_DEBUG(" === MLP Access Unit ===\n");

  int64_t startOff = tellPos(ac3Input);
  uint8_t checkNibbleValue = 0x00;

  /* [v4 check_nibble] */
  READ_BITS_NIBBLE_MLP(
    &param->checkNibble, ac3Input, 4,
    &checkNibbleValue, return -1
  );

  /* [u12 access_unit_length] */
  READ_BITS_NIBBLE_MLP(
    &param->accessUnitLength, ac3Input, 12,
    &checkNibbleValue, return -1
  );

  /* [u16 input_timing] */
  READ_BITS_NIBBLE_MLP(
    &param->inputTiming, ac3Input, 16,
    &checkNibbleValue, return -1
  );

  /* Check if current frame contain Major Sync : */
  if ((nextUint32(ac3Input) >> 8) == MLP_SYNCWORD_PREFIX) {
    MlpMajorSyncParameters majorSyncParam = {0}; /* Used as comparison */

    /**
     * Major Sync present, used as reference if it is the first one in stream,
     * or check if parameters in the current frame remain the same as the
     * reference during the whole bitsream.
     */
    LIBBLU_DEBUG_COM("MajorSyncInfo present:\n");

    if (decodeMlpMajorSyncInfo(ac3Input, &majorSyncParam) < 0)
      return -1; /* Error happen during decoding. */

    if (!param->checkMode) {
      /*
        No reference parameters where supplied (first frame in stream ?),
        making current frame Major Sync as new reference.
      */
      // TODO: Check

      if (BDAV_TRUE_HD_MAX_AUTHORIZED_BITRATE < majorSyncParam.peakDataRate) {
        // TODO: Check and add to core sync frame bit-rate.
      }

      if (MLP_MAX_EXT_SS_NB < majorSyncParam.substreams) {
        // TODO
      }

      param->majorSync = majorSyncParam;
      param->containAtmos = atmosPresentMlpMajorSyncParameters(&majorSyncParam);
    }
    else {
      /* Huge constant test */
      if (!constantMlpMajorSyncCheck(&(param->majorSync), &majorSyncParam))
        LIBBLU_AC3_DEBUG("Major Sync values are not constant.\n");
    }
  }
#if 0 // This shall not be possible.
  else if (!param->checkMode) {
    /*
      Error, current frame does not contain Major Sync, and no reference parameters have been supplied,
      meaning that the bistream is not a TrueHD bitsream and/or can't be used.
    */
    LIBBLU_AC3_ERROR_RETURN(
      "Missing Major Sync.\n"
    );
  }
#endif

  // TODO: Move sync header check here.
  {
    /* TEMP */
    LIBBLU_AC3_DEBUG(" access_unit()\n");
    LIBBLU_AC3_DEBUG("  /* mlp_sync */\n");

    LIBBLU_AC3_DEBUG(
      "   check_nibble: 0x%" PRIX8 "\n",
      param->checkNibble
    );

    LIBBLU_AC3_DEBUG(
      "   access_unit_length: %" PRIu16 " words (0x%03" PRIX16 ").\n",
      param->accessUnitLength,
      param->accessUnitLength
    );
    LIBBLU_AC3_DEBUG(
      "    -> %u bytes.\n",
      param->accessUnitLength * AC3_WORD_SIZE
    );

    LIBBLU_AC3_DEBUG(
      "   input_timing: %" PRIu16 " (0x%04" PRIX16 ")\n",
      param->inputTiming,
      param->inputTiming
    );
  }

  /* extSubstream_directory */
  LIBBLU_DEBUG_COM("extSubstream directory:\n");

  for (unsigned i = 0; i < param->majorSync.substreams; i++) {
    LIBBLU_DEBUG_COM(" - extSubstream %d.\n", i);

    /* [b1 extra_extSubstream_word] [b1 restart_nonexistent] [b1 crc_present] [v1 reserved] */
    READ_BITS_NIBBLE_MLP(&value, ac3Input, 4, &checkNibbleValue, return -1);

    param->extSubstreams[i].extra_extSubstream_word = value >> 3;
    param->extSubstreams[i].restart_nonexistent = (value >> 2) & 0x1;
    param->extSubstreams[i].crc_present = (value >> 1) & 0x1;
    param->extSubstreams[i].reserved_field = (value >> 1) & 0x1;

    LIBBLU_DEBUG_COM(
      "    -> Extra extSubstream word: %s (0x%d).\n",
      (param->extSubstreams[i].extra_extSubstream_word) ? "Present" : "Absent", param->extSubstreams[i].extra_extSubstream_word
    );
    LIBBLU_DEBUG_COM(
      "    -> Restart header: %s (0x%d).\n",
      (param->extSubstreams[i].restart_nonexistent) ? "Absent" : "Present", param->extSubstreams[i].extra_extSubstream_word
    );
    LIBBLU_DEBUG_COM(
      "    -> extSubstream CRC: %s (0x%d).\n",
      (param->extSubstreams[i].crc_present) ? "Present" : "Absent", param->extSubstreams[i].extra_extSubstream_word
    );
    if (param->extSubstreams[i].reserved_field) {
      LIBBLU_DEBUG_COM("    -> Unknown reserved flag (0x1) present.\n");
    }

    /* [u12 extSubstream_end_ptr] */
    READ_BITS_NIBBLE_MLP(&value, ac3Input, 12, &checkNibbleValue, return -1);

    param->extSubstreams[i].extSubstream_end_ptr = value;
    LIBBLU_DEBUG_COM("    -> extSubstream end ptr: 0x04%" PRIx16 ".\n", param->extSubstreams[i].extSubstream_end_ptr);

    if (param->extSubstreams[i].extra_extSubstream_word) {
      READ_BITS_NIBBLE_MLP(&value, ac3Input, 16, &checkNibbleValue, return -1);
      LIBBLU_DEBUG_COM("    -> drc_gain_update: 0x%" PRIx16 ".\n", (value >> 7) & 0x1FF);
      LIBBLU_DEBUG_COM("    -> drc_time_update: 0x%" PRIx16 ".\n", (value >> 4) & 0x7);
    }
  }

  /* Check nibble */
  LIBBLU_DEBUG_COM(
    "Ending nibble check: %s (0x%" PRIx8 ").\n",
    (checkNibbleValue == 0xF) ? "OK" : "ERROR",
    checkNibbleValue
  );

  if (checkNibbleValue != 0xF)
    LIBBLU_AC3_ERROR_RETURN("Minor Sync check nibble value error.\n");

  /* extSubstreams */
  int64_t startSubOff = 0;
  LIBBLU_DEBUG_COM("extSubstreams: (Start Offset: 0x%04" PRIx32 ")\n", startSubOff);
  for (unsigned i = 0; i < param->majorSync.substreams; i++) {
    LIBBLU_DEBUG_COM(
      " - extSubstream %d, start offset: 0x%04" PRIx32 ", end offset: 0x%04" PRIx32 ".\n",
      i, startSubOff, param->extSubstreams[i].extSubstream_end_ptr
    );

    for (; startSubOff++ < param->extSubstreams[i].extSubstream_end_ptr;) {
      if (readBits(ac3Input,  &value, 16) < 0)
        return -1;
    }
  }

  /* Extra data */
  for (; tellPos(ac3Input) < startOff + param->accessUnitLength * 2;) {
    if (readBits(ac3Input,  &value, 16) < 0)
      return -1;
  }

  return 0;
}

static bool _isMlpExtension(
  const lbc * filename
)
{
  unsigned i;

  const lbc * ext;
  size_t extSize;

  static const lbc * extensions[] = {
    lbc_str(".mlp"),
    lbc_str(".thd"),
    lbc_str(".ac3+thd")
  };

  if (!lbc_cwk_path_get_extension(filename, &ext, &extSize))
    return false;

  for (i = 0; i < ARRAY_SIZE(extensions); i++) {
    if (lbc_equal(ext, extensions[i]))
      return true;
  }
  return false;
}

int analyzeAc3(
  LibbluESParsingSettings * settings
)
{
  int ret;

  EsmsFileHeaderPtr ac3Infos;
  unsigned ac3SourceFileIdx;

  BitstreamReaderPtr ac3Input = NULL;
  BitstreamWriterPtr essOutput = NULL;

  bool isMlpFile, containDolbyAtmos;
  bool extractCore;

  uint64_t startOff;
  uint64_t ac3pts, eac3pts, trueHdpts;

  LibbluESAc3SpecProperties * param;

  enum {
    AC3_AUDIO           = STREAM_CODING_TYPE_AC3,
    TRUE_HD_AUDIO       = STREAM_CODING_TYPE_TRUEHD,
    EAC3_AUDIO          = STREAM_CODING_TYPE_EAC3,
    EAC3_AUDIO_SEC      = STREAM_CODING_TYPE_EAC3_SEC, /* Not supported yet */
    DOLBY_AUDIO_DEFAULT = 0xFF
  } detectedStream = DOLBY_AUDIO_DEFAULT;

  Ac3SyncFrameParameters syncFrame = {0};
  MlpAccessUnitParameters accessUnit = {0};

  extractCore = settings->options.extractCore;

  if (NULL == (ac3Infos = createEsmsFileHandler(ES_AUDIO, settings->options, FMT_SPEC_INFOS_AC3)))
    return -1;

  /* If not a TrueHD extension file, don't expect MLP-TrueHD frames. */
  isMlpFile = _isMlpExtension(settings->esFilepath);

  LIBBLU_DEBUG_COM("MLP/TrueHD content expected: %s.\n", (isMlpFile) ? "YES" : "NO");

  /* Pre-gen CRC-32 */
  if (appendSourceFileEsms(ac3Infos, settings->esFilepath, &ac3SourceFileIdx) < 0)
    return -1;

  /* Open input file : */
  if (NULL == (ac3Input = createBitstreamReaderDefBuf(settings->esFilepath)))
    return -1;

  if (NULL == (essOutput = createBitstreamWriterDefBuf(settings->scriptFilepath)))
    return -1;

  if (writeEsmsHeader(essOutput) < 0)
    return -1;

  containDolbyAtmos = false;
  ac3pts = eac3pts = trueHdpts = 0;

  while (!isEof(ac3Input)) {
    /* Progress bar : */
    printFileParsingProgressionBar(ac3Input);

    startOff = tellPos(ac3Input);

    if (nextUint16(ac3Input) == AC3_SYNCWORD) {
      /* AC-3 Core Sync Frame */
      bool isEac3Frame = false;

      if (_decodeAc3Sync(ac3Input, &syncFrame, &isEac3Frame, USE_CRC) < 0)
        return -1;

      if (!syncFrame.checkMode) {
        if (detectedStream == DOLBY_AUDIO_DEFAULT || detectedStream == AC3_AUDIO)
          detectedStream = (syncFrame.eac3Present) ? (extractCore ? AC3_AUDIO : EAC3_AUDIO) : AC3_AUDIO;

        if (_isPresentMlpMajorSyncFormatSyncWord(ac3Input)) {
          LIBBLU_DEBUG_COM("Detection of MLP major sync word.\n");
          isMlpFile = true;
        }

        syncFrame.checkMode = true; /* Set checking mode for following frames. */
      }

      if (extractCore && isEac3Frame)
        continue; /* Skip consideration of extension. */

      /* Writing PES frames cutting commands : */
      ret = initEsmsAudioPesFrame(
        ac3Infos,
        isEac3Frame,
        false,
        (isEac3Frame) ? eac3pts : ac3pts,
        0
      );
      if (ret < 0)
        return -1;

      ret = appendAddPesPayloadCommand(
        ac3Infos,
        ac3SourceFileIdx,
        0x0,
        startOff,
        (isEac3Frame) ?
          syncFrame.eac3BitstreamInfo.frameSize
        :
          syncFrame.syncInfo.frameSize
      );
      if (ret < 0)
        return -1;

      if (writeEsmsPesPacket(essOutput, ac3Infos) < 0)
        return -1;

      if (isEac3Frame)
        eac3pts += ((uint64_t) MAIN_CLOCK_27MHZ * AC3_SAMPLES_PER_FRAME) / 48000;
      else
        ac3pts += ((uint64_t) MAIN_CLOCK_27MHZ * AC3_SAMPLES_PER_FRAME) / 48000;

      continue;
    }

    if (isMlpFile) {
      /* MLP-TrueHD frame(s) may be present. */

      /* TrueHD Ext Access Unit */
      if (decodeMlpAccessUnit(ac3Input, &accessUnit) < 0)
        return -1;

      if (!accessUnit.checkMode) {
        if (!extractCore) {
          if (detectedStream == DOLBY_AUDIO_DEFAULT || detectedStream == AC3_AUDIO)
            detectedStream = TRUE_HD_AUDIO; // Set stream as Dolby TrueHD.
          else
            LIBBLU_ERROR_RETURN(
              "Incompatible bitstream combination with MLP extension.\n"
            );
        }

        containDolbyAtmos = accessUnit.containAtmos;
        accessUnit.checkMode = true;
      }

      if (extractCore)
        continue; /* Skip consideration of extension. */

      /* Writing PES frames cutting commands : */
      ret = initEsmsAudioPesFrame(
        ac3Infos,
        true,
        false,
        trueHdpts,
        0
      );
      if (ret < 0)
        return -1;

      ret = appendAddPesPayloadCommand(
        ac3Infos,
        ac3SourceFileIdx,
        0x0,
        startOff,
        accessUnit.accessUnitLength * AC3_WORD_SIZE
      );
      if (ret < 0)
        return -1;

      if (writeEsmsPesPacket(essOutput, ac3Infos) < 0)
        return -1;

      trueHdpts += ((uint64_t) MAIN_CLOCK_27MHZ) / TRUE_HD_UNITS_PER_SEC;
      continue;
    }

    /* else */
    LIBBLU_AC3_ERROR_RETURN(
      "Unexpected sync word 0x%08" PRIX32 ".\n",
      nextUint32(ac3Input)
    );
  }

  /* lbc_printf(" === Parsing finished with success. ===\n"); */
  closeBitstreamReader(ac3Input);

  /* [u8 endMarker] */
  if (writeByte(essOutput, ESMS_SCRIPT_END_MARKER) < 0)
    return -1;

  if (!syncFrame.checkMode)
    LIBBLU_AC3_ERROR_RETURN(
      "Missing mandatory AC-3 core.\n"
    );

  ac3Infos->prop.codingType = (LibbluStreamCodingType) detectedStream;

  /* Register AC-3 infos : */
  param = ac3Infos->fmtSpecProp.ac3;
  param->sample_rate_code = syncFrame.syncInfo.fscod;
  param->bsid = syncFrame.bitstreamInfo.bsid;
  param->bit_rate_code = syncFrame.syncInfo.frmsizecod >> 1;
  param->surround_mode = syncFrame.bitstreamInfo.dsurmod;
  param->bsmod = syncFrame.bitstreamInfo.bsmod;
  param->num_channels = syncFrame.bitstreamInfo.acmod;
  param->full_svc = 0; // FIXME

  if (detectedStream == AC3_AUDIO || detectedStream == EAC3_AUDIO) {
    switch (syncFrame.bitstreamInfo.acmod) {
      case 0: /* Dual-Mono */
        ac3Infos->prop.audioFormat = 0x02; break;
      case 1: /* Mono */
        ac3Infos->prop.audioFormat = 0x01; break;
      case 2: /* Stereo */
        ac3Infos->prop.audioFormat = 0x03; break;
      default: /* Multi-channel */
        ac3Infos->prop.audioFormat = 0x06; break;
    }
    ac3Infos->prop.sampleRate = 0x01; /* 48 kHz */
    ac3Infos->prop.bitDepth = 16; /* 16 bits */
  }
  else if (detectedStream == TRUE_HD_AUDIO) {
    assert(accessUnit.majorSync.format_info.u6ch_presentation_channel_nb != 0);

    switch (accessUnit.majorSync.format_info.u6ch_presentation_channel_nb) {
      case 1: /* Mono */
        ac3Infos->prop.audioFormat = 0x01; break;
      case 2: /* Stereo */
        ac3Infos->prop.audioFormat = 0x03; break;
      default:
        if (syncFrame.bitstreamInfo.acmod < 3)
          ac3Infos->prop.audioFormat = 0x0C; /* Stereo + Multi-channel */
        else
          ac3Infos->prop.audioFormat = 0x06; /* Multi-channel */
    }

    switch (accessUnit.majorSync.format_info.sampleRate) {
      case 48000:  /* 48 kHz */
        ac3Infos->prop.sampleRate = 0x01; break;
      case 96000:  /* 96 kHz */
        ac3Infos->prop.sampleRate = 0x04; break;
      default: /* 192 kHz + 48kHz Core */
        ac3Infos->prop.sampleRate = 0x0E; break;
    }

    ac3Infos->prop.bitDepth = 16; /* 16, 20 or 24 bits (Does not matter, not contained in header) */
  }
  else {
    LIBBLU_TODO();
  }

  switch (detectedStream) {
    case AC3_AUDIO:
      ac3Infos->bitrate = 640000;
      break;
    case TRUE_HD_AUDIO:
      ac3Infos->bitrate = 18640000;
      break;
    case EAC3_AUDIO:
      ac3Infos->bitrate = 4736000;
      break;
    default: /* EAC3_AUDIO_SEC */
      ac3Infos->bitrate = 256000;
  }

  /* Display infos : */
  lbc_printf("== Stream Infos =======================================================================\n");
  lbc_printf("Codec: ");
  switch (detectedStream) {
    case AC3_AUDIO:
      lbc_printf("Audio/AC-3");
      break;

    case TRUE_HD_AUDIO:
      if (containDolbyAtmos)
        lbc_printf("Audio/AC-3 (+ TrueHD/Dolby Atmos Extensions)");
      else
        lbc_printf("Audio/AC-3 (+ TrueHD Extension)");
      break;

    case EAC3_AUDIO:
      if (containDolbyAtmos)
        lbc_printf("Audio/AC-3 (+ E-AC3/Dolby Atmos Extensions)");
      else
        lbc_printf("Audio/AC-3 (+ E-AC3 Extension)");
      break;

    default:
      lbc_printf("Audio/Unknown");
  }

  lbc_printf(", Nb channels: %u, Sample rate: 48000 Hz, Bits per sample: 16bits.\n",
    syncFrame.bitstreamInfo.nbChannels
  );
  lbc_printf(
    "Stream Duration: %02" PRIu64 ":%02" PRIu64 ":%02" PRIu64 "\n",
    (ac3pts / MAIN_CLOCK_27MHZ) / 60 / 60, (ac3pts / MAIN_CLOCK_27MHZ) / 60 % 60, (ac3pts / MAIN_CLOCK_27MHZ) % 60
  );
  lbc_printf("=======================================================================================\n");

  ac3Infos->endPts = ac3pts;

  if (addEsmsFileEnd(essOutput, ac3Infos) < 0)
    return -1;
  closeBitstreamWriter(essOutput);

  if (updateEsmsHeader(settings->scriptFilepath, ac3Infos) < 0)
    return -1;
  destroyEsmsFileHandler(ac3Infos);
  return 0;
}