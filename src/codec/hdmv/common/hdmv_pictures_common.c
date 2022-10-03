#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <assert.h>

#include "hdmv_pictures_common.h"

/* ### HDMV Picture infos : ################################################ */

static bool checkPictureDimensions(
  unsigned width,
  unsigned height
)
{
  if (!width || !height)
    return true;
  if (width < HDMV_PIC_MIN_WIDTH || HDMV_PIC_MAX_WIDTH < width)
    LIBBLU_HDMV_PIC_ERROR_BRETURN("Invalid width %u.\n", width);
  if (height < HDMV_PIC_MIN_HEIGHT || HDMV_PIC_MAX_HEIGHT < height)
    LIBBLU_HDMV_PIC_ERROR_BRETURN("Invalid height %u.\n", height);
  return true;
}

/* ### HDMV Picture : ###################################################### */

static int allocateRgba(
  uint32_t ** dst,
  size_t size
)
{
  if (!size) {
    *dst = NULL;
    return 0;
  }

  if (NULL == (*dst = (uint32_t *) realloc(*dst, size * sizeof(uint32_t))))
    LIBBLU_HDMV_PIC_ERROR_RETURN("Memory allocation error.\n");
  return 0;
}

static int allocatePal(
  uint8_t ** dst,
  size_t size
)
{
  if (!size) {
    *dst = NULL;
    return 0;
  }

  if (NULL == (*dst = (uint8_t *) realloc(*dst, size)))
    LIBBLU_HDMV_PIC_ERROR_RETURN("Memory allocation error.\n");
  return 0;
}

static size_t maxRleSize(
  unsigned width,
  unsigned height
)
{
  return (width + 1) * 2 * height;
}

static int allocateRle(
  uint8_t ** dst,
  size_t size
)
{
  if (!size) {
    *dst = NULL;
    return 0;
  }

  if (NULL == (*dst = (uint8_t *) malloc(size)))
    LIBBLU_HDMV_PIC_ERROR_RETURN("Memory allocation error.\n");
  return 0;
}

/* ###### Creation / Destruction : ######################################### */

HdmvPicturePtr createHdmvPicture(
  uint8_t version,
  unsigned width,
  unsigned height
)
{
  HdmvPicturePtr pic;
  size_t picSize, rleSize;

  if (!checkPictureDimensions(width, height))
    return NULL;
  rleSize = maxRleSize(width, height);
  picSize = width * height;

  pic = (HdmvPicturePtr) malloc(sizeof(HdmvPicture));
  if (NULL == pic)
    LIBBLU_HDMV_PIC_ERROR_NRETURN("Memory allocation error.\n");

  *pic = (HdmvPicture) {
    .infos = {
      .version = version,
      .width = width,
      .height = height,
      .imgAllocatedSize = picSize,
      .rleAllocatedSize = rleSize
    }
  };

  if (allocateRgba(&pic->rgba, picSize) < 0)
    goto free_return;
  if (allocatePal(&pic->pal, picSize) < 0)
    goto free_return;
  if (allocateRle(&pic->rle, rleSize) < 0)
    goto free_return;

  return pic;

free_return:
  destroyHdmvPicture(pic);
  return NULL;
}

