

#ifndef __LIBBLU_MUXER_CODECS__LCPM__UTIL_H__
#define __LIBBLU_MUXER_CODECS__LCPM__UTIL_H__

#include "lpcm_error.h"

#include "../../esms/scriptCreation.h"
#include "../../pesPackets.h"
#include "../../util.h"
#include "../common/esParsingSettings.h"

typedef struct {


  unsigned nb_frames;
  uint64_t pts;
} LpcmParsingContext;

#endif