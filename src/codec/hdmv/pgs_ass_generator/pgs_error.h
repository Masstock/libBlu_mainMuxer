
#ifndef __LIBBLU_MUXER__CODECS__PGS_GENERATOR__ERROR_H__
#define __LIBBLU_MUXER__CODECS__PGS_GENERATOR__ERROR_H__

#include "../../../util.h"

#include "../common/hdmv_error.h"

#define LIBBLU_HDMV_PGS_ASS_NAME  LIBBLU_HDMV_KEYWORD "/PGS ASS Generator"
#define LIBBLU_HDMV_PGS_ASS_PREFIX  LIBBLU_HDMV_PGS_ASS_NAME ": "

#define LIBBLU_HDMV_PGS_ASS_ERROR(format, ...)                                \
  LIBBLU_ERROR(LIBBLU_HDMV_PGS_ASS_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_PGS_ASS_ERROR_RETURN(format, ...)                         \
  LIBBLU_ERROR_RETURN(LIBBLU_HDMV_PGS_ASS_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_PGS_ASS_ERROR_NRETURN(format, ...)                        \
  LIBBLU_ERROR_NRETURN(LIBBLU_HDMV_PGS_ASS_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_PGS_ASS_DEBUG(format, ...)                                \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_PGS_ASS_OPERATIONS,                                          \
    LIBBLU_HDMV_PGS_ASS_NAME,                                                 \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

#define LIBBLU_HDMV_PGS_ASS_TSC_NAME  LIBBLU_HDMV_PGS_ASS_NAME " TS compute"

#define LIBBLU_HDMV_PGS_ASS_TSC_DEBUG(format, ...)                            \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_PGS_ASS_TS_COMPUTE,                                          \
    LIBBLU_HDMV_PGS_ASS_TSC_NAME,                                             \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

#endif