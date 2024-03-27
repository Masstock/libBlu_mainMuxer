#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "hdmv_bitmap_io.h"

/* ######################################################################### */

#define HDMV_PIC_SIGNATURE_SIZE  8

typedef enum {
  HDMV_PIC_FORMAT_UNK  = -1,
  HDMV_PIC_FORMAT_PNG
} HdmvPictureFormat;

static HdmvPictureFormat _identifyFormatHdmvPictureLibraries(
  uint8_t magic[static HDMV_PIC_SIGNATURE_SIZE]
)
{
  size_t i;

  static const struct {
    uint8_t value[HDMV_PIC_SIGNATURE_SIZE];  /**< Signature value. */
    unsigned size;                           /**< Size of the used signature value. */
    HdmvPictureFormat format;                /**< Associated format. */
  } signatures[] = {
    {HDMV_LIBPNG_SIG, HDMV_LIBPNG_SIG_SIZE, HDMV_PIC_FORMAT_PNG} // PNG
  };

  for (i = 0; i < ARRAY_SIZE(signatures); i++) {
    if (0 == memcmp(signatures[i].value, magic, signatures[i].size))
      return signatures[i].format;
  }

  return HDMV_PIC_FORMAT_UNK;
}

/* ######################################################################### */

#define ERROR_FRETURN(msg)                                                    \
  LIBBLU_HDMV_PIC_ERROR_FRETURN(                                              \
    msg " '%s', %s (errno: %d).\n",                                \
    filepath,                                                                 \
    strerror(errno),                                                          \
    errno                                                                     \
  )

int openHdmvBitmap(
  HdmvBitmap *dst,
  HdmvPictureLibraries *libs,
  const lbc *filepath,
  const IniFileContext conf_hdl
)
{
  assert(NULL != filepath);

  FILE *fd = lbc_fopen(filepath, "rb");
  if (NULL == fd)
    ERROR_FRETURN("Unable to open picture");

  /* Read file signature (first n bytes) */
  uint8_t signature[HDMV_PIC_SIGNATURE_SIZE];
  if (!RA_FILE(fd, signature, HDMV_PIC_SIGNATURE_SIZE))
    ERROR_FRETURN("Unable to read picture");

  /* Use the signature to identify used format */
  switch (_identifyFormatHdmvPictureLibraries(signature)) {
  case HDMV_PIC_FORMAT_PNG:
    if (!isLoadedHdmvLibpngHandle(&libs->libpng)) {
      const lbc *libFilepath = lookupIniFile(conf_hdl, "LIBRARIES.LIBPNG");
      if (loadHdmvLibpngHandle(&libs->libpng, libFilepath) < 0)
        goto free_return;
    }
    if (openPngHdmvBitmap(dst, &libs->libpng, filepath, fd) < 0)
      goto free_return;
    break;

  default:
    ERROR_FRETURN("Unhandled picture format");
  }

  fclose(fd);
  return 0;

free_return:
  fclose(fd);
  return -1;
}

#undef ERROR_FRETURN