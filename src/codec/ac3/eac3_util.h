

#ifndef __LIBBLU_MUXER__CODECS__AC3__EAC3_UTIL_H__
#define __LIBBLU_MUXER__CODECS__AC3__EAC3_UTIL_H__

#include "ac3_error.h"
#include "ac3_data.h"

typedef struct {
  Eac3BitStreamInfoParameters bsi;
  Eac3Informations info;

  unsigned nb_frames;
  uint64_t pts;
} Eac3ExtensionContext;

#endif