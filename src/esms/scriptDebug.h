

#ifndef __LIBBLU_MUXER__ESMS__SCRIPT_DEBUG_H__
#define __LIBBLU_MUXER__ESMS__SCRIPT_DEBUG_H__

#include "../util/errorCodes.h"

#define LIBBLU_SCRIPT_DEBUG(format, ...)                                      \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_SCRIPTS,                                                     \
    "ESMS",                                                                   \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

#endif