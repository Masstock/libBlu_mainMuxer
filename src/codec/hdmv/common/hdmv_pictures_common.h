

#ifndef __LIBBLU_MUXER__CODECS__HDMV__COMMON__COMMON_H__
#define __LIBBLU_MUXER__CODECS__HDMV__COMMON__COMMON_H__

#include "hdmv_error.h"
#include "hdmv_palette_def.h"
#include "hdmv_color.h"

/* ### HDMV Picture infos : ################################################ */

typedef enum {
  HDMV_PIC_CDM_DISABLED,

  HDMV_PIC_CDM_FLOYD_STEINBERG
} HdmvPictureColorDitheringMethod;

typedef struct {
  uint8_t version;

  unsigned width;
  unsigned height;

  HdmvPaletteDefinitionPtr linkedPal;
  HdmvPictureColorDitheringMethod ditherMeth;

  size_t imgAllocatedSize;
  size_t rleAllocatedSize;
  size_t rleUsedSize;
} HdmvPictureInfos;

/* ### HDMV Picture : ###################################################### */

typedef struct {
  HdmvPictureInfos infos;

  uint32_t * rgba;
  bool updatedRgba;

  uint8_t * pal;
  bool updatedPal;

  uint8_t * rle;
  bool updatedRle;
} HdmvPicture, *HdmvPicturePtr;

#define HDMV_PIC_MIN_WIDTH  8
#define HDMV_PIC_MAX_WIDTH  2048
#define HDMV_PIC_MIN_HEIGHT  8
#define HDMV_PIC_MAX_HEIGHT  2048

/* ###### Creation / Destruction : ######################################### */

/** \~english
 * \brief Create a new empty picture.
 *
 * \param version Picture version.
 * \param width Picture width in pixels, shall be equal to 0 or between
 * #HDMV_PIC_MIN_WIDTH and #HDMV_PIC_MAX_WIDTH inclusive.
 * \param height Picture height in pixels, shall be equal to 0 or between
 * #HDMV_PIC_MIN_HEIGHT and #HDMV_PIC_MAX_HEIGHT inclusive.
 * \return HdmvPicturePtr Upon success, created object is returned. Otherwise,
 * a NULL pointer is returned.
 *
 * \note If width or height parameter is equal to zero, the picture size is
 * set to zero.
 */
HdmvPicturePtr createHdmvPicture(
  uint8_t version,
  unsigned width,
  unsigned height
);

/** \~english
 * \brief Duplicate a picture.
 *
 * \param pic
 * \return HdmvPicturePtr
 *
 * \warning Duplicated picture must be filled before.
 */
HdmvPicturePtr dupHdmvPicture(
  const HdmvPicturePtr pic
);

static inline void destroyHdmvPicture(
  HdmvPicturePtr pic
)
{
  if (NULL == pic)
    return;

  free(pic->rgba);
  free(pic->pal);
  free(pic->rle);
  free(pic);
}

/* ###### Accessors : ###################################################### */

static inline void getVersionHdmvPicture(
  const HdmvPicturePtr pic,
  uint8_t * version
)
{
  if (NULL != version)
    *version = pic->infos.version;
}

static inline void getDimensionsHdmvPicture(
  const HdmvPicturePtr pic,
  unsigned * width,
  unsigned * height
)
{
  if (NULL != width)
    *width = pic->infos.width;
  if (NULL != height)
    *height = pic->infos.height;
}

/* ######### RGBA data : ################################################### */

static inline size_t getRgbaSizeHdmvPicture(
  const HdmvPicturePtr pic
)
{
  unsigned width, height;

  getDimensionsHdmvPicture(pic, &width, &height);
  return width * height;
}

const uint32_t * getRgbaHdmvPicture(
  HdmvPicturePtr pic
);

/** \~english
 * \brief Return a write handle to the picture RGBA data.
 *
 * \param pic
 * \return uint32_t*
 *
 * \warning Using this function after edition of Palette or RLE data may erase
 * modifications and must not be used just for accessing data (use rather
 * #getRgbaHdmvPicture()). If this kind of manipulations is required,
 * #getRgbaHdmvPicture() shall be called prior.
 */
uint32_t * getRgbaHandleHdmvPicture(
  HdmvPicturePtr pic
);

/* ######### Palette data : ################################################ */

static inline size_t getPalSizeHdmvPicture(
  const HdmvPicturePtr pic
)
{
  return getRgbaSizeHdmvPicture(pic);
}

const uint8_t * getPalHdmvPicture(
  HdmvPicturePtr pic
);

uint8_t * getPalHandleHdmvPicture(
  HdmvPicturePtr pic
);

/* ######### RLE data : #################################################### */

size_t getRleSizeHdmvPicture(
  const HdmvPicturePtr pic
);

const uint8_t * getRleHdmvPicture(
  HdmvPicturePtr pic
);

uint8_t * getRleHandleHdmvPicture(
  HdmvPicturePtr pic
);

/* ###### Operations : ##################################################### */

/** \~english
 * \brief Set the Palette Hdmv Picture object
 *
 * \param pic Destination picture.
 * \param pal Applied palette definition.
 * \param ditherMeth Dithering method applyed.
 * \return int Upon success, a zero is returned. Otherwise, a negative value
 * is returned.
 */
int setPaletteHdmvPicture(
  HdmvPicturePtr pic,
  HdmvPaletteDefinitionPtr pal,
  HdmvPictureColorDitheringMethod ditherMeth
);

int cropHdmvPicture(
  HdmvPicturePtr pic,
  unsigned left,
  unsigned top,
  unsigned width,
  unsigned height
);

int setSizeHdmvPicture(
  HdmvPicturePtr pic,
  unsigned width,
  unsigned height
);

#endif