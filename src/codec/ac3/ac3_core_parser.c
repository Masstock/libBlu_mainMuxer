#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include "ac3_core_parser.h"


#define READ_BITS_BS(d, bs, s, e)                                             \
  do {                                                                        \
    uint32_t _val;                                                            \
    if (readBits(bs, &_val, s) < 0)                                           \
      e;                                                                      \
    *(d) = _val;                                                              \
  } while (0)


#define READ_BITS(d, br, s, e)                                                \
  do {                                                                        \
    uint32_t _val;                                                            \
    if (readLibbluBitReader(br, &_val, s) < 0)                                \
      e;                                                                      \
    *(d) = _val;                                                              \
  } while (0)


#define SKIP_BITS(br, s, e)                                                   \
  do {                                                                        \
    if (s < remainingBitsLibbluBitReader(br)) {                               \
      LIBBLU_AC3_ERROR("Too many bits to skip, prematurate end of frame.\n"); \
      e;                                                                      \
    }                                                                         \
    skipLibbluBitReader(br, s);                                               \
  } while (0)


int parseAc3SyncInfo(
  BitstreamReaderPtr bs,
  Ac3SyncInfoParameters * syncinfo
)
{
  /* For (bsid <= 0x8) syntax */

  LIBBLU_AC3_DEBUG(" Synchronization Information, syncinfo()\n");

  /* [v16 syncword] // 0x0B77 */
  uint16_t syncword;
  READ_BITS_BS(&syncword, bs, 16, return -1);

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
  READ_BITS_BS(&syncinfo->crc1, bs, 16, return -1);

  /* [u2 fscod] */
  READ_BITS_BS(&syncinfo->fscod, bs, 2, return -1);

  /* [u6 frmsizecod] */
  READ_BITS_BS(&syncinfo->frmsizecod, bs, 6, return -1);

  return 0;
}


int parseAc3Addbsi(
  LibbluBitReader * br,
  Ac3Addbsi * addbsi
)
{

  /* [u6 addbsil] */
  READ_BITS(&addbsi->addbsil, br, 6, return -1);

  if (addbsi->addbsil == 1) {
    /* Assuming presence of EC3 Extension      */
    /* Reference: ETSI TS 103 420 V1.2.1 - 8.3 */

    /* Try [v7 reserved] [b1 flag_ec3_extension_type_a] ... */
    uint16_t content;
    READ_BITS(&content, br, 16, return -1);

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
    SKIP_BITS(br, (addbsi->addbsil + 1u) << 3u, return -1);
  }

  return 0;
}


