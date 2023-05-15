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

#define READ_BITS(d, bs, s, e)                                                \
  do {                                                                        \
    uint32_t _val;                                                            \
    if (readBits(bs, &_val, s) < 0)                                           \
      e;                                                                      \
    *(d) = _val;                                                              \
  } while (0)

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
  BitstreamReaderPtr bs
)
{
  uint64_t value = nextUint64(bs);

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
    param->frameSize / AC3_WORD,
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

static int _parseAc3SyncInfo(
  BitstreamReaderPtr bs,
  Ac3SyncInfoParameters * syncinfo,
  bool useCrcCheck
)
{
  /* For (bsid <= 0x8) syntax */
  uint32_t value;

  LIBBLU_AC3_DEBUG(" Synchronization Information, syncinfo()\n");

  /* [v16 syncword] // 0x0B77 */
  if (readValueBigEndian(bs, 2, &value) < 0)
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
  if (useCrcCheck)
    initCrcContext(crcCtxBitstream(bs), AC3_CRC_PARAMS, 0);

  /* [v16 crc1] */
  if (readValueBigEndian(bs, 2, &value) < 0)
    return -1;
  syncinfo->crc1 = value;

  /* [u2 fscod] */
  if (readBits(bs, &value, 2) < 0)
    return -1;
  syncinfo->fscod = value;

  /* [u6 frmsizecod] */
  if (readBits(bs, &value, 6) < 0)
    return -1;
  syncinfo->frmsizecod = value;

  syncinfo->sampleRate = sampleRateAc3Fscod(syncinfo->fscod);
  size_t bitrate = _frmsizecodToNominalBitrate(syncinfo->frmsizecod);
  syncinfo->bitrate = bitrate;
  syncinfo->frameSize = bitrate * 2 * AC3_WORD;

  return 0;
}

static int _parseEac3SyncInfo(
  BitstreamReaderPtr bs
)
{
  /* For (bsid == 16) syntax */
  uint32_t value;

  LIBBLU_AC3_DEBUG(" Synchronization Information, syncinfo()\n");

  /* [v16 syncword] // 0x0B77 */
  if (readValueBigEndian(bs, 2, &value) < 0)
    return -1;

  LIBBLU_AC3_DEBUG("  Sync Word (syncword): 0x%04" PRIX16 ".\n", value);

  if (AC3_SYNCWORD != value)
    LIBBLU_AC3_ERROR_RETURN(
      "Unexpected 'syncword' value 0x%04X, expect 0x0B77.\n",
      value
    );

  return 0;
}

static int _parseAc3Addbsi(
  BitstreamReaderPtr bs,
  Ac3Addbsi * param
)
{
  uint32_t value;

  /* [u6 addbsil] */
  if (readBits(bs, &value, 6) < 0)
    return -1;
  param->addbsil = value;

  if (param->addbsil == 1) {
    /* Assuming presence of EC3 Extension      */
    /* Reference: ETSI TS 103 420 V1.2.1 - 8.3 */

    /* Try [v7 reserved] [b1 flag_ec3_extension_type_a] ... */
    if (readBits(bs, &value, 16) < 0)
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
    if (skipBits(bs, (value + 1) * 8) < 0)
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

static int _checkAc3BitStreamInfoCompliance(
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

static int _parseAc3BitStreamInfo(
  BitstreamReaderPtr bs,
  Ac3BitStreamInfoParameters * bsi
)
{
  /* For (bsid <= 0x8) syntax */
  uint32_t value;

  /* [u5 bsid] */
  if (readBits(bs, &value, 5) < 0)
    return -1;
  bsi->bsid = value;

  if (8 < bsi->bsid)
    LIBBLU_AC3_ERROR_RETURN(
      "Unexpected or unsupported Bit Stream Identifier "
      "(bsid == %" PRIu8 ").\n",
      bsi->bsid
    );

  /* [u3 bsmod] */
  if (readBits(bs, &value, 3) < 0)
    return -1;
  bsi->bsmod = value;

  /* [u3 acmod] */
  if (readBits(bs, &value, 3) < 0)
    return -1;
  bsi->acmod = value;

  if (threeFrontChannelsPresentAc3Acmod(bsi->acmod)) {
    /* If 3 front channels present. */

    /* [u2 cmixlevel] */
    if (readBits(bs, &value, 2) < 0)
      return -1;
    bsi->cmixlev = value;
  }

  if (isSurroundAc3Acmod(bsi->acmod)) {
    /* If surround channel(s) present. */

    /* [u2 surmixlev] */
    if (readBits(bs, &value, 2) < 0)
      return -1;
    bsi->surmixlev = value;
  }

  if (bsi->acmod == AC3_ACMOD_L_R) {
    /* If Stereo 2/0 (L, R) */

    /* [u2 dsurmod] */
    if (readBits(bs, &value, 2) < 0)
      return -1;
    bsi->dsurmod = value;
  }

  /* [b1 lfeon] */
  if (readBits(bs, &value, 1) < 0)
      return -1;
  bsi->lfeon = value;

  bsi->nbChannels = nbChannelsAc3Acmod(
    bsi->acmod,
    bsi->lfeon
  );

  /* [u5 dialnorm] */
  if (readBits(bs, &value, 5) < 0)
    return -1;
  bsi->dialnorm = value;

  /* [b1 compre] */
  if (readBits(bs, &value, 1) < 0)
    return -1;
  bsi->compre = value;

  if (bsi->compre) {
    /* [u8 compr] */
    if (readBits(bs, &value, 8) < 0)
      return -1;
    bsi->compr = value;
  }

  /* [b1 langcode] */
  if (readBits(bs, &value, 1) < 0)
    return -1;
  bsi->langcode = value;

  if (bsi->langcode) {
    /* [u8 langcod] */
    if (readBits(bs, &value, 8) < 0)
      return -1;
    bsi->langcod = value;
  }

  /* [b1 audprodie] */
  if (readBits(bs, &value, 1) < 0)
    return -1;
  bsi->audprodie = value;

  if (bsi->audprodie) {
    /* [u5 mixlevel] */
    if (readBits(bs, &value, 5) < 0)
      return -1;
    bsi->audioProdInfo.mixlevel = value;

    /* [u2 roomtyp] */
    if (readBits(bs, &value, 2) < 0)
      return -1;
    bsi->audioProdInfo.roomtyp = value;
  }

  if (bsi->acmod == AC3_ACMOD_CH1_CH2) {
    /* If 1+1 duplicated mono, require parameters for the second mono builded channel. */

    /* [u5 dialnorm2] */
    if (readBits(bs, &value, 5) < 0)
      return -1;
    bsi->dualMonoParam.dialnorm2 = value;

    /* [b1 compr2e] */
    if (readBits(bs, &value, 1) < 0)
      return -1;
    bsi->dualMonoParam.compr2e = value;

    if (bsi->dualMonoParam.compr2e) {
      /* [u8 compr2] */
      if (readBits(bs, &value, 8) < 0)
        return -1;
      bsi->dualMonoParam.compr2 = value;
    }

    /* [b1 langcod2e] */
    if (readBits(bs, &value, 1) < 0)
      return -1;
    bsi->dualMonoParam.langcod2e = value;

    if (bsi->dualMonoParam.langcod2e) {
      /* [u8 langcod2] */
      if (readBits(bs, &value, 8) < 0)
        return -1;
      bsi->dualMonoParam.langcod2 = value;
    }

    /* [b1 audprodi2e] */
    if (readBits(bs, &value, 1) < 0)
      return -1;
    bsi->dualMonoParam.audprodi2e = value;

    if (bsi->dualMonoParam.audprodi2e) {
      /* [u5 mixlevel2] */
      if (readBits(bs, &value, 5) < 0)
        return -1;
      bsi->dualMonoParam.audioProdInfo.mixlevel2 = value;

      /* [u2 roomtyp2] */
      if (readBits(bs, &value, 2) < 0)
        return -1;
      bsi->dualMonoParam.audioProdInfo.roomtyp2 = value;
    }
  }

  /* [b1 copyrightb] */
  if (readBits(bs, &value, 1) < 0)
    return -1;
  bsi->copyrightb = value;

  /* [b1 origbs] */
  if (readBits(bs, &value, 1) < 0)
    return -1;
  bsi->origbs = value;

  if (bsi->bsid == 0x6) {
    /* Alternate Bit Stream Syntax */
    /* ETSI TS 102 366 V1.4.1 - Annex D */

    /* [b1 xbsi1e] */
    if (readBits(bs, &value, 1) < 0)
      return -1;
    bsi->xbsi1e = value;

    if (bsi->xbsi1e) {
      /* [u2 dmixmod] */
      if (readBits(bs, &value, 2) < 0)
        return -1;
      bsi->xbsi1.dmixmod = value;

      /* [u3 ltrtcmixlev] */
      if (readBits(bs, &value, 3) < 0)
        return -1;
      bsi->xbsi1.ltrtcmixlev = value;

      /* [u3 ltrtsurmixlev] */
      if (readBits(bs, &value, 3) < 0)
        return -1;
      bsi->xbsi1.ltrtsurmixlev = value;

      /* [u3 lorocmixlev] */
      if (readBits(bs, &value, 3) < 0)
        return -1;
      bsi->xbsi1.lorocmixlev = value;

      /* [u3 lorosurmixlev] */
      if (readBits(bs, &value, 3) < 0)
        return -1;
      bsi->xbsi1.lorosurmixlev = value;
    }

    /* [b1 xbsi2e] */
    if (readBits(bs, &value, 1) < 0)
      return -1;
    bsi->xbsi2e = value;

    if (bsi->xbsi2e) {
      /* [u2 dsurexmod] */
      if (readBits(bs, &value, 2) < 0)
        return -1;
      bsi->xbsi2.dsurexmod = value;

      /* [u2 dheadphonmod] */
      if (readBits(bs, &value, 2) < 0)
        return -1;
      bsi->xbsi2.dheadphonmod = value;

      /* [b1 adconvtyp] */
      if (readBits(bs, &value, 1) < 0)
        return -1;
      bsi->xbsi2.adconvtyp = value;

      /* [u8 xbsi2] */
      if (readBits(bs, &value, 8) < 0)
        return -1;
      bsi->xbsi2.xbsi2 = value;

      /* [b1 encinfo] */
      if (readBits(bs, &value, 1) < 0)
        return -1;
      bsi->xbsi2.encinfo = value;
    }
  }
  else {
    /* [b1 timecod1e] */
    if (readBits(bs, &value, 1) < 0)
      return -1;
    bsi->timecod1e = value;

    if (bsi->timecod1e) {
      /* [u16 timecod1] */
      if (readBits(bs, &value, 16) < 0)
        return -1;
      bsi->timecod1 = value;
    }

    /* [b1 timecod2e] */
    if (readBits(bs, &value, 1) < 0)
      return -1;
    bsi->timecod2e = value;

    if (bsi->timecod2e) {
      /* [u16 timecod2] */
      if (readBits(bs, &value, 16) < 0)
        return -1;
      bsi->timecod2 = value;
    }
  }

  /* [b1 addbsie] */
  if (readBits(bs, &value, 1) < 0)
    return -1;
  bsi->addbsie = value;

  if (bsi->addbsie) {
    if (_parseAc3Addbsi(bs, &bsi->addbsi) < 0)
      return -1;
  }

  if (paddingByte(bs) < 0)
    return -1;

  return 0;
}

static int _parseRemainingAc3Frame(
  BitstreamReaderPtr bs,
  Ac3SyncInfoParameters syncinfo,
  int64_t syncFrameOffset,
  bool enable_crc_checks
)
{
  if (enable_crc_checks) {
    int64_t firstPartSize    = syncinfo.frameSize * 5 / 8;
    int64_t firstPartRemSize = firstPartSize - (tellPos(bs) - syncFrameOffset);
    int64_t secondPartSize   = syncinfo.frameSize * 3 / 8;

    /* Compute CRC for the remaining bytes of the first 5/8 of the sync */
    if (skipBytes(bs, firstPartRemSize) < 0)
      return -1;

    uint32_t crcResult = completeCrcContext(crcCtxBitstream(bs));

    if (0x0 != crcResult)
      LIBBLU_AC3_ERROR_RETURN("CRC checksum error at the 5/8 of the frame.\n");

    /* Compute CRC for the remaining bytes */
    initCrcContext(crcCtxBitstream(bs), AC3_CRC_PARAMS, 0);

    if (skipBytes(bs, secondPartSize) < 0)
      return -1;

    crcResult = completeCrcContext(crcCtxBitstream(bs));

    if (0x0 != crcResult)
      LIBBLU_AC3_ERROR_RETURN("CRC checksum error at the end of the frame.\n");
  }
  else {
    int64_t remaining = syncinfo.frameSize - (tellPos(bs) - syncFrameOffset);
    if (skipBytes(bs, remaining) < 0)
      return -1;
  }

  return 0;
}

static void _buildStrReprEac3Chanmap(
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

      _buildStrReprEac3Chanmap(chanmapRepr, param->chanmap);

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
        (infParam->audioProdInfo.adconvtyp) ? "HDCD" : "Standard"
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
          (infParam->audioProdInfo2.adconvtyp2) ? "HDCD" : "Standard"
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

static int _parseEac3BitStreamInfo(
  BitstreamReaderPtr bs,
  Eac3BitStreamInfoParameters * bsi
)
{
  /* For (bsid == 0x10) syntax */
  uint32_t value;

  /* [u2 strmtyp] */
  if (readBits(bs, &value, 2) < 0)
    return -1;
  bsi->strmtyp = value;

  /* [u3 substreamid] */
  if (readBits(bs, &value, 3) < 0)
    return -1;
  bsi->substreamid = value;

  /* [u11 frmsiz] */
  if (readBits(bs, &value, 11) < 0)
    return -1;
  bsi->frmsiz = value;
  bsi->frameSize = (value + 1) * AC3_WORD;

  /* [u2 fscod] */
  if (readBits(bs, &value, 2) < 0)
    return -1;
  bsi->fscod = value;
  bsi->sampleRate = sampleRateAc3Fscod(value);

  if (bsi->fscod == 0x3) {
    /* [u2 fscod2] */
    if (readBits(bs, &value, 2) < 0)
      return -1;
    bsi->fscod2 = value;
    bsi->sampleRate = sampleRateEac3Fscod2(value);

    bsi->numblkscod = EAC3_NUMBLKSCOD_6_BLOCKS;
  }
  else {
    /* [u2 numblkscod] */
    if (readBits(bs, &value, 2) < 0)
      return -1;
    bsi->numblkscod = value;
  }

  /* Converting numblkscod to Number of Blocks in Sync Frame. */
  bsi->numBlksPerSync = nbBlocksEac3Numblkscod(bsi->numblkscod);

  /* [u3 acmod] */
  if (readBits(bs, &value, 3) < 0)
    return -1;
  bsi->acmod = value;

  /* [b1 lfeon] */
  if (readBits(bs, &value, 1) < 0)
    return -1;
  bsi->lfeon = value;

  bsi->nbChannels = nbChannelsAc3Acmod(
    bsi->acmod,
    bsi->lfeon
  );

  /* [u5 bsid] // 0x10/16 in this syntax */
  if (readBits(bs, &value, 5) < 0)
    return -1;
  bsi->bsid = value;

  if (bsi->bsid <= 10 || 16 < bsi->bsid)
    LIBBLU_AC3_ERROR_RETURN(
      "Unexpected or unsupported Bit Stream Identifier "
      "(bsid == %" PRIu8 ", parser expect 16).\n",
      bsi->bsid
    );

  /* [u5 dialnorm] */
  if (readBits(bs, &value, 5) < 0)
    return -1;
  bsi->dialnorm = value;

  /* [b1 compre] */
  if (readBits(bs, &value, 1) < 0)
    return -1;
  bsi->compre = value;

  if (bsi->compre) {
    /* [u8 compr] */
    if (readBits(bs, &value, 8) < 0)
      return -1;
    bsi->compr = value;
  }

  if (bsi->acmod == 0x0) {
    /* If 1+1 duplicated mono, require parameters for the second mono builded channel. */

    /* [u5 dialnorm2] */
    if (readBits(bs, &value, 5) < 0)
      return -1;
    bsi->dualMonoParam.dialnorm2 = value;

    /* [b1 compr2e] */
    if (readBits(bs, &value, 1) < 0)
      return -1;
    bsi->dualMonoParam.compr2e = value;

    if (bsi->dualMonoParam.compr2e) {
      /* [u8 compr2] */
      if (readBits(bs, &value, 8) < 0)
        return -1;
      bsi->dualMonoParam.compr2 = value;
    }
  }

  if (bsi->strmtyp == 0x1) {
    /* [b1 chanmape] */
    if (readBits(bs, &value, 1) < 0)
      return -1;
    bsi->chanmape = value;

    if (bsi->chanmape) {
      /* [u16 chanmap] */
      if (readBits(bs, &value, 16) < 0)
        return -1;
      bsi->chanmap = value;
      bsi->nbChannelsFromMap = nbChannelsEac3Chanmap(value);
    }
  }

  /* [b1 mixmdate] */
  if (readBits(bs, &value, 1) < 0)
    return -1;
  bsi->mixmdate = value;

  if (bsi->mixmdate) {
    /* Mixing meta-data */
    /* Not checked. */

    if (0x2 < bsi->acmod) {
      /* More than two channels. */

      /* [u2 dmixmod] */
      if (readBits(bs, &value, 2) < 0)
        return -1;
      bsi->Mixdata.dmixmod = value;
    }

    if ((bsi->acmod & 0x1) && (0x2 < bsi->acmod)) {
      /* Three front channels present. */

      /* [u3 ltrtcmixlev] */
      if (readBits(bs, &value, 3) < 0)
        return -1;
      bsi->Mixdata.ltrtcmixlev = value;

      /* [u3 lorocmixlev] */
      if (readBits(bs, &value, 3) < 0)
        return -1;
      bsi->Mixdata.lorocmixlev = value;
    }

    if (bsi->acmod & 0x4) {
      /* Surround channels present. */

      /* [u3 ltrtsurmixlev] */
      if (readBits(bs, &value, 3) < 0)
        return -1;
      bsi->Mixdata.ltrtsurmixlev = value;

      /* [u3 lorosurmixlev] */
      if (readBits(bs, &value, 3) < 0)
        return -1;
      bsi->Mixdata.lorosurmixlev = value;
    }

    if (bsi->lfeon) {
      /* [b1 lfemixlevcode] */
      if (readBits(bs, &value, 1) < 0)
        return -1;
      bsi->Mixdata.lfemixlevcode = value;

      if (value) {
        /* [u5 lfemixlevcod] */
        if (readBits(bs, &value, 5) < 0)
          return -1;
        bsi->Mixdata.lfemixlevcod = value;
      }
    }

    if (bsi->strmtyp == 0x0) {
      /* [b1 pgmscle] */
      if (readBits(bs, &value, 1) < 0)
        return -1;
      bsi->Mixdata.pgmscle = value;

      if (value) {
        /* [u6 pgmscl] */
        if (readBits(bs, &value, 6) < 0)
          return -1;
        bsi->Mixdata.pgmscl = value;
      }

      if (bsi->acmod == 0x0) {
        /* [b1 pgmscl2e] */
        if (readBits(bs, &value, 1) < 0)
          return -1;
        bsi->Mixdata.pgmscl2e = value;

        if (value) {
          /* [u6 pgmscl2] */
          if (readBits(bs, &value, 6) < 0)
            return -1;
          bsi->Mixdata.pgmscl2 = value;
        }
      }

      /* [b1 extpgmscle] */
      if (readBits(bs, &value, 1) < 0)
        return -1;
      bsi->Mixdata.extpgmscle = value;

      if (bsi->Mixdata.extpgmscle) {
        /* [u6 extpgmscl] */
        if (readBits(bs, &value, 6) < 0)
          return -1;
        bsi->Mixdata.extpgmscl = value;
      }

      /* [u2 mixdef] */
      if (readBits(bs, &value, 2) < 0)
        return -1;
      bsi->Mixdata.mixdef = value;

      if (bsi->Mixdata.mixdef == 0x1) {
        /* [b1 premixcmpsel] */
        if (readBits(bs, &value, 1) < 0)
          return -1;
        bsi->Mixdata.premixcmpsel = value;

        /* [b1 drcsrc] */
        if (readBits(bs, &value, 1) < 0)
          return -1;
        bsi->Mixdata.drcsrc = value;

        /* [u3 premixcmpscl] */
        if (readBits(bs, &value, 3) < 0)
          return -1;
        bsi->Mixdata.premixcmpscl = value;
      }

      else if (bsi->Mixdata.mixdef == 0x2) {
        /* [v12 mixdata] */
        if (skipBits(bs, 12) < 0)
          return -1;
      }

      else if (bsi->Mixdata.mixdef == 0x3) {
        /* [u5 mixdeflen] */
        if (readBits(bs, &value, 5) < 0)
          return -1;

        /* [v<(mixdeflen + 2) * 8> mixdata] */
        if (skipBits(bs, (value + 2) * 8) < 0)
          return -1;
      }

      if (bsi->acmod < 0x2) {
        /* Mono or Dual-Mono */

        /* [b1 paninfoe] */
        if (readBits(bs, &value, 1) < 0)
          return -1;

        if (value) {
          /* [u8 panmean] */
          /* [u6 paninfo] */
          if (skipBits(bs, 14) < 0)
            return -1;
        }

        if (bsi->acmod == 0x0) {
          /* Dual-Mono */

          /* [b1 paninfo2e] */
          if (readBits(bs, &value, 1) < 0)
            return -1;

          if (value) {
            /* [u8 panmean2] */
            /* [u6 paninfo2] */
            if (skipBits(bs, 14) < 0)
              return -1;
          }
        }
      }

      /* [b1 frmmixcfginfoe] */
      if (readBits(bs, &value, 1) < 0)
        return -1;

      if (value) {
        if (bsi->numblkscod == 0x0) {
          /* [u5 blkmixcfginfo[0]] */
          if (skipBits(bs, 5) < 0)
            return -1;
        }
        else {
          unsigned i;

          for (i = 0; i < bsi->numBlksPerSync; i++) {
            /* [b1 blkmixcfginfoe] */
            if (readBits(bs, &value, 1) < 0)
              return -1;

            if (value) {
              /* [u5 blkmixcfginfo[i]] */
              if (skipBits(bs, 5) < 0)
                return -1;
            }
          }
        }
      }
    }
  }

  /* [b1 infomdate] */
  if (readBits(bs, &value, 1) < 0)
    return -1;
  bsi->infomdate = value;

  if (bsi->infomdate) {
    /* [u3 bsmod] */
    if (readBits(bs, &value, 1) < 0)
      return -1;
    bsi->InformationalMetadata.bsmod = value;

    /* [b1 copyrightb] */
    if (readBits(bs, &value, 1) < 0)
      return -1;
    bsi->InformationalMetadata.copyrightb = value;

    /* [b1 origbs] */
    if (readBits(bs, &value, 1) < 0)
      return -1;
    bsi->InformationalMetadata.origbs = value;

    if (bsi->acmod == 0x2) {
      /* If Stereo 2.0 */

      /* [u2 dsurmod] */
      if (readBits(bs, &value, 2) < 0)
        return -1;
      bsi->InformationalMetadata.dsurmod = value;

      /* [u2 dheadphonmod] */
      if (readBits(bs, &value, 2) < 0)
        return -1;
      bsi->InformationalMetadata.dheadphonmod = value;
    }

    if (0x6 <= bsi->acmod) {
      /* If Both Surround channels present (2/2, 3/2 modes) */
      /* [u2 dsurexmod] */
      if (readBits(bs, &value, 2) < 0)
        return -1;
      bsi->InformationalMetadata.dsurexmod = value;
    }

    /* [b1 audprodie] */
    if (readBits(bs, &value, 1) < 0)
      return -1;
    bsi->InformationalMetadata.audprodie = value;

    if (bsi->InformationalMetadata.audprodie) {
      /* [u5 mixlevel] */
      if (readBits(bs, &value, 5) < 0)
        return -1;
      bsi->InformationalMetadata.audioProdInfo.mixlevel = value;

      /* [u2 roomtyp] */
      if (readBits(bs, &value, 2) < 0)
        return -1;
      bsi->InformationalMetadata.audioProdInfo.roomtyp = value;

      /* [b1 adconvtyp] */
      if (readBits(bs, &value, 1) < 0)
        return -1;
      bsi->InformationalMetadata.audioProdInfo.adconvtyp = value;
    }

    if (bsi->acmod == 0x0) {
      /* If Dual-Mono mode */

      /* [b1 audprodi2e] */
      if (readBits(bs, &value, 1) < 0)
        return -1;
      bsi->InformationalMetadata.audprodi2e = value;

      if (bsi->InformationalMetadata.audprodi2e) {
        /* [u5 mixlevel2] */
        if (readBits(bs, &value, 5) < 0)
          return -1;
        bsi->InformationalMetadata.audioProdInfo2.mixlevel2 = value;

        /* [u2 roomtyp2] */
        if (readBits(bs, &value, 2) < 0)
          return -1;
        bsi->InformationalMetadata.audioProdInfo2.roomtyp2 = value;

        /* [b1 adconvtyp2] */
        if (readBits(bs, &value, 1) < 0)
          return -1;
        bsi->InformationalMetadata.audioProdInfo2.adconvtyp2 = value;
      }
    }

    if (bsi->fscod < 0x3) {
      /* [b1 sourcefscod] */
      if (readBits(bs, &value, 1) < 0)
        return -1;
      bsi->InformationalMetadata.sourcefscod = value;
    }
  }

  if (bsi->strmtyp == 0x0 && bsi->numblkscod != 0x3) {
    /* [b1 convsync] */
    if (readBits(bs, &value, 1) < 0)
      return -1;
    bsi->convsync = value;
  }

  if (bsi->strmtyp == 0x2) {
    if (bsi->numblkscod == 0x3)
      bsi->blkid = true;
    else {
      /* [b1 blkid] */
      if (readBits(bs, &value, 1) < 0)
        return -1;
      bsi->blkid = value;
    }

    if (bsi->blkid) {
      /* [u6 frmsizecod] */
      if (readBits(bs, &value, 6) < 0)
        return -1;
      bsi->frmsizecod = value;
    }
  }

  /* [b1 addbsie] */
  if (readBits(bs, &value, 1) < 0)
    return -1;
  bsi->addbsie = value;

  if (bsi->addbsie) {
    if (_parseAc3Addbsi(bs, &bsi->addbsi) < 0)
      return -1;
  }

  if (paddingByte(bs) < 0)
    return -1;

  return 0;
}

static int _parseRemainingEac3Frame(
  BitstreamReaderPtr bs,
  Eac3BitStreamInfoParameters bsi,
  int64_t syncFrameOffset,
  bool checkCrc
)
{
  /* Skipping remaining frame bytes */
  int64_t remaining = bsi.frameSize - (tellPos(bs) - syncFrameOffset);
  if (skipBytes(bs, remaining) < 0)
    return -1;

  if (checkCrc) {
    uint32_t crcResult = completeCrcContext(crcCtxBitstream(bs));

    if (0x0 != crcResult)
      LIBBLU_AC3_ERROR("CRC checksum error at the end of the frame.\n");
  }

  return 0;
}

#if 0
static int _decodeAc3Sync(
  BitstreamReaderPtr bs,
  Ac3SyncFrameParameters * param,
  bool * isEac3Frame,
  bool useCrcCheck
)
{
  uint8_t bsid;

  uint64_t startFrameOff = tellPos(bs);

  /* Get bsid to know correct syntax structure : */
  bsid = _getSyncFrameBsid(bs);
  LIBBLU_DEBUG_COM("# Identified bsid: 0x%" PRIx8 ".\n", bsid);

  if (bsid <= 8) {
    /* AC-3 Bit stream syntax */
    LIBBLU_DEBUG_COM(" === AC3 Sync Frame ===\n");

    /* syncinfo() */
    if (_parseAc3SyncInfo(bs, param, useCrcCheck) < 0)
      return -1;

    /* bsi() */
    if (_parseAc3BitStreamInfo(bs, param) < 0)
      return -1;

    if (useCrcCheck) {
      /* Skipping remaining frame bytes with CRC enabled */
      size_t firstPartSize, firstPartRemSize, secondPartSize;
      uint32_t crcResult;

      firstPartSize  = (param->syncInfo.frameSize * 5 / 8);
      firstPartRemSize = firstPartSize - (tellPos(bs) - startFrameOff);
      secondPartSize = (param->syncInfo.frameSize * 3 / 8);

      /* Compute CRC for the remaining bytes of the first 5/8 of the sync */
      if (skipBytes(bs, firstPartRemSize) < 0)
        return -1;

      if (completeCrcContext(crcCtxBitstream(bs), &crcResult) < 0)
        return -1;

      if (0x0 != crcResult)
        LIBBLU_AC3_ERROR_RETURN("CRC checksum error at the 5/8 of the frame.\n");

      /* Compute CRC for the remaining bytes */
      if (initCrcContext(crcCtxBitstream(bs), AC3_CRC_PARAMS, 0) < 0)
        return -1;

      if (skipBytes(bs, secondPartSize) < 0)
        return -1;

      if (completeCrcContext(crcCtxBitstream(bs), &crcResult) < 0)
        return -1;

      if (0x0 != crcResult)
        LIBBLU_AC3_ERROR_RETURN("CRC checksum error at the end of the frame.\n");
    }
    else {
      /* Skipping remaining frame bytes with CRC disabled */
      size_t syncFrameRemSize =
        param->syncInfo.frameSize - (tellPos(bs) - startFrameOff)
      ;

      if (skipBytes(bs, syncFrameRemSize) < 0)
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
    if (_parseEac3SyncInfo(bs) < 0)
      return -1;

    if (useCrcCheck) {
      if (initCrcContext(crcCtxBitstream(bs), AC3_CRC_PARAMS, 0) < 0)
        return -1;
    }

    /* bsi() */
    if (_parseEac3BitStreamInfo(bs, param) < 0)
      return -1;

    /* Skipping remaining frame bytes */
    syncFrameRemSize =
      param->eac3BitstreamInfo.frameSize - (tellPos(bs) - startFrameOff)
    ;
    if (skipBytes(bs, syncFrameRemSize) < 0)
      return -1;

    if (useCrcCheck) {
      uint32_t crcResult;

      if (completeCrcContext(crcCtxBitstream(bs), &crcResult) < 0)
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
#endif

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

void _unpackMlpMajorSyncFormatInfo(
  MlpMajorSyncFormatInfo * dst,
  uint32_t value
)
{
  dst->value = value;

  *dst = (MlpMajorSyncFormatInfo) {
    .value = value,

    .audio_sampling_frequency = (value >> 28) & 0xF,
    .u6ch_multichannel_type   = (value >> 27) & 0x1,
    .u8ch_multichannel_type   = (value >> 26) & 0x1,

    .u2ch_presentation_channel_modifier   = (value >> 22) & 0x3,
    .u6ch_presentation_channel_modifier   = (value >> 20) & 0x3,
    .u6ch_presentation_channel_assignment = (value >> 15) & 0x1F,
    .u8ch_presentation_channel_modifier   = (value >> 13) & 0x3,
    .u8ch_presentation_channel_assignment = value & 0x1FFF,

    // .sample_rate = sampleRateMlpAudioSamplingFrequency((value >> 28) & 0xF),
    // .u6ch_presentation_channel_nb = getNbChannels6ChPresentationAssignment(
    //   (value >> 15) & 0x1F
    // ),
    // .u8ch_presentation_channel_nb = getNbChannels8ChPresentationAssignment(
    //   value & 0x1FFF
    // )
  };

#if 0
  /* [u4 audio_sampling_frequency] */
  dst->audio_sampling_frequency = (value >> 28) & 0xF;

  /* [v1 6ch_multichannel_type] */
  dst->u6ch_multichannel_type = (value >> 27) & 0x1;

  /* if (MLP_6CH_MULTICHANNEL_TYPE_RES <= param->u6ch_multichannel_type)
    LIBBLU_AC3_ERROR_RETURN(
      "Reserved value in use (6ch_multichannel_type == 0x%x).\n",
      param->u6ch_multichannel_type
    ); */

  /* [v1 8ch_multichannel_type] */
  dst->u8ch_multichannel_type = (value >> 26) & 0x1;

  /* if (MLP_8CH_MULTICHANNEL_TYPE_RES <= param->u8ch_multichannel_type)
    LIBBLU_AC3_ERROR_RETURN(
      "Reserved value in use (8ch_multichannel_type == 0x%x).\n",
      param->u8ch_multichannel_type
    ); */

  /* [v2 reserved] // ignored */

  /* [u2 2ch_presentation_channel_modifier] */
  dst->u2ch_presentation_channel_modifier = (value >> 22) & 0x3;

  /* [u2 6ch_presentation_channel_modifier] */
  dst->u6ch_presentation_channel_modifier = (value >> 20) & 0x3;

  /* [u5 6ch_presentation_channel_assignment] */
  dst->u6ch_presentation_channel_assignment = (value >> 15) & 0x1F;

  /* [u2 8ch_presentation_channel_modifier] */
  dst->u8ch_presentation_channel_modifier = (value >> 13) & 0x3;

  /* [u13 8ch_presentation_channel_assignment] */
  dst->u8ch_presentation_channel_assignment = value & 0x1FFF;

  dst->sampleRate = sampleRateMlpAudioSamplingFrequency(
    dst->audio_sampling_frequency
  );

  dst->u6ch_presentation_channel_nb = getNbChannels6ChPresentationAssignment(
    dst->u6ch_presentation_channel_assignment
  );

  dst->u8ch_presentation_channel_nb = getNbChannels8ChPresentationAssignment(
    dst->u8ch_presentation_channel_assignment
  );
#endif
}

static int _unpackMlpMajorSyncextSubstreamInfo(
  uint8_t value,
  MlpSubstreamInfo * si
)
{
  si->value = value;

  /* [v2 reserved] */
  si->reserved_field             = (value     ) & 0x3;

  /* [u2 6ch_presentation] */
  si->u6ch_presentation          = (value >> 2) & 0x3;

  /* [u3 8ch_presentation] */
  si->u8ch_presentation          = (value >> 4) & 0x7;

  /* [b1 16ch_presentation_present] */
  si->b16ch_presentation_present = (value >> 7) & 0x1;

  return 0;
}

/** \~english
 * \brief
 *
 * \param bs
 * \param ecm
 * \param substream_info
 * \param length Size of extra_channel_meaning_data() section in bits.
 * \return int
 */
static int _parseMlpExtraChannelMeaningData(
  BitstreamReaderPtr bs,
  MlpExtraChannelMeaningData * ecmd,
  MlpSubstreamInfo si,
  unsigned length
)
{
  unsigned ecm_used_length = 0;
  ecmd->type = MLP_EXTRA_CH_MEANING_CONTENT_UNKNOWN;

  if (si.b16ch_presentation_present) {
    /* 16ch_channel_meaning() */
    Mlp16ChChannelMeaning * v16ch_cm = &ecmd->content.v16ch_channel_meaning;

    ecmd->type = MLP_EXTRA_CH_MEANING_CONTENT_16CH_MEANING;

    /* [u5 16ch_dialogue_norm] */
    READ_BITS(&v16ch_cm->u16ch_dialogue_norm, bs, 5, return -1);

    /* [u6 16ch_mix_level] */
    READ_BITS(&v16ch_cm->u16ch_mix_level, bs, 6, return -1);

    /* [u5 16ch_channel_count] */
    READ_BITS(&v16ch_cm->u16ch_channel_count, bs, 5, return -1);

    /* [b1 dyn_object_only] */
    READ_BITS(&v16ch_cm->dyn_object_only, bs, 1, return -1);

    if (v16ch_cm->dyn_object_only) {
      /* [b1 lfe_present] */
      READ_BITS(&v16ch_cm->lfe_present, bs, 1, return -1);
      ecm_used_length = 18;
    }
    else {
      // TODO: Implement (!dyn_object_only)
      LIBBLU_AC3_ERROR_RETURN(
        "16ch_channel_meaning() with multiple type content not supported yet, "
        "please send sample.\n"
      );
    }
  }

  /* Check extension size. */
  if (length < ecm_used_length)
    LIBBLU_AC3_ERROR_RETURN(
      "Too many bits in extra_channel_meaning_data "
      "(expect %u bits, got %u).\n",
      length, ecm_used_length
    );

  /* [vn reserved] */
  unsigned size = ecmd->reserved_size = length - ecm_used_length;
  if (readBytes(bs, ecmd->reserved, size >> 3) < 0)
    return -1;
  READ_BITS(&ecmd->reserved[size >> 3], bs, size & 0x7, return -1);

  return 0;
}

static int _parseMlpChannelMeaning(
  BitstreamReaderPtr bs,
  MlpChannelMeaning * param,
  MlpSubstreamInfo si
)
{

  /* [v6 reserved] */
  READ_BITS(&param->reserved_field, bs, 6, return -1);

  /* [b1 2ch_control_enabled] */
  READ_BITS(&param->b2ch_control_enabled, bs, 1, return -1);

  /* [b1 6ch_control_enabled] */
  READ_BITS(&param->b6ch_control_enabled, bs, 1, return -1);

  /* [b1 8ch_control_enabled] */
  READ_BITS(&param->b8ch_control_enabled, bs, 1, return -1);

  /* [v1 reserved] */
  READ_BITS(&param->reserved_bool_1, bs, 1, return -1);

  /* [s7 drc_start_up_gain] */
  READ_BITS(&param->drc_start_up_gain, bs, 7, return -1);

  /* [u6 2ch_dialogue_norm] */
  READ_BITS(&param->u2ch_dialogue_norm, bs, 6, return -1);

  /* [u6 2ch_mix_level] */
  READ_BITS(&param->u2ch_mix_level, bs, 6, return -1);

  /* [u5 6ch_dialogue_norm] */
  READ_BITS(&param->u6ch_dialogue_norm, bs, 5, return -1);

  /* [u6 6ch_mix_level] */
  READ_BITS(&param->u6ch_mix_level, bs, 6, return -1);

  /* [u5 6ch_source_format] */
  READ_BITS(&param->u6ch_source_format, bs, 5, return -1);

  /* [u5 8ch_dialogue_norm] */
  READ_BITS(&param->u8ch_dialogue_norm, bs, 5, return -1);

  /* [u6 8ch_mix_level] */
  READ_BITS(&param->u8ch_mix_level, bs, 6, return -1);

  /* [u6 8ch_source_format] */
  READ_BITS(&param->u8ch_source_format, bs, 6, return -1);

  /* [v1 reserved] */
  if (readBit(bs, &param->reserved_bool_2) < 0)
    return -1;

  /* [b1 extra_channel_meaning_present] */
  if (readBit(bs, &param->extra_channel_meaning_present) < 0)
    return -1;
  param->extra_channel_meaning_length = 0;

  if (param->extra_channel_meaning_present) {
    MlpExtraChannelMeaningData * ecmd = &param->extra_channel_meaning_data;

    /* [u4 extra_channel_meaning_length] */
    READ_BITS(&param->extra_channel_meaning_length, bs, 4, return -1);

    /* [vn extra_channel_meaning_data] */
    unsigned length = ((param->extra_channel_meaning_length + 1) << 4) - 4;
    if (_parseMlpExtraChannelMeaningData(bs, ecmd, si, length) < 0)
      return -1;

    /* [vn padding] */
    if (paddingBoundary(bs, 16) < 0)
      return -1;
  }

  return 0;
}

static void _unpackMlpMajorSyncFlags(
  MlpMajorSyncFlags * dst,
  uint16_t value
)
{
  /** [v16 flags]
   * -> b1:  constantFifoBufDelay;
   * -> v3:  reserved;
   * -> b1:  formatInfoAlternative8chAssSyntax;
   * -> v11: reserved.
   *
   * NOTE: reserved fields are ignored.
   */
  *dst = (MlpMajorSyncFlags) {
    .value = value,

    .constant_fifo_buf_delay          = (value >> 15) & 0x1,
    .fmt_info_alter_8ch_asgmt_syntax  = (value >> 11) & 0x1
  };
}

static int _parseMlpMajorSyncInfo(
  BitstreamReaderPtr bs,
  MlpMajorSyncInfoParameters * param
)
{

  initCrcContext(crcCtxBitstream(bs), TRUE_HD_MS_CRC_PARAMS, 0);

  /* [v32 format_sync] */
  READ_BITS(&param->format_sync, bs, 32, return -1);

  if (MLP_SYNCWORD == param->format_sync) {
    LIBBLU_AC3_ERROR_RETURN(
      "Unexpected DVD Audio MLP format sync word "
      "(format_sync == 0x%08X).\n",
      MLP_SYNCWORD
    );
  }
  else if (TRUE_HD_SYNCWORD != param->format_sync) {
    LIBBLU_AC3_ERROR_RETURN(
      "Unexpected sync word in MLP Major Sync "
      "(format_sync == 0x%08" PRIX32 ").\n",
      param->format_sync
    );
  }

  /* [v32 format_info] */
  uint32_t format_info_value;
  READ_BITS(&format_info_value, bs, 32, return -1);
  /* Unpacking is delayed to flags field parsing */

  /* [v16 signature] */
  READ_BITS(&param->signature, bs, 16, return -1);

  if (TRUE_HD_SIGNATURE != param->signature) {
    LIBBLU_AC3_ERROR_RETURN(
      "Invalid MLP Major Sync signature word "
      "(signature == 0x%04" PRIX32 ", expect 0x%04X).\n",
      param->signature,
      TRUE_HD_SIGNATURE
    );
  }

  /* [v16 flags] */
  uint16_t flags_value;
  READ_BITS(&flags_value, bs, 16, return -1);
  _unpackMlpMajorSyncFlags(&param->flags, flags_value);

  /* Unpack format_info field using flags */
  _unpackMlpMajorSyncFormatInfo(&param->format_info, format_info_value);

  /* [v16 reserved] // ignored */
  READ_BITS(&param->reserved_field_1, bs, 16, return -1);

  /* [b1 variable_rate] */
  READ_BITS(&param->variable_bitrate, bs, 1, return -1);

  /* [u15 peak_data_rate] */
  READ_BITS(&param->peak_data_rate, bs, 15, return -1);

  /* [u4 substreams] */
  READ_BITS(&param->substreams, bs, 4, return -1);

  /* [v2 reserved] // ignored */
  READ_BITS(&param->reserved_field_2, bs, 2, return -1);

  /* [u2 extended_substream_info] */
  READ_BITS(&param->extended_substream_info, bs, 2, return -1);

  /* [v8 substream_info] */
  uint8_t substream_info_value;
  READ_BITS(&substream_info_value, bs, 8, return -1);
  if (_unpackMlpMajorSyncextSubstreamInfo(substream_info_value, &param->substream_info) < 0)
    return -1;

  /* [v64 channel_meaning()] */
  if (_parseMlpChannelMeaning(bs, &param->channel_meaning, param->substream_info) < 0)
    return -1;

  uint16_t crc_result = completeCrcContext(crcCtxBitstream(bs));

  /* [u16 major_sync_info_CRC] */
  READ_BITS(&param->major_sync_info_CRC, bs, 16, return -1);

  if (crc_result != param->major_sync_info_CRC)
    LIBBLU_AC3_ERROR_RETURN(
      "Unexpected Major Sync CRC value result.\n"
    );

  return 0;
}

#if 0
static bool _constantMlpMajorSyncChannelMeaningCheck(
  const MlpChannelMeaning * first,
  const MlpChannelMeaning * second
)
{
  return CHECK(
    EQUAL(->b2ch_control_enabled)
    EQUAL(->b6ch_control_enabled)
    EQUAL(->b8ch_control_enabled)
    EQUAL(->drc_start_up_gain)
    EQUAL(->u2ch_dialogue_norm)
    EQUAL(->u2ch_mix_level)
    EQUAL(->u6ch_dialogue_norm)
    EQUAL(->u6ch_mix_level)
    EQUAL(->u6ch_source_format)
    EQUAL(->u8ch_dialogue_norm)
    EQUAL(->u8ch_mix_level)
    EQUAL(->u8ch_source_format)
    EQUAL(->extra_channel_meaning_present)
    START_COND(->extra_channel_meaning_present, 0x1)
      SUB_FUN_PTR(->extra_channel_meaning, constantMlpExtraChannelMeaningCheck)
    END_COND
  );
}

static bool _constantMlpMajorSyncCheck(
  const MlpMajorSyncInfoParameters * first,
  const MlpMajorSyncInfoParameters * second
)
{
  return CHECK(
    EQUAL(->major_sync_info_CRC)
    EQUAL(->format_info.value)
    EQUAL(->flags.value)
    EQUAL(->variable_bitrate)
    EQUAL(->peak_data_rate)
    EQUAL(->substreams)
    EQUAL(->extended_substream_info)
    EQUAL(->substream_info.b16ch_presentation_present)
    EQUAL(->substream_info.u8ch_presentation)
    EQUAL(->substream_info.u6ch_presentation)
    SUB_FUN_PTR(->channel_meaning, _constantMlpMajorSyncChannelMeaningCheck)
  );
}
#endif

/** \~english
 * \brief
 *
 * \param buf
 * \param value
 * \param size Field size in bits, must be between 0-32 and a multiple of 4.
 */
static inline void _applyNibbleXorMlp(
  uint8_t * parity,
  uint32_t value,
  size_t size
)
{
  assert(NULL != parity);
  assert(size <= 32 && 0 == (size % 4));

  for (; 0 < size; size -= 4) {
    *parity ^= (value >> (size - 4)) & 0xF;
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

/** \~english
 * \brief
 *
 * \param bs
 * \param sh
 * \param cnv Check nibble value.
 * \return int
 */
static int _parseMlpSyncHeader(
  BitstreamReaderPtr bs,
  MlpSyncHeaderParameters * sh,
  uint8_t * cnv
)
{

  /* [v4 check_nibble] */
  READ_BITS_NIBBLE_MLP(
    &sh->check_nibble, bs, 4,
    cnv, return -1
  );

  /* [u12 access_unit_length] */
  READ_BITS_NIBBLE_MLP(
    &sh->access_unit_length, bs, 12,
    cnv, return -1
  );

  /* [u16 input_timing] */
  READ_BITS_NIBBLE_MLP(
    &sh->input_timing, bs, 16,
    cnv, return -1
  );

  /* Check if current frame contain Major Sync : */
  bool major_sync = ((nextUint32(bs) >> 8) == MLP_SYNCWORD_PREFIX);
  sh->major_sync = major_sync;

  if (major_sync) {
    LIBBLU_MLP_DEBUG_PARSING_HDR(
      "  Major Sync present.\n"
    );

    if (_parseMlpMajorSyncInfo(bs, &sh->major_sync_info) < 0)
      return -1; /* Error happen during decoding. */
  }

  /* Computed values : */
  sh->accessUnitLength = 2 * sh->access_unit_length;

  return 0;
}

/** \~english
 * \brief Return the size in 16-bits words of the 'mlp_sync' section.
 *
 * \param param 'mlp_sync' syntax object.
 * \return unsigned Size in 16-bits words.
 */
static unsigned _getSizeMlpSyncHeader(
  const MlpSyncHeaderParameters * param
)
{
  unsigned size = 2; // Minor sync

  if (param->major_sync) {
    const MlpChannelMeaning * chm = &param->major_sync_info.channel_meaning;
    size += 10 + 4; // Major sync info (plus channel_meaning())
    if (chm->extra_channel_meaning_present)
      size += 1 + chm->extra_channel_meaning_length;
  }

  return size;
}

#define MLP_6CH_CHASSIGN_STR_BUFFSIZE  48

static void _buildMlp6ChPresChAssignStr(
  char dst[static MLP_6CH_CHASSIGN_STR_BUFFSIZE],
  uint8_t u6ch_presentation_channel_assignment
)
{
  static const char * channels[] = {
    "L, R",
    "C",
    "LFE",
    "Ls, Rs",
    "Tfl, Tfr"
  };

  char * sep = "";
  for (unsigned i = 0; i < 5; i++) {
    if (u6ch_presentation_channel_assignment & (1 << i)) {
      lb_str_cat(&dst, sep);
      lb_str_cat(&dst, channels[i]);
      sep = ", ";
    }
  }
}

#define MLP_8CH_CHASSIGN_STR_BUFFSIZE  130

static void _buildMlp8ChPresChAssignStr(
  char dst[static MLP_8CH_CHASSIGN_STR_BUFFSIZE],
  uint8_t u8ch_presentation_channel_assignment,
  bool alternateSyntax
)
{
  static const char * channels[] = {
    "L, R",
    "C",
    "LFE",
    "Ls, Rs",
    "Tfl, Tfr",
    "Lsc, Rsc",
    "Lb, Rb",
    "Cb",
    "Tc",
    "Lsd, Rsd",
    "Lw, Rw",
    "Tfc",
    "LFE2"
  };

  char * sep = "";
  for (unsigned i = 0; i < ((alternateSyntax) ? 4 : 13); i++) {
    if (u8ch_presentation_channel_assignment & (1 << i)) {
      lb_str_cat(&dst, sep);
      lb_str_cat(&dst, channels[i]);
      sep = ", ";
    }
  }
  if (alternateSyntax && (u8ch_presentation_channel_assignment & (1 << 4))) {
    lb_str_cat(&dst, sep);
    lb_str_cat(&dst, "Tsl, Tsr");
  }
}

static int _checkMlpMajorSyncFormatInfo(
  const MlpMajorSyncFormatInfo * fi,
  const MlpMajorSyncFlags * flags,
  MlpInformations * info
)
{

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    Sampling frequency (audio_sampling_frequency): 0x%" PRIX8 ".\n",
    fi->audio_sampling_frequency
  );
  unsigned sampling_frequency = sampleRateMlpAudioSamplingFrequency(
    fi->audio_sampling_frequency
  );
  info->sampling_frequency = sampling_frequency;
  LIBBLU_MLP_DEBUG_PARSING_MS(
    "     -> %u Hz.\n",
    sampling_frequency
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    6-ch multichannel type (6ch_multichannel_type): 0b%x.\n",
    fi->u6ch_multichannel_type
  );
  LIBBLU_MLP_DEBUG_PARSING_MS(
    "     -> %s.\n",
    Mlp6ChMultichannelTypeStr(fi->u6ch_multichannel_type)
  );
  if (fi->u6ch_multichannel_type == MLP_6CH_MULTICHANNEL_TYPE_RES) {
    LIBBLU_MLP_ERROR_RETURN(
      "Reserved value in use (6ch_multichannel_type == 0x%x).",
      fi->u6ch_multichannel_type
    );
  }

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    8-ch multichannel type (8ch_multichannel_type): 0b%x.\n",
    fi->u8ch_multichannel_type
  );
  LIBBLU_MLP_DEBUG_PARSING_MS(
    "     -> %s.\n",
    Mlp8ChMultichannelTypeStr(fi->u8ch_multichannel_type)
  );
  if (fi->u8ch_multichannel_type == MLP_8CH_MULTICHANNEL_TYPE_RES) {
    LIBBLU_MLP_ERROR_RETURN(
      "Reserved value in use (8ch_multichannel_type == 0x%x).",
      fi->u8ch_multichannel_type
    );
  }

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    2-ch presentation channels assignment/content type "
    "(2ch_presentation_channel_modifier): 0x%x.\n",
    fi->u2ch_presentation_channel_modifier
  );
  LIBBLU_MLP_DEBUG_PARSING_MS(
    "     -> %s.\n",
    Mlp2ChPresentationChannelModifierStr(
      fi->u2ch_presentation_channel_modifier
    )
  );

  Mlp2ChPresentationChannelModifier u2ch_pres_ch_mod =
    fi->u2ch_presentation_channel_modifier;
  if (MLP_2CH_PRES_CH_MOD_LT_RT == u2ch_pres_ch_mod)
    info->matrix_surround = MLP_INFO_MS_ENCODED;
  if (MLP_2CH_PRES_CH_MOD_LBIN_RBIN == u2ch_pres_ch_mod)
    info->binaural_audio = true;
  if (MLP_2CH_PRES_CH_MOD_MONO == u2ch_pres_ch_mod)
    info->mono_audio = true;

  unsigned u6chNbCh = getNbChannels6ChPresentationAssignment(
    fi->u6ch_presentation_channel_assignment
  );
  bool b6chLsRsPres = lsRsPresent6ChPresentationAssignment(
    fi->u6ch_presentation_channel_assignment
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    6-ch presentation channels assignment/content type "
    "(6ch_presentation_channel_modifier): 0x%x.\n",
    fi->u6ch_presentation_channel_modifier
  );
  LIBBLU_MLP_DEBUG_PARSING_MS(
    "     -> %s.\n",
    Mlp6ChPresentationChannelModifierStr(
      fi->u6ch_presentation_channel_modifier,
      u6chNbCh,
      b6chLsRsPres
    )
  );

  if (
    2 != u6chNbCh
    && !b6chLsRsPres
    && 0 < fi->u6ch_presentation_channel_modifier
  ) {
    LIBBLU_MLP_WARNING(
      "Unexpected '6ch_presentation_channel_modifier' == 0x%" PRIX8 ", "
      "for a %u channels presentation without surround channels.\n",
      u6chNbCh
    );
  }

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    6-ch presentation channels assignment "
    "(6ch_presentation_channel_assignment): 0x%x.\n",
    fi->u6ch_presentation_channel_assignment
  );

  char s6ch_channels[MLP_6CH_CHASSIGN_STR_BUFFSIZE] = {'\0'};
  _buildMlp6ChPresChAssignStr(
    s6ch_channels,
    fi->u6ch_presentation_channel_assignment
  );
  LIBBLU_MLP_DEBUG_PARSING_MS(
    "     -> Channels: %s (%u channels).\n",
    s6ch_channels,
    u6chNbCh
  );

  if (0 == u6chNbCh || 6 < u6chNbCh) {
    LIBBLU_MLP_ERROR_RETURN(
      "Invalid number of channels for 6ch presentation (%u).\n",
      u6chNbCh
    );
  }
  info->nb_channels = u6chNbCh;

  unsigned u8chNbCh = getNbChannels8ChPresentationAssignment(
    fi->u8ch_presentation_channel_assignment,
    flags->fmt_info_alter_8ch_asgmt_syntax
  );
  bool b8chMainLRPres = mainLRPresent8ChPresentationAssignment(
    fi->u8ch_presentation_channel_assignment
  );
  bool b8chOnlyLsRsPres = onlyLsRsPresent8ChPresentationAssignment(
    fi->u8ch_presentation_channel_assignment
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    8-ch presentation channels assignment/content type "
    "(8ch_presentation_channel_modifier): 0x%x.\n",
    fi->u8ch_presentation_channel_modifier
  );
  LIBBLU_MLP_DEBUG_PARSING_MS(
    "     -> %s.\n",
    Mlp8ChPresentationChannelModifierStr(
      fi->u8ch_presentation_channel_modifier,
      u8chNbCh,
      b8chMainLRPres,
      b8chOnlyLsRsPres
    )
  );

  if (
    !((2 == u8chNbCh && b8chMainLRPres) || b8chOnlyLsRsPres)
    && 0 < fi->u6ch_presentation_channel_modifier
  ) {
    LIBBLU_MLP_WARNING(
      "Unexpected '8ch_presentation_channel_modifier' == 0x%" PRIX8 ", "
      "for a %u channels presentation without surround channels nor "
      "Main L/R channels.\n",
      u8chNbCh
    );
  }

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    8-ch presentation channels assignment "
    "(8ch_presentation_channel_assignment): 0x%x.\n",
    fi->u8ch_presentation_channel_assignment
  );

  char s8ch_channels[MLP_8CH_CHASSIGN_STR_BUFFSIZE] = {'\0'};
  _buildMlp8ChPresChAssignStr(
    s8ch_channels,
    fi->u8ch_presentation_channel_assignment,
    flags->fmt_info_alter_8ch_asgmt_syntax
  );
  LIBBLU_MLP_DEBUG_PARSING_MS(
    "     -> Channels: %s (%u channels).\n",
    s8ch_channels,
    u8chNbCh
  );

  if (8 < u8chNbCh) {
    LIBBLU_MLP_ERROR_RETURN(
      "Invalid number of channels for 8-ch presentation (%u).\n",
      u8chNbCh
    );
  }

  if (0 < u8chNbCh)
    info->nb_channels = u8chNbCh;
  else
    LIBBLU_MLP_WARNING(
      "Missing 8-ch presentation channels assignment mask.\n"
    );

  return 0;
}

static int _checkMlpSubstreamInfo(
  const MlpSubstreamInfo * si
)
{
  LIBBLU_MLP_DEBUG_PARSING_MS(
    "   Substreams carriage informations (substream_info): 0x%02" PRIX8 ".\n",
    si->value
  );

  if (0x0 != si->reserved_field)
    LIBBLU_MLP_DEBUG_PARSING_MS(
      "    WARNING Unexpected non-zero reserved field (reserved): 0x%x.\n",
      si->reserved_field
    );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    2-ch presentation carriage (*inferred*): Substream 0.\n"
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    6-ch presentation carriage (6ch_presentation): %s (0x%x).\n",
    MlpSubstreamInfo6ChPresentationStr(si->u6ch_presentation),
    si->u6ch_presentation
  );

  if (MLP_SS_INFO_6CH_PRES_RESERVED == si->u6ch_presentation)
    LIBBLU_MLP_ERROR_RETURN(
      "Reserved value in use, 'substream_info' bits 2-3 == 0x%x.\n",
      si->u6ch_presentation
    );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    8-ch presentation carriage (8ch_presentation): %s (0x%x).\n",
    MlpSubstreamInfo8ChPresentationStr(si->u8ch_presentation),
    si->u8ch_presentation
  );

  if (MLP_SS_INFO_8CH_PRES_RESERVED == si->u8ch_presentation)
    LIBBLU_MLP_ERROR_RETURN(
      "Reserved value in use, 'substream_info' bits 6-4 == 0x%x.\n",
      si->u8ch_presentation
    );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    16-ch presentation presence (16ch_presentation_present): %s (0x%x).\n",
    BOOL_STR(si->b16ch_presentation_present),
    si->b16ch_presentation_present
  );

  return 0;
}

static int _checkMlpSubstreamInfoCombination(
  const MlpSubstreamInfo * si,
  MlpExtendedSubstreamInfo esi
)
{
  static const unsigned valid_ranges[][4] = {
  // A  B  C  D      8ch_pres min A, max B / 6ch_pres min C, max D
    {4, 7, 1, 3}, // 16-ch pres in Substream 3
    {4, 4, 2, 3}, // 16-ch pres in Substreams 2 and 3
    {6, 6, 2, 2}, // 16-ch pres in Substreams 1, 2 and 3
    {7, 7, 3, 3}  // 16-ch pres in Substreams 0, 1, 2 and 3
  };

  int ret = 0;

  unsigned u8ch_p = si->u8ch_presentation;
  if (u8ch_p < valid_ranges[esi][0] || valid_ranges[esi][1] < u8ch_p) {
    LIBBLU_MLP_ERROR(
      "Invalid substream carriage information combination "
      "for 8-ch presentation.\n"
    );
    ret = -1;
  }

  unsigned u6ch_p = si->u6ch_presentation;
  if (u6ch_p < valid_ranges[esi][2] || valid_ranges[esi][3] < u6ch_p) {
    LIBBLU_MLP_ERROR(
      "Invalid substream carriage information combination "
      "for 6-ch presentation.\n"
    );
    ret = -1;
  }

  return ret;
}

static int _checkMlp16ChChannelMeaning(
  const Mlp16ChChannelMeaning * cm
)
{
  LIBBLU_MLP_DEBUG_PARSING_MS(
    "      16-ch dialogi\n"
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "      16-ch dialogue norm (16ch_dialogue_norm): "
    "-%" PRIu8 " LKFS (0x%02" PRIX8 ").\n",
    (0 < cm->u16ch_dialogue_norm) ? cm->u16ch_dialogue_norm : 31,
    cm->u16ch_dialogue_norm
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "      16-ch mix level (16ch_mix_level): %u dB (0x%02" PRIX8 ").\n",
    (unsigned) cm->u16ch_mix_level + 70,
    cm->u16ch_mix_level
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "      16-ch presentation number of channels (16ch_channel_count): "
    "%u channel(s) (0x%02" PRIX8 ").\n",
    cm->u16ch_channel_count + 1,
    cm->u16ch_channel_count
  );

  if (16 < cm->u16ch_channel_count + 1) {
    LIBBLU_MLP_ERROR_RETURN(
      "Number of channels for the 16-ch presentation exceed 16 channels (%u).",
      cm->u16ch_channel_count + 1
    );
  }

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "      Dynamic objects only 16-ch presentation (dyn_object_only): %s (0b%x).\n",
    BOOL_STR(cm->dyn_object_only),
    cm->dyn_object_only
  );

  if (cm->dyn_object_only) {
    LIBBLU_MLP_DEBUG_PARSING_MS(
      "      First channel contains LFE (lfe_present): %s (0b%x).\n",
      BOOL_STR(cm->lfe_present),
      cm->lfe_present
    );
  }
  else {
    LIBBLU_TODO(); // TODO
  }

  return 0;
}

static int _checkMlpExtraChannelMeaningData(
  const MlpExtraChannelMeaningData * ecmd
)
{

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "     Extra channel meaning data, extra_channel_meaning_data()\n"
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "      -> Identified type: %s.\n",
    MlpExtraChannelMeaningContentStr(ecmd->type)
  );

  switch (ecmd->type) {
    case MLP_EXTRA_CH_MEANING_CONTENT_UNKNOWN:
      break;
    case MLP_EXTRA_CH_MEANING_CONTENT_16CH_MEANING:
      if (_checkMlp16ChChannelMeaning(&ecmd->content.v16ch_channel_meaning) < 0)
        return -1;
  }

  char reserved_field[32*5] = {0};
  for (unsigned i = 0; i < ecmd->reserved_size; i += 8) {
    sprintf(&reserved_field[i*5], " 0x%02X", ecmd->reserved[i >> 3]);
  }

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "      Reserved field (reserved):%s.\n",
    reserved_field
  );

  return 0;
}

