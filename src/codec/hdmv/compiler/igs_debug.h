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

#include "../common/hdmv_data.h"
#include "../common/hdmv_pictures_quantizer.h"

#define ENABLE_IGS_DEBUG 1

#if ENABLE_IGS_DEBUG
void writeHexatreeNode(FILE * f, HdmvQuantHexTreeNodePtr node);

void ecrireDebut(FILE * f);
void ecrireFin(FILE * f);
void dessineCode(FILE * f, HdmvQuantHexTreeNodePtr tree);

#  define IGS_DEBUG_MAX_CMD_LEN 200

int creePDFCode(char *dot, char *pdf, HdmvQuantHexTreeNodePtr tree);
int creePNGCode(char *dot, char *pdf, HdmvQuantHexTreeNodePtr tree);
#endif

#endif