/** \~english
 * \file h262_error.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief H.262 video (MPEG-2 Part 2) bitstreams error handling macros module.
 */

#ifndef __LIBBLU_MUXER__CODECS__H262__ERROR_H__
#define __LIBBLU_MUXER__CODECS__H262__ERROR_H__

#include "../../util/errorCodes.h"

#define LIBBLU_H262_KEYWORD  "H262"

#define LIBBLU_H262_NAME  LIBBLU_H262_KEYWORD
#define LIBBLU_H262_PREFIX  LIBBLU_H262_NAME ": "

#define LIBBLU_H262_ERROR(format, ...)                                       \
  LIBBLU_ERROR(LIBBLU_H262_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_H262_ERROR_RETURN(format, ...)                                \
  LIBBLU_ERROR_RETURN(LIBBLU_H262_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_H262_ERROR_NRETURN(format, ...)                               \
  LIBBLU_ERROR_NRETURN(LIBBLU_H262_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_H262_ERROR_FRETURN(format, ...)                               \
  LIBBLU_ERROR_FRETURN(LIBBLU_H262_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_H262_ERROR_BRETURN(format, ...)                               \
  LIBBLU_ERROR_BRETURN(LIBBLU_H262_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_H262_WARNING(format, ...)                                     \
  LIBBLU_WARNING(LIBBLU_H262_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_H262_DEBUG(format, ...)                                       \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_H262_PARSING,                                               \
    LIBBLU_H262_NAME,                                                        \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

#define LIBBLU_H262_DEBUG_NH(format, ...)                                    \
  LIBBLU_DEBUG_NO_HEADER(                                                     \
    LIBBLU_DEBUG_H262_PARSING,                                               \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

#endif