static int _checkMlpChannelMeaning(
  const MlpChannelMeaning * cm
)
{

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "   Presentation channels metadata, channel_meaning()\n"
  );

  if (0x0 != cm->reserved_field) {
    LIBBLU_MLP_DEBUG_PARSING_MS(
      "    WARNING Unexpected non-zero reserved field (reserved): "
      "0x%02" PRIX8 ".\n",
      cm->reserved_field
    );
  }

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    Enabled 2-ch control gain (2ch_control_enabled): %s (0b%x).\n",
    BOOL_STR(cm->b2ch_control_enabled),
    cm->b2ch_control_enabled
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    Enabled 6-ch control gain (6ch_control_enabled): %s (0b%x).\n",
    BOOL_STR(cm->b6ch_control_enabled),
    cm->b6ch_control_enabled
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    Enabled 8-ch control gain (8ch_control_enabled): %s (0b%x).\n",
    BOOL_STR(cm->b8ch_control_enabled),
    cm->b8ch_control_enabled
  );

  if (cm->reserved_bool_1) {
    LIBBLU_MLP_DEBUG_PARSING_MS(
      "    WARNING Unexpected non-zero reserved field (reserved): 0b%x.\n",
      cm->reserved_bool_1
    );
  }

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    DRC startup gain (drc_start_up_gain): %d (0x%02" PRIX8 ").\n",
    lb_sign_extend(cm->drc_start_up_gain, 7),
    cm->drc_start_up_gain
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    2-ch dialogue norm (2ch_dialogue_norm): "
    "-%" PRIu8 " LKFS (0x%02" PRIX8 ").\n",
    (0 < cm->u2ch_dialogue_norm) ? cm->u2ch_dialogue_norm : 31,
    cm->u2ch_dialogue_norm
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    2-ch mix level (2ch_mix_level): %u dB (0x%02" PRIX8 ").\n",
    (unsigned) cm->u2ch_mix_level + 70,
    cm->u2ch_mix_level
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    6-ch dialogue norm (6ch_dialogue_norm): "
    "-%" PRIu8 " LKFS (0x%02" PRIX8 ").\n",
    (0 < cm->u6ch_dialogue_norm) ? cm->u6ch_dialogue_norm : 31,
    cm->u6ch_dialogue_norm
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    6-ch mix level (6ch_mix_level): %u dB (0x%02" PRIX8 ").\n",
    (unsigned) cm->u6ch_mix_level + 70,
    cm->u6ch_mix_level
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    6-ch hierarchical source information (6ch_source_format): "
    "%u (0x%02" PRIX8 ").\n",
    cm->u6ch_source_format,
    cm->u6ch_source_format
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    8-ch dialogue norm (8ch_dialogue_norm): "
    "-%" PRIu8 " LKFS (0x%02" PRIX8 ").\n",
    (0 < cm->u8ch_dialogue_norm) ? cm->u8ch_dialogue_norm : 31,
    cm->u8ch_dialogue_norm
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    8-ch mix level (8ch_mix_level): %u dB (0x%02" PRIX8 ").\n",
    (unsigned) cm->u8ch_mix_level + 70,
    cm->u8ch_mix_level
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    8-ch hierarchical source information (8ch_source_format): "
    "%u (0x%02" PRIX8 ").\n",
    cm->u8ch_source_format,
    cm->u8ch_source_format
  );

  if (cm->reserved_bool_2) {
    LIBBLU_MLP_DEBUG_PARSING_MS(
      "    WARNING Unexpected non-zero reserved field (reserved): 0b%x.\n",
      cm->reserved_bool_2
    );
  }

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    Extra channel meaning (extra_channel_meaning_present): %s (0b%x).\n",
    BOOL_PRESENCE(cm->extra_channel_meaning_present),
    cm->extra_channel_meaning_present
  );

  if (cm->extra_channel_meaning_present) {
    LIBBLU_MLP_DEBUG_PARSING_MS(
      "     Extra channel meaning data length (extra_channel_meaning_length): "
      "%u words (0x%" PRIX8 ").\n",
      cm->extra_channel_meaning_length + 1,
      cm->extra_channel_meaning_length
    );

    if (_checkMlpExtraChannelMeaningData(&cm->extra_channel_meaning_data) < 0)
      return -1;
  }

  return 0;
}

