/** \~english
 * \file ac3_data.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Dolby audio (AC-3, E-AC-3, TrueHD...) bitstreams data definitions.
 */

/** \~english
 * \dir ac3
 *
 * \brief Dolby audio (AC-3, E-AC-3, TrueHD...) bitstreams handling modules.
 *
 * \todo Split AC-3 parsing module in sub-modules to increase readability.
 * \todo E-AC-3 Parameters checking and secondary track support.
 * \todo Audio block parsing for extended checking (and Dolby Atmos audio
 * block verifications).
 * \todo Issues with some CRC checks.
 *
 * \xrefitem references "References" "References list"
 *  [1] AC-3/E-AC-3 - ETSI TS 102 366 V1.4.1;\n
 *  [2] AC-3/E-AC-3 - ATSC Standard A52-2018;\n
 *  [3] TrueHD - Dolby TrueHD (MLP) - High-level bitstream description;\n
 *  [4] Object Audio E-AC-3 - ETSI TS 103 420 V1.2.1;\n
 *  [5] ffmpeg source code.
 */

#ifndef __LIBBLU_MUXER__CODECS__AC3__DATA_H__
#define __LIBBLU_MUXER__CODECS__AC3__DATA_H__

#include "../../util.h"
#include "../common/constantCheckFunctionsMacros.h"

/** \~english
 * \brief AC-3/MLP word size in bytes.
 *
 */
#define AC3_WORD  2

/* ### AC-3 : ############################################################## */

#define AC3_SYNCWORD  0x0B77

typedef enum {
  EAC3_FSCOD_48000_HZ    = 0x0,
  EAC3_FSCOD_44100_HZ    = 0x1,
  EAC3_FSCOD_32000_HZ    = 0x2,
  EAC3_FSCOD_RES_FSCOD2  = 0x3
} Ac3Fscod;

static inline bool notHalfSampleRateAc3Fscod(
  Ac3Fscod fscod
)
{
  return (fscod <= EAC3_FSCOD_32000_HZ);
}

static inline unsigned sampleRateAc3Fscod(
  Ac3Fscod code
)
{
  static const unsigned fscodCodes[] = {
    48000,
    44100,
    32000
  };

  if (code < ARRAY_SIZE(fscodCodes))
    return fscodCodes[code];
  return 0;
}

static inline const char * Ac3FscodStr(
  Ac3Fscod fscod,
  bool fromEac3
)
{
  static const char * strings[] = {
    "48 kHz",
    "44.1 kHz",
    "32 kHz",
    "Indicated by fscod2"
  };

  if (fscod < (ARRAY_SIZE(strings) - !fromEac3))
    return strings[fscod];
  return "Reserved value";
}

static inline unsigned norminalBitrate_frmsizecod(
  uint8_t frmsizecod
)
{
  static const unsigned frmsizecod_list[] = {
     32,  40,  48,  56,  64,
     80,  96, 112, 128, 160,
    192, 224, 256, 320, 384,
    448, 512, 576, 640
  };

  if ((frmsizecod >> 1) < ARRAY_SIZE(frmsizecod_list))
    return frmsizecod_list[frmsizecod >> 1];
  return 0;
}

/** \~english
 * \brief Return
 *
 * \param frmsizecod Frame size code from syncinfo AC-3 header.
 * \param fscod Sample rate code from syncinfo AC-3 header.
 * \return unsigned Sync frame (syncframe) size in 16-bit words unit.
 */
static inline unsigned framesize_frmsizecod(
  uint8_t frmsizecod,
  Ac3Fscod fscod
)
{
  static const unsigned frmsizecod_list[][19] = {
    { // fscod (factor): 48 kHz (2) / 32 kHz (3)
       32,  40,  48,  56, 64,
       80,  96, 112, 128, 160,
      192, 224, 256, 320, 384,
      448, 512, 576, 640
    },
    { // fscod: 44.1 kHz
       69,  87, 104, 121, 139,
      174, 208, 243, 278, 348,
      417, 487, 557, 696, 835,
      975, 1114, 1253, 1393
    }
  };

  if (frmsizecod < 38) {
    if (EAC3_FSCOD_48000_HZ == fscod)
      return 2 * frmsizecod_list[0][frmsizecod >> 1];
    if (EAC3_FSCOD_44100_HZ == fscod)
      return frmsizecod_list[1][frmsizecod >> 1] + (frmsizecod & 0x1);
    if (EAC3_FSCOD_32000_HZ == fscod)
      return 3 * frmsizecod_list[0][frmsizecod >> 1];
  }

  return 0;
}

typedef struct {
  uint16_t crc1;
  Ac3Fscod fscod;
  uint8_t frmsizecod;

  // unsigned sampleRate;
  // size_t bitrate;
  // size_t frameSize;
} Ac3SyncInfoParameters;

typedef struct {
  uint8_t dialnorm2;
  bool compr2e;
  uint8_t compr2;

  bool langcod2e;
  uint8_t langcod2;

  bool audprodi2e;
  struct {
    uint8_t mixlevel2;
    uint8_t roomtyp2;
  } audprodi2;
} Ac3DualMonoBitStreamInfoParameters;

typedef enum {
  AC3_ADDBSI_UNK,
  AC3_ADDBSI_EC3_EXT_TYPE_A
} Ac3Ec3ExtensionType;

typedef struct {
  unsigned complexityIndex;
} Ac3Ec3ExtensionTypeA;

typedef struct {
  uint8_t addbsil;

  Ac3Ec3ExtensionType type;
  union {
    Ac3Ec3ExtensionTypeA ec3ExtTypeA;
  };
} Ac3Addbsi;

typedef enum {
  AC3_ACMOD_CH1_CH2      = 0x0,  /**< 1+1 Duplicated mono.                   */
  AC3_ACMOD_C            = 0x1,  /**< 1. Mono.                               */
  AC3_ACMOD_L_R          = 0x2,  /**< 2. Stereo.                             */
  AC3_ACMOD_L_C_R        = 0x3,  /**< 3. Stereo.                             */
  AC3_ACMOD_L_R_S        = 0x4,  /**< 3. Surround.                           */
  AC3_ACMOD_L_C_R_S      = 0x5,  /**< 4. Surround.                           */
  AC3_ACMOD_L_R_SL_SR    = 0x6,  /**< 4. Quad.                               */
  AC3_ACMOD_L_C_R_SL_SR  = 0x7,  /**< 5. Surround.                           */
} Ac3Acmod;

static inline bool threeFrontChannelsPresentAc3Acmod(
  Ac3Acmod acmod
)
{
  return ((acmod & 0x1) && (acmod > AC3_ACMOD_L_R));
}

static inline bool isSurroundAc3Acmod(
  Ac3Acmod acmod
)
{
  return (AC3_ACMOD_L_R_S <= acmod);
}

static inline bool bothSurroundChannelsPresentAc3mod(
  Ac3Acmod acmod
)
{
  return (AC3_ACMOD_L_R_SL_SR <= acmod);
}

