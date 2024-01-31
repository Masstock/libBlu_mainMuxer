#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include "hdmv_paletized_bitmap.h"

#define GET_MAX_DIST  false

typedef struct {
  int16_t r;
  int16_t g;
  int16_t b;
  int16_t a;
} IntRgba;

static unsigned _findNearestColorPalette(
  const HdmvPalette * pal,
  uint32_t rgba,
  IntRgba * quand_error_ret
)
{
#if GET_MAX_DIST
  int max_dist_rgba = 0;
#endif
  int32_t min_dist_rgba = INT32_MAX;
  uint8_t selected_idx = 0xFF; // By default 0xFF transparent color

  assert(NULL != pal);

  uint8_t s_r = GET_CHANNEL(rgba, C_R);
  uint8_t s_g = GET_CHANNEL(rgba, C_G);
  uint8_t s_b = GET_CHANNEL(rgba, C_B);
  uint8_t s_a = GET_CHANNEL(rgba, C_A);

  unsigned pal_nb_looked_entries =
    pal->non_sequential ? HDMV_PAL_SIZE : pal->nb_entries
  ;

  for (unsigned i = 0; i < pal_nb_looked_entries; i++) {
    HdmvPaletteEntry entry = pal->entries[i];
    if (!entry.in_use)
      continue;

    uint32_t pal_rgba = entry.rgba;
    uint8_t p_r = GET_CHANNEL(pal_rgba, C_R);
    uint8_t p_g = GET_CHANNEL(pal_rgba, C_G);
    uint8_t p_b = GET_CHANNEL(pal_rgba, C_B);
    uint8_t p_a = GET_CHANNEL(pal_rgba, C_A);

    uint16_t dist_r = ABS((int16_t) s_r - p_r);
    uint16_t dist_g = ABS((int16_t) s_g - p_g);
    uint16_t dist_b = ABS((int16_t) s_b - p_b);
    uint16_t dist_a = ABS((int16_t) s_a - p_a);

    int32_t dist_rgba =
      (dist_r * dist_r)
      + (dist_g * dist_g)
      + (dist_b * dist_b)
      + (dist_a * dist_a)
    ;

#if GET_MAX_DIST
    if (max_dist_rgba < dist_rgba)
      max_dist_rgba = dist_rgba;
#endif

    if (dist_rgba < min_dist_rgba) {
      min_dist_rgba = dist_rgba;
      selected_idx = i;

      if (pal_rgba == rgba)
        break; /* Exact color found */
    }
  }

  /* LIBBLU_IGS_COMPL_QUANT_DEBUG("Min color error distance: %d.\n", minDist_rgba); */
#if GET_MAX_DIST
  /* LIBBLU_IGS_COMPL_QUANT_DEBUG("Max color error distance: %d.\n", maxDist_rgba); */
#endif

  uint32_t pal_rgba = pal->entries[selected_idx].rgba;
  if (NULL != quand_error_ret) {
    *quand_error_ret = (IntRgba) {
      .r = s_r - (int) GET_CHANNEL(pal_rgba, C_R),
      .g = s_g - (int) GET_CHANNEL(pal_rgba, C_G),
      .b = s_b - (int) GET_CHANNEL(pal_rgba, C_B),
      .a = s_a - (int) GET_CHANNEL(pal_rgba, C_A)
    };
  }

  return selected_idx;
}

static void _computeCoeffFloydSteinberg(
  IntRgba * res,
  float div,
  IntRgba quand_error
)
{
  res->r = (int) nearbyintf(div * quand_error.r);
  res->g = (int) nearbyintf(div * quand_error.g);
  res->b = (int) nearbyintf(div * quand_error.b);
  res->a = (int) nearbyintf(div * quand_error.a);
}

static uint32_t _applyCoeffErrorFloydSteinberg(
  uint32_t rgba,
  IntRgba coeffs
)
{
  uint32_t ret = 0x0;
  ret |= CHANNEL_VALUE(CLIP_UINT8((int) GET_CHANNEL(rgba, C_R) + coeffs.r), C_R);
  ret |= CHANNEL_VALUE(CLIP_UINT8((int) GET_CHANNEL(rgba, C_G) + coeffs.g), C_G);
  ret |= CHANNEL_VALUE(CLIP_UINT8((int) GET_CHANNEL(rgba, C_B) + coeffs.b), C_B);
  ret |= CHANNEL_VALUE(CLIP_UINT8((int) GET_CHANNEL(rgba, C_A) + coeffs.a), C_A);
  return ret;
}