static int _checkMlpMajorSyncInfo(
  const MlpMajorSyncInfoParameters * msi,
  MlpInformations * info
)
{

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "   Format synchronization word (format_sync): 0x%08" PRIX32 ".\n",
    msi->format_sync
  );
  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    -> %s.\n",
    MlpFormatSyncStr(msi->format_sync)
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "   Bitstream format informations (format_info): 0x%08" PRIX32 ".\n",
    msi->format_info.value
  );

  int ret = _checkMlpMajorSyncFormatInfo(
    &msi->format_info,
    &msi->flags,
    info
  );
  if (ret < 0)
    return -1;

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "   Format signature (signature): 0x%04" PRIX16 ".\n",
    msi->signature
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "   Informations flags (flags): 0x%04" PRIX16 ".\n",
    msi->flags.value
  );
  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    Constant FIFO delay: %s.\n",
    BOOL_STR(msi->flags.constant_fifo_buf_delay)
  );
  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    8ch presentation channel assignment modifier: %s.\n",
    BOOL_STR(msi->flags.fmt_info_alter_8ch_asgmt_syntax)
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "   Variable bitrate mode (variable_bitrate): %s (0b%x).\n",
    BOOL_STR(msi->variable_bitrate),
    msi->variable_bitrate
  );

  if (msi->variable_bitrate) {
    LIBBLU_MLP_DEBUG_PARSING_MS(
      "   Peak data-rate (peak_data_rate): 0x%04" PRIX16 ".\n",
      msi->peak_data_rate
    );
  }
  else {
    LIBBLU_MLP_DEBUG_PARSING_MS(
      "   Constant data-rate (peak_data_rate): 0x%04" PRIX16 ".\n",
      msi->peak_data_rate
    );
  }

  info->peak_data_rate = (msi->peak_data_rate * info->sampling_frequency) >> 4;

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    -> %u bps, %u kbps.\n",
    info->peak_data_rate,
    info->peak_data_rate / 1000
  );

  if (MLP_MAX_PEAK_DATA_RATE < info->peak_data_rate)
    LIBBLU_MLP_ERROR_RETURN(
      "Too high peak data rate %u bps (shall not exceed %u bps).\n",
      info->peak_data_rate,
      MLP_MAX_PEAK_DATA_RATE
    );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "   Number of substreams (substreams): %u.\n",
    msi->substreams
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "   16-ch presentation carriage (extended_substream_info): %s (0x%x).\n",
    MlpExtendedSubstreamInfoStr(
      msi->extended_substream_info,
      msi->substream_info.b16ch_presentation_present
    ),
    msi->extended_substream_info
  );

  const MlpExtendedSubstreamInfo esi = msi->extended_substream_info;

  if (msi->substream_info.b16ch_presentation_present && 0 != esi) {
    LIBBLU_MLP_WARNING(
      "Unexpected field in use, 'extended_substream_info' == 0x%x.\n",
      esi
    );
  }

  if (_checkMlpSubstreamInfo(&msi->substream_info) < 0)
    return -1;

  if (3 < msi->substreams) {
    if (_checkMlpSubstreamInfoCombination(&msi->substream_info, esi) < 0)
      LIBBLU_ERROR_RETURN(
        "Invalid 'substream_info'/'extended_substream_info' fields.\n"
      );

    if (msi->substream_info.b16ch_presentation_present)
      info->atmos = true;
  }

  if (_checkMlpChannelMeaning(&msi->channel_meaning) < 0)
    return -1;

  return 0;
}

