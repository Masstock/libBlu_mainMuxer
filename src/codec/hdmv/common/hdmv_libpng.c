#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "hdmv_libpng.h"

int loadHdmvLibpngHandle(
  HdmvLibpngHandle * handle,
  const lbc * path
)
{
  LibbluLibraryHandlePtr lib;

  assert(!isLoadedHdmvLibpngHandle(handle));

  if (NULL == path || *path == '\0')
    path = lbc_str(HDMV_DEFAULT_LIBPNG_PATH);

  if (NULL == (lib = loadLibbluLibrary(path)))
    LIBBLU_HDMV_LIBPNG_ERROR_RETURN("Unable to load the libpng.\n");
  handle->lib = lib;

#define RESOLVE_SYM(h, n)  \
  RESOLVE_SYM_LB_LIB(h, STR(n), handle->n,  \
    LIBBLU_HDMV_LIBPNG_ERROR_RETURN("Unable to load " STR(n) " symbol from the libpng.\n"))
  RESOLVE_SYM(lib, png_create_info_struct);
  RESOLVE_SYM(lib, png_create_read_struct);
  RESOLVE_SYM(lib, png_destroy_read_struct);
  RESOLVE_SYM(lib, png_init_io);
  RESOLVE_SYM(lib, png_set_sig_bytes);
  RESOLVE_SYM(lib, png_read_info);
  RESOLVE_SYM(lib, png_get_IHDR);
  RESOLVE_SYM(lib, png_get_valid);
  RESOLVE_SYM(lib, png_set_palette_to_rgb);
  RESOLVE_SYM(lib, png_set_expand_gray_1_2_4_to_8);
  RESOLVE_SYM(lib, png_set_tRNS_to_alpha);
  RESOLVE_SYM(lib, png_set_filler);
  RESOLVE_SYM(lib, png_set_gray_to_rgb);
  RESOLVE_SYM(lib, png_set_scale_16);
  RESOLVE_SYM(lib, png_set_packing);
  RESOLVE_SYM(lib, png_read_update_info);
  RESOLVE_SYM(lib, png_read_image);
  RESOLVE_SYM(lib, png_read_end);
  RESOLVE_SYM(lib, png_set_longjmp_fn);
#undef RESOLVE_SYM

  return 0;
}

/* ### Picture handling : ################################################## */

int initHdmvLibpngPictureInfos(
  const HdmvLibpngHandle * libpng,
  HdmvLibpngPictureInfos * dst
)
{
  HdmvLibpngPictureInfos infos = {0};

  infos.reader = libpng->png_create_read_struct(
    PNG_LIBPNG_VER_STRING,
    NULL, NULL, NULL
  );
  if (NULL == infos.reader)
    goto free_return;

  if (NULL == (infos.info = libpng->png_create_info_struct(infos.reader)))
    goto free_return;

  *dst = infos;
  return 0;

free_return:
  cleanHdmvLibpngPictureInfos(libpng, infos);
  return -1;
}

/* ### Picture IO : ######################################################## */

#define ERROR_RETURN(msg)                                                     \
  LIBBLU_HDMV_LIBPNG_ERROR_RETURN(                                            \
    msg " '%" PRI_LBCS "'.\n",                                                \
    filepath,                                                                 \
    strerror(errno),                                                          \
    errno                                                                     \
  )

#define ERROR_NRETURN(msg)                                                    \
  LIBBLU_HDMV_LIBPNG_ERROR_NRETURN(                                           \
    msg " '%" PRI_LBCS "', %s (errno: %d).\n",                                \
    filepath,                                                                 \
    strerror(errno),                                                          \
    errno                                                                     \
  )

static int _readIHDRPngHdmvPicture(
  const HdmvLibpngHandle * libpng,
  HdmvLibpngPictureInfos * png
)
{
  HdmvLibpngPictureProp * prop = &png->prop;
  png_uint_32 ret;

  ret = libpng->png_get_IHDR(
    png->reader,
    png->info,
    &prop->width,
    &prop->height,
    &prop->bit_depth,
    &prop->color_type,
    &prop->interlace_method,
    &prop->compression_method,
    &prop->filter_method
  );

  return (ret) ? 0 : -1;
}

static int _readInfosPngHdmvPicture(
  const HdmvLibpngHandle * libpng,
  HdmvLibpngPictureInfos * png
)
{
  /* libpng informations parsing. */
  libpng->png_read_info(png->reader, png->info);

  /* Parse the IHDR chunk */
  if (_readIHDRPngHdmvPicture(libpng, png) < 0)
    return -1;

#define P LIBBLU_HDMV_LIBPNG_DEBUG
  P("Picture informations:\n");
  P(" Width       : %u px.\n", (unsigned) png->prop.width);
  P(" Height      : %u px.\n", (unsigned) png->prop.height);
  P(" Bit depth   : %d bits.\n", png->prop.bit_depth);
  P(" Color mode  : %s.\n", color_typeHdmvLibpngStr(png->prop.color_type));
  P(" Interlacing : %s.\n", interlace_methodHdmvLibpngStr(png->prop.interlace_method));
  P(" Compr. Met. : %s.\n", compression_methodHdmvLibpngStr(png->prop.compression_method));
  P(" Filter Met. : %s.\n", filter_methodHdmvLibpngStr(png->prop.filter_method));
#undef P

  return 0;
}

