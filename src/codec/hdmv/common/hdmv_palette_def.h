/** \~english
 * \file hdmv_palette_def.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief
 *
 * \xrefitem references "References" "References list"
 *  [1] Rec. ITU-R BR.601-7 (03/2011)\n
 */

#ifndef __LIBBLU_MUXER__CODECS__HDMV__COMMON__PALETTE_DEF_H__
#define __LIBBLU_MUXER__CODECS__HDMV__COMMON__PALETTE_DEF_H__

#include "hdmv_error.h"
#include "hdmv_color.h"

/* ### YCbCr Color conversion matrix : ##################################### */

/** \~english
 * \brief HDMV Palette RGB to YCbCr conversion matrix standard.
 *
 */
typedef enum {
  HDMV_PAL_CM_DISABLED,

  HDMV_PAL_CM_BT_601,
  HDMV_PAL_CM_BT_709,
  HDMV_PAL_CM_BT_2020
} HdmvPaletteColorMatrix;

static inline const char * HdmvPaletteColorMatrixStr(
  HdmvPaletteColorMatrix matrix
)
{
  static const char * strings[] = {
    "Disabled",
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
  float * r,
  float * g,
  float * b
)
{
  static const float values[][3] = {
    { 0.299f,  0.587f,  0.114f},
    {0.2126f, 0.7152f, 0.0722f},
    {0.2627f, 0.6780f, 0.0593f}
  };

  if (matrix <= 0 || ARRAY_SIZE(values) <= matrix-1)
    return -1;

  *r = values[matrix-1][0];
  *g = values[matrix-1][1];
  *b = values[matrix-1][2];

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
  uint32_t rgba;  /**< RGB + Alpha. */
  uint32_t ycbcr;  /**< YCbCr + Alpha. */
} HdmvPaletteEntry;

#define HDMV_PAL_SIZE 256

/** \~english
 * \brief HDMV Palette.
 *
 */
typedef struct {
  uint8_t version;

  HdmvPaletteColorMatrix ycbcrMatrix;
  bool updatedRgba;
  bool updatedYcbcr;

  HdmvPaletteEntry entries[HDMV_PAL_SIZE];
  size_t nbUsedEntries;
} HdmvPaletteDefinition, *HdmvPaletteDefinitionPtr;

/* ###### Creation / Destruction : ######################################### */

HdmvPaletteDefinitionPtr createHdmvPaletteDefinition(
  uint8_t version
);

static inline void destroyHdmvPaletteDefinition(
  HdmvPaletteDefinitionPtr pal
)
{
  if (NULL == pal)
    return;
  free(pal);
}

/* ###### Accessors : ###################################################### */

static inline HdmvPaletteColorMatrix getColorMatrixHdmvPaletteDefinition(
  const HdmvPaletteDefinitionPtr pal
)
{
  return pal->ycbcrMatrix;
}

static inline uint8_t getVersionHdmvPaletteDefinition(
  const HdmvPaletteDefinitionPtr pal
)
{
  return pal->version;
}

static inline size_t getNbEntriesHdmvPaletteDefinition(
  const HdmvPaletteDefinitionPtr pal
)
{
  return pal->nbUsedEntries;
}

/* ###### Operations : ##################################################### */

/** \~english
 * \brief Set the color matrix conversion used to produce YCbCr values.
 *
 * \param pal
 * \param matrix
 */
void setColorMatrixHdmvPaletteDefinition(
  HdmvPaletteDefinitionPtr pal,
  HdmvPaletteColorMatrix matrix
);

void sortEntriesHdmvPaletteDefinition(
  HdmvPaletteDefinitionPtr pal
);

void updateHdmvPaletteDefinition(
  HdmvPaletteDefinitionPtr pal
);

/* ###### Add Entry : ###################################################### */

static inline int addRgbaEntryHdmvPaletteDefinition(
  HdmvPaletteDefinitionPtr pal,
  uint32_t rgba
)
{
  if (HDMV_PAL_SIZE <= pal->nbUsedEntries)
    LIBBLU_HDMV_PIC_ERROR_RETURN("Too many palette entries.\n");

  if (!pal->updatedRgba && 0 < pal->nbUsedEntries)
    updateHdmvPaletteDefinition(pal);

  pal->entries[pal->nbUsedEntries++] = (HdmvPaletteEntry) {
    .rgba = rgba
  };

  pal->updatedRgba = true;
  pal->updatedYcbcr = false;
  return 0;
}

static inline int addYcbcrEntryHdmvPaletteDefinition(
  HdmvPaletteDefinitionPtr pal,
  uint32_t ycbcr
)
{
  if (HDMV_PAL_SIZE <= pal->nbUsedEntries)
    LIBBLU_HDMV_PIC_ERROR_RETURN("Too many palette entries.\n");

  if (!pal->updatedYcbcr && 0 < pal->nbUsedEntries)
    updateHdmvPaletteDefinition(pal);

  pal->entries[pal->nbUsedEntries++] = (HdmvPaletteEntry) {
    .ycbcr = ycbcr
  };

  pal->updatedRgba = false;
  pal->updatedYcbcr = true;

  return 0;
}

/* ###### Get Entry : ###################################################### */

static inline int getRgbaEntryHdmvPaletteDefinition(
  HdmvPaletteDefinitionPtr pal,
  size_t id,
  uint32_t * rgba
)
{
  if (getNbEntriesHdmvPaletteDefinition(pal) <= id)
    LIBBLU_HDMV_PIC_ERROR_RETURN("Invalid palette entry ID %zu.\n", id);

  if (!pal->updatedRgba)
    updateHdmvPaletteDefinition(pal);

  *rgba = pal->entries[id].rgba;
  return 0;
}

static inline int getYCbCrEntryHdmvPaletteDefinition(
  HdmvPaletteDefinitionPtr pal,
  size_t id,
  uint32_t * ycbcr
)
{
  if (getNbEntriesHdmvPaletteDefinition(pal) <= id)
    LIBBLU_HDMV_PIC_ERROR_RETURN("Invalid palette entry ID %zu.\n", id);

  if (!pal->updatedYcbcr)
    updateHdmvPaletteDefinition(pal);

  *ycbcr = pal->entries[id].ycbcr;

  return 0;
}

#endif