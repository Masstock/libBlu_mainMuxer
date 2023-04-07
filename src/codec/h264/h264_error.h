/** \~english
 * \file h264_error.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief H.264 video (AVC) bitstreams error handling macros module.
 */

#ifndef __LIBBLU_MUXER__CODECS__H264__ERROR_H__
#define __LIBBLU_MUXER__CODECS__H264__ERROR_H__

#include "../../util/errorCodes.h"

#define LIBBLU_H264_PREFIX "H.264: "
#define LIBBLU_H264_HRDV_PREFIX "H.264 HRD Verifier: "

/* ### H.264 Common : ###################################################### */

#define LIBBLU_H264_INFO(format, ...)                                         \
  LIBBLU_INFO(LIBBLU_H264_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_H264_ERROR_RETURN(format, ...)                                 \
  LIBBLU_ERROR_RETURN(LIBBLU_H264_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_H264_ERROR_NRETURN(format, ...)                                \
  LIBBLU_ERROR_NRETURN(LIBBLU_H264_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_H264_ERROR_FRETURN(format, ...)                                \
  LIBBLU_ERROR_FRETURN(LIBBLU_H264_PREFIX format, ##__VA_ARGS__)

/* ### H.264 Checks : ###################################################### */

#define LIBBLU_H264_COMPLIANCE_ERROR_RETURN(format, ...)                      \
  LIBBLU_ERROR_RETURN("H.264 Compliance issue: " format, ##__VA_ARGS__)

#define LIBBLU_H264_COMPLIANCE_ERROR_FRETURN(format, ...)                     \
  LIBBLU_ERROR_FRETURN("H.264 Compliance issue: " format, ##__VA_ARGS__)

/* ### H.264 HDR Verifier : ################################################ */

#define LIBBLU_H264_HRDV_INFO_BRETURN(format, ...)                            \
  do {                                                                        \
    LIBBLU_INFO(LIBBLU_H264_HRDV_PREFIX format, ##__VA_ARGS__);               \
    return false;                                                             \
  } while (0)

#define LIBBLU_H264_HRDV_WARNING(format, ...)                                 \
  LIBBLU_WARNING(LIBBLU_H264_HRDV_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_H264_HRDV_ERROR(format, ...)                                   \
  LIBBLU_ERROR(LIBBLU_H264_HRDV_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_H264_HRDV_ERROR_RETURN(format, ...)                            \
  LIBBLU_ERROR_RETURN(LIBBLU_H264_HRDV_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_H264_HRDV_ERROR_NRETURN(format, ...)                           \
  LIBBLU_ERROR_NRETURN(LIBBLU_H264_HRDV_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_H264_HRDV_ERROR_FRETURN(format, ...)                           \
  LIBBLU_ERROR_FRETURN(LIBBLU_H264_HRDV_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_H264_HRDV_FAIL_RETURN(format, ...)                             \
  LIBBLU_FAIL_RETURN(                                                         \
    LIBBLU_EXPLODE_STD_COMPLIANCE,                                            \
    LIBBLU_H264_HRDV_PREFIX format,                                           \
    ##__VA_ARGS__                                                             \
  )

#define LIBBLU_H264_HRDV_BD_FAIL_RETURN(format, ...)                          \
  LIBBLU_FAIL_RETURN(                                                         \
    LIBBLU_EXPLODE_BDAV_STD_COMPLIANCE,                                       \
    LIBBLU_H264_HRDV_PREFIX format,                                           \
    ##__VA_ARGS__                                                             \
  )

#define LIBBLU_H264_READING_FAIL_RETURN()                                     \
  LIBBLU_ERROR_RETURN(                                                        \
    LIBBLU_H264_PREFIX "Reading failed at %s:%s():%u.\n",                     \
    __FILE__, __func__, __LINE__                                              \
  )

/* ### Debugging ########################################################### */

#define LIBBLU_H264_DEBUG(format, ...)                                        \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_H264_OPERATIONS, "H.264", format, ##__VA_ARGS__              \
  )

#define LIBBLU_H264_DEBUG_NAL(format, ...)                                    \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_H264_PARSING_NAL, "H.264/NAL", format, ##__VA_ARGS__         \
  )

#define LIBBLU_H264_DEBUG_AU_PROCESSING(format, ...)                          \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_H264_AU_PROCESSING, "H.264/AU handling",                     \
    format, ##__VA_ARGS__                                                     \
  )

#define LIBBLU_H264_DEBUG_AUD(format, ...)                                    \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_H264_PARSING_AUD, "H.264/AUD", format, ##__VA_ARGS__         \
  )

#define LIBBLU_H264_DEBUG_AUD_NH(format, ...)                                 \
  LIBBLU_DEBUG_NO_HEADER(                                                     \
    LIBBLU_DEBUG_H264_PARSING_AUD, format, ##__VA_ARGS__                      \
  )

#define LIBBLU_H264_DEBUG_SPS(format, ...)                                    \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_H264_PARSING_SPS, "H.264/SPS", format, ##__VA_ARGS__         \
  )

#define LIBBLU_H264_DEBUG_VUI(format, ...)                                    \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_H264_PARSING_VUI, "H.264/VUI", format, ##__VA_ARGS__         \
  )

#define LIBBLU_H264_DEBUG_VUI_NH(format, ...)                                 \
  LIBBLU_DEBUG_NO_HEADER(                                                     \
    LIBBLU_DEBUG_H264_PARSING_VUI, format, ##__VA_ARGS__                      \
  )

#define LIBBLU_H264_DEBUG_PPS(format, ...)                                    \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_H264_PARSING_PPS, "H.264/PPS", format, ##__VA_ARGS__         \
  )

#define LIBBLU_H264_DEBUG_SEI(format, ...)                                    \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_H264_PARSING_SEI, "H.264/SEI", format, ##__VA_ARGS__         \
  )

#define LIBBLU_H264_DEBUG_SLICE(format, ...)                                  \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_H264_PARSING_SLICE, "H.264/Slc", format, ##__VA_ARGS__       \
  )

#define LIBBLU_H264_DEBUG_HRD(format, ...)                                    \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_H264_HRD, "H.264/HRD", format, ##__VA_ARGS__                 \
  )

#endif