HdmvPicturePtr dupHdmvPicture(
  const HdmvPicturePtr pic
)
{
  HdmvPicturePtr dst;
  uint8_t version;
  unsigned width, height;

  size_t size;
  const uint32_t * rgba;
  const uint8_t * palRle;

  /* Create a new picture */
  getVersionHdmvPicture(pic, &version);
  getDimensionsHdmvPicture(pic, &width, &height);
  if (NULL == (dst = createHdmvPicture(version, width, height)))
    return NULL;

  /* Duplicate RGBA data */
  if (NULL == (rgba = getRgbaHdmvPicture(pic)))
    goto free_return;
  if (0 < (size = getRgbaSizeHdmvPicture(pic)))
    memcpy(pic->rgba, rgba, size * sizeof(uint32_t));
  dst->updatedRgba = true;

  /* Duplicate palette data */
  if (NULL == (palRle = getPalHdmvPicture(pic)))
    goto free_return;
  if (0 < (size = getPalSizeHdmvPicture(pic)))
    memcpy(pic->pal, palRle, size);
  dst->updatedPal = true;

  /* Duplicate RLE data */
  if (NULL == (palRle = getRleHdmvPicture(pic)))
    goto free_return;
  if (0 < (size = getRleSizeHdmvPicture(pic)))
    memcpy(pic->rle, palRle, size);
  dst->updatedRle = true;

  return dst;

free_return:
  destroyHdmvPicture(dst);
  return NULL;
}

/* ###### Accessors : ###################################################### */

/* Static definition : */
static int updatePalHdmvPicture(
  HdmvPicturePtr pic,
  bool fromRgba
);

/* ######### RGBA data : ################################################### */

static int updateRgbaHdmvPicture(
  HdmvPicturePtr pic
)
{
  if (pic->updatedRgba)
    return 0;

  if (updatePalHdmvPicture(pic, false) < 0)
    return -1;

  /* Render RGBA from palette */
  LIBBLU_ERROR_RETURN("NOT IMPLEMENTED %s %d\n", __FILE__, __LINE__);
}

const uint32_t * getRgbaHdmvPicture(
  HdmvPicturePtr pic
)
{
  if (updateRgbaHdmvPicture(pic) < 0)
    return NULL;
  return pic->rgba;
}

uint32_t * getRgbaHandleHdmvPicture(
  HdmvPicturePtr pic
)
{
  pic->updatedRgba = true;
  pic->updatedPal = false;
  pic->updatedRle = false;

  return pic->rgba;
}

/* ######### Palette data : ################################################ */

/* Static definitions : */
static int updatePalFromRleHdmvPicture(
  HdmvPicturePtr pic
);
static int updatePalFromRgbaHdmvPicture(
  HdmvPicturePtr pic
);

static int updatePalHdmvPicture(
  HdmvPicturePtr pic,
  bool fromRgba
)
{
  if (pic->updatedPal)
    return 0;

  LIBBLU_HDMV_PIC_DEBUG("Updating Palette data.\n");

  if (!fromRgba) {
    /* Update the palette by decompressing the RLE data */
    LIBBLU_HDMV_PIC_DEBUG(" Performing RLE data decompression.\n");
    if (!pic->updatedRle)
      LIBBLU_HDMV_PIC_ERROR_RETURN("Unable to render picture, missing RLE.\n");
    return updatePalFromRleHdmvPicture(pic);
  }

  /* Update the palette by sampling the RGBA data */
  LIBBLU_HDMV_PIC_DEBUG(" Performing color sampling from RGBA data.\n");
  if (!pic->updatedRgba)
    LIBBLU_HDMV_PIC_ERROR_RETURN("Unable to render picture, missing RGBA.\n");
  return updatePalFromRgbaHdmvPicture(pic);
}

const uint8_t * getPalHdmvPicture(
  HdmvPicturePtr pic
)
{
  if (pic->updatedRgba) {
    /* Update from RGBA data */
    if (updatePalHdmvPicture(pic, true) < 0)
      return NULL;
  }
  else {
    /* Update from RLE data */
    if (updatePalHdmvPicture(pic, false) < 0)
      return NULL;
  }

  return pic->pal;
}

uint8_t * getPalHandleHdmvPicture(
  HdmvPicturePtr pic
)
{
  pic->updatedRgba = false;
  pic->updatedPal = true;
  pic->updatedRle = false;

  return pic->pal;
}

/* ############ RLE decompression : ######################################## */