static inline const char * Ac3AcmodStr(
  Ac3Acmod acmod,
  bool lfeon
)
{
  static const char * strings[] = {
    "Duplicated Mono (1+1): Ch1, Ch2",
    "Mono 1.0 (1/0): C",
    "Stereo 2.0 (2/0): L, R",
    "Stereo 3.0 (3/0): L, C, R",
    "Surround 3.0 (2/1): L, R, S",
    "Surround 4.0 (3/1): L, C, R, S",
    "Quadphonic 4.0 (2/2): L, R, Ls, Rs",
    "Surround 5.0 (3/2): L, C, R, Ls, Rs",

    "Duplicated Mono (1+1): Ch1, Ch2, LFE",
    "Mono 1.1 (1/0): C, LFE",
    "Stereo 2.1 (2/0): L, R, LFE",
    "Stereo 3.1 (3/0): L, C, R, LFE",
    "Surround 3.1 (2/1): L, R, S, LFE",
    "Surround 4.1 (3/1): L, C, R, S, LFE",
    "Quadphonic 4.1 (2/2): L, R, Ls, Rs, LFE",
    "Surround 5.1 (3/2): L, C, R, Ls, Rs, LFE"
  };

  if (acmod < ARRAY_SIZE(strings))
    return (lfeon) ? strings[acmod+8] : strings[acmod];
  return "Unknown";
}

static inline unsigned nbChannelsAc3Acmod(
  Ac3Acmod acmod,
  bool lfeon
)
{
  static const unsigned nbChannels[] = {
    1, 1, 2, 3, 3, 4, 4, 5
  };

  if (acmod < ARRAY_SIZE(nbChannels))
    return nbChannels[acmod] + lfeon;
  return 0;
}

typedef enum {
  AC3_BSMOD_CM       = 0x0,
  AC3_BSMOD_ME       = 0x1,
  AC3_BSMOD_VI       = 0x2,
  AC3_BSMOD_HI       = 0x3,
  AC3_BSMOD_D        = 0x4,
  AC3_BSMOD_C        = 0x5,
  AC3_BSMOD_E        = 0x6,
  AC3_BSMOD_VO_KARA  = 0x7   /**< Voice Over (VO) or Karaoke.                */
} Ac3Bsmod;

static inline const char * Ac3BsmodStr(
  Ac3Bsmod bsmod,
  Ac3Acmod acmod
)
{
  static const char * strings[] = {
    "Main audio service - Complete Main (CM)",
    "Main audio service - Music and effects (ME)",
    "Associated service - Visually impaired (VI)",
    "Associated service - Hearing impaired (HI)",
    "Associated service - Dialogue (D)",
    "Associated service - Commentary (C)",
    "Associated service - Emergency (E)"
  };

  if (bsmod < ARRAY_SIZE(strings))
    return strings[bsmod];
  if (bsmod == AC3_BSMOD_VO_KARA) {
    if (acmod == AC3_ACMOD_C)
      return "Associated service - Voice over (VO)";
    return "Main audio service - Karaoke";
  }

  return "Unknown";
}

typedef enum {
  AC3_CMIXLEV_707  = 0x0,
  AC3_CMIXLEV_595  = 0x1,
  AC3_CMIXLEV_500  = 0x3,
  AC3_CMIXLEV_RES  = 0x4
} Ac3Cmixlev;

static inline const char * Ac3CmixlevStr(
  Ac3Cmixlev cmixlev
)
{
  static const char * strings[] = {
    "0.707 (-3.0 dB)",
    "0.595 (-4.5 dB)",
    "0.500 (-6.0 dB)"
  };

  if (cmixlev < ARRAY_SIZE(strings))
    return strings[cmixlev];
  return "Reserved value";
}

typedef enum {
  AC3_SURMIXLEV_707  = 0x0,
  AC3_SURMIXLEV_500  = 0x1,
  AC3_SURMIXLEV_0    = 0x2,
  AC3_SURMIXLEV_RES  = 0x4
} Ac3Surmixlev;

static inline const char * Ac3SurmixlevStr(
  Ac3Surmixlev surmixlev
)
{
  static const char * strings[] = {
    "0.707 (-3.0 dB)",
    "0.500 (-6.0 dB)",
    "0.0 (-0 dB)"
  };

  if (surmixlev < ARRAY_SIZE(strings))
    return strings[surmixlev];
  return "Reserved value";
}

typedef enum {
  AC3_DSURMOD_NOT_INDICATED  = 0x0,
  AC3_DSURMOD_NOT_DS_ENC     = 0x2,
  AC3_DSURMOD_DS_ENC         = 0x3,
  AC3_DSURMOD_RES            = 0x4
} Ac3Dsurmod;

static inline const char * Ac3DsurmodStr(
  Ac3Dsurmod dsurmod
)
{
  static const char * strings[] = {
    "Not indicated",
    "Not Dolby Surround encoded",
    "Dolby Surround encoded"
  };

  if (dsurmod < ARRAY_SIZE(strings))
    return strings[dsurmod];
  return "Reserved value";
}

typedef enum {
  AC3_ROOMTYP_NOT_INDICATED  = 0x0,
  AC3_ROOMTYP_LARGE_CURVE    = 0x1,
  AC3_ROOMTYP_SMALL_FLAT     = 0x2,
  AC3_ROOMTYP_RES            = 0x3
} Ac3Roomtyp;

static inline const char * Ac3RoomtypStr(
  Ac3Roomtyp roomtyp
)
{
  static const char * strings[] = {
    "Not indicated",
    "Large room, X curve monitor",
    "Small room, flat monitor"
  };

  if (roomtyp < ARRAY_SIZE(strings))
    return strings[roomtyp];
  return "Reserved value";
}

typedef enum {
  AC3_DMIXMOD_NOT_INDICATED  = 0x0,
  AC3_DMIXMOD_LT_RT          = 0x1,
  AC3_DMIXMOD_LO_RO          = 0x2,
  AC3_DMIXMOD_RES            = 0x3
} Ac3Dmixmod;

static inline const char * Ac3DmixmodStr(
  Ac3Dmixmod dmixmod
)
{
  static const char * strings[] = {
    "Not indicated",
    "Lt/Rt downmix preferred",
    "Lo/Ro downmix preferred"
  };

  if (dmixmod < ARRAY_SIZE(strings))
    return strings[dmixmod];
  return "Not indicated (Reserved value)";
}

typedef enum {
  AC3_LTRTCMIXLEV_1_414  = 0x0,
  AC3_LTRTCMIXLEV_1_189  = 0x1,
  AC3_LTRTCMIXLEV_1_000  = 0x2,
  AC3_LTRTCMIXLEV_0_841  = 0x3,
  AC3_LTRTCMIXLEV_0_707  = 0x4,
  AC3_LTRTCMIXLEV_0_595  = 0x5,
  AC3_LTRTCMIXLEV_0_500  = 0x6,
  AC3_LTRTCMIXLEV_0_000  = 0x7
} Ac3Ltrtcmixlev;

static inline const char * Ac3LtrtcmixlevStr(
  Ac3Ltrtcmixlev ltrtcmixlev
)
{
  static const char * strings[] = {
    "1.414 (+3.0 dB)",
    "1.189 (+1.5 dB)",
    "1.000 (0.0 dB)",
    "0.841 (-1.5 dB)",
    "0.707 (-3.0 dB)",
    "0.595 (-4.5 dB)",
    "0.500 (-6.0 dB)",
    "0.000 (-inf dB)"
  };

  if (ltrtcmixlev < ARRAY_SIZE(strings))
    return strings[ltrtcmixlev];
  return "Reserved value";
}

