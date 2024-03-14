/** \~english
 * \file lpcm_parser.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief LPCM audio (Uncompressed) bitstreams handling module.
 */

/** \~english
 * \dir lpcm
 *
 * \brief LPCM audio bitstreams handling modules.
 *
 * \todo Cleanup
 * \todo Split module in sub-modules to increase readability.
 * \todo Add Wav64 support.
 *
 * \xrefitem references "References" "References list"
 *  [1] Audio File Format Specifications (for all of this sources)
 *      http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html;\n
 *  [2] WAVE specifications, Version 1.0, 1991-08 (Multimedia Programming
 *      Interface and Data Specifications 1.0);\n
 *  [3] WAVE update (Revision: 3.0), 1994-04-15 (Multiple Channel Audio Data
 *      and WAVE Files);\n
 *  [4] Multi-channel / high bit resolution formats, 2001-12-04 (Multiple
 *      Channel Audio Data and WAVE Files).
 */

#ifndef __LIBBLU_MUXER_CODECS__LCPM__PARSER_H__
#define __LIBBLU_MUXER_CODECS__LCPM__PARSER_H__

#include "lpcm_error.h"
#include "lpcm_data.h"
#include "lpcm_util.h"

#define LPCM_PES_HEADER_LENGTH 0x04
#define LPCM_PES_FRAMES_PER_SECOND 200

typedef enum {
  WAVE_CH_MASK_L    = (1 <<  0),
  WAVE_CH_MASK_R    = (1 <<  1),
  WAVE_CH_MASK_C    = (1 <<  2),
  WAVE_CH_MASK_LFE  = (1 <<  3),
  WAVE_CH_MASK_LS   = (1 <<  4),
  WAVE_CH_MASK_RS   = (1 <<  5),
  WAVE_CH_MASK_LC   = (1 <<  6),
  WAVE_CH_MASK_RC   = (1 <<  7),
  WAVE_CH_MASK_CS   = (1 <<  8),
  WAVE_CH_MASK_LSS  = (1 <<  9),
  WAVE_CH_MASK_RSS  = (1 << 10),
  WAVE_CH_MASK_OH   = (1 << 11),
  WAVE_CH_MASK_LH   = (1 << 12),
  WAVE_CH_MASK_CH   = (1 << 13),
  WAVE_CH_MASK_RH   = (1 << 14),
  WAVE_CH_MASK_LHR  = (1 << 15),
  WAVE_CH_MASK_CHR  = (1 << 16),
  WAVE_CH_MASK_RHR  = (1 << 17),
  WAVE_CH_MASK_RES  = (1 << 31),
} WaveChannelMaskCode;

static inline unsigned nbChannelsWaveChannelMask(
  uint32_t dwChannelMask
)
{
  unsigned i, nbChannels;

  nbChannels = 0;
  for (i = 0; i < 31; i++)
    nbChannels += (dwChannelMask >> i) & 0x1;

  return nbChannels;
}

#define WAVE_CH_MASK_STR_BUFSIZE  160

static inline void buildStrReprWaveChannelMask(
  char *dst,
  uint32_t dwChannelMask
)
{
  unsigned i;
  char *sep;

  static const char *strings[] = {
    "L",
    "R",
    "C",
    "LFE",
    "Ls",
    "Rs",
    "Lc",
    "Rc",
    "Cs",
    "Lss",
    "Rss",
    "Oh",
    "Lh",
    "Ch",
    "Rh",
    "Lhr",
    "Chr",
    "Rhr"
  };

  sep = "";
  for (i = 0; i < ARRAY_SIZE(strings); i++) {
    if (dwChannelMask & (0x1 << i)) {
      lb_str_cat(&dst, sep);
      lb_str_cat(&dst, strings[i]);
    }
    sep = ", ";
  }
}

typedef struct {
  unsigned size;

  struct {
    unsigned validBitsPerSample;
    uint32_t speakersLayoutMask;
    uint8_t subFormatTag[16];
  } content;
} WaveFmtExtensible;

typedef enum {
  WAVE_FORMAT_PCM         = 0x0001,
  WAVE_FORMAT_EXTENSIBLE  = 0xFFFE
} WaveFmtFormatTag;

static inline const char *WaveFmtFormatTagStr(
  WaveFmtFormatTag wFormatTag
)
{
  switch (wFormatTag) {
  case WAVE_FORMAT_PCM:
    return "Microsoft Pulse Code Modulation (PCM)";
  case WAVE_FORMAT_EXTENSIBLE:
    return "Extensible";
  }
  return "Unknown";
}

typedef struct {
  WaveFmtFormatTag wFormatTag;
  uint16_t wChannels;
  uint32_t nSamplesPerSec;
  uint32_t dwAvgBytesPerSec;
  uint16_t wBlockAlign;
} WaveFmtChunkCommonFields;

typedef struct {
  uint16_t wBitsPerSample;
} WaveFmtPCMFormatSpecific;

typedef struct {
  WaveFmtPCMFormatSpecific pcm;  /**< PCM format specific common. Placed
    at structure start to ensure correct memory mapping on parsing/checking. */
  union {
    uint16_t wValidBitsPerSample;
    uint16_t wSamplesPerBlock;
  } samples;
  uint16_t cbSize;
  uint32_t dwChannelMask;
  LB_DECL_GUID(SubFormat);
} WaveFmtExtensibleSpecific;

typedef union {
  WaveFmtPCMFormatSpecific pcm;
  WaveFmtExtensibleSpecific extensible;
} WaveFmtChunkFormatSpecificFields;

typedef struct {
  WaveFmtChunkCommonFields common_fields;
  WaveFmtChunkFormatSpecificFields fmt_spec;
} WaveFmtChunk;

typedef struct {
  // uint32_t fileSize;
  // uint32_t data_size;
  // uint32_t pesPacketLength;
  RiffFormHeader riff_form;

  WaveFmtChunk fmt;
  bool fmt_present;
  RiffChunkHeader data;
} WaveFile;

int analyzeLpcm(
  LibbluESParsingSettings *settings
);

#endif