static int _checkMlpSyncHeader(
  const MlpSyncHeaderParameters * sync,
  MlpInformations * info,
  bool firstAU
)
{

  LIBBLU_MLP_DEBUG_PARSING_HDR(
    "  Minor Sync check sum (check_nibble): 0x%" PRIX8 ".\n",
    sync->check_nibble
  );

  LIBBLU_MLP_DEBUG_PARSING_HDR(
    "  Access Unit length (access_unit_length): %u words (0x%03" PRIX16 ").\n",
    sync->access_unit_length,
    sync->access_unit_length
  );

  LIBBLU_MLP_DEBUG_PARSING_HDR(
    "   -> %u bytes.\n",
    sync->accessUnitLength
  );

  LIBBLU_MLP_DEBUG_PARSING_HDR(
    "  Decoding timestamp (input_timing): %u samples (0x%02" PRIX16 ").\n",
    sync->input_timing,
    sync->input_timing
  );

  if (firstAU && !sync->major_sync) {
    LIBBLU_MLP_ERROR_RETURN(
      "Invalid first Access Unit, missing Major Sync.\n"
    );
  }

  if (sync->major_sync) {
    LIBBLU_MLP_DEBUG_PARSING_HDR(
      "  Major Sync present.\n"
    );

    if (_checkMlpMajorSyncInfo(&sync->major_sync_info, info) < 0)
      return -1;
  }

  return 0;
}