typedef enum {
  AC3_LOROCMIXLEV_1_414  = 0x0,
  AC3_LOROCMIXLEV_1_189  = 0x1,
  AC3_LOROCMIXLEV_1_000  = 0x2,
  AC3_LOROCMIXLEV_0_841  = 0x3,
  AC3_LOROCMIXLEV_0_707  = 0x4,
  AC3_LOROCMIXLEV_0_595  = 0x5,
  AC3_LOROCMIXLEV_0_500  = 0x6,
  AC3_LOROCMIXLEV_0_000  = 0x7
} Ac3Lorocmixlev;

static inline const char * Ac3lLorocmixlevStr(
  Ac3Lorocmixlev lorocmixlev
)
{
  static const char * strings[] = {
    "1.414 (+3.0 dB)",
    "1.189 (+1.5 dB)",
    "1.000 (0.0 dB)",
    "0.841 (-1.5 dB)",
    "0.707 (-3.0 dB)",
    "0.595 (-4.5 dB)",
    "0.500 (-6.0 dB)",
    "0.000 (-inf dB)"
  };

  if (lorocmixlev < ARRAY_SIZE(strings))
    return strings[lorocmixlev];
  return "Reserved value";
}

typedef enum {
  AC3_LTRTSURMIXLEV_RES_0  = 0x0,
  AC3_LTRTSURMIXLEV_RES_1  = 0x1,
  AC3_LTRTSURMIXLEV_RES_2  = 0x2,
  AC3_LTRTSURMIXLEV_0_841  = 0x3,
  AC3_LTRTSURMIXLEV_0_707  = 0x4,
  AC3_LTRTSURMIXLEV_0_595  = 0x5,
  AC3_LTRTSURMIXLEV_0_500  = 0x6,
  AC3_LTRTSURMIXLEV_0_000  = 0x7
} Ac3Ltrtsurmixlev;

static inline const char * Ac3LtrtsurmixlevStr(
  Ac3Ltrtsurmixlev ltrtsurmixlev
)
{
  static const char * strings[] = {
    "Reserved value",
    "Reserved value",
    "Reserved value",
    "0.841 (-1.5 dB)",
    "0.707 (-3.0 dB)",
    "0.595 (-4.5 dB)",
    "0.500 (-6.0 dB)",
    "0.000 (-inf dB)"
  };

  if (ltrtsurmixlev < ARRAY_SIZE(strings))
    return strings[ltrtsurmixlev];
  return "Reserved value";
}

typedef enum {
  AC3_LOROSURMIXLEV_RES_0  = 0x0,
  AC3_LOROSURMIXLEV_RES_1  = 0x1,
  AC3_LOROSURMIXLEV_RES_2  = 0x2,
  AC3_LOROSURMIXLEV_0_841  = 0x3,
  AC3_LOROSURMIXLEV_0_707  = 0x4,
  AC3_LOROSURMIXLEV_0_595  = 0x5,
  AC3_LOROSURMIXLEV_0_500  = 0x6,
  AC3_LOROSURMIXLEV_0_000  = 0x7
} Ac3Lorosurmixlev;

static inline const char * Ac3LorosurmixlevStr(
  Ac3Lorosurmixlev lorosurmixlev
)
{
  static const char * strings[] = {
    "Reserved value",
    "Reserved value",
    "Reserved value",
    "0.841 (-1.5 dB)",
    "0.707 (-3.0 dB)",
    "0.595 (-4.5 dB)",
    "0.500 (-6.0 dB)",
    "0.000 (-inf dB)"
  };

  if (lorosurmixlev < ARRAY_SIZE(strings))
    return strings[lorosurmixlev];
  return "Reserved value";
}

typedef enum {
  AC3_DHEADPHONMOD_NOT_INDICATED   = 0x0,
  AC3_DHEADPHONMOD_NOT_DH_ENCODED  = 0x1,  /**< Not Dolby Headphone encoded. */
  AC3_DHEADPHONMOD_DH_ENCODED      = 0x2,
  AC3_DHEADPHONMOD_RES             = 0x3
} Ac3Dheadphonmod;

static inline const char * Ac3DheadphonmodStr(
  Ac3Dheadphonmod dheadphonmod
)
{
  static const char * strings[] = {
    "Not indicated",
    "Not Dolby Headphone encoded",
    "Dolby Headphone encoded",
    "Reserved value"
  };

  if (dheadphonmod < ARRAY_SIZE(strings))
    return strings[dheadphonmod];
  return "Reserved value";
}

typedef enum {
  AC3_DSUREXMOD_NOT_INDICATED           = 0x0,
  AC3_DSUREXMOD_NOT_ENCODED             = 0x1,
  AC3_DSUREXMOD_DSEX_OR_DPLIIX_ENCODED  = 0x2,
  AC3_DSUREXMOD_DPLIIZ_ENCODED          = 0x2
} Ac3Dsurexmod;

static inline const char * Ac3DsurexmodStr(
  Ac3Dsurexmod dsurexmod
)
{
  static const char * strings[] = {
    "Not indicated",
    "Not Dolby Surround EX nor Dolby Pro Logic IIx/IIz encoded",
    "Dolby Surround EX or Dolby Pro Logic IIx encoded",
    "Dolby Pro Logic IIz encoded"
  };

  if (dsurexmod < ARRAY_SIZE(strings))
    return strings[dsurexmod];
  return "Reserved value";
}

typedef struct {
  Ac3Dmixmod dmixmod;
  Ac3Ltrtcmixlev ltrtcmixlev;
  Ac3Ltrtsurmixlev ltrtsurmixlev;
  Ac3Lorocmixlev lorocmixlev;
  Ac3Lorosurmixlev lorosurmixlev;
} Ac3ExtraBitStreamInfo1;

typedef struct {
  Ac3Dsurexmod dsurexmod;
  Ac3Dheadphonmod dheadphonmod;
  bool adconvtyp;
  uint8_t xbsi2;
  bool encinfo;
} Ac3ExtraBitStreamInfo2;

typedef struct {
  uint8_t bsid;
  Ac3Bsmod bsmod;
  Ac3Acmod acmod;

  Ac3Cmixlev cmixlev;
  Ac3Surmixlev surmixlev;
  Ac3Dsurmod dsurmod;

  bool lfeon;

  uint8_t dialnorm;
  bool compre;
  uint8_t compr;

  bool langcode;
  uint8_t langcod;

  bool audprodie;
  struct {
    uint8_t mixlevel;
    Ac3Roomtyp roomtyp;
  } audprodi;

  Ac3DualMonoBitStreamInfoParameters dual_mono;

  bool copyrightb;
  bool origbs;

  /* Legacy Syntax : */
  union {
    struct {
      bool timecod1e;
      uint16_t timecod1;
      bool timecod2e;
      uint16_t timecod2;
    };  /**< Legacy syntax, if bsid <= 8. */

    struct {
      bool xbsi1e;
      Ac3ExtraBitStreamInfo1 xbsi1;
      bool xbsi2e;
      Ac3ExtraBitStreamInfo2 xbsi2;
    };  /**< Alternate syntax, if bsid == 6. */
  };

  bool addbsie;
  Ac3Addbsi addbsi;

  unsigned nbChannels;
} Ac3BitStreamInfoParameters;

