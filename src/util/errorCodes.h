/** \~english
 * \file errorCodes.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Errors feedback module.
 *
 * Macros naming radical convention:
 *  - DEBUG : Debugging message, printed on stderr if enabled.
 *  - INFO : Informational message, printed on stdout.
 *  - WARNING : Warning message, printed on stderr.
 *  - ERROR : Fatal error message, printed on stderr.
 * Suffix convention:
 *  - NO_HEADER or NH : Without message category prefix between square
 *    brackets.
 *  - EXIT : Terminate program immediately after printing.
 *  - RETURN : Print the message and use instruction 'return -1'.
 *  - BRETURN : Print the message and use instruction 'return false'.
 *  - ZRETURN : Print the message and use instruction 'return 0'.
 *  - NRETURN : Print the message and use instruction 'return NULL'.
 *  - GRETURN : Print the message and use instruction 'goto <label>' with
 *    supplied label.
 *  - FRETURN : Print the message and use instruction 'goto free_return'.
 *  - VRETURN : Print the message and use instruction 'return'.
 */

#ifndef __LIBBLU_MUXER__UTIL__ERROR_CODES_H__
#define __LIBBLU_MUXER__UTIL__ERROR_CODES_H__

#include "macros.h"

#define LIBBLU_ECHO(status, format, ...)                                      \
  echoMessage(status, lbc_str(format), ##__VA_ARGS__)

#define LIBBLU_ERROR(format, ...)                                             \
  LIBBLU_ECHO(LIBBLU_FATAL_ERROR, "[ERROR] " format, ##__VA_ARGS__)

#define LIBBLU_WARNING(format, ...)                                           \
  LIBBLU_ECHO(LIBBLU_WARNING, "[WARNING] " format, ##__VA_ARGS__)

#define LIBBLU_INFO(format, ...)                                              \
  LIBBLU_ECHO(LIBBLU_INFO, "[INFO] " format, ##__VA_ARGS__)

#define LIBBLU_DEBUG(status, name, format, ...)                               \
  LIBBLU_ECHO(status, "[DEBUG/" name "] " format, ##__VA_ARGS__)

#define LIBBLU_DEBUG_NO_HEADER(status, format, ...)                           \
  LIBBLU_ECHO(status, format, ##__VA_ARGS__)

#define LIBBLU_DEBUG_COM(format, ...)                                         \
  LIBBLU_ECHO(LIBBLU_DEBUG_GLB, "[DEBUG] " format, ##__VA_ARGS__)

#define LIBBLU_DEBUG_COM_NO_HEADER(format, ...)                               \
  LIBBLU_ECHO(LIBBLU_DEBUG_GLB, format, ##__VA_ARGS__)

#define LIBBLU_TODO()                                                         \
  do {                                                                        \
    LIBBLU_ECHO(                                                              \
      LIBBLU_FATAL_ERROR, "[TODO] %s(), line %d, %s.\n",                      \
      __func__, __LINE__, __FILE__                                            \
    );                                                                        \
    exit(EXIT_FAILURE);                                                       \
  } while(0)

#define __LIBBLU_ERROR_INSTR_(instr, format, ...)                             \
  do {                                                                        \
    LIBBLU_ERROR(format, ##__VA_ARGS__);                                      \
    instr;                                                                    \
  } while(0)

#define LIBBLU_ERROR_EXIT(format, ...)                                        \
  __LIBBLU_ERROR_INSTR_(exit(EXIT_FAILURE), format, ##__VA_ARGS__)

#define LIBBLU_ERROR_RETURN(format, ...)                                      \
  __LIBBLU_ERROR_INSTR_(return -1, format, ##__VA_ARGS__)

#define LIBBLU_ERROR_ZRETURN(format, ...)                                     \
  __LIBBLU_ERROR_INSTR_(return 0, format, ##__VA_ARGS__)

#define LIBBLU_ERROR_NRETURN(format, ...)                                     \
  __LIBBLU_ERROR_INSTR_(return NULL, format, ##__VA_ARGS__)

#define LIBBLU_ERROR_GRETURN(label, format, ...)                              \
  __LIBBLU_ERROR_INSTR_(goto label, format, ##__VA_ARGS__)

#define LIBBLU_ERROR_VRETURN(format, ...)                                     \
  __LIBBLU_ERROR_INSTR_(return, format, ##__VA_ARGS__)

typedef enum {
  LIBBLU_EXPLODE_COMPLIANCE,
  LIBBLU_EXPLODE_BD_COMPLIANCE,
  LIBBLU_EXPLODE_STD_COMPLIANCE,
  LIBBLU_EXPLODE_BDAV_STD_COMPLIANCE
} LibbluExplodeLevel;

int isDisabledExplodeLevel(
  LibbluExplodeLevel level
);

#define __LIBBLU_FAIL_INSTR_(instr, level, format, ...)                       \
  do {                                                                        \
    if (!isDisabledExplodeLevel(level)) {                                     \
      LIBBLU_ERROR(format, ##__VA_ARGS__);                                    \
      instr;                                                                  \
    }                                                                         \
    LIBBLU_WARNING(format, ##__VA_ARGS__);                                    \
  } while (0)

#define LIBBLU_FAIL_RETURN(level, format, ...)                                \
  __LIBBLU_FAIL_INSTR_(return -1, level, format, ##__VA_ARGS__)

/** \~english
 * \brief Error printing and goto 'free_return' label macro.
 *
 * \param format Error message formating string.
 * \param ... Error message formating parameters.
 */
#define LIBBLU_ERROR_FRETURN(format, ...)                                     \
  LIBBLU_ERROR_GRETURN(free_return, format, ##__VA_ARGS__);

#define LIBBLU_ERROR_BRETURN(format, ...)                                     \
  __LIBBLU_ERROR_INSTR_(return false, format, ##__VA_ARGS__);

typedef enum {
  LIBBLU_INFO,
  LIBBLU_WARNING,

  LIBBLU_FATAL_ERROR,

  LIBBLU_DEBUG_GLB,

  LIBBLU_DEBUG_SCRIPTS,

  LIBBLU_DEBUG_MUXER_DECISION,
  LIBBLU_DEBUG_PES_BUILDING,

  /* Muxer T-STD Buffer Verifier : */
  LIBBLU_DEBUG_T_STD_VERIF_TEST,
  LIBBLU_DEBUG_T_STD_VERIF_DECLARATIONS,
  LIBBLU_DEBUG_T_STD_VERIF_OPERATIONS,

  /* LPCM Audio : */
  LIBBLU_DEBUG_LPCM_PARSING,

  /* AC3 Audio : */
  LIBBLU_DEBUG_AC3_PARSING,

  /* DTS Audio : */
  LIBBLU_DEBUG_DTS_PARSING_DTSHD,
  LIBBLU_DEBUG_DTS_PARSING_PBRFILE,
  LIBBLU_DEBUG_DTS_PARSING_CORE,
  LIBBLU_DEBUG_DTS_PARSING_EXTSS,
  LIBBLU_DEBUG_DTS_PARSING_XLL,
  LIBBLU_DEBUG_DTS_PBR,
  LIBBLU_DEBUG_DTS_OPERATIONS,

  /* H.262 Video : */
  LIBBLU_DEBUG_H262_PARSING,

  /* H.264 Video : */
  LIBBLU_DEBUG_H264_PARSING_NAL,
  LIBBLU_DEBUG_H264_PARSING_AUD,
  LIBBLU_DEBUG_H264_PARSING_SPS,
  LIBBLU_DEBUG_H264_PARSING_VUI,
  LIBBLU_DEBUG_H264_PARSING_PPS,
  LIBBLU_DEBUG_H264_PARSING_SEI,
  LIBBLU_DEBUG_H264_PARSING_SLICE,
  LIBBLU_DEBUG_H264_OPERATIONS,
  LIBBLU_DEBUG_H264_AU_PROCESSING,

  LIBBLU_DEBUG_H264_HRD,
  LIBBLU_DEBUG_H264_HRD_CPB,
  LIBBLU_DEBUG_H264_HRD_DPB,

  /* HDMV Common : */
  LIBBLU_DEBUG_HDMV_COMMON,
  LIBBLU_DEBUG_HDMV_PARSER,
  LIBBLU_DEBUG_HDMV_CHECKS,
  LIBBLU_DEBUG_HDMV_TS_COMPUTE,
  LIBBLU_DEBUG_HDMV_SEG_BUILDER,
  LIBBLU_DEBUG_HDMV_QUANTIZER,
  LIBBLU_DEBUG_HDMV_PAL,
  LIBBLU_DEBUG_HDMV_PICTURES,
  LIBBLU_DEBUG_HDMV_LIBPNG,
  LIBBLU_DEBUG_HDMV_TC,

  /* HDMV IGS Parser : */
  LIBBLU_DEBUG_IGS_PARSER,

  /* HDMV PGS Parser : */
  LIBBLU_DEBUG_PGS_PARSER,

  /* HDMV IGS Compiler : */
  LIBBLU_DEBUG_IGS_COMPL_XML_OPERATIONS,
  LIBBLU_DEBUG_IGS_COMPL_XML_PARSING,
  LIBBLU_DEBUG_IGS_COMPL_OPERATIONS,

  LIBBLU_NB_STATUS
} LibbluStatus;

int enableDebugStatusString(
  const char * string
);

bool isDebugEnabledLibbbluStatus(
  void
);

bool isEnabledLibbbluStatus(
  LibbluStatus status
);

void echoMessageFd(
  FILE * fd,
  LibbluStatus status,
  const lbc * format,
  ...
);

void echoMessage(
  const LibbluStatus status,
  const lbc * format,
  ...
);

void printListLibbbluStatus(
  unsigned indent
);

void printListWithDescLibbbluStatus(
  unsigned indent
);

#endif