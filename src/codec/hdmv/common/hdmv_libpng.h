

#ifndef __LIBBLU_MUXER__CODECS__HDMV__COMMON__LIBPNG_H__
#define __LIBBLU_MUXER__CODECS__HDMV__COMMON__LIBPNG_H__

#include "../../../util.h"

#include "hdmv_error.h"
#include "hdmv_bitmap.h"

#include <png.h>
#if !defined(png_jmpbuf)
#  define png_jmpbuf(png_ptr)  ((png_ptr)->png_jmpbuf)
#endif

/* ### Library handle : #################################################### */

typedef struct {
  LibbluLibraryHandlePtr lib;

  png_infop (*png_create_info_struct) (
    png_const_structrp png_ptr
  );
  png_structp (*png_create_read_struct) (
    png_const_charp user_png_ver,
    png_voidp error_ptr,
    png_error_ptr error_fn,
    png_error_ptr warn_fn
  );
  void (*png_destroy_read_struct) (
    png_structpp png_ptr_ptr,
    png_infopp info_ptr_ptr,
    png_infopp end_info_ptr_ptr
  );
  void (*png_init_io) (
    png_structp png_ptr,
    png_FILE_p fp
  );
  void (*png_set_sig_bytes) (
    png_structp png_ptr,
    int num_bytes
  );
  void (*png_read_info) (
    png_structp png_ptr,
    png_infop info_ptr
  );
  png_uint_32 (*png_get_IHDR) (
    png_structp png_ptr,
    png_infop info_ptr,
    png_uint_32 * width,
    png_uint_32 * height,
    int * bit_depth,
    int * color_type,
    int * interlace_method,
    int * compression_method,
    int * filter_method
  );
  png_uint_32 (*png_get_valid) (
    png_structp png_ptr,
    png_infop info_ptr,
    png_uint_32 flag
  );
  void (*png_set_palette_to_rgb) (
    png_structrp png_ptr
  );
  void (*png_set_expand_gray_1_2_4_to_8) (
    png_structrp png_ptr
  );
  void (*png_set_tRNS_to_alpha) (
    png_structrp png_ptr
  );
  void (*png_set_filler) (
    png_structp png_ptr,
    png_uint_32 filler,
    int flags
  );
  void (*png_set_gray_to_rgb) (
    png_structp png_ptr
  );
  void (*png_set_scale_16) (
    png_structrp png_ptr
  );
  void (*png_set_packing) (
    png_structp png_ptr
  );
  void (*png_read_update_info) (
    png_structp png_ptr,
    png_infop info_ptr
  );
  void (*png_read_image) (
    png_structp png_ptr,
    png_bytepp image
  );
  void (*png_read_end) (
    png_structp png_ptr,
    png_infop info_ptr
  );
  jmp_buf * (*png_set_longjmp_fn) (
    png_structp,
    png_longjmp_ptr,
    size_t
  );
} HdmvLibpngHandle;

static inline void initHdmvLibpngHandle(
  HdmvLibpngHandle * dst
)
{
  *dst = (HdmvLibpngHandle) {
    0
  };
}

static inline void cleanHdmvLibpngHandle(
  HdmvLibpngHandle handle
)
{
  closeLibbluLibrary(handle.lib);
}

static inline bool isLoadedHdmvLibpngHandle(
  const HdmvLibpngHandle * handle
)
{
  return NULL != handle->lib;
}

#if defined(ARCH_LINUX)
#  define HDMV_DEFAULT_LIBPNG_PATH  "libpng16.so"
#else
#  define HDMV_DEFAULT_LIBPNG_PATH  "libpng16-16"
#endif

/** \~english
 * \brief Load the libpng dynamic library.
 *
 * \param handle Destination handle.
 * \param path Optionnal library path. If NULL, #HDMV_DEFAULT_LIBPNG_PATH is
 * used.
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned.
 *
 * If the library is already loaded, nothing is done.
 */
int loadHdmvLibpngHandle(
  HdmvLibpngHandle * handle,
  const lbc * path
);

/* ### PNG Picture handling : ############################################## */

#define HDMV_LIBPNG_SIG_SIZE  8
#define HDMV_LIBPNG_SIG  {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A}

/* ###### PNG Picture properties : ######################################### */

typedef struct {
  png_uint_32 width;
  png_uint_32 height;
  int bit_depth;
  int color_type;
  int interlace_method;
  int compression_method;
  int filter_method;
} HdmvLibpngPictureProp;

static inline const char * color_typeHdmvLibpngStr(
  int color_type
)
{
  switch (color_type) {
    case PNG_COLOR_TYPE_GRAY:
      return "Shades of gray";
    case PNG_COLOR_TYPE_GRAY_ALPHA:
      return "Shades of gray with transparency";
    case PNG_COLOR_TYPE_PALETTE:
      return "Indexed colors";
    case PNG_COLOR_TYPE_RGB:
      return "RGB";
    case PNG_COLOR_TYPE_RGB_ALPHA:
      return "RGB with transparency";
  }

  return "Unknown";
}

static inline const char * interlace_methodHdmvLibpngStr(
  int interlace_method
)
{
  switch (interlace_method) {
    case PNG_INTERLACE_NONE:
      return "Not interlaced (progressive)";
    case PNG_INTERLACE_ADAM7:
      return "Adam7 interlacing";
    case PNG_INTERLACE_LAST:
      return "Invalid";
  }

  return "Unknown";
}

static inline const char * compression_methodHdmvLibpngStr(
  int compression_method
)
{
  switch (compression_method) {
    case PNG_COMPRESSION_TYPE_BASE:
      return "Method 0 Inflate/Deflate (default)";
  }

  return "Unknown";
}

static inline const char * filter_methodHdmvLibpngStr(
  int filter_method
)
{
  switch (filter_method) {
    case PNG_FILTER_TYPE_BASE:
      return "Method 0 (default)";
    case PNG_INTRAPIXEL_DIFFERENCING:
      return "Method 0 w/ Intra differencing (MNG standard)";
  }

  return "Unknown";
}

/* ### Reading handle : #################################################### */

typedef struct {
  png_structp reader;
  png_infop info;
  HdmvLibpngPictureProp prop;
} HdmvLibpngPictureInfos;

int initHdmvLibpngPictureInfos(
  const HdmvLibpngHandle * libpng,
  HdmvLibpngPictureInfos * dst
);

static inline void cleanHdmvLibpngPictureInfos(
  const HdmvLibpngHandle * libpng,
  HdmvLibpngPictureInfos pngInfos
)
{
  libpng->png_destroy_read_struct(&pngInfos.reader, &pngInfos.info, NULL);
}

/* ### Picture IO : ######################################################## */

int openPngHdmvBitmap(
  HdmvBitmap * dst,
  const HdmvLibpngHandle * libpng,
  const lbc * filepath,
  FILE * file
);

#endif