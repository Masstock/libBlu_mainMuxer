/** \~english
 * \file hdmv_palette.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief
 */

#ifndef __LIBBLU_MUXER__CODECS__HDMV__COMMON__PALETTE_H__
#define __LIBBLU_MUXER__CODECS__HDMV__COMMON__PALETTE_H__

#include "hdmv_error.h"
#include "hdmv_color.h"
#include "hdmv_data.h"

/* ### YCbCr Color conversion matrix : ##################################### */

/** \~english
 * \brief HDMV Palette RGB to YCbCr conversion matrix standard.
 *
 */
typedef enum {
  HDMV_PAL_CM_BT_601,  /**< Rec. ITU-R BR.601-7 (03/2011) */
  HDMV_PAL_CM_BT_709,
  HDMV_PAL_CM_BT_2020
} HdmvPaletteColorMatrix;

static inline const char *HdmvPaletteColorMatrixStr(
  HdmvPaletteColorMatrix matrix
)
{
  static const char *strings[] = {
    "BT.601",
    "BT.709",
    "BT.2020"
  };

  if (matrix < ARRAY_SIZE(strings))
    return strings[matrix];
  return "Unknown";
}

static inline int getCoefficientsHdmvPaletteColorMatrix(
  HdmvPaletteColorMatrix matrix,
  float *r,
  float *g,
  float *b
)
{
  static const float values[][3] = {
    { 0.299f,  0.587f,  0.114f},
    {0.2126f, 0.7152f, 0.0722f},
    {0.2627f, 0.6780f, 0.0593f}
  };

  if (ARRAY_SIZE(values) <= matrix)
    return -1;

  *r = values[matrix][0];
  *g = values[matrix][1];
  *b = values[matrix][2];

  return 0;
}

uint32_t convToYCbCrHdmvPaletteColorMatrix(
  uint32_t rgba,
  HdmvPaletteColorMatrix matrix
);

uint32_t convToRGBAHdmvPaletteColorMatrix(
  uint32_t ycbcra,
  HdmvPaletteColorMatrix matrix
);

/* ### HDMV Palette : ###################################################### */

typedef struct {
  uint32_t rgba;   /**< RGB + Alpha. */
  uint32_t ycbcr;  /**< YCbCr + Alpha. */

  bool in_use;
} HdmvPaletteEntry;

#define HDMV_PAL_SIZE  0xFF

/** \~english
 * \brief HDMV Palette.
 *
 */
typedef struct {
  uint8_t version;
  HdmvPaletteColorMatrix ycbcr_matrix;

  HdmvPaletteEntry entries[HDMV_PAL_SIZE];
  unsigned nb_entries;
  bool non_sequential;
} HdmvPalette;

/* ###### Creation / Destruction : ######################################### */

static inline void initHdmvPalette(
  HdmvPalette *dst,
  uint8_t version,
  HdmvPaletteColorMatrix ycbcr_matrix
)
{
  *dst = (HdmvPalette) {
    .version = version,
    .ycbcr_matrix = ycbcr_matrix
  };
}

/* ###### Accessors : ###################################################### */

static inline unsigned getNbEntriesHdmvPalette(
  const HdmvPalette pal
)
{
  if (!pal.non_sequential)
    return pal.nb_entries;

  unsigned count = 0u;
  for (unsigned i = 0; i < HDMV_PAL_SIZE; i++)
    if (pal.entries[i].in_use) count++;
  return count;
}

/* ###### Operations : ##################################################### */

void sortEntriesHdmvPalette(
  HdmvPalette *pal
);

/* ###### Add Entry : ###################################################### */

int addRgbaEntryHdmvPalette(
  HdmvPalette *pal,
  uint32_t rgba
);

/* ###### Get Entry : ###################################################### */

static inline int getYCbCrEntryHdmvPalette(
  const HdmvPalette *pal,
  uint32_t *ycbcr_ret,
  unsigned id
)
{
  assert(id < HDMV_PAL_SIZE);
  if (!pal->entries[id].in_use)
    LIBBLU_HDMV_PIC_ERROR_RETURN("Invalid palette entry ID %zu.\n", id);
  *ycbcr_ret = pal->entries[id].ycbcr;
  return 0;
}

static inline bool getYCbCrEntryIfInUseHdmvPalette(
  const HdmvPalette pal,
  uint32_t *ycbcr_ret,
  unsigned id
)
{
  assert(id < HDMV_PAL_SIZE);
  if (!pal.entries[id].in_use)
    return false;
  *ycbcr_ret = pal.entries[id].ycbcr;
  return true;
}

#endif