static void _setReadingSettingsPngHdmvPicture(
  const HdmvLibpngHandle * libpng,
  HdmvLibpngPictureInfos * png
)
{
  if (png->prop.color_type == PNG_COLOR_TYPE_PALETTE)
    libpng->png_set_palette_to_rgb(png->reader);

  if (png->prop.bit_depth < 8 && png->prop.color_type == PNG_COLOR_TYPE_GRAY)
    libpng->png_set_expand_gray_1_2_4_to_8(png->reader);

  if (libpng->png_get_valid(png->reader, png->info, PNG_INFO_tRNS))
    libpng->png_set_tRNS_to_alpha(png->reader);

  if(
    png->prop.color_type == PNG_COLOR_TYPE_RGB
    || png->prop.color_type == PNG_COLOR_TYPE_GRAY
    || png->prop.color_type == PNG_COLOR_TYPE_PALETTE
  )
    libpng->png_set_filler(png->reader, 0xFF, PNG_FILLER_AFTER);

  if (
    png->prop.color_type == PNG_COLOR_TYPE_GRAY
    || png->prop.color_type == PNG_COLOR_TYPE_GRAY_ALPHA
  )
    libpng->png_set_gray_to_rgb(png->reader);

  if (png->prop.bit_depth == 16)
    libpng->png_set_scale_16(png->reader);
  else if (png->prop.bit_depth < 8)
    libpng->png_set_packing(png->reader);

  libpng->png_read_update_info(png->reader, png->info); /* Apply */
}

static int _initReadingPngHdmvPicture(
  const HdmvLibpngHandle * libpng,
  HdmvLibpngPictureInfos * png,
  const lbc * filepath,
  FILE * file
)
{
  /* Init libpng I/O to the file handle. */
  libpng->png_init_io(png->reader, file);

  /* Set the number of bytes already readed (the signature size). */
  libpng->png_set_sig_bytes(png->reader, HDMV_LIBPNG_SIG_SIZE);

  /* Read and store informations */
  LIBBLU_HDMV_LIBPNG_DEBUG("Reading input file informations.\n");
  if (_readInfosPngHdmvPicture(libpng, png) < 0)
    ERROR_RETURN("Unable to parse image informations");

  _setReadingSettingsPngHdmvPicture(libpng, png);

  return 0;
}

static int _readRowsPngHdmvPicture(
  const HdmvLibpngHandle * libpng,
  HdmvLibpngPictureInfos png,
  HdmvPicturePtr pic
)
{
  unsigned width, height;
  png_byte ** rows;

  getDimensionsHdmvPicture(pic, &width, &height);
  uint32_t * rgba = getRgbaHandleHdmvPicture(pic);

  if (NULL == (rows = (png_byte **) malloc(height * sizeof(png_byte *))))
    LIBBLU_HDMV_LIBPNG_ERROR_RETURN("Memory allocation error.\n");
  for (unsigned i = 0; i < height; i++)
    rows[i] = (png_byte *) &rgba[i * width];

  libpng->png_read_image(png.reader, rows);
  free(rows);

  return 0;
}

static HdmvPicturePtr _readPngHdmvPicture(
  const HdmvLibpngHandle * libpng,
  HdmvLibpngPictureInfos png
)
{
  HdmvPicturePtr pic;


  unsigned width = png.prop.width;
  unsigned height = png.prop.height;

  if (NULL == (pic = createHdmvPicture(0x0, width, height)))
    return NULL;
  if (_readRowsPngHdmvPicture(libpng, png, pic) < 0)
    goto free_return;

  LIBBLU_HDMV_LIBPNG_DEBUG("Completing file reading.\n");
  libpng->png_read_end(png.reader, NULL);

  return pic;

free_return:
  destroyHdmvPicture(pic);
  return NULL;
}

HdmvPicturePtr openPngHdmvPicture(
  const HdmvLibpngHandle * libpng,
  const lbc * filepath,
  FILE * file
)
{
  HdmvLibpngPictureInfos png;
  HdmvPicturePtr pic;

  if (!isLoadedHdmvLibpngHandle(libpng))
    ERROR_NRETURN("Unable to open file, libpng not loaded");

  if (initHdmvLibpngPictureInfos(libpng, &png) < 0)
    return NULL;

  /* Set the exit point of libpng on error */
  if (setjmp(*libpng->png_set_longjmp_fn(png.reader, longjmp, sizeof(jmp_buf))))
    goto free_return; /* On error, goto free_return. */

  /* Initialize PNG file reading and decoding */
  if (_initReadingPngHdmvPicture(libpng, &png, filepath, file) < 0)
    goto free_return;

  /* Read picture data (in case of error, a NULL pointer is returned) */
  pic = _readPngHdmvPicture(libpng, png);

  cleanHdmvLibpngPictureInfos(libpng, png);
  return pic;

free_return:
  cleanHdmvLibpngPictureInfos(libpng, png);
  return NULL;
}

#undef ERROR_NRETURN