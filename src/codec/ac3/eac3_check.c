#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "eac3_check.h"

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

int checkEac3BitStreamInfoCompliance(
  const Eac3BitStreamInfoParameters * bsi
)
{

  LIBBLU_EAC3_DEBUG(" Bit stream information, bsi()\n");

  LIBBLU_EAC3_DEBUG(
    "  Stream type (strmtyp): Type %u (0x%X).\n",
    bsi->strmtyp, bsi->strmtyp
  );
  LIBBLU_EAC3_DEBUG(
    "   -> %s.\n",
    Eac3StrmtypStr(bsi->strmtyp)
  );

  if (EAC3_STRMTYP_RES <= bsi->strmtyp)
    LIBBLU_EAC3_ERROR_RETURN(
      "Reserved value in use (strmtyp == 0x%x).\n",
      bsi->strmtyp
    );

  if (bsi->strmtyp != EAC3_STRMTYP_TYPE_1)
    LIBBLU_EAC3_ERROR_RETURN(
      "E-AC3 Bit Stream type must be independant "
      "(strmtyp == 0x%x, %s).\n",
      bsi->strmtyp,
      Eac3StrmtypStr(bsi->strmtyp)
    );

  LIBBLU_EAC3_DEBUG(
    "  Substream ID (substreamid): 0x%02" PRIX8 ".\n",
    bsi->substreamid
  );

  if (bsi->substreamid != 0x0)
    LIBBLU_EAC3_ERROR_RETURN(
      "Unexpected non-zero Substream Identification (substreamid == %u).\n",
      bsi->substreamid
    );

  LIBBLU_EAC3_DEBUG(
    "  Frame size (frmsiz): %" PRIu16 " words (0x%04" PRIX16 ").\n",
    bsi->frmsiz, bsi->frmsiz
  );
  LIBBLU_EAC3_DEBUG(
    "   -> %u bytes.\n",
    (bsi->frmsiz + 1) << 1
  );

  LIBBLU_EAC3_DEBUG(
    "  Sample Rate Code (fscod): %" PRIu8 " (0x%02" PRIX8 ").\n",
    bsi->fscod, bsi->fscod
  );
  LIBBLU_EAC3_DEBUG(
    "   -> %s.\n",
    Ac3FscodStr(bsi->fscod, true)
  );
  /* Check for fscod value is delayed to display sample rate after fscod2 */

  if (bsi->fscod == EAC3_FSCOD_RES_FSCOD2) {
    LIBBLU_EAC3_DEBUG(
      "  Sample Rate Code 2 (fscod2): %" PRIu8 " (0x%02" PRIX8 ").\n",
      bsi->fscod2, bsi->fscod2
    );
    LIBBLU_EAC3_DEBUG(
      "   -> %s.\n",
      Eac3Fscod2Str(bsi->fscod2)
    );

    if (EAC3_FSCOD2_RES <= bsi->fscod2)
      LIBBLU_EAC3_ERROR_RETURN(
        "Reserved value in use (fscod2 == 0x%x).\n",
        bsi->fscod2
      );

    LIBBLU_EAC3_DEBUG(
      "  Number of Audio Blocks (numblkscod): 0x%02" PRIX8 " (induced).\n",
      bsi->numblkscod
    );
  }
  else {
    LIBBLU_EAC3_DEBUG(
      "  Number of Audio Blocks (numblkscod): 0x%02" PRIX8 ".\n",
      bsi->numblkscod
    );
  }

  LIBBLU_EAC3_DEBUG(
    "   -> %u block(s) per syncframe.\n",
    bsi->numblks
  );

  if (EAC3_FSCOD_48000_HZ != bsi->fscod)
    LIBBLU_EAC3_ERROR_RETURN(
      "Unexpected sample rate of %u Hz, only 48 kHz bitstreams are allowed "
      "(fscod == 0x%x, fscod2 == 0x%x).\n",
      bsi->fscod, bsi->fscod2
    );

  LIBBLU_EAC3_DEBUG(
    "  Audio Coding Mode (acmod): 0x%02" PRIX8 ".\n",
    bsi->acmod
  );

  if (AC3_ACMOD_CH1_CH2 == bsi->acmod) {
    LIBBLU_EAC3_COMPLIANCE_ERROR_RETURN(
      "Invalid dual mono audio coding mode (acmod == 0x0).\n"
    );
  }

  LIBBLU_EAC3_DEBUG(
    "  Low Frequency Effects (LFE) channel (lfeon): 0x%02" PRIX8 ".\n",
    bsi->lfeon
  );
  LIBBLU_EAC3_DEBUG(
    "   -> %s.\n",
    BOOL_PRESENCE(bsi->lfeon)
  );

  unsigned nb_channels = nbChannelsAc3Acmod(bsi->acmod, bsi->lfeon);

  LIBBLU_EAC3_DEBUG(
    "   => %s (%u channels).\n",
    Ac3AcmodStr(bsi->acmod, bsi->lfeon),
    nb_channels
  );

  LIBBLU_EAC3_DEBUG(
    "  Bit Stream Identification (bsid): %" PRIu8 " (0x%02" PRIx8 ").\n",
    bsi->bsid, bsi->bsid
  );

  LIBBLU_EAC3_DEBUG(
    "  Dialogue Normalization (dialnorm): 0x%02" PRIX8 ".\n",
    bsi->dialnorm
  );
  LIBBLU_EAC3_DEBUG(
    "   -> -%" PRIu8 " dB.\n",
    bsi->dialnorm
  );

  /* TODO: Is fatal ? */
  if (!bsi->dialnorm)
    LIBBLU_AC3_WARNING(
      "Non-fatal reserved value in use (dialnorm == 0x%" PRIX8 ").\n",
      bsi->dialnorm
    );

  LIBBLU_EAC3_DEBUG(
    "  Compression Gain Word Exists (compre): %s (0b%x).\n",
    BOOL_STR(bsi->compre),
    bsi->compre
  );
  if (bsi->compre) {
    LIBBLU_EAC3_DEBUG(
      "   Compression Gain Word (compr): 0x%02" PRIX8 ".\n",
      bsi->compr
    );
  }

  if (bsi->acmod == AC3_ACMOD_CH1_CH2) {
    /* Dual Mono */
    LIBBLU_EAC3_DEBUG(
      "  Dual-Mono (acmod == 0x00) specific parameters:\n"
    );

    LIBBLU_EAC3_DEBUG(
      "   Dialogue Normalization for ch2 (dialnorm2): 0x%02" PRIX8 ".\n",
      bsi->dual_mono.dialnorm2
    );
    LIBBLU_EAC3_DEBUG(
      "    -> -%" PRIu8 " dB.\n",
      bsi->dual_mono.dialnorm2
    );

    /* TODO: Is fatal ? */
    if (!bsi->dual_mono.dialnorm2)
      LIBBLU_AC3_WARNING(
        "Non-fatal reserved value in use (dialnorm2 == 0x%" PRIX8 ").\n",
        bsi->dual_mono.dialnorm2
      );

    LIBBLU_EAC3_DEBUG(
      "   Compression Gain Word Exists for ch2 (compr2e): %s (0b%x).\n",
      BOOL_STR(bsi->dual_mono.compr2e),
      bsi->dual_mono.compr2e
    );
    if (bsi->dual_mono.compr2e) {
      LIBBLU_EAC3_DEBUG(
        "     Compression Gain Word for ch2 (compr2): 0x%02" PRIX8 ".\n",
        bsi->dual_mono.compr2
      );
    }
  }

  if (bsi->strmtyp == EAC3_STRMTYP_TYPE_1) {
    /* Dependent substream. */

    LIBBLU_EAC3_DEBUG(
      "  Custom Channel Map Exists (chanmape): %s (0b%x).\n",
      BOOL_STR(bsi->chanmape),
      bsi->chanmape
    );

    if (bsi->chanmape) {
      char chanmapRepr[EAC3_CHANMAP_STR_BUFSIZE];
      _buildStrReprEac3Chanmap(chanmapRepr, bsi->chanmap);

      unsigned nb_channels_chanmap = nbChannelsEac3Chanmap(bsi->chanmap);

      LIBBLU_EAC3_DEBUG(
        "   Custom Channel Map (chanmap): 0x%04" PRIX16 ".\n",
        bsi->chanmap
      );
      LIBBLU_EAC3_DEBUG(
        "    -> %s (%u channel(s)).\n",
        chanmapRepr,
        nb_channels_chanmap
      );

      /* [1] E.1.3.1.8 / [2] 2.3.1.8 Violation */
      if (nb_channels != nb_channels_chanmap)
        LIBBLU_EAC3_ERROR_RETURN(
          "Unexpected Custom Channel Map, the number of speakers does not match "
          "the expected value according to 'acmod' and 'lfeon' fields "
          "(expect %u, got %u).\n",
          nb_channels,
          nb_channels_chanmap
        );
    }
  }

  LIBBLU_EAC3_DEBUG(
    "  Mixing Meta-Data Exists (mixmdate): %s (0b%x).\n",
    BOOL_STR(bsi->mixmdate),
    bsi->mixmdate
  );

  if (bsi->mixmdate) {
    if (AC3_ACMOD_L_R < bsi->acmod) {
      /* More than two channels. */

      LIBBLU_EAC3_DEBUG(
        "   Preferred Stereo Downmix Mode (dmixmod): 0x%x.\n",
        bsi->Mixdata.dmixmod
      );
      LIBBLU_EAC3_DEBUG(
        "    -> %s.\n",
        Ac3DmixmodStr(bsi->Mixdata.dmixmod)
      );
    }

    if (threeFrontChannelsPresentAc3Acmod(bsi->acmod)) {
      /* Three front channels present. */

      LIBBLU_EAC3_DEBUG(
        "   Lt/Rt Center Mix Level (ltrtcmixlev): 0x%x.\n",
        bsi->Mixdata.ltrtcmixlev
      );
      LIBBLU_EAC3_DEBUG(
        "    -> %s.\n",
        Ac3LtrtcmixlevStr(bsi->Mixdata.ltrtcmixlev)
      );

      LIBBLU_EAC3_DEBUG(
        "   Lo/Ro Center Mix Level (lorocmixlev): 0x%x.\n",
        bsi->Mixdata.lorocmixlev
      );
      LIBBLU_EAC3_DEBUG(
        "    -> %s.\n",
        Ac3lLorocmixlevStr(bsi->Mixdata.lorocmixlev)
      );
    }

    if (isSurroundAc3Acmod(bsi->acmod)) {
      /* Surround channels present. */

      LIBBLU_EAC3_DEBUG(
        "   Lt/Rt Surround Mix Level (ltrtsurmixlev): 0x%x.\n",
        bsi->Mixdata.ltrtsurmixlev
      );
      LIBBLU_EAC3_DEBUG(
        "    -> Surround mixing level coefficient (slev): %s.\n",
        Ac3LtrtsurmixlevStr(bsi->Mixdata.ltrtsurmixlev)
      );

      if (bsi->Mixdata.ltrtsurmixlev <= AC3_LTRTSURMIXLEV_RES_2)
        LIBBLU_AC3_WARNING(
          "Non-fatal reserved value in use (ltrtsurmixlev == 0x%" PRIX8 ", "
          "interpreted as 0x3, '0.841 (-1.5 dB)').\n",
          bsi->Mixdata.ltrtsurmixlev
        );

      LIBBLU_EAC3_DEBUG(
        "   Lo/Ro Surround Mix Level (lorosurmixlev): 0x%x.\n",
        bsi->Mixdata.lorosurmixlev
      );
      LIBBLU_EAC3_DEBUG(
        "    -> Surround mixing level coefficient (slev): %s.\n",
        Ac3LorosurmixlevStr(bsi->Mixdata.lorosurmixlev)
      );

      if (bsi->Mixdata.lorosurmixlev <= AC3_LOROSURMIXLEV_RES_2)
        LIBBLU_AC3_WARNING(
          "Non-fatal reserved value in use (lorosurmixlev == 0x%" PRIX8 ", "
          "interpreted as 0x3, '0.841 (-1.5 dB)').\n",
          bsi->Mixdata.lorosurmixlev
        );
    }

    if (bsi->lfeon) {

      LIBBLU_EAC3_DEBUG(
        "   LFE mix level code exists (lfemixlevcode): %s (0b%x).\n",
        BOOL_STR(bsi->Mixdata.lfemixlevcode),
        bsi->Mixdata.lfemixlevcode
      );

      if (bsi->Mixdata.lfemixlevcode) {

        LIBBLU_EAC3_DEBUG(
          "    LFE mix level code (lfemixlevcod): "
          "%" PRIu8 " (0x%02" PRIX8 ").\n",
          bsi->Mixdata.lfemixlevcod
        );
        LIBBLU_EAC3_DEBUG(
          "     -> LFE mix level: %d dB.\n",
          10 - (int) bsi->Mixdata.lfemixlevcod
        );
      }
    }

    if (bsi->strmtyp == EAC3_STRMTYP_TYPE_0) {
      LIBBLU_EAC3_DEBUG(
        "   Independant stream type (strmtyp == 0x00) specific parameters:\n"
      );

      /* TODO */
      LIBBLU_EAC3_DEBUG(
        "    *Not checked*\n"
      );
    }
  }

  LIBBLU_EAC3_DEBUG(
    "  Informational Metadata Exists (infomdate): %s (0b%x).\n",
    BOOL_STR(bsi->infomdate),
    bsi->infomdate
  );

  if (bsi->infomdate) {
    const Eac3Infomdat * infParam = &bsi->infomdat;

    LIBBLU_EAC3_DEBUG(
      "   Bit Stream Mode (bsmod): 0x%02" PRIX8 ".\n",
      infParam->bsmod
    );
    LIBBLU_EAC3_DEBUG(
      "    -> %s.\n",
      Ac3BsmodStr(infParam->bsmod, bsi->acmod)
    );

    LIBBLU_EAC3_DEBUG(
      "   Copyrighted stream (copyrightb): %s (0b%x).\n",
      (infParam->copyrightb) ? "Protected" : "Not indicated",
      infParam->copyrightb
    );

    LIBBLU_EAC3_DEBUG(
      "   Original Bit Stream (origbs): %s (0b%x).\n",
      (infParam->origbs) ? "Original" : "Copy of another bitsream",
      infParam->origbs
    );

    if (bsi->acmod == AC3_ACMOD_L_R) {
      /* if in 2/0 mode */

      LIBBLU_EAC3_DEBUG(
        "   Dolby Surround Mode (dsurmod): 0x%02" PRIX8 ".\n",
        infParam->dsurmod
      );
      LIBBLU_EAC3_DEBUG(
        "    -> %s.\n",
        Ac3DsurmodStr(infParam->dsurmod)
      );

      if (AC3_DSURMOD_RES == infParam->dsurmod)
        LIBBLU_AC3_WARNING(
          "Non-fatal reserved value in use (dsurmod == 0x%" PRIX8 ", "
          "interpreted as 0x0, 'not indicated').\n",
          infParam->dsurmod
        );

      LIBBLU_EAC3_DEBUG(
        "   Dolby Headphone Mode (dheadphonmod): 0x%X.\n",
        infParam->dheadphonmod
      );
      LIBBLU_EAC3_DEBUG(
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

    if (bothSurroundChannelsPresentAc3mod(bsi->acmod)) {
      /* if both surround channels exist */

      LIBBLU_EAC3_DEBUG(
        "   Dolby Surround EX Mode (dsurexmod): 0x%X.\n",
        infParam->dsurexmod
      );
      LIBBLU_EAC3_DEBUG(
        "    -> %s.\n",
        Ac3DsurexmodStr(infParam->dsurexmod)
      );
    }

    LIBBLU_EAC3_DEBUG(
      "   Audio Production Information Exists (audprodie): %s (0b%x).\n",
      BOOL_STR(infParam->audprodie),
      infParam->audprodie
    );

    if (infParam->audprodie) {

      LIBBLU_EAC3_DEBUG(
        "    Mixing Level (mixlevel): %" PRIu8 " (0x%02" PRIX8 ").\n",
        infParam->audprodi.mixlevel,
        infParam->audprodi.mixlevel
      );
      LIBBLU_EAC3_DEBUG(
        "     -> %u dB SPL.\n",
        80u + infParam->audprodi.mixlevel
      );

      LIBBLU_EAC3_DEBUG(
        "    Room Type (roomtyp): %" PRIu8 " (0x%02" PRIX8 ").\n",
        infParam->audprodi.roomtyp,
        infParam->audprodi.roomtyp
      );
      LIBBLU_EAC3_DEBUG(
        "     -> %s.\n",
        Ac3RoomtypStr(infParam->audprodi.roomtyp)
      );

      if (AC3_ROOMTYP_RES == infParam->audprodi.roomtyp)
        LIBBLU_AC3_WARNING(
          "Non-fatal reserved value in use (roomtyp == 0x%" PRIX8 ", "
          "interpreted as 0x0, 'not indicated').\n",
          infParam->audprodi.roomtyp
        );

      LIBBLU_EAC3_DEBUG(
        "    A/D Converter Type (adconvtyp): %s (0b%x).\n",
        BOOL_STR(infParam->audprodi.adconvtyp),
        infParam->audprodi.adconvtyp
      );
      LIBBLU_EAC3_DEBUG(
        "     -> %s.\n",
        (infParam->audprodi.adconvtyp) ? "HDCD" : "Standard"
      );
    }

    if (bsi->acmod == AC3_ACMOD_CH1_CH2) {
      /* Dual Mono */
      LIBBLU_EAC3_DEBUG(
        "   Dual-Mono (acmod == 0x00) specific parameters:\n"
      );

      LIBBLU_EAC3_DEBUG(
        "    Audio Production Information Exists for ch2 (audprodi2e): "
        "%s (0b%x).\n",
        BOOL_STR(infParam->audprodi2e),
        infParam->audprodi2e
      );

      if (infParam->audprodi2e) {

        LIBBLU_EAC3_DEBUG(
          "     Mixing Level for ch2 (mixlevel2): "
          "%" PRIu8 " (0x%02" PRIX8 ").\n",
          infParam->audprodi2.mixlevel2,
          infParam->audprodi2.mixlevel2
        );
        LIBBLU_EAC3_DEBUG(
          "      -> %u dB SPL.\n",
          80u + infParam->audprodi2.mixlevel2
        );

        LIBBLU_EAC3_DEBUG(
          "     Room Type for ch2 (roomtyp2): %" PRIu8 " (0x%02" PRIX8 ").\n",
          infParam->audprodi2.roomtyp2,
          infParam->audprodi2.roomtyp2
        );
        LIBBLU_EAC3_DEBUG(
          "      -> %s.\n",
          Ac3RoomtypStr(infParam->audprodi2.roomtyp2)
        );

        if (AC3_ROOMTYP_RES == infParam->audprodi2.roomtyp2)
          LIBBLU_AC3_WARNING(
            "Non-fatal reserved value in use (roomtyp2 == 0x%" PRIX8 ", "
            "interpreted as 0x0, 'not indicated').\n",
            infParam->audprodi2.roomtyp2
          );

        LIBBLU_EAC3_DEBUG(
          "     A/D Converter Type for ch2 (adconvtyp2): %s (0b%x).\n",
          BOOL_STR(infParam->audprodi2.adconvtyp2),
          infParam->audprodi2.adconvtyp2
        );
        LIBBLU_EAC3_DEBUG(
          "      -> %s.\n",
          (infParam->audprodi2.adconvtyp2) ? "HDCD" : "Standard"
        );
      }
    }

    if (notHalfSampleRateAc3Fscod(bsi->fscod)) {
      /* if not half sample rate */

      LIBBLU_EAC3_DEBUG(
        "   Source Sample Rate Code (sourcefscod): %s (0b%x).\n",
        infParam->sourcefscod,
        infParam->sourcefscod
      );

      if (infParam->sourcefscod)
        LIBBLU_EAC3_DEBUG(
          "    -> Source material was sampled at twice the sample rate "
          "indicated by fscod.\n"
        );
      else
        LIBBLU_EAC3_DEBUG(
          "    -> Source material was sampled at the same sample rate "
          "indicated by fscod.\n"
        );
    }
  }

  /* TODO : convsync */
  if (
    bsi->strmtyp == EAC3_STRMTYP_TYPE_0
    && bsi->numblkscod != EAC3_NUMBLKSCOD_6_BLOCKS
  ) {
    /* If independant substream with fewer than 6 blocks of audio. */

    LIBBLU_EAC3_DEBUG(
      "  Converter Synchronization Flag (convsync): %s (0b%x).\n",
      BOOL_STR(bsi->convsync),
      bsi->convsync
    );
  }

  if (bsi->strmtyp == EAC3_STRMTYP_TYPE_2) {
    /* if bit stream converted from AC-3 */

    if (bsi->numblkscod == EAC3_NUMBLKSCOD_6_BLOCKS) {
      LIBBLU_EAC3_DEBUG(
        "  Block Identification (blkid): %s (0b%x, induced).\n",
        BOOL_STR(bsi->blkid),
        bsi->blkid
      );
    }
    else {
      LIBBLU_EAC3_DEBUG(
        "  Block Identification (blkid): %s (0b%x).\n",
        BOOL_STR(bsi->blkid),
        bsi->blkid
      );
    }

    if (bsi->blkid) {
      LIBBLU_EAC3_DEBUG(
        "   Frame Size Code (frmsizecod): 0x%02" PRIX8 ".\n",
        bsi->frmsizecod
      );
    }
  }

  LIBBLU_EAC3_DEBUG(
    "  Additional Bit Stream Information Exists (addbsie): %s (0b%x).\n",
    BOOL_STR(bsi->addbsie),
    bsi->addbsie
  );

  if (bsi->addbsie) {
    if (checkAc3AddbsiCompliance(&bsi->addbsi) < 0)
      return -1;
  }

  return 0; /* OK */
}

int checkChangeEac3BitStreamInfoCompliance(
  const Eac3BitStreamInfoParameters * old_bsi,
  const Eac3BitStreamInfoParameters * new_bsi
)
{
  char changed_param_str[20 * 10] = {0};
  char * cpstr_ptr = changed_param_str;

  LIBBLU_EAC3_DEBUG(" Changes in bsi(), check change compliance.\n");
  if (checkEac3BitStreamInfoCompliance(new_bsi) < 0)
    return -1;

  if (old_bsi->frmsiz != new_bsi->frmsiz)
    lb_str_cat_comma(&cpstr_ptr, "frmsiz", cpstr_ptr != changed_param_str);

  if (old_bsi->fscod != new_bsi->fscod)
    lb_str_cat_comma(&cpstr_ptr, "fscod", cpstr_ptr != changed_param_str);

  if (old_bsi->numblkscod != new_bsi->numblkscod)
    lb_str_cat_comma(&cpstr_ptr, "numblkscod", cpstr_ptr != changed_param_str);

  if (old_bsi->acmod != new_bsi->acmod)
    lb_str_cat_comma(&cpstr_ptr, "acmod", cpstr_ptr != changed_param_str);

  if (old_bsi->lfeon != new_bsi->lfeon)
    lb_str_cat_comma(&cpstr_ptr, "lfeon", cpstr_ptr != changed_param_str);

  if (old_bsi->bsid != new_bsi->bsid)
    lb_str_cat_comma(&cpstr_ptr, "bsid", cpstr_ptr != changed_param_str);

  if (old_bsi->chanmape != new_bsi->chanmape)
    lb_str_cat_comma(&cpstr_ptr, "chanmape", cpstr_ptr != changed_param_str);
  if (old_bsi->chanmape && new_bsi->chanmape) {
    if (old_bsi->chanmap != new_bsi->chanmap)
      lb_str_cat_comma(&cpstr_ptr, "chanmap", cpstr_ptr != changed_param_str);
  }

  if (old_bsi->infomdate != new_bsi->infomdate)
    lb_str_cat_comma(&cpstr_ptr, "infomdate", cpstr_ptr != changed_param_str);
  if (old_bsi->infomdate && new_bsi->infomdate) {
    if (old_bsi->infomdat.bsmod != new_bsi->infomdat.bsmod)
      lb_str_cat_comma(&cpstr_ptr, "bsmod", cpstr_ptr != changed_param_str);
  }

  if (changed_param_str != cpstr_ptr)
    LIBBLU_EAC3_ERROR_RETURN(
      "Non compliant parameters change in bsi(): %s.\n",
      changed_param_str
    );
  return 0;
}