int parseAc3BitStreamInfo(
  LibbluBitReader * br,
  Ac3BitStreamInfoParameters * bsi
)
{
  /* For (bsid <= 0x8) syntax */

  /* [u5 bsid] */
  READ_BITS(&bsi->bsid, br, 5, return -1);

  if (8 < bsi->bsid)
    LIBBLU_AC3_ERROR_RETURN(
      "Unexpected or unsupported Bit Stream Identifier "
      "(bsid == %" PRIu8 ").\n",
      bsi->bsid
    );

  /* [u3 bsmod] */
  READ_BITS(&bsi->bsmod, br, 3, return -1);

  /* [u3 acmod] */
  READ_BITS(&bsi->acmod, br, 3, return -1);

  if (threeFrontChannelsPresentAc3Acmod(bsi->acmod)) {
    /* If 3 front channels present. */
    /* [u2 cmixlevel] */
    READ_BITS(&bsi->cmixlev, br, 2, return -1);
  }

  if (isSurroundAc3Acmod(bsi->acmod)) {
    /* If surround channel(s) present. */
    /* [u2 surmixlev] */
    READ_BITS(&bsi->surmixlev, br, 2, return -1);
  }

  if (bsi->acmod == AC3_ACMOD_L_R) {
    /* If Stereo 2/0 (L, R) */
    /* [u2 dsurmod] */
    READ_BITS(&bsi->dsurmod, br, 2, return -1);
  }

  /* [b1 lfeon] */
  READ_BITS(&bsi->lfeon, br, 1, return -1);

  bsi->nbChannels = nbChannelsAc3Acmod(
    bsi->acmod,
    bsi->lfeon
  );

  /* [u5 dialnorm] */
  READ_BITS(&bsi->dialnorm, br, 5, return -1);

  /* [b1 compre] */
  READ_BITS(&bsi->compre, br, 1, return -1);

  if (bsi->compre) {
    /* [u8 compr] */
    READ_BITS(&bsi->compr, br, 8, return -1);
  }

  /* [b1 langcode] */
  READ_BITS(&bsi->langcode, br, 1, return -1);

  if (bsi->langcode) {
    /* [u8 langcod] */
    READ_BITS(&bsi->langcod, br, 8, return -1);
  }

  /* [b1 audprodie] */
  READ_BITS(&bsi->audprodie, br, 1, return -1);

  if (bsi->audprodie) {
    /* [u5 mixlevel] */
    READ_BITS(&bsi->audprodi.mixlevel, br, 5, return -1);

    /* [u2 roomtyp] */
    READ_BITS(&bsi->audprodi.roomtyp, br, 2, return -1);
  }

  if (bsi->acmod == AC3_ACMOD_CH1_CH2) {
    /* If 1+1 duplicated mono, require parameters for the second mono builded channel. */

    /* [u5 dialnorm2] */
    READ_BITS(&bsi->dual_mono.dialnorm2, br, 5, return -1);

    /* [b1 compr2e] */
    READ_BITS(&bsi->dual_mono.compr2e, br, 1, return -1);

    if (bsi->dual_mono.compr2e) {
      /* [u8 compr2] */
      READ_BITS(&bsi->dual_mono.compr2, br, 8, return -1);
    }

    /* [b1 langcod2e] */
    READ_BITS(&bsi->dual_mono.langcod2e, br, 1, return -1);

    if (bsi->dual_mono.langcod2e) {
      /* [u8 langcod2] */
      READ_BITS(&bsi->dual_mono.langcod2, br, 8, return -1);
    }

    /* [b1 audprodi2e] */
    READ_BITS(&bsi->dual_mono.audprodi2e, br, 1, return -1);

    if (bsi->dual_mono.audprodi2e) {
      /* [u5 mixlevel2] */
      READ_BITS(&bsi->dual_mono.audprodi2.mixlevel2, br, 5, return -1);

      /* [u2 roomtyp2] */
      READ_BITS(&bsi->dual_mono.audprodi2.roomtyp2, br, 2, return -1);
    }
  }

  /* [b1 copyrightb] */
  READ_BITS(&bsi->copyrightb, br, 1, return -1);

  /* [b1 origbs] */
  READ_BITS(&bsi->origbs, br, 1, return -1);

  if (bsi->bsid == 0x6) {
    /* Alternate Bit Stream Syntax */
    /* ETSI TS 102 366 V1.4.1 - Annex D */

    /* [b1 xbsi1e] */
    READ_BITS(&bsi->xbsi1e, br, 1, return -1);

    if (bsi->xbsi1e) {
      /* [u2 dmixmod] */
      READ_BITS(&bsi->xbsi1.dmixmod, br, 2, return -1);

      /* [u3 ltrtcmixlev] */
      READ_BITS(&bsi->xbsi1.ltrtcmixlev, br, 3, return -1);

      /* [u3 ltrtsurmixlev] */
      READ_BITS(&bsi->xbsi1.ltrtsurmixlev, br, 3, return -1);

      /* [u3 lorocmixlev] */
      READ_BITS(&bsi->xbsi1.lorocmixlev, br, 3, return -1);

      /* [u3 lorosurmixlev] */
      READ_BITS(&bsi->xbsi1.lorosurmixlev, br, 3, return -1);
    }

    /* [b1 xbsi2e] */
    READ_BITS(&bsi->xbsi2e, br, 1, return -1);

    if (bsi->xbsi2e) {
      /* [u2 dsurexmod] */
      READ_BITS(&bsi->xbsi2.dsurexmod, br, 2, return -1);

      /* [u2 dheadphonmod] */
      READ_BITS(&bsi->xbsi2.dheadphonmod, br, 2, return -1);

      /* [b1 adconvtyp] */
      READ_BITS(&bsi->xbsi2.adconvtyp, br, 1, return -1);

      /* [u8 xbsi2] */
      READ_BITS(&bsi->xbsi2.xbsi2, br, 8, return -1);

      /* [b1 encinfo] */
      READ_BITS(&bsi->xbsi2.encinfo, br, 1, return -1);
    }
  }
  else {
    /* [b1 timecod1e] */
    READ_BITS(&bsi->timecod1e, br, 1, return -1);

    if (bsi->timecod1e) {
      /* [u16 timecod1] */
      READ_BITS(&bsi->timecod1, br, 16, return -1);
    }

    /* [b1 timecod2e] */
    READ_BITS(&bsi->timecod2e, br, 1, return -1);

    if (bsi->timecod2e) {
      /* [u16 timecod2] */
      READ_BITS(&bsi->timecod2, br, 16, return -1);
    }
  }

  /* [b1 addbsie] */
  READ_BITS(&bsi->addbsie, br, 1, return -1);

  if (bsi->addbsie) {
    if (parseAc3Addbsi(br, &bsi->addbsi) < 0)
      return -1;
  }

  if (padByteLibbluBitReader(br) < 0)
    return -1;

  return 0;
}