#define AC3_SAMPLES_PER_FRAME  1536

/* ### E-AC-3 : ############################################################# */

typedef struct {
  bool atmos;
} Eac3Informations;

typedef enum {
  EAC3_STRMTYP_TYPE_0  = 0x0,  /**< Independant substream.                   */
  EAC3_STRMTYP_TYPE_1  = 0x1,  /**< Dependant substream.                     */
  EAC3_STRMTYP_TYPE_2  = 0x2,  /**< Independant substream, bit stream
    converted from AC-3.                                                     */
  EAC3_STRMTYP_RES     = 0x3
} Eac3Strmtyp;

static inline const char * Eac3StrmtypStr(
  Eac3Strmtyp strmtyp
)
{
  static const char * strings[] = {
    "Type 1, independent substream",
    "Type 2, dependent substream",
    "Type 3, independent substream (AC-3 encoded)"
  };

  if (strmtyp < ARRAY_SIZE(strings))
    return strings[strmtyp];
  return "Reserved value";
}

typedef enum {
  EAC3_FSCOD2_24000_HZ  = 0x0,
  EAC3_FSCOD2_22050_HZ  = 0x1,
  EAC3_FSCOD2_16000_HZ  = 0x2,
  EAC3_FSCOD2_RES
} Eac3Fscod2;

static inline unsigned sampleRateEac3Fscod2(
  Eac3Fscod2 fscod2
)
{
  static const unsigned fscod2Codes[] = {
    24000,
    22050,
    16000
  };

  if (fscod2 < ARRAY_SIZE(fscod2Codes))
    return fscod2Codes[fscod2];
  return 0;
}

static inline const char * Eac3Fscod2Str(
  Eac3Fscod2 fscod2
)
{
  static const char * strings[] = {
    "24 kHz",
    "22.05 kHz",
    "16 kHz"
  };

  if (fscod2 < ARRAY_SIZE(strings))
    return strings[fscod2];
  return "Reserved value";
}

typedef enum {
  EAC3_NUMBLKSCOD_1_BLOCK   = 0x0,
  EAC3_NUMBLKSCOD_2_BLOCKS  = 0x1,
  EAC3_NUMBLKSCOD_3_BLOCKS  = 0x2,
  EAC3_NUMBLKSCOD_6_BLOCKS  = 0x3
} Eac3Numblkscod;

static inline unsigned nbBlocksEac3Numblkscod(
  Eac3Numblkscod numblkscod
)
{
  static const unsigned values[] = {
    1, 2, 3, 6
  };

  if (numblkscod < ARRAY_SIZE(values))
    return values[numblkscod];
  return 0;
}

typedef enum {
  EAC3_CHANMAP_L        = 1 << 15,
  EAC3_CHANMAP_C        = 1 << 14,
  EAC3_CHANMAP_R        = 1 << 13,
  EAC3_CHANMAP_LS       = 1 << 12,
  EAC3_CHANMAP_RS       = 1 << 11,
  EAC3_CHANMAP_LC_RC    = 1 << 10,
  EAC3_CHANMAP_LRS_RRS  = 1 <<  9,
  EAC3_CHANMAP_CS       = 1 <<  8,
  EAC3_CHANMAP_TS       = 1 <<  7,
  EAC3_CHANMAP_LSD_RSD  = 1 <<  6,
  EAC3_CHANMAP_LW_RW    = 1 <<  5,
  EAC3_CHANMAP_VHL_VHR  = 1 <<  4,
  EAC3_CHANMAP_VHC      = 1 <<  3,
  EAC3_CHANMAP_LTS_RTS  = 1 <<  2,
  EAC3_CHANMAP_LFE2     = 1 <<  1,
  EAC3_CHANMAP_LFE      = 1 <<  0
} Eac3ChanmapMaskCode;

static inline unsigned nbChannelsEac3Chanmap(
  uint16_t mask
)
{
  static const unsigned maskEntries[] = {
    1, 1, 2, 1, 2, 2, 2, 1, 1, 2, 2, 1, 1, 1, 1, 1
  };

  unsigned nb_channels = 0;
  for (unsigned i = 0; i < 16; i++)
    nb_channels += ((mask >> i) & 0x1) * maskEntries[i];

  return nb_channels;
}

#define EAC3_CHANMAP_STR_BUFSIZE  80

typedef struct {
  uint8_t dialnorm2;
  bool compr2e;
  uint8_t compr2;
} Eac3DualMonoBitStreamInfoParameters;

typedef struct {
  Ac3Bsmod bsmod;
  bool copyrightb;
  bool origbs;

  Ac3Dsurmod dsurmod;
  Ac3Dheadphonmod dheadphonmod;
  Ac3Dsurexmod dsurexmod;

  bool audprodie;
  struct {
    uint8_t mixlevel;
    Ac3Roomtyp roomtyp;
    bool adconvtyp;
  } audprodi;

  bool audprodi2e;
  struct {
    uint8_t mixlevel2;
    Ac3Roomtyp roomtyp2;
    bool adconvtyp2;
  } audprodi2;

  bool sourcefscod;
} Eac3Infomdat;

typedef struct {
  Eac3Strmtyp strmtyp;
  uint8_t substreamid;
  uint16_t frmsiz;
  Ac3Fscod fscod;
  Eac3Fscod2 fscod2;
  Eac3Numblkscod numblkscod;

  Ac3Acmod acmod;
  bool lfeon;

  uint8_t bsid;

  uint8_t dialnorm;
  bool compre;
  uint8_t compr;
  Eac3DualMonoBitStreamInfoParameters dual_mono;

  bool chanmape;
  uint16_t chanmap;

  bool mixmdate;
  struct {
    Ac3Dmixmod dmixmod;

    Ac3Ltrtcmixlev ltrtcmixlev;
    Ac3Lorocmixlev lorocmixlev;

    Ac3Ltrtsurmixlev ltrtsurmixlev;
    Ac3Lorosurmixlev lorosurmixlev;

    bool lfemixlevcode;
    uint8_t lfemixlevcod;

    bool pgmscle;
    uint8_t pgmscl;
    bool pgmscl2e;
    uint8_t pgmscl2;

    bool extpgmscle;
    uint8_t extpgmscl;

    uint8_t mixdef;
    bool premixcmpsel;
    bool drcsrc;
    uint8_t premixcmpscl;
  } Mixdata;

  bool infomdate;
  Eac3Infomdat infomdat;

  bool convsync;
  bool blkid;
  uint8_t frmsizecod;

  bool addbsie;
  Ac3Addbsi addbsi;

  unsigned numblks;

  // unsigned sampleRate;
  // unsigned numBlksPerSync;
  // unsigned frameSize;
  // unsigned nbChannels;
  // unsigned nbChannelsFromMap;
} Eac3BitStreamInfoParameters;

/* ### TrueHD / MLP : ###################################################### */

