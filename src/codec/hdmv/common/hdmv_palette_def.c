#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>
#include <assert.h>

#include "hdmv_palette_def.h"

/* ### YCbCr Conversion matrix : ########################################### */

/** \~english
 * \brief
 *
 * \param rgba
 * \param coeff_r
 * \param coeff_g
 * \param coeff_b
 * \param off_y Y' channel digital quantification offset. If conversion use
 * 'limited range', this value shall be set to 16. Otherwise, 0.
 * \param off_br CbCr channels digital quantification offset. If conversion use
 * 'limited range', this value shall be set to 128. Otherwise, 0.
 * \param scl_y Y' channel digital quantification scalar. If conversion use
 * 'limited range', this value shall be set to 219/255. Otherwise, 1.
 * \param scl_br CbCr channels digital quantification scalar. If conversion use
 * 'limited range', this value shall be set to 224/255. Otherwise, 2.
 * \return uint32_t
 *
 * \note off_y, off_br, scl_y and scl_br values are used to scale YCbCr
 * values range if requested. This process is described in [1] 2.5.3.
 */
static uint32_t convRGBAToYCbCrA(
  uint32_t rgba,
  float coeff_r,
  float coeff_g,
  float coeff_b,
  float off_y,
  float off_br,
  float scl_y,
  float scl_br
)
{
  float coeff_cb_r;
  float coeff_cb_g;
  float coeff_cr_g;
  float coeff_cr_b;

  float r, g, b;
  float y, cb, cr;
  uint8_t ch_y, ch_cb, ch_cr, ch_a;
  uint32_t ycbcra;

  /* Compute fixed correction coefficients: */
  coeff_cb_r = - coeff_r / (1 - coeff_b);
  coeff_cb_g = - coeff_g / (1 - coeff_b);
  coeff_cr_g = - coeff_g / (1 - coeff_r);
  coeff_cr_b = - coeff_b / (1 - coeff_r);

  /* Extract each channel value: */
  r    = (float) GET_CHANNEL(rgba, C_R);
  g    = (float) GET_CHANNEL(rgba, C_G);
  b    = (float) GET_CHANNEL(rgba, C_B);
  ch_a = (float) GET_CHANNEL(rgba, C_A);

  /* Apply transformation matrix */
  y  = coeff_r          * r + coeff_g          * g + coeff_b          * b;
  cb = 0.5 * coeff_cb_r * r + 0.5 * coeff_cb_g * g + 0.5              * b;
  cr = 0.5              * r + 0.5 * coeff_cr_g * g + 0.5 * coeff_cr_b * b;

  /* Apply YCbCr values range clipping if required. */
  /* e.g. (0-255, 0-255, 0-255) -> (16-235, 16-240, 16-240) */
#define ROUND_FUN  nearbyintf
  ch_y  = CLIP_UINT8(ROUND_FUN(off_y  + y  * scl_y ));
  ch_cb = CLIP_UINT8(ROUND_FUN(off_br + cb * scl_br));
  ch_cr = CLIP_UINT8(ROUND_FUN(off_br + cr * scl_br));

  /* Pack result */
  ycbcra  = CHANNEL_VALUE(ch_y ,  C_Y);
  ycbcra |= CHANNEL_VALUE(ch_cb, C_CB);
  ycbcra |= CHANNEL_VALUE(ch_cr, C_CR);
  ycbcra |= CHANNEL_VALUE(ch_a ,  C_A);

  /* lbc_printf(
    " => %.0f %.0f %.0f -> %u %u %u.\n",
    r, g, b,
    ch_y, ch_cb, ch_cr
  ); */

  return ycbcra;
}

uint32_t convToYCbCrHdmvPaletteColorMatrix(
  uint32_t rgba,
  HdmvPaletteColorMatrix matrix
)
{
  float r, g, b;
  float off_y, off_br, scl_y, scl_br;

  if (getCoefficientsHdmvPaletteColorMatrix(matrix, &r, &g, &b) < 0)
    return 0x0; /* Invalid / disabled matrix */

  off_y  = 16.0;
  off_br = 128.0;
  scl_y  = 219.0 / 255.0;
  scl_br = 224.0 / 255.0;

  return convRGBAToYCbCrA(rgba, r, g, b, off_y, off_br, scl_y, scl_br);
}