static int _parseMlpSubstreamDirectoryEntry(
  BitstreamReaderPtr bs,
  MlpSubstreamDirectoryEntry * entry,
  uint8_t * chkn
)
{

  /** [v4 flags]
   * -> b1: extra_substream_word
   * -> b1: restart_nonexistent
   * -> b1: crc_present
   * -> v1: reserved
   */
  uint8_t flags;
  READ_BITS_NIBBLE_MLP(&flags, bs, 4, chkn, return -1);
  entry->extra_substream_word = flags & 0x8;
  entry->restart_nonexistent  = flags & 0x4;
  entry->crc_present          = flags & 0x2;
  entry->reserved_field_1     = flags & 0x1;

  /* [u12 substream_end_ptr] */
  READ_BITS_NIBBLE_MLP(&entry->substream_end_ptr, bs, 12, chkn, return -1);

  if (entry->extra_substream_word) {
    /** [v16 extra_substream_word]
     * -> s9: drc_gain_update
     * -> u3: drc_time_update
     * -> v4: reserved_field_2
     */
    uint16_t drc_fields;
    READ_BITS_NIBBLE_MLP(&drc_fields, bs, 16, chkn, return -1);
    entry->drc_gain_update  = (drc_fields >> 7);
    entry->drc_time_update  = (drc_fields >> 4) & 0x7;
    entry->reserved_field_2 = (drc_fields     ) & 0xF;
  }

  return 0;
}


static unsigned _computeLengthMlpSync(
  const MlpSyncHeaderParameters * mlp_sync_header,
  const MlpSubstreamDirectoryEntry * directory
)
{
  /* Compute access_unit headers size (in 16-bits words unit) */
  const MlpMajorSyncInfoParameters * msi = &mlp_sync_header->major_sync_info;

  unsigned au_hdr_length = _getSizeMlpSyncHeader(mlp_sync_header);
  for (unsigned i = 0; i < msi->substreams; i++)
    au_hdr_length += 1 + directory[i].extra_substream_word;

  return au_hdr_length;
}


/** \~english
 * \brief Check and compute substreams sizes from substream_directory().
 *
 * \param directory substream_directory() syntax object.
 * \param au_ss_length Access unit remaining size in 16-bits words.
 * \param is_major_sync Is the access unit a major sync.
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
static int _checkAndComputeSSSizesMlpSubstreamDirectory(
  MlpSubstreamDirectoryEntry * directory,
  const MlpMajorSyncInfoParameters * msi,
  unsigned au_ss_length,
  bool is_major_sync
)
{

  LIBBLU_MLP_DEBUG_PARSING_HDR(
    "  Substream directory, substream_directory\n"
  );

  unsigned substream_start = 0;
  for (unsigned i = 0; i < msi->substreams; i++) {
    MlpSubstreamDirectoryEntry * entry = &directory[i];

    LIBBLU_MLP_DEBUG_PARSING_HDR("   Substream %u:\n", i);
    LIBBLU_MLP_DEBUG_PARSING_HDR(
      "    Extra 16-bit word (extra_substream_word): %s (0b%x).\n",
      BOOL_PRESENCE(entry->extra_substream_word),
      entry->extra_substream_word
    );
    LIBBLU_MLP_DEBUG_PARSING_HDR(
      "    Restart header (restart_nonexistent): %s (0b%x).\n",
      BOOL_PRESENCE(!entry->restart_nonexistent),
      entry->restart_nonexistent
    );
    LIBBLU_MLP_DEBUG_PARSING_HDR(
      "    CRC and parity check (crc_present): %s (0b%x).\n",
      BOOL_PRESENCE(entry->crc_present),
      entry->crc_present
    );
    if (entry->reserved_field_1) {
      LIBBLU_MLP_DEBUG_PARSING_HDR(
        "    WARNING Unexpected non-zero reserved field (reserved): 0b1.\n"
      );
    }
    LIBBLU_MLP_DEBUG_PARSING_HDR(
      "    Substream end pointer offset (substream_end_ptr): "
      "%u (0x%03" PRIX16 ").\n",
      entry->substream_end_ptr,
      entry->substream_end_ptr
    );

    if (entry->extra_substream_word) {
      LIBBLU_MLP_DEBUG_PARSING_HDR(
        "    Dynamic range control, dynamic_range_control\n"
      );
      LIBBLU_MLP_DEBUG_PARSING_HDR(
        "     Dynamic range update (drc_gain_update): %d (0x%03" PRIX16 ").\n",
        lb_sign_extend(entry->drc_gain_update, 9),
        entry->drc_gain_update
      );
      LIBBLU_MLP_DEBUG_PARSING_HDR(
        "     Dynamic time update (drc_time_update): %u (0x%" PRIX8 ").\n",
        entry->drc_time_update,
        entry->drc_time_update
      );
      if (entry->reserved_field_2) {
        LIBBLU_MLP_DEBUG_PARSING_HDR(
          "     WARNING Unexpected non-zero reserved field (reserved): 0b1.\n"
        );
      }
    }

    if (!entry->restart_nonexistent ^ is_major_sync)
      LIBBLU_MLP_ERROR_RETURN(
        "Substream %u 'restart_nonexistent' shall be set to 0b%x since "
        "the access unit %s with a major sync.",
        i, !entry->restart_nonexistent,
        (is_major_sync) ? "starts" : "does not start"
      );

    if (au_ss_length < entry->substream_end_ptr)
      LIBBLU_MLP_ERROR_RETURN(
        "Substream %u 'substream_end_ptr' out of Access Unit (%u < %u).\n",
        i, au_ss_length, entry->substream_end_ptr
      );

    if (entry->substream_end_ptr <= substream_start)
      LIBBLU_MLP_ERROR_RETURN(
        "Substream %u 'substream_end_ptr' lower than previous one (%u < %u).\n",
        i, entry->substream_end_ptr, substream_start
      );

    entry->substreamSize = 2 * (entry->substream_end_ptr - substream_start);
    substream_start = entry->substream_end_ptr;
  }

  return 0;
}

typedef struct {
  uint8_t * buf;
  unsigned size;
  unsigned offset;
} MlpSubstreamBitsReader;

static int _initMlpSubstreamBitsReader(
  MlpSubstreamBitsReader * r,
  BitstreamReaderPtr bs,
  size_t ss_size,
  uint8_t * parity
)
{

  uint8_t * buffer;
  if (NULL == (buffer = (uint8_t *) malloc(ss_size)))
    LIBBLU_ERROR_RETURN("Memory allocation error.\n");
  r->buf     = buffer;
  r->size    = 8 * ss_size;
  r->offset  = 0;

  if (readBytes(bs, r->buf, ss_size) < 0)
    return -1;

  for (size_t i = 0; i < ss_size; i++)
    _applyNibbleXorMlp(parity, r->buf[i], 8);
  *parity ^= 0xA9; // XOR-ed with 0xA9 parity constant.

  return 0;
}

static unsigned _remainingBitsMlpSubstreamBitsReader(
  const MlpSubstreamBitsReader * r
)
{
  return r->size - r->offset;
}

static int _readMlpSubstreamBitsReader(
  MlpSubstreamBitsReader * r,
  uint32_t * value,
  unsigned size
)
{
  const uint8_t * buf = r->buf;
  unsigned off = r->offset;
  uint32_t result = 0x0;

  if (r->size < off + size)
    LIBBLU_ERROR_RETURN("Too many bits to unpack.\n");

  for (unsigned i = 0; i < size; i++, off++) {
    result = (result << 1) | ((buf[off >> 3] >> (7 - (off & 0x7))) & 0x1);
  }

  r->offset = off;
  *value = result;

  return 0;
}

static uint32_t _fetchBitsMlpSubstreamBitsReader(
  const MlpSubstreamBitsReader * r,
  unsigned size
)
{
  const uint8_t * buf = r->buf;
  unsigned off = r->offset;
  uint32_t result = 0x0;

  assert(size <= _remainingBitsMlpSubstreamBitsReader(r));

  for (unsigned i = 0; i < size; i++, off++) {
    result = (result << 1) | ((buf[off >> 3] >> (7 - (off & 0x7))) & 0x1);
  }

  return result;
}

static void _skipBitsMlpSubstreamBitsReader(
  MlpSubstreamBitsReader * r,
  unsigned size
)
{
  assert(size <= _remainingBitsMlpSubstreamBitsReader(r));
  r->offset += size;
}

static void _padBitsMlpSubstreamBitsReader(
  MlpSubstreamBitsReader * r
)
{
  unsigned padding_size = (~(r->offset - 1)) & 0xF;

  assert(padding_size <= _remainingBitsMlpSubstreamBitsReader(r));
  _skipBitsMlpSubstreamBitsReader(r, padding_size);
}

static uint8_t _computeRestartHeaderMlpSubstreamBitsReader(
  MlpSubstreamBitsReader * r,
  unsigned start_offset,
  unsigned end_offset
)
{
  const uint8_t * buf = r->buf;

  unsigned crc = 0x00;
  for (unsigned off = start_offset; off < end_offset; off++) {
    crc = (crc << 1) ^ ((buf[off >> 3] >> (7 - (off & 0x7))) & 0x1);
    if (crc & 0x100)
      crc ^= 0x11D;
  }

  return crc & 0xFF;
}

/** \~english
 * \brief MLP Huffman codebook look-up-table entry.
 *
 */
typedef struct {
  unsigned mask;  /**< Code word mask, binary mask of the code word size. */
  unsigned code;  /**< Huffman code word. */
  unsigned size;  /**< Code word size in bits. */
  int32_t value;  /**< Actual associated symbol. */
} MlpHuffmanLUTEntry;

#define MLP_HUFFMAN_LONGEST_CODE_SIZE  9

/** \~english
 * \brief MLP Huffman codebook look-up-table.
 *
 */
typedef struct {
  MlpHuffmanLUTEntry entries[18];
  unsigned book_size[MLP_HUFFMAN_LONGEST_CODE_SIZE+1];
} MlpHuffmanLUT;

static const MlpHuffmanLUT mlpHuffmanTables[3] = {
  { /* Huffman table 0, -7 - +10 */
    .entries = {
      {0x1C0, 0x040, 3,  -1},
      {0x1C0, 0x140, 3,   0},
      {0x1C0, 0x100, 3,  +1},
      {0x1C0, 0x180, 3,  +2},
      {0x1C0, 0x1C0, 3,  +3},
      {0x1C0, 0x0C0, 3,  +4},
      {0x1E0, 0x020, 4,  -2},
      {0x1E0, 0x0A0, 4,  +5},
      {0x1F0, 0x010, 5,  -3},
      {0x1F0, 0x090, 5,  +6},
      {0x1F8, 0x008, 6,  -4},
      {0x1F8, 0x088, 6,  +7},
      {0x1FC, 0x004, 7,  -5},
      {0x1FC, 0x084, 7,  +8},
      {0x1FE, 0x002, 8,  -6},
      {0x1FE, 0x082, 8,  +9},
      {0x1FF, 0x001, 9,  -7},
      {0x1FF, 0x081, 9, +10}
    },
    .book_size = {
      0, 0, 0, 6, 8, 10, 12, 14, 16, 18
    }
  },
  { /* Huffman table 1, -7 - +8 */
    .entries = {
      {0x180, 0x100, 2,   0},
      {0x180, 0x180, 2,  +1},
      {0x1C0, 0x040, 3,  -1},
      {0x1C0, 0x0C0, 3,  +2},
      {0x1E0, 0x020, 4,  -2},
      {0x1E0, 0x0A0, 4,  +3},
      {0x1F0, 0x010, 5,  -3},
      {0x1F0, 0x090, 5,  +4},
      {0x1F8, 0x008, 6,  -4},
      {0x1F8, 0x088, 6,  +5},
      {0x1FC, 0x004, 7,  -5},
      {0x1FC, 0x084, 7,  +6},
      {0x1FE, 0x002, 8,  -6},
      {0x1FE, 0x082, 8,  +7},
      {0x1FF, 0x001, 9,  -7},
      {0x1FF, 0x081, 9,  +8}
    },
    .book_size = {
      0, 0, 2, 4, 6, 8, 10, 12, 14, 16
    }
  },
  { /* Huffman table 2, -7 - +7 */
    .entries = {
      {0x100, 0x100, 1,   0},
      {0x1C0, 0x040, 3,  -1},
      {0x1C0, 0x0C0, 3,  +1},
      {0x1E0, 0x020, 4,  -2},
      {0x1E0, 0x0A0, 4,  +2},
      {0x1F0, 0x010, 5,  -3},
      {0x1F0, 0x090, 5,  +3},
      {0x1F8, 0x008, 6,  -4},
      {0x1F8, 0x088, 6,  +4},
      {0x1FC, 0x004, 7,  -5},
      {0x1FC, 0x084, 7,  +5},
      {0x1FE, 0x002, 8,  -6},
      {0x1FE, 0x082, 8,  +6},
      {0x1FF, 0x001, 9,  -7},
      {0x1FF, 0x081, 9,  +7}
    },
    .book_size = {
      0, 1, 1, 3, 5, 7, 9, 11, 13, 15
    }
  }
};

static int _decodeVlcMlpSubstreamBitsReader(
  MlpSubstreamBitsReader * r,
  const MlpHuffmanLUT * huffman_book_lut,
  int32_t * value
)
{
  /* https://commandlinefanatic.com/cgi-bin/showarticle.cgi?article=art007 */
  const MlpHuffmanLUTEntry * entries = huffman_book_lut->entries;

  /* Compute Huffman VLC maximum size */
  unsigned max_code_size = _remainingBitsMlpSubstreamBitsReader(r);
  if (0 == max_code_size)
    LIBBLU_MLP_ERROR_RETURN(
      "Too many bits to unpack from substream, "
      "unable to decode block properly.\n"
    );
  if (MLP_HUFFMAN_LONGEST_CODE_SIZE < max_code_size)
    max_code_size = MLP_HUFFMAN_LONGEST_CODE_SIZE; // Constrain to maximum code size

  /* Constrain LUT size to the maximum code size */
  unsigned lut_size = huffman_book_lut->book_size[max_code_size];
  /* Pick but do not read next bits of data */
  uint32_t input = _fetchBitsMlpSubstreamBitsReader(r, max_code_size);
  input <<= (MLP_HUFFMAN_LONGEST_CODE_SIZE - max_code_size);

  // LIBBLU_MLP_DEBUG_PARSING_SS("%u %u 0x%X\n", lut_size, max_code_size, input);

  for (unsigned i = 0; i < lut_size; i++) {
    if ((input & entries[i].mask) == entries[i].code) {
      /* Huffman code match! */
      _skipBitsMlpSubstreamBitsReader(r, entries[i].size);
      *value = entries[i].value;
      return 0;
    }
  }

  LIBBLU_MLP_ERROR_RETURN(
    "Unable to decode a valid huffman code word in block data "
    "(input: 0x%03" PRIX32 ", max_code_size: %u, lut_size: %u).\n",
    input, max_code_size, lut_size
  );
}

static void _cleanMlpSubstreamBitsReader(
  MlpSubstreamBitsReader reader
)
{
  free(reader.buf);
}

#define READ_BITS_MLP_SUBSTR(d, r, s, e)                                      \
  do {                                                                        \
    uint32_t _val;                                                            \
    if (_readMlpSubstreamBitsReader(r, &_val, s) < 0)                         \
      e;                                                                      \
    *(d) = _val;                                                              \
  } while (0)

static int _isValidMlpRestartSyncWord(
  uint16_t restart_sync_word,
  unsigned ss_idx
)
{
  uint16_t allowed_range[][2] = {
    {0x31EA, 0x31EA},  // 0: 0x31EA
    {0x31EA, 0x31EB},  // 1: 0x31EA-0x31EB
    {0x31EB, 0x31EB},  // 2: 0x31EB
    {0x31EC, 0x31EC}   // 3: 0x31EC
  };

  return (
    allowed_range[ss_idx][0] <= restart_sync_word
    && restart_sync_word <= allowed_range[ss_idx][1]
  );
}

