

#ifndef __LIBBLU_MUXER__T_STD_VERIFIER__ERROR_H__
#define __LIBBLU_MUXER__T_STD_VERIFIER__ERROR_H__

#include "../util.h"

#define LIBBLU_T_STD_VERIF_NAME  "T-STD Buffer Verifier"
#define LIBBLU_T_STD_VERIF_PREFIX LIBBLU_T_STD_VERIF_NAME ": "

#define LIBBLU_T_STD_VERIF_TEST_DEBUG(format, ...)                            \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_T_STD_VERIF_TEST,                                            \
    LIBBLU_T_STD_VERIF_NAME,                                                  \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

#define LIBBLU_T_STD_VERIF_DECL_DEBUG(format, ...)                            \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_T_STD_VERIF_DECLARATIONS,                                    \
    LIBBLU_T_STD_VERIF_NAME,                                                  \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

#define LIBBLU_T_STD_VERIF_DEBUG(format, ...)                                 \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_T_STD_VERIF_OPERATIONS,                                      \
    LIBBLU_T_STD_VERIF_NAME,                                                  \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

#endif