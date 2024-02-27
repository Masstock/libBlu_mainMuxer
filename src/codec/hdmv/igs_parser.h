/** \~english
 * \file igs_parser.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief HDMV menus (Interactive Graphic Stream) parsing module.
 *
 * \todo Add references.
 */

#ifndef __LIBBLU_MUXER__CODECS__HDMV__IGS_PARSER_H__
#define __LIBBLU_MUXER__CODECS__HDMV__IGS_PARSER_H__

#include "../common/esParsingSettings.h"
#include "../../util.h"
#include "../../esms/scriptCreation.h"

#include "common/hdmv_context.h"
#include "common/hdmv_parser.h"

#if !defined(DISABLE_IGS_COMPILER)
#  include "compiler/igs_compiler.h"
#endif

int analyzeIgs(
  LibbluESParsingSettings * settings
);

#endif