static int _decodeMlpRestartHeader(
  MlpSubstreamBitsReader * reader,
  MlpRestartHeader * restart_hdr,
  unsigned ss_idx
)
{
  LIBBLU_MLP_DEBUG_PARSING_SS(
    "       Restart header, restart_header()\n"
  );

  unsigned start_offset = reader->offset;

  /* [v14 restart_sync_word] */
  uint16_t restart_sync_word;
  READ_BITS_MLP_SUBSTR(&restart_sync_word, reader, 14, return -1);
  restart_hdr->restart_sync_word = restart_sync_word;
  restart_hdr->noise_type = restart_sync_word & MLP_RH_SW_NOISE_TYPE_MASK;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        Restart header sync word (restart_sync_word): 0x%04" PRIX16 ".\n",
    restart_sync_word
  );

  if (restart_sync_word < 0x31EA || 0x31EC < restart_sync_word) {
    LIBBLU_MLP_ERROR_RETURN(
      "Unknown 'restart_sync_word' value 0x%04" PRIX16 ".\n",
      restart_sync_word
    );
  }

  if (!_isValidMlpRestartSyncWord(restart_sync_word, ss_idx))
    LIBBLU_MLP_ERROR_RETURN(
      "Unexpected 'restart_sync_word' value 0x%04" PRIX16 " "
      "for substream %u.\n",
      restart_sync_word,
      ss_idx
    );

  /* [u16 output_timing] */
  uint16_t output_timing;
  READ_BITS_MLP_SUBSTR(&output_timing, reader, 16, return -1);

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        Output timing (output_timing): %u samples (0x%04" PRIX16 ").\n",
    output_timing,
    output_timing
  );

  /* [u4 min_chan] */
  uint8_t min_chan;
  READ_BITS_MLP_SUBSTR(&min_chan, reader, 4, return -1);
  restart_hdr->min_chan = min_chan;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        TODO (min_chan): "
    "%u channels (0x%" PRIX8 ").\n",
    min_chan + 1,
    min_chan
  );

  /* [u4 max_chan] */
  uint8_t max_chan;
  READ_BITS_MLP_SUBSTR(&max_chan, reader, 4, return -1);
  restart_hdr->max_chan = max_chan;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        TODO (max_chan): "
    "%u channels (0x%" PRIX8 ").\n",
    max_chan + 1,
    max_chan
  );

  // TODO: Check 'max_chan' according to ss carriage.

  if (max_chan <= min_chan)
    LIBBLU_MLP_ERROR_RETURN(
      "Maximum number of substream carried channels is equal or lower "
      "than the minimum (%u <= %u).\n",
      max_chan + 1,
      min_chan + 1
    );

  /* [u4 max_matrix_chan] */
  uint8_t max_matrix_chan;
  READ_BITS_MLP_SUBSTR(&max_matrix_chan, reader, 4, return -1);
  restart_hdr->max_matrix_chan = max_matrix_chan;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        Maximum number of substream carried matrix channels "
    "(max_matrix_chan): %u channels (0x%" PRIX8 ").\n",
    max_matrix_chan + 1,
    max_matrix_chan
  );

  // TODO: Check 'max_matrix_chan'.

  /* [u4 dither_shift] */
  uint8_t dither_shift;
  READ_BITS_MLP_SUBSTR(&dither_shift, reader, 4, return -1);

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        Dithering noise left-shift (dither_shift): %u (0x%" PRIX8 ").\n",
    dither_shift,
    dither_shift
  );

  /* [u23 dither_seed] */
  uint32_t dither_seed;
  READ_BITS_MLP_SUBSTR(&dither_seed, reader, 23, return -1);

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        Dithering noise seed (dither_seed): %u (0x%03" PRIX32 ").\n",
    dither_seed,
    dither_seed
  );

  /* [s4 max_shift] */
  uint8_t max_shift;
  READ_BITS_MLP_SUBSTR(&max_shift, reader, 4, return -1);

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        TODO (max_shift): %d (0x%" PRIX8 ").\n",
    lb_sign_extend(max_shift, 4),
    max_shift
  );

  /* [u5 max_lsbs] */
  uint8_t max_lsbs;
  READ_BITS_MLP_SUBSTR(&max_lsbs, reader, 5, return -1);

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        Maximum bit width of LSBs (max_lsbs): %u (0x%" PRIX8 ").\n",
    max_lsbs,
    max_lsbs
  );

  /* [u5 max_bits] */
  uint8_t max_bits;
  READ_BITS_MLP_SUBSTR(&max_bits, reader, 5, return -1);

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        TODO (max_bits): %u (0x%" PRIX8 ").\n",
    max_bits,
    max_bits
  );

  /* [u5 max_bits] */
  // uint8_t max_bits;
  READ_BITS_MLP_SUBSTR(&max_bits, reader, 5, return -1);

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        TODO (max_bits): %u (0x%" PRIX8 ").\n",
    max_bits,
    max_bits
  );

  /* [b1 error_protect] */
  bool error_protect;
  READ_BITS_MLP_SUBSTR(&error_protect, reader, 1, return -1);
  restart_hdr->error_protect = error_protect;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        TODO (error_protect): %s (0b%x).\n",
    BOOL_STR(error_protect),
    error_protect
  );

  /* [u8 lossless_check] */
  bool lossless_check;
  READ_BITS_MLP_SUBSTR(&lossless_check, reader, 8, return -1);

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        TODO (lossless_check): 0x%02" PRIX8 ".\n",
    lossless_check
  );

  /* [v16 reserved] */
  uint16_t reserved;
  READ_BITS_MLP_SUBSTR(&reserved, reader, 16, return -1);

  if (0x0000 != reserved) {
    LIBBLU_MLP_DEBUG_PARSING_SS(
      "        WARNING Unexpected non-zero reserved field (reserved): "
      "0x%04" PRIX16 ".\n",
      reserved
    );
  }

  for (unsigned ch = 0; ch <= max_matrix_chan; ch++) {
    /* [u6 ch_assign[ch]] */
    uint8_t ch_assign;
    READ_BITS_MLP_SUBSTR(&ch_assign, reader, 6, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "        Channel assignment for matrix channel %u (ch_assign[%u]): "
      "0x%02" PRIX8 ".\n",
      ch, ch, ch_assign
    );
  }

  uint8_t computed_crc = _computeRestartHeaderMlpSubstreamBitsReader(
    reader, start_offset, reader->offset
  );

  /* [u8 restart_header_CRC] */
  uint8_t restart_header_CRC;
  READ_BITS_MLP_SUBSTR(&restart_header_CRC, reader, 8, return -1);

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        TODO (restart_header_CRC): 0x%02" PRIX8 ".\n",
    restart_header_CRC
  );

  if (restart_header_CRC != computed_crc)
    LIBBLU_MLP_ERROR_RETURN("Unexpected restart header CRC checksum.\n");

  return 0;
}

static void _defaultMlpSubstreamParameters(
  MlpSubstreamParameters * ss_param
)
{
  ss_param->block_header_content = MLP_BHC_DEFAULT; // 0xFF, full header.
  ss_param->block_size = 8; // 8 PCM samples
  ss_param->matrix_parameters.num_primitive_matrices = 0; // No matrix
  memset(ss_param->quant_step_size, 0x00, sizeof(ss_param->quant_step_size));

  unsigned min_chan = ss_param->restart_header.min_chan;
  unsigned max_chan = ss_param->restart_header.max_chan;
  for (unsigned ch = min_chan; ch <= max_chan; ch++) {
    MlpChannelParameters * ch_param = &ss_param->channels_parameters[ch];
    ch_param->huffman_codebook = 0x0; // TODO
    ch_param->num_huffman_lsbs = 24; // 24 bits.
  }
}

static int _decodeMlpMatrixParameters(
  MlpSubstreamBitsReader * reader,
  MlpMatrixParameters * matrix_param,
  const MlpRestartHeader * restart_hdr
)
{
  bool noise_type = restart_hdr->noise_type;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        Matrix parameters, matrix_parameters()\n"
  );

  /* [u4 num_primitive_matrices] */
  unsigned num_primitive_matrices;
  READ_BITS_MLP_SUBSTR(&num_primitive_matrices, reader, 4, return -1);
  matrix_param->num_primitive_matrices = num_primitive_matrices;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "         TODO (num_primitive_matrices): %u (0x%X).\n",
    num_primitive_matrices,
    num_primitive_matrices
  );

  for (unsigned mat = 0; mat < num_primitive_matrices; mat++) {
    MlpMatrix * mat_param = &matrix_param->matrices[mat];

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "          Primitive matrix configuration %u:\n",
      mat
    );

    /* [u4 matrix_output_chan[mat]] */
    uint8_t matrix_output_chan;
    READ_BITS_MLP_SUBSTR(&matrix_output_chan, reader, 4, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "           TODO (matrix_output_chan[%u]): 0x%" PRIX8 ".\n",
      mat, matrix_output_chan
    );

    /* [u4 num_frac_bits] */
    unsigned num_frac_bits;
    READ_BITS_MLP_SUBSTR(&num_frac_bits, reader, 4, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "           TODO (num_frac_bits): %u bit(s).\n",
      num_frac_bits
    );

    /* [b1 lsb_bypass_exists[mat]] */
    bool lsb_bypass_exists;
    READ_BITS_MLP_SUBSTR(&lsb_bypass_exists, reader, 1, return -1);
    mat_param->lsb_bypass_exists = lsb_bypass_exists;

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "           TODO (lsb_bypass_exists[%u]): %s (0b%x).\n",
      mat, BOOL_STR(lsb_bypass_exists),
      lsb_bypass_exists
    );

    unsigned max_nb_channels = restart_hdr->max_chan;
    if (!noise_type)
      max_nb_channels += 2; // TODO: +2 noise channels?

    for (unsigned ch = 0; ch <= max_nb_channels; ch++) {
      /* [b1 matrix_coeff_present[mat][ch]] */
      bool matrix_coeff_present;
      READ_BITS_MLP_SUBSTR(&matrix_coeff_present, reader, 1, return -1);

      LIBBLU_MLP_DEBUG_PARSING_SS(
        "           TODO (matrix_coeff_present[%u][%u]): %s (0b%x).\n",
        mat, ch, BOOL_STR(matrix_coeff_present),
        matrix_coeff_present
      );

      if (matrix_coeff_present) {
        /* [s(2+num_frac_bits) coeff_value[mat][ch]] */
        unsigned coeff_value;
        READ_BITS_MLP_SUBSTR(&coeff_value, reader, 2+num_frac_bits, return -1);

        LIBBLU_MLP_DEBUG_PARSING_SS(
          "            TODO (coeff_value[%u][%u]): %d (0x%X).\n",
          mat, ch, lb_sign_extend(coeff_value, 2+num_frac_bits),
          coeff_value
        );
      }
    }

    if (noise_type) {
      /* [u4 matrix_noise_shift[mat]] */
      uint8_t matrix_noise_shift;
      READ_BITS_MLP_SUBSTR(&matrix_noise_shift, reader, 4, return -1);

      LIBBLU_MLP_DEBUG_PARSING_SS(
        "           TODO (matrix_noise_shift[%u]): "
        "%" PRIu8 " (0x%" PRIX8 ").\n",
        mat, matrix_noise_shift,
        matrix_noise_shift
      );
    }
  }

  return 0;
}

typedef enum {
  MLP_FIR,
  MLP_IIR
} MlpFilterType;

static int _decodeMlpFilterParameters(
  MlpSubstreamBitsReader * reader,
  MlpFilterType filter_type
)
{

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "          %s filter parameters, filter_parameters(%s)\n",
    (MLP_FIR == filter_type) ? "FIR" : "IIR",
    (MLP_FIR == filter_type) ? "FIR_filter_type" : "IIR_filter_type"
  );

  /* [u4 filter_order] */
  unsigned filter_order;
  READ_BITS_MLP_SUBSTR(&filter_order, reader, 4, return -1);

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "           TODO (filter_order): %u (0x%X).\n",
    filter_order,
    filter_order
  );

  if (0 < filter_order) {
    /* [u4 shift] */
    unsigned shift;
    READ_BITS_MLP_SUBSTR(&shift, reader, 4, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "           TODO (shift): %u (0x%X).\n",
      shift,
      shift
    );

    /* [u5 coeff_bits] */
    unsigned coeff_bits;
    READ_BITS_MLP_SUBSTR(&coeff_bits, reader, 5, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "           TODO (coeff_bits): %u (0x%02X).\n",
      coeff_bits,
      coeff_bits
    );

    /* [u3 coeff_shift] */
    unsigned coeff_shift;
    READ_BITS_MLP_SUBSTR(&coeff_shift, reader, 3, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "           TODO (coeff_shift): %u (0x%02X).\n",
      coeff_shift,
      coeff_shift
    );

    for (unsigned i = 0; i < filter_order; i++) {
      /* [u<coeff_bits> coeff[i]] */
      uint32_t coeff;
      READ_BITS_MLP_SUBSTR(&coeff, reader, coeff_bits, return -1);

      LIBBLU_MLP_DEBUG_PARSING_SS(
        "            TODO (coeff[i]): %" PRIu32 " (0x%0*" PRIX32 ").\n",
        coeff,
        (coeff_bits + 3) >> 2,
        coeff
      );
    }

    /* [b1 state_present] */
    bool state_present;
    READ_BITS_MLP_SUBSTR(&state_present, reader, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "            TODO (state_present): %s (0b%x).\n",
      BOOL_PRESENCE(state_present),
      state_present
    );

    if (state_present) {
      /* [u4 state_bits] */
      unsigned state_bits;
      READ_BITS_MLP_SUBSTR(&state_bits, reader, 4, return -1);

      LIBBLU_MLP_DEBUG_PARSING_SS(
        "             TODO (state_bits): %u (0x%X).\n",
        state_bits,
        state_bits
      );

      /* [u4 state_shift] */
      unsigned state_shift;
      READ_BITS_MLP_SUBSTR(&state_shift, reader, 4, return -1);

      LIBBLU_MLP_DEBUG_PARSING_SS(
        "             TODO (state_shift): %u (0x%X).\n",
        state_shift,
        state_shift
      );

      for (unsigned i = 0; i < filter_order; i++) {
        /* [u<state_bits> state[i]] */
        uint32_t state;
        READ_BITS_MLP_SUBSTR(&state, reader, state_bits, return -1);

        LIBBLU_MLP_DEBUG_PARSING_SS(
          "             TODO (state[i]): %" PRIu32 " (0x%0*" PRIX32 ").\n",
          state,
          (state_bits + 3) >> 2,
          state
        );
      }
    }
  }

  return 0;
}

static int _decodeMlpChannelParameters(
  MlpSubstreamBitsReader * reader,
  MlpSubstreamParameters * ss_param,
  unsigned ch_idx
)
{
  MlpChannelParameters * ch_param = &ss_param->channels_parameters[ch_idx];

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "        Channel %u parameters, channel_parameters[%u]()\n",
    ch_idx, ch_idx
  );

  if (ss_param->block_header_content & MLP_BHC_FIR_FILTER_PARAMETERS) {
    /* [b1 fir_filter_parameters_present] */
    bool fir_filter_parameters_present;
    READ_BITS_MLP_SUBSTR(&fir_filter_parameters_present, reader, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "         TODO (fir_filter_parameters_present): %s (0b%x).\n",
      BOOL_PRESENCE(fir_filter_parameters_present),
      fir_filter_parameters_present
    );

    if (fir_filter_parameters_present) {
      if (0 < ch_param->fir_filter_nb_changes++) // FIXME Compliance error?
        LIBBLU_MLP_ERROR_RETURN(
          "FIR filter parameters present more than once in access unit, "
          "which is not allowed.\n"
        );

      /* [vn filter_parameters(FIR_filter_type)] */
      if (_decodeMlpFilterParameters(reader, MLP_FIR) < 0)
        return -1;
    }
  }

  if (ss_param->block_header_content & MLP_BHC_IIR_FILTER_PARAMETERS) {
    /* [b1 iir_filter_parameters_present] */
    bool iir_filter_parameters_present;
    READ_BITS_MLP_SUBSTR(&iir_filter_parameters_present, reader, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "         TODO (iir_filter_parameters_present): %s (0b%x).\n",
      BOOL_PRESENCE(iir_filter_parameters_present),
      iir_filter_parameters_present
    );

    if (iir_filter_parameters_present) {
      if (0 < ch_param->iir_filter_nb_changes++) // FIXME Compliance error?
        LIBBLU_MLP_ERROR_RETURN(
          "IIR filter parameters present more than once in access unit, "
          "which is not allowed.\n"
        );

      /* [vn filter_parameters(IIR_filter_type)] */
      if (_decodeMlpFilterParameters(reader, MLP_IIR) < 0)
        return -1;
    }
  }

  if (ss_param->block_header_content & MLP_BHC_HUFFMAN_OFFSET) {
    /* [b1 huffman_offset_present] */
    bool huffman_offset_present;
    READ_BITS_MLP_SUBSTR(&huffman_offset_present, reader, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "         TODO (huffman_offset_present): %s (0b%x).\n",
      BOOL_PRESENCE(huffman_offset_present),
      huffman_offset_present
    );

    if (huffman_offset_present) {
      /* [s15 huffman_offset] */
      uint16_t huffman_offset;
      READ_BITS_MLP_SUBSTR(&huffman_offset, reader, 15, return -1);

      LIBBLU_MLP_DEBUG_PARSING_SS(
        "          TODO (huffman_offset): %d (0x%04" PRIX16 ").\n",
        lb_sign_extend(huffman_offset, 15),
        huffman_offset
      );
    }
  }

  /* [u2 huffman_codebook] */
  unsigned huffman_codebook;
  READ_BITS_MLP_SUBSTR(&huffman_codebook, reader, 2, return -1);
  ch_param->huffman_codebook = huffman_codebook;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "         TODO (huffman_codebook): %u.\n",
    huffman_codebook
  );

  /* [u5 num_huffman_lsbs] */
  unsigned num_huffman_lsbs;
  READ_BITS_MLP_SUBSTR(&num_huffman_lsbs, reader, 5, return -1);
  ch_param->num_huffman_lsbs = num_huffman_lsbs;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "         TODO (num_huffman_lsbs): %u (0x%02X).\n",
    num_huffman_lsbs,
    num_huffman_lsbs
  );

  // TODO
  assert(0 == huffman_codebook || num_huffman_lsbs <= 24);

  return 0;
}