typedef enum {
  MLP_INFO_MS_NOT_INDICATED,  /**< No indication over any matrix-surround encoding. */
  MLP_INFO_MS_NOT_ENCODED,    /**< Not matrix-surround encoded. */
  MLP_INFO_MS_ENCODED,        /**< Unspecified matrix-surround encoding encoded. */
  MLP_INFO_MS_DSEX_DPLIIX,    /**< Dolby Surround EX / Dolby Pro Logic IIx encoded. */
  MLP_INFO_MS_DPLIIZ          /**< Dolby Pro Logic IIz encoded. */
} MlpInformationsMatrixSurroundEncoding;

/** \~english
 * \brief MLP/TrueHD bitstream informations.
 *
 * Bitstream informations synthesis.
 */
typedef struct {
  unsigned sampling_frequency;
  unsigned peak_data_rate;
  unsigned nb_channels;

  bool binaural_audio;
  bool mono_audio;
  bool atmos;
  MlpInformationsMatrixSurroundEncoding matrix_surround;

  unsigned observed_bit_depth;
} MlpInformations;

/** \~english
 * \brief MLP generic bit-stream syncword prefix.
 *
 */
#define MLP_SYNCWORD_PREFIX  ((uint32_t) 0xF8726F)

/** \~english
 * \brief MLP TrueHD extension bit-stream syncword suffix.
 *
 * This suffix combined with #MLP_SYNCWORD_PREFIX prefix indicates a TrueHD
 * bitstream (#TRUE_HD_SYNCWORD).
 */
#define TRUE_HD_SYNCWORD_SUFFIX  ((uint8_t) 0xBA)

/** \~english
 * \brief MLP bit-stream syncword suffix.
 *
 * This suffix combined with #MLP_SYNCWORD_SUFFIX prefix indicates a MLP
 * bitstream without TrueHD extensions (#MLP_SYNCWORD).
 */
#define MLP_SYNCWORD_SUFFIX  ((uint8_t) 0xBB)

/** \~english
 * \brief MLP TrueHD extension bit-stream syncword.
 *
 */
#define TRUE_HD_SYNCWORD                                                      \
  ((uint32_t) (MLP_SYNCWORD_PREFIX << 8) | TRUE_HD_SYNCWORD_SUFFIX)

/** \~english
 * \brief MLP bit-stream syncword.
 *
 */
#define MLP_SYNCWORD                                                          \
  ((uint32_t) (MLP_SYNCWORD_PREFIX << 8) | MLP_SYNCWORD_SUFFIX)

static inline char * MlpFormatSyncStr(
  uint32_t format_sync
)
{
  if (TRUE_HD_SYNCWORD == format_sync)
    return "Dolby TrueHD (FBA)";
  if (MLP_SYNCWORD == format_sync)
    return "Meridian Lossless Packing (FBB)";
  return "Unknown/reserved value";
}

#define TRUE_HD_SIGNATURE  0xB752

typedef enum {
  MLP_AUDIO_SAMPLING_FREQ_48_000   = 0x0,
  MLP_AUDIO_SAMPLING_FREQ_96_000   = 0x1,
  MLP_AUDIO_SAMPLING_FREQ_192_000  = 0x2,
  MLP_AUDIO_SAMPLING_FREQ_44_100   = 0x8,
  MLP_AUDIO_SAMPLING_FREQ_88_200   = 0x9,
  MLP_AUDIO_SAMPLING_FREQ_176_400  = 0xA
} MlpAudioSamplingFrequency;

static inline unsigned sampleRateMlpAudioSamplingFrequency(
  MlpAudioSamplingFrequency audio_sampling_frequency
)
{
  static const unsigned values[] = {
    48000, 96000, 192000
  };
  static const unsigned values2[] = {
    44100, 88200, 176400
  };

  if (audio_sampling_frequency < ARRAY_SIZE(values))
    return values[audio_sampling_frequency];
  if ((audio_sampling_frequency & 0x7) < ARRAY_SIZE(values2))
    return values2[audio_sampling_frequency & 0x7];
  return 0;
}

static inline const char * MlpAudioSamplingFrequencyStr(
  MlpAudioSamplingFrequency audio_sampling_frequency
)
{
  static const char * strings[] = {
    "48 kHz",
    "96 kHz",
    "192 kHz"
  };
  static const char * strings2[] = {
    "44.1 kHz",
    "88.2 kHz",
    "176.4 kHz"
  };

  if (audio_sampling_frequency < ARRAY_SIZE(strings))
    return strings[audio_sampling_frequency];
  if ((audio_sampling_frequency & 0x7) < ARRAY_SIZE(strings2))
    return strings2[audio_sampling_frequency & 0x7];
  return "Reserved value";
}


typedef enum {
  MLP_6CH_MULTICHANNEL_TYPE_STD_SPKR_LAYOUT  = 0x0,
  MLP_6CH_MULTICHANNEL_TYPE_RES              = 0x1
} Mlp6ChMultichannelType;

static inline const char * Mlp6ChMultichannelTypeStr(
  Mlp6ChMultichannelType u6ch_multichannel_type
)
{
  if (u6ch_multichannel_type)
    return "Reserved value";
  return "Standard loudspeaker layout";
}

typedef enum {
  MLP_8CH_MULTICHANNEL_TYPE_STD_SPKR_LAYOUT  = 0x0,
  MLP_8CH_MULTICHANNEL_TYPE_RES              = 0x1
} Mlp8ChMultichannelType;

static inline const char * Mlp8ChMultichannelTypeStr(
  Mlp8ChMultichannelType u8ch_multichannel_type
)
{
  if (u8ch_multichannel_type)
    return "Reserved value";
  return "Standard loudspeaker layout";
}

typedef enum {
  MLP_2CH_PRES_CH_MOD_STEREO     = 0x0,
  MLP_2CH_PRES_CH_MOD_LT_RT      = 0x1,
  MLP_2CH_PRES_CH_MOD_LBIN_RBIN  = 0x2,
  MLP_2CH_PRES_CH_MOD_MONO       = 0x3
} Mlp2ChPresentationChannelModifier;

static inline const char * Mlp2ChPresentationChannelModifierStr(
  Mlp2ChPresentationChannelModifier u2ch_presentation_channel_modifier
)
{
  static const char * strings[] = {
    "Stereo (L/R speaker feeds)",
    "Lt/Rt (Matrix-surround encoded, L/R speaker feeds compatible)",
    "Lbin/Rbin (Binaural headphone playback encoded)",
    "Mono (Single channel signal/Identical L/R speaker feeds)"
  };

  if (u2ch_presentation_channel_modifier < ARRAY_SIZE(strings))
    return strings[u2ch_presentation_channel_modifier];
  return "Reserved value";
}

typedef enum {
  MLP_6CH_PRES_CH_MOD_STEREO__NI              = 0x0,
  MLP_6CH_PRES_CH_MOD_LT_RT__NE               = 0x1,
  MLP_6CH_PRES_CH_MOD_LBIN_RBIN__DSEX_DPLIIX  = 0x2,
  MLP_6CH_PRES_CH_MOD_MONO__DPLIIZ            = 0x3
} Mlp6ChPresentationChannelModifier;

