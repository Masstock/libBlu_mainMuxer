#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>
#include <assert.h>

#include "hdmv_palette.h"

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
static uint32_t _convRGBAToYCbCrA(
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
  /* Compute fixed correction coefficients: */
  float cf_cb_r = - coeff_r / (1 - coeff_b);
  float cf_cb_g = - coeff_g / (1 - coeff_b);
  float cf_cr_g = - coeff_g / (1 - coeff_r);
  float cf_cr_b = - coeff_b / (1 - coeff_r);

  /* Extract each channel value: */
  float r  = (float) GET_CHANNEL(rgba, C_R);
  float g  = (float) GET_CHANNEL(rgba, C_G);
  float b  = (float) GET_CHANNEL(rgba, C_B);

  /* Apply transformation matrix */
  float y  = coeff_r        * r + coeff_g        * g + coeff_b        * b;
  float cb = 0.5f * cf_cb_r * r + 0.5f * cf_cb_g * g + 0.5f           * b;
  float cr = 0.5f           * r + 0.5f * cf_cr_g * g + 0.5f * cf_cr_b * b;

  /* Apply YCbCr values range clipping if required. */
  /* e.g. (0-255, 0-255, 0-255) -> (16-235, 16-240, 16-240) */
#define ROUND_FUN  nearbyintf
  uint8_t ch_y  = CLIP_UINT8(ROUND_FUN(off_y  + y  * scl_y ));
  uint8_t ch_cb = CLIP_UINT8(ROUND_FUN(off_br + cb * scl_br));
  uint8_t ch_cr = CLIP_UINT8(ROUND_FUN(off_br + cr * scl_br));

  uint8_t ch_a  = GET_CHANNEL(rgba, C_A);

  /* Pack result */
  uint32_t ycbcra = 0x00;
  ycbcra |= CHANNEL_VALUE(ch_y ,  C_Y);
  ycbcra |= CHANNEL_VALUE(ch_cb, C_CB);
  ycbcra |= CHANNEL_VALUE(ch_cr, C_CR);
  ycbcra |= CHANNEL_VALUE(ch_a ,  C_A);

  return ycbcra;
}

#define USE_LIMITED_RANGE

uint32_t convToYCbCrHdmvPaletteColorMatrix(
  uint32_t rgba,
  HdmvPaletteColorMatrix matrix
)
{
  float r, g, b;
  if (getCoefficientsHdmvPaletteColorMatrix(matrix, &r, &g, &b) < 0)
    LIBBLU_HDMV_PAL_ERROR_EXIT("Invalid/disable %u matrix.\n", matrix);

#if defined(USE_LIMITED_RANGE)
  float off_y  = 16.f;
  float off_br = 128.f;
  float scl_y  = 219.f / 255.f;
  float scl_br = 224.f / 255.f;
#else
  float off_y  = 0.f;
  float off_br = 0.f;
  float scl_y  = 1.f;
  float scl_br = 2.f;
#endif

  return _convRGBAToYCbCrA(rgba, r, g, b, off_y, off_br, scl_y, scl_br);
}

#if 0

static void convertToYCbCrHdmvPalette(
  HdmvPalette * pal
)
{
  float r, g, b;
  size_t i;

  if (getCoefficientsHdmvPaletteColorMatrix(pal->ycbcr_matrix, &r, &g, &b) < 0) {
    /* Invalid / disabled matrix */
    for (i = 0; i < pal->nb_entries; i++)
      pal->entries[i].ycbcr = 0x0;
    return;
  }

  float off_y  = 16.f;
  float off_br = 128.f;
  float scl_y  = 219.f / 255.f;
  float scl_br = 224.f / 255.f;

  LIBBLU_HDMV_PAL_DEBUG("Converting palette entries:\n");
  for (i = 0; i < pal->nb_entries; i++) {
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
}

static void convertToRgbaHdmvPalette(
  HdmvPalette * pal
)
{
  (void) pal;
  LIBBLU_ERROR("NOT IMPLEMENTED %s %d\n", __FILE__, __LINE__);
}

/* ### Operations : ######################################################## */

void setColorMatrixHdmvPalette(
  HdmvPalette * pal,
  HdmvPaletteColorMatrix matrix
)
{
  if (pal->ycbcr_matrix == matrix)
    return;

  pal->ycbcr_matrix = matrix;
}

#endif

static int compFun(
  const void * first,
  const void * second
)
{
  int64_t rgba_fst = ((HdmvPaletteEntry *)  first)->rgba;
  int64_t rgba_sec = ((HdmvPaletteEntry *) second)->rgba;

  int64_t dist_fst = GET_DIST(rgba_fst);
  int64_t dist_sec = GET_DIST(rgba_sec);

  return MAX(INT_MIN, MIN(INT_MAX, dist_fst - dist_sec));
}

void sortEntriesHdmvPalette(
  HdmvPalette * pal
)
{
  qsort(pal->entries, pal->nb_entries, sizeof(HdmvPaletteEntry), compFun);
}

int addRgbaEntryHdmvPalette(
  HdmvPalette * pal,
  uint32_t rgba
)
{
  unsigned idx;

  if (!pal->non_sequential) {
    if (HDMV_PAL_SIZE <= pal->nb_entries)
      LIBBLU_HDMV_PIC_ERROR_RETURN("Too many palette entries.\n");
    idx = pal->nb_entries++;
  }
  else {
    /* Find available entry */
    idx = HDMV_PAL_SIZE;
    for (unsigned i = 0; i < HDMV_PAL_SIZE; i++) {
      if (!pal->entries[i].in_use) {
        idx = i;
        break;
      }
    }
    if (HDMV_PAL_SIZE == idx)
      LIBBLU_HDMV_PIC_ERROR_RETURN("Too many palette entries.\n");
  }

  pal->entries[idx] = (HdmvPaletteEntry) {
    .rgba   = rgba,
    .ycbcr  = convToYCbCrHdmvPaletteColorMatrix(rgba, pal->ycbcr_matrix),
    .in_use = true
  };

  return 0;
}