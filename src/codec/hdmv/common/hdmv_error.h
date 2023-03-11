

#ifndef __LIBBLU_MUXER__CODECS__HDMV__COMMON__ERROR_H__
#define __LIBBLU_MUXER__CODECS__HDMV__COMMON__ERROR_H__

#include "../../../util/errorCodes.h"

#define LIBBLU_HDMV_KEYWORD  "HDMV"

/* ### HDMV Common : ####################################################### */

#define LIBBLU_HDMV_COM_NAME  LIBBLU_HDMV_KEYWORD "/Common"
#define LIBBLU_HDMV_COM_PREFIX LIBBLU_HDMV_COM_NAME ": "

#define LIBBLU_HDMV_COM_ERROR(format, ...)                                    \
  LIBBLU_ERROR(LIBBLU_HDMV_COM_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_COM_ERROR_RETURN(format, ...)                             \
  LIBBLU_ERROR_RETURN(LIBBLU_HDMV_COM_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_COM_ERROR_NRETURN(format, ...)                            \
  LIBBLU_ERROR_NRETURN(LIBBLU_HDMV_COM_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_COM_ERROR_FRETURN(format, ...)                            \
  LIBBLU_ERROR_FRETURN(LIBBLU_HDMV_COM_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_COM_ERROR_BRETURN(format, ...)                            \
  LIBBLU_ERROR_BRETURN(LIBBLU_HDMV_COM_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_COM_DEBUG(format, ...)                                    \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_HDMV_COMMON,                                                 \
    LIBBLU_HDMV_COM_NAME,                                                     \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

#define LIBBLU_HDMV_COM_DEBUG_NH(format, ...)                                 \
  LIBBLU_DEBUG_NO_HEADER(                                                     \
    LIBBLU_DEBUG_HDMV_COMMON,                                                 \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

/* ### HDMV Parser : ####################################################### */

#define LIBBLU_HDMV_PARSER_NAME  LIBBLU_HDMV_KEYWORD "/Parser"
#define LIBBLU_HDMV_PARSER_PREFIX LIBBLU_HDMV_PARSER_NAME ": "

#define LIBBLU_HDMV_PARSER_ERROR(format, ...)                                 \
  LIBBLU_ERROR(LIBBLU_HDMV_PARSER_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_PARSER_ERROR_RETURN(format, ...)                          \
  LIBBLU_ERROR_RETURN(LIBBLU_HDMV_PARSER_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_PARSER_ERROR_NRETURN(format, ...)                         \
  LIBBLU_ERROR_NRETURN(LIBBLU_HDMV_PARSER_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_PARSER_ERROR_FRETURN(format, ...)                         \
  LIBBLU_ERROR_FRETURN(LIBBLU_HDMV_PARSER_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_PARSER_ERROR_BRETURN(format, ...)                         \
  LIBBLU_ERROR_BRETURN(LIBBLU_HDMV_PARSER_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_PARSER_DEBUG(format, ...)                                 \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_HDMV_PARSER,                                                 \
    LIBBLU_HDMV_PARSER_NAME,                                                  \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

#define LIBBLU_HDMV_PARSER_DEBUG_NH(format, ...)                              \
  LIBBLU_DEBUG_NO_HEADER(                                                     \
    LIBBLU_DEBUG_HDMV_PARSER,                                                 \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

/* ### HDMV Timestamps compute : ########################################### */

#define LIBBLU_HDMV_TS_COMPUTE_NAME  LIBBLU_HDMV_KEYWORD "/TsCompute"
#define LIBBLU_HDMV_TS_COMPUTE_PREFIX LIBBLU_HDMV_TS_COMPUTE_NAME ": "

#define LIBBLU_HDMV_TSC_DEBUG(format, ...)                                    \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_HDMV_TS_COMPUTE,                                             \
    LIBBLU_HDMV_TS_COMPUTE_NAME,                                              \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

#define LIBBLU_HDMV_TSC_WARNING(format, ...)                                  \
  LIBBLU_WARNING(LIBBLU_HDMV_TS_COMPUTE_PREFIX format, ##__VA_ARGS__)

/* ### IGS Parser : ######################################################## */

#define LIBBLU_HDMV_IGS_NAME  LIBBLU_HDMV_KEYWORD "/IGS Parser"
#define LIBBLU_HDMV_IGS_PREFIX LIBBLU_HDMV_IGS_NAME ": "

#define LIBBLU_HDMV_IGS_ERROR(format, ...)                                    \
  LIBBLU_ERROR(LIBBLU_HDMV_IGS_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_IGS_ERROR_RETURN(format, ...)                             \
  LIBBLU_ERROR_RETURN(LIBBLU_HDMV_IGS_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_IGS_ERROR_NRETURN(format, ...)                            \
  LIBBLU_ERROR_NRETURN(LIBBLU_HDMV_IGS_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_IGS_ERROR_FRETURN(format, ...)                            \
  LIBBLU_ERROR_FRETURN(LIBBLU_HDMV_IGS_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_IGS_ERROR_BRETURN(format, ...)                            \
  LIBBLU_ERROR_BRETURN(LIBBLU_HDMV_IGS_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_IGS_DEBUG(format, ...)                                    \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_IGS_PARSER,                                             \
    LIBBLU_HDMV_IGS_NAME,                                                     \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

/* ### PGS Parser : ######################################################## */

#define LIBBLU_HDMV_PGS_NAME  LIBBLU_HDMV_KEYWORD "/PGS Parser"
#define LIBBLU_HDMV_PGS_PREFIX LIBBLU_HDMV_PGS_NAME ": "

#define LIBBLU_HDMV_PGS_ERROR(format, ...)                                    \
  LIBBLU_ERROR(LIBBLU_HDMV_PGS_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_PGS_ERROR_RETURN(format, ...)                             \
  LIBBLU_ERROR_RETURN(LIBBLU_HDMV_PGS_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_PGS_ERROR_NRETURN(format, ...)                            \
  LIBBLU_ERROR_NRETURN(LIBBLU_HDMV_PGS_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_PGS_ERROR_FRETURN(format, ...)                            \
  LIBBLU_ERROR_FRETURN(LIBBLU_HDMV_PGS_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_PGS_ERROR_BRETURN(format, ...)                            \
  LIBBLU_ERROR_BRETURN(LIBBLU_HDMV_PGS_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_PGS_DEBUG(format, ...)                                    \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_PGS_PARSER,                                             \
    LIBBLU_HDMV_PGS_NAME,                                                     \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

/* ### IGS Compiler : ###################################################### */

#define LIBBLU_HDMV_IGS_COMPL_NAME  LIBBLU_HDMV_KEYWORD "/IGS Compiler"
#define LIBBLU_HDMV_IGS_COMPL_PREFIX LIBBLU_HDMV_IGS_COMPL_NAME ": "

#define LIBBLU_HDMV_IGS_COMPL_ERROR(format, ...)                              \
  LIBBLU_ERROR(LIBBLU_HDMV_IGS_COMPL_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_IGS_COMPL_ERROR_RETURN(format, ...)                       \
  LIBBLU_ERROR_RETURN(LIBBLU_HDMV_IGS_COMPL_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_IGS_COMPL_ERROR_NRETURN(format, ...)                      \
  LIBBLU_ERROR_NRETURN(LIBBLU_HDMV_IGS_COMPL_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_IGS_COMPL_ERROR_FRETURN(format, ...)                      \
  LIBBLU_ERROR_FRETURN(LIBBLU_HDMV_IGS_COMPL_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_IGS_COMPL_ERROR_GRETURN(lbl, format, ...)                 \
  LIBBLU_ERROR_GRETURN(lbl, LIBBLU_HDMV_IGS_COMPL_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_IGS_COMPL_ERROR_BRETURN(format, ...)                      \
  LIBBLU_ERROR_BRETURN(LIBBLU_HDMV_IGS_COMPL_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_IGS_COMPL_INFO(format, ...)                               \
  LIBBLU_INFO(LIBBLU_HDMV_IGS_COMPL_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_IGS_COMPL_DEBUG(format, ...)                              \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_IGS_COMPL_OPERATIONS,                                        \
    LIBBLU_HDMV_IGS_COMPL_NAME,                                               \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

#define LIBBLU_HDMV_IGS_COMPL_ERROR_VA(format, ap)                            \
  LIBBLU_ERROR_VA(LIBBLU_HDMV_IGS_COMPL_PREFIX format, ap)

/* ### IGS Compiler XML : ######################### ######################## */

#define LIBBLU_HDMV_IGS_COMPL_XML_NAME  LIBBLU_HDMV_KEYWORD "/IGS Compiler XML"
#define LIBBLU_HDMV_IGS_COMPL_XML_PREFIX LIBBLU_HDMV_IGS_COMPL_XML_NAME ": "

#define LIBBLU_HDMV_IGS_COMPL_XML_ERROR(format, ...)                          \
  LIBBLU_ERROR(LIBBLU_HDMV_IGS_COMPL_XML_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_IGS_COMPL_XML_ERROR_RETURN(format, ...)                   \
  LIBBLU_ERROR_RETURN(LIBBLU_HDMV_IGS_COMPL_XML_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_IGS_COMPL_XML_ERROR_NRETURN(format, ...)                  \
  LIBBLU_ERROR_NRETURN(LIBBLU_HDMV_IGS_COMPL_XML_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_IGS_COMPL_XML_ERROR_FRETURN(format, ...)                  \
  LIBBLU_ERROR_FRETURN(LIBBLU_HDMV_IGS_COMPL_XML_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_IGS_COMPL_XML_ERROR_GRETURN(lbl, format, ...)             \
  LIBBLU_ERROR_GRETURN(lbl, LIBBLU_HDMV_IGS_COMPL_XML_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_IGS_COMPL_XML_ERROR_BRETURN(format, ...)                  \
  LIBBLU_ERROR_BRETURN(LIBBLU_HDMV_IGS_COMPL_XML_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_IGS_COMPL_XML_WARNING(format, ...)                        \
  LIBBLU_WARNING(LIBBLU_HDMV_IGS_COMPL_XML_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_IGS_COMPL_XML_INFO(format, ...)                           \
  LIBBLU_INFO(LIBBLU_HDMV_IGS_COMPL_XML_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_IGS_COMPL_XML_DEBUG(format, ...)                          \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_IGS_COMPL_XML_OPERATIONS,                                    \
    LIBBLU_HDMV_IGS_COMPL_XML_NAME,                                           \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

#define LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(format, ...)                  \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_IGS_COMPL_XML_PARSING,                                       \
    LIBBLU_HDMV_IGS_COMPL_XML_NAME,                                           \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

/* ### HDMV Segments Building : ############################################ */

#define LIBBLU_HDMV_SEGBUILD_NAME  LIBBLU_HDMV_KEYWORD "/Builder"
#define LIBBLU_HDMV_SEGBUILD_PREFIX LIBBLU_HDMV_SEGBUILD_NAME ": "

#define LIBBLU_HDMV_SEGBUILD_ERROR(format, ...)                               \
  LIBBLU_ERROR(LIBBLU_HDMV_SEGBUILD_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_SEGBUILD_ERROR_RETURN(format, ...)                        \
  LIBBLU_ERROR_RETURN(LIBBLU_HDMV_SEGBUILD_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_SEGBUILD_ERROR_NRETURN(format, ...)                       \
  LIBBLU_ERROR_NRETURN(LIBBLU_HDMV_SEGBUILD_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_SEGBUILD_ERROR_FRETURN(format, ...)                       \
  LIBBLU_ERROR_FRETURN(LIBBLU_HDMV_SEGBUILD_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_SEGBUILD_ERROR_GRETURN(lbl, format, ...)                  \
  LIBBLU_ERROR_GRETURN(lbl, LIBBLU_HDMV_SEGBUILD_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_SEGBUILD_ERROR_BRETURN(format, ...)                       \
  LIBBLU_ERROR_BRETURN(LIBBLU_HDMV_SEGBUILD_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_SEGBUILD_ERROR_ZRETURN(format, ...)                       \
  LIBBLU_ERROR_ZRETURN(LIBBLU_HDMV_SEGBUILD_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_SEGBUILD_INFO(format, ...)                                \
  LIBBLU_INFO(LIBBLU_HDMV_SEGBUILD_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_SEGBUILD_DEBUG(format, ...)                               \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_HDMV_SEG_BUILDER,                                            \
    LIBBLU_HDMV_SEGBUILD_NAME,                                                \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

/* ### Pictures handling : ################################################# */

#define LIBBLU_HDMV_PIC_NAME  LIBBLU_HDMV_KEYWORD " picture"
#define LIBBLU_HDMV_PIC_PREFIX LIBBLU_HDMV_PIC_NAME ": "

#define LIBBLU_HDMV_PIC_ERROR(format, ...)                                    \
  LIBBLU_ERROR(LIBBLU_HDMV_PIC_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_PIC_ERROR_RETURN(format, ...)                             \
  LIBBLU_ERROR_RETURN(LIBBLU_HDMV_PIC_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_PIC_ERROR_NRETURN(format, ...)                            \
  LIBBLU_ERROR_NRETURN(LIBBLU_HDMV_PIC_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_PIC_ERROR_FRETURN(format, ...)                            \
  LIBBLU_ERROR_FRETURN(LIBBLU_HDMV_PIC_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_PIC_ERROR_BRETURN(format, ...)                            \
  LIBBLU_ERROR_BRETURN(LIBBLU_HDMV_PIC_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_PIC_DEBUG(format, ...)                                    \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_HDMV_PICTURES,                                               \
    LIBBLU_HDMV_PIC_NAME,                                                     \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

/* ### Palettes : ########################################################## */

#define LIBBLU_HDMV_PAL_NAME  LIBBLU_HDMV_KEYWORD " palette"
#define LIBBLU_HDMV_PAL_PREFIX LIBBLU_HDMV_PAL_NAME ": "

#define LIBBLU_HDMV_PAL_ERROR(format, ...)                                    \
  LIBBLU_ERROR(LIBBLU_HDMV_PAL_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_PAL_ERROR_RETURN(format, ...)                             \
  LIBBLU_ERROR_RETURN(LIBBLU_HDMV_PAL_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_PAL_ERROR_NRETURN(format, ...)                            \
  LIBBLU_ERROR_NRETURN(LIBBLU_HDMV_PAL_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_PAL_DEBUG(format, ...)                                    \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_HDMV_PAL,                                                    \
    LIBBLU_HDMV_PAL_NAME,                                                     \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

#define LIBBLU_HDMV_PAL_DEBUG_NH(format, ...)                                 \
  LIBBLU_DEBUG_NO_HEADER(                                                     \
    LIBBLU_DEBUG_HDMV_PAL,                                                    \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

/* ### Quantization : ###################################################### */

#define LIBBLU_HDMV_QUANT_NAME  LIBBLU_HDMV_KEYWORD " quant"
#define LIBBLU_HDMV_QUANT_PREFIX LIBBLU_HDMV_QUANT_NAME ": "

#define LIBBLU_HDMV_QUANT_ERROR(format, ...)                                  \
  LIBBLU_ERROR(LIBBLU_HDMV_QUANT_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_QUANT_ERROR_RETURN(format, ...)                           \
  LIBBLU_ERROR_RETURN(LIBBLU_HDMV_QUANT_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_QUANT_ERROR_ZRETURN(format, ...)                          \
  LIBBLU_ERROR_ZRETURN(LIBBLU_HDMV_QUANT_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_QUANT_ERROR_NRETURN(format, ...)                          \
  LIBBLU_ERROR_NRETURN(LIBBLU_HDMV_QUANT_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_QUANT_DEBUG(format, ...)                                  \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_HDMV_QUANTIZER,                                              \
    LIBBLU_HDMV_QUANT_NAME,                                                   \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

/* ### libpng : ############################################################ */

#define LIBBLU_HDMV_LIBPNG_NAME  LIBBLU_HDMV_KEYWORD " libpng"
#define LIBBLU_HDMV_LIBPNG_PREFIX  LIBBLU_HDMV_LIBPNG_NAME ": "

#define LIBBLU_HDMV_LIBPNG_ERROR(format, ...)                                 \
  LIBBLU_ERROR(LIBBLU_HDMV_LIBPNG_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_LIBPNG_ERROR_RETURN(format, ...)                          \
  LIBBLU_ERROR_RETURN(LIBBLU_HDMV_LIBPNG_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_LIBPNG_ERROR_NRETURN(format, ...)                         \
  LIBBLU_ERROR_NRETURN(LIBBLU_HDMV_LIBPNG_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_LIBPNG_DEBUG(format, ...)                                 \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_HDMV_LIBPNG,                                                 \
    LIBBLU_HDMV_LIBPNG_NAME,                                                  \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

/* ### Timecodes : ######################################################### */

#define LIBBLU_HDMV_TC_NAME  LIBBLU_HDMV_KEYWORD "/Timecodes"
#define LIBBLU_HDMV_TC_PREFIX  LIBBLU_HDMV_TC_NAME ": "

#define LIBBLU_HDMV_TC_ERROR(format, ...)                                     \
  LIBBLU_ERROR(LIBBLU_HDMV_TC_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_TC_ERROR_RETURN(format, ...)                              \
  LIBBLU_ERROR_RETURN(LIBBLU_HDMV_TC_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_TC_ERROR_NRETURN(format, ...)                             \
  LIBBLU_ERROR_NRETURN(LIBBLU_HDMV_TC_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_TC_DEBUG(format, ...)                                     \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_HDMV_TC,                                                     \
    LIBBLU_HDMV_TC_NAME,                                                      \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

/* ### Checks : ############################################################ */

#define LIBBLU_HDMV_CK_NAME  LIBBLU_HDMV_KEYWORD "/Checks"
#define LIBBLU_HDMV_CK_PREFIX  LIBBLU_HDMV_CK_NAME ": "

#define LIBBLU_HDMV_CK_ERROR(format, ...)                                     \
  LIBBLU_ERROR(LIBBLU_HDMV_CK_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_CK_ERROR_RETURN(format, ...)                              \
  LIBBLU_ERROR_RETURN(LIBBLU_HDMV_CK_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_CK_ERROR_NRETURN(format, ...)                             \
  LIBBLU_ERROR_NRETURN(LIBBLU_HDMV_CK_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_CK_WARNING(format, ...)                                   \
  LIBBLU_WARNING(LIBBLU_HDMV_CK_PREFIX format, ##__VA_ARGS__)

#define LIBBLU_HDMV_CK_DEBUG(format, ...)                                     \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_HDMV_CHECKS,                                                 \
    LIBBLU_HDMV_CK_NAME,                                                      \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

#endif