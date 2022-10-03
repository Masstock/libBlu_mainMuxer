/** \~english
 * \file lpcm_error.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief LPCM audio bitstreams error handling macros module.
 */

#ifndef __LIBBLU_MUXER__CODECS__LCPM__ERROR_H__
#define __LIBBLU_MUXER__CODECS__LCPM__ERROR_H__

#include "../../util.h"

#define LIBBLU_LPCM_KEYWORD  "LPCM"

#define LIBBLU_LPCM_NAME  LIBBLU_LPCM_KEYWORD
#define LIBBLU_LPCM_PREFIX  LIBBLU_LPCM_NAME ": "

#define LIBBLU_LPCM_ERROR(format, ...)                                        \
  LIBBLU_ERROR(LIBBLU_LPCM_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_LPCM_ERROR_NH(format, ...)                                     \
  LIBBLU_ECHO(LIBBLU_FATAL_ERROR, format, ##__VA_ARGS__)

#define LIBBLU_LPCM_ERROR_RETURN(format, ...)                                 \
  LIBBLU_ERROR_RETURN(LIBBLU_LPCM_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_LPCM_ERROR_NRETURN(format, ...)                                \
  LIBBLU_ERROR_NRETURN(LIBBLU_LPCM_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_LPCM_ERROR_FRETURN(format, ...)                                \
  LIBBLU_ERROR_FRETURN(LIBBLU_LPCM_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_LPCM_ERROR_BRETURN(format, ...)                                \
  LIBBLU_ERROR_BRETURN(LIBBLU_LPCM_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_LPCM_WARNING(format, ...)                                      \
  LIBBLU_WARNING(LIBBLU_LPCM_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_LPCM_DEBUG(format, ...)                                        \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_LPCM_PARSING,                                                \
    LIBBLU_LPCM_NAME,                                                         \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

#define LIBBLU_LPCM_DEBUG_NH(format, ...)                                     \
  LIBBLU_DEBUG_NO_HEADER(                                                     \
    LIBBLU_DEBUG_LPCM_PARSING,                                                \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

#endif