
#ifndef __LIBBLU_MUXER__CODECS__HDMV__COMMON__IO_H__
#define __LIBBLU_MUXER__CODECS__HDMV__COMMON__IO_H__

#include "hdmv_error.h"
#include "hdmv_pictures_common.h"
#include "hdmv_libpng.h"
#include "../../../ini/iniData.h"

#if !defined(DISABLE_INI)
#  include "../../../ini/iniHandler.h"
#endif

/* ### HDMV Pictures IO Libraries : ######################################## */

typedef struct {
  HdmvLibpngHandle libpng;
} HdmvPictureLibraries;

static inline void initHdmvPictureLibraries(
  HdmvPictureLibraries * dst
)
{
  initHdmvLibpngHandle(&dst->libpng);
}

static inline void cleanHdmvPictureLibraries(
  HdmvPictureLibraries libs
)
{
  cleanHdmvLibpngHandle(libs.libpng);
}

/* ######################################################################### */

#define HDMV_PIC_SIGNATURE_SIZE  8

typedef enum {
  HDMV_PIC_FORMAT_UNK  = -1,

  HDMV_PIC_FORMAT_PNG
} HdmvPictureFormat;

HdmvPictureFormat identifyFormatHdmvPictureLibraries(
  uint8_t magic[HDMV_PIC_SIGNATURE_SIZE]
);

/* ######################################################################### */

HdmvPicturePtr openHdmvPicture(
  HdmvPictureLibraries * libs,
  const lbc * filepath,
  const IniFileContextPtr conf
);

#endif