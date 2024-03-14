/** \~english
 * \file igs_debug.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief HDMV IGS Compiler debugging module.
 */

#ifndef __LIBBLU_MUXER__CODECS__IGS_COMPILER__DEBUG_H__
#define __LIBBLU_MUXER__CODECS__IGS_COMPILER__DEBUG_H__

#include "../../../util.h"

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

#define LIBBLU_HDMV_IGS_COMPL_XML_DEBUG_VA(format, ap)                        \
  LIBBLU_DEBUG_VA(                                                            \
    LIBBLU_DEBUG_IGS_COMPL_XML_OPERATIONS,                                    \
    LIBBLU_HDMV_IGS_COMPL_XML_NAME,                                           \
    format,                                                                   \
    ap                                                                        \
  )

#define LIBBLU_HDMV_IGS_COMPL_XML_PARSING_DEBUG(format, ...)                  \
  LIBBLU_DEBUG(                                                               \
    LIBBLU_DEBUG_IGS_COMPL_XML_PARSING,                                       \
    LIBBLU_HDMV_IGS_COMPL_XML_NAME,                                           \
    format,                                                                   \
    ##__VA_ARGS__                                                             \
  )

#define ENABLE_IGS_DEBUG  0

#if ENABLE_IGS_DEBUG
#include "../common/hdmv_data.h"
#include "../common/hdmv_quantizer.h"

void writeHexatreeNode(FILE *f, HdmvQuantHexTreeNodePtr node);

void ecrireDebut(FILE *f);
void ecrireFin(FILE *f);
void dessineCode(FILE *f, HdmvQuantHexTreeNodePtr tree);

#  define IGS_DEBUG_MAX_CMD_LEN 200

int creePDFCode(char *dot, char *pdf, HdmvQuantHexTreeNodePtr tree);
int creePNGCode(char *dot, char *pdf, HdmvQuantHexTreeNodePtr tree);
#endif
#endif