static int updatePalFromRleHdmvPicture(
  HdmvPicturePtr pic
)
{
  (void) pic;

  LIBBLU_ERROR_RETURN("NOT IMPLEMENTED %s %d\n", __FILE__, __LINE__);
}

/* ############ Color picking : ############################################ */

#define GET_MAX_DIST  false

static unsigned findNearestColorHdmvPaletteDefinition(
  const HdmvPaletteDefinitionPtr pal,
  uint32_t rgba,
  IntRgba * quantError
)
{
  uint32_t pal_rgba;
  int s_r, s_g, s_b, s_a;
  size_t i;

#if GET_MAX_DIST
  int maxDist_rgba = 0;
#endif
  int minDist_rgba = INT_MAX;
  unsigned selectedIdx = 0;

  assert(NULL != pal);
  assert(0 < pal->nbUsedEntries);

  s_r = (int) GET_CHANNEL(rgba, C_R);
  s_g = (int) GET_CHANNEL(rgba, C_G);
  s_b = (int) GET_CHANNEL(rgba, C_B);
  s_a = (int) GET_CHANNEL(rgba, C_A);

  for (i = 0; i < pal->nbUsedEntries; i++) {
    int dist_r, dist_g, dist_b, dist_a, dist_rgba;
    int p_r, p_g, p_b, p_a;

    pal_rgba = pal->entries[i].rgba;
    p_r = (int) GET_CHANNEL(pal_rgba, C_R);
    p_g = (int) GET_CHANNEL(pal_rgba, C_G);
    p_b = (int) GET_CHANNEL(pal_rgba, C_B);
    p_a = (int) GET_CHANNEL(pal_rgba, C_A);

    dist_r = abs(s_r - p_r);
    dist_g = abs(s_g - p_g);
    dist_b = abs(s_b - p_b);
    dist_a = abs(s_a - p_a);

    dist_rgba =
      (dist_r * dist_r)
      + (dist_g * dist_g)
      + (dist_b * dist_b)
      + (dist_a * dist_a)
    ;

#if GET_MAX_DIST
    if (maxDist_rgba < dist_rgba)
      maxDist_rgba = dist_rgba;
#endif

    if (dist_rgba < minDist_rgba) {
      minDist_rgba = dist_rgba;
      selectedIdx = i;

      if (pal_rgba == rgba)
        break; /* Exact color found */
    }
  }

  /* LIBBLU_IGS_COMPL_QUANT_DEBUG("Min color error distance: %d.\n", minDist_rgba); */
#if GET_MAX_DIST
  /* LIBBLU_IGS_COMPL_QUANT_DEBUG("Max color error distance: %d.\n", maxDist_rgba); */
#endif

  pal_rgba = pal->entries[selectedIdx].rgba;
  if (NULL != quantError) {
    *quantError = (IntRgba) {
      .r = s_r - (int) GET_CHANNEL(pal_rgba, C_R),
      .g = s_g - (int) GET_CHANNEL(pal_rgba, C_G),
      .b = s_b - (int) GET_CHANNEL(pal_rgba, C_B),
      .a = s_a - (int) GET_CHANNEL(pal_rgba, C_A)
    };
  }

  return selectedIdx;
}

static void computeCoeffFloydSteinberg(
  IntRgba * res,
  float div,
  IntRgba quantError
)
{
  res->r = (int) nearbyintf(div * quantError.r);
  res->g = (int) nearbyintf(div * quantError.g);
  res->b = (int) nearbyintf(div * quantError.b);
  res->a = (int) nearbyintf(div * quantError.a);
}

static uint32_t applyCoeffErrorFloydSteinberg(
  uint32_t rgba,
  IntRgba coeffs
)
{
  uint32_t ret;

  ret  = CHANNEL_VALUE(CLIP_UINT8((int) GET_CHANNEL(rgba, C_R) + coeffs.r), C_R);
  ret |= CHANNEL_VALUE(CLIP_UINT8((int) GET_CHANNEL(rgba, C_G) + coeffs.g), C_G);
  ret |= CHANNEL_VALUE(CLIP_UINT8((int) GET_CHANNEL(rgba, C_B) + coeffs.b), C_B);
  ret |= CHANNEL_VALUE(CLIP_UINT8((int) GET_CHANNEL(rgba, C_A) + coeffs.a), C_A);

  return ret;
}

