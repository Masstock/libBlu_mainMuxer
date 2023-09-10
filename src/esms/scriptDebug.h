

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

#define LIBBLU_SCRIPTWO_DEBUG(format, ...)                                    \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_SCRIPTS_WRITING_OPERATIONS,                                  \
    "ESMS Writing util",                                                      \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

#define LIBBLU_SCRIPTW_DEBUG(format, ...)                                     \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_SCRIPTS_WRITING,                                             \
    "ESMS Writing",                                                           \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

#define LIBBLU_SCRIPTRO_DEBUG(format, ...)                                    \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_SCRIPTS_READING,                                             \
    "ESMS",                                                                   \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

#define LIBBLU_SCRIPTR_DEBUG(format, ...)                                     \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_SCRIPTS_READING,                                             \
    "ESMS",                                                                   \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

#endif