static void convertToYCbCrHdmvPaletteDefinition(
  HdmvPaletteDefinitionPtr pal
)
{
  float r, g, b;
  float off_y, off_br, scl_y, scl_br;
  size_t i;

  if (getCoefficientsHdmvPaletteColorMatrix(pal->ycbcrMatrix, &r, &g, &b) < 0) {
    /* Invalid / disabled matrix */
    for (i = 0; i < pal->nbUsedEntries; i++)
      pal->entries[i].ycbcr = 0x0;
    return;
  }

  off_y = 16.0;
  off_br = 128.0;
  scl_y = 219.0 / 255.0;
  scl_br = 224.0 / 255.0;

  LIBBLU_HDMV_PAL_DEBUG("Converting palette entries:\n");
  for (i = 0; i < pal->nbUsedEntries; i++) {
    pal->entries[i].ycbcr = convRGBAToYCbCrA(
      pal->entries[i].rgba, r, g, b, off_y, off_br, scl_y, scl_br
    );

    LIBBLU_HDMV_PAL_DEBUG(
      " - Palette_entry(%zu): 0x%08X => 0x%08X;\n",
      i,
      pal->entries[i].rgba,
      pal->entries[i].ycbcr
    );
  }

  pal->updatedYcbcr = true;
}

static void convertToRgbaHdmvPaletteDefinition(
  HdmvPaletteDefinitionPtr pal
)
{
  (void) pal;
  LIBBLU_ERROR("NOT IMPLEMENTED %s %d\n", __FILE__, __LINE__);
}

/* ### Palette creation / destruction : #################################### */

HdmvPaletteDefinitionPtr createHdmvPaletteDefinition(
  uint8_t version
)
{
  HdmvPaletteDefinitionPtr pal;

  pal = (HdmvPaletteDefinitionPtr) malloc(sizeof(HdmvPaletteDefinition));
  if (NULL == pal)
    LIBBLU_HDMV_PAL_ERROR_NRETURN("Memory allocation error.\n");

  *pal = (HdmvPaletteDefinition) {
    .version = version
  };

  return pal;
}

/* ### Operations : ######################################################## */

void setColorMatrixHdmvPaletteDefinition(
  HdmvPaletteDefinitionPtr pal,
  HdmvPaletteColorMatrix matrix
)
{
  if (pal->ycbcrMatrix == matrix)
    return;

  pal->ycbcrMatrix = matrix;
  pal->updatedYcbcr = false;
}

static int compFun(
  const void * first,
  const void * second
)
{
  int64_t rgbaFirst = ((HdmvPaletteEntry *) first)->rgba;
  int64_t rgbaSecond = ((HdmvPaletteEntry *) second)->rgba;
  int64_t distFirst, distSecond;

  distFirst = GET_DIST(rgbaFirst);
  distSecond = GET_DIST(rgbaSecond);

  return MAX(INT_MIN, MIN(INT_MAX, distFirst - distSecond));
}

void sortEntriesHdmvPaletteDefinition(
  HdmvPaletteDefinitionPtr pal
)
{
  updateHdmvPaletteDefinition(pal);

  qsort(pal->entries, pal->nbUsedEntries, sizeof(HdmvPaletteEntry), compFun);
}

void updateHdmvPaletteDefinition(
  HdmvPaletteDefinitionPtr pal
)
{
  /* If RGBA is up to date, convert it to YCbCr */
  if (pal->updatedRgba)
    return convertToYCbCrHdmvPaletteDefinition(pal);

  /* Otherwise convert from YCbCr to RGBA */
  assert(pal->updatedYcbcr); /* Assume YCbCr is available */
  convertToRgbaHdmvPaletteDefinition(pal);
}
