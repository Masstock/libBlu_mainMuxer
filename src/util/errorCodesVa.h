

#ifndef __LIBBLU_MUXER__UTIL__ERROR_CODES_VA_H__
#define __LIBBLU_MUXER__UTIL__ERROR_CODES_VA_H__

#include "errorCodes.h"

#define LIBBLU_ECHO_VA  echoMessageVa

#define LIBBLU_ERROR_VA(format, ap)                                           \
  LIBBLU_ECHO_VA(LIBBLU_FATAL_ERROR, format, ap)

#define LIBBLU_DEBUG_VA(status, name, format, ap)                             \
  LIBBLU_ECHO_VA(status, "[DEBUG/" name "] ", format, ap)

void echoMessageFdVa(
  FILE *fd,
  LibbluStatus status,
  const lbc *format,
  va_list args
);

void echoMessageVa(
  const LibbluStatus status,
  const lbc *format,
  va_list args
);

#endif