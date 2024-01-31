
#ifndef __LIBBLU_MUXER__CODECS__HDMV__COMMON__BITMAP_IO_H__
#define __LIBBLU_MUXER__CODECS__HDMV__COMMON__BITMAP_IO_H__

#include "hdmv_error.h"
#include "hdmv_bitmap.h"
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

int openHdmvBitmap(
  HdmvBitmap * dst,
  HdmvPictureLibraries * libs,
  const lbc * filepath,
  const IniFileContextPtr conf
);

#endif