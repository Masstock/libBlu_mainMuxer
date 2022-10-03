#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "hdmv_pictures_io.h"

/* ######################################################################### */

HdmvPictureFormat identifyFormatHdmvPictureLibraries(
  uint8_t magic[HDMV_PIC_SIGNATURE_SIZE]
)
{
  size_t i;

  struct {
    uint8_t value[HDMV_PIC_SIGNATURE_SIZE];  /**< Signature value. */
    HdmvPictureFormat fmt;  /**< Associated format. */
    unsigned char size;  /**< Size of the used signature value. */
  } signatures[] = {
#define DECLARE(f, s, v)  {.size = (s), .value = v, .fmt = (f)}
#define V(...) __VA_ARGS__  /* This is declared to protect '{' array char */
    /* ### Supported formats : */

    DECLARE(HDMV_PIC_FORMAT_PNG, HDMV_LIBPNG_SIG_SIZE, HDMV_LIBPNG_SIG)

#undef DECLARE
#undef V
  };

  for (i = 0; i < ARRAY_SIZE(signatures); i++) {
    if (0 == memcmp(signatures[i].value, magic, signatures[i].size))
      return signatures[i].fmt;
  }

  return -1;
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

  uint8_t fileSignature[HDMV_PIC_SIGNATURE_SIZE];
  FILE * file;

  assert(NULL != filepath);

  if (NULL == (file = lbc_fopen(filepath, "rb")))
    ERROR_FRETURN("Unable to open picture");

  /* Read file signature (first n bytes) */
  if (!RA_FILE(file, fileSignature, HDMV_PIC_SIGNATURE_SIZE))
    ERROR_FRETURN("Unable to read picture");

  /* Use the signature to identify used format */
  switch (identifyFormatHdmvPictureLibraries(fileSignature)) {
    case HDMV_PIC_FORMAT_PNG: /* PNG */
      if (!isLoadedHdmvLibpngHandle(&libs->libpng)) {
        /* Load the libpng library if required */
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