static int _decodeMlpBlockHeader(
  MlpSubstreamBitsReader * reader,
  MlpSubstreamParameters * ss_param
)
{
  unsigned max_matrix_chan = ss_param->restart_header.max_matrix_chan;
  unsigned min_chan        = ss_param->restart_header.min_chan;
  unsigned max_chan        = ss_param->restart_header.max_chan;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "      Block header, block_header()\n"
  );

  if (ss_param->block_header_content & MLP_BHC_BLOCK_HEADER_CONTENT) {
    /* [b1 block_header_content_exists] */
    bool block_header_content_exists;
    READ_BITS_MLP_SUBSTR(&block_header_content_exists, reader, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "       TODO (block_header_content_exists): %s (0b%x).\n",
      BOOL_PRESENCE(block_header_content_exists),
      block_header_content_exists
    );

    if (block_header_content_exists) {
      /* [u8 block_header_content] */
      uint8_t block_header_content;
      READ_BITS_MLP_SUBSTR(&block_header_content, reader, 8, return -1);

      LIBBLU_MLP_DEBUG_PARSING_SS(
        "        TODO (block_header_content): 0x%02" PRIX8 ".\n",
        block_header_content
      );
      ss_param->block_header_content = block_header_content;
    }
  }

  if (ss_param->block_header_content & MLP_BHC_BLOCK_SIZE) {
    /* [b1 block_size_present] */
    bool block_size_present;
    READ_BITS_MLP_SUBSTR(&block_size_present, reader, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "       TODO (block_size_present): %s (0b%x).\n",
      BOOL_PRESENCE(block_size_present),
      block_size_present
    );

    if (block_size_present) {
      /* [u9 block_size] */
      unsigned block_size;
      READ_BITS_MLP_SUBSTR(&block_size, reader, 9, return -1);

      LIBBLU_MLP_DEBUG_PARSING_SS(
        "        TODO (block_size): %u samples (0x%03X).\n",
        block_size,
        block_size
      );
      ss_param->block_size = block_size;
    }
  }

  if (ss_param->block_header_content & MLP_BHC_MATRIX_PARAMETERS) {
    /* [b1 matrix_parameters_present] */
    bool matrix_parameters_present;
    READ_BITS_MLP_SUBSTR(&matrix_parameters_present, reader, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "       TODO (matrix_parameters_present): %s (0b%x).\n",
      BOOL_PRESENCE(matrix_parameters_present),
      matrix_parameters_present
    );

    if (matrix_parameters_present) {
      if (0 < ss_param->matrix_parameters_nb_changes++) // FIXME Compliance error?
        LIBBLU_MLP_ERROR_RETURN(
          "Matrix parameters present more than once in access unit, "
          "which is not allowed.\n"
        );

      int ret = _decodeMlpMatrixParameters(
        reader,
        &ss_param->matrix_parameters,
        &ss_param->restart_header
      );
      if (ret < 0)
        return -1;
    }
  }

  if (ss_param->block_header_content & MLP_BHC_OUTPUT_SHIFT) {
    /* [b1 output_shift_present] */
    bool output_shift_present;
    READ_BITS_MLP_SUBSTR(&output_shift_present, reader, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "       TODO (output_shift_present): %s (0b%x).\n",
      BOOL_PRESENCE(output_shift_present),
      output_shift_present
    );

    if (output_shift_present) {
      for (unsigned ch = 0; ch <= max_matrix_chan; ch++) {
        /* [s4 output_shift[ch]] */
        uint8_t output_shift;
        READ_BITS_MLP_SUBSTR(&output_shift, reader, 4, return -1);

        LIBBLU_MLP_DEBUG_PARSING_SS(
          "        TODO (output_shift[%u]): %d (0x%" PRIX8 ").\n",
          ch, lb_sign_extend(output_shift, 4),
          output_shift
        );
      }
    }
  }

  if (ss_param->block_header_content & MLP_BHC_QUANT_STEP_SIZE) {
    /* [b1 quant_step_size_present] */
    bool quant_step_size_present;
    READ_BITS_MLP_SUBSTR(&quant_step_size_present, reader, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "       TODO (quant_step_size_present): %s (0b%x).\n",
      BOOL_PRESENCE(quant_step_size_present),
      quant_step_size_present
    );

    if (quant_step_size_present) {
      for (unsigned ch = 0; ch <= max_matrix_chan; ch++) {
        /* [u4 quant_step_size[ch]] */
        uint8_t quant_step_size;
        READ_BITS_MLP_SUBSTR(&quant_step_size, reader, 4, return -1);
        ss_param->quant_step_size[ch] = quant_step_size;

        LIBBLU_MLP_DEBUG_PARSING_SS(
          "        TODO (quant_step_size[%u]): %u (0x%" PRIX8 ").\n",
          ch, quant_step_size,
          quant_step_size
        );
      }
    }
  }

  for (unsigned ch = min_chan; ch <= max_chan; ch++) {
    /* [b1 channel_parameters_present[ch]] */
    bool channel_parameters_present;
    READ_BITS_MLP_SUBSTR(&channel_parameters_present, reader, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "       TODO (channel_parameters_present[%u]): %s (0b%x).\n",
      ch, BOOL_PRESENCE(channel_parameters_present),
      channel_parameters_present
    );

    if (channel_parameters_present) {
      /* [vn channel_parameters()] */
      if (_decodeMlpChannelParameters(reader, ss_param, ch) < 0)
        return -1;
    }
  }

  for (unsigned ch = 0; ch <= max_chan; ch++) {
    const MlpChannelParameters * ch_param = &ss_param->channels_parameters[ch];

    assert(
      0 == ch_param->huffman_codebook
      || ss_param->quant_step_size[ch] <= ch_param->num_huffman_lsbs
    ); // TODO
  }

  return 0;
}

static int _decodeMlpBlockData(
  MlpSubstreamBitsReader * reader,
  MlpSubstreamParameters * ss_param
)
{
  unsigned min_chan   = ss_param->restart_header.min_chan;
  unsigned max_chan   = ss_param->restart_header.max_chan;
  unsigned block_size = ss_param->block_size;
  const unsigned * quant_step_size = ss_param->quant_step_size;

  const MlpMatrixParameters * matrix_param = &ss_param->matrix_parameters;
  unsigned num_pmat = matrix_param->num_primitive_matrices;

  LIBBLU_MLP_DEBUG_PARSING_SS("      Block data, block_data()\n");

  for (unsigned sample = 0; sample < block_size; sample++) {

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "       Huffman coded channels for sample %u, huffman_channels[%u]:\n",
      sample, sample
    );

    for (unsigned mat = 0; mat < num_pmat; mat++) {
      bool lsb_bypass_exists = matrix_param->matrices[mat].lsb_bypass_exists;

      if (lsb_bypass_exists) {
        bool lsb_bypass;
        READ_BITS_MLP_SUBSTR(&lsb_bypass, reader, 1, return -1);

        LIBBLU_MLP_DEBUG_PARSING_SS(
          "        TODO (lsb_bypass[%u]): %s (0b%x).\n",
          mat, BOOL_STR(lsb_bypass),
          lsb_bypass
        );
      }
    }

    for (unsigned ch = min_chan; ch <= max_chan; ch++) {
      const MlpChannelParameters * ch_param = &ss_param->channels_parameters[ch];
      unsigned huffman_codebook = ch_param->huffman_codebook;
      unsigned num_huffman_lsbs = ch_param->num_huffman_lsbs;
      int num_lsb_bits = num_huffman_lsbs - quant_step_size[ch];

      assert(0 <= num_lsb_bits);

      if (0 < huffman_codebook) {
        /* Huffman VLC */
        const MlpHuffmanLUT * huffman_book_lut = &mlpHuffmanTables[huffman_codebook-1];

        /* [vn huffman_code] */
        int32_t value;
        if (_decodeVlcMlpSubstreamBitsReader(reader, huffman_book_lut, &value) < 0)
          return -1;

        LIBBLU_MLP_DEBUG_PARSING_SS(
          "        TODO (huffman_code[%u]): %" PRId32 ".\n",
          ch, value
        );
      }

      if (0 < num_lsb_bits) {
        /* [u<num_lsb_bits> lsb_bits[ch]] */
        uint32_t lsb_bits;
        READ_BITS_MLP_SUBSTR(&lsb_bits, reader, num_lsb_bits, return -1);

        LIBBLU_MLP_DEBUG_PARSING_SS(
          "        TODO (lsb_bits[%u]): 0x%0*" PRIX32 " (%u bit(s)).\n",
          ch, (num_lsb_bits + 3) >> 2,
          lsb_bits,
          num_lsb_bits
        );
      }
    }
  }

  return 0;
}

static int _decodeMlpBlock(
  MlpSubstreamBitsReader * reader,
  MlpSubstreamParameters * ss_param,
  const MlpSubstreamDirectoryEntry * entry,
  unsigned ss_idx,
  unsigned blk_idx
)
{
  (void) entry; // TODO

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "    Audio data block %u, block()\n",
    blk_idx
  );

  /* [b1 block_header_exists] */
  bool block_header_exists;
  READ_BITS_MLP_SUBSTR(&block_header_exists, reader, 1, return -1);
  bool restart_header_exists = block_header_exists;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "     Block header (block_header_exists): %s (0b%x).\n",
    BOOL_PRESENCE(block_header_exists),
    block_header_exists
  );

  if (block_header_exists) {

    /* [b1 restart_header_exists] */
    READ_BITS_MLP_SUBSTR(&restart_header_exists, reader, 1, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "      Restart header (restart_header_exists): %s (0b%x).\n",
      BOOL_PRESENCE(restart_header_exists),
      restart_header_exists
    );

    if (restart_header_exists) {
      /* [vn restart_header()] */
      if (_decodeMlpRestartHeader(reader, &ss_param->restart_header, ss_idx) < 0)
        return -1;
      _defaultMlpSubstreamParameters(ss_param); // Set defaults.
      ss_param->restart_header_seen = true;
    }

    /* [vn block_header()] */
    if (_decodeMlpBlockHeader(reader, ss_param) < 0)
      return -1;
  }

  if (!ss_param->restart_header_seen)
    LIBBLU_MLP_ERROR_RETURN(
      "Missing required substream block first restart header, "
      "decoder won't be able to decode audio.\n"
    );

  if (ss_param->restart_header.error_protect) {
    /* [v16 error_protect] */
    uint16_t error_protect;
    READ_BITS_MLP_SUBSTR(&error_protect, reader, 16, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "     TODO (error_protect): 0x%04" PRIX16 ".\n",
      error_protect
    );
  }

  /* [vn block_data()] */
  if (_decodeMlpBlockData(reader, ss_param) < 0)
    return -1;

  if (ss_param->restart_header.error_protect) {
    /* [v8 block_header_CRC] */
    uint8_t block_header_CRC;
    READ_BITS_MLP_SUBSTR(&block_header_CRC, reader, 8, return -1);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "     TODO (block_header_CRC): 0x%02" PRIX8 ".\n",
      block_header_CRC
    );
  }

  return 0;
}

static int _decodeMlpSubstreamSegment(
  BitstreamReaderPtr input,
  MlpSubstreamParameters * substream,
  const MlpSubstreamDirectoryEntry * entry,
  unsigned ss_idx
)
{
  MlpSubstreamBitsReader reader;
  uint8_t parity = 0x00;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "   Substream segment %u, substream_segment(%u)\n",
    ss_idx,
    ss_idx
  );

  /* Init bitstream reader */
  uint16_t substreamSize = entry->substreamSize;
  if (_initMlpSubstreamBitsReader(&reader, input, substreamSize, &parity) < 0)
    return -1;

  /* Clear substream changes counters */
  substream->matrix_parameters_nb_changes = 0;
  for (unsigned i = 0; i < MLP_MAX_NB_CHANNELS; i++) {
    substream->channels_parameters[i].fir_filter_nb_changes = 0;
    substream->channels_parameters[i].iir_filter_nb_changes = 0;
  }

  bool last_block_in_segment;
  unsigned block_idx = 0;
  do {
    /* [vn block()] */
    if (_decodeMlpBlock(&reader, substream, entry, ss_idx, block_idx++) < 0)
      goto free_return;

    /* [b1 last_block_in_segment] */
    READ_BITS_MLP_SUBSTR(&last_block_in_segment, &reader, 1, goto free_return);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "    TODO (last_block_in_segment): %s (0b%x).\n",
      BOOL_STR(last_block_in_segment),
      last_block_in_segment
    );
  } while (!last_block_in_segment);

  /* [vn padding] */
  _padBitsMlpSubstreamBitsReader(&reader);

  if (32 <= _remainingBitsMlpSubstreamBitsReader(&reader)) {
    LIBBLU_MLP_DEBUG_PARSING_SS(
      "    End of stream extra parameters present.\n"
    );

    /* [v18 terminatorA] */
    uint32_t terminatorA;
    READ_BITS_MLP_SUBSTR(&terminatorA, &reader, 18, goto free_return);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "     TODO (terminatorA): 0x%05" PRIX32 ".\n",
      terminatorA
    );

    /* [b1 zero_samples_indicated] */
    bool zero_samples_indicated;
    READ_BITS_MLP_SUBSTR(&zero_samples_indicated, &reader, 1, goto free_return);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "     TODO (zero_samples_indicated): %s (0b%x).\n",
      BOOL_STR(zero_samples_indicated),
      zero_samples_indicated
    );

    if (zero_samples_indicated) {
      /* [u13 zero_samples] */
      unsigned zero_samples;
      READ_BITS_MLP_SUBSTR(&zero_samples, &reader, 13, goto free_return);

      LIBBLU_MLP_DEBUG_PARSING_SS(
        "     TODO (zero_samples): %u (0x%04X).\n",
        zero_samples,
        zero_samples
      );
    }
    else {
      /* [v13 terminatorB] */
      uint32_t terminatorB;
      READ_BITS_MLP_SUBSTR(&terminatorB, &reader, 13, goto free_return);

      LIBBLU_MLP_DEBUG_PARSING_SS(
        "     TODO (terminatorB): 0x%05" PRIX32 ".\n",
        terminatorB
      );
    }
  }

  if (entry->crc_present) {
    /* [v8 substream_parity[i]] */
    uint8_t substream_parity;
    READ_BITS_MLP_SUBSTR(&substream_parity, &reader, 8, goto free_return);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "     TODO (substream_parity[%u]): 0x%02" PRIX8 ".\n",
      ss_idx,
      substream_parity
    );

    /* [v8 substream_CRC[i]] */
    uint8_t substream_CRC;
    READ_BITS_MLP_SUBSTR(&substream_CRC, &reader, 8, goto free_return);

    LIBBLU_MLP_DEBUG_PARSING_SS(
      "     TODO (substream_CRC[%u]): 0x%02" PRIX8 ".\n",
      ss_idx,
      substream_CRC
    );
  }

  _cleanMlpSubstreamBitsReader(reader);
  return 0;

free_return:
  _cleanMlpSubstreamBitsReader(reader);
  return -1;
}

static int _decodeMlpExtraData(
  BitstreamReaderPtr input,
  unsigned au_remaining_length
)
{
  MlpSubstreamBitsReader reader;
  uint8_t parity = 0x00;

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "  Extension data, EXTRA_DATA()\n"
  );

  /** [v16 EXTRA_DATA_length_word]
   * -> v4:  length_check_nibble
   * -> u12: EXTRA_DATA_length
   */
  uint16_t length_word;
  READ_BITS(&length_word, input, 5, return -1);

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "   TODO (length_check_nibble): 0x%X.\n",
    length_word >> 12
  );

  for (int i = 3; 0 <= i; i--)
    parity ^= (length_word >> (i << 2)) & 0xFF;
  if (parity != 0xF)
    LIBBLU_MLP_ERROR_RETURN("Invalid EXTRA DATA length nibble check.\n");

  unsigned EXTRA_DATA_length = length_word & 0xFFF;
  LIBBLU_MLP_DEBUG_PARSING_SS(
    "   TODO (EXTRA_DATA_length): 0x%02X.\n",
    EXTRA_DATA_length
  );

  if (au_remaining_length < EXTRA_DATA_length)
    LIBBLU_MLP_ERROR_RETURN("EXTRA DATA length is out of access unit.\n");

  unsigned data_length = (EXTRA_DATA_length << 4) - 4;
  if (_initMlpSubstreamBitsReader(&reader, input, data_length, &parity) < 0)
    return -1;
  _cleanMlpSubstreamBitsReader(reader);

  /* [vn EXTRA_DATA_padding] */
  // Skipped

  /* [v8 EXTRA_DATA_parity] */
  uint8_t EXTRA_DATA_parity;
  READ_BITS(&EXTRA_DATA_parity, input, 8, return -1);

  if (parity != EXTRA_DATA_parity)
    LIBBLU_MLP_ERROR_RETURN("Invalid EXTRA DATA parity check.\n");

  return 0;
}

