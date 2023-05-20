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

#define SKIP_BITS(bs, s, e)                                                   \
  do {                                                                        \
    if (skipBits(bs, s) < 0)                                                  \
      e;                                                                      \
  } while (0)

static unsigned _frmsizecodToNominalBitrate(
  uint8_t frmsizecod
)
{
  static const unsigned frmsizecod_list[] = {
     32,  40,  48,  56,  64,
     80,  96, 112, 128, 160,
    192, 224, 256, 320, 384,
    448, 512, 576, 640
  };

  if (frmsizecod < ARRAY_SIZE(frmsizecod_list) * 2)
    return frmsizecod_list[frmsizecod / 2];
  return 0;
}

/** \~english
 * \brief Return size in 16-bits words of an AC-3 sync.
 *
 * \param frmsizecod
 * \param fscod
 * \return unsigned
 */
static unsigned _frmsizecodToFrameSize(
  uint8_t frmsizecod,
  Ac3Fscod fscod
)
{
  static const unsigned frmsizecod_list[][38] = {
    { // fscod == 0, 48 KHz
        64,   64,   80,   80,   96,   96,  112,  112,  128,  128,
       160,  160,  192,  192,  224,  224,  256,  256,  320,  320,
       384,  384,  448,  448,  512,  512,  640,  640,  768,  768,
       896,  896, 1024, 1024, 1152, 1152, 1280, 1280
    },
    { // fscod == 1, 44.1 KHz
        69,   70,   87,   88,  104,  105,  121,  122,  139,  140,
       174,  175,  209,  209,  243,  244,  278,  279,  348,  349,
       417,  418,  487,  488,  557,  558,  696,  697,  835,  836,
       975,  976, 1114, 1115, 1253, 1254, 1393, 1394
    },
    { // fscod == 2, 32 KHz
        96,   96,  120,  120,  144,  144,  168,  168,  192,  192,
       240,  240,  288,  288,  336,  336,  384,  384,  480,  480,
       576,  576,  672,  672,  768,  768,  960,  960, 1152, 1152,
      1344, 1344, 1536, 1536, 1728, 1728, 1920, 1920
    }
  };

  if (fscod < ARRAY_SIZE(frmsizecod_list) && frmsizecod < 38)
    return frmsizecod_list[fscod][frmsizecod];
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

  unsigned bitrate = _frmsizecodToNominalBitrate(param->frmsizecod);
  if (0 == bitrate)
    LIBBLU_AC3_ERROR_RETURN(
      "Reserved value in use (frmsizecod == 0x%" PRIX8 ").\n",
      param->frmsizecod
    );

  LIBBLU_AC3_DEBUG(
    "   -> %u words, %u bytes, %u kbps.\n",
    _frmsizecodToFrameSize(param->frmsizecod, param->fscod),
    2 * _frmsizecodToFrameSize(param->frmsizecod, param->fscod),
    bitrate
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
  Ac3SyncInfoParameters * syncinfo
)
{
  /* For (bsid <= 0x8) syntax */

  LIBBLU_AC3_DEBUG(" Synchronization Information, syncinfo()\n");

  /* [v16 syncword] // 0x0B77 */
  uint16_t syncword;
  READ_BITS(&syncword, bs, 16, return -1);

  LIBBLU_AC3_DEBUG(
    "  Sync Word (syncword): 0x%04" PRIX32 ".\n",
    syncword
  );

  if (AC3_SYNCWORD != syncword)
    LIBBLU_AC3_ERROR_RETURN(
      "Unexpected 'syncword' value 0x%04X, expect 0x0B77.\n",
      syncword
    );

  /* Init the CRC check if requested */
  // initCrcContext(crcCtxBitstream(bs), AC3_CRC_PARAMS, 0x00);

  /* [v16 crc1] */
  READ_BITS(&syncinfo->crc1, bs, 16, return -1);

  /* [u2 fscod] */
  READ_BITS(&syncinfo->fscod, bs, 2, return -1);

  /* [u6 frmsizecod] */
  READ_BITS(&syncinfo->frmsizecod, bs, 6, return -1);

  return 0;
}

static int _parseEac3SyncInfo(
  BitstreamReaderPtr bs
)
{
  /* For (bsid == 16) syntax */

  LIBBLU_AC3_DEBUG(" Synchronization Information, syncinfo()\n");

  /* [v16 syncword] // 0x0B77 */
  uint16_t syncword;
  READ_BITS(&syncword, bs, 16, return -1);

  LIBBLU_AC3_DEBUG(
    "  Sync Word (syncword): 0x%04" PRIX16 ".\n",
    syncword
  );

  if (AC3_SYNCWORD != syncword)
    LIBBLU_AC3_ERROR_RETURN(
      "Unexpected 'syncword' value 0x%04X, expect 0x0B77.\n",
      syncword
    );

  return 0;
}

static int _parseAc3Addbsi(
  BitstreamReaderPtr bs,
  Ac3Addbsi * addbsi
)
{

  /* [u6 addbsil] */
  READ_BITS(&addbsi->addbsil, bs, 6, return -1);

  if (addbsi->addbsil == 1) {
    /* Assuming presence of EC3 Extension      */
    /* Reference: ETSI TS 103 420 V1.2.1 - 8.3 */

    /* Try [v7 reserved] [b1 flag_ec3_extension_type_a] ... */
    uint16_t content;
    READ_BITS(&content, bs, 16, return -1);

    if ((content >> 8) == 0x1) {
      /* If 'reserved' == 0x0 && 'flag_ec3_extension_type_a' == 0x1 */
      addbsi->type = AC3_ADDBSI_EC3_EXT_TYPE_A;

      /* [u8 complexity_index_type_a] */
      addbsi->ec3ExtTypeA.complexityIndex = (content & 0xFF);
    }
    /* else Unknown extension. */
  }
  else {
    /* Unknown extension. */
    /* [v<(addbsil + 1) * 8> addbsi] */
    SKIP_BITS(bs, (addbsi->addbsil + 1) << 3, return -1);
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

  /* [u5 bsid] */
  READ_BITS(&bsi->bsid, bs, 5, return -1);

  if (8 < bsi->bsid)
    LIBBLU_AC3_ERROR_RETURN(
      "Unexpected or unsupported Bit Stream Identifier "
      "(bsid == %" PRIu8 ").\n",
      bsi->bsid
    );

  /* [u3 bsmod] */
  READ_BITS(&bsi->bsmod, bs, 3, return -1);

  /* [u3 acmod] */
  READ_BITS(&bsi->acmod, bs, 3, return -1);

  if (threeFrontChannelsPresentAc3Acmod(bsi->acmod)) {
    /* If 3 front channels present. */
    /* [u2 cmixlevel] */
    READ_BITS(&bsi->cmixlev, bs, 2, return -1);
  }

  if (isSurroundAc3Acmod(bsi->acmod)) {
    /* If surround channel(s) present. */
    /* [u2 surmixlev] */
    READ_BITS(&bsi->surmixlev, bs, 2, return -1);
  }

  if (bsi->acmod == AC3_ACMOD_L_R) {
    /* If Stereo 2/0 (L, R) */
    /* [u2 dsurmod] */
    READ_BITS(&bsi->dsurmod, bs, 2, return -1);
  }

  /* [b1 lfeon] */
  READ_BITS(&bsi->lfeon, bs, 1, return -1);

  bsi->nbChannels = nbChannelsAc3Acmod(
    bsi->acmod,
    bsi->lfeon
  );

  /* [u5 dialnorm] */
  READ_BITS(&bsi->dialnorm, bs, 5, return -1);

  /* [b1 compre] */
  READ_BITS(&bsi->compre, bs, 1, return -1);

  if (bsi->compre) {
    /* [u8 compr] */
    READ_BITS(&bsi->compr, bs, 8, return -1);
  }

  /* [b1 langcode] */
  READ_BITS(&bsi->langcode, bs, 1, return -1);

  if (bsi->langcode) {
    /* [u8 langcod] */
    READ_BITS(&bsi->langcod, bs, 8, return -1);
  }

  /* [b1 audprodie] */
  READ_BITS(&bsi->audprodie, bs, 1, return -1);

  if (bsi->audprodie) {
    /* [u5 mixlevel] */
    READ_BITS(&bsi->audprodi.mixlevel, bs, 5, return -1);

    /* [u2 roomtyp] */
    READ_BITS(&bsi->audprodi.roomtyp, bs, 2, return -1);
  }

  if (bsi->acmod == AC3_ACMOD_CH1_CH2) {
    /* If 1+1 duplicated mono, require parameters for the second mono builded channel. */

    /* [u5 dialnorm2] */
    READ_BITS(&bsi->dual_mono.dialnorm2, bs, 5, return -1);

    /* [b1 compr2e] */
    READ_BITS(&bsi->dual_mono.compr2e, bs, 1, return -1);

    if (bsi->dual_mono.compr2e) {
      /* [u8 compr2] */
      READ_BITS(&bsi->dual_mono.compr2, bs, 8, return -1);
    }

    /* [b1 langcod2e] */
    READ_BITS(&bsi->dual_mono.langcod2e, bs, 1, return -1);

    if (bsi->dual_mono.langcod2e) {
      /* [u8 langcod2] */
      READ_BITS(&bsi->dual_mono.langcod2, bs, 8, return -1);
    }

    /* [b1 audprodi2e] */
    READ_BITS(&bsi->dual_mono.audprodi2e, bs, 1, return -1);

    if (bsi->dual_mono.audprodi2e) {
      /* [u5 mixlevel2] */
      READ_BITS(&bsi->dual_mono.audprodi2.mixlevel2, bs, 5, return -1);

      /* [u2 roomtyp2] */
      READ_BITS(&bsi->dual_mono.audprodi2.roomtyp2, bs, 2, return -1);
    }
  }

  /* [b1 copyrightb] */
  READ_BITS(&bsi->copyrightb, bs, 1, return -1);

  /* [b1 origbs] */
  READ_BITS(&bsi->origbs, bs, 1, return -1);

  if (bsi->bsid == 0x6) {
    /* Alternate Bit Stream Syntax */
    /* ETSI TS 102 366 V1.4.1 - Annex D */

    /* [b1 xbsi1e] */
    READ_BITS(&bsi->xbsi1e, bs, 1, return -1);

    if (bsi->xbsi1e) {
      /* [u2 dmixmod] */
      READ_BITS(&bsi->xbsi1.dmixmod, bs, 2, return -1);

      /* [u3 ltrtcmixlev] */
      READ_BITS(&bsi->xbsi1.ltrtcmixlev, bs, 3, return -1);

      /* [u3 ltrtsurmixlev] */
      READ_BITS(&bsi->xbsi1.ltrtsurmixlev, bs, 3, return -1);

      /* [u3 lorocmixlev] */
      READ_BITS(&bsi->xbsi1.lorocmixlev, bs, 3, return -1);

      /* [u3 lorosurmixlev] */
      READ_BITS(&bsi->xbsi1.lorosurmixlev, bs, 3, return -1);
    }

    /* [b1 xbsi2e] */
    READ_BITS(&bsi->xbsi2e, bs, 1, return -1);

    if (bsi->xbsi2e) {
      /* [u2 dsurexmod] */
      READ_BITS(&bsi->xbsi2.dsurexmod, bs, 2, return -1);

      /* [u2 dheadphonmod] */
      READ_BITS(&bsi->xbsi2.dheadphonmod, bs, 2, return -1);

      /* [b1 adconvtyp] */
      READ_BITS(&bsi->xbsi2.adconvtyp, bs, 1, return -1);

      /* [u8 xbsi2] */
      READ_BITS(&bsi->xbsi2.xbsi2, bs, 8, return -1);

      /* [b1 encinfo] */
      READ_BITS(&bsi->xbsi2.encinfo, bs, 1, return -1);
    }
  }
  else {
    /* [b1 timecod1e] */
    READ_BITS(&bsi->timecod1e, bs, 1, return -1);

    if (bsi->timecod1e) {
      /* [u16 timecod1] */
      READ_BITS(&bsi->timecod1, bs, 16, return -1);
    }

    /* [b1 timecod2e] */
    READ_BITS(&bsi->timecod2e, bs, 1, return -1);

    if (bsi->timecod2e) {
      /* [u16 timecod2] */
      READ_BITS(&bsi->timecod2, bs, 16, return -1);
    }
  }

  /* [b1 addbsie] */
  READ_BITS(&bsi->addbsie, bs, 1, return -1);

  if (bsi->addbsie) {
    if (_parseAc3Addbsi(bs, &bsi->addbsi) < 0)
      return -1;
  }

  if (paddingByte(bs) < 0)
    return -1;

  return 0;
}

/** \~english
 * \brief
 *
 * \param bs
 * \param syncinfo
 * \param frame_start_off
 * \param frame_size Sync frame size in 16-bits words unit.
 * \return int
 */
static int _parseRemainingAc3Frame(
  BitstreamReaderPtr bs,
  Ac3SyncInfoParameters syncinfo,
  int64_t frame_start_off,
  unsigned frame_size
)
{
  int64_t already_parsed_size = tellPos(bs) - frame_start_off;
  if (already_parsed_size < 0)
    LIBBLU_AC3_ERROR_RETURN("Unexpected negative file offset.\n");

  unsigned rem_size = (frame_size << 1) - already_parsed_size;
  if (skipBytes(bs, rem_size) < 0)
    return -1;

  // if (0x0 != completeCrcContext(crcCtxBitstream(bs)))
  //   LIBBLU_AC3_ERROR_RETURN("CRC checksum error at the end of the frame.\n");
  // FIXME: Issue with CRC check on some streams.

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

  if (param->fscod == EAC3_FSCOD_RES_FSCOD2) {
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
        infParam->audprodi.mixlevel,
        infParam->audprodi.mixlevel
      );
      LIBBLU_AC3_DEBUG(
        "     -> %u dB SPL.\n",
        80u + infParam->audprodi.mixlevel
      );

      LIBBLU_AC3_DEBUG(
        "    Room Type (roomtyp): %" PRIu8 " (0x%02" PRIX8 ").\n",
        infParam->audprodi.roomtyp,
        infParam->audprodi.roomtyp
      );
      LIBBLU_AC3_DEBUG(
        "     -> %s.\n",
        Ac3RoomtypStr(infParam->audprodi.roomtyp)
      );

      if (AC3_ROOMTYP_RES == infParam->audprodi.roomtyp)
        LIBBLU_AC3_WARNING(
          "Non-fatal reserved value in use (roomtyp == 0x%" PRIX8 ", "
          "interpreted as 0x0, 'not indicated').\n",
          infParam->audprodi.roomtyp
        );

      LIBBLU_AC3_DEBUG(
        "    A/D Converter Type (adconvtyp): %s (0b%x).\n",
        BOOL_STR(infParam->audprodi.adconvtyp),
        infParam->audprodi.adconvtyp
      );
      LIBBLU_AC3_DEBUG(
        "     -> %s.\n",
        (infParam->audprodi.adconvtyp) ? "HDCD" : "Standard"
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
          infParam->audprodi2.mixlevel2,
          infParam->audprodi2.mixlevel2
        );
        LIBBLU_AC3_DEBUG(
          "      -> %u dB SPL.\n",
          80u + infParam->audprodi2.mixlevel2
        );

        LIBBLU_AC3_DEBUG(
          "     Room Type for ch2 (roomtyp2): %" PRIu8 " (0x%02" PRIX8 ").\n",
          infParam->audprodi2.roomtyp2,
          infParam->audprodi2.roomtyp2
        );
        LIBBLU_AC3_DEBUG(
          "      -> %s.\n",
          Ac3RoomtypStr(infParam->audprodi2.roomtyp2)
        );

        if (AC3_ROOMTYP_RES == infParam->audprodi2.roomtyp2)
          LIBBLU_AC3_WARNING(
            "Non-fatal reserved value in use (roomtyp2 == 0x%" PRIX8 ", "
            "interpreted as 0x0, 'not indicated').\n",
            infParam->audprodi2.roomtyp2
          );

        LIBBLU_AC3_DEBUG(
          "     A/D Converter Type for ch2 (adconvtyp2): %s (0b%x).\n",
          BOOL_STR(infParam->audprodi2.adconvtyp2),
          infParam->audprodi2.adconvtyp2
        );
        LIBBLU_AC3_DEBUG(
          "      -> %s.\n",
          (infParam->audprodi2.adconvtyp2) ? "HDCD" : "Standard"
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

  /* [u2 strmtyp] */
  READ_BITS(&bsi->strmtyp, bs, 2, return -1);

  /* [u3 substreamid] */
  READ_BITS(&bsi->substreamid, bs, 3, return -1);

  /* [u11 frmsiz] */
  READ_BITS(&bsi->frmsiz, bs, 11, return -1);
  bsi->frameSize = 2 * (bsi->frmsiz + 1);

  /* [u2 fscod] */
  READ_BITS(&bsi->fscod, bs, 2, return -1);
  bsi->sampleRate = sampleRateAc3Fscod(bsi->fscod);

  if (bsi->fscod == 0x3) {
    /* [u2 fscod2] */
    READ_BITS(&bsi->fscod2, bs, 2, return -1);
    bsi->sampleRate = sampleRateEac3Fscod2(bsi->fscod2);

    bsi->numblkscod = EAC3_NUMBLKSCOD_6_BLOCKS;
  }
  else {
    /* [u2 numblkscod] */
    READ_BITS(&bsi->numblkscod, bs, 2, return -1);
  }

  /* Converting numblkscod to Number of Blocks in Sync Frame. */
  bsi->numBlksPerSync = nbBlocksEac3Numblkscod(bsi->numblkscod);

  /* [u3 acmod] */
  READ_BITS(&bsi->acmod, bs, 3, return -1);

  /* [b1 lfeon] */
  READ_BITS(&bsi->lfeon, bs, 1, return -1);
  bsi->nbChannels = nbChannelsAc3Acmod(bsi->acmod, bsi->lfeon);

  /* [u5 bsid] // 0x10/16 in this syntax */
  READ_BITS(&bsi->bsid, bs, 5, return -1);

  if (bsi->bsid <= 10 || 16 < bsi->bsid)
    LIBBLU_AC3_ERROR_RETURN(
      "Unexpected or unsupported Bit Stream Identifier "
      "(bsid == %" PRIu8 ", parser expect 16).\n",
      bsi->bsid
    );

  /* [u5 dialnorm] */
  READ_BITS(&bsi->dialnorm, bs, 5, return -1);

  /* [b1 compre] */
  READ_BITS(&bsi->compre, bs, 1, return -1);

  if (bsi->compre) {
    /* [u8 compr] */
    READ_BITS(&bsi->compr, bs, 8, return -1);
  }

  if (bsi->acmod == 0x0) {
    /* If 1+1 duplicated mono, require parameters for the second mono builded channel. */

    /* [u5 dialnorm2] */
    READ_BITS(&bsi->dual_mono.dialnorm2, bs, 5, return -1);

    /* [b1 compr2e] */
    READ_BITS(&bsi->dual_mono.compr2e, bs, 1, return -1);

    if (bsi->dual_mono.compr2e) {
      /* [u8 compr2] */
      READ_BITS(&bsi->dual_mono.compr2, bs, 8, return -1);
    }
  }

  if (bsi->strmtyp == 0x1) {
    /* [b1 chanmape] */
    READ_BITS(&bsi->chanmape, bs, 1, return -1);

    if (bsi->chanmape) {
      /* [u16 chanmap] */
      READ_BITS(&bsi->chanmap, bs, 16, return -1);
      bsi->nbChannelsFromMap = nbChannelsEac3Chanmap(bsi->chanmap);
    }
  }

  /* [b1 mixmdate] */
  READ_BITS(&bsi->mixmdate, bs, 1, return -1);

  if (bsi->mixmdate) {
    /* Mixing meta-data */
    /* Not checked. */

    if (0x2 < bsi->acmod) {
      /* More than two channels. */

      /* [u2 dmixmod] */
      READ_BITS(&bsi->Mixdata.dmixmod, bs, 2, return -1);
    }

    if ((bsi->acmod & 0x1) && (0x2 < bsi->acmod)) {
      /* Three front channels present. */

      /* [u3 ltrtcmixlev] */
      READ_BITS(&bsi->Mixdata.ltrtcmixlev, bs, 3, return -1);

      /* [u3 lorocmixlev] */
      READ_BITS(&bsi->Mixdata.lorocmixlev, bs, 3, return -1);
    }

    if (bsi->acmod & 0x4) {
      /* Surround channels present. */

      /* [u3 ltrtsurmixlev] */
      READ_BITS(&bsi->Mixdata.ltrtsurmixlev, bs, 3, return -1);

      /* [u3 lorosurmixlev] */
      READ_BITS(&bsi->Mixdata.lorosurmixlev, bs, 3, return -1);
    }

    if (bsi->lfeon) {
      /* [b1 lfemixlevcode] */
      READ_BITS(&bsi->Mixdata.lfemixlevcode, bs, 1, return -1);

      if (bsi->Mixdata.lfemixlevcode) {
        /* [u5 lfemixlevcod] */
        READ_BITS(&bsi->Mixdata.lfemixlevcod, bs, 5, return -1);
      }
    }

    if (bsi->strmtyp == 0x0) {
      /* [b1 pgmscle] */
      READ_BITS(&bsi->Mixdata.pgmscle, bs, 1, return -1);

      if (bsi->Mixdata.pgmscle) {
        /* [u6 pgmscl] */
        READ_BITS(&bsi->Mixdata.pgmscl, bs, 6, return -1);
      }

      if (bsi->acmod == 0x0) {
        /* [b1 pgmscl2e] */
        READ_BITS(&bsi->Mixdata.pgmscl2e, bs, 1, return -1);

        if (bsi->Mixdata.pgmscl2e) {
          /* [u6 pgmscl2] */
          READ_BITS(&bsi->Mixdata.pgmscl2, bs, 6, return -1);
        }
      }

      /* [b1 extpgmscle] */
      READ_BITS(&bsi->Mixdata.extpgmscle, bs, 1, return -1);

      if (bsi->Mixdata.extpgmscle) {
        /* [u6 extpgmscl] */
        READ_BITS(&bsi->Mixdata.extpgmscl, bs, 6, return -1);
      }

      /* [u2 mixdef] */
      READ_BITS(&bsi->Mixdata.mixdef, bs, 2, return -1);

      if (bsi->Mixdata.mixdef == 0x1) {
        /* [b1 premixcmpsel] */
        READ_BITS(&bsi->Mixdata.premixcmpsel, bs, 1, return -1);

        /* [b1 drcsrc] */
        READ_BITS(&bsi->Mixdata.drcsrc, bs, 1, return -1);

        /* [u3 premixcmpscl] */
        READ_BITS(&bsi->Mixdata.premixcmpscl, bs, 3, return -1);
      }

      else if (bsi->Mixdata.mixdef == 0x2) {
        /* [v12 mixdata] */
        SKIP_BITS(bs, 12, return -1);
      }

      else if (bsi->Mixdata.mixdef == 0x3) {
        /* [u5 mixdeflen] */
        unsigned mixdeflen;
        READ_BITS(&mixdeflen, bs, 5, return -1);

        /* [v<(mixdeflen + 2) * 8> mixdata] */
        SKIP_BITS(bs, (mixdeflen + 2) << 3, return -1);
      }

      if (bsi->acmod < 0x2) {
        /* Mono or Dual-Mono */

        /* [b1 paninfoe] */
        bool paninfoe;
        READ_BITS(&paninfoe, bs, 1, return -1);

        if (paninfoe) {
          /* [u8 panmean] */
          /* [u6 paninfo] */
          SKIP_BITS(bs, 14, return -1);
        }

        if (bsi->acmod == 0x0) {
          /* Dual-Mono */

          /* [b1 paninfo2e] */
          bool paninfo2e;
          READ_BITS(&paninfo2e, bs, 1, return -1);

          if (paninfo2e) {
            /* [u8 panmean2] */
            /* [u6 paninfo2] */
            SKIP_BITS(bs, 14, return -1);
          }
        }
      }

      /* [b1 frmmixcfginfoe] */
      bool frmmixcfginfoe;
      READ_BITS(&frmmixcfginfoe, bs, 1, return -1);

      if (frmmixcfginfoe) {
        if (bsi->numblkscod == 0x0) {
          /* [u5 blkmixcfginfo[0]] */
          SKIP_BITS(bs, 5, return -1);
        }
        else {
          unsigned i;

          for (i = 0; i < bsi->numBlksPerSync; i++) {
            /* [b1 blkmixcfginfoe] */
            bool blkmixcfginfoe;
            READ_BITS(&blkmixcfginfoe, bs, 1, return -1);

            if (blkmixcfginfoe) {
              /* [u5 blkmixcfginfo[i]] */
              SKIP_BITS(bs, 5, return -1);
            }
          }
        }
      }
    }
  }

  /* [b1 infomdate] */
  READ_BITS(&bsi->infomdate, bs, 1, return -1);

  if (bsi->infomdate) {
    /* [u3 bsmod] */
    READ_BITS(&bsi->InformationalMetadata.bsmod, bs, 3, return -1);

    /* [b1 copyrightb] */
    READ_BITS(&bsi->InformationalMetadata.copyrightb, bs, 1, return -1);

    /* [b1 origbs] */
    READ_BITS(&bsi->InformationalMetadata.origbs, bs, 1, return -1);

    if (bsi->acmod == 0x2) {
      /* If Stereo 2.0 */

      /* [u2 dsurmod] */
      READ_BITS(&bsi->InformationalMetadata.dsurmod, bs, 2, return -1);

      /* [u2 dheadphonmod] */
      READ_BITS(&bsi->InformationalMetadata.dheadphonmod, bs, 2, return -1);
    }

    if (0x6 <= bsi->acmod) {
      /* If Both Surround channels present (2/2, 3/2 modes) */
      /* [u2 dsurexmod] */
      READ_BITS(&bsi->InformationalMetadata.dsurexmod, bs, 2, return -1);
    }

    /* [b1 audprodie] */
    READ_BITS(&bsi->InformationalMetadata.audprodie, bs, 1, return -1);

    if (bsi->InformationalMetadata.audprodie) {
      /* [u5 mixlevel] */
      READ_BITS(&bsi->InformationalMetadata.audprodi.mixlevel, bs, 5, return -1);

      /* [u2 roomtyp] */
      READ_BITS(&bsi->InformationalMetadata.audprodi.roomtyp, bs, 2, return -1);

      /* [b1 adconvtyp] */
      READ_BITS(&bsi->InformationalMetadata.audprodi.adconvtyp, bs, 1, return -1);
    }

    if (bsi->acmod == 0x0) {
      /* If Dual-Mono mode */

      /* [b1 audprodi2e] */
      READ_BITS(&bsi->InformationalMetadata.audprodi2e, bs, 1, return -1);

      if (bsi->InformationalMetadata.audprodi2e) {
        /* [u5 mixlevel2] */
        READ_BITS(&bsi->InformationalMetadata.audprodi2.mixlevel2, bs, 5, return -1);

        /* [u2 roomtyp2] */
        READ_BITS(&bsi->InformationalMetadata.audprodi2.roomtyp2, bs, 2, return -1);

        /* [b1 adconvtyp2] */
        READ_BITS(&bsi->InformationalMetadata.audprodi2.adconvtyp2, bs, 1, return -1);
      }
    }

    if (bsi->fscod < 0x3) {
      /* [b1 sourcefscod] */
      READ_BITS(&bsi->InformationalMetadata.sourcefscod, bs, 1, return -1);
    }
  }

  if (bsi->strmtyp == 0x0 && bsi->numblkscod != 0x3) {
    /* [b1 convsync] */
    READ_BITS(&bsi->convsync, bs, 1, return -1);
  }

  if (bsi->strmtyp == 0x2) {
    if (bsi->numblkscod == 0x3)
      bsi->blkid = true;
    else {
      /* [b1 blkid] */
      READ_BITS(&bsi->blkid, bs, 1, return -1);
    }

    if (bsi->blkid) {
      /* [u6 frmsizecod] */
      READ_BITS(&bsi->frmsizecod, bs, 6, return -1);
    }
  }

  /* [b1 addbsie] */
  READ_BITS(&bsi->addbsie, bs, 1, return -1);

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
  int64_t syncFrameOffset
)
{
  /* Skipping remaining frame bytes */
  int64_t remaining = bsi.frameSize - (tellPos(bs) - syncFrameOffset);
  if (skipBytes(bs, remaining) < 0)
    return -1;

  if (0x0 != completeCrcContext(crcCtxBitstream(bs)))
    LIBBLU_AC3_ERROR("CRC checksum error at the end of the frame.\n");

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
  /* [v4 check_nibble] [u12 access_unit_length] [u16 input_timing] */
  /* [v32 format_sync] */
  return (nextUint64(input) & 0xFFFFFFFF);
}

static bool _isPresentMlpMajorSyncFormatSyncWord(
  BitstreamReaderPtr input
)
{
  uint32_t format_sync = _getMlpMajorSyncFormatSyncWord(input) >> 8;

  return (MLP_SYNCWORD_PREFIX == format_sync);
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
    size += 10 + 3; // Major sync info (plus channel_meaning())
    if (chm->extra_channel_meaning_present)
      size += 1 + chm->extra_channel_meaning_length;
  }

  return size;
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


/* ######################################################################### */


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
    .extract_core = settings->options.extractCore
  };
  uint8_t * buffer = NULL;
  size_t buffer_size = 0;

  while (!isEof(bs)) {
    int64_t start_off = tellPos(bs);
    if (start_off < 0)
      LIBBLU_AC3_ERROR_RETURN("Unexpected negative file offset.\n");

    /* Progress bar : */
    printFileParsingProgressionBar(bs);

    if (nextUint16(bs) == AC3_SYNCWORD) {
      uint8_t bsid = _getSyncFrameBsid(bs);

      if (bsid <= 8) {
        /* AC-3 Bit stream syntax */
        LIBBLU_AC3_DEBUG(
          "0x%08" PRIX64 " === AC3 Core sync frame ===\n",
          start_off
        );

        /* syncinfo() */
        Ac3SyncInfoParameters syncinfo;
        if (_parseAc3SyncInfo(bs, &syncinfo) < 0)
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

        unsigned frame_size = _frmsizecodToFrameSize(syncinfo.frmsizecod, syncinfo.fscod);
        if (_parseRemainingAc3Frame(bs, syncinfo, start_off, frame_size) < 0)
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

        ret = appendAddPesPayloadCommand(script_hdl, src_file_idx, 0x0, start_off, frame_size);
        if (ret < 0)
          return -1;

        ctx.ac3.pts += ((uint64_t) MAIN_CLOCK_27MHZ * AC3_SAMPLES_PER_FRAME) / 48000;
        ctx.ac3.nb_frames++;
      }
      else if (10 < bsid && bsid <= 16) {
        /* E-AC-3 Bit stream syntax */
        LIBBLU_AC3_DEBUG(
          "0x%08" PRIX64 " === E-AC3 Sync frame ===\n",
          start_off
        );

        /* syncinfo() */
        if (_parseEac3SyncInfo(bs) < 0)
          return -1;

        /* Init CRC */
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

        if (_parseRemainingEac3Frame(bs, bsi, start_off) < 0)
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
      /* MLP/TrueHD Access Unit */
      LibbluBitReader br;

      LIBBLU_AC3_DEBUG(
        "0x%08" PRIX64 " === MLP/TrueHD Access Unit ===\n",
        start_off
      );

      /* mlp_sync_header */
      MlpSyncHeaderParameters mlp_sync_header;
      LIBBLU_MLP_DEBUG_PARSING_HDR(" MLP Sync, mlp_sync_header\n");
      if (parseMlpSyncHeader(bs, &br, &buffer, &buffer_size, &mlp_sync_header) < 0)
        return -1;

      if (0 < ctx.mlp.nb_frames) {
        // TODO: Constant check

        updateMlpSyncHeaderParametersParameters(
          &ctx.mlp.mlp_sync_header,
          mlp_sync_header
        );
      }
      else {
        if (checkMlpSyncHeader(&mlp_sync_header, &ctx.mlp.info, true) < 0)
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
        if (parseMlpSubstreamDirectoryEntry(&br, &entry) < 0)
          return -1;

        ctx.mlp.substream_directory[i] = entry;
      }

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

      int ret = checkAndComputeSSSizesMlpSubstreamDirectory(
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

      for (unsigned i = 0; i < ms_info->substreams; i++) {
        const MlpSubstreamDirectoryEntry * entry = &ctx.mlp.substream_directory[i];
        MlpSubstreamParameters * ss = &ctx.mlp.substreams[i];

        /* substream_segment(i) */
        if (decodeMlpSubstreamSegment(&br, ss, entry, i) < 0)
          return -1;
        // substream_parity/substream_CRC in substream_segment.
      }

      /* extra_start */
      if (remainingBitsLibbluBitReader(&br)) {
        if (decodeMlpExtraData(&br) < 0)
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
      "Unexpected sync word 0x%08" PRIX32 " at 0x%" PRIX64 ".\n",
      nextUint32(bs),
      start_off
    );
  }

  /* lbc_printf(" === Parsing finished with success. ===\n"); */
  closeBitstreamReader(bs);
  free(buffer);

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