void applyFloydSteinberg(
  uint32_t * rgba,
  unsigned x,
  unsigned y,
  unsigned width,
  IntRgba quantError
)
{
  uint32_t * px;
  IntRgba coeff;

  /* (x+1, y) 7/16 */
  px = &rgba[y*width + x+1];
  computeCoeffFloydSteinberg(&coeff, 7.0f / 16.0f, quantError);
  *px = applyCoeffErrorFloydSteinberg(*px, coeff);

  /* (x-1, y+1) 3/16 */
  px = &rgba[(y+1)*width + x-1];
  computeCoeffFloydSteinberg(&coeff, 3.0f / 16.0f, quantError);
  *px = applyCoeffErrorFloydSteinberg(*px, coeff);

  /* (x, y+1) 5/16 */
  px = &rgba[(y+1)*width + x];
  computeCoeffFloydSteinberg(&coeff, 5.0f / 16.0f, quantError);
  *px = applyCoeffErrorFloydSteinberg(*px, coeff);

  /* (x+1, y+1) 1/16 */
  px = &rgba[(y+1)*width + x+1];
  computeCoeffFloydSteinberg(&coeff, 1.0f / 16.0f, quantError);
  *px = applyCoeffErrorFloydSteinberg(*px, coeff);
}

static void updatePalFromRgbaNoDitheringHdmvPicture(
  HdmvPicturePtr pic
)
{
  unsigned i, j;

  LIBBLU_HDMV_PIC_DEBUG("  Using no dithering.\n");
  for (j = 0; j < pic->infos.height; j++) {
    for (i = 0; i < pic->infos.width; i++) {
      pic->pal[j * pic->infos.width + i] =
        findNearestColorHdmvPaletteDefinition(
          pic->infos.linkedPal,
          pic->rgba[j * pic->infos.width + i],
          NULL
        )
      ;
    }
  }
}

static void updatePalFromRgbaFloydSteinbergHdmvPicture(
  HdmvPicturePtr pic
)
{
  unsigned i, j;

  LIBBLU_HDMV_PIC_DEBUG("  Using Floyd-Steinberg dithering.\n");

  for (j = 0; j < pic->infos.height-1; j++) {
    for (i = 1; i < pic->infos.width-1; i++) {
      IntRgba quantError;

      pic->pal[j * pic->infos.width + i] =
        findNearestColorHdmvPaletteDefinition(
          pic->infos.linkedPal,
          pic->rgba[j * pic->infos.width + i],
          &quantError
        )
      ;

      applyFloydSteinberg(
        pic->rgba, i, j,
        pic->infos.width,
        quantError
      );
    }
  }

  for (j = 0; j < pic->infos.height; j++) {
    pic->pal[j * pic->infos.width] =
      findNearestColorHdmvPaletteDefinition(
        pic->infos.linkedPal,
        pic->rgba[j * pic->infos.width],
        NULL
      )
    ;
    pic->pal[(j + 1) * pic->infos.width - 1] =
      findNearestColorHdmvPaletteDefinition(
        pic->infos.linkedPal,
        pic->rgba[(j + 1) * pic->infos.width - 1],
        NULL
      )
    ;
  }

  for (i = 0; i < pic->infos.width; i++) {
    pic->pal[pic->infos.width * (pic->infos.height-1) + i] =
      findNearestColorHdmvPaletteDefinition(
        pic->infos.linkedPal,
        pic->rgba[pic->infos.width * (pic->infos.height-1) + i],
        NULL
      )
    ;
  }
}