static inline const char * Mlp6ChPresentationChannelModifierStr(
  Mlp6ChPresentationChannelModifier u6ch_presentation_channel_modifier,
  unsigned nbChannels,
  bool lsRsPres
)
{
  static const char * strings1[] = {
    "Stereo (L/R speaker feeds)",
    "Lt/Rt (Matrix-surround encoded, L/R speaker feeds compatible)",
    "Lbin/Rbin (Binaural headphone playback encoded)",
    "Mono (Single channel signal/Identical L/R speaker feeds)"
  };
  static const char * strings2[] = {
    "Ls/Rs content type: Not indicated",
    "Ls/Rs content type: Not Dolby Surround EX, Dolby Pro Logic IIx or IIz encoded",
    "Ls/Rs content type: Dolby Surround EX/Dolby Pro Logic IIx encoded",
    "Ls/Rs content type: Dolby Pro Logic IIz encoded"
  };

  if (0x3 < u6ch_presentation_channel_modifier)
    return "Reserved value";
  if (2 == nbChannels)
    return strings1[u6ch_presentation_channel_modifier];
  if (3 <= nbChannels && lsRsPres)
    return strings2[u6ch_presentation_channel_modifier];
  return "No meaning";
}

unsigned getNbChannels6ChPresentationAssignment(
  uint8_t u6ch_presentation_channel_assignment
);

static inline bool lsRsPresent6ChPresentationAssignment(
  uint8_t u6ch_presentation_channel_assignment
)
{
  return u6ch_presentation_channel_assignment & 0x2; // Ls/Rs
}

typedef enum {
  MLP_8CH_PRES_CH_MOD_STEREO__NI              = 0x0,
  MLP_8CH_PRES_CH_MOD_LT_RT__NE               = 0x1,
  MLP_8CH_PRES_CH_MOD_LBIN_RBIN__DSEX_DPLIIX  = 0x2,
  MLP_8CH_PRES_CH_MOD_MONO__DPLIIZ            = 0x3
} Mlp8ChPresentationChannelModifier;

static inline const char * Mlp8ChPresentationChannelModifierStr(
  Mlp8ChPresentationChannelModifier u8ch_presentation_channel_modifier,
  unsigned nb_channels,
  bool main_L_R_ch_present,
  bool sur_Ls_Rs_present
)
{
  static const char * strings1[] = {
    "Stereo (L/R speaker feeds)",
    "Lt/Rt (Matrix-surround encoded, L/R speaker feeds compatible)",
    "Lbin/Rbin (Binaural headphone playback encoded)",
    "Mono (Single channel signal/Identical L/R speaker feeds)"
  };
  static const char * strings2[] = {
    "Ls/Rs content type: Not indicated",
    "Ls/Rs content type: Not Dolby Surround EX, Dolby Pro Logic IIx or IIz encoded",
    "Ls/Rs content type: Dolby Surround EX/Dolby Pro Logic IIx encoded",
    "Ls/Rs content type: Dolby Pro Logic IIz encoded"
  };

  if (0x3 < u8ch_presentation_channel_modifier)
    return "Reserved value";
  if (2 == nb_channels && main_L_R_ch_present)
    return strings1[u8ch_presentation_channel_modifier];
  if (3 <= nb_channels && sur_Ls_Rs_present)
    return strings2[u8ch_presentation_channel_modifier];
  return "No meaning";
}

unsigned getNbChannels8ChPresentationAssignment(
  uint8_t u8ch_presentation_channel_assignment,
  bool alternate_meaning
);

static inline bool mainLRPresent8ChPresentationAssignment(
  uint8_t u8ch_presentation_channel_assignment
)
{
  return u8ch_presentation_channel_assignment & 0x1; // L/R
}

static inline bool onlyLsRsPresent8ChPresentationAssignment(
  uint8_t u8ch_presentation_channel_assignment
)
{
  return (u8ch_presentation_channel_assignment >> 3) == 0x1; // Only Ls/Rs
}

typedef struct {
  uint32_t value;

  MlpAudioSamplingFrequency audio_sampling_frequency;
  Mlp6ChMultichannelType u6ch_multichannel_type;
  Mlp8ChMultichannelType u8ch_multichannel_type;

  Mlp2ChPresentationChannelModifier u2ch_presentation_channel_modifier;

  Mlp6ChPresentationChannelModifier u6ch_presentation_channel_modifier;
  uint8_t u6ch_presentation_channel_assignment;

  Mlp8ChPresentationChannelModifier u8ch_presentation_channel_modifier;
  uint16_t u8ch_presentation_channel_assignment;
} MlpMajorSyncFormatInfo;

#define MLP_MAX_PEAK_DATA_RATE  18000000u

typedef struct {
  uint8_t u16ch_dialogue_norm;
  uint8_t u16ch_mix_level;
  uint8_t u16ch_channel_count;

  bool dyn_object_only;
  union {
    struct {
      bool lfe_present;
    };  /**< If (dyn_object_only) */

    struct {
      uint8_t v16ch_content_description;

      bool chan_distribute;
      bool lfe_only;
      uint16_t v16ch_channel_assignment;

      uint8_t v16ch_intermediate_spatial_format;

      uint8_t u16ch_dynamic_object_count;
    };  /**< If (!dyn_object_only), currently non implemented */
  };
} Mlp16ChChannelMeaning;

static inline bool constantMlp16ChChannelMeaningCheck(
  const Mlp16ChChannelMeaning * first,
  const Mlp16ChChannelMeaning * second
)
{
  return CHECK(
    EQUAL(->dyn_object_only)
    START_COND(->dyn_object_only, true)
      EQUAL(->lfe_present)
    END_COND
  );
}

typedef enum {
  MLP_EXTRA_CH_MEANING_CONTENT_UNKNOWN,
  MLP_EXTRA_CH_MEANING_CONTENT_16CH_MEANING
} MlpExtraChannelMeaningContent;

static inline const char * MlpExtraChannelMeaningContentStr(
  MlpExtraChannelMeaningContent type
)
{
  static const char * strings[] = {
    "Unknown",
    "16-ch meaning"
  };

  return strings[type];
}

/** \~english
 * \brief AC-3 MLP extension extra_channel_meaning of channel_meaning().
 *
 * This include field extra_channel_meaning_length and block
 * extra_channel_meaning_data.
 *
 * [2] 3.3.3-3.3.5, 4.4
 */
typedef struct {
  MlpExtraChannelMeaningContent type;
  union {
    Mlp16ChChannelMeaning v16ch_channel_meaning;
  } content;  /**< extra_channel_meaning_data() content, declared as an
    union to allow future additions.                                         */

  uint8_t reserved[32];
  unsigned reserved_size;
} MlpExtraChannelMeaningData;

static inline bool atmosPresentMlpExtraChannelMeaning(
  const MlpExtraChannelMeaningData * param
)
{
  return (MLP_EXTRA_CH_MEANING_CONTENT_16CH_MEANING != param->type);
}

static inline bool constantMlpExtraChannelMeaningCheck(
  const MlpExtraChannelMeaningData * first,
  const MlpExtraChannelMeaningData * second
)
{
  return CHECK(
    EQUAL(->type)
    START_COND(->type, MLP_EXTRA_CH_MEANING_CONTENT_16CH_MEANING)
      SUB_FUN_PTR(->content.v16ch_channel_meaning, constantMlp16ChChannelMeaningCheck)
    END_COND
  );
}

