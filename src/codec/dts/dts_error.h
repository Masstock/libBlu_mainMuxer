/** \~english
 * \file dts_error.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief DTS audio bitstreams error handling macros module.
 */

#ifndef __LIBBLU_MUXER__CODECS__DTS__ERROR_H__
#define __LIBBLU_MUXER__CODECS__DTS__ERROR_H__

#include "../../util.h"

#define LIBBLU_DTS_PREFIX "DTS: "

#define LIBBLU_DTS_INFO(format, ...)                                          \
  LIBBLU_INFO(LIBBLU_DTS_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_DTS_ERROR_RETURN(format, ...)                                  \
  LIBBLU_ERROR_RETURN(LIBBLU_DTS_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_DTS_ERROR_NRETURN(format, ...)                                 \
  LIBBLU_ERROR_NRETURN(LIBBLU_DTS_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_DTS_ERROR_FRETURN(format, ...)                                 \
  LIBBLU_ERROR_FRETURN(LIBBLU_DTS_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_DTS_COMPLIANCE_ERROR_RETURN(format, ...)                       \
  LIBBLU_ERROR_RETURN("DTS Compliance issue: " format, ##__VA_ARGS__)

#define LIBBLU_DTS_XLL_PREFIX "DTS XLL: "

#define LIBBLU_DTS_XLL_ERROR_RETURN(format, ...)                              \
  LIBBLU_ERROR_RETURN(LIBBLU_DTS_XLL_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_DTS_XLL_ERROR_NRETURN(format, ...)                             \
  LIBBLU_ERROR_NRETURN(LIBBLU_DTS_XLL_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_DTS_XLL_ERROR_FRETURN(format, ...)                             \
  LIBBLU_ERROR_FRETURN(LIBBLU_DTS_XLL_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_DTS_XLL_COMPLIANCE_ERROR_RETURN(format, ...)                   \
  LIBBLU_ERROR_RETURN("DTS XLL Compliance issue: " format, ##__VA_ARGS__)

#define LIBBLU_DTS_PBR_PREFIX "DTS PBR: "

#define LIBBLU_DTS_PBR_ERROR_RETURN(format, ...)                              \
  LIBBLU_ERROR_RETURN(LIBBLU_DTS_PBR_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_DTS_PBR_ERROR_NRETURN(format, ...)                             \
  LIBBLU_ERROR_NRETURN(LIBBLU_DTS_PBR_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_DTS_PBR_ERROR_FRETURN(format, ...)                             \
  LIBBLU_ERROR_FRETURN(LIBBLU_DTS_PBR_PREFIX format, ##__VA_ARGS__)

/* ### Debugging ########################################################### */

#define LIBBLU_DTS_DEBUG(format, ...)                                         \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_DTS_OPERATIONS, "DTS", format, ##__VA_ARGS__                 \
  )

#define LIBBLU_DTS_DEBUG_DTSHD(format, ...)                                   \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_DTS_PARSING_DTSHD, "DTS/DTSHD File", format, ##__VA_ARGS__   \
  )

#define LIBBLU_DTS_DEBUG_DTSHD_NH(format, ...)                                \
  LIBBLU_DEBUG_NO_HEADER(                                                     \
    LIBBLU_DEBUG_DTS_PARSING_DTSHD, format, ##__VA_ARGS__                     \
  )

#define LIBBLU_DTS_DEBUG_PBRFILE(format, ...)                                 \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_DTS_PARSING_PBRFILE, "DTS/PBR FILE", format, ##__VA_ARGS__   \
  )

#define LIBBLU_DTS_DEBUG_CORE(format, ...)                                    \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_DTS_PARSING_CORE, "DTS/Core", format, ##__VA_ARGS__          \
  )

#define LIBBLU_DTS_DEBUG_CORE_NH(format, ...)                                 \
  LIBBLU_DEBUG_NO_HEADER(                                                     \
    LIBBLU_DEBUG_DTS_PARSING_CORE, format, ##__VA_ARGS__                      \
  )

#define LIBBLU_DTS_DEBUG_EXTSS(format, ...)                                   \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_DTS_PARSING_EXTSS, "DTS/ExtSS", format, ##__VA_ARGS__        \
  )

#define LIBBLU_DTS_DEBUG_EXTSS_NH(format, ...)                                \
  LIBBLU_DEBUG_NO_HEADER(                                                     \
    LIBBLU_DEBUG_DTS_PARSING_EXTSS, format, ##__VA_ARGS__                     \
  )

#define LIBBLU_DTS_DEBUG_XLL(format, ...)                                     \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_DTS_PARSING_XLL, "DTS/XLL", format, ##__VA_ARGS__            \
  )

#define LIBBLU_DTS_DEBUG_PATCHER(format, ...)                                 \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_DTS_PATCHER_OPERATIONS, "DTS Patch", format, ##__VA_ARGS__   \
  )

#define LIBBLU_DTS_DEBUG_PATCHER_WRITE(format, ...)                           \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_DTS_PATCHER_WRITING,                                         \
    "DTS Patch writer", format, ##__VA_ARGS__                                 \
  )

#define LIBBLU_DTS_DEBUG_PATCHER_WRITE_NH(format, ...)                        \
  LIBBLU_DEBUG_NO_HEADER(                                                     \
    LIBBLU_DEBUG_DTS_PATCHER_WRITING, format, ##__VA_ARGS__                   \
  )

#define LIBBLU_DTS_DEBUG_PBR(format, ...)                                     \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_DTS_PBR, "DTS/XLL", format, ##__VA_ARGS__                    \
  )

#endif