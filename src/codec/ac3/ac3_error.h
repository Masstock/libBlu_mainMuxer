/** \~english
 * \file ac3_error.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Dolby audio bitstreams error handling macros module.
 */

#ifndef __LIBBLU_MUXER__CODECS__AC3__ERROR_H__
#define __LIBBLU_MUXER__CODECS__AC3__ERROR_H__

#include "../../util/errorCodes.h"

#define LIBBLU_AC3_KEYWORD  "AC3"

#define LIBBLU_AC3_NAME  LIBBLU_AC3_KEYWORD
#define LIBBLU_AC3_PREFIX  LIBBLU_AC3_NAME ": "

#define LIBBLU_AC3_ERROR(format, ...)                                         \
  LIBBLU_ERROR(LIBBLU_AC3_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_AC3_ERROR_RETURN(format, ...)                                  \
  LIBBLU_ERROR_RETURN(LIBBLU_AC3_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_AC3_ERROR_NRETURN(format, ...)                                 \
  LIBBLU_ERROR_NRETURN(LIBBLU_AC3_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_AC3_ERROR_FRETURN(format, ...)                                 \
  LIBBLU_ERROR_FRETURN(LIBBLU_AC3_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_AC3_ERROR_BRETURN(format, ...)                                 \
  LIBBLU_ERROR_BRETURN(LIBBLU_AC3_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_AC3_WARNING(format, ...)                                       \
  LIBBLU_WARNING(LIBBLU_AC3_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_AC3_DEBUG(format, ...)                                         \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_AC3_PARSING,                                                 \
    LIBBLU_AC3_NAME,                                                          \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

#define LIBBLU_AC3_DEBUG_NH(format, ...)                                      \
  LIBBLU_DEBUG_NO_HEADER(                                                     \
    LIBBLU_DEBUG_AC3_PARSING,                                                 \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

#endif