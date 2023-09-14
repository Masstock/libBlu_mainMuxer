#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include "eac3_parser.h"


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


int parseEac3SyncInfo(
  LibbluBitReader * br
)
{
  /* For (bsid == 16) syntax */

  LIBBLU_AC3_DEBUG(" Synchronization Information, syncinfo()\n");

  /* [v16 syncword] // 0x0B77 */
  uint16_t syncword;
  READ_BITS(&syncword, br, 16, return -1);

  LIBBLU_AC3_DEBUG(
    "  Sync Word (syncword): 0x%04" PRIX16 ".\n",
    syncword
  );

  assert(syncword == AC3_SYNCWORD);

  return 0;
}


int parseEac3BitStreamInfo(
  LibbluBitReader * br,
  Eac3BitStreamInfoParameters * bsi
)
{
  /* For (bsid == 0x10) syntax */

  /* [u2 strmtyp] */
  READ_BITS(&bsi->strmtyp, br, 2, return -1);

  /* [u3 substreamid] */
  READ_BITS(&bsi->substreamid, br, 3, return -1);

  /* [u11 frmsiz] */
  READ_BITS(&bsi->frmsiz, br, 11, return -1);

  /* [u2 fscod] */
  READ_BITS(&bsi->fscod, br, 2, return -1);

  if (bsi->fscod == 0x3) {
    /* [u2 fscod2] */
    READ_BITS(&bsi->fscod2, br, 2, return -1);

    bsi->numblkscod = EAC3_NUMBLKSCOD_6_BLOCKS;
  }
  else {
    bsi->fscod2 = 0x3;

    /* [u2 numblkscod] */
    READ_BITS(&bsi->numblkscod, br, 2, return -1);
  }

  /* Converting numblkscod to numblks (number of blocks) in Sync Frame. */
  bsi->numblks = nbBlocksEac3Numblkscod(bsi->numblkscod);

  /* [u3 acmod] */
  READ_BITS(&bsi->acmod, br, 3, return -1);

  /* [b1 lfeon] */
  READ_BITS(&bsi->lfeon, br, 1, return -1);

  /* [u5 bsid] // 0x10/16 in this syntax */
  READ_BITS(&bsi->bsid, br, 5, return -1);

  if (bsi->bsid <= 10 || 16 < bsi->bsid)
    LIBBLU_AC3_ERROR_RETURN(
      "Unexpected or unsupported Bit Stream Identifier "
      "(bsid == %" PRIu8 ", parser expect 16).\n",
      bsi->bsid
    );

  /* [u5 dialnorm] */
  READ_BITS(&bsi->dialnorm, br, 5, return -1);

  /* [b1 compre] */
  READ_BITS(&bsi->compre, br, 1, return -1);

  if (bsi->compre) {
    /* [u8 compr] */
    READ_BITS(&bsi->compr, br, 8, return -1);
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
  }

  if (bsi->strmtyp == 0x1) {
    /* [b1 chanmape] */
    READ_BITS(&bsi->chanmape, br, 1, return -1);

    if (bsi->chanmape) {
      /* [u16 chanmap] */
      READ_BITS(&bsi->chanmap, br, 16, return -1);
    }
  }

  /* [b1 mixmdate] */
  READ_BITS(&bsi->mixmdate, br, 1, return -1);

  if (bsi->mixmdate) {
    /* Mixing meta-data */
    /* Not checked. */

    if (0x2 < bsi->acmod) {
      /* More than two channels. */

      /* [u2 dmixmod] */
      READ_BITS(&bsi->Mixdata.dmixmod, br, 2, return -1);
    }

    if ((bsi->acmod & 0x1) && (0x2 < bsi->acmod)) {
      /* Three front channels present. */

      /* [u3 ltrtcmixlev] */
      READ_BITS(&bsi->Mixdata.ltrtcmixlev, br, 3, return -1);

      /* [u3 lorocmixlev] */
      READ_BITS(&bsi->Mixdata.lorocmixlev, br, 3, return -1);
    }

    if (bsi->acmod & 0x4) {
      /* Surround channels present. */

      /* [u3 ltrtsurmixlev] */
      READ_BITS(&bsi->Mixdata.ltrtsurmixlev, br, 3, return -1);

      /* [u3 lorosurmixlev] */
      READ_BITS(&bsi->Mixdata.lorosurmixlev, br, 3, return -1);
    }

    if (bsi->lfeon) {
      /* [b1 lfemixlevcode] */
      READ_BITS(&bsi->Mixdata.lfemixlevcode, br, 1, return -1);

      if (bsi->Mixdata.lfemixlevcode) {
        /* [u5 lfemixlevcod] */
        READ_BITS(&bsi->Mixdata.lfemixlevcod, br, 5, return -1);
      }
    }

    if (bsi->strmtyp == 0x0) {
      /* [b1 pgmscle] */
      READ_BITS(&bsi->Mixdata.pgmscle, br, 1, return -1);

      if (bsi->Mixdata.pgmscle) {
        /* [u6 pgmscl] */
        READ_BITS(&bsi->Mixdata.pgmscl, br, 6, return -1);
      }

      if (bsi->acmod == 0x0) {
        /* [b1 pgmscl2e] */
        READ_BITS(&bsi->Mixdata.pgmscl2e, br, 1, return -1);

        if (bsi->Mixdata.pgmscl2e) {
          /* [u6 pgmscl2] */
          READ_BITS(&bsi->Mixdata.pgmscl2, br, 6, return -1);
        }
      }

      /* [b1 extpgmscle] */
      READ_BITS(&bsi->Mixdata.extpgmscle, br, 1, return -1);

      if (bsi->Mixdata.extpgmscle) {
        /* [u6 extpgmscl] */
        READ_BITS(&bsi->Mixdata.extpgmscl, br, 6, return -1);
      }

      /* [u2 mixdef] */
      READ_BITS(&bsi->Mixdata.mixdef, br, 2, return -1);

      if (bsi->Mixdata.mixdef == 0x1) {
        /* [b1 premixcmpsel] */
        READ_BITS(&bsi->Mixdata.premixcmpsel, br, 1, return -1);

        /* [b1 drcsrc] */
        READ_BITS(&bsi->Mixdata.drcsrc, br, 1, return -1);

        /* [u3 premixcmpscl] */
        READ_BITS(&bsi->Mixdata.premixcmpscl, br, 3, return -1);
      }

      else if (bsi->Mixdata.mixdef == 0x2) {
        /* [v12 mixdata] */
        SKIP_BITS(br, 12, return -1);
      }

      else if (bsi->Mixdata.mixdef == 0x3) {
        /* [u5 mixdeflen] */
        unsigned mixdeflen;
        READ_BITS(&mixdeflen, br, 5, return -1);

        /* [v<(mixdeflen + 2) * 8> mixdata] */
        SKIP_BITS(br, (mixdeflen + 2) << 3, return -1);
      }

      if (bsi->acmod < 0x2) {
        /* Mono or Dual-Mono */

        /* [b1 paninfoe] */
        bool paninfoe;
        READ_BITS(&paninfoe, br, 1, return -1);

        if (paninfoe) {
          /* [u8 panmean] */
          /* [u6 paninfo] */
          SKIP_BITS(br, 14, return -1);
        }

        if (bsi->acmod == 0x0) {
          /* Dual-Mono */

          /* [b1 paninfo2e] */
          bool paninfo2e;
          READ_BITS(&paninfo2e, br, 1, return -1);

          if (paninfo2e) {
            /* [u8 panmean2] */
            /* [u6 paninfo2] */
            SKIP_BITS(br, 14, return -1);
          }
        }
      }

      /* [b1 frmmixcfginfoe] */
      bool frmmixcfginfoe;
      READ_BITS(&frmmixcfginfoe, br, 1, return -1);

      if (frmmixcfginfoe) {
        if (bsi->numblkscod == 0x0) {
          /* [u5 blkmixcfginfo[0]] */
          SKIP_BITS(br, 5, return -1);
        }
        else {
          for (unsigned i = 0; i < bsi->numblks; i++) {
            /* [b1 blkmixcfginfoe] */
            bool blkmixcfginfoe;
            READ_BITS(&blkmixcfginfoe, br, 1, return -1);

            if (blkmixcfginfoe) {
              /* [u5 blkmixcfginfo[i]] */
              SKIP_BITS(br, 5, return -1);
            }
          }
        }
      }
    }
  }

  /* [b1 infomdate] */
  READ_BITS(&bsi->infomdate, br, 1, return -1);

  if (bsi->infomdate) {
    /* [u3 bsmod] */
    READ_BITS(&bsi->infomdat.bsmod, br, 3, return -1);

    /* [b1 copyrightb] */
    READ_BITS(&bsi->infomdat.copyrightb, br, 1, return -1);

    /* [b1 origbs] */
    READ_BITS(&bsi->infomdat.origbs, br, 1, return -1);

    if (bsi->acmod == 0x2) {
      /* If Stereo 2.0 */

      /* [u2 dsurmod] */
      READ_BITS(&bsi->infomdat.dsurmod, br, 2, return -1);

      /* [u2 dheadphonmod] */
      READ_BITS(&bsi->infomdat.dheadphonmod, br, 2, return -1);
    }

    if (0x6 <= bsi->acmod) {
      /* If Both Surround channels present (2/2, 3/2 modes) */
      /* [u2 dsurexmod] */
      READ_BITS(&bsi->infomdat.dsurexmod, br, 2, return -1);
    }

    /* [b1 audprodie] */
    READ_BITS(&bsi->infomdat.audprodie, br, 1, return -1);

    if (bsi->infomdat.audprodie) {
      /* [u5 mixlevel] */
      READ_BITS(&bsi->infomdat.audprodi.mixlevel, br, 5, return -1);

      /* [u2 roomtyp] */
      READ_BITS(&bsi->infomdat.audprodi.roomtyp, br, 2, return -1);

      /* [b1 adconvtyp] */
      READ_BITS(&bsi->infomdat.audprodi.adconvtyp, br, 1, return -1);
    }

    if (bsi->acmod == 0x0) {
      /* If Dual-Mono mode */

      /* [b1 audprodi2e] */
      READ_BITS(&bsi->infomdat.audprodi2e, br, 1, return -1);

      if (bsi->infomdat.audprodi2e) {
        /* [u5 mixlevel2] */
        READ_BITS(&bsi->infomdat.audprodi2.mixlevel2, br, 5, return -1);

        /* [u2 roomtyp2] */
        READ_BITS(&bsi->infomdat.audprodi2.roomtyp2, br, 2, return -1);

        /* [b1 adconvtyp2] */
        READ_BITS(&bsi->infomdat.audprodi2.adconvtyp2, br, 1, return -1);
      }
    }

    if (bsi->fscod < 0x3) {
      /* [b1 sourcefscod] */
      READ_BITS(&bsi->infomdat.sourcefscod, br, 1, return -1);
    }
  }

  if (bsi->strmtyp == 0x0 && bsi->numblkscod != 0x3) {
    /* [b1 convsync] */
    READ_BITS(&bsi->convsync, br, 1, return -1);
  }

  if (bsi->strmtyp == 0x2) {
    if (bsi->numblkscod == 0x3)
      bsi->blkid = true;
    else {
      /* [b1 blkid] */
      READ_BITS(&bsi->blkid, br, 1, return -1);
    }

    if (bsi->blkid) {
      /* [u6 frmsizecod] */
      READ_BITS(&bsi->frmsizecod, br, 6, return -1);
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