void _applyFloydSteinberg(
  uint32_t * rgba,
  uint16_t x,
  uint16_t y,
  uint16_t stride,
  IntRgba quant_error
)
{
  uint32_t * px;
  IntRgba coeff;

  /* (x+1, y) 7/16 */
  px = &rgba[y*stride + x+1];
  _computeCoeffFloydSteinberg(&coeff, 7.0f / 16.0f, quant_error);
  *px = _applyCoeffErrorFloydSteinberg(*px, coeff);

  /* (x-1, y+1) 3/16 */
  px = &rgba[(y+1)*stride + x-1];
  _computeCoeffFloydSteinberg(&coeff, 3.0f / 16.0f, quant_error);
  *px = _applyCoeffErrorFloydSteinberg(*px, coeff);

  /* (x, y+1) 5/16 */
  px = &rgba[(y+1)*stride + x];
  _computeCoeffFloydSteinberg(&coeff, 5.0f / 16.0f, quant_error);
  *px = _applyCoeffErrorFloydSteinberg(*px, coeff);

  /* (x+1, y+1) 1/16 */
  px = &rgba[(y+1)*stride + x+1];
  _computeCoeffFloydSteinberg(&coeff, 1.0f / 16.0f, quant_error);
  *px = _applyCoeffErrorFloydSteinberg(*px, coeff);
}

static int _applyPaletteNoDithering(
  HdmvPalletizedBitmap * dst,
  const HdmvBitmap src,
  const HdmvPalette * pal
)
{
  LIBBLU_HDMV_PIC_DEBUG("  Using no dithering.\n");

  assert(dst->height == src.height);
  assert(dst->width == src.width);

  uint16_t stride = src.width;
  for (uint16_t j = 0; j < src.height; j++) {
    for (uint16_t i = 0; i < src.width; i++) {
      dst->data[j * stride + i] = _findNearestColorPalette(
        pal, src.rgba[j * stride + i], NULL
      );
    }
  }

  return 0;
}

static int _applyPaletteFloydSteinberg(
  HdmvPalletizedBitmap * dst,
  const HdmvBitmap src,
  const HdmvPalette * pal
)
{
  LIBBLU_HDMV_PIC_DEBUG("  Using Floyd-Steinberg dithering.\n");

  assert(dst->height == src.height);
  assert(dst->width  == src.width);

  HdmvBitmap src_copy;
  if (dupHdmvBitmap(&src_copy, &src) < 0)
    return -1;

  uint16_t stride = src_copy.width;
  for (uint16_t j = 0; j < src_copy.height-1; j++) {
    for (uint16_t i = 1; i < src_copy.width-1; i++) {
      IntRgba quant_err;
      dst->data[j * stride + i] = _findNearestColorPalette(
        pal, src_copy.rgba[j * stride + i], &quant_err
      );

      _applyFloydSteinberg(src_copy.rgba, i, j, stride, quant_err);
    }
  }

  for (uint16_t j = 0; j < src_copy.height; j++) {
    dst->data[j * stride] = _findNearestColorPalette(
      pal, src_copy.rgba[j * stride], NULL
    );
    dst->data[(j + 1) * stride - 1] = _findNearestColorPalette(
      pal, src_copy.rgba[(j + 1) * stride - 1], NULL
    );
  }

  for (uint16_t i = 0; i < src_copy.width; i++) {
    dst->data[stride * (src_copy.height-1) + i] = _findNearestColorPalette(
      pal, src_copy.rgba[stride * (src_copy.height-1) + i], NULL
    );
  }

  cleanHdmvBitmap(src_copy);
  return 0;
}

static int _applyPalette(
  HdmvPalletizedBitmap * dst,
  const HdmvBitmap src,
  const HdmvPalette * pal,
  HdmvColorDitheringMethod dither_method
)
{
  // TODO: Speed-up!

  clock_t start = clock();
  if ((start = clock()) < 0)
    LIBBLU_HDMV_PIC_DEBUG("   Warning: Unable to compute duration.\n");

  int ret;
  switch (dither_method) {
  case HDMV_PIC_CDM_DISABLED:
    ret = _applyPaletteNoDithering(dst, src, pal); break;
  case HDMV_PIC_CDM_FLOYD_STEINBERG:
    ret = _applyPaletteFloydSteinberg(dst, src, pal);
  }
  if (ret < 0)
    return -1;

  clock_t duration = clock() - start;
  LIBBLU_HDMV_PIC_DEBUG(
    "  Palette content filling duration: %ld ticks (%.2fs, %ld ticks/s).\n",
    duration, (float) duration / CLOCKS_PER_SEC, (clock_t) CLOCKS_PER_SEC
  );

  return 0;
}

int getPalletizedHdmvBitmap(
  HdmvPalletizedBitmap * result,
  const HdmvBitmap src_bitmap,
  const HdmvPalette * palette,
  HdmvColorDitheringMethod dither_method
)
{
  assert(src_bitmap.width && src_bitmap.height);
  assert(src_bitmap.rgba);

  HdmvPalletizedBitmap pal_bm = {
    .width  = src_bitmap.width,
    .height = src_bitmap.height
  };

  pal_bm.data = (uint8_t *) malloc(pal_bm.width * pal_bm.height);
  if (NULL == pal_bm.data)
    LIBBLU_HDMV_PAL_ERROR_RETURN("Memory allocation error.\n");

  if (_applyPalette(&pal_bm, src_bitmap, palette, dither_method) < 0) {
    cleanHdmvPaletizedBitmap(pal_bm);
    return -1;
  }

  *result = pal_bm;
  return 0;
}
