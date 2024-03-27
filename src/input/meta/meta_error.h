

#ifndef __LIBBLU_MUXER__INPUT__META__ERROR_H__
#define __LIBBLU_MUXER__INPUT__META__ERROR_H__

#include "../../util.h"

#define LIBBLU_META_KEYWORD  "META"

/* ### META file parser : ################################################## */

#define LIBBLU_META_PARSER_NAME  LIBBLU_META_KEYWORD "/Parser"
#define LIBBLU_META_PARSER_PREFIX  LIBBLU_META_PARSER_NAME ": "

#define LIBBLU_META_PARSER_INFO(format, ...)                                  \
  LIBBLU_INFO(LIBBLU_META_PARSER_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_META_PARSER_ERROR_RETURN(format, ...)                          \
  LIBBLU_ERROR_RETURN(LIBBLU_META_PARSER_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_META_PARSER_ERROR_NRETURN(format, ...)                         \
  LIBBLU_ERROR_NRETURN(LIBBLU_META_PARSER_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_META_PARSER_ERROR_FRETURN(format, ...)                         \
  LIBBLU_ERROR_FRETURN(LIBBLU_META_PARSER_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_META_PARSER_DEBUG(format, ...)                                 \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_META_PARSER,                                                 \
    LIBBLU_META_PARSER_NAME,                                                  \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

/* ### META file analyzer : ################################################ */

#define LIBBLU_META_ANALYZER_NAME  LIBBLU_META_KEYWORD "/Analyzer"
#define LIBBLU_META_ANALYZER_PREFIX  LIBBLU_META_ANALYZER_NAME ": "

#define LIBBLU_META_ANALYZER_INFO(format, ...)                                \
  LIBBLU_INFO(LIBBLU_META_ANALYZER_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_META_ANALYZER_ERROR_RETURN(format, ...)                        \
  LIBBLU_ERROR_RETURN(LIBBLU_META_ANALYZER_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_META_ANALYZER_ERROR_NRETURN(format, ...)                       \
  LIBBLU_ERROR_NRETURN(LIBBLU_META_ANALYZER_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_META_ANALYZER_ERROR_FRETURN(format, ...)                       \
  LIBBLU_ERROR_FRETURN(LIBBLU_META_ANALYZER_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_META_ANALYZER_DEBUG(format, ...)                               \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_META_ANALYZER,                                               \
    LIBBLU_META_ANALYZER_NAME,                                                \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

#endif