typedef enum {
  MLP_EXT_SS_INFO_16CH_PRES_SS_3        = 0x0,
  MLP_EXT_SS_INFO_16CH_PRES_SS_2_3      = 0x1,
  MLP_EXT_SS_INFO_16CH_PRES_SS_1_2_3    = 0x2,
  MLP_EXT_SS_INFO_16CH_PRES_SS_0_1_2_3  = 0x3
} MlpExtendedSubstreamInfo;

static inline const char * MlpExtendedSubstreamInfoStr(
  MlpExtendedSubstreamInfo extended_substream_info,
  bool b16ch_presentation_present
)
{
  static const char * strings[] = {
    "Substream 3",
    "Substreams 2 and 3",
    "Substreams 1, 2 and 3",
    "Substreams 0, 1, 2 and 3",
  };

  if (!b16ch_presentation_present && extended_substream_info == 0x0)
    return "Unused field";
  if (b16ch_presentation_present && extended_substream_info < ARRAY_SIZE(strings))
    return strings[extended_substream_info];
  return "Unknown value";
}

typedef enum {
  MLP_SS_INFO_6CH_PRES_RESERVED  = 0x0,
  MLP_SS_INFO_6CH_PRES_SS_0      = 0x1,
  MLP_SS_INFO_6CH_PRES_SS_1      = 0x2,
  MLP_SS_INFO_6CH_PRES_SS_0_1    = 0x3
} MlpSubstreamInfo6ChPresentation;

static inline const char * MlpSubstreamInfo6ChPresentationStr(
  MlpSubstreamInfo6ChPresentation u6ch_presentation
)
{
  static const char * strings[] = {
    "Illegal",
    "Substream 0 (Copy of 2-ch presentation)",
    "Substream 1",
    "Substreams 1 and 2"
  };

  if (u6ch_presentation < ARRAY_SIZE(strings))
    return strings[u6ch_presentation];
  return "Reserved value";
}

typedef enum {
  MLP_SS_INFO_8CH_PRES_RESERVED  = 0x0,
  MLP_SS_INFO_8CH_PRES_SS_0      = 0x1,
  MLP_SS_INFO_8CH_PRES_SS_1      = 0x2,
  MLP_SS_INFO_8CH_PRES_SS_0_1    = 0x3,
  MLP_SS_INFO_8CH_PRES_SS_2      = 0x4,
  MLP_SS_INFO_8CH_PRES_SS_0_2    = 0x5,
  MLP_SS_INFO_8CH_PRES_SS_1_2    = 0x6,
  MLP_SS_INFO_8CH_PRES_SS_0_1_2  = 0x7
} MlpSubstreamInfo8ChPresentation;

static inline const char * MlpSubstreamInfo8ChPresentationStr(
  MlpSubstreamInfo8ChPresentation u8ch_presentation
)
{
  static const char * strings[] = {
    "Illegal",
    "Substream 0 (Copy of 2-ch presentation)",
    "Substream 1 (Copy of 6-ch presentation)",
    "Substreams 0 and 1 (Copy of 6-ch presentation)",
    "Substream 2",
    "Substreams 0 and 2",
    "Substreams 1 and 2",
    "Substreams 0, 1 and 2",
  };

  if (u8ch_presentation < ARRAY_SIZE(strings))
    return strings[u8ch_presentation];
  return "Reserved value";
}

typedef struct {
  uint8_t value;

  uint8_t reserved_field;
  MlpSubstreamInfo6ChPresentation u6ch_presentation;
  MlpSubstreamInfo8ChPresentation u8ch_presentation;
  bool b16ch_presentation_present;
} MlpSubstreamInfo;

typedef struct {
  uint8_t reserved_field;

  bool b2ch_control_enabled;
  bool b6ch_control_enabled;
  bool b8ch_control_enabled;

  bool reserved_bool_1;

  uint8_t drc_start_up_gain;

  uint8_t u2ch_dialogue_norm;
  uint8_t u2ch_mix_level;

  uint8_t u6ch_dialogue_norm;
  uint8_t u6ch_mix_level;
  uint8_t u6ch_source_format;

  uint8_t u8ch_dialogue_norm;
  uint8_t u8ch_mix_level;
  uint8_t u8ch_source_format;

  bool reserved_bool_2;

  bool extra_channel_meaning_present;
  uint8_t extra_channel_meaning_length;
  MlpExtraChannelMeaningData extra_channel_meaning_data;
} MlpChannelMeaning;

typedef struct {
  uint16_t value;  /**< Value of the 'flags' field.                          */

  bool constant_fifo_buf_delay;
  bool fmt_info_alter_8ch_asgmt_syntax; /**< Field 'format_info' alternative
    8ch assignment syntax.                                                   */
} MlpMajorSyncFlags;

/** \~english
 * \brief MLP/TrueHD Major Sync CRC-16 parameters.
 *
 * 16-bit CRC word, poly: 0x1002D, initial value: 0x0000, shifted algorithm.
 * Described in [3] 4.2.10 major_sync_info_CRC.
 */
#define MLP_MS_CRC_PARAMS                                                     \
  (CrcParam) {.table = mlp_ms_crc_16_table, .length = 16, .shifted = true}

typedef struct {
  uint32_t format_sync;
  MlpMajorSyncFormatInfo format_info;
  uint16_t signature;
  MlpMajorSyncFlags flags;
  uint16_t reserved_field_1;

  bool variable_bitrate;
  uint16_t peak_data_rate;
  uint8_t substreams;  /**< Number of substreams */
  uint8_t reserved_field_2;

  MlpExtendedSubstreamInfo extended_substream_info;
  MlpSubstreamInfo substream_info;

  MlpChannelMeaning channel_meaning;

  uint16_t major_sync_info_CRC;
} MlpMajorSyncInfoParameters;

static inline bool atmosPresentMlpMajorSyncParameters(
  const MlpMajorSyncInfoParameters * param
)
{
  if (!param->channel_meaning.extra_channel_meaning_present)
    return false;
  return atmosPresentMlpExtraChannelMeaning(
    &param->channel_meaning.extra_channel_meaning_data
  );
}

typedef struct {
  uint8_t check_nibble;
  uint16_t access_unit_length;  /**< Total length of the Access Unit in
    2-byte word units.                                                       */
  uint16_t input_timing;

  bool major_sync;
  MlpMajorSyncInfoParameters major_sync_info;
} MlpSyncHeaderParameters;

static inline void updateMlpSyncHeaderParametersParameters(
  MlpSyncHeaderParameters * dst,
  MlpSyncHeaderParameters src
)
{
  if (src.major_sync) {
    *dst = src;
  }
  else {
    dst->check_nibble = src.check_nibble;
    dst->access_unit_length = src.access_unit_length;
    dst->input_timing = src.input_timing;
    dst->major_sync = src.major_sync;
  }
}

typedef struct {
  bool extra_substream_word;
  bool restart_nonexistent;
  bool crc_present;
  bool reserved_field_1;

  uint16_t substream_end_ptr;

  uint16_t drc_gain_update;
  uint8_t drc_time_update;
  uint8_t reserved_field_2;

  unsigned substream_size;  /**< Substream size in 16-bits words unit. */
} MlpSubstreamDirectoryEntry;

