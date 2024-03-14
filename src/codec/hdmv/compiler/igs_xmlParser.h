/** \~english
 * \file igs_xmlParser.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief HDMV IGS Compiler internal XML definition format parsing module.
 */

#ifndef __LIBBLU_MUXER__CODECS__IGS_COMPILER__XML_PARSER_H__
#define __LIBBLU_MUXER__CODECS__IGS_COMPILER__XML_PARSER_H__

#include "../../../ini/iniData.h"
#include "../../../util/xmlParsing.h"
#include "../common/hdmv_pictures_common.h"

#include "igs_compiler_context_data.h"
#include "igs_compiler_data.h"
#include "igs_debug.h"

#define XML_CTX(IgsCompilerContextPtr)                                        \
  ((IgsCompilerContextPtr)->xml_ctx)

/** \~english
 * \brief Parse context opened IGS XML description file.
 *
 * \param ctx Used Context (containing file, and used to store results).
 * \return int A zero value on success, otherwise a negative value.
 */
int parseIgsXmlFile(
  IgsCompilerContext *ctx
);

#endif