static int updatePalFromRgbaHdmvPicture(
  HdmvPicturePtr pic
)
{
  clock_t start, duration;

  /* TODO: Speed-up ! */

  if ((start = clock()) < 0)
    LIBBLU_HDMV_PIC_DEBUG("   Warning: Unable to compute duration.\n");

  switch (pic->infos.ditherMeth) {
    case HDMV_PIC_CDM_DISABLED:
      updatePalFromRgbaNoDitheringHdmvPicture(pic);
      break;

    case HDMV_PIC_CDM_FLOYD_STEINBERG:
      updatePalFromRgbaFloydSteinbergHdmvPicture(pic);
  }

  duration = clock() - start;
  LIBBLU_HDMV_PIC_DEBUG(
    "  Palette content filling duration: %ld ticks (%.2fs, %ld ticks/s).\n",
    duration, (float) duration / CLOCKS_PER_SEC, (clock_t) CLOCKS_PER_SEC
  );

  return 0;
}

/* ######### RLE data : #################################################### */

static int performRleCompressionHdmvPicture(
  HdmvPicturePtr pic
);

static int updateRleHdmvPicture(
  HdmvPicturePtr pic
)
{
  if (pic->updatedRle)
    return 0;

  LIBBLU_HDMV_PIC_DEBUG("Updating RLE data.\n");

  /* Update if required palette */
  if (updatePalHdmvPicture(pic, true) < 0)
    return -1;

  /* Perform RLE compression */
  LIBBLU_HDMV_PIC_DEBUG(" Performing RLE compression.\n");
  return performRleCompressionHdmvPicture(pic);
}

size_t getRleSizeHdmvPicture(
  const HdmvPicturePtr pic
)
{
  if (updateRleHdmvPicture(pic) < 0)
    return 0;
  return pic->infos.rleUsedSize;
}

const uint8_t * getRleHdmvPicture(
  HdmvPicturePtr pic
)
{
  if (updateRleHdmvPicture(pic) < 0)
    return NULL;
  return pic->rle;
}

uint8_t * getRleHandleHdmvPicture(
  HdmvPicturePtr pic
)
{
  pic->updatedRgba = false;
  pic->updatedPal = false;
  pic->updatedRle = true;

  return pic->rle;
}

/* ############ RLE compression : ########################################## */

static int performRleCompressionHdmvPicture(
  HdmvPicturePtr pic
)
{
  uint8_t * dst, * src;
  size_t height;

  src = pic->pal;
  dst = pic->rle;

  for (height = pic->infos.height; 0 < height; height--) {
    uint8_t runningPx, * nextLine;
    size_t i, runningLen;

    nextLine = &src[pic->infos.width];
    runningLen = 0;

    while (true) {
      if (!runningLen) {
        /* Initialize a new running */
        runningPx = *(src++);
        runningLen++;
      }

      if (
        src == nextLine /* End of line */
        || *src != runningPx  /* End of running */
        || runningLen == 16383 /* Running is longer than max L */
      ) {
        /* End of running */
        if (runningPx == 0) {
          /* Color 0 */
          if (runningLen <= 63) {
            /* 0b00000000 0b00LLLLLL */
            *(dst++) = 0x00;
            *(dst++) = runningLen & 0x3F;
          }
          else {
            /* 0b00000000 0b01LLLLLL 0bLLLLLLLL */
            *(dst++) = 0x00;
            *(dst++) = 0x40 | ((runningLen >> 8) & 0x3F);
            *(dst++) = runningLen & 0xFF;
          }
        }
        else {
          if (runningLen <= 3) {
            /* 0bCCCCCCCC */
            for (i = 0; i < runningLen; i++)
              *(dst++) = runningPx;
          }
          else if (runningLen <= 63) {
            /* 0b00000000 0b10LLLLLL 0bCCCCCCCC */
            *(dst++) = 0x00;
            *(dst++) = 0x80 | (runningLen & 0x3F);
            *(dst++) = runningPx;
          }
          else {
            /* 0b00000000 0b11LLLLLL 0bLLLLLLLL 0bCCCCCCCC */
            *(dst++) = 0x00;
            *(dst++) = 0xC0 | ((runningLen >> 8) & 0x3F);
            *(dst++) = runningLen & 0xFF;
            *(dst++) = runningPx;
          }
        }

        runningLen = 0;

        if (src == nextLine)
          break; /* End of line */
        continue;
      }

      /* Continue running */
      src++;
      runningLen++;
    }

    /* End of line */
    /* 0b00000000 0b00000000 */
    *(dst++) = 0x00;
    *(dst++) = 0x00;
  }

  pic->infos.rleUsedSize = dst - pic->rle;
  pic->updatedRle = true;

  return 0;
}