/** \~english
 * \brief MLP/TrueHD Restart Header CRC-8 parameters.
 *
 * 8-bit CRC word, poly: 0x11D, initial value: 0x00, shifted algorithm.
 * Described in [3] 4.7.2 restart_header_CRC.
 */
#define MLP_RH_CRC_PARAMS                                                     \
  (CrcParam) {.table = mlp_rh_crc_8_table, .length = 8, .shifted = true}


#define MLP_MAX_NB_CHANNELS  16


static inline unsigned getMlpMaxNbChannels(
  unsigned ss_idx
)
{
  switch (ss_idx) {
    case 0:  // Substream 0
      return 2;  // 2-channel presentation
    case 1:  // Substream 1
      return 6;  // 6-channel presentation
    case 2:  // Substream 2
      return 8;  // 8-channel presentation
    default: // Substream 3
      return 16; // 16-channel presentation
  }
}


#define MLP_MAX_NB_MATRIX_CHANNELS  8


typedef struct {
  uint16_t restart_sync_word;
  bool noise_type;
  uint16_t output_timing;
  uint8_t min_chan;
  uint8_t max_chan;
  uint8_t max_matrix_chan;
  // uint8_t dither_shift;
  int8_t max_shift;            /**< Max output applied left-shift.           */
  uint8_t max_lsbs;            /**< Maximum coded LSB size.                  */
  uint8_t max_bits;
  bool error_protect;
} MlpRestartHeader;

/** \~english
 * \brief MLP block header content flags.
 *
 * Block header 'block_header_content' parameter flags. Each flag is used to
 * check presence or not of a specific section in the block header. If the
 * binary AND with a flag is non-zero, the section is present. Otherwise it is
 * absent until potential further update of the 'block_header_content'
 * parameter.
 *
 * Default value of the parameter is #MLP_BHC_DEFAULT and is used each time
 * a restart header is present in block before block header.
 */
typedef enum {
  MLP_BHC_BLOCK_HEADER_CONTENT   = (1u << 0),  /**< Block header content
    section.                                                                 */
  MLP_BHC_HUFFMAN_OFFSET         = (1u << 1),  /**< Huffman coded residuals
    offset section.                                                          */
  MLP_BHC_IIR_FILTER_PARAMETERS  = (1u << 2),  /**< IIR filter parameters
    section.                                                                 */
  MLP_BHC_FIR_FILTER_PARAMETERS  = (1u << 3),  /**< FIR filter parameters
    section.                                                                 */
  MLP_BHC_QUANT_STEP_SIZE        = (1u << 4),  /**< Quantization step
    section.                                                                 */
  MLP_BHC_OUTPUT_SHIFT           = (1u << 5),  /**< Output samples shift
    section.                                                                 */
  MLP_BHC_MATRIX_PARAMETERS      = (1u << 6),  /**< Primitive matrices
    parameters section.                                                      */
  MLP_BHC_BLOCK_SIZE             = (1u << 7),  /**< Block size section.      */

  MLP_BHC_DEFAULT                = 0xFF  /**< Block header content default
    value, setting presence of all sections.                                 */
} MlpBlockHeaderContentFlag;

#define MLP_MAX_NB_MATRICES  8

typedef struct {
  bool lsb_bypass_exists;
} MlpMatrix;

typedef struct {
  unsigned num_primitive_matrices;
  MlpMatrix matrices[MLP_MAX_NB_MATRICES];
} MlpMatrixParameters;

typedef enum {
  MLP_HUFFCB_NONE     = 0x0,
  MLP_HUFFCB_TABLE_0  = 0x1,
  MLP_HUFFCB_TABLE_1  = 0x2,
  MLP_HUFFCB_TABLE_2  = 0x3
} MlpHuffmanCodebook;

static inline const char * MlpHuffmanCodebookStr(
  MlpHuffmanCodebook huffman_codebook
)
{
  static const char * strings[] = {
    "None, no entropy coded MSB",
    "Table 0, -7 to +10 values",
    "Table 1, -7 to +8 values",
    "Table 2, -7 to +7 values"
  };

  return strings[huffman_codebook];
}

#define MLP_FIR_MAX_ORDER  8
#define MLP_IIR_MAX_ORDER  4
#define MLP_FIR_IIR_TOTAL_MAX_ORDER  8

typedef struct {
  uint8_t filter_order;
  uint8_t shift;
} MlpFilter;

typedef struct {
  unsigned fir_filter_nb_changes;
  MlpFilter fir_filter;
  unsigned iir_filter_nb_changes;
  MlpFilter iir_filter;

  MlpHuffmanCodebook huffman_codebook;
  uint8_t num_huffman_lsbs;
} MlpChannelParameters;

#define MLP_TERMINATOR_A  0x348D3
#define MLP_TERMINATOR_B  0x1234

#define MLP_SS_CRC_PARAMS                                                     \
  (CrcParam) {.table = mlp_ss_crc_8_table, .length = 8, .shifted = true}
  // (CrcParam) {.length = 8, .poly = 0x63, .shifted = true}

typedef struct {
  MlpRestartHeader restart_header;
  bool restart_header_seen;

  uint8_t block_header_content;

  unsigned block_size;  /**< Number of samples carried in one block.
    Can be redefined in block header, 8 by default.                          */

  MlpMatrixParameters matrix_parameters;
  unsigned matrix_parameters_nb_changes;

  uint8_t quant_step_size[MLP_MAX_NB_CHANNELS];

  MlpChannelParameters channels_parameters[MLP_MAX_NB_CHANNELS];

  uint16_t cur_output_timing;  /**< Parsed samples counter, used to check
    output_timing fields. Modulo 0xFFFF. */
  bool terminator_reached;
} MlpSubstreamParameters;

#define MLP_MAX_NB_SS  4

#define TRUE_HD_UNITS_PER_SEC  1200

#define TRUE_HD_SAMPLES_PER_FRAME  (48000 / TRUE_HD_UNITS_PER_SEC)

/* ### AC-3 Audio Descriptor : ############################################# */

static inline uint8_t get_sample_rate_code_ac3_descriptor(
  Ac3Fscod fscod
)
{
  return (uint8_t) fscod;
}

static inline uint8_t get_bit_rate_code_ac3_descriptor(
  uint8_t frmsizecod
)
{
  return frmsizecod >> 1u;
}

static inline uint8_t get_num_channels_ac3_descriptor(
  Ac3Acmod acmod
)
{
  return (uint8_t) acmod;
}

/* ### BDAV constraints : ################################################## */

/* ###### AC-3 : ########################################################### */

/** \~english
 * \brief BDAV specifications minimal allowed AC-3 bit-rate in kbps.
 *
 * Fixed to 96 kbps.
 */
#define BDAV_AC3_MINIMAL_BITRATE  96

/* ###### TrueHD / MLP : ################################################### */

/** \~english
 * \brief BDAV maximum allowed combined TrueHD bit-rate.
 *
 * Equal to 18 * 10**6 bps (18 mbps).
 */
#define BDAV_TRUE_HD_MAX_AUTHORIZED_BITRATE  (18*1000000)

/** \~english
 * TODO
 */
#define TRUE_HD_MAX_AUTHORIZED_16CH_CHANNEL_COUNT  15

#endif