#if 0
int decodeMlpAccessUnit(
  BitstreamReaderPtr bs,
  MlpAccessUnitParameters * param
)
{
  uint32_t value;

  LIBBLU_AC3_DEBUG(" === MLP Access Unit ===\n");

  int64_t startOff = tellPos(bs);
  uint8_t checkNibbleValue = 0x00;



  /* Check if current frame contain Major Sync : */
  if ((nextUint32(bs) >> 8) == MLP_SYNCWORD_PREFIX) {
    MlpMajorSyncParameters majorSyncParam = {0}; /* Used as comparison */

    /**
     * Major Sync present, used as reference if it is the first one in stream,
     * or check if parameters in the current frame remain the same as the
     * reference during the whole bitsream.
     */
    LIBBLU_DEBUG_COM("MajorSyncInfo present:\n");

    if (decodeMlpMajorSyncInfo(bs, &majorSyncParam) < 0)
      return -1; /* Error happen during decoding. */

    if (!param->checkMode) {
      /*
        No reference parameters where supplied (first frame in stream ?),
        making current frame Major Sync as new reference.
      */
      // TODO: Check

      if (BDAV_TRUE_HD_MAX_AUTHORIZED_BITRATE < majorSyncParam.peak_data_rate) {
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
      if (!_constantMlpMajorSyncCheck(&(param->majorSync), &majorSyncParam))
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
    LIBBLU_AC3_DEBUG("  /* mlp_sync_header */\n");

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
      param->accessUnitLength * AC3_WORD
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
    READ_BITS_NIBBLE_MLP(&value, bs, 4, &checkNibbleValue, return -1);

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
    READ_BITS_NIBBLE_MLP(&value, bs, 12, &checkNibbleValue, return -1);

    param->extSubstreams[i].extSubstream_end_ptr = value;
    LIBBLU_DEBUG_COM("    -> extSubstream end ptr: 0x04%" PRIx16 ".\n", param->extSubstreams[i].extSubstream_end_ptr);

    if (param->extSubstreams[i].extra_extSubstream_word) {
      READ_BITS_NIBBLE_MLP(&value, bs, 16, &checkNibbleValue, return -1);
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
      if (readBits(bs,  &value, 16) < 0)
        return -1;
    }
  }

  /* Extra data */
  for (; tellPos(bs) < startOff + param->accessUnitLength * 2;) {
    if (readBits(bs,  &value, 16) < 0)
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
#endif

int analyzeAc3(
  LibbluESParsingSettings * settings
)
{
  int ret;


  EsmsFileHeaderPtr script_hdl = createEsmsFileHandler(
    ES_AUDIO, settings->options, FMT_SPEC_INFOS_AC3
  );
  if (NULL == script_hdl)
    return -1;

  unsigned src_file_idx;
  if (appendSourceFileEsms(script_hdl, settings->esFilepath, &src_file_idx) < 0)
    return -1;

  BitstreamReaderPtr bs = createBitstreamReaderDefBuf(
    settings->esFilepath
  );
  if (NULL == bs)
    return -1;

  BitstreamWriterPtr script_bs = createBitstreamWriterDefBuf(
    settings->scriptFilepath
  );
  if (NULL == script_bs)
    return -1;

  if (writeEsmsHeader(script_bs) < 0)
    return -1;

  Ac3Context ctx = (Ac3Context) {
    .perform_crc_checks = USE_CRC,
    .extract_core = settings->options.extractCore
  };

  while (!isEof(bs)) {
    uint64_t start_off = tellPos(bs);

    /* Progress bar : */
    printFileParsingProgressionBar(bs);

    if (nextUint16(bs) == AC3_SYNCWORD) {
      uint8_t bsid = _getSyncFrameBsid(bs);

      if (bsid <= 8) {
        /* AC-3 Bit stream syntax */
        LIBBLU_DEBUG_COM(" === AC3 Sync Frame ===\n");

        /* syncinfo() */
        Ac3SyncInfoParameters syncinfo;
        if (_parseAc3SyncInfo(bs, &syncinfo, ctx.perform_crc_checks) < 0)
          return -1;

        if (0 < ctx.ac3.nb_frames) {
          if (!_constantAc3SyncInfoCheck(&ctx.ac3.syncinfo, &syncinfo))
            return -1;
        }
        else {
          if (_checkAc3SyncInfoCompliance(&syncinfo) < 0)
            return -1;
          ctx.ac3.syncinfo = syncinfo;
        }

        /* bsi() */
        Ac3BitStreamInfoParameters bsi;
        if (_parseAc3BitStreamInfo(bs, &bsi) < 0)
          return -1;

        if (0 < ctx.ac3.nb_frames) {
          if (!_constantAc3BitStreamInfoCheck(&ctx.ac3.bsi, &bsi))
            return -1;
        }
        else {
          if (_checkAc3BitStreamInfoCompliance(&bsi) < 0)
            return -1;
          ctx.ac3.bsi = bsi;
        }

        if (_parseRemainingAc3Frame(bs, syncinfo, start_off, ctx.perform_crc_checks) < 0)
          return -1;

        if (0 == ctx.ac3.nb_frames) {
          if (_isPresentMlpMajorSyncFormatSyncWord(bs)) {
            LIBBLU_DEBUG_COM("Detection of MLP major sync word.\n");
            ctx.contains_mlp = true;
          }
        }

        ret = initEsmsAudioPesFrame(script_hdl, false, false, ctx.ac3.pts, 0);
        if (ret < 0)
          return -1;

        ret = appendAddPesPayloadCommand(script_hdl, src_file_idx, 0x0, start_off, syncinfo.frameSize);
        if (ret < 0)
          return -1;

        ctx.ac3.pts += ((uint64_t) MAIN_CLOCK_27MHZ * AC3_SAMPLES_PER_FRAME) / 48000;
        ctx.ac3.nb_frames++;
      }
      else if (10 < bsid && bsid <= 16) {
        /* E-AC-3 Bit stream syntax */
        LIBBLU_DEBUG_COM(" === E-AC3 Sync Frame ===\n");

        /* syncinfo() */
        if (_parseEac3SyncInfo(bs) < 0)
          return -1;

        if (ctx.perform_crc_checks)
          initCrcContext(crcCtxBitstream(bs), AC3_CRC_PARAMS, 0);

        /* bsi() */
        Eac3BitStreamInfoParameters bsi;
        if (_parseEac3BitStreamInfo(bs, &bsi) < 0)
          return -1;

        if (0 < ctx.eac3.nb_frames) {
          if (!_constantEac3BitStreamInfoCheck(&ctx.eac3.bsi, &bsi))
            return -1;
        }
        else {
          if (_checkEac3BitStreamInfoCompliance(&bsi) < 0)
            return -1;
          ctx.eac3.bsi = bsi;
        }

        if (_parseRemainingEac3Frame(bs, bsi, start_off, ctx.perform_crc_checks) < 0)
          return -1;

        if (ctx.extract_core)
          continue; // Skip extension frame.

        ret = initEsmsAudioPesFrame(script_hdl, true, false, ctx.eac3.pts, 0);
        if (ret < 0)
          return -1;

        ret = appendAddPesPayloadCommand(script_hdl, src_file_idx, 0x0, start_off, bsi.frameSize);
        if (ret < 0)
          return -1;

        ctx.eac3.pts += ((uint64_t) MAIN_CLOCK_27MHZ * AC3_SAMPLES_PER_FRAME) / 48000;
        ctx.eac3.nb_frames++;
      }
      else
        LIBBLU_AC3_ERROR("Unknown or unsupported bsid value %u.\n", bsid);

      if (writeEsmsPesPacket(script_bs, script_hdl) < 0)
        return -1;
      continue;
    }

    if (ctx.contains_mlp) {
      /* MLP-TrueHD Access Unit(s) may be present. */

      LIBBLU_AC3_DEBUG(" === MLP Access Unit ===\n");
      uint8_t chk_nibble = 0x00;

      /* mlp_sync_header */
      MlpSyncHeaderParameters mlp_sync_header;
      LIBBLU_MLP_DEBUG_PARSING_HDR(" MLP Sync, mlp_sync_header\n");
      if (_parseMlpSyncHeader(bs, &mlp_sync_header, &chk_nibble) < 0)
        return -1;

      if (0 < ctx.mlp.nb_frames) {
        // TODO: Constant check

        updateMlpSyncHeaderParametersParameters(
          &ctx.mlp.mlp_sync_header,
          mlp_sync_header
        );
      }
      else {
        if (_checkMlpSyncHeader(&mlp_sync_header, &ctx.mlp.info, true) < 0)
          return -1;

        // TODO: Check compliance (only if !ctx.extractCore)

        if (MLP_MAX_NB_SS < mlp_sync_header.major_sync_info.substreams) {
          LIBBLU_ERROR_RETURN("Unsupported number of substreams.\n"); // TODO
        }
        ctx.mlp.mlp_sync_header = mlp_sync_header;
      }

      if (mlp_sync_header.major_sync) {
        for (unsigned i = 0; i < MLP_MAX_NB_SS; i++)
          ctx.mlp.substreams[i].restart_header_seen = false;
      }

      // major_sync_info
      const MlpMajorSyncInfoParameters * ms_info =
        &ctx.mlp.mlp_sync_header.major_sync_info
      ;

      /* substream_directory */
      for (unsigned i = 0; i < ms_info->substreams; i++) {
        MlpSubstreamDirectoryEntry entry;
        if (_parseMlpSubstreamDirectoryEntry(bs, &entry, &chk_nibble) < 0)
          return -1;

        ctx.mlp.substream_directory[i] = entry;
      }

      if (chk_nibble != 0xF)
        LIBBLU_AC3_ERROR_RETURN(
          "Invalid minor sync check nibble (0x%" PRIX8 ").\n",
          chk_nibble
        );

      unsigned access_unit_length = ctx.mlp.mlp_sync_header.access_unit_length;
      unsigned mlp_sync_length = _computeLengthMlpSync(
        &ctx.mlp.mlp_sync_header,
        ctx.mlp.substream_directory
      );

      if (access_unit_length < mlp_sync_length)
        LIBBLU_MLP_ERROR_RETURN(
          "Too many words in access unit header, "
          "header is longer than 'access_unit_length' (%u, actual %u words).\n",
          access_unit_length,
          mlp_sync_length
        );

      unsigned unit_end = access_unit_length - mlp_sync_length;

      int ret = _checkAndComputeSSSizesMlpSubstreamDirectory(
        ctx.mlp.substream_directory,
        ms_info,
        unit_end,
        ctx.mlp.mlp_sync_header.major_sync
      );
      if (ret < 0)
        return -1;

      LIBBLU_MLP_DEBUG_PARSING_SS(
        "  Substream data, start\n"
      );

      unsigned last_substream_end_ptr = 0;
      for (unsigned i = 0; i < ms_info->substreams; i++) {
        const MlpSubstreamDirectoryEntry * entry = &ctx.mlp.substream_directory[i];
        MlpSubstreamParameters * ss = &ctx.mlp.substreams[i];

        /* substream_segment(i) */
        if (_decodeMlpSubstreamSegment(bs, ss, entry, i) < 0)
          return -1;
        // substream_parity/substream_CRC in substream_segment.

        last_substream_end_ptr = entry->substream_end_ptr;
      }

      /* extra_start */
      if (last_substream_end_ptr < unit_end) {
        unsigned au_remaining_length = unit_end - last_substream_end_ptr;

        if (_decodeMlpExtraData(bs, au_remaining_length) < 0)
          return -1;
      }
      else {
        LIBBLU_MLP_DEBUG_PARSING_SS(
          "  No EXTRA_DATA.\n"
        );
      }

      if (!ctx.extract_core) {
        ret = initEsmsAudioPesFrame(script_hdl, true, false, ctx.mlp.pts, 0);
        if (ret < 0)
          return -1;

        ret = appendAddPesPayloadCommand(
          script_hdl, src_file_idx, 0x0, start_off, 2 * access_unit_length
        );
        if (ret < 0)
          return -1;

        if (writeEsmsPesPacket(script_bs, script_hdl) < 0)
          return -1;

        ctx.mlp.pts += ((uint64_t) MAIN_CLOCK_27MHZ) / TRUE_HD_UNITS_PER_SEC;
        ctx.mlp.nb_frames++;
      }
      continue;

#if 0
      if (decodeMlpAccessUnit(bs, &accessUnit) < 0)
        return -1;

      if (!accessUnit.checkMode) {
        if (!ctx.extractCore && 0 == ctx.ac3Core.nbFrames) {
          LIBBLU_ERROR_RETURN(
            "Incompatible bitstream combination with MLP extension.\n"
          );
        }

        ctx.containsAtmos = accessUnit.containAtmos;
        accessUnit.checkMode = true;
      }

      if (ctx.extractCore)
        continue; /* Skip consideration of extension. */

      /* Writing PES frames cutting commands : */
      ret = initEsmsAudioPesFrame(
        script,
        true,
        false,
        ctx.mlpExt.pts,
        0
      );
      if (ret < 0)
        return -1;

      ret = appendAddPesPayloadCommand(
        script,
        srcFileIdx,
        0x0,
        startOff,
        accessUnit.accessUnitLength * AC3_WORD
      );
      if (ret < 0)
        return -1;

      if (writeEsmsPesPacket(essOutput, script) < 0)
        return -1;

      ctx.mlpExt.pts += ((uint64_t) MAIN_CLOCK_27MHZ) / TRUE_HD_UNITS_PER_SEC;
      continue;
#endif
    }

    /* else */
    LIBBLU_AC3_ERROR_RETURN(
      "Unexpected sync word 0x%08" PRIX32 ".\n",
      nextUint32(bs)
    );
  }

  /* lbc_printf(" === Parsing finished with success. ===\n"); */
  closeBitstreamReader(bs);

  /* [u8 endMarker] */
  if (writeByte(script_bs, ESMS_SCRIPT_END_MARKER) < 0)
    return -1;

  enum {
    AC3_AUDIO           = STREAM_CODING_TYPE_AC3,
    TRUE_HD_AUDIO       = STREAM_CODING_TYPE_TRUEHD,
    EAC3_AUDIO          = STREAM_CODING_TYPE_EAC3,
    EAC3_AUDIO_SEC      = STREAM_CODING_TYPE_EAC3_SEC, /* Not supported yet */
    DOLBY_AUDIO_DEFAULT = 0xFF
  } stream_type;

  if (0 == ctx.ac3.nb_frames)
    LIBBLU_AC3_ERROR_RETURN("Missing mandatory AC-3 core.\n");

  // FIXME
  if (0 < ctx.mlp.nb_frames && !ctx.extract_core)
    stream_type = TRUE_HD_AUDIO;
  else if (0 < ctx.eac3.nb_frames && !ctx.extract_core)
    stream_type = EAC3_AUDIO;
  else
    stream_type = AC3_AUDIO;

  script_hdl->prop.codingType = (LibbluStreamCodingType) stream_type;

  /* Register AC-3 infos : */
  LibbluESAc3SpecProperties * param = script_hdl->fmtSpecProp.ac3;
  param->sample_rate_code = ctx.ac3.syncinfo.fscod;
  param->bsid = ctx.ac3.bsi.bsid;
  param->bit_rate_code = ctx.ac3.syncinfo.frmsizecod >> 1;
  param->surround_mode = ctx.ac3.bsi.dsurmod;
  param->bsmod = ctx.ac3.bsi.bsmod;
  param->num_channels = ctx.ac3.bsi.acmod;
  param->full_svc = 0; // FIXME

  if (stream_type == AC3_AUDIO || stream_type == EAC3_AUDIO) {
    switch (ctx.ac3.bsi.acmod) {
      case 0: /* Dual-Mono */
        script_hdl->prop.audioFormat = 0x02; break;
      case 1: /* Mono */
        script_hdl->prop.audioFormat = 0x01; break;
      case 2: /* Stereo */
        script_hdl->prop.audioFormat = 0x03; break;
      default: /* Multi-channel */
        script_hdl->prop.audioFormat = 0x06; break;
    }
    script_hdl->prop.sampleRate = 0x01; /* 48 kHz */
    script_hdl->prop.bitDepth = 16; /* 16 bits */
  }
  else if (stream_type == TRUE_HD_AUDIO) {
    const MlpMajorSyncFormatInfo * format_info =
      &ctx.mlp.mlp_sync_header.major_sync_info.format_info
    ;

    unsigned nb_channels = getNbChannels6ChPresentationAssignment(
      format_info->u6ch_presentation_channel_assignment
    );
    assert(0 != nb_channels);

    switch (nb_channels) {
      case 1: /* Mono */
        script_hdl->prop.audioFormat = 0x01; break;
      case 2: /* Stereo */
        script_hdl->prop.audioFormat = 0x03; break;
      default:
        if (ctx.ac3.bsi.acmod < 3)
          script_hdl->prop.audioFormat = 0x0C; /* Stereo + Multi-channel */
        else
          script_hdl->prop.audioFormat = 0x06; /* Multi-channel */
    }

    unsigned sample_rate = sampleRateMlpAudioSamplingFrequency(
      format_info->audio_sampling_frequency
    );

    switch (sample_rate) {
      case 48000:  /* 48 kHz */
        script_hdl->prop.sampleRate = 0x01; break;
      case 96000:  /* 96 kHz */
        script_hdl->prop.sampleRate = 0x04; break;
      default: /* 192 kHz + 48kHz Core */
        script_hdl->prop.sampleRate = 0x0E; break;
    }

    script_hdl->prop.bitDepth = 16; /* 16, 20 or 24 bits (Does not matter, not contained in header) */
  }
  else {
    LIBBLU_TODO();
  }

  switch (stream_type) {
    case AC3_AUDIO:
      script_hdl->bitrate = 640000;
      break;
    case TRUE_HD_AUDIO:
      script_hdl->bitrate = 18640000;
      break;
    case EAC3_AUDIO:
      script_hdl->bitrate = 4736000;
      break;
    default: /* EAC3_AUDIO_SEC */
      script_hdl->bitrate = 256000;
  }

  /* Display infos : */
  lbc_printf("== Stream Infos =======================================================================\n");
  lbc_printf("Codec: ");
  switch (stream_type) {
    case AC3_AUDIO:
      lbc_printf("Audio/AC-3");
      break;

    case TRUE_HD_AUDIO:
      if (ctx.mlp.info.atmos)
        lbc_printf("Audio/AC-3 (+ TrueHD/Dolby Atmos Extensions)");
      else
        lbc_printf("Audio/AC-3 (+ TrueHD Extension)");
      break;

    case EAC3_AUDIO:
      // TODO
      // if (ctx.eac3.info.atmos)
      //   lbc_printf("Audio/AC-3 (+ E-AC3/Dolby Atmos Extensions)");
      // else
        lbc_printf("Audio/AC-3 (+ E-AC3 Extension)");
      break;

    default:
      lbc_printf("Audio/Unknown");
  }

  lbc_printf(", Nb channels: %u, Sample rate: 48000 Hz, Bits per sample: 16bits.\n",
    ctx.ac3.bsi.nbChannels
  );
  lbc_printf(
    "Stream Duration: %02" PRIu64 ":%02" PRIu64 ":%02" PRIu64 "\n",
    (ctx.ac3.pts / MAIN_CLOCK_27MHZ) / 60 / 60,
    (ctx.ac3.pts / MAIN_CLOCK_27MHZ) / 60 % 60,
    (ctx.ac3.pts / MAIN_CLOCK_27MHZ) % 60
  );
  lbc_printf("=======================================================================================\n");

  script_hdl->endPts = ctx.ac3.pts;

  if (addEsmsFileEnd(script_bs, script_hdl) < 0)
    return -1;
  closeBitstreamWriter(script_bs);

  if (updateEsmsHeader(settings->scriptFilepath, script_hdl) < 0)
    return -1;
  destroyEsmsFileHandler(script_hdl);
  return 0;
}