/* ###### Operations : ##################################################### */

int setPaletteHdmvPicture(
  HdmvPicturePtr pic,
  HdmvPaletteDefinitionPtr pal,
  HdmvPictureColorDitheringMethod ditherMeth
)
{
  if (pic->infos.linkedPal != pal || pic->infos.ditherMeth != ditherMeth)
    pic->updatedPal = false;

  pic->infos.linkedPal = pal;
  pic->infos.ditherMeth = ditherMeth;
  return 0;
}

int cropHdmvPicture(
  HdmvPicturePtr pic,
  unsigned left,
  unsigned top,
  unsigned width,
  unsigned height
)
{
  unsigned picWidth, picHeight;
  unsigned dstWidth, dstHeight;
  unsigned i, j;

  assert(NULL != pic);

  /* Update picture content prior to cropping */
  if (updateRgbaHdmvPicture(pic) < 0)
    return -1;

  /* Compute destination picture dimensions */
  getDimensionsHdmvPicture(pic, &picWidth, &picHeight);
  dstWidth  = (0 <  width) ? width  : picWidth - left;
  dstHeight = (0 < height) ? height : picHeight - top;

  if (!left && !top && dstWidth == picWidth && dstHeight == picHeight)
    return 0; /* Nothing to do */

  if (
    picWidth < left + dstWidth
    || picHeight < top + dstHeight
  ) {
    LIBBLU_HDMV_PIC_ERROR_RETURN(
      "Image cropping values are out of dimensions (%ux%u)\n",
      picWidth, picHeight
    );
  }

  /* NOTE: No need to reallocate since pic size can only decrease */

  LIBBLU_HDMV_PIC_DEBUG("Applying windowing to picture:\n");

  /* Copy each pixel to destination */
  for (j = 0; j < dstHeight; j++) {
    for (i = 0; i < dstWidth; i++) {
      pic->rgba[j * dstWidth + i] =
        pic->rgba[(j + top) * picWidth + (i + left)]
      ;
    }
  }

  LIBBLU_HDMV_PIC_DEBUG(" Width      = %u px.\n", dstWidth);
  LIBBLU_HDMV_PIC_DEBUG(" Height     = %u px.\n", dstHeight);

  return 0;
}

int setSizeHdmvPicture(
  HdmvPicturePtr pic,
  unsigned width,
  unsigned height
)
{
  size_t picSize, rleSize;

  if (!checkPictureDimensions(width, height))
    return -1;
  rleSize = maxRleSize(width, height);
  picSize = width * height;

  if (pic->infos.imgAllocatedSize < picSize) {
    if (allocateRgba(&pic->rgba, picSize) < 0)
      return -1;
    if (allocatePal(&pic->pal, picSize) < 0)
      return -1;
    pic->infos.imgAllocatedSize = picSize;
  }

  if (pic->infos.rleAllocatedSize < rleSize) {
    if (allocateRle(&pic->rle, rleSize) < 0)
      return -1;
    pic->infos.rleAllocatedSize = rleSize;
  }

  return 0;
}