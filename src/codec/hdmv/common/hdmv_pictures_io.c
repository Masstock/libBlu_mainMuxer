#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "hdmv_pictures_io.h"

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
    msg " '%" PRI_LBCS "', %s (errno: %d).\n",                                \
    filepath,                                                                 \
    strerror(errno),                                                          \
    errno                                                                     \
  )

HdmvPicturePtr openHdmvPicture(
  HdmvPictureLibraries * libs,
  const lbc * filepath,
  const IniFileContextPtr conf
)
{
  HdmvPicturePtr pic;
  FILE * file;

  assert(NULL != filepath);

  if (NULL == (file = lbc_fopen(filepath, "rb")))
    ERROR_FRETURN("Unable to open picture");

  /* Read file signature (first n bytes) */
  uint8_t signature[HDMV_PIC_SIGNATURE_SIZE];
  if (!RA_FILE(file, signature, HDMV_PIC_SIGNATURE_SIZE))
    ERROR_FRETURN("Unable to read picture");

  /* Use the signature to identify used format */
  switch (_identifyFormatHdmvPictureLibraries(signature)) {
    case HDMV_PIC_FORMAT_PNG:
      if (!isLoadedHdmvLibpngHandle(&libs->libpng)) {
        const lbc * libFilepath = lookupIniFile(conf, "LIBRARIES.LIBPNG");
        if (loadHdmvLibpngHandle(&libs->libpng, libFilepath) < 0)
          goto free_return;
      }
      pic = openPngHdmvPicture(&libs->libpng, filepath, file);
      break;

    default:
      ERROR_FRETURN("Unhandled picture format");
  }

  fclose(file);
  return pic;

free_return:
  fclose(file);
  return NULL;
}

